"""Word completion: extract the word being typed and rank dictionary words that start with it."""

import re
from bisect import bisect_left
from collections.abc import Iterable
from dataclasses import dataclass
from functools import lru_cache
from heapq import nlargest

# A word is a letter followed by letters or apostrophes. Only the tail of the text matters.
_WORD_AT_END = re.compile(r"[^\W\d_](?:[^\W\d_]|')*$")
_TAIL_CHARS = 100
_AFTER_LAST_KEY = "\U0010ffff"


@dataclass(frozen=True)
class Completion:
    replace: int
    """How many characters before the caret the chosen word replaces."""
    words: tuple[str, ...]


def current_word(before: str) -> str:
    match = _WORD_AT_END.search(before[-_TAIL_CHARS:])
    return match.group() if match else ""


class WordCompleter:
    def __init__(self, vocabulary: Iterable[tuple[str, float]]) -> None:
        """`vocabulary` is (word, weight) pairs; a higher weight ranks the word higher."""
        weights: dict[str, float] = {}
        for word, weight in vocabulary:
            key = word.lower()
            weights[key] = max(weight, weights.get(key, weight))
        self._words = sorted(weights)
        self._weights = [weights[w] for w in self._words]
        self._ranked = lru_cache(maxsize=4096)(self._rank)

    def complete(self, before: str, limit: int = 5) -> Completion:
        prefix = current_word(before)
        if not prefix:
            return Completion(replace=0, words=())
        cased = (_match_case(word, prefix) for word in self._ranked(prefix.lower(), limit))
        return Completion(replace=len(prefix), words=tuple(dict.fromkeys(cased)))

    def _rank(self, key: str, limit: int) -> tuple[str, ...]:
        lo = bisect_left(self._words, key)
        hi = bisect_left(self._words, key + _AFTER_LAST_KEY, lo)
        if lo < hi and self._words[lo] == key:
            lo += 1  # never suggest the word already typed
        # nlargest keeps equal-weight words in their (alphabetical) index order.
        best = nlargest(limit, range(lo, hi), key=self._weights.__getitem__)
        return tuple(self._words[i] for i in best)


def _match_case(word: str, prefix: str) -> str:
    if word == "i" or word.startswith("i'"):
        word = "I" + word[1:]
    if len(prefix) > 1 and prefix.isupper():
        return word.upper()
    if prefix[0].isupper():
        return word[0].upper() + word[1:]
    return word
