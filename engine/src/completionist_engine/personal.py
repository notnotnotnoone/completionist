"""The writer's own word counts, kept in memory and saved to SQLite.

Only counts of single words and of (previous word, word) pairs are stored: never sentences, never
raw text. Words are plain lowercase English (letters with inner apostrophes) so numbers, codes and
other structured input never end up in it.
"""

import logging
import re
import sqlite3
from bisect import bisect_left, insort
from pathlib import Path

from completionist_engine.counts import Counts

logger = logging.getLogger("completionist_engine.personal")

_WORD = re.compile(r"[a-z]+(?:'[a-z]+)*")
_MAX_WORD_LENGTH = 24
_MAX_RESULTS = 50
_AFTER_LAST_KEY = "\U0010ffff"


def _clean(word: str) -> str | None:
    word = word.lower()
    return word if len(word) <= _MAX_WORD_LENGTH and _WORD.fullmatch(word) else None


class PersonalStore:
    def __init__(self, path: Path | None = None, max_bigrams: int = 300_000) -> None:
        """Counts held in memory; with a `path`, loaded from and saved to that SQLite file."""
        self._path = path
        self._max_bigrams = max_bigrams
        self._words: dict[str, int] = {}
        self._sorted: list[str] = []
        self._total = 0
        self._bigrams: dict[str, dict[str, int]] = {}
        self._bigram_totals: dict[str, int] = {}
        self._dirty_words: set[str] = set()
        self._dirty_bigrams: set[tuple[str, str]] = set()
        self._db: sqlite3.Connection | None = None
        if path is not None:
            self._open(path)

    # -- recording -------------------------------------------------------------------------------

    def record_typed(self, word: str, context: tuple[str, ...]) -> None:
        self._add(word, context)

    def record_accepted(self, word: str, context: tuple[str, ...]) -> None:
        self._add(word, context)

    def _add(self, word: str, context: tuple[str, ...], count: int = 1) -> None:
        cleaned = _clean(word)
        if cleaned is None:
            return
        if cleaned not in self._words:
            insort(self._sorted, cleaned)
            self._words[cleaned] = 0
        self._words[cleaned] += count
        self._total += count
        self._dirty_words.add(cleaned)
        previous = _clean(context[-1]) if context else None
        if previous is not None:
            self._add_bigram(previous, cleaned, count)
            self._dirty_bigrams.add((previous, cleaned))

    def _add_bigram(self, previous: str, word: str, count: int) -> None:
        row = self._bigrams.setdefault(previous, {})
        row[word] = row.get(word, 0) + count
        self._bigram_totals[previous] = self._bigram_totals.get(previous, 0) + count

    # -- querying --------------------------------------------------------------------------------

    def counts(self, context: tuple[str, ...], prefix: str) -> Counts:
        prefix = prefix.lower()
        if not context:
            lo = bisect_left(self._sorted, prefix)
            hi = bisect_left(self._sorted, prefix + _AFTER_LAST_KEY, lo)
            matches = {w: self._words[w] for w in self._sorted[lo:hi]}
            return Counts(_top(matches), self._total)
        previous = context[-1].lower()
        row = self._bigrams.get(previous)
        if not row:
            return Counts({}, 0)
        matches = {w: n for w, n in row.items() if w.startswith(prefix)}
        return Counts(_top(matches), self._bigram_totals[previous])

    # -- persistence -----------------------------------------------------------------------------

    def _open(self, path: Path) -> None:
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            self._connect(path)
        except sqlite3.DatabaseError as err:
            logger.warning("personal store %s is unreadable (%s); starting empty", path, err)
            self._reset_memory()
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
        db.execute("CREATE TABLE IF NOT EXISTS words (word TEXT PRIMARY KEY, n INTEGER NOT NULL) WITHOUT ROWID")
        db.execute(
            "CREATE TABLE IF NOT EXISTS bigrams (prev TEXT NOT NULL, word TEXT NOT NULL, n INTEGER NOT NULL,"
            " PRIMARY KEY (prev, word)) WITHOUT ROWID"
        )
        for word, n in db.execute("SELECT word, n FROM words"):
            self._words[word] = n
            self._total += n
        self._sorted = sorted(self._words)
        for previous, word, n in db.execute("SELECT prev, word, n FROM bigrams"):
            self._add_bigram(previous, word, n)
        db.commit()

    def _reset_memory(self) -> None:
        self._words.clear()
        self._sorted.clear()
        self._total = 0
        self._bigrams.clear()
        self._bigram_totals.clear()
        self._dirty_words.clear()
        self._dirty_bigrams.clear()

    def flush(self) -> None:
        if self._db is None or not (self._dirty_words or self._dirty_bigrams):
            return
        try:
            with self._db:
                self._db.executemany(
                    "INSERT INTO words (word, n) VALUES (?, ?) ON CONFLICT(word) DO UPDATE SET n = excluded.n",
                    [(w, self._words[w]) for w in self._dirty_words],
                )
                self._db.executemany(
                    "INSERT INTO bigrams (prev, word, n) VALUES (?, ?, ?)"
                    " ON CONFLICT(prev, word) DO UPDATE SET n = excluded.n",
                    [(p, w, self._bigrams[p][w]) for p, w in self._dirty_bigrams],
                )
            self._dirty_words.clear()
            self._dirty_bigrams.clear()
            self._prune_bigrams()
        except sqlite3.Error as err:
            logger.warning("could not save the personal store: %s", err)  # keep the counts dirty; retry next time

    def _prune_bigrams(self) -> None:
        if sum(len(row) for row in self._bigrams.values()) <= self._max_bigrams:
            return
        for previous in list(self._bigrams):
            row = self._bigrams[previous]
            for word in [w for w, n in row.items() if n == 1]:
                del row[word]
                self._bigram_totals[previous] -= 1
            if not row:
                del self._bigrams[previous]
                del self._bigram_totals[previous]
        if self._db is not None:
            with self._db:
                self._db.execute("DELETE FROM bigrams WHERE n = 1")

    def clear(self) -> None:
        """Forget everything, including what's saved."""
        self._reset_memory()
        if self._db is not None:
            with self._db:
                self._db.execute("DELETE FROM words")
                self._db.execute("DELETE FROM bigrams")

    def close(self) -> None:
        self.flush()
        if self._db is not None:
            self._db.close()
            self._db = None


def _top(matches: dict[str, int]) -> dict[str, int]:
    if len(matches) <= _MAX_RESULTS:
        return matches
    best = sorted(matches.items(), key=lambda item: (-item[1], item[0]))[:_MAX_RESULTS]
    return dict(best)
