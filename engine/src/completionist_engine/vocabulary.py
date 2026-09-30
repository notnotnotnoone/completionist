"""Base English vocabulary from the `wordfreq` package."""

import re
from collections.abc import Mapping

from wordfreq import top_n_list

from completionist_engine.ngrams import UnigramStat

# Lowercase letters, optionally with inner apostrophes ("don't", "i'm"); no digits or dots.
_PLAIN_WORD = re.compile(r"[a-z](?:[a-z']*[a-z])?")


def load_wordfreq_vocabulary(size: int = 150_000) -> list[tuple[str, float]]:
    """The `size` most frequent English words, weighted so more frequent words weigh more."""
    ranked = top_n_list("en", size, wordlist="large")
    return [(word, float(size - rank)) for rank, word in enumerate(ranked) if _PLAIN_WORD.fullmatch(word)]


def filter_vocabulary(
    vocabulary: list[tuple[str, float]],
    stats: Mapping[str, UnigramStat],
    *,
    core_rank: int = 20_000,
    min_lower: int = 3,
) -> list[tuple[str, float]]:
    """Drop misspellings and names from the tail of `vocabulary`, judging by how a corpus writes them.

    The `core_rank` most common words are trusted as they are (months, countries and slang live here).
    Beyond that, a word must appear written lowercase at least `min_lower` times in the corpus: typos
    are rare and surnames are always capitalised, so both fail. With no stats nothing is filtered.
    """
    if not stats:
        return vocabulary
    by_weight = sorted(range(len(vocabulary)), key=lambda i: -vocabulary[i][1])
    rank = {vocabulary[i][0]: r for r, i in enumerate(by_weight)}
    keep = []
    for word, weight in vocabulary:
        stat = stats.get(word)
        if rank[word] < core_rank or (stat is not None and stat.lower >= min_lower):
            keep.append((word, weight))
    return keep
