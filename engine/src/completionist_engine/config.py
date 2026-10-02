"""User configuration, read from a TOML file with defaults for anything left out.

    [apps]
    block = ["code.exe", ...]   # no suggestions at all (replaces the default list)
    allow = ["obsidian.exe"]    # automatic phrase suggestions (replaces the default list)

    [words]
    limit = 5                   # words shown in the popup
    next = true                 # offer likely next words after a space
    chunks = true               # offer two- or three-word chunks while typing
    next_threshold = 0.05       # how likely a next word must be (0 to 1); 0 offers the best few whatever the odds

    [learning]
    enabled = true              # learn your words and habits (counts only, never text)
    promote_after = 3           # uses before a word outside the dictionary is suggested

    [data]
    dir = "C:/somewhere"        # where personal counts and n-gram tables live

    [phrase]                    # phrase continuations from a cloud completion model
    enabled = true
    base_url = "https://openrouter.ai/api/v1"    # OpenRouter API root; phrase requests use /chat/completions
    models = ["meta-llama/llama-3.3-70b-instruct"]    # tried in order; the next only if the one before fails
    provider_order = ["Groq"]                    # OpenRouter providers to prefer, in order (others stay as fallbacks)
    api_key = "sk-or-..."                        # your OpenRouter key; empty turns phrases off
    fim = false                 # send the text after the caret too (fill-in-the-middle); OpenRouter has none
    max_tokens = 40
    temperature = 0.2
    timeout = 4.0               # seconds
    debounce_ms = 350           # pause before an automatic request
    context_before = 6000       # characters of text before the caret sent to the model
    context_after = 2000
    instructions = ""           # replaces the built-in instructions the model is given ("" keeps them)
    screen_context = true       # read the window you type in (Windows OCR, on this PC) and send that text with your own
    reasoning_effort = "none"   # disable model reasoning where supported

    [hotkeys]
    pause = "ctrl+alt+p"        # pause / resume Completionist from anywhere ("" turns the hotkey off)
"""

import logging
import math
import re
import os
import tomllib
from dataclasses import dataclass, field, replace
from pathlib import Path
from typing import Any

from completionist_engine.hotkeys import parse_hotkey
from completionist_engine.phrase_provider import ProviderSettings

logger = logging.getLogger("completionist_engine")

DEFAULT_BLOCK = frozenset(
    {
        # Code editors and IDEs have their own completion.
        "code.exe",
        "code - insiders.exe",
        "cursor.exe",
        "devenv.exe",
        "idea64.exe",
        "pycharm64.exe",
        "webstorm64.exe",
        "rider64.exe",
        "clion64.exe",
        "goland64.exe",
        # Terminals: English suggestions on shell commands are noise.
        "windowsterminal.exe",
        "openconsole.exe",
        "conhost.exe",
        "cmd.exe",
        "powershell.exe",
        "pwsh.exe",
    }
)
DEFAULT_ALLOW = frozenset({"obsidian.exe", "notepad.exe", "winword.exe", "outlook.exe", "olk.exe"})

_PHRASE_KEYS = {
    "enabled", "base_url", "models", "provider_order", "api_key", "fim", "max_tokens", "temperature", "timeout", "debounce_ms",
    "context_before", "context_after", "instructions", "screen_context",
}  # fmt: skip
_SCHEMA = {
    "apps": {"block", "allow", "profiles"},
    "words": {"limit", "next", "next_threshold", "chunks", "typo_correction"},
    "learning": {"enabled", "promote_after"},
    "data": {"dir"},
    "phrase": _PHRASE_KEYS,
    "hotkeys": {"pause", "partial_accept", "dismiss"},
    "popup": {"font_size", "width_scale"},
    "privacy": {"private_mode", "pause_minutes"},
}


class ConfigError(Exception):
    """The config file can't be read or holds invalid settings."""


def default_data_dir() -> Path:
    localappdata = os.environ.get("LOCALAPPDATA")
    base = Path(localappdata) if localappdata else Path.home() / "AppData" / "Local"
    return base / "Completionist"


@dataclass(frozen=True)
class PhraseConfig:
    enabled: bool = True
    provider: ProviderSettings = field(default_factory=ProviderSettings)
    debounce: float = 0.35  # seconds
    context_before: int = 6000  # below what the DLL sends (8000), so the window can anchor
    context_after: int = 2000
    instructions: str = ""  # blank: the built-in instructions in context.py
    screen_context: bool = True  # send text read from the window in front along with the typed text
    mode: str = "apps"
    min_chars: int = 0
    trigger: str = "pause"
    dismiss_cooldown: float = 0
    failure_limit: int = 3
    failure_pause: float = 30
    completion_length: str = "default"
    writing_style: str = "match"
    spelling: str = "auto"
    preserve_casing: bool = True
    multiline: bool = False
    avoid_phrases: tuple[str, ...] = ()
    context_source: str = "ocr"
    context_apps: frozenset[str] = frozenset()
    preview_context: bool = False
    log_mode: str = "full"
    log_retention_minutes: float = 0


