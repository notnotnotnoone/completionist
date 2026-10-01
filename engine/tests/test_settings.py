import pytest

from completionist_engine.config import ConfigError, load_config
from completionist_engine.settings import apply_settings, read_settings

COMMENTED = """# my settings
[words]
limit = 5   # how many to show
# next = false

[phrase]
# api_key = "sk-or-..."
models = [
    "a/first",   # cheap
    "b/second",
]

[hotkeys]
pause = "ctrl+alt+p"
"""


def write(tmp_path, text):
    path = tmp_path / "config.toml"
    path.write_text(text, encoding="utf-8")
    return path


def test_reading_a_missing_file_gives_the_defaults(tmp_path):
    settings = read_settings(tmp_path / "none.toml")
    assert settings["words"]["limit"] == 5
    assert settings["phrase"]["api_key_set"] is False
    assert settings["apps"]["block"] and "code.exe" in settings["apps"]["block"]


def test_a_change_keeps_comments_and_other_lines(tmp_path):
    path = write(tmp_path, COMMENTED)
    apply_settings(path, {"words": {"limit": 8}})
    text = path.read_text(encoding="utf-8")
    assert "limit = 8   # how many to show" in text
    assert text.startswith("# my settings\n") and "# next = false" in text and "# cheap" in text
    assert load_config(path).word_limit == 8


def test_a_list_spread_over_lines_is_replaced_whole(tmp_path):
    path = write(tmp_path, COMMENTED)
    apply_settings(path, {"phrase": {"models": ["c/third"]}})
    assert load_config(path).phrase.provider.models == ("c/third",)
    assert "b/second" not in path.read_text(encoding="utf-8")
    assert 'pause = "ctrl+alt+p"' in path.read_text(encoding="utf-8")


def test_preferred_providers_can_be_read_and_changed(tmp_path):
    path = write(tmp_path, COMMENTED)
    assert read_settings(path)["phrase"]["provider_order"] == ["Groq"]
    apply_settings(path, {"phrase": {"provider_order": ["Cerebras", "Groq"]}})
    assert load_config(path).phrase.provider.provider_order == ("Cerebras", "Groq")
    apply_settings(path, {"phrase": {"provider_order": []}})
    assert load_config(path).phrase.provider.provider_order == ()


def test_a_missing_setting_is_added_to_its_section_and_a_missing_section_is_created(tmp_path):
    path = write(tmp_path, "[words]\nlimit = 5\n\n[hotkeys]\npause = ''\n")
    apply_settings(path, {"words": {"chunks": False}, "learning": {"promote_after": 4}})
    config = load_config(path)
    assert config.chunks is False and config.promote_after == 4 and config.pause_hotkey == ""
    assert path.read_text(encoding="utf-8").index("chunks") < path.read_text(encoding="utf-8").index("[hotkeys]")


def test_a_file_that_does_not_exist_yet_is_created(tmp_path):
    path = tmp_path / "sub" / "config.toml"
    apply_settings(path, {"words": {"limit": 3}})
    assert load_config(path).word_limit == 3


def test_windows_line_endings_are_kept(tmp_path):
    path = tmp_path / "config.toml"
    path.write_bytes(b"[words]\r\nlimit = 5\r\n")
    apply_settings(path, {"words": {"limit": 6}, "learning": {"enabled": False}})
    raw = path.read_bytes()
    assert b"\r\nlimit = 6\r\n" in raw or raw.startswith(b"[words]\r\nlimit = 6\r\n")
    assert b"\n" not in raw.replace(b"\r\n", b"")


@pytest.mark.parametrize(
    "changes",
    [
        {"words": {"limit": 0}},
        {"words": {"next_threshold": 2}},
        {"words": {"limit": "five"}},
        {"words": {"nonsense": 1}},
        {"data": {"dir": "C:/elsewhere"}},  # not editable from the page
        {"phrase": {"base_url": "https://evil.example"}},
        {"apps": {"block": "code.exe"}},
        {"hotkeys": {"pause": "ctrl+banana"}},
        {"words": {"limit": None}},
    ],
)
def test_a_bad_change_is_refused_and_the_file_is_untouched(tmp_path, changes):
    path = write(tmp_path, COMMENTED)
    with pytest.raises(ConfigError):
        apply_settings(path, changes)
    assert path.read_text(encoding="utf-8") == COMMENTED
    assert not list(tmp_path.glob("*.tmp"))


def test_the_api_key_can_be_written_but_is_never_read_back(tmp_path):
    path = write(tmp_path, COMMENTED)
    apply_settings(path, {"phrase": {"api_key": "sk-or-secret-123"}})
    assert load_config(path).phrase.provider.api_key == "sk-or-secret-123"
    assert "secret" not in repr(read_settings(path))
    assert read_settings(path)["phrase"]["api_key_set"] is True
    apply_settings(path, {"phrase": {"api_key": ""}})
    assert read_settings(path)["phrase"]["api_key_set"] is False


