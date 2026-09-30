"""Word completion: extract the word being typed and rank dictionary words that start with it.

Ranking starts from how common each word is in English, then adjusts for the words just before it
(n-gram counts from a corpus) and for the writer's own habits (the personal store).
"""

import re
from bisect import bisect_left
from collections.abc import Iterable
from dataclasses import dataclass
from functools import lru_cache
from heapq import nlargest

from completionist_engine.counts import NO_COUNTS, Counts, CountSource

# A word is a letter followed by letters or apostrophes. Only the tail of the text matters.
_WORD_AT_END = re.compile(r"[^\W\d_](?:[^\W\d_]|')*$")
_TAIL_CHARS = 100
_AFTER_LAST_KEY = "\U0010ffff"

# Context words come from the current sentence; a number breaks the chain.
_SENTENCE_END = re.compile(r"[.!?\n]")
_TOKENS = re.compile(r"\d+|[^\W\d_]+(?:'[^\W\d_]+)*")
_CONTEXT_CHARS = 300

# Scoring. Probabilities are approximate: base frequency follows Zipf's law over the vocabulary rank.
_ZIPF_OFFSET = 10
_ZIPF_NORMALISER = 13.0  # about ln(vocabulary size) + 0.58
_UNKNOWN_FLOOR = 1e-7  # base probability of a word outside the dictionary (only the personal store adds these)
_MIN_CONTEXT_TOTAL = 5  # contexts seen fewer times than this are too thin to trust
_CONTEXT_SMOOTHING = 5.0  # a context needs this many sightings before it counts for half
_PERSONAL_MIN_COUNT = 2  # one use of a word is noise
_PERSONAL_SMOOTHING = 500.0
_PERSONAL_BIGRAM_SMOOTHING = 3.0
_BASE_CANDIDATES = 24
_NGRAM_ORDERS = (1, 2)  # context lengths asked of the n-gram source: bigram, then trigram


@dataclass(frozen=True)
class Completion:
    replace: int
    """How many characters before the caret the chosen word replaces."""
    words: tuple[str, ...]


def current_word(before: str) -> str:
    match = _WORD_AT_END.search(before[-_TAIL_CHARS:])
    return match.group() if match else ""


def sentence_words(text: str) -> list[str]:
    """The lowercase words of the sentence `text` ends in. A number starts the chain over."""
    segment = _SENTENCE_END.split(text[-_CONTEXT_CHARS:])[-1]
    words: list[str] = []
    for token in _TOKENS.findall(segment.lower()):
        if token[0].isdigit():
            words.clear()
        else:
            words.append(token)
    return words


def previous_words(before: str, limit: int = 2) -> tuple[str, ...]:
    """Up to `limit` complete words before the word being typed, from the same sentence."""
    typed = current_word(before)
    return tuple(sentence_words(before[: len(before) - len(typed)])[-limit:])


