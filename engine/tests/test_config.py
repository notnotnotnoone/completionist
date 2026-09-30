from pathlib import Path

import pytest

from completionist_engine.config import Config, ConfigError, default_config_path, load_config


def write(tmp_path: Path, text: str) -> Path:
    path = tmp_path / "config.toml"
    path.write_text(text, encoding="utf-8")
    return path


def test_missing_file_gives_defaults(tmp_path):
    assert load_config(tmp_path / "nope.toml") == Config()


def test_defaults_block_editors_and_terminals():
    config = Config()
    assert {"code.exe", "devenv.exe", "windowsterminal.exe", "cmd.exe"} <= config.block
    assert config.word_limit == 5


def test_file_overrides_defaults(tmp_path):
    path = write(
        tmp_path,
        """
        [apps]
        block = ["Code.exe"]
        allow = ["Obsidian.exe", "WINWORD.EXE"]

        [words]
        limit = 8
        """,
    )
    assert load_config(path) == Config(
        block=frozenset({"code.exe"}),
        allow=frozenset({"obsidian.exe", "winword.exe"}),
        word_limit=8,
    )


def test_partial_file_keeps_other_defaults(tmp_path):
    path = write(tmp_path, "[words]\nlimit = 3\n")
    assert load_config(path) == Config(word_limit=3)


@pytest.mark.parametrize(
    "text",
    [
        "[words]\nlimit = 'five'\n",
        "[words]\nlimit = 0\n",
        "[apps]\nblock = 'code.exe'\n",
        "[apps]\nallow = [1, 2]\n",
        "[apps]\nblok = []\n",
        "[colours]\nx = 1\n",
        "this is not toml = = =",
    ],
)
def test_invalid_config_is_rejected(tmp_path, text):
    with pytest.raises(ConfigError):
        load_config(write(tmp_path, text))


def test_default_path_is_under_appdata(monkeypatch, tmp_path):
    monkeypatch.setenv("APPDATA", str(tmp_path))
    assert default_config_path() == tmp_path / "Completionist" / "config.toml"


def test_learning_defaults_on_with_promotion_after_three_uses():
    config = Config()
    assert config.learning is True
    assert config.promote_after == 3


def test_learning_can_be_tuned_or_switched_off(tmp_path):
    path = write(tmp_path, "[learning]\nenabled = false\npromote_after = 5\n")
    config = load_config(path)
    assert config.learning is False
    assert config.promote_after == 5


@pytest.mark.parametrize(
    "text",
    ["[learning]\nenabled = 'yes'\n", "[learning]\npromote_after = 0\n", "[learning]\nturbo = true\n", "[data]\ndir = 3\n"],
)
def test_invalid_learning_or_data_settings_are_rejected(tmp_path, text):
    with pytest.raises(ConfigError):
        load_config(write(tmp_path, text))


def test_data_dir_defaults_under_localappdata_and_can_move(monkeypatch, tmp_path):
    monkeypatch.setenv("LOCALAPPDATA", str(tmp_path))
    assert Config().data_dir == tmp_path / "Completionist"
    moved = load_config(write(tmp_path, f"[data]\ndir = '{tmp_path / 'elsewhere'}'\n"))
    assert moved.data_dir == tmp_path / "elsewhere"


# --- phrase settings --------------------------------------------------------------------------


def test_phrase_defaults():
    phrase = Config().phrase
    assert phrase.enabled is True
    assert phrase.provider.api_key == ""
    assert phrase.provider.base_url == "https://openrouter.ai/api/v1"
    assert phrase.provider.fim is False
    assert phrase.provider.models == ("meta-llama/llama-3.3-70b-instruct",)
    assert phrase.provider.provider_order == ("Groq",)
    assert phrase.debounce == pytest.approx(0.35)
    assert (phrase.context_before, phrase.context_after) == (8000, 2000)


def test_phrase_settings_can_be_overridden(tmp_path):
    path = write(
        tmp_path,
        """
        [phrase]
        enabled = false
        base_url = "https://api.example.com/v1"
        models = ["small-fim", "backup-fim"]
        provider_order = ["DeepInfra", "Fireworks"]
        api_key = "  test-key-123  "
        fim = false
        max_tokens = 24
        temperature = 0
        timeout = 2.5
        debounce_ms = 500
        context_before = 4000
        context_after = 0
        """,
    )
    phrase = load_config(path).phrase
    assert phrase.enabled is False
    assert (phrase.provider.base_url, phrase.provider.models, phrase.provider.api_key) == ("https://api.example.com/v1", ("small-fim", "backup-fim"), "test-key-123")
    assert phrase.provider.fim is False and phrase.provider.max_tokens == 24 and phrase.provider.temperature == 0
    assert phrase.provider.timeout == 2.5
    assert phrase.provider.provider_order == ("DeepInfra", "Fireworks")
    assert phrase.debounce == pytest.approx(0.5)
    assert (phrase.context_before, phrase.context_after) == (4000, 0)


