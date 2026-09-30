"""Local usage numbers: how often suggestions are shown and accepted, keystrokes saved, provider speed.

Only counts and timings are stored (per day, per app name, per provider), never anything typed.
"""

import logging
import sqlite3
from collections import defaultdict
from collections.abc import Callable
from dataclasses import dataclass, field
from datetime import date, timedelta
from pathlib import Path

logger = logging.getLogger("completionist_engine.metrics")

_BUCKET_MS = 50
_BUCKETS = 100  # time to first token in 50 ms steps up to 5 s; the last bucket holds everything slower

KINDS = ("word", "phrase", "phrase_word")


@dataclass
class AppSummary:
    shown: int = 0
    accepted: int = 0
    saved: int = 0


@dataclass
class ProviderSummary:
    requests: int = 0
    errors: int = 0
    ttft_p50: float = 0.0
    ttft_p95: float = 0.0
    total_avg: float = 0.0

    @property
    def error_rate(self) -> float:
        return self.errors / self.requests if self.requests else 0.0


@dataclass
class Summary:
    shown_words: int = 0
    shown_phrases: int = 0
    accepted_words: int = 0
    accepted_phrases: int = 0
    accepted_partial: int = 0
    dismissed: int = 0
    keystrokes_saved: int = 0
    per_app: dict[str, AppSummary] = field(default_factory=dict)
    providers: dict[str, ProviderSummary] = field(default_factory=dict)

    @property
    def word_acceptance_rate(self) -> float:
        return self.accepted_words / self.shown_words if self.shown_words else 0.0

    @property
    def phrase_acceptance_rate(self) -> float:
        return self.accepted_phrases / self.shown_phrases if self.shown_phrases else 0.0


