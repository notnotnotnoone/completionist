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
"""

import os
import tomllib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

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

_SCHEMA = {"apps": {"block", "allow"}, "words": {"limit"}, "learning": {"enabled", "promote_after"}, "data": {"dir"}}


class ConfigError(Exception):
    """The config file can't be read or holds invalid settings."""


def default_data_dir() -> Path:
    localappdata = os.environ.get("LOCALAPPDATA")
    base = Path(localappdata) if localappdata else Path.home() / "AppData" / "Local"
    return base / "Typer"


@dataclass(frozen=True)
class Config:
    block: frozenset[str] = DEFAULT_BLOCK
    allow: frozenset[str] = DEFAULT_ALLOW
    word_limit: int = 5
    learning: bool = True
    promote_after: int = 3
    data_dir: Path = field(default_factory=default_data_dir)


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


def _boolean(value: Any, name: str) -> bool:
    if not isinstance(value, bool):
        raise ConfigError(f"{name} must be true or false")
    return value


def _directory(value: Any, name: str) -> Path:
    if not isinstance(value, str) or not value:
        raise ConfigError(f"{name} must be a folder path")
    return Path(value)
