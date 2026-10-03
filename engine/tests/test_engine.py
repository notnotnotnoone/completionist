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


def test_origins_follow_candidates_through_chunks_casing_and_deduplication():
    personal = PersonalStore()
    personal.record_accepted("work", ())
    personal.record_accepted("work", ())
    personal.record_accepted("worldwide", ())
    personal.record_accepted("worldwide", ())
    config = Config(block=frozenset(), allow=frozenset(), word_limit=5, chunks=False)
    engine = Engine(WordCompleter(VOCAB, personal=personal, promote_after=2), config, personal=personal)

    reply = engine.handle(keystroke("Hello Wor"))

    by_lower_word = {word.lower(): origin for word, origin in zip(reply.words, reply.origins)}
    assert by_lower_word["work"] == "local"  # the personal boost does not change its provenance
    assert by_lower_word["worldwide"] == "learned"
    assert len(reply.origins) == len(reply.words)
    assert len({word.lower() for word in reply.words}) == len(reply.words)  # casing/dedup kept alignment


# --- next words -------------------------------------------------------------------------------------

from completionist_engine.counts import Counts  # noqa: E402


class _After:
    """After "to", "know" follows 70% of the time."""

    def counts(self, context, prefix):
        if context == ("to",):
            return Counts({w: n for w, n in {"know": 70, "see": 30}.items() if w.startswith(prefix)}, 100)
        return Counts({}, 0)


NEXT_VOCAB = VOCAB + [("know", 6.0), ("see", 6.0)]


def next_engine(**config) -> Engine:
    base = Config(block=frozenset(), allow=frozenset(), word_limit=5, **config)
    return Engine(WordCompleter(NEXT_VOCAB, ngrams=_After()), base)


def test_a_space_brings_the_likely_next_words():
    engine = next_engine(next_words=True, next_threshold=0.1)
    assert engine.handle(keystroke("I'd like to ")) == WordReply(id=9, replace=0, words=("know", "see"), kinds=("next", "next"))


def test_next_words_stay_off_when_switched_off():
    assert next_engine(next_words=False).handle(keystroke("I'd like to ")) == WordReply(id=9, replace=0, words=())


def test_next_words_respect_the_threshold_and_the_quiet_rules():
    assert next_engine(next_words=True, next_threshold=0.8).handle(keystroke("I'd like to ")).words == ()
    engine = next_engine(next_words=True, next_threshold=0.1)
    assert engine.handle(keystroke("I'd like to ", scope=("IS_PASSWORD",))).words == ()
    engine.paused = True
    assert engine.handle(keystroke("I'd like to ")).words == ()


def test_typing_a_letter_goes_back_to_normal_completion():
    engine = next_engine(next_words=True, next_threshold=0.1)
    assert engine.handle(keystroke("I'd like to k")).words == ("know",)


# --- chunks and kinds -------------------------------------------------------------------------------


class _Chains:
    """After "to": know 70%, see 30%. After "know": about 80%, it 20%."""

    def counts(self, context, prefix):
        table = {("to",): {"know": 70, "see": 30}, ("know",): {"about": 80, "it": 20}}
        row = table.get(context[-1:]) if len(context) == 1 else None
        if row is None:
            return Counts({}, 0)
        return Counts({w: n for w, n in row.items() if w.startswith(prefix)}, sum(row.values()))


CHAIN_VOCAB = VOCAB + [("know", 6.0), ("see", 6.0), ("about", 6.0), ("it", 9.0), ("known", 3.0)]


def chain_engine(**config) -> Engine:
    base = Config(block=frozenset(), allow=frozenset(), word_limit=3, **config)
    personal = PersonalStore()
    return Engine(WordCompleter(CHAIN_VOCAB, ngrams=_Chains(), personal=personal), base, personal=personal), personal


def test_chunks_come_before_words_and_each_suggestion_names_its_kind():
    engine, _ = chain_engine(chunks=True)
    reply = engine.handle(keystroke("I'd like to kn"))
    assert reply.words == ("know about", "know", "known")
    assert reply.kinds == ("chunk", "word", "word")
    assert reply.replace == 2


def test_chunks_stay_off_when_switched_off():
    engine, _ = chain_engine(chunks=False)
    reply = engine.handle(keystroke("I'd like to kn"))
    assert reply.words == ("know", "known") and set(reply.kinds) <= {"word"}


def test_next_words_are_named_next():
    engine, _ = chain_engine(next_words=True, next_threshold=0.1)
    reply = engine.handle(keystroke("I'd like to "))
    assert reply.words == ("know", "see") and reply.kinds == ("next", "next")


class _OutOfBaseNext:
    def counts(self, context, prefix):
        if context == ("to",):
            return Counts({"woogle": 90} if "woogle".startswith(prefix) else {}, 100)
        return Counts({}, 0)


def test_next_word_origin_requires_personal_promotion_outside_the_base_dictionary():
    config = Config(block=frozenset(), allow=frozenset(), word_limit=5, next_words=True, next_threshold=0.1)
    corpus_only = Engine(WordCompleter(VOCAB, ngrams=_OutOfBaseNext()), config)
    assert corpus_only.handle(keystroke("I'd like to ")).words == ()

    personal = PersonalStore()
    personal.record_accepted("woggle", ("to",))
    personal.record_accepted("woggle", ("to",))
    promoted = Engine(WordCompleter(VOCAB, ngrams=_OutOfBaseNext(), personal=personal, promote_after=2), config,
                      personal=personal)
    reply = promoted.handle(keystroke("I'd like to "))
    assert reply.words == ("woggle",)
    assert reply.origins == ("learned",)


def test_the_list_is_never_longer_than_the_word_limit():
    engine, _ = chain_engine(chunks=True)
    assert len(engine.handle(keystroke("I'd like to kn")).words) <= 3


def accept(engine, accepted: str, kind: str, before: str):
    session = engine.open_session()
    session.handle(Request(id=2, event="accept", app="discord.exe", before=before, accepted=accepted, kind=kind))


def test_accepting_a_next_word_teaches_it_like_an_accepted_word():
    engine, personal = chain_engine()
    accept(engine, "know", "next", "I'd like to ")
    assert personal.counts(("to",), "kn").words["know"] == 1


def test_accepting_a_chunk_teaches_each_of_its_words_in_order():
    engine, personal = chain_engine()
    accept(engine, "know about", "chunk", "I'd like to kn")
    assert personal.counts(("to",), "kn").words["know"] == 1
    assert personal.counts(("know",), "ab").words["about"] == 1
