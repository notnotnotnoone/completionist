import sqlite3
from datetime import date

import pytest

from typer_engine.metrics import Metrics, format_summary

DAY = date(2026, 9, 29)


def new(**kwargs):
    return Metrics(today=lambda: DAY, **kwargs)


def test_an_empty_store_summarises_to_zeros():
    s = new().summary(days=7)
    assert (s.shown_words, s.accepted_words, s.keystrokes_saved) == (0, 0, 0)
    assert s.word_acceptance_rate == 0 and s.phrase_acceptance_rate == 0
    assert s.per_app == {} and s.providers == {}


def test_shown_and_accepted_are_counted_by_kind():
    m = new()
    for _ in range(4):
        m.record_shown("discord.exe", "word")
    m.record_shown("discord.exe", "phrase")
    m.record_accept("discord.exe", "word", chars=8)
    m.record_accept("discord.exe", "phrase", chars=30)
    m.record_accept("discord.exe", "phrase_word", chars=6)
    s = m.summary()
    assert (s.shown_words, s.shown_phrases) == (4, 1)
    assert (s.accepted_words, s.accepted_phrases, s.accepted_partial) == (1, 1, 1)
    assert s.word_acceptance_rate == pytest.approx(0.25)
    assert s.phrase_acceptance_rate == pytest.approx(1.0)


def test_keystrokes_saved_is_characters_inserted_minus_the_key_that_accepted_them():
    m = new()
    m.record_accept("a.exe", "word", chars=9)  # "recommend" typed as "recomm": 3 characters, Tab is 1
    m.record_accept("a.exe", "word", chars=3)
    m.record_accept("a.exe", "phrase", chars=21)
    assert m.summary().keystrokes_saved == (9 - 1) + (3 - 1) + (21 - 1)


def test_an_accept_that_inserts_nothing_saves_nothing_not_less():
    m = new()
    m.record_accept("a.exe", "word", chars=0)
    assert m.summary().keystrokes_saved == 0


def test_dismissals_are_counted():
    m = new()
    m.record_dismiss("a.exe")
    m.record_dismiss("b.exe")
    assert m.summary().dismissed == 2


def test_the_summary_breaks_down_by_app():
    m = new()
    m.record_shown("discord.exe", "word")
    m.record_shown("discord.exe", "word")
    m.record_accept("discord.exe", "word", chars=6)
    m.record_shown("notepad.exe", "word")
    per = m.summary().per_app
    assert (per["discord.exe"].shown, per["discord.exe"].accepted, per["discord.exe"].saved) == (2, 1, 5)
    assert (per["notepad.exe"].shown, per["notepad.exe"].accepted) == (1, 0)


def test_provider_latency_and_errors_are_tracked():
    m = new()
    for ttft in (0.12, 0.15, 0.18, 0.40):
        m.record_provider("deepseek", ttft=ttft, total=ttft + 0.1, ok=True)
    m.record_provider("deepseek", ttft=None, total=None, ok=False)
    p = m.summary().providers["deepseek"]
    assert p.requests == 5 and p.errors == 1
    assert 0.10 <= p.ttft_p50 <= 0.20
    assert 0.35 <= p.ttft_p95 <= 0.50
    assert p.error_rate == pytest.approx(0.2)


def test_the_window_covers_only_the_requested_days():
    day = [date(2026, 9, 20)]
    m = Metrics(today=lambda: day[0])
    m.record_accept("a.exe", "word", chars=10)
    day[0] = date(2026, 9, 29)
    m.record_accept("a.exe", "word", chars=5)
    assert m.summary(days=1).keystrokes_saved == 4
    assert m.summary(days=30).keystrokes_saved == 9 + 4


