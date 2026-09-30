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


def test_phrase_defaults_match_the_budget_plan():
    phrase = Config().phrase
    assert phrase.enabled is True
    assert phrase.provider.api_key_env == "DEEPSEEK_API_KEY"
    assert phrase.provider.fim is True
    assert phrase.debounce == pytest.approx(0.35)
    assert (phrase.context_before, phrase.context_after) == (8000, 2000)
    assert phrase.daily_budget_usd == pytest.approx(0.50)


def test_phrase_settings_can_be_overridden(tmp_path):
    path = write(
        tmp_path,
        """
        [phrase]
        enabled = false
        base_url = "https://api.example.com/v1"
        model = "small-fim"
        api_key_env = "MY_KEY"
        fim = false
        max_tokens = 24
        temperature = 0
        timeout = 2.5
        debounce_ms = 500
        context_before = 4000
        context_after = 0
        daily_budget_usd = 0.25
        price_input_per_m = 0.1
        price_cached_per_m = 0.01
        price_output_per_m = 0.4
        """,
    )
    phrase = load_config(path).phrase
    assert phrase.enabled is False
    assert (phrase.provider.base_url, phrase.provider.model, phrase.provider.api_key_env) == ("https://api.example.com/v1", "small-fim", "MY_KEY")
    assert phrase.provider.fim is False and phrase.provider.max_tokens == 24 and phrase.provider.temperature == 0
    assert phrase.provider.timeout == 2.5
    assert phrase.debounce == pytest.approx(0.5)
    assert (phrase.context_before, phrase.context_after) == (4000, 0)
    assert phrase.daily_budget_usd == 0.25
    assert (phrase.prices.input_per_m, phrase.prices.cached_per_m, phrase.prices.output_per_m) == (0.1, 0.01, 0.4)


def test_the_api_key_itself_cannot_be_put_in_the_config(tmp_path):
    with pytest.raises(ConfigError, match="api_key"):
        load_config(write(tmp_path, "[phrase]\napi_key = 'sk-secret'\n"))


@pytest.mark.parametrize(
    "line",
    [
        "enabled = 'yes'", "base_url = ''", "model = 3", "fim = 1", "max_tokens = 0", "temperature = -1", "timeout = 0",
        "debounce_ms = -5", "context_before = 0", "context_after = -1", "daily_budget_usd = -0.5", "price_input_per_m = 'x'",
        "turbo = true",
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
