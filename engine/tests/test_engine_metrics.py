from datetime import date

import pytest

from typer_engine.config import Config, PhraseConfig
from typer_engine.engine import Engine
from typer_engine.metrics import Metrics
from typer_engine.protocol import Request
from typer_engine.words import WordCompleter

VOCAB = [("recommend", 6.0), ("recommended", 5.0), ("world", 5.8), ("work", 6.0)]
CONFIG = Config(block=frozenset({"code.exe"}), allow=frozenset(), phrase=PhraseConfig(enabled=False))


@pytest.fixture
def parts():
    metrics = Metrics(today=lambda: date(2026, 9, 29))
    engine = Engine(WordCompleter(VOCAB), CONFIG, metrics=metrics)
    return engine, metrics, engine.open_session()


def key(text, app="discord.exe", request_id=1, **kwargs):
    return Request(id=request_id, event="keystroke", app=app, before=text, **kwargs)


def test_a_popup_is_counted_once_however_many_keystrokes_it_stays_up(parts):
    _, metrics, session = parts
    for i, text in enumerate(["reco", "recom", "recomm", "recomme"], start=1):
        session.handle(key(text, request_id=i))
    assert metrics.summary().shown_words == 1


def test_a_popup_that_closes_and_reopens_is_counted_again(parts):
    _, metrics, session = parts
    session.handle(key("reco"))
    session.handle(key("reco "))  # space: closes
    session.handle(key("reco wo"))
    assert metrics.summary().shown_words == 2


def test_a_quiet_request_is_not_counted_as_shown(parts):
    _, metrics, session = parts
    session.handle(key("reco", quiet=True))
    assert metrics.summary().shown_words == 0


def test_accepting_a_word_counts_the_characters_it_added_minus_the_tab(parts):
    _, metrics, session = parts
    session.handle(key("I recomm"))
    session.handle(Request(id=2, event="accept", app="discord.exe", before="I recomm", accepted="recommend"))
    s = metrics.summary()
    assert s.accepted_words == 1
    assert s.keystrokes_saved == (len("recommend") - len("recomm")) - 1


def test_accepting_a_phrase_counts_all_its_characters(parts):
    _, metrics, session = parts
    session.handle(Request(id=2, event="accept", app="discord.exe", before="Hello wor", accepted="ld is big", kind="phrase"))
    session.handle(Request(id=3, event="accept", app="discord.exe", before="Hello", accepted=" there", kind="phrase_word"))
    s = metrics.summary()
    assert (s.accepted_phrases, s.accepted_partial) == (1, 1)
    assert s.keystrokes_saved == (9 - 1) + (6 - 1)


def test_a_phrase_accept_does_not_teach_the_word_store(parts):
    engine, _, session = parts
    from typer_engine.personal import PersonalStore

    engine._store = PersonalStore()
    session.handle(Request(id=2, event="accept", app="discord.exe", before="Hello wor", accepted="ld is big", kind="phrase"))
    assert engine._store.counts((), "").total == 0


def test_dismissing_an_open_popup_is_counted_but_dismissing_nothing_is_not(parts):
    _, metrics, session = parts
    session.handle(Request(id=1, event="dismiss", app="discord.exe"))
    assert metrics.summary().dismissed == 0
    session.handle(key("reco"))
    session.handle(Request(id=2, event="dismiss", app="discord.exe"))
    assert metrics.summary().dismissed == 1


def test_silent_apps_record_nothing(parts):
    _, metrics, session = parts
    session.handle(key("reco", app="code.exe"))
    session.handle(Request(id=2, event="accept", app="code.exe", before="reco", accepted="recommend"))
    s = metrics.summary()
    assert (s.shown_words, s.accepted_words) == (0, 0)


def test_metrics_are_kept_per_app(parts):
    _, metrics, session = parts
    session.handle(key("reco", app="Discord.exe"))
    other = parts[0].open_session()
    other.handle(key("wor", app="notepad.exe"))
    assert set(metrics.summary().per_app) == {"discord.exe", "notepad.exe"}


def test_pausing_makes_every_request_silent_and_resuming_restores_it(parts):
    engine, metrics, session = parts
    assert session.handle(key("reco")).words
    engine.paused = True
    reply = session.handle(key("recom"))
    assert reply.words == () and reply.phrase_mode == "off"
    assert session.handle(Request(id=3, event="hotkey", app="discord.exe", before="x")) is None
    engine.paused = False
    assert session.handle(key("reco")).words


def test_nothing_is_learned_while_paused():
    from typer_engine.personal import PersonalStore

    store = PersonalStore()
    engine = Engine(WordCompleter(VOCAB), CONFIG, personal=store)
    session = engine.open_session()
    engine.paused = True
    for text in ["hey", "hey "]:
        session.handle(key(text))
    assert store.counts((), "").total == 0


def test_a_changed_config_applies_to_running_sessions(parts):
    engine, _, session = parts
    assert session.handle(key("wor")).words
    engine.set_config(Config(block=frozenset({"discord.exe"}), allow=frozenset(), word_limit=1, phrase=PhraseConfig(enabled=False)))
    assert session.handle(key("wor")).words == ()  # discord is now block-listed
    assert len(session.handle(key("wor", app="notepad.exe")).words) == 1  # and the word limit is 1


def test_turning_learning_off_in_the_config_stops_learning_at_once():
    from typer_engine.personal import PersonalStore

    store = PersonalStore()
    engine = Engine(WordCompleter(VOCAB), CONFIG, personal=store)
    session = engine.open_session()
    engine.set_config(Config(block=frozenset(), allow=frozenset(), learning=False, phrase=PhraseConfig(enabled=False)))
    for text in ["hey", "hey "]:
        session.handle(key(text))
    assert store.counts((), "").total == 0