@dataclass(frozen=True)
class PopupConfig:
    font_size: int = 9
    width_scale: float = 1


@dataclass(frozen=True)
class AppProfile:
    app: str
    phrase_mode: str | None = None
    completion_length: str | None = None
    writing_style: str | None = None
    spelling: str | None = None
    context_source: str | None = None
    learning: bool | None = None
    word_limit: int | None = None


@dataclass(frozen=True)
class Config:
    block: frozenset[str] = DEFAULT_BLOCK
    allow: frozenset[str] = DEFAULT_ALLOW
    word_limit: int = 5
    next_words: bool = True
    chunks: bool = True
    next_threshold: float = 0.05
    learning: bool = True
    promote_after: int = 3
    data_dir: Path = field(default_factory=default_data_dir)
    phrase: PhraseConfig = field(default_factory=PhraseConfig)
    pause_hotkey: str = "ctrl+alt+p"
    typo_correction: bool = True
    popup: PopupConfig = field(default_factory=PopupConfig)
    partial_accept_hotkey: str = "ctrl+right"
    dismiss_hotkey: str = "escape"
    private_mode: bool = False
    pause_minutes: float = 0
    app_profiles: tuple[AppProfile, ...] = ()


def migrate_legacy_dirs(local: Path | None = None, roaming: Path | None = None) -> None:
    """Move the data and config folders from the app's old name (Typer) to Completionist, once.

    Skips any folder whose new name already exists, so it never overwrites anything."""
    if local is None:
        localappdata = os.environ.get("LOCALAPPDATA")
        local = Path(localappdata) if localappdata else Path.home() / "AppData" / "Local"
    if roaming is None:
        appdata = os.environ.get("APPDATA")
        roaming = Path(appdata) if appdata else Path.home() / "AppData" / "Roaming"
    for base in (local, roaming):
        old, new = base / "Typer", base / "Completionist"
        if old.is_dir() and not new.exists():
            try:
                old.rename(new)
            except OSError as err:
                logger.warning("can't move %s to %s (%s)", old, new, err)


def default_config_path() -> Path:
    appdata = os.environ.get("APPDATA")
    base = Path(appdata) if appdata else Path.home() / "AppData" / "Roaming"
    return base / "Completionist" / "config.toml"


def load_config(path: Path) -> Config:
    if not path.exists():
        return Config()
    try:
        data = tomllib.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeDecodeError, tomllib.TOMLDecodeError) as err:
        raise ConfigError(f"can't read {path}: {err}") from err
    _check_known_keys(data)

    apps = data.get("apps", {})
    words = data.get("words", {})
    learning = data.get("learning", {})
    data_section = data.get("data", {})
    config = Config()
    return Config(
        block=_app_names(apps["block"], "apps.block") if "block" in apps else config.block,
        allow=_app_names(apps["allow"], "apps.allow") if "allow" in apps else config.allow,
        word_limit=_positive_int(words["limit"], "words.limit") if "limit" in words else config.word_limit,
        next_words=_boolean(words["next"], "words.next") if "next" in words else config.next_words,
        chunks=_boolean(words["chunks"], "words.chunks") if "chunks" in words else config.chunks,
        next_threshold=(
            _probability(words["next_threshold"], "words.next_threshold") if "next_threshold" in words else config.next_threshold
        ),
        learning=_boolean(learning["enabled"], "learning.enabled") if "enabled" in learning else config.learning,
        promote_after=(
            _positive_int(learning["promote_after"], "learning.promote_after")
            if "promote_after" in learning
            else config.promote_after
        ),
        data_dir=_directory(data_section["dir"], "data.dir") if "dir" in data_section else config.data_dir,
        phrase=_phrase(data.get("phrase", {})),
        pause_hotkey=_hotkey(data.get("hotkeys", {}), config.pause_hotkey),
        typo_correction=_boolean(words.get("typo_correction", True), "words.typo_correction"),
        popup=_popup(data.get("popup", {})),
        partial_accept_hotkey=_enum(data.get("hotkeys", {}).get("partial_accept", "ctrl+right"), "hotkeys.partial_accept", ("ctrl+right", "alt+right", "ctrl+tab")),
        dismiss_hotkey=_enum(data.get("hotkeys", {}).get("dismiss", "escape"), "hotkeys.dismiss", ("escape", "ctrl+backspace", "alt+backspace")),
        private_mode=_boolean(data.get("privacy", {}).get("private_mode", False), "privacy.private_mode"),
        pause_minutes=_non_negative_number(data.get("privacy", {}).get("pause_minutes", 0), "privacy.pause_minutes"),
        app_profiles=_profiles(apps.get("profiles", [])),
    )


