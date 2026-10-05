"""Turns requests from the text service into replies, and learns from what the writer types."""

import logging
import time
import weakref
from collections.abc import Callable

from completionist_engine.config import Config, effective_config
from completionist_engine.learning import TypingLearner
from completionist_engine.metrics import Metrics
from completionist_engine.personal import PersonalStore
from completionist_engine.phrases import PhraseService, PhraseSession
from completionist_engine.policy import decide
from completionist_engine.protocol import PhraseUpdate, Request, WordReply
from completionist_engine.words import WordCompleter, current_word, previous_words

logger = logging.getLogger("completionist_engine")


class Engine:
    def __init__(
        self,
        completer: WordCompleter,
        config: Config,
        personal: PersonalStore | None = None,
        phrases: PhraseService | None = None,
        metrics: Metrics | None = None,
        clock: Callable[[], float] = time.monotonic,
    ) -> None:
        self._completer = completer
        self._config = config
        self._store = personal
        self._phrases = phrases
        self._metrics = metrics
        self._clock = clock
        self._paused = False
        self._paused_until: float | None = None
        self._private_mode = config.private_mode
        self._sessions: weakref.WeakSet[Session] = weakref.WeakSet()
        if self._phrases is not None and config.private_mode:
            self._phrases.set_private(True)

    @property
    def paused(self) -> bool:
        if self._paused_until is not None and self._clock() >= self._paused_until:
            self._paused = False
            self._paused_until = None
        return self._paused

    @paused.setter
    def paused(self, value: bool) -> None:
        self.set_paused(value)

    def set_paused(self, value: bool, *, minutes: float | None = None) -> None:
        self._paused = value
        duration = self._config.pause_minutes if minutes is None else minutes
        self._paused_until = self._clock() + duration * 60 if value and duration else None
        if value:
            self._suspend_sessions()

    @property
    def private_mode(self) -> bool:
        return self._private_mode

    def set_private_mode(self, enabled: bool) -> None:
        self._private_mode = enabled
        self._suspend_sessions()
        if self._phrases is not None:
            self._phrases.set_private(enabled)

    def _suspend_sessions(self) -> None:
        for session in self._sessions:
            session._learner.reset()
            if session._phrase is not None:
                session._phrase.suspend()

    @property
    def config(self) -> Config:
        return self._config

    @property
    def _personal(self) -> PersonalStore | None:
        return self._store if not self.private_mode else None

    def set_config(self, config: Config) -> None:
        """Apply a changed config to running sessions (app lists, word limit, phrase settings, learning)."""
        if config.learning and self._store is None:
            logger.warning("learning was turned on, but it starts only after the engine restarts")
        self._config = config
        self._suspend_sessions()
        self._completer.set_promote_after(config.promote_after)
        if self._phrases is not None:
            for context in (self._phrases.screen, self._phrases.accessible):
                if context is not None and hasattr(context, "clear"):
                    context.clear()
            self._phrases.reconfigure(config.phrase)
        self.set_private_mode(config.private_mode)

    def open_session(self, push: Callable[[PhraseUpdate], None] | None = None) -> "Session":
        """State for one connection (one app's text service): it follows that app's typing.

        `push` delivers streamed phrase updates; without it there are no phrases.
        """
        session = Session(self, push)
        self._sessions.add(session)
        return session

    def handle(self, request: Request) -> WordReply | None:
        """Handle a request with no memory of earlier ones on the same connection."""
        return Session(self, None).handle(request)


