from pathlib import Path

import pytest

from typer_engine.config import Config, ConfigError, default_config_path, load_config


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
    assert default_config_path() == tmp_path / "Typer" / "config.toml"
