"""Runs phrase requests: the scheduler decides, this asks the provider and pushes results as they stream."""

import asyncio
import logging
import time
from dataclasses import replace
import weakref
from collections.abc import Callable

from completionist_engine.config import PhraseConfig
from completionist_engine.metrics import Metrics
from completionist_engine.context import anchored_window, build_prompt, screen_for_prompt, trim_suffix
from completionist_engine.phrase_provider import Attempt, PhraseProvider, PhraseRequest, ProviderError
from completionist_engine.phrase_scheduler import Action, Cancel, Mode, PhraseScheduler, Start, Update
from completionist_engine.protocol import PhraseUpdate, Request
from completionist_engine.request_log import RequestLog
from completionist_engine.screen_context import ScreenContext
from completionist_engine.writing import phrase_instructions, limit_reply

logger = logging.getLogger("completionist_engine.phrases")

def _build_provider(config: PhraseConfig) -> PhraseProvider | None:
    key = config.provider.api_key
    return PhraseProvider(config.provider, key) if key and config.enabled else None


class PhraseService:
    """Shared by every connection: the provider and its health."""

    def __init__(
        self,
        config: PhraseConfig,
        *,
        provider: PhraseProvider | None = None,
        clock: Callable[[], float] = time.monotonic,
        metrics: Metrics | None = None,
        log: RequestLog | None = None,
        screen: ScreenContext | None = None,
        accessible: ScreenContext | None = None,
    ) -> None:
        """The API key is `config.provider.api_key`; with none, phrases are off."""
        self.config = config
        self.metrics = metrics
        self.log = log if log is not None else RequestLog()
        self.screen = screen  # reads the window in front; None where there is no screen (tests, no desktop)
        self.accessible = accessible
        self.clock = clock
        self._provider = provider
        if provider is None:
            self._provider = _build_provider(config)
        self._failures = 0
        self._paused_until = 0.0
        self._private = False
        self._sessions: weakref.WeakSet[PhraseSession] = weakref.WeakSet()
        self._running: set[asyncio.Task] = set()
        self._preview: tuple[int, PhraseSession, Request, tuple] | None = None
        self._preview_id = 0
        self._privacy_generation = 0
        self.log.configure(config.log_mode, config.log_retention_minutes)

    def set_private(self, enabled: bool) -> None:
        self._private = enabled
        if enabled:
            self._privacy_generation += 1
            self.cancel_preview()
            for session in self._sessions:
                session.suspend()
            self.log.clear()
            for context in (self.screen, self.accessible):
                if context is not None:
                    context.clear()
        self.log.configure("off" if enabled else self.config.log_mode, self.config.log_retention_minutes)

    def context_preview(self) -> dict | None:
        if self._preview is None or self._private:
            return None
        ident, _, request, prepared = self._preview
        return {"id": ident, "app": request.app, "title": request.title, "prompt": prepared[0].prompt,
                "suffix": prepared[0].suffix if prepared[3].provider.fim else ""}

    def cancel_preview(self, owner: "PhraseSession | None" = None, preview_id: int | None = None) -> bool:
        if self._preview is None:
            return False
        ident, session, _, _ = self._preview
        if (owner is not None and session is not owner) or (preview_id is not None and ident != preview_id):
            return False
        self._preview = None
        return True

    def send_preview(self, preview_id: int) -> bool:
        if self._preview is None or self._preview[0] != preview_id or not self.usable():
            return False
        _, session, request, prepared = self._preview
        self._preview = None
        # The scheduler is created only after approval; outgoing text is the frozen preview.
        session._preview_approved = prepared
        session._apply(session._scheduler.hotkey(request, "hotkey", self.clock(), True).actions)
        return True

    @property
    def available(self) -> bool:
        return self.config.enabled and self._provider is not None

    def usable(self) -> bool:
        """Whether a new request may start right now."""
        return self.available and not self._private and self.clock() >= self._paused_until

    def reconfigure(self, config: PhraseConfig) -> None:
        """Apply reloaded settings: a new provider or model, debounce, or switching off."""
        old = self.config
        self.config = config
        self._privacy_generation += 1
        self.cancel_preview()
        for session in self._sessions:
            session.suspend()
        self.log.configure("off" if self._private else config.log_mode, config.log_retention_minutes)
        if config.provider != old.provider or config.enabled != old.enabled:  # includes a new or removed key
            stale = self._provider
            self._provider = _build_provider(config)
            if stale is not None:
                try:
                    asyncio.get_running_loop().create_task(stale.aclose())
                except RuntimeError:
                    pass  # no loop running (a test): the old client is simply dropped
        logger.info("phrase settings reloaded (%s)", "on" if self.available else "off")

    def note_success(self) -> None:
        self._failures = 0

    def note_failure(self) -> None:
        self._failures += 1
        if self._failures >= self.config.failure_limit:
            self._paused_until = self.clock() + self.config.failure_pause
            self._failures = 0
            logger.warning("phrase provider failed %d times in a row; pausing phrases for %gs", self.config.failure_limit, self.config.failure_pause)

    def record_provider(self, ttft: float | None, total: float | None, ok: bool) -> None:
        if self.metrics is not None:
            self.metrics.record_provider(self.config.provider.models[0], ttft, total, ok)

    def open_session(self, push: Callable[[PhraseUpdate], None]) -> "PhraseSession":
        session = PhraseSession(self, push)
        self._sessions.add(session)
        return session

    async def aclose(self) -> None:
        self.cancel_preview()
        running = list(self._running)
        for task in running:
            task.cancel()
        if running:
            await asyncio.gather(*running, return_exceptions=True)
        if self._provider is not None:
            await self._provider.aclose()

    def _stream(self, request: PhraseRequest, attempts: list[Attempt], on_event=None):
        assert self._provider is not None
        return self._provider.stream(request, attempts, on_event=on_event)


