"""Context and personal re-ranking of word completions."""

import pytest

from completionist_engine.counts import NO_COUNTS, Counts
from completionist_engine.words import WordCompleter, previous_words, sentence_words


class FakeCounts:
    """A count source backed by a dict: {context tuple: {word: count}}; totals default to the sum."""

    def __init__(self, table: dict[tuple[str, ...], dict[str, int]], totals: dict[tuple[str, ...], int] | None = None):
        self._table = table
        self._totals = totals or {}
        self.queries: list[tuple[tuple[str, ...], str]] = []

    def counts(self, context: tuple[str, ...], prefix: str) -> Counts:
        self.queries.append((context, prefix))
        row = self._table.get(context)
        if row is None:
            return NO_COUNTS
        total = self._totals.get(context, sum(row.values()))
        return Counts({w: n for w, n in row.items() if w.startswith(prefix)}, total)


# "knowledge" outranks "know" outranks "knew" on frequency alone.
VOCAB = [("knowledge", 6.0), ("know", 5.0), ("knew", 4.0), ("known", 3.5), ("the", 9.0), ("they", 8.0), ("then", 7.0)]


@pytest.mark.parametrize(
    ("before", "expected"),
    [
        ("I would like to kn", ("like", "to")),
        ("I would like to ", ("like", "to")),
        ("Hello. I would kn", ("i", "would")),  # a sentence break resets the context
        ("wait!\nnow kn", ("now",)),
        ("in 2020 the kn", ("the",)),  # a number resets it too
        ("kn", ()),
        ("", ()),
        ("don't kn", ("don't",)),
        ("Well, you kn", ("well", "you")),
    ],
)
def test_previous_words_are_the_complete_words_before_the_one_being_typed(before, expected):
    assert previous_words(before, 2) == expected


def test_previous_words_honours_the_limit():
    assert previous_words("one two three fo", 1) == ("three",)


def test_sentence_words_includes_the_trailing_word():
    assert sentence_words("Hello. I would like") == ["i", "would", "like"]


def test_without_any_context_frequency_decides():
    assert WordCompleter(VOCAB).complete("kn").words == ("knowledge", "know", "knew", "known")


def test_a_bigram_moves_the_word_that_follows_the_previous_word_to_the_top():
    ngrams = FakeCounts({("to",): {"know": 50, "knew": 5, "knowledge": 1}})
    completer = WordCompleter(VOCAB, ngrams=ngrams)
    assert completer.complete("I want to kn").words[0] == "know"
    assert completer.complete("kn").words[0] == "knowledge"  # no context, no change


def test_a_trigram_beats_a_bigram_when_it_has_evidence():
    ngrams = FakeCounts(
        {
            ("to",): {"know": 60, "knew": 10},
            ("like", "to"): {"knew": 40, "know": 2},
        }
    )
    completer = WordCompleter(VOCAB, ngrams=ngrams)
    assert completer.complete("I would like to kn").words[:2] == ("knew", "know")


def test_the_context_is_asked_for_the_typed_prefix_in_lowercase():
    ngrams = FakeCounts({})
    WordCompleter(VOCAB, ngrams=ngrams).complete("I Would Like To Kn")
    assert ((("like", "to")), "kn") in ngrams.queries
    assert (("to",), "kn") in ngrams.queries


def test_an_unseen_context_leaves_frequency_ranking_alone():
    completer = WordCompleter(VOCAB, ngrams=FakeCounts({("zzz",): {"knew": 99}}))
    assert completer.complete("kn").words == ("knowledge", "know", "knew", "known")


def test_thin_evidence_only_nudges_the_ranking():
    # One sighting in a context seen once shouldn't beat a word that's far more common.
    ngrams = FakeCounts({("to",): {"known": 1}})
    assert WordCompleter(VOCAB, ngrams=ngrams).complete("I want to kn").words[0] == "knowledge"


def test_words_only_the_ngrams_know_are_not_suggested():
    ngrams = FakeCounts({("to",): {"knox": 500, "know": 3}})
    assert "knox" not in WordCompleter(VOCAB, ngrams=ngrams).complete("I want to kn").words


def test_the_exact_typed_word_is_still_excluded_with_ngrams():
    ngrams = FakeCounts({("to",): {"know": 50}})
    assert "know" not in WordCompleter(VOCAB, ngrams=ngrams).complete("I want to know").words


def test_case_follows_the_typed_prefix_with_ngrams():
    ngrams = FakeCounts({("to",): {"know": 50}})
    assert WordCompleter(VOCAB, ngrams=ngrams).complete("I want to Kn").words[0] == "Know"


# --- personal ---------------------------------------------------------------------------------


def test_a_word_you_use_a_lot_rises_above_more_common_ones():
    personal = FakeCounts({(): {"known": 30}}, totals={(): 400})
    assert WordCompleter(VOCAB, personal=personal).complete("kn").words[0] == "known"


def test_a_single_use_is_not_enough_to_reorder():
    personal = FakeCounts({(): {"known": 1}}, totals={(): 400})
    assert WordCompleter(VOCAB, personal=personal).complete("kn").words[0] == "knowledge"


def test_a_personal_bigram_favours_the_word_you_usually_put_there():
    personal = FakeCounts({("to",): {"knew": 8}}, totals={("to",): 20})
    assert WordCompleter(VOCAB, personal=personal).complete("I want to kn").words[0] == "knew"


def test_a_word_outside_the_dictionary_is_added_after_enough_uses():
    personal = FakeCounts({(): {"knoxville": 4}}, totals={(): 300})
    completer = WordCompleter(VOCAB, personal=personal, promote_after=3)
    assert "knoxville" in completer.complete("kn").words