def test_a_refused_key_is_not_echoed_in_the_error(tmp_path):
    path = write(tmp_path, COMMENTED)
    with pytest.raises(ConfigError) as info:
        apply_settings(path, {"phrase": {"api_key": 12345678}})
    assert "12345678" not in str(info.value)


def test_special_characters_in_text_survive(tmp_path):
    path = write(tmp_path, COMMENTED)
    apply_settings(path, {"phrase": {"api_key": 'a"b\\c #d'}})
    assert load_config(path).phrase.provider.api_key == 'a"b\\c #d'


def test_the_model_tuning_settings_are_read_with_their_defaults(tmp_path):
    phrase = read_settings(tmp_path / "none.toml")["phrase"]
    assert phrase["max_tokens"] == 40 and phrase["temperature"] == 0.2 and phrase["timeout"] == 4.0
    assert phrase["debounce"] == 0.35  # seconds, though the file counts milliseconds
    assert (phrase["context_before"], phrase["context_after"], phrase["fim"]) == (6000, 2000, False)


def test_the_tuning_settings_are_written_and_the_wait_is_saved_in_milliseconds(tmp_path):
    path = write(tmp_path, COMMENTED)
    apply_settings(path, {"phrase": {"max_tokens": 80, "temperature": 0.7, "timeout": 6, "debounce": 0.5, "context_before": 3000, "context_after": 0, "fim": True}})
    provider = load_config(path).phrase
    assert (provider.provider.max_tokens, provider.provider.temperature, provider.provider.timeout) == (80, 0.7, 6.0)
    assert provider.debounce == 0.5 and (provider.context_before, provider.context_after, provider.provider.fim) == (3000, 0, True)
    assert "debounce_ms = 500" in path.read_text(encoding="utf-8")
    assert read_settings(path)["phrase"]["debounce"] == 0.5


@pytest.mark.parametrize("bad", [{"debounce": -1}, {"debounce": "soon"}, {"debounce": True}, {"max_tokens": 0}, {"temperature": -0.5}])
def test_a_bad_tuning_value_is_refused_and_changes_nothing(tmp_path, bad):
    path = write(tmp_path, COMMENTED)
    before = path.read_text(encoding="utf-8")
    with pytest.raises(ConfigError):
        apply_settings(path, {"phrase": bad})
    assert path.read_text(encoding="utf-8") == before


def test_the_instructions_show_the_built_in_text_until_you_change_them(tmp_path):
    from completionist_engine.context import INSTRUCTIONS

    path = write(tmp_path, COMMENTED)
    phrase = read_settings(path)["phrase"]
    assert phrase["instructions"] == INSTRUCTIONS == phrase["instructions_default"] and phrase["instructions_custom"] is False
    apply_settings(path, {"phrase": {"instructions": "Continue the text.\nTEXT TO CONTINUE:"}})
    assert load_config(path).phrase.instructions == "Continue the text.\nTEXT TO CONTINUE:"
    phrase = read_settings(path)["phrase"]
    assert phrase["instructions"].startswith("Continue the text.\nTEXT") and phrase["instructions_custom"] is True


def test_saving_the_built_in_text_or_nothing_goes_back_to_the_default(tmp_path):
    from completionist_engine.context import INSTRUCTIONS

    path = write(tmp_path, COMMENTED)
    for restore in (INSTRUCTIONS, "", "  \n"):
        apply_settings(path, {"phrase": {"instructions": "Be brief."}})
        apply_settings(path, {"phrase": {"instructions": restore}})
        assert load_config(path).phrase.instructions == "" and read_settings(path)["phrase"]["instructions_custom"] is False


def test_reading_the_screen_is_on_by_default_and_can_be_switched_off(tmp_path):
    path = write(tmp_path, COMMENTED)
    assert read_settings(path)["phrase"]["screen_context"] is True
    assert load_config(path).phrase.screen_context is True
    apply_settings(path, {"phrase": {"screen_context": False}})
    assert load_config(path).phrase.screen_context is False
    assert read_settings(path)["phrase"]["screen_context"] is False
    assert "screen_context = false" in path.read_text(encoding="utf-8")


def test_a_bad_screen_context_value_is_refused(tmp_path):
    path = write(tmp_path, COMMENTED)
    before = path.read_text(encoding="utf-8")
    with pytest.raises(ConfigError):
        apply_settings(path, {"phrase": {"screen_context": "yes"}})
    assert path.read_text(encoding="utf-8") == before
