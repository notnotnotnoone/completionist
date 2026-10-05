from completionist_engine.tense import detect_tense, load_tense_forms
from completionist_engine.words import WordCompleter

FORMS = load_tense_forms()
VOCAB = [("walk", 100.0), ("walked", 100.0), ("walks", 100.0), ("walking", 100.0), ("red", 50.0), ("need", 50.0), ("needed", 50.0)]


def words(before: str, *, tense_aware: bool = True) -> tuple[str, ...]:
    # A personal store with no counts forces the full ranking path, where tense applies.
    completer = WordCompleter(VOCAB, tense_forms=FORMS, fuzzy=None, personal=_Empty())
    return completer.complete(before, tense_aware=tense_aware).words


class _Empty:
    def counts(self, context, prefix):
        from completionist_engine.counts import NO_COUNTS
        return NO_COUNTS


def sentence(text: str):
    from completionist_engine.words import sentence_words
    return detect_tense(sentence_words(text), FORMS)


def test_detects_past_and_present():
    assert sentence("Yesterday she walked home ") == "past"
    assert sentence("He is here and she goes ") == "present"
    assert sentence("We went there ") == "past"
    assert sentence("It happened two days ago ") == "past"
    assert sentence("Today I ") == "present"


def test_mixed_or_unclear_is_none():
    assert sentence("Yesterday she goes ") == "none"
    assert sentence("The dog ") == "none"
    assert sentence("I walk ") == "none"


def test_participles_and_lookalikes_are_not_past_tense():
    assert sentence("She has walked ") == "present"  # "walked" after "has" is a participle, so only "has" counts
    assert sentence("She had walked ") == "past"
    assert sentence("The red ") == "none"
    assert sentence("I need ") == "none"


def test_new_sentence_resets():
    assert sentence("Yesterday she walked home. Then ") == "none"


def test_past_sentence_prefers_past_form_and_demotes_present():
    ranked = words("Yesterday I wal")
    assert ranked.index("walked") < ranked.index("walks")
    assert ranked.index("walked") < ranked.index("walk")


def test_present_sentence_prefers_present_form():
    ranked = words("Today she is wal")
    assert ranked.index("walks") < ranked.index("walked")


def test_unclear_tense_leaves_ranking_unchanged():
    assert words("Yesterday she goes wal") == words("Yesterday she goes wal", tense_aware=False)
    assert words("I wal") == words("I wal", tense_aware=False)


def test_switch_off_leaves_ranking_unchanged():
    assert words("Yesterday I wal", tense_aware=False) != words("Yesterday I wal")


def test_reply_carries_tense_only_when_clear():
    from completionist_engine.config import Config
    from completionist_engine.engine import Engine
    from completionist_engine.protocol import Request

    engine = Engine(WordCompleter(VOCAB, tense_forms=FORMS), Config(chunks=False, next_words=False))

    def reply(before: str):
        return engine.handle(Request(id=1, event="keystroke", before=before, app="notepad.exe")).to_message()

    assert reply("Yesterday we wal")["tense"] == "past"
    assert "tense" not in reply("I wal")
