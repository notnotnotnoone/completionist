from datetime import date

from typer_engine.assemble import METRICS_FILE
from typer_engine.metrics import Metrics
from typer_engine.stats_cli import main
from typer_engine.tray import icon_image, pause_label, stats_line


def test_the_stats_command_prints_the_summary(tmp_path, capsys):
    metrics = Metrics(tmp_path / METRICS_FILE)
    for _ in range(4):
        metrics.record_shown("discord.exe", "word")
    metrics.record_accept("discord.exe", "word", chars=9)
    metrics.close()
    assert main(["--data-dir", str(tmp_path)]) == 0
    out = capsys.readouterr().out
    assert "keystrokes saved" in out and "8" in out and "discord.exe" in out


def test_the_stats_command_says_so_when_there_is_nothing_yet(tmp_path, capsys):
    assert main(["--data-dir", str(tmp_path)]) == 1
    assert "No stats yet" in capsys.readouterr().err


def test_the_stats_command_takes_a_day_count(tmp_path, capsys):
    metrics = Metrics(tmp_path / METRICS_FILE, today=lambda: date(2020, 1, 1))
    metrics.record_accept("a.exe", "word", chars=50)
    metrics.close()
    assert main(["--data-dir", str(tmp_path), "--days", "3"]) == 0
    assert "nothing recorded" in capsys.readouterr().out.lower()


def test_the_tray_icon_is_blue_when_on_and_grey_when_paused():
    on, off = icon_image(False, 32), icon_image(True, 32)
    assert on.size == (32, 32) and on.mode == "RGBA"
    assert on.getpixel((3, 16)) != off.getpixel((3, 16))
    assert on.getpixel((0, 0))[3] == 0  # the rounded corner is transparent


def test_the_pause_menu_item_says_what_clicking_it_does():
    assert pause_label(False) == "Pause Typer" and pause_label(True) == "Resume Typer"


def test_the_stats_notification_is_a_single_readable_line():
    metrics = Metrics()
    assert stats_line(metrics.summary()) == "Nothing recorded yet."
    for _ in range(10):
        metrics.record_shown("a.exe", "word")
    for _ in range(4):
        metrics.record_accept("a.exe", "word", chars=6)
    line = stats_line(metrics.summary())
    assert "20 keystrokes saved" in line and "40%" in line and "\n" not in line
