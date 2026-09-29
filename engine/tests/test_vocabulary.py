import string
import time

import pytest

from typer_engine.vocabulary import load_wordfreq_vocabulary
from typer_engine.words import WordCompleter


@pytest.fixture(scope="module")
def vocabulary():
    return load_wordfreq_vocabulary(size=100_000)


@pytest.fixture(scope="module")
def completer(vocabulary):
    return WordCompleter(vocabulary)


def test_vocabulary_has_common_words_and_contractions(vocabulary):
    words = {word for word, _ in vocabulary}
    assert {"recommend", "because", "don't", "i'm"} <= words


def test_vocabulary_has_only_letters_and_inner_apostrophes(vocabulary):
    allowed = set(string.ascii_lowercase) | {"'"}
    for word, _ in vocabulary:
        assert set(word) <= allowed, word
        assert word[0] != "'" and word[-1] != "'", word


def test_more_frequent_words_get_higher_weights(vocabulary):
    weights = dict(vocabulary)
    assert weights["the"] > weights["house"] > weights["recommend"] > weights["kaleidoscope"]


def test_real_vocabulary_completes_a_common_word(completer):
    assert completer.complete("I'd like to recomm").words[0] == "recommend"


def test_uncached_lookups_are_fast_enough_for_every_keystroke(vocabulary):
    completer = WordCompleter(vocabulary)  # fresh, so nothing is cached
    prefixes = [a + b for a in string.ascii_lowercase for b in ["", *string.ascii_lowercase]]
    timings = []
    for prefix in prefixes:
        start = time.perf_counter()
        completer.complete(prefix)
        timings.append(time.perf_counter() - start)
    timings.sort()
    p95 = timings[int(len(timings) * 0.95)]
    assert p95 < 0.005, f"p95 lookup took {p95 * 1000:.2f} ms"
