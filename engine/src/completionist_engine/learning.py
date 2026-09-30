"""Spots words the writer has just finished typing, from the stream of `before` texts."""

from completionist_engine.words import current_word, previous_words

_TAIL = 300


class TypingLearner:
    """One per connection. A word counts as typed only when the text grew by exactly one character
    that ends it (a space or punctuation), so pasted text, caret jumps, backspacing and text loaded
    into a field never count."""

    def __init__(self) -> None:
        self._last: str | None = None

    def reset(self) -> None:
        self._last = None

    def observe(self, before: str) -> tuple[str, tuple[str, ...]] | None:
        """The finished word (lowercase) and up to two words before it, if this text completed one."""
        last, self._last = self._last, before[-_TAIL:]
        if last is None or not before or before[:-1][-_TAIL:] != last:
            return None
        ender = before[-1]
        if ender.isalnum() or ender in "'_":
            return None
        typed = current_word(last)
        if not typed:
            return None
        return typed.lower(), previous_words(last, 2)
