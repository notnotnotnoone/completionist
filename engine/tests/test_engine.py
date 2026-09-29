import pytest

from typer_engine.config import Config
from typer_engine.engine import Engine
from typer_engine.protocol import Request, WordReply
from typer_engine.words import WordCompleter

VOCAB = [("world", 5.8), ("work", 6.0), ("worry", 5.0), ("worse", 5.2)]
CONFIG = Config(block=frozenset({"code.exe"}), allow=frozenset(), word_limit=2)


@pytest.fixture
def engine():
    return Engine(WordCompleter(VOCAB), CONFIG)


def keystroke(before: str, app: str = "discord.exe", scope: tuple[str, ...] = ()) -> Request:
    return Request(id=9, event="keystroke", app=app, input_scope=scope, before=before)


def test_keystroke_gets_ranked_words_up_to_the_configured_limit(engine):
    assert engine.handle(keystroke("hello wor")) == WordReply(id=9, replace=3, words=("work", "world"))


def test_keystroke_in_block_listed_app_gets_an_empty_reply(engine):
    assert engine.handle(keystroke("hello wor", app="Code.exe")) == WordReply(id=9, replace=0, words=())


def test_keystroke_in_password_field_gets_an_empty_reply(engine):
    assert engine.handle(keystroke("hunter", scope=("IS_PASSWORD",))) == WordReply(id=9, replace=0, words=())


def test_keystroke_after_a_space_gets_an_empty_reply(engine):
    assert engine.handle(keystroke("hello ")) == WordReply(id=9, replace=0, words=())


@pytest.mark.parametrize("event", ["accept", "dismiss"])
def test_accept_and_dismiss_need_no_reply(engine, event):
    assert engine.handle(Request(id=1, event=event)) is None
