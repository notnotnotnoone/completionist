"""What the phrase model is shown: a window of the text before the caret, the text after, and a header.

The start of the window is *anchored*: it moves only when a paragraph (or sentence) boundary scrolls
out of range, not on every keystroke. Consecutive requests then begin with identical text, which
providers with prefix caching (DeepSeek) serve at a fraction of the price.
"""

_BREAKS = ("\n\n", "\n", ". ", "? ", "! ")


def anchored_window(before: str, cap: int) -> str:
    """The last `cap` characters of `before` or fewer, starting just after a nearby boundary."""
    if len(before) <= cap:
        return before
    low = len(before) - cap
    high = low + max(cap // 4, 1)
    for marker in _BREAKS:
        i = before.find(marker, low, high + len(marker))
        if i != -1 and i + len(marker) <= len(before):
            return before[i + len(marker) :]
    i = before.find(" ", low, high)
    if i != -1:
        return before[i + 1 :]
    return before[low:]


def trim_suffix(after: str, cap: int) -> str:
    return after[:cap]


def _one_line(text: str) -> str:
    return " ".join(text.split())


def build_prompt(app: str, title: str, window: str) -> str:
    """A short, stable header (so the cached prefix is shared) followed by the text so far."""
    app_name = _one_line(app).removesuffix(".exe").removesuffix(".EXE")
    where = " - ".join(part for part in (_one_line(title), app_name) if part)
    header = f"[Text typed in: {where}]" if where else "[Text typed in an app]"
    return f"{header}\n\n{window}"