class Metrics:
    def __init__(self, path: Path | None = None, today: Callable[[], date] = date.today) -> None:
        self._today = today
        self._path = path
        self._db: sqlite3.Connection | None = None
        # (day, app, kind) -> [count, chars, saved]; "shown_*", "accept_*", "dismiss" kinds
        self._events: dict[tuple[str, str, str], list[int]] = defaultdict(lambda: [0, 0, 0])
        self._providers: dict[tuple[str, str], list[float]] = defaultdict(lambda: [0, 0, 0.0])  # requests, errors, total_s
        self._ttft: dict[tuple[str, str, int], int] = defaultdict(int)
        self._dirty = False
        if path is not None:
            self._open(path)

    # -- recording -------------------------------------------------------------------------------

    def record_shown(self, app: str, kind: str) -> None:
        """A popup (or phrase row) appeared: `kind` is "word" or "phrase"."""
        self._events[(self._day(), app, f"shown_{kind}")][0] += 1
        self._dirty = True

    def record_accept(self, app: str, kind: str, chars: int) -> None:
        """A suggestion was taken: `chars` inserted; `kind` is "word", "phrase" or "phrase_word"."""
        entry = self._events[(self._day(), app, f"accept_{kind}")]
        entry[0] += 1
        entry[1] += chars
        entry[2] += max(chars - 1, 0)  # the key that accepted it isn't saved
        self._dirty = True

    def record_dismiss(self, app: str) -> None:
        self._events[(self._day(), app, "dismiss")][0] += 1
        self._dirty = True

    def record_provider(self, provider: str, ttft: float | None, total: float | None, ok: bool) -> None:
        day = self._day()
        entry = self._providers[(day, provider)]
        entry[0] += 1
        if not ok:
            entry[1] += 1
        else:
            entry[2] += total or 0.0
            if ttft is not None:
                self._ttft[(day, provider, min(int(ttft * 1000) // _BUCKET_MS, _BUCKETS - 1))] += 1
        self._dirty = True

    def _day(self) -> str:
        return self._today().isoformat()

    # -- summary ---------------------------------------------------------------------------------

    def summary(self, days: int = 7) -> Summary:
        """Everything recorded in the last `days` days, today included."""
        since = (self._today() - timedelta(days=days - 1)).isoformat()
        events = self._merged_events(since)
        s = Summary()
        for (app, kind), (count, _chars, saved) in events.items():
            per_app = s.per_app.setdefault(app, AppSummary())
            if kind == "shown_word":
                s.shown_words += count
                per_app.shown += count
            elif kind == "shown_phrase":
                s.shown_phrases += count
                per_app.shown += count
            elif kind.startswith("accept_"):
                per_app.accepted += count
                per_app.saved += saved
                s.keystrokes_saved += saved
                if kind == "accept_word":
                    s.accepted_words += count
                elif kind == "accept_phrase":
                    s.accepted_phrases += count
                else:
                    s.accepted_partial += count
            elif kind == "dismiss":
                s.dismissed += count
        s.per_app = {a: v for a, v in s.per_app.items() if v.shown or v.accepted}
        totals, buckets = self._merged_providers(since)
        for provider, (requests, errors, total_s) in totals.items():
            ok = requests - errors
            counts = buckets.get(provider, {})
            s.providers[provider] = ProviderSummary(
                requests=int(requests),
                errors=int(errors),
                ttft_p50=_percentile(counts, 0.50),
                ttft_p95=_percentile(counts, 0.95),
                total_avg=total_s / ok if ok > 0 else 0.0,
            )
        return s

    def daily(self, days: int = 7) -> list[tuple[str, int, int, int]]:
        """One (day, shown, accepted, keystrokes saved) row for each of the last `days` days, oldest first."""
        first = self._today() - timedelta(days=days - 1)
        rows = {(first + timedelta(n)).isoformat(): [0, 0, 0] for n in range(days)}
        found = [(day, kind, count, saved) for (day, _app, kind), (count, _chars, saved) in self._events.items()]
        if self._db is not None:
            found += self._db.execute("SELECT day, kind, count, saved FROM daily WHERE day >= ?", (first.isoformat(),)).fetchall()
        for day, kind, count, saved in found:
            if day in rows:
                if kind.startswith("shown_"):
                    rows[day][0] += count
                elif kind.startswith("accept_"):
                    rows[day][1] += count
                    rows[day][2] += saved
        return [(day, *values) for day, values in rows.items()]

    def _merged_events(self, since: str) -> dict[tuple[str, str], list[int]]:
        merged: dict[tuple[str, str], list[int]] = defaultdict(lambda: [0, 0, 0])
        for (day, app, kind), values in self._events.items():
            if day >= since:
                for i in range(3):
                    merged[(app, kind)][i] += values[i]
        if self._db is not None:
            for app, kind, count, chars, saved in self._db.execute(
                "SELECT app, kind, SUM(count), SUM(chars), SUM(saved) FROM daily WHERE day >= ? GROUP BY app, kind", (since,)
            ):
                for i, v in enumerate((count, chars, saved)):
                    merged[(app, kind)][i] += v
        return merged

    def _merged_providers(self, since: str):
        totals: dict[str, list[float]] = defaultdict(lambda: [0, 0, 0.0])
        buckets: dict[str, dict[int, int]] = defaultdict(lambda: defaultdict(int))
        for (day, provider), values in self._providers.items():
            if day >= since:
                for i in range(3):
                    totals[provider][i] += values[i]
        for (day, provider, bucket), count in self._ttft.items():
            if day >= since:
                buckets[provider][bucket] += count
        if self._db is not None:
            for provider, requests, errors, total_s in self._db.execute(
                "SELECT provider, SUM(requests), SUM(errors), SUM(total_s) FROM provider_daily WHERE day >= ? GROUP BY provider", (since,)
            ):
                for i, v in enumerate((requests, errors, total_s)):
                    totals[provider][i] += v
            for provider, bucket, count in self._db.execute(
                "SELECT provider, bucket, SUM(count) FROM ttft WHERE day >= ? GROUP BY provider, bucket", (since,)
            ):
                buckets[provider][bucket] += count
        return totals, buckets

    # -- persistence -----------------------------------------------------------------------------

    def _open(self, path: Path) -> None:
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            self._connect(path)
        except sqlite3.DatabaseError as err:
            logger.warning("metrics file %s is unreadable (%s); starting fresh", path, err)
            if self._db is not None:
                self._db.close()
                self._db = None
            aside = path.with_name(f"{path.name}.corrupt")
            for n in range(1, 100):
                if not aside.exists():
                    break
                aside = path.with_name(f"{path.name}.corrupt{n}")
            path.replace(aside)
            self._connect(path)

    def _connect(self, path: Path) -> None:
        db = sqlite3.connect(path)
        self._db = db
        db.execute("PRAGMA journal_mode=WAL")
        db.execute("PRAGMA synchronous=NORMAL")
        db.executescript(
            """
            CREATE TABLE IF NOT EXISTS daily (day TEXT NOT NULL, app TEXT NOT NULL, kind TEXT NOT NULL,
                count INTEGER NOT NULL, chars INTEGER NOT NULL, saved INTEGER NOT NULL, PRIMARY KEY (day, app, kind)) WITHOUT ROWID;
            CREATE TABLE IF NOT EXISTS provider_daily (day TEXT NOT NULL, provider TEXT NOT NULL, requests INTEGER NOT NULL,
                errors INTEGER NOT NULL, total_s REAL NOT NULL, PRIMARY KEY (day, provider)) WITHOUT ROWID;
            CREATE TABLE IF NOT EXISTS ttft (day TEXT NOT NULL, provider TEXT NOT NULL, bucket INTEGER NOT NULL,
                count INTEGER NOT NULL, PRIMARY KEY (day, provider, bucket)) WITHOUT ROWID;
            """
        )
        if "cost" in [row[1] for row in db.execute("PRAGMA table_info(provider_daily)")]:
            db.execute("ALTER TABLE provider_daily DROP COLUMN cost")  # older files counted spend; that's gone
        db.commit()

    def flush(self) -> None:
        """Add what's been recorded since the last flush to the file."""
        if self._db is None or not self._dirty:
            return
        try:
            with self._db:
                for (day, app, kind), (count, chars, saved) in self._events.items():
                    self._db.execute(
                        "INSERT INTO daily VALUES (?, ?, ?, ?, ?, ?) ON CONFLICT(day, app, kind) DO UPDATE SET "
                        "count = count + excluded.count, chars = chars + excluded.chars, saved = saved + excluded.saved",
                        (day, app, kind, count, chars, saved),
                    )
                for (day, provider), (requests, errors, total_s) in self._providers.items():
                    self._db.execute(
                        "INSERT INTO provider_daily VALUES (?, ?, ?, ?, ?) ON CONFLICT(day, provider) DO UPDATE SET "
                        "requests = requests + excluded.requests, errors = errors + excluded.errors, "
                        "total_s = total_s + excluded.total_s",
                        (day, provider, requests, errors, total_s),
                    )
                for (day, provider, bucket), count in self._ttft.items():
                    self._db.execute(
                        "INSERT INTO ttft VALUES (?, ?, ?, ?) ON CONFLICT(day, provider, bucket) DO UPDATE SET count = count + excluded.count",
                        (day, provider, bucket, count),
                    )
            self._events.clear()
            self._providers.clear()
            self._ttft.clear()
            self._dirty = False
        except sqlite3.Error as err:
            logger.warning("could not save metrics: %s", err)

    def close(self) -> None:
        self.flush()
        if self._db is not None:
            self._db.close()
            self._db = None


def _percentile(buckets: dict[int, int], fraction: float) -> float:
    """Approximate percentile (seconds) from time-to-first-token buckets: the bucket's midpoint."""
    total = sum(buckets.values())
    if not total:
        return 0.0
    target = fraction * total
    seen = 0
    for bucket in sorted(buckets):
        seen += buckets[bucket]
        if seen >= target:
            return (bucket * _BUCKET_MS + _BUCKET_MS / 2) / 1000
    return 0.0


def format_summary(s: Summary, days: int) -> str:
    if not (s.shown_words or s.shown_phrases or s.accepted_words or s.accepted_phrases or s.accepted_partial or s.providers):
        return f"Completionist, last {days} days: nothing recorded yet."
    lines = [f"Completionist, last {days} days", ""]
    lines.append(f"  keystrokes saved   {s.keystrokes_saved:,}")
    lines.append(f"  words              shown {s.shown_words:,}, accepted {s.accepted_words:,} ({s.word_acceptance_rate:.0%})")
    lines.append(
        f"  phrases            shown {s.shown_phrases:,}, accepted {s.accepted_phrases:,} whole ({s.phrase_acceptance_rate:.0%})"
        f" + {s.accepted_partial:,} word by word"
    )
    lines.append(f"  dismissed          {s.dismissed:,}")
    if s.per_app:
        lines += ["", "  by app"]
        for app, a in sorted(s.per_app.items(), key=lambda item: -item[1].saved)[:10]:
            lines.append(f"    {app:<24} shown {a.shown:>6,}  accepted {a.accepted:>5,}  saved {a.saved:>6,}")
    if s.providers:
        lines += ["", "  phrase providers"]
        for name, p in s.providers.items():
            lines.append(
                f"    {name:<16} {p.requests:,} requests, {p.error_rate:.0%} errors, first token p50 {p.ttft_p50 * 1000:.0f} ms"
                f" / p95 {p.ttft_p95 * 1000:.0f} ms"
            )
    return "\n".join(lines)
