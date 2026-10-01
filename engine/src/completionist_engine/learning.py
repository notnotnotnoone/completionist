"""Spots words the writer has just finished typing, from the stream of `before` texts."""

from completionist_engine.words import current_word, previous_words

_TAIL = 300


class TypingLearner:
    """One per connection. A word counts as typed only when the text grew by exactly one character
    that ends it (a space or punctuation), so pasted text, caret jumps, backspacing and text loaded
    into a field never count. A word the writer backspaced over while typing it is skipped too, so
    typos, and the fixes made to them, are never learned."""

    def __init__(self) -> None:
        self._last: str | None = None
        self._last_len = 0  # full length of the previous text, which `_last` (a tail) can't tell
        self._corrected = False

    def reset(self) -> None:
        self._last = None
        self._corrected = False

    def observe(self, before: str) -> tuple[str, tuple[str, ...]] | None:
        """The finished word (lowercase) and up to two words before it, if this text completed one."""
        last, self._last = self._last, before[-_TAIL:]
        last_len, self._last_len = self._last_len, len(before)
        removed = last_len - len(before)
        if last is not None and 0 < removed < len(last) and before.endswith(last[: len(last) - removed]):
            self._corrected = True
            return None
        if last is None or not before or before[:-1][-_TAIL:] != last:
            self._corrected = False
            return None
        ender = before[-1]
        if ender.isalnum() or ender in "'_":
            return None
        corrected, self._corrected = self._corrected, False
        typed = current_word(last)
        if not typed or corrected:
            return None
        return typed.lower(), previous_words(last, 2)
