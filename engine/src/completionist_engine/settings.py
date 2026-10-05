"""Reads and edits the settings the viewer's form offers, writing them back into `config.toml`.

The file is edited line by line so the user's comments and ordering survive. A change is only saved
once the engine's own config loader accepts the result, so the file can't be left broken. The phrase
`api_key` can be written but never read back: `read_settings` only says whether one is set.
"""

import json
from dataclasses import asdict
import math
import os
import re
import tomllib
from pathlib import Path
from typing import Any

from completionist_engine.config import ConfigError, load_config, _PHRASE_CHECKS, _PROVIDER_CHECKS, _finite_number
from completionist_engine.context import INSTRUCTIONS, PROMPT_TEST_SCENARIOS

EDITABLE: dict[str, frozenset[str]] = {
    "words": frozenset({"limit", "next", "chunks", "next_threshold", "typo_correction", "tense_aware"}),
    "learning": frozenset({"enabled", "promote_after"}),
    "phrase": frozenset({
        "enabled", "api_key", "models", "provider_order", "max_tokens", "temperature", "timeout", "debounce",
        "context_before", "context_after", "fim", "instructions", "screen_context",
    }),  # fmt: skip
    "apps": frozenset({"block", "allow", "profiles"}),
    "hotkeys": frozenset({"pause", "partial_accept", "dismiss"}),
    "popup": frozenset({"font_size", "width_scale"}),
    "privacy": frozenset({"private_mode", "pause_minutes"}),
}

EDITABLE["phrase"] = EDITABLE["phrase"] | frozenset(_PHRASE_CHECKS) | frozenset(_PROVIDER_CHECKS)

_HEADER = re.compile(r"^\s*\[([^\[\]]+)\]\s*(#.*)?$")


def read_settings(path: Path) -> dict[str, dict[str, Any]]:
    """The editable settings as they stand in the file (defaults for anything left out). Never the api key."""
    config = load_config(path)
    result = {
        "words": {"limit": config.word_limit, "next": config.next_words, "chunks": config.chunks, "next_threshold": config.next_threshold},
        "learning": {"enabled": config.learning, "promote_after": config.promote_after},
        "phrase": {
            "enabled": config.phrase.enabled,
            "models": list(config.phrase.provider.models),
            "provider_order": list(config.phrase.provider.provider_order),
            "max_tokens": config.phrase.provider.max_tokens,
            "temperature": config.phrase.provider.temperature,
            "timeout": config.phrase.provider.timeout,
            "debounce": round(config.phrase.debounce, 3),  # seconds here; the file keeps milliseconds
            "context_before": config.phrase.context_before,
            "context_after": config.phrase.context_after,
            "fim": config.phrase.provider.fim,
            "screen_context": config.phrase.screen_context,
            "instructions": config.phrase.instructions or INSTRUCTIONS,
            "instructions_default": INSTRUCTIONS,
            "instructions_custom": bool(config.phrase.instructions),
            "prompt_test_scenarios": [dict(scenario) for scenario in PROMPT_TEST_SCENARIOS],
            "api_key_set": bool(config.phrase.provider.api_key),
        },
        "apps": {"block": sorted(config.block), "allow": sorted(config.allow)},
        "hotkeys": {"pause": config.pause_hotkey},
    }  # fmt: skip
    result["words"]["typo_correction"] = config.typo_correction
    result["words"]["tense_aware"] = config.tense_aware
    result["popup"] = asdict(config.popup)
    result["privacy"] = {"private_mode": config.private_mode, "pause_minutes": config.pause_minutes}
    result["hotkeys"].update(partial_accept=config.partial_accept_hotkey, dismiss=config.dismiss_hotkey)
    result["apps"]["profiles"] = [{key: value for key, value in asdict(profile).items() if value is not None}
                                   for profile in config.app_profiles]
    for key in _PHRASE_CHECKS:
        value = getattr(config.phrase, key)
        result["phrase"][key] = sorted(value) if isinstance(value, frozenset) else list(value) if isinstance(value, tuple) else value
    for key in _PROVIDER_CHECKS:
        value = getattr(config.phrase.provider, key)
        result["phrase"][key] = list(value) if isinstance(value, tuple) else value
    return result