class Session:
    def __init__(self, engine: Engine, push: Callable[[PhraseUpdate], None] | None) -> None:
        self._engine = engine
        self._learner = TypingLearner()
        self._push = push
        self._app = ""
        self._words_open = False  # a word popup is currently showing (for counting how often one appears)
        self._phrase_open = False
        phrases = engine._phrases
        self._phrase: PhraseSession | None = (
            phrases.open_session(self._on_push) if phrases is not None and push is not None else None
        )

    def close(self) -> None:
        if self._phrase is not None:
            self._phrase.close()

    def _on_push(self, update: PhraseUpdate) -> None:
        self._note_phrase(update.text)
        if self._push is not None:
            self._push(update)

    def _note_phrase(self, text: str) -> None:
        """Count a phrase as shown when a row appears, not on every keystroke while it stays."""
        if text and not self._phrase_open and self._engine._metrics is not None:
            self._engine._metrics.record_shown(self._app, "phrase")
        self._phrase_open = bool(text)

    def handle(self, request: Request) -> WordReply | None:
        engine = self._engine
        self._app = request.app.lower()
        mode = decide(request.app, request.input_scope, engine._config)
        config = effective_config(engine._config, request.app)
        if engine.private_mode:
            mode = type(mode)(words=mode.words, phrase="off")
        if request.event == "accept":
            self._accept(request, mode.words)
            return None
        if request.event == "hotkey":
            if self._phrase is not None and mode.words and not engine.paused and not engine.private_mode:
                self._phrase.on_hotkey(request, mode.phrase, config=config.phrase)
            return None
        if request.event == "dismiss":
            if self._phrase is not None:
                self._phrase.on_dismiss()
            if engine._metrics is not None and (self._words_open or self._phrase_open):
                engine._metrics.record_dismiss(self._app)
            self._words_open = self._phrase_open = False
            return None
        if request.event != "keystroke":
            return None
        if engine.paused or not mode.words:
            self._learner.reset()
            self._words_open = self._phrase_open = False
            if self._phrase is not None:
                self._phrase.on_dismiss()  # cancel a phrase still showing or streaming
            status = self._phrase.display_status() if self._phrase is not None else {}
            return WordReply(id=request.id, replace=0, words=(), **status)
        if engine._personal is not None and config.learning:
            finished = self._learner.observe(request.before)
            if finished is not None:
                word, context = finished
                engine._personal.record_typed(word, context)
        else:
            self._learner.reset()
        completion = engine._completer.complete(request.before, limit=config.word_limit, typo_correction=config.typo_correction, tense_aware=config.tense_aware)
        tense = engine._completer.tense_of(request.before) if config.tense_aware else "none"
        words, kinds, origins = completion.words, ("word",) * len(completion.words), completion.origins
        marks: tuple[tuple[int, ...], ...] = completion.marks
        if config.next_words and not words:
            completion = engine._completer.next_words(
                request.before, limit=config.word_limit, threshold=config.next_threshold, tense_aware=config.tense_aware
            )
            words, kinds = completion.words, ("next",) * len(completion.words)
            origins = completion.origins
            marks = ()
        elif config.chunks and words:
            chunk_completion = engine._completer.chunks(request.before, seeds=completion.words)
            chunks = chunk_completion.words
            words = (*chunks, *words)[: config.word_limit]
            kinds = (*("chunk",) * len(chunks), *("word",) * len(completion.words))[: config.word_limit]
            marks = (*(() for _ in chunks), *completion.marks)[: config.word_limit]
            origins = (*chunk_completion.origins, *completion.origins)[: config.word_limit]
        if engine._metrics is not None and words and not request.quiet and not self._words_open:
            engine._metrics.record_shown(self._app, "word")
        self._words_open = bool(words) and not request.quiet
        phrase, phrase_done, phrase_mode = "", True, "off"
        if self._phrase is not None:
            # A quiet request (the popup is held back after Esc) keeps learning but asks for no phrase.
            update = self._phrase.on_request(request, "off" if request.quiet else mode.phrase, config=config.phrase)
            phrase, phrase_done = update.phrase, update.done
            self._note_phrase(phrase)
            if engine._phrases is not None and engine._phrases.available:
                phrase_mode = "hotkey" if config.phrase.preview_context and mode.phrase != "off" else mode.phrase
            status = self._phrase.display_status()
        else:
            status = {}
        return WordReply(
            id=request.id,
            replace=completion.replace,
            words=words,
            kinds=kinds if any(kind != "word" for kind in kinds) else (),
            marks=marks if any(marks) else (),
            phrase=phrase,
            phrase_done=phrase_done,
            phrase_mode=phrase_mode,
            popup=self._popup_settings(),
            origins=origins,
            tense=tense if tense != "none" else None,
            **status,
        )

    def _popup_settings(self) -> dict | None:
        config = self._engine.config
        defaults = Config()
        if (config.popup, config.partial_accept_hotkey, config.dismiss_hotkey) == (
            defaults.popup, defaults.partial_accept_hotkey, defaults.dismiss_hotkey
        ):
            return None
        return {"font_size": config.popup.font_size, "width_scale": config.popup.width_scale,
                "partial_accept": config.partial_accept_hotkey, "dismiss": config.dismiss_hotkey}

    def _accept(self, request: Request, words_allowed: bool) -> None:
        engine = self._engine
        if not words_allowed or not request.accepted or engine.paused:
            return
        if request.kind in ("word", "chunk", "next"):
            typed = len(current_word(request.before))
            if engine._metrics is not None:
                engine._metrics.record_accept(self._app, "word", max(len(request.accepted) - typed, 0))
            if engine._personal is not None and effective_config(engine.config, request.app).learning:
                context = previous_words(request.before, 2)
                for word in request.accepted.split():  # a chunk teaches each of its words in turn
                    engine._personal.record_accepted(word, context)
                    context = (*context, word.lower())[-2:]
            self._words_open = False
        else:
            if engine._metrics is not None:
                engine._metrics.record_accept(self._app, request.kind, len(request.accepted))
            if request.kind == "phrase":
                self._phrase_open = False