def test_counts_survive_a_flush_and_reopen(tmp_path):
    path = tmp_path / "metrics.sqlite"
    m = new(path=path)
    m.record_shown("a.exe", "word")
    m.record_accept("a.exe", "word", chars=7)
    m.record_provider("p", ttft=0.2, total=0.3, ok=True)
    m.flush()
    m.close()
    s = new(path=path).summary()
    assert (s.shown_words, s.accepted_words, s.keystrokes_saved) == (1, 1, 6)
    assert s.providers["p"].requests == 1


def test_reopening_keeps_adding_rather_than_overwriting(tmp_path):
    path = tmp_path / "metrics.sqlite"
    first = new(path=path)
    first.record_shown("a.exe", "word")
    first.close()
    second = new(path=path)
    second.record_shown("a.exe", "word")
    second.close()
    assert new(path=path).summary().shown_words == 2


def test_the_file_holds_only_counts_and_timings_never_text(tmp_path):
    path = tmp_path / "metrics.sqlite"
    m = new(path=path)
    m.record_shown("discord.exe", "word")
    m.record_accept("discord.exe", "word", chars=6)
    m.record_provider("p", ttft=0.2, total=0.3, ok=True)
    m.close()
    with sqlite3.connect(path) as db:
        for table in [r[0] for r in db.execute("select name from sqlite_master where type='table'")]:
            for row in db.execute(f"select * from {table}"):
                assert all(not isinstance(v, str) or v in {"discord.exe", "shown_word", "accept_word", "p", DAY.isoformat()} for v in row), (table, row)


def test_a_corrupt_file_is_set_aside(tmp_path):
    path = tmp_path / "metrics.sqlite"
    path.write_bytes(b"garbage" * 100)
    m = new(path=path)
    m.record_shown("a.exe", "word")
    m.close()
    assert new(path=path).summary().shown_words == 1
    assert list(tmp_path.glob("metrics.sqlite.corrupt*"))


def test_the_report_reads_like_a_summary():
    m = new()
    for _ in range(10):
        m.record_shown("discord.exe", "word")
    for chars in (8, 6, 9):
        m.record_accept("discord.exe", "word", chars=chars)
    m.record_provider("deepseek", ttft=0.2, total=0.4, ok=True)
    report = format_summary(m.summary(days=7), days=7)
    assert "last 7 days" in report
    assert "keystrokes saved" in report and "20" in report  # (8-1)+(6-1)+(9-1)
    assert "30%" in report  # 3 of 10 accepted
    assert "discord.exe" in report and "deepseek" in report


def test_an_empty_report_says_so():
    assert "nothing recorded" in format_summary(new().summary(), days=7).lower()


def test_a_file_from_before_spend_was_dropped_still_opens_and_keeps_its_counts(tmp_path):
    import sqlite3

    path = tmp_path / "metrics.sqlite"
    db = sqlite3.connect(path)
    db.executescript(
        """
        CREATE TABLE daily (day TEXT NOT NULL, app TEXT NOT NULL, kind TEXT NOT NULL,
            count INTEGER NOT NULL, chars INTEGER NOT NULL, saved INTEGER NOT NULL, PRIMARY KEY (day, app, kind)) WITHOUT ROWID;
        CREATE TABLE provider_daily (day TEXT NOT NULL, provider TEXT NOT NULL, requests INTEGER NOT NULL,
            errors INTEGER NOT NULL, total_s REAL NOT NULL, cost REAL NOT NULL, PRIMARY KEY (day, provider)) WITHOUT ROWID;
        CREATE TABLE ttft (day TEXT NOT NULL, provider TEXT NOT NULL, bucket INTEGER NOT NULL,
            count INTEGER NOT NULL, PRIMARY KEY (day, provider, bucket)) WITHOUT ROWID;
        INSERT INTO provider_daily VALUES ('2026-09-29', 'old-model', 7, 1, 3.0, 0.02);
        """
    )
    db.commit()
    db.close()
    m = Metrics(path, today=lambda: date(2026, 9, 29))
    m.record_provider("old-model", ttft=0.2, total=0.3, ok=True)
    m.flush()
    assert m.summary().providers["old-model"].requests == 8
    m.close()