def apply_settings(path: Path, changes: dict[str, dict[str, Any]]) -> None:
    """Write `changes` ({section: {key: value}}) into the file, or raise ConfigError and leave it untouched."""
    if not isinstance(changes, dict):
        raise ConfigError("settings must be an object")
    text = path.read_bytes().decode("utf-8") if path.exists() else ""  # bytes, to keep the line endings
    wanted: list[tuple[str, str, Any]] = []
    for section, values in changes.items():
        if section not in EDITABLE or not isinstance(values, dict):
            raise ConfigError(f"unknown section {section!r}")
        for key, value in values.items():
            if key not in EDITABLE[section]:
                raise ConfigError(f"unknown setting {section}.{key}")
            wanted.append((section, *_as_stored(section, key, value)))
    for section, key, value in wanted:
        text = _set(text, section, key, _toml(value, f"{section}.{key}"))

    try:
        parsed = tomllib.loads(text)  # a line the edit mangled shows up here
    except tomllib.TOMLDecodeError as err:
        raise ConfigError("the settings file can't be edited while it has a syntax error") from err
    for section, key, value in wanted:
        if parsed.get(section, {}).get(key) != value:
            raise ConfigError(f"could not write {section}.{key}")
    path.parent.mkdir(parents=True, exist_ok=True)
    candidate = path.with_name(path.name + ".tmp")
    candidate.write_text(text, encoding="utf-8", newline="")
    try:
        load_config(candidate)  # the same checks the engine applies when it reloads
    except ConfigError:
        candidate.unlink(missing_ok=True)
        raise
    try:
        os.replace(candidate, path)
    except OSError:
        candidate.unlink(missing_ok=True)
        raise


def _as_stored(section: str, key: str, value: Any) -> tuple[str, Any]:
    """The key and value as the file keeps them where the form differs.

    The wait is seconds on the page and milliseconds in the file. Instructions that are blank or the built-in text
    are stored as blank, so a later improvement to the built-in text still applies."""
    if (section, key) == ("phrase", "debounce"):
        if not _finite_number(value) or not 0 <= value <= 86400:
            raise ConfigError("phrase.debounce must be a number of seconds, 0 or more")
        return "debounce_ms", round(value * 1000)
    if (section, key) == ("phrase", "instructions") and isinstance(value, str):
        return key, "" if value.strip() in ("", INSTRUCTIONS.strip()) else value.strip()
    return key, value


def _toml(value: Any, name: str) -> str:
    """A value as TOML text: booleans, numbers, strings and lists of strings only."""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float) and math.isfinite(value):
        return repr(value)
    if isinstance(value, str):
        return json.dumps(value)  # a JSON string is also a valid TOML one
    if isinstance(value, list):
        if all(isinstance(v, str) for v in value):
            return "[" + ", ".join(json.dumps(v) for v in value) + "]"
        if name == "apps.profiles" and all(isinstance(v, dict) for v in value):
            rows = []
            for row in value:
                if not all(isinstance(key, str) and re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", key) for key in row):
                    raise ConfigError("apps.profiles has an invalid field name")
                rows.append("{ " + ", ".join(f"{key} = {_toml(item, name + '.' + key)}" for key, item in row.items()) + " }")
            return "[" + ", ".join(rows) + "]"
    raise ConfigError(f"{name} has a value of the wrong kind")


def _set(text: str, section: str, key: str, value: str) -> str:
    newline = "\r\n" if "\r\n" in text else "\n"
    lines = text.splitlines(keepends=True)
    start = next((i for i, line in enumerate(lines) if _header(line) == section), None)
    if start is None:
        if lines and not lines[-1].endswith(("\n", "\r")):
            lines[-1] += newline
        if lines:
            lines.append(newline)
        lines += [f"[{section}]{newline}", f"{key} = {value}{newline}"]
        return "".join(lines)

    end = next((i for i in range(start + 1, len(lines)) if _header(lines[i]) is not None), len(lines))
    found = re.compile(rf"^(\s*{re.escape(key)}\s*=\s*)(.*?)(\s+#.*)?\s*$")
    i = start + 1
    while i < end:
        match = found.match(lines[i].rstrip("\r\n"))
        if match:
            last = i
            depth = _open_brackets(lines[i])
            while depth > 0 and last + 1 < end:  # a list spread over several lines
                last += 1
                depth += _open_brackets(lines[last])
            comment = _trailing_comment(lines[i].rstrip("\r\n")) if last == i else ""
            lines[i : last + 1] = [f"{match.group(1)}{value}{comment}{newline}"]
            return "".join(lines)
        i += 1

    at = end
    while at > start + 1 and not lines[at - 1].strip():
        at -= 1
    if not lines[at - 1].endswith(("\n", "\r")):
        lines[at - 1] += newline
    lines.insert(at, f"{key} = {value}{newline}")
    return "".join(lines)


def _header(line: str) -> str | None:
    match = _HEADER.match(line.rstrip("\r\n"))
    return match.group(1).strip() if match else None


def _scan_value(text: str) -> tuple[int, int | None]:
    """Scan a TOML value, respecting escapes and literal/basic quoted strings."""
    depth, quote, escaped = 0, "", False
    for index, char in enumerate(text):
        if quote:
            if escaped:
                escaped = False
            elif char == "\\" and quote == '"':
                escaped = True
            elif char == quote:
                quote = ""
        elif char in "\"'":
            quote = char
        elif char == "#":
            return depth, index
        elif char in "[{":
            depth += 1
        elif char in "]}":
            depth -= 1
    return depth, None


def _open_brackets(text: str) -> int:
    return _scan_value(text)[0]


def _trailing_comment(text: str) -> str:
    _, index = _scan_value(text)
    if index is None:
        return ""
    start = index
    while start and text[start - 1].isspace():
        start -= 1
    return text[start:]
