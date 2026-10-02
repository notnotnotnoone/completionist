"""Decides what Completionist may do in a given app and text field."""

from collections.abc import Iterable
from dataclasses import dataclass
from typing import Literal

from completionist_engine.config import Config, effective_config

# TSF input scopes where suggestions would leak secrets or get in the way of structured input.
SILENT_SCOPES = frozenset(
    {
        "IS_PASSWORD",
        "IS_URL",
        "IS_EMAIL_SMTPEMAILADDRESS",
        "IS_EMAIL_USERNAME",
        "IS_LOGONNAME",
        "IS_NUMBER",
        "IS_NUMBER_FULLWIDTH",
        "IS_DIGITS",
        "IS_TELEPHONE_FULLTELEPHONENUMBER",
        "IS_TELEPHONE_LOCALNUMBER",
    }
)


@dataclass(frozen=True)
class Mode:
    words: bool
    phrase: Literal["auto", "hotkey", "off"]


_OFF = Mode(words=False, phrase="off")


def decide(app: str, input_scope: Iterable[str], config: Config) -> Mode:
    app = app.lower()
    if app in config.block or not SILENT_SCOPES.isdisjoint(input_scope):
        return _OFF
    local = effective_config(config, app)
    if config.private_mode:
        return Mode(words=True, phrase="off")
    preference = local.phrase.mode
    return Mode(words=True, phrase=("auto" if app in config.allow else "hotkey") if preference == "apps" else preference)
