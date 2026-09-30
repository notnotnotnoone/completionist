"""User configuration, read from a TOML file with defaults for anything left out.

    [apps]
    block = ["code.exe", ...]   # no suggestions at all (replaces the default list)
    allow = ["obsidian.exe"]    # automatic phrase suggestions (replaces the default list)

    [words]
    limit = 5                   # words shown in the popup

    [learning]
    enabled = true              # learn your words and habits (counts only, never text)
    promote_after = 3           # uses before a word outside the dictionary is suggested

    [data]
    dir = "C:/somewhere"        # where personal counts and n-gram tables live

    [phrase]                    # phrase continuations from a cloud completion model
    enabled = true
    base_url = "https://openrouter.ai/api/v1"    # OpenRouter, or any OpenAI-compatible /completions endpoint
    models = ["mistralai/codestral-2508"]        # tried in order; the next only if the one before fails
    api_key_env = "OPENROUTER_API_KEY"           # name of the environment variable holding the key
    fim = false                 # send the text after the caret too (fill-in-the-middle); OpenRouter has none
    max_tokens = 40
    temperature = 0.2
    timeout = 4.0               # seconds
    debounce_ms = 350           # pause before an automatic request
    context_before = 8000       # characters of text before the caret sent to the model
    context_after = 2000
    daily_budget_usd = 0.50     # phrases stop for the day once this is spent
    price_input_per_m = 0.30    # dollars per million tokens, used to count spend (one price for all models)
    price_cached_per_m = 0.30
    price_output_per_m = 0.90

    [hotkeys]
    pause = "ctrl+alt+p"        # pause / resume Typer from anywhere ("" turns the hotkey off)
"""

import os
import tomllib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from typer_engine.budget import Prices
from typer_engine.hotkeys import parse_hotkey
from typer_engine.phrase_provider import ProviderSettings

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
    "enabled", "base_url", "models", "api_key_env", "fim", "max_tokens", "temperature", "timeout", "debounce_ms",
    "context_before", "context_after", "daily_budget_usd", "price_input_per_m", "price_cached_per_m", "price_output_per_m",
}  # fmt: skip
_SCHEMA = {
    "apps": {"block", "allow"},
    "words": {"limit"},
    "learning": {"enabled", "promote_after"},
    "data": {"dir"},
    "phrase": _PHRASE_KEYS,
    "hotkeys": {"pause"},
}


class ConfigError(Exception):
    """The config file can't be read or holds invalid settings."""


def default_data_dir() -> Path:
    localappdata = os.environ.get("LOCALAPPDATA")
    base = Path(localappdata) if localappdata else Path.home() / "AppData" / "Local"
    return base / "Typer"


@dataclass(frozen=True)
class PhraseConfig:
    enabled: bool = True
    provider: ProviderSettings = field(default_factory=ProviderSettings)
    debounce: float = 0.35  # seconds
    context_before: int = 8000
    context_after: int = 2000
    daily_budget_usd: float = 0.50
    prices: Prices = field(default_factory=Prices)


@dataclass(frozen=True)
class Config:
    block: frozenset[str] = DEFAULT_BLOCK
    allow: frozenset[str] = DEFAULT_ALLOW
    word_limit: int = 5
    learning: bool = True
    promote_after: int = 3
    data_dir: Path = field(default_factory=default_data_dir)
    phrase: PhraseConfig = field(default_factory=PhraseConfig)
    pause_hotkey: str = "ctrl+alt+p"


def default_config_path() -> Path:
    appdata = os.environ.get("APPDATA")
    base = Path(appdata) if appdata else Path.home() / "AppData" / "Roaming"
    return base / "Typer" / "config.toml"


def load_config(path: Path) -> Config:
    if not path.exists():
        return Config()
    try:
        data = tomllib.loads(path.read_text(encoding="utf-8"))
    except (OSError, tomllib.TOMLDecodeError) as err:
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
        learning=_boolean(learning["enabled"], "learning.enabled") if "enabled" in learning else config.learning,
        promote_after=(
            _positive_int(learning["promote_after"], "learning.promote_after")
            if "promote_after" in learning
            else config.promote_after
        ),
        data_dir=_directory(data_section["dir"], "data.dir") if "dir" in data_section else config.data_dir,
        phrase=_phrase(data.get("phrase", {})),
        pause_hotkey=_hotkey(data.get("hotkeys", {}), config.pause_hotkey),
    )


def _phrase(section: dict[str, Any]) -> PhraseConfig:
    base = PhraseConfig()
    p = base.provider

    def get(key: str, default: Any, check: Any) -> Any:
        return check(section[key], f"phrase.{key}") if key in section else default

    provider = ProviderSettings(
        base_url=get("base_url", p.base_url, _text),
        models=get("models", p.models, _model_list),
        api_key_env=get("api_key_env", p.api_key_env, _text),
        fim=get("fim", p.fim, _boolean),
        max_tokens=get("max_tokens", p.max_tokens, _positive_int),
        temperature=get("temperature", p.temperature, _non_negative_number),
        timeout=get("timeout", p.timeout, _positive_number),
        stop=p.stop,
    )
    prices = Prices(
        input_per_m=get("price_input_per_m", base.prices.input_per_m, _non_negative_number),
        cached_per_m=get("price_cached_per_m", base.prices.cached_per_m, _non_negative_number),
        output_per_m=get("price_output_per_m", base.prices.output_per_m, _non_negative_number),
    )
    debounce_ms = get("debounce_ms", base.debounce * 1000, _non_negative_number)
    return PhraseConfig(
        enabled=get("enabled", base.enabled, _boolean),
        provider=provider,
        debounce=debounce_ms / 1000,
        context_before=get("context_before", base.context_before, _positive_int),
        context_after=get("context_after", base.context_after, _non_negative_int),
        daily_budget_usd=get("daily_budget_usd", base.daily_budget_usd, _non_negative_number),
        prices=prices,
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
    if not isinstance(value, int | float) or isinstance(value, bool) or value < 0:
        raise ConfigError(f"{name} must be a number, 0 or more")
    return float(value)


def _positive_number(value: Any, name: str) -> float:
    if not isinstance(value, int | float) or isinstance(value, bool) or value <= 0:
        raise ConfigError(f"{name} must be a number above 0")
    return float(value)


def _text(value: Any, name: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ConfigError(f"{name} must be non-empty text")
    return value.strip()


def _model_list(value: Any, name: str) -> tuple[str, ...]:
    if not isinstance(value, list) or not value or not all(isinstance(m, str) and m.strip() for m in value):
        raise ConfigError(f"{name} must be a non-empty list of model names")
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