def _phrase(section: dict[str, Any]) -> PhraseConfig:
    base = PhraseConfig()
    p = base.provider

    def get(key: str, default: Any, check: Any) -> Any:
        return check(section[key], f"phrase.{key}") if key in section else default

    provider = ProviderSettings(
        base_url=get("base_url", p.base_url, _text),
        models=get("models", p.models, _model_list),
        provider_order=get("provider_order", p.provider_order, _provider_list),
        api_key=get("api_key", p.api_key, _api_key),
        fim=get("fim", p.fim, _boolean),
        max_tokens=get("max_tokens", p.max_tokens, _positive_int),
        temperature=get("temperature", p.temperature, _non_negative_number),
        timeout=get("timeout", p.timeout, _positive_number),
        stop=p.stop,
        **{key: get(key, getattr(p, key), check) for key, check in _PROVIDER_CHECKS.items()},
    )
    debounce_ms = get("debounce_ms", base.debounce * 1000, _non_negative_number)
    return PhraseConfig(
        enabled=get("enabled", base.enabled, _boolean),
        provider=provider,
        debounce=debounce_ms / 1000,
        context_before=get("context_before", base.context_before, _positive_int),
        context_after=get("context_after", base.context_after, _non_negative_int),
        instructions=get("instructions", base.instructions, _instructions),
        screen_context=get("screen_context", base.screen_context, _boolean),
        **{key: get(key, getattr(base, key), check) for key, check in _PHRASE_CHECKS.items()},
    )


def _check_known_keys(data: dict[str, Any]) -> None:
    for section, values in data.items():
        if section not in _SCHEMA:
            raise ConfigError(f"unknown section [{section}]")
        if not isinstance(values, dict):
            raise ConfigError(f"[{section}] must be a table")
        unknown = set(values) - _SCHEMA[section]
        if unknown:
            raise ConfigError(f"unknown setting(s) in [{section}]: {', '.join(sorted(unknown))}")


def _app_names(value: Any, name: str) -> frozenset[str]:
    if not isinstance(value, list) or not all(isinstance(v, str) for v in value):
        raise ConfigError(f"{name} must be a list of process names")
    return frozenset(v.lower() for v in value)


