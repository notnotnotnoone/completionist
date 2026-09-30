"""Runs phrase requests: the scheduler decides, this asks the provider and pushes results as they stream."""

import asyncio
import logging
import time
from collections.abc import Callable

from typer_engine.budget import DailyBudget, Usage
from typer_engine.config import PhraseConfig
from typer_engine.context import anchored_window, build_prompt, trim_suffix
from typer_engine.phrase_provider import PhraseProvider, PhraseRequest, ProviderError
from typer_engine.phrase_scheduler import Action, Cancel, Mode, PhraseScheduler, Start, Update
from typer_engine.protocol import PhraseUpdate, Request

logger = logging.getLogger("typer_engine.phrases")

_FAILURES_BEFORE_PAUSE = 3
_PAUSE_SECONDS = 30.0


class PhraseService:
    """Shared by every connection: the provider, the daily budget and the health of the provider."""

    def __init__(
        self,
        config: PhraseConfig,
        api_key: str | None,
        budget: DailyBudget,
        *,
        provider: PhraseProvider | None = None,
        clock: Callable[[], float] = time.monotonic,
    ) -> None:
        self.config = config
        self.budget = budget
        self.clock = clock
        self._provider = provider
        if provider is None and api_key and config.enabled:
            self._provider = PhraseProvider(config.provider, api_key)
        self._failures = 0
        self._paused_until = 0.0

    @property
    def available(self) -> bool:
        return self.config.enabled and self._provider is not None and self.config.daily_budget_usd > 0

    def usable(self) -> bool:
        """Whether a new request may start right now."""
        return self.available and not self.budget.exhausted and self.clock() >= self._paused_until

    def note_success(self) -> None:
        self._failures = 0

    def note_failure(self) -> None:
        self._failures += 1
        if self._failures >= _FAILURES_BEFORE_PAUSE:
            self._paused_until = self.clock() + _PAUSE_SECONDS
            self._failures = 0
            logger.warning("phrase provider failed %d times in a row; pausing phrases for %ds", _FAILURES_BEFORE_PAUSE, _PAUSE_SECONDS)

    def open_session(self, push: Callable[[PhraseUpdate], None]) -> "PhraseSession":
        return PhraseSession(self, push)

    async def aclose(self) -> None:
        if self._provider is not None:
            await self._provider.aclose()

    def _stream(self, request: PhraseRequest):
        assert self._provider is not None
        return self._provider.stream(request)


class PhraseSession:
    """One connection's phrase state: a scheduler, the running request and the pause timer."""

    def __init__(self, service: PhraseService, push: Callable[[PhraseUpdate], None]) -> None:
        self._service = service
        self._push = push
        self._scheduler = PhraseScheduler(service.config.debounce)
        self._tasks: dict[int, asyncio.Task] = {}
        self._timer: asyncio.TimerHandle | None = None

    # -- called for each event from the text service ---------------------------------------------

    def on_request(self, request: Request, mode: Mode) -> Update:
        if not self._service.available:
            mode = "off"
        update = self._scheduler.request(request, mode, self._now(), self._service.usable())
        self._apply(update.actions)
        self._arm_timer()
        return update

    def on_hotkey(self, request: Request, mode: Mode) -> Update:
        if not self._service.available:
            mode = "off"
        update = self._scheduler.hotkey(request, mode, self._now(), self._service.usable())
        self._apply(update.actions)
        self._arm_timer()
        return update

    def on_dismiss(self) -> None:
        self._apply(self._scheduler.dismiss())
        self._arm_timer()

    def close(self) -> None:
        self._apply(self._scheduler.dismiss())
        if self._timer is not None:
            self._timer.cancel()
            self._timer = None
        for task in list(self._tasks.values()):
            task.cancel()
        self._tasks.clear()

    # -- internals -------------------------------------------------------------------------------

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
        self._tasks[request.id] = loop.create_task(self._run(request))

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
        update = self._scheduler.tick(self._now())
        self._apply(update.actions)
        self._arm_timer()

    def _emit(self, update: Update | None) -> None:
        if update is None or update.request_id is None:
            return
        self._apply(update.actions)
        self._arm_timer()
        self._push(PhraseUpdate(id=update.request_id, text=update.phrase, done=update.done))

    async def _run(self, request: Request) -> None:
        config = self._service.config
        prompt = build_prompt(request.app, request.title, anchored_window(request.before, config.context_before))
        phrase_request = PhraseRequest(prompt=prompt, suffix=trim_suffix(request.after, config.context_after))
        task = asyncio.current_task()
        try:
            async for event in self._service._stream(phrase_request):
                if isinstance(event, Usage):
                    self._service.budget.record(event)
                else:
                    self._emit(self._scheduler.chunk(event, self._now()))
            self._service.note_success()
            self._emit(self._scheduler.finished())
        except ProviderError as err:
            logger.info("phrase request failed: %s", err)
            self._service.note_failure()
            self._emit(self._scheduler.failed())
        finally:
            if self._tasks.get(request.id) is task:
                del self._tasks[request.id]
