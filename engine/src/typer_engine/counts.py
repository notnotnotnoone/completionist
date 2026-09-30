"""What the completer needs from a source of word counts (n-gram tables, the personal store)."""

from collections.abc import Mapping
from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True)
class Counts:
    words: Mapping[str, int]
    """How often each word starting with the queried prefix followed the context."""
    total: int
    """How often the context was seen at all, whatever came next."""


NO_COUNTS = Counts({}, 0)


class CountSource(Protocol):
    def counts(self, context: tuple[str, ...], prefix: str) -> Counts:
        """Counts of words with `prefix` after `context` (lowercase words, oldest first).

        An empty context means plain word counts.
        """
        ...