class PhraseSession:
    """One connection's phrase state: a scheduler, the running request and the pause timer."""

    def __init__(self, service: PhraseService, push: Callable[[PhraseUpdate], None]) -> None:
        self._service = service
        self._push = push
        self._scheduler = PhraseScheduler(service.config.debounce)
        self._tasks: dict[int, asyncio.Task] = {}
        self._timer: asyncio.TimerHandle | None = None
        self._config = service.config
        self._dismissed_until = 0.0
        self._preview_approved: tuple | None = None
        self._last_request_id: int | None = None

    # -- called for each event from the text service ---------------------------------------------

    def on_request(self, request: Request, mode: Mode, *, config: PhraseConfig | None = None) -> Update:
        self._last_request_id = request.id
        self._config = config or self._service.config
        self._service.cancel_preview(self)
        self._scheduler.debounce = self._config.debounce
        eligible = (len(request.before.strip()) >= self._config.min_chars
                    and (self._config.trigger == "pause" or request.before.endswith("\n") or request.before.rstrip().endswith((".", "!", "?")))
                    and self._now() >= self._dismissed_until and not self._config.preview_context)
        if mode == "auto" and not eligible:
            mode = "hotkey"
        if not self._config.enabled or not self._service.usable() or self._text_after_clashes(request):
            mode = "off"
        update = self._scheduler.request(request, mode, self._now(), self._service.usable())
        self._apply(update.actions)
        self._arm_timer()
        return update

    def on_hotkey(self, request: Request, mode: Mode, *, config: PhraseConfig | None = None) -> Update:
        self._last_request_id = request.id
        self._config = config or self._service.config
        if not self._config.enabled or not self._service.usable() or self._text_after_clashes(request):
            mode = "off"
        if self._config.preview_context and mode != "off" and request.before.strip():
            self.suspend()
            self._service._preview_id += 1
            self._service._preview = (self._service._preview_id, self, request, self._prepare(request, self._config))
            return Update((), "", True, request.id)
        update = self._scheduler.hotkey(request, mode, self._now(), self._service.usable())
        self._apply(update.actions)
        self._arm_timer()
        return update

    def on_dismiss(self) -> None:
        self._dismissed_until = self._now() + self._config.dismiss_cooldown
        self._service.cancel_preview(self)
        self._apply(self._scheduler.dismiss())
        self._arm_timer()

    def close(self) -> None:
        self._service.cancel_preview(self)
        self._apply(self._scheduler.dismiss())
        if self._timer is not None:
            self._timer.cancel()
            self._timer = None
        for task in list(self._tasks.values()):
            task.cancel()
        self._tasks.clear()

    def suspend(self) -> None:
        """Cancel pending work without treating a privacy or settings change as a rejection."""
        self._service.cancel_preview(self)
        self._apply(self._scheduler.dismiss())
        if self._timer is not None:
            self._timer.cancel()
            self._timer = None
        self._preview_approved = None
        for task in list(self._tasks.values()):
            task.cancel()
        if self._last_request_id is not None:
            self._push(PhraseUpdate(id=self._last_request_id, text="", done=True))

    # -- internals -------------------------------------------------------------------------------

    def _text_after_clashes(self, request: Request) -> bool:
        """Without fill-in-the-middle the model never sees what follows the caret, so a phrase would run
        into text already on the same line. Hold it back there (later lines don't matter)."""
        if self._config.provider.fim:
            return False
        return bool(request.after.split("\n", 1)[0].strip())

    def _now(self) -> float:
        return self._service.clock()

    def _apply(self, actions: tuple[Action, ...]) -> None:
        for action in actions:
            if isinstance(action, Cancel):
                task = self._tasks.pop(action.request_id, None)
                if task is not None:
                    task.cancel()
        for action in actions:
            if isinstance(action, Start):
                self._launch(action.request)

    def _launch(self, request: Request) -> None:
        try:
            loop = asyncio.get_running_loop()
        except RuntimeError:  # no event loop (called from a plain test): nothing can run
            return
        task = loop.create_task(self._run(request))
        self._tasks[request.id] = task
        self._service._running.add(task)
        task.add_done_callback(self._service._running.discard)

    def _arm_timer(self) -> None:
        if self._timer is not None:
            self._timer.cancel()
            self._timer = None
        due = self._scheduler.next_due()
        if due is None:
            return
        try:
            loop = asyncio.get_running_loop()
        except RuntimeError:
            return
        self._timer = loop.call_later(max(due - self._now(), 0.0), self._on_timer)

    def _on_timer(self) -> None:
        self._timer = None
        if not self._service.usable():
            self.suspend()
            return
        update = self._scheduler.tick(self._now())
        self._apply(update.actions)
        self._arm_timer()

    def _emit(self, update: Update | None) -> None:
        if update is None or update.request_id is None:
            return
        self._apply(update.actions)
        self._arm_timer()
        self._push(PhraseUpdate(id=update.request_id, text=update.phrase, done=update.done))

    def _prepare(self, request: Request, config: PhraseConfig) -> tuple:
        context = self._service.accessible if config.context_source == "accessible" else self._service.screen
        may_read = config.screen_context and config.context_source != "caret" and (not config.context_apps or request.app.lower() in config.context_apps)
        shot = context.for_request() if context is not None and may_read else None
        if shot is not None and (shot.app.lower() != request.app.lower() or shot.title != request.title):
            shot = None
        screen_sent = screen_for_prompt(shot.text, request.before) if shot is not None else ""
        prompt = build_prompt(
            request.app, request.title, anchored_window(request.before, config.context_before), phrase_instructions(config), screen_sent
        )
        phrase_request = PhraseRequest(prompt=prompt, suffix=trim_suffix(request.after, config.context_after))
        return phrase_request, shot, screen_sent, config, self._service._privacy_generation

    async def _run(self, request: Request) -> None:
        phrase_request, shot, screen_sent, config, privacy_generation = self._preview_approved or self._prepare(request, self._config)
        self._preview_approved = None
        prompt = phrase_request.prompt
        task = asyncio.current_task()
        service = self._service
        started = service.clock()
        first: float | None = None
        attempts: list[Attempt] = []
        reply = ""
        outcome = "cancelled"  # stays this way if the request is cancelled (the user kept typing)
        temporary_provider = None
        events: list[dict] = []

        def event(kind: str, **detail) -> None:
            events.append({"ms": round((service.clock() - started) * 1000), "kind": kind, **detail})

        event("started", trigger="manual" if request.event == "hotkey" else "automatic")
        event("context", source=config.context_source, screen_used=shot is not None,
              screen_text_sent=bool(screen_sent), suffix_sent=bool(phrase_request.suffix and config.provider.fim))
        settings = config.provider
        try:
            # Defaults reuse the injected/shared provider. Output preferences affect this request only.
            if config.multiline:
                settings = replace(settings, stop=())
            if config.completion_length == "short":
                settings = replace(settings, max_tokens=min(settings.max_tokens, 24))
            elif config.completion_length == "long":
                settings = replace(settings, max_tokens=max(settings.max_tokens, 120))
            if settings != service.config.provider:
                temporary_provider = PhraseProvider(settings, settings.api_key)
            stream = temporary_provider.stream(phrase_request, attempts, on_event=event) if temporary_provider else service._stream(phrase_request, attempts, on_event=event)
            emitted = ""
            async for text in stream:
                if first is None:
                    first = service.clock() - started
                    event("first_text")
                reply += text
                event("text_chunk", text=text)
                if config.avoid_phrases:
                    continue  # buffer to avoid showing part of an excluded phrase across chunk boundaries
                limited = limit_reply(reply, config)
                if limited != reply:
                    event("output_limited", shown=limited)
                delta = limited[len(emitted):]
                if delta:
                    self._emit(self._scheduler.chunk(delta, self._now()))
                emitted = limited
                if limited != reply:
                    break
            if config.avoid_phrases:
                limited = limit_reply(reply, config)
                if limited != reply:
                    event("output_limited", shown=limited)
                if limited:
                    self._emit(self._scheduler.chunk(limited, self._now()))
            outcome = "failover" if any(not a.ok for a in attempts) else "ok"
            service.note_success()
            service.record_provider(first, service.clock() - started, True)
            self._emit(self._scheduler.finished())
        except ProviderError as err:
            outcome = "failed"
            event("provider_error", error=str(err))
            logger.warning("phrase request failed: %s", err)
            service.note_failure()
            service.record_provider(first, None, False)
            self._emit(self._scheduler.failed())
        finally:
            event("finished", outcome=outcome)
            if temporary_provider is not None:
                await temporary_provider.aclose()
            if privacy_generation == service._privacy_generation:
                service.log.add(
                app=request.app,
                prompt=prompt,
                reply=reply,
                outcome=outcome,
                attempts=attempts,
                ttft_ms=None if first is None else round(first * 1000),
                total_ms=round((service.clock() - started) * 1000),
                screen=shot,
                screen_sent=screen_sent,
                suffix=phrase_request.suffix if settings.fim else "",
                settings={key: value for key, value in vars(settings).items() if key != "api_key"},
                events=events,
                )
            if self._tasks.get(request.id) is task:
                del self._tasks[request.id]
