"""Typo-tolerant lookup over the vocabulary: which words the typed fragment almost matches.

`FuzzyIndex` groups words by their first letter, then compares the fragment against the
start of each word in its group with an OSA edit distance (insert, delete, substitute or
swap of two neighbours count as one). The comparison allows up to `max_distance` edits:
1 for fragments under 5 letters, 2 for longer ones. Lookup only runs when exact prefix
matches leave rows unfilled, and only for fragments of 3 or more letters: shorter ones
match everything, and names and learned words are never corrected.
"""

import difflib

from rapidfuzz.distance import OSA

_MAX_SHORT = 4  # fragments shorter than this get distance 1
_SHORT_DISTANCE = 1
_LONG_DISTANCE = 2
_MIN_LENGTH = 3  # shorter fragments match everything, so they are never corrected

Hit = tuple[int, str]
"""A fuzzy hit: (edit distance, word). Lower distance is a closer guess."""


def guessed_positions(fragment: str, word: str) -> tuple[int, ...]:
    """Which letter positions of `word` the fragment did not earn.

    Lines the word up against the fragment's start (the fragment is a half-typed word,
    so only the first `len(fragment) + a little` letters count) and marks every word
    letter outside the longest matching runs: inserted letters the writer skipped, and
    substituted ones they got wrong. Deleted letters (typed but not in the word) mark
    nothing, since there is no word letter to point at.
    """
    prefix = word[: len(fragment) + max_distance(fragment)] if len(word) > len(fragment) + max_distance(fragment) else word
    marks: set[int] = set()
    for tag, _, _, low, high in difflib.SequenceMatcher(None, fragment, prefix, autojunk=False).get_opcodes():
        if tag == "equal":
            continue
        if tag == "insert" and high == len(prefix) and low >= len(fragment):
            continue  # the word's untyped tail: not a guess, just not typed yet
        marks.update(range(low, high))
    return tuple(sorted(marks))


def max_distance(fragment: str) -> int:
    """How many edits a fragment of this length may be off by."""
    return _SHORT_DISTANCE if len(fragment) < 5 else _LONG_DISTANCE


class FuzzyIndex:
    def __init__(self, words: set[str]) -> None:
        """Group `words` (already lowercase) by first letter for lookup."""
        buckets: dict[str, list[str]] = {}
        for word in words:
            if word:
                buckets.setdefault(word[0], []).append(word)
        self._buckets = {letter: sorted(bucket) for letter, bucket in buckets.items()}

    def candidates(self, fragment: str) -> list[Hit]:
        """Words whose start the fragment almost matches, closest guesses first.

        Only words starting with the same first letter are tried, and the fragment is
        compared against each word's first `len(fragment) + distance` letters, so a word
        whose start is close still matches when barely any of it is typed yet.
        """
        distance = max_distance(fragment)
        bucket = self._buckets.get(fragment[0]) if fragment else None
        if bucket is None or len(fragment) < _MIN_LENGTH:
            return []
        found: list[Hit] = []
        for word in bucket:
            if len(word) < len(fragment) - distance:
                continue
            prefix = word[: len(fragment) + distance] if len(word) > len(fragment) + distance else word
            if OSA.distance(fragment, prefix, score_cutoff=distance) <= distance:
                found.append((OSA.distance(fragment, prefix), word))
        found.sort()
        return found
