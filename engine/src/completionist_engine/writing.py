"""Writing preferences and local limits for cloud continuations."""

import re

from completionist_engine.config import PhraseConfig
from completionist_engine.context import INSTRUCTIONS


def phrase_instructions(config: PhraseConfig) -> str:
    preferences = []
    if config.writing_style != "match":
        preferences.append(f"Use a {config.writing_style} writing style.")
    if config.spelling != "auto":
        preferences.append(f"Use {config.spelling.title()} English spelling.")
    if not config.preserve_casing:
        preferences.append("Use standard sentence capitalization.")
    lengths = {"short": "Continue with no more than eight words.", "sentence": "Finish at most one sentence.",
               "long": "A longer continuation is allowed, but never invent unknown details."}
    if config.completion_length in lengths:
        preferences.append(lengths[config.completion_length])
    if config.multiline:
        preferences.append("Line breaks are allowed when they fit the text.")
    if config.avoid_phrases:
        preferences.append("Never use these phrases: " + "; ".join(config.avoid_phrases))
    if not preferences:
        return config.instructions or INSTRUCTIONS
    if config.preserve_casing:
        preferences.append("Preserve the writer's intentional casing, including lowercase messages.")
    return "Explicit writing preferences (take priority over general style matching below):\n" + "\n".join(preferences) + "\n\n" + (config.instructions or INSTRUCTIONS)


def fit_reply_start(text: str, partial: str, known: bool) -> str:
    """A whole dictionary word at the caret gets a space before the phrase; a half-typed one gets none,
    so the phrase finishes it. Punctuation right after a whole word ("is," "is.") keeps no space."""
    if not partial or not text:
        return text
    if not known:
        return text.lstrip(" ")
    if text[0].isspace() or text[0] in _NO_SPACE_BEFORE:
        return text
    return " " + text


_NO_SPACE_BEFORE = frozenset(",.;:!?)]}'’\"…")


def limit_reply(text: str, config: PhraseConfig) -> str:
    if any(phrase.casefold() in text.casefold() for phrase in config.avoid_phrases):
        return ""
    if not config.multiline:
        text = text.split("\n", 1)[0].split("\r", 1)[0]
    if config.completion_length == "sentence":
        boundary = re.search(r"[.!?](?:\s|$)", text)
        if boundary:
            text = text[:boundary.start() + 1]
    elif config.completion_length == "short":
        words = list(re.finditer(r"\S+", text))
        if len(words) > 8:
            text = text[:words[7].end()]
    return text