def test_a_word_outside_the_dictionary_needs_enough_uses():
    personal = FakeCounts({(): {"knoxville": 2}}, totals={(): 300})
    completer = WordCompleter(VOCAB, personal=personal, promote_after=3)
    assert "knoxville" not in completer.complete("kn").words


def test_a_promoted_word_takes_the_typed_prefix_casing():
    personal = FakeCounts({(): {"linqi": 12}}, totals={(): 300})
    completer = WordCompleter(VOCAB, personal=personal)
    assert completer.complete("Li").words[0] == "Linqi"
    assert completer.complete("li").words[0] == "linqi"


def test_ngrams_and_personal_habits_work_together():
    ngrams = FakeCounts({("to",): {"know": 50, "knew": 5}})
    personal = FakeCounts({(): {"knew": 25}}, totals={(): 300})
    words = WordCompleter(VOCAB, ngrams=ngrams, personal=personal).complete("I want to kn").words
    assert set(words[:2]) == {"know", "knew"}


# --- next words: suggestions after a space, before any letter is typed ------------------------------

NEXT_VOCAB = VOCAB + [("see", 7.0), ("be", 9.0), ("write", 6.0), ("i", 9.5), ("go", 8.0)]
TO = {("to",): {"know": 30, "see": 20, "be": 10, "go": 5}}  # after "to": know 46%, see 31%, be 15%, go 8%


def test_after_a_space_the_words_that_usually_follow_are_offered():
    completer = WordCompleter(NEXT_VOCAB, ngrams=FakeCounts(TO))
    result = completer.next_words("I'd like to ", limit=5, threshold=0.10)
    assert result.words == ("know", "see", "be")  # "go" is under the threshold
    assert result.replace == 0


def test_the_limit_caps_the_list():
    completer = WordCompleter(NEXT_VOCAB, ngrams=FakeCounts(TO))
    assert completer.next_words("I'd like to ", limit=2, threshold=0.0).words == ("know", "see")


def test_nothing_is_offered_when_no_word_is_likely_enough():
    flat = {("to",): {w: 10 for w in ("know", "see", "be", "go", "write", "the", "they", "then", "knew", "known")}}
    completer = WordCompleter(NEXT_VOCAB, ngrams=FakeCounts(flat))
    assert completer.next_words("I'd like to ", limit=5, threshold=0.25).words == ()


def test_a_zero_threshold_switches_the_filter_off():
    flat = {("to",): {w: 10 for w in ("know", "see", "be", "go", "write", "the", "they", "then", "knew", "known")}}
    completer = WordCompleter(NEXT_VOCAB, ngrams=FakeCounts(flat))
    assert len(completer.next_words("I'd like to ", limit=5, threshold=0.0).words) == 5


def test_a_trigram_beats_the_bigram_for_the_next_word():
    ngrams = FakeCounts({**TO, ("like", "to"): {"go": 90, "know": 5}})
    completer = WordCompleter(NEXT_VOCAB, ngrams=ngrams)
    assert completer.next_words("I'd like to ", limit=5, threshold=0.10).words[0] == "go"


def test_too_thin_a_context_is_not_trusted():
    completer = WordCompleter(NEXT_VOCAB, ngrams=FakeCounts({("to",): {"know": 2}}))
    assert completer.next_words("I'd like to ", limit=5, threshold=0.0).words == ()


@pytest.mark.parametrize(
    "before",
    [
        "I'd like to",  # still typing the word
        "I'd like to ss",
        "Hello. ",  # new sentence: no context
        "",
        "to\n",
        "to, ",
        "to. ",
        "to 5 ",  # a number breaks the chain
    ],
)
def test_next_words_only_follow_a_word_and_a_space(before):
    completer = WordCompleter(NEXT_VOCAB, ngrams=FakeCounts({**TO, ("hello",): {"to": 50}}))
    assert completer.next_words(before, threshold=0.0).words == ()


def test_a_comma_earlier_in_the_sentence_does_not_stop_next_words():
    completer = WordCompleter(NEXT_VOCAB, ngrams=FakeCounts(TO))
    assert completer.next_words("Hello, I'd like to ", threshold=0.0).words


def test_your_own_pairs_raise_a_word_that_follows_the_last_word():
    personal = FakeCounts({("to",): {"write": 6}, (): {"write": 6}})
    completer = WordCompleter(NEXT_VOCAB, ngrams=FakeCounts(TO), personal=personal)
    assert completer.next_words("I'd like to ", limit=5, threshold=0.10).words[0] == "write"


def test_words_outside_the_vocabulary_need_promotion_first():
    personal = FakeCounts({("to",): {"linqi": 2}, (): {"linqi": 2}})
    completer = WordCompleter(NEXT_VOCAB, personal=personal, promote_after=3)
    assert completer.next_words("talk to ", threshold=0.0).words == ()
    personal = FakeCounts({("to",): {"linqi": 4}, (): {"linqi": 4}})
    completer = WordCompleter(NEXT_VOCAB, personal=personal, promote_after=3)
    assert completer.next_words("talk to ", threshold=0.0).words == ("linqi",)


def test_the_pronoun_i_is_capitalised():
    completer = WordCompleter(NEXT_VOCAB, ngrams=FakeCounts({("and",): {"i": 40, "see": 10}}))
    assert completer.next_words("and ", threshold=0.10).words[0] == "I"


def test_no_sources_means_no_next_words():
    assert WordCompleter(NEXT_VOCAB).next_words("I'd like to ", threshold=0.0).words == ()