def _positive_int(value: Any, name: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < 1:
        raise ConfigError(f"{name} must be a whole number of at least 1")
    return value


def _non_negative_int(value: Any, name: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        raise ConfigError(f"{name} must be a whole number, 0 or more")
    return value


def _non_negative_number(value: Any, name: str) -> float:
    if not _finite_number(value) or value < 0:
        raise ConfigError(f"{name} must be a number, 0 or more")
    return float(value)


def _probability(value: Any, name: str) -> float:
    if not isinstance(value, int | float) or isinstance(value, bool) or not 0 <= value <= 1:
        raise ConfigError(f"{name} must be a number from 0 to 1")
    return float(value)


def _positive_number(value: Any, name: str) -> float:
    if not _finite_number(value) or value <= 0:
        raise ConfigError(f"{name} must be a number above 0")
    return float(value)


def _finite_number(value: Any) -> bool:
    if isinstance(value, bool) or not isinstance(value, int | float):
        return False
    try:
        return math.isfinite(value)
    except OverflowError:
        return False


def _text(value: Any, name: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ConfigError(f"{name} must be non-empty text")
    return value.strip()


def _model_list(value: Any, name: str) -> tuple[str, ...]:
    if not isinstance(value, list) or not value or not all(isinstance(m, str) and m.strip() for m in value):
        raise ConfigError(f"{name} must be a non-empty list of model names")
    return tuple(m.strip() for m in value)


def _instructions(value: Any, name: str) -> str:
    if not isinstance(value, str):
        raise ConfigError(f"{name} must be text in quotes")
    return value.strip()  # blank keeps the built-in instructions


def _api_key(value: Any, name: str) -> str:
    if not isinstance(value, str):
        raise ConfigError(f"{name} must be text in quotes")  # never echo the value: it may be a key
    return value.strip()


def _provider_list(value: Any, name: str) -> tuple[str, ...]:
    if not isinstance(value, list) or not all(isinstance(m, str) and m.strip() for m in value):
        raise ConfigError(f"{name} must be a list of OpenRouter provider names")
    return tuple(m.strip() for m in value)


def _boolean(value: Any, name: str) -> bool:
    if not isinstance(value, bool):
        raise ConfigError(f"{name} must be true or false")
    return value


def _directory(value: Any, name: str) -> Path:
    if not isinstance(value, str) or not value:
        raise ConfigError(f"{name} must be a folder path")
    return Path(value)


def _hotkey(section: dict[str, Any], default: str) -> str:
    if "pause" not in section:
        return default
    value = section["pause"]
    if not isinstance(value, str):
        raise ConfigError("hotkeys.pause must be text like \"ctrl+alt+p\", or \"\" for none")
    if value.strip():
        try:
            parse_hotkey(value)
        except ValueError as err:
            raise ConfigError(f"hotkeys.pause: {err}") from err
    return value.strip()


def _enum(value: Any, name: str, choices: tuple[str, ...]) -> str:
    if not isinstance(value, str) or value not in choices:
        raise ConfigError(f"{name} must be one of: {', '.join(choices)}")
    return value


def _choice(*choices: str):
    return lambda value, name: _enum(value, name, choices)


def _bounded(value: Any, name: str, lower: float, upper: float) -> float:
    number = _non_negative_number(value, name)
    if not lower <= number <= upper:
        raise ConfigError(f"{name} must be from {lower:g} to {upper:g}")
    return number


def _popup(section: dict[str, Any]) -> PopupConfig:
    font_size = _positive_int(section.get("font_size", 9), "popup.font_size")
    if not 7 <= font_size <= 24:
        raise ConfigError("popup.font_size must be from 7 to 24 whole points")
    return PopupConfig(
        font_size=font_size,
        width_scale=_bounded(section.get("width_scale", 1), "popup.width_scale", .5, 2),
    )


def _profiles(value: Any) -> tuple[AppProfile, ...]:
    if not isinstance(value, list):
        raise ConfigError("apps.profiles must be a list of inline tables")
    profiles = []
    seen = set()
    allowed = {"app", "phrase_mode", "completion_length", "writing_style", "spelling", "context_source", "learning", "word_limit"}
    for index, row in enumerate(value):
        name = f"apps.profiles[{index}]"
        if not isinstance(row, dict) or set(row) - allowed or "app" not in row:
            raise ConfigError(f"{name} must identify an app and contain only supported overrides")
        app = _text(row["app"], f"{name}.app").lower()
        if len(app) <= 4 or not app.endswith(".exe") or any(c in app for c in '\\/:*?"<>|') or any(ord(c) < 32 for c in app):
            raise ConfigError(f"{name}.app must be a process name ending in .exe")
        if app in seen:
            raise ConfigError(f"{name}.app duplicates another profile")
        seen.add(app)
        fields = {}
        for key, item in row.items():
            if key == "app": continue
            label = f"{name}.{key}"
            if key == "learning": fields[key] = _boolean(item, label)
            elif key == "word_limit":
                number = _positive_int(item, label)
                if number > 20: raise ConfigError(f"{label} must be from 1 to 20")
                fields[key] = number
            else: fields[key] = _PHRASE_CHECKS["mode" if key == "phrase_mode" else key](item, label)
        profiles.append(AppProfile(app, **fields))
    return tuple(profiles)


def effective_config(config: Config, app: str) -> Config:
    """Apply a process profile without changing privacy or blocked-app protections."""
    for profile in config.app_profiles:
        if profile.app.lower() != app.lower(): continue
        phrase = {("mode" if key == "phrase_mode" else key): getattr(profile, key)
                  for key in ("phrase_mode", "completion_length", "writing_style", "spelling", "context_source")
                  if getattr(profile, key) is not None}
        overrides = {key: getattr(profile, key) for key in ("learning", "word_limit") if getattr(profile, key) is not None}
        return replace(config, phrase=replace(config.phrase, **phrase), **overrides)
    return config


_PROVIDER_CHECKS = {
    "provider_sort": _choice("", "latency", "throughput", "price"),
    "allow_fallbacks": _boolean, "provider_ignore": _provider_list,
    "max_price_input": _non_negative_number, "max_price_output": _non_negative_number,
    "require_parameters": _boolean, "zdr": _boolean,
    "reasoning_effort": _choice("none", "minimal", "low", "medium", "high", "xhigh"),
}
_PHRASE_CHECKS = {
    "mode": _choice("apps", "auto", "hotkey"), "min_chars": _non_negative_int,
    "trigger": _choice("pause", "sentence"), "dismiss_cooldown": _non_negative_number,
    "failure_limit": _positive_int, "failure_pause": _non_negative_number,
    "completion_length": _choice("default", "short", "sentence", "long"),
    "writing_style": _choice("match", "neutral", "casual", "professional"),
    "spelling": _choice("auto", "canadian", "american", "british"),
    "preserve_casing": _boolean, "multiline": _boolean, "avoid_phrases": _provider_list,
    "context_source": _choice("ocr", "caret", "accessible"), "context_apps": _app_names,
    "preview_context": _boolean, "log_mode": _choice("full", "timings", "off"),
    "log_retention_minutes": _non_negative_number,
}
_PHRASE_KEYS.update(_PROVIDER_CHECKS)
_PHRASE_KEYS.update(_PHRASE_CHECKS)
