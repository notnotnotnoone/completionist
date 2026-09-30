from typer_engine.ngrams import UnigramStat
from typer_engine.vocabulary import filter_vocabulary

# (word, weight): higher weight = more common. Rank = position by weight.
VOCAB = [("the", 9.0), ("tomorrow", 8.0), ("january", 7.0), ("recommend", 6.0), ("tomorow", 3.0), ("updike", 2.0), ("lol", 1.5), ("zzzz", 1.0)]


def stat(total, lower):
    return UnigramStat(total=total, lower=lower)


STATS = {
    "the": stat(1000, 990),
    "tomorrow": stat(200, 190),
    "january": stat(300, 2),  # names of months are always capitalised
    "recommend": stat(80, 78),
    "tomorow": stat(2, 2),  # a typo seen twice
    "updike": stat(40, 0),  # a surname
}


def kept(core_rank=4, min_lower=3):
    return {w for w, _ in filter_vocabulary(VOCAB, STATS, core_rank=core_rank, min_lower=min_lower)}


def test_common_words_written_in_lowercase_are_kept():
    assert {"the", "tomorrow", "recommend"} <= kept()


def test_a_rare_misspelling_is_dropped():
    assert "tomorow" not in kept()


def test_a_word_that_is_only_ever_capitalised_is_dropped_when_it_is_uncommon():
    assert "updike" not in kept()


def test_common_capitalised_words_like_months_survive_via_the_core_rank():
    assert "january" in kept()  # rank 2, inside the core


def test_words_the_corpus_never_saw_are_kept_only_inside_the_core():
    assert "lol" not in kept(core_rank=4)
    assert "lol" in kept(core_rank=7)


def test_a_word_with_enough_lowercase_use_is_kept_outside_the_core():
    stats = dict(STATS, tomorow=stat(50, 40))
    assert "tomorow" in {w for w, _ in filter_vocabulary(VOCAB, stats, core_rank=4, min_lower=3)}


def test_weights_and_order_are_unchanged():
    result = filter_vocabulary(VOCAB, STATS, core_rank=4, min_lower=3)
    assert result == [(w, x) for w, x in VOCAB if w in {w for w, _ in result}]


def test_no_stats_means_no_filtering():
    assert filter_vocabulary(VOCAB, {}, core_rank=4, min_lower=3) == VOCAB
