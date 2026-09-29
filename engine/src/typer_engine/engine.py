"""Turns requests from the text service into replies."""

from typer_engine.config import Config
from typer_engine.policy import decide
from typer_engine.protocol import Request, WordReply
from typer_engine.words import WordCompleter


class Engine:
    def __init__(self, completer: WordCompleter, config: Config) -> None:
        self._completer = completer
        self._config = config

    def handle(self, request: Request) -> WordReply | None:
        if request.event != "keystroke":
            return None
        if not decide(request.app, request.input_scope, self._config).words:
            return WordReply(id=request.id, replace=0, words=())
        completion = self._completer.complete(request.before, limit=self._config.word_limit)
        return WordReply(id=request.id, replace=completion.replace, words=completion.words)
