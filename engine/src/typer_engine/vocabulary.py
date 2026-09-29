"""Base English vocabulary from the `wordfreq` package."""

import re

from wordfreq import top_n_list

# Lowercase letters, optionally with inner apostrophes ("don't", "i'm"); no digits or dots.
_PLAIN_WORD = re.compile(r"[a-z](?:[a-z']*[a-z])?")


def load_wordfreq_vocabulary(size: int = 150_000) -> list[tuple[str, float]]:
    """The `size` most frequent English words, weighted so more frequent words weigh more."""
    ranked = top_n_list("en", size, wordlist="large")
    return [(word, float(size - rank)) for rank, word in enumerate(ranked) if _PLAIN_WORD.fullmatch(word)]