def test_an_empty_api_key_is_allowed_and_means_phrases_are_off(tmp_path):
    assert load_config(write(tmp_path, "[phrase]\napi_key = ''\n")).phrase.provider.api_key == ""


def test_the_old_environment_variable_setting_is_gone(tmp_path):
    with pytest.raises(ConfigError, match="api_key_env"):
        load_config(write(tmp_path, "[phrase]\napi_key_env = 'OPENROUTER_API_KEY'\n"))


def test_a_bad_key_error_never_shows_the_key(tmp_path):
    with pytest.raises(ConfigError) as info:
        load_config(write(tmp_path, "[phrase]\napi_key = 12345\n"))
    assert "12345" not in str(info.value)


@pytest.mark.parametrize(
    "line",
    [
        "enabled = 'yes'", "base_url = ''", "models = 3", "models = []", "models = ['a', 3]", "models = 'a'", "fim = 1", "max_tokens = 0", "temperature = -1", "timeout = 0",
        "api_key = 3", "api_key = ['a']",
        "provider_order = 'DeepInfra'", "provider_order = [3]", "provider_order = ['']",
        "debounce_ms = -5", "context_before = 0", "context_after = -1",
        "turbo = true", "daily_budget_usd = 0.5", "price_input_per_m = 0.3",
    ],
)  # fmt: skip
def test_invalid_phrase_settings_are_rejected(tmp_path, line):
    with pytest.raises(ConfigError):
        load_config(write(tmp_path, f"[phrase]\n{line}\n"))


def test_the_pause_hotkey_defaults_to_ctrl_alt_p_and_can_change_or_be_turned_off(tmp_path):
    assert Config().pause_hotkey == "ctrl+alt+p"
    assert load_config(write(tmp_path, '[hotkeys]\npause = "ctrl+shift+f9"\n')).pause_hotkey == "ctrl+shift+f9"
    assert load_config(write(tmp_path, '[hotkeys]\npause = ""\n')).pause_hotkey == ""


@pytest.mark.parametrize("line", ["pause = 5", 'pause = "p"', 'pause = "ctrl+banana"', 'other = "x"'])
def test_a_bad_pause_hotkey_is_rejected(tmp_path, line):
    with pytest.raises(ConfigError):
        load_config(write(tmp_path, f"[hotkeys]\n{line}\n"))


def test_legacy_typer_folders_move_to_the_new_name(tmp_path):
    from completionist_engine.config import migrate_legacy_dirs

    local, roaming = tmp_path / "local", tmp_path / "roaming"
    (local / "Typer").mkdir(parents=True)
    (local / "Typer" / "personal.sqlite").write_text("mine")
    (roaming / "Typer").mkdir(parents=True)
    (roaming / "Typer" / "config.toml").write_text("[words]\nlimit = 7\n")

    migrate_legacy_dirs(local, roaming)

    assert (local / "Completionist" / "personal.sqlite").read_text() == "mine"
    assert (roaming / "Completionist" / "config.toml").exists()
    assert not (local / "Typer").exists() and not (roaming / "Typer").exists()


def test_legacy_migration_never_overwrites_the_new_folder(tmp_path):
    from completionist_engine.config import migrate_legacy_dirs

    local, roaming = tmp_path / "local", tmp_path / "roaming"
    (local / "Typer").mkdir(parents=True)
    (local / "Typer" / "old.txt").write_text("old")
    (local / "Completionist").mkdir()
    (local / "Completionist" / "new.txt").write_text("new")

    migrate_legacy_dirs(local, roaming)

    assert (local / "Completionist" / "new.txt").exists()
    assert (local / "Typer" / "old.txt").exists()


def test_next_words_are_on_by_default_with_a_five_percent_threshold():
    config = Config()
    assert config.next_words is True
    assert config.next_threshold == 0.05


def test_next_words_can_be_switched_off_and_the_threshold_set_or_switched_off(tmp_path):
    config = load_config(write(tmp_path, "[words]\nnext = false\nnext_threshold = 0\n"))
    assert config.next_words is False
    assert config.next_threshold == 0.0
    assert load_config(write(tmp_path, "[words]\nnext_threshold = 0.2\n")).next_threshold == 0.2


@pytest.mark.parametrize(
    "text", ["[words]\nnext = 'yes'\n", "[words]\nnext_threshold = -1\n", "[words]\nnext_threshold = 1.5\n", "[words]\nnext_threshold = 'x'\n"]
)
def test_invalid_next_word_settings_are_rejected(tmp_path, text):
    with pytest.raises(ConfigError):
        load_config(write(tmp_path, text))


def test_chunks_are_on_by_default_and_can_be_switched_off(tmp_path):
    assert Config().chunks is True
    assert load_config(write(tmp_path, "[words]\nchunks = false\n")).chunks is False
    with pytest.raises(ConfigError):
        load_config(write(tmp_path, "[words]\nchunks = 3\n"))
