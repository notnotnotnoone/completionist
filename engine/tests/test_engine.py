import pytest

from completionist_engine.config import Config
from completionist_engine.engine import Engine
from completionist_engine.protocol import Request, WordReply
from completionist_engine.words import WordCompleter

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


# --- learning ---------------------------------------------------------------------------------

from completionist_engine.personal import PersonalStore  # noqa: E402

LEARNING_CONFIG = Config(block=frozenset({"code.exe"}), allow=frozenset(), word_limit=5)


def learning_engine(config: Config = LEARNING_CONFIG):
    personal = PersonalStore()
    return Engine(WordCompleter(VOCAB, personal=personal), config, personal=personal), personal


def type_through(session, text: str, app: str = "discord.exe", scope: tuple[str, ...] = ()) -> None:
    for i in range(1, len(text) + 1):
        session.handle(Request(id=i, event="keystroke", app=app, input_scope=scope, before=text[:i]))


def test_words_you_finish_typing_are_learned():
    engine, personal = learning_engine()
    type_through(engine.open_session(), "hey linqi ")
    assert dict(personal.counts((), "lin").words) == {"linqi": 1}
    assert dict(personal.counts(("hey",), "lin").words) == {"linqi": 1}


def test_words_are_learned_only_from_typing_not_from_pasted_text():
    engine, personal = learning_engine()
    session = engine.open_session()
    session.handle(keystroke("hey "))
    session.handle(keystroke("hey a pasted paragraph of text "))
    assert personal.counts((), "").total == 0


def test_each_connection_tracks_its_own_typing():
    engine, personal = learning_engine()
    first, second = engine.open_session(), engine.open_session()
    first.handle(keystroke("alpha"))
    second.handle(keystroke("beta"))
    first.handle(keystroke("alpha "))
    assert dict(personal.counts((), "").words) == {"alpha": 1}


@pytest.mark.parametrize(
    ("app", "scope"),
    [("code.exe", ()), ("discord.exe", ("IS_PASSWORD",)), ("discord.exe", ("IS_URL",)), ("discord.exe", ("IS_EMAIL_USERNAME",))],
)
def test_nothing_is_learned_where_suggestions_are_silent(app, scope):
    engine, personal = learning_engine()
    type_through(engine.open_session(), "hunter two ", app=app, scope=scope)
    assert personal.counts((), "").total == 0


def test_nothing_is_learned_when_learning_is_off():
    engine, personal = learning_engine(Config(block=frozenset(), allow=frozenset(), learning=False))
    type_through(engine.open_session(), "hey linqi ")
    assert personal.counts((), "").total == 0


def test_accepting_a_word_teaches_the_word_and_what_came_before_it():
    engine, personal = learning_engine()
    engine.open_session().handle(Request(id=1, event="accept", app="discord.exe", before="I want to recomm", accepted="recommend"))
    assert dict(personal.counts((), "rec").words) == {"recommend": 1}
    assert dict(personal.counts(("to",), "rec").words) == {"recommend": 1}


def test_accepts_in_silent_places_teach_nothing():
    engine, personal = learning_engine()
    session = engine.open_session()
    session.handle(Request(id=1, event="accept", app="code.exe", before="wor", accepted="world"))
    session.handle(Request(id=2, event="accept", app="discord.exe", input_scope=("IS_PASSWORD",), before="wor", accepted="world"))
    assert personal.counts((), "").total == 0


def test_an_accept_without_a_word_teaches_nothing():
    engine, personal = learning_engine()
    engine.open_session().handle(Request(id=1, event="accept", app="discord.exe", before="wor"))
    assert personal.counts((), "").total == 0


def test_accept_and_dismiss_still_need_no_reply_with_a_session():
    engine, _ = learning_engine()
    session = engine.open_session()
    assert session.handle(Request(id=1, event="accept", app="discord.exe", accepted="world")) is None
    assert session.handle(Request(id=2, event="dismiss")) is None


def test_what_you_type_soon_shows_up_in_suggestions():
    engine, _ = learning_engine()
    session = engine.open_session()
    for _ in range(4):
        type_through(session, "hello worldwide ")
        session.handle(keystroke(""))
    reply = session.handle(keystroke("hi worldw"))
    assert "worldwide" in reply.words


def test_an_engine_without_a_personal_store_still_works():
    engine = Engine(WordCompleter(VOCAB), CONFIG)
    session = engine.open_session()
    assert session.handle(keystroke("hello wor")).words == ("work", "world")
