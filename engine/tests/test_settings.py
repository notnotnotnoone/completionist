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
