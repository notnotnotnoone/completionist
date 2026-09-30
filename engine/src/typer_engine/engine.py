"""Turns requests from the text service into replies, and learns from what the writer types."""

from typer_engine.config import Config
from typer_engine.learning import TypingLearner
from typer_engine.personal import PersonalStore
from typer_engine.policy import decide
from typer_engine.protocol import Request, WordReply
from typer_engine.words import WordCompleter, previous_words


class Engine:
    def __init__(self, completer: WordCompleter, config: Config, personal: PersonalStore | None = None) -> None:
        self._completer = completer
        self._config = config
        self._personal = personal if config.learning else None

    def open_session(self) -> "Session":
        """State for one connection (one app's text service): it follows that app's typing."""
        return Session(self)

    def handle(self, request: Request) -> WordReply | None:
        """Handle a request with no memory of earlier ones on the same connection."""
        return Session(self).handle(request)


class Session:
    def __init__(self, engine: Engine) -> None:
        self._engine = engine
        self._learner = TypingLearner()

    def handle(self, request: Request) -> WordReply | None:
        engine = self._engine
        words_allowed = decide(request.app, request.input_scope, engine._config).words
        if request.event == "accept":
            self._learn_accept(request, words_allowed)
            return None
        if request.event != "keystroke":
            return None
        if not words_allowed:
            self._learner.reset()
            return WordReply(id=request.id, replace=0, words=())
        if engine._personal is not None:
            finished = self._learner.observe(request.before)
            if finished is not None:
                word, context = finished
                engine._personal.record_typed(word, context)
        completion = engine._completer.complete(request.before, limit=engine._config.word_limit)
        return WordReply(id=request.id, replace=completion.replace, words=completion.words)

    def _learn_accept(self, request: Request, words_allowed: bool) -> None:
        personal = self._engine._personal
        if personal is not None and words_allowed and request.accepted:
            personal.record_accepted(request.accepted, previous_words(request.before, 2))
