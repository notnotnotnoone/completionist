"""Turns requests from the text service into replies, and learns from what the writer types."""

from collections.abc import Callable

from typer_engine.config import Config
from typer_engine.learning import TypingLearner
from typer_engine.personal import PersonalStore
from typer_engine.phrases import PhraseService, PhraseSession
from typer_engine.policy import decide
from typer_engine.protocol import PhraseUpdate, Request, WordReply
from typer_engine.words import WordCompleter, previous_words


class Engine:
    def __init__(
        self,
        completer: WordCompleter,
        config: Config,
        personal: PersonalStore | None = None,
        phrases: PhraseService | None = None,
    ) -> None:
        self._completer = completer
        self._config = config
        self._personal = personal if config.learning else None
        self._phrases = phrases

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
        phrases = engine._phrases
        self._phrase: PhraseSession | None = (
            phrases.open_session(push) if phrases is not None and push is not None else None
        )

    def close(self) -> None:
        if self._phrase is not None:
            self._phrase.close()

    def handle(self, request: Request) -> WordReply | None:
        engine = self._engine
        mode = decide(request.app, request.input_scope, engine._config)
        if request.event == "accept":
            self._learn_accept(request, mode.words)
            return None
        if request.event == "hotkey":
            if self._phrase is not None and mode.words:
                self._phrase.on_hotkey(request, mode.phrase)
            return None
        if request.event == "dismiss":
            if self._phrase is not None:
                self._phrase.on_dismiss()
            return None
        if request.event != "keystroke":
            return None
        if not mode.words:
            self._learner.reset()
            if self._phrase is not None:
                self._phrase.on_request(request, "off")
            return WordReply(id=request.id, replace=0, words=())
        if engine._personal is not None:
            finished = self._learner.observe(request.before)
            if finished is not None:
                word, context = finished
                engine._personal.record_typed(word, context)
        completion = engine._completer.complete(request.before, limit=engine._config.word_limit)
        phrase, phrase_done, phrase_mode = "", True, "off"
        if self._phrase is not None:
            update = self._phrase.on_request(request, mode.phrase)
            phrase, phrase_done = update.phrase, update.done
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

    def _learn_accept(self, request: Request, words_allowed: bool) -> None:
        personal = self._engine._personal
        if personal is not None and words_allowed and request.accepted:
            personal.record_accepted(request.accepted, previous_words(request.before, 2))
