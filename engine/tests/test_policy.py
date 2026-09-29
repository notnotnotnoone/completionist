import pytest

from typer_engine.config import Config
from typer_engine.policy import Mode, decide

CONFIG = Config(block=frozenset({"code.exe"}), allow=frozenset({"obsidian.exe"}))
OFF = Mode(words=False, phrase="off")


def test_ordinary_app_gets_words_and_phrases_on_hotkey():
    assert decide("discord.exe", (), CONFIG) == Mode(words=True, phrase="hotkey")


def test_allow_listed_app_gets_automatic_phrases():
    assert decide("obsidian.exe", (), CONFIG) == Mode(words=True, phrase="auto")


def test_block_listed_app_gets_nothing():
    assert decide("code.exe", (), CONFIG) == OFF


def test_app_names_match_case_insensitively():
    assert decide("Code.EXE", (), CONFIG) == OFF
    assert decide("OBSIDIAN.exe", (), CONFIG).phrase == "auto"


@pytest.mark.parametrize(
    "scope",
    ["IS_PASSWORD", "IS_URL", "IS_EMAIL_SMTPEMAILADDRESS", "IS_EMAIL_USERNAME", "IS_NUMBER", "IS_DIGITS"],
)
def test_sensitive_or_structured_fields_get_nothing_even_when_allow_listed(scope):
    assert decide("obsidian.exe", ("IS_DEFAULT", scope), CONFIG) == OFF


def test_ordinary_input_scopes_do_not_silence():
    assert decide("discord.exe", ("IS_DEFAULT", "IS_CHAT"), CONFIG).words is True