class WordCompleter:
    def __init__(
        self,
        vocabulary: Iterable[tuple[str, float]],
        ngrams: CountSource | None = None,
        personal: CountSource | None = None,
        promote_after: int = 3,
    ) -> None:
        """`vocabulary` is (word, weight) pairs; a higher weight ranks the word higher.

        `ngrams` re-ranks by the words before the one being typed; `personal` boosts the writer's own
        words and adds ones outside the dictionary once they've been used `promote_after` times.
        """
        weights: dict[str, float] = {}
        for word, weight in vocabulary:
            key = word.lower()
            weights[key] = max(weight, weights.get(key, weight))
        self._words = sorted(weights)
        self._weights = [weights[w] for w in self._words]
        self._index = {word: i for i, word in enumerate(self._words)}
        by_weight = sorted(range(len(self._words)), key=lambda i: -self._weights[i])
        self._base = [0.0] * len(self._words)
        for rank, i in enumerate(by_weight):
            self._base[i] = 1.0 / ((rank + _ZIPF_OFFSET) * _ZIPF_NORMALISER)
        self._ngrams = ngrams
        self._personal = personal
        self._promote_after = promote_after
        self._ranked = lru_cache(maxsize=4096)(self._rank)

    def complete(self, before: str, limit: int = 5) -> Completion:
        prefix = current_word(before)
        if not prefix:
            return Completion(replace=0, words=())
        key = prefix.lower()
        if self._ngrams is None and self._personal is None:
            words = self._ranked(key, limit)  # frequency alone
        else:
            words = self._rerank(key, previous_words(before, 2), limit)
        cased = (_match_case(word, prefix) for word in words)
        return Completion(replace=len(prefix), words=tuple(dict.fromkeys(cased)))

    def next_words(self, before: str, limit: int = 5, threshold: float = 0.05) -> Completion:
        """Likely words to follow the last one, offered after a space and before any letter is typed.

        Only words that score at least `threshold` (roughly, their chance of coming next) are
        offered, so most of the time nothing is; a threshold of 0 offers the best few regardless.
        """
        nothing = Completion(replace=0, words=())
        if len(before) < 2 or before[-1] != " " or not (before[-2].isalpha() or before[-2] == "'"):
            return nothing
        context = previous_words(before, 2)
        if not context:
            return nothing
        scores = self._next_scores(context)
        likely = [(p, w) for w, p in scores.items() if p > 0 and p >= threshold]
        likely.sort(key=lambda pair: (-pair[0], pair[1]))
        return Completion(replace=0, words=tuple(_display(w) for _, w in likely[:limit]))

    def _next_scores(self, context: tuple[str, ...]) -> dict[str, float]:
        scores: dict[str, float] = {}
        if self._ngrams is not None:
            for order in _NGRAM_ORDERS:  # the bigram first, then the trigram refines it
                if len(context) < order:
                    continue
                counts = self._ngrams.counts(context[-order:], "")
                if counts.total < _MIN_CONTEXT_TOTAL:
                    continue
                weight = counts.total / (counts.total + _CONTEXT_SMOOTHING)
                for word in scores.keys() | counts.words.keys():
                    scores[word] = (1 - weight) * scores.get(word, 0.0) + weight * counts.words.get(word, 0) / counts.total
            scores = {w: p for w, p in scores.items() if w in self._index}
        if self._personal is not None:
            known = self._personal.counts((), "")
            for order in _NGRAM_ORDERS:
                if len(context) < order:
                    continue
                counts = self._personal.counts(context[-order:], "")
                for word, n in counts.words.items():
                    if n >= _PERSONAL_MIN_COUNT and (word in self._index or known.words.get(word, 0) >= self._promote_after):
                        scores[word] = scores.get(word, 0.0) + n / (counts.total + _PERSONAL_BIGRAM_SMOOTHING)
        return scores

    def _rerank(self, key: str, context: tuple[str, ...], limit: int) -> tuple[str, ...]:
        # Evidence from the n-gram tables: a bigram (last word) and a trigram (last two words).
        ngram_counts = [NO_COUNTS, NO_COUNTS]
        if self._ngrams is not None:
            for order in _NGRAM_ORDERS:
                if len(context) >= order:
                    counts = self._ngrams.counts(context[-order:], key)
                    if counts.total >= _MIN_CONTEXT_TOTAL:
                        ngram_counts[order - 1] = counts

        personal_uni = personal_bi = NO_COUNTS
        if self._personal is not None:
            personal_uni = self._personal.counts((), key)
            if context:
                personal_bi = self._personal.counts(context[-1:], key)

        candidates = dict.fromkeys(self._ranked(key, max(limit * 4, _BASE_CANDIDATES)))
        for counts in ngram_counts:
            candidates.update(dict.fromkeys(w for w in counts.words if w in self._index))
        for counts in (personal_uni, personal_bi):
            candidates.update(dict.fromkeys(w for w in counts.words if self._usable_personal(w, counts, personal_uni)))
        candidates.pop(key, None)  # never suggest the word already typed

        scored = [(self._score(w, ngram_counts, personal_uni, personal_bi), w) for w in candidates]
        scored.sort(key=lambda pair: (-pair[0], pair[1]))
        return tuple(word for _, word in scored[:limit])

    def _usable_personal(self, word: str, counts: Counts, unigrams: Counts) -> bool:
        if word in self._index:
            return counts.words[word] >= _PERSONAL_MIN_COUNT
        return unigrams.words.get(word, 0) >= self._promote_after  # promoted into the vocabulary

    def _score(self, word: str, ngram_counts: list[Counts], personal_uni: Counts, personal_bi: Counts) -> float:
        i = self._index.get(word)
        p = self._base[i] if i is not None else _UNKNOWN_FLOOR
        for counts in ngram_counts:  # bigram first, then trigram refines it
            if counts.total:
                weight = counts.total / (counts.total + _CONTEXT_SMOOTHING)
                p = (1 - weight) * p + weight * counts.words.get(word, 0) / counts.total
        used = personal_uni.words.get(word, 0)
        if used >= _PERSONAL_MIN_COUNT:
            p += used / (personal_uni.total + _PERSONAL_SMOOTHING)
        after_previous = personal_bi.words.get(word, 0)
        if after_previous >= _PERSONAL_MIN_COUNT:
            p += after_previous / (personal_bi.total + _PERSONAL_BIGRAM_SMOOTHING)
        return p

    def _rank(self, key: str, limit: int) -> tuple[str, ...]:
        lo = bisect_left(self._words, key)
        hi = bisect_left(self._words, key + _AFTER_LAST_KEY, lo)
        if lo < hi and self._words[lo] == key:
            lo += 1  # never suggest the word already typed
        # nlargest keeps equal-weight words in their (alphabetical) index order.
        best = nlargest(limit, range(lo, hi), key=self._weights.__getitem__)
        return tuple(self._words[i] for i in best)


def _display(word: str) -> str:
    return "I" + word[1:] if word == "i" or word.startswith("i'") else word


def _match_case(word: str, prefix: str) -> str:
    if word == "i" or word.startswith("i'"):
        word = "I" + word[1:]
    if len(prefix) > 1 and prefix.isupper():
        return word.upper()
    if prefix[0].isupper():
        return word[0].upper() + word[1:]
    return word
