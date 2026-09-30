"""Turns requests from the text service into replies, and learns from what the writer types."""

from collections.abc import Callable

from typer_engine.config import Config
from typer_engine.learning import TypingLearner
from typer_engine.metrics import Metrics
from typer_engine.personal import PersonalStore
from typer_engine.phrases import PhraseService, PhraseSession
from typer_engine.policy import decide
from typer_engine.protocol import PhraseUpdate, Request, WordReply
from typer_engine.words import WordCompleter, current_word, previous_words


class Engine:
    def __init__(
        self,
        completer: WordCompleter,
        config: Config,
        personal: PersonalStore | None = None,
        phrases: PhraseService | None = None,
        metrics: Metrics | None = None,
    ) -> None:
        self._completer = completer
        self._config = config
        self._store = personal
        self._phrases = phrases
        self._metrics = metrics
        self.paused = False
        """While paused the engine answers every request with nothing, so the text service goes quiet."""

    @property
    def config(self) -> Config:
        return self._config

    @property
    def _personal(self) -> PersonalStore | None:
        return self._store if self._config.learning else None

    def set_config(self, config: Config) -> None:
        """Apply a changed config to running sessions (app lists, word limit, phrase settings, learning)."""
        self._config = config
        if self._phrases is not None:
            self._phrases.reconfigure(config.phrase)

    def open_session(self, push: Callable[[PhraseUpdate], None] | None = None) -> "Session":
        """State for one connection (one app's text service): it follows that app's typing.

        `push` delivers streamed phrase updates; without it there are no phrases.
        """
        return Session(self, push)

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
        if request.event == "accept":
            self._accept(request, mode.words)
            return None
        if request.event == "hotkey":
            if self._phrase is not None and mode.words and not engine.paused:
                self._phrase.on_hotkey(request, mode.phrase)
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
                self._phrase.on_request(request, "off")
            return WordReply(id=request.id, replace=0, words=())
        if engine._personal is not None:
            finished = self._learner.observe(request.before)
            if finished is not None:
                word, context = finished
                engine._personal.record_typed(word, context)
        completion = engine._completer.complete(request.before, limit=engine._config.word_limit)
        if engine._metrics is not None and completion.words and not request.quiet and not self._words_open:
            engine._metrics.record_shown(self._app, "word")
        self._words_open = bool(completion.words) and not request.quiet
        phrase, phrase_done, phrase_mode = "", True, "off"
        if self._phrase is not None:
            # A quiet request (the popup is held back after Esc) keeps learning but asks for no phrase.
            update = self._phrase.on_request(request, "off" if request.quiet else mode.phrase)
            phrase, phrase_done = update.phrase, update.done
            self._note_phrase(phrase)
            if engine._phrases is not None and engine._phrases.available:
                phrase_mode = mode.phrase
        return WordReply(
            id=request.id,
            replace=completion.replace,
            words=completion.words,
            phrase=phrase,
            phrase_done=phrase_done,
            phrase_mode=phrase_mode,
        )

    def _accept(self, request: Request, words_allowed: bool) -> None:
        engine = self._engine
        if not words_allowed or not request.accepted:
            return
        if request.kind == "word":
            typed = len(current_word(request.before))
            if engine._metrics is not None:
                engine._metrics.record_accept(self._app, "word", max(len(request.accepted) - typed, 0))
            if engine._personal is not None:
                engine._personal.record_accepted(request.accepted, previous_words(request.before, 2))
            self._words_open = False
        else:
            if engine._metrics is not None:
                engine._metrics.record_accept(self._app, request.kind, len(request.accepted))
            if request.kind == "phrase":
                self._phrase_open = False
