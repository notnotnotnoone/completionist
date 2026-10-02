"""The writer's own word counts, kept in memory and saved to SQLite.

Only counts of single words, of (previous word, word) pairs and of (two previous words, word)
triples are stored: never sentences, never raw text. Words are plain lowercase English (letters with inner apostrophes) so numbers, codes and
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
    return word if 2 <= len(word) <= _MAX_WORD_LENGTH and _WORD.fullmatch(word) else None


class _Chains:
    """Counts of words that followed a context of `order` words, with what needs saving."""

    def __init__(self, table: str, order: int) -> None:
        self.table = table
        self.order = order
        self.columns = ["prev"] if order == 1 else ["prev2", "prev1"]
        self.rows: dict[tuple[str, ...], dict[str, int]] = {}
        self.totals: dict[tuple[str, ...], int] = {}
        self.dirty: set[tuple[tuple[str, ...], str]] = set()

    def add(self, context: tuple[str, ...], word: str, count: int) -> None:
        row = self.rows.setdefault(context, {})
        row[word] = row.get(word, 0) + count
        self.totals[context] = self.totals.get(context, 0) + count

    def size(self) -> int:
        return sum(len(row) for row in self.rows.values())

    def clear(self) -> None:
        self.rows.clear()
        self.totals.clear()
        self.dirty.clear()

    def forget(self, words: set[str]) -> None:
        """Drop every row that has one of `words` as its context or as the word that followed."""
        for context in list(self.rows):
            if words.intersection(context):
                del self.rows[context]
                del self.totals[context]
                continue
            row = self.rows[context]
            for word in words.intersection(row):
                self.totals[context] -= row.pop(word)
            if not row:
                del self.rows[context]
                del self.totals[context]
        self.dirty = {(c, w) for c, w in self.dirty if c in self.rows and w in self.rows[c]}

    def remove(self, context: tuple[str, ...], word: str) -> bool:
        """Drop one count row (this word after this context), leaving everything else. False if it wasn't there."""
        row = self.rows.get(context)
        if row is None or word not in row:
            return False
        self.totals[context] -= row.pop(word)
        if not row:
            del self.rows[context]
            del self.totals[context]
        self.dirty.discard((context, word))
        return True

    def prune_singles(self) -> None:
        for context in list(self.rows):
            row = self.rows[context]
            for word in [w for w, n in row.items() if n == 1]:
                del row[word]
                self.totals[context] -= 1
            if not row:
                del self.rows[context]
                del self.totals[context]


class PersonalStore:
    def __init__(self, path: Path | None = None, max_bigrams: int = 300_000) -> None:
        """Counts held in memory; with a `path`, loaded from and saved to that SQLite file."""
        self._path = path
        self._max_bigrams = max_bigrams
        self._words: dict[str, int] = {}
        self._sorted: list[str] = []
        self._total = 0
        self._chains = {1: _Chains("bigrams", 1), 2: _Chains("trigrams", 2)}
        self._dirty_words: set[str] = set()
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
        for order, chains in self._chains.items():
            if len(context) < order:
                continue
            previous = [_clean(w) for w in context[-order:]]
            if None in previous:
                continue
            key = tuple(w for w in previous if w is not None)
            chains.add(key, cleaned, count)
            chains.dirty.add((key, cleaned))

    # -- querying --------------------------------------------------------------------------------

    def counts(self, context: tuple[str, ...], prefix: str) -> Counts:
        prefix = prefix.lower()
        if not context:
            lo = bisect_left(self._sorted, prefix)
            hi = bisect_left(self._sorted, prefix + _AFTER_LAST_KEY, lo)
            matches = {w: self._words[w] for w in self._sorted[lo:hi]}
            return Counts(_top(matches), self._total)
        chains = self._chains[min(len(context), 2)]
        key = tuple(w.lower() for w in context[-chains.order :])
        row = chains.rows.get(key)
        if not row:
            return Counts({}, 0)
        matches = {w: n for w, n in row.items() if w.startswith(prefix)}
        return Counts(_top(matches), chains.totals[key])

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
        for chains in self._chains.values():
            columns = ", ".join(f"{c} TEXT NOT NULL" for c in chains.columns)
            key = ", ".join(chains.columns)
            db.execute(
                f"CREATE TABLE IF NOT EXISTS {chains.table} ({columns}, word TEXT NOT NULL, n INTEGER NOT NULL,"
                f" PRIMARY KEY ({key}, word)) WITHOUT ROWID"
            )
        for word, n in db.execute("SELECT word, n FROM words"):
            self._words[word] = n
            self._total += n
        self._sorted = sorted(self._words)
        for chains in self._chains.values():
            cols = ", ".join(chains.columns)
            for row in db.execute(f"SELECT {cols}, word, n FROM {chains.table}"):
                chains.add(tuple(row[: chains.order]), row[chains.order], row[chains.order + 1])
        db.commit()
        for word in tuple(self._words):
            if len(word) == 1:
                self.forget(word)

    def _reset_memory(self) -> None:
        self._words.clear()
        self._sorted.clear()
        self._total = 0
        for chains in self._chains.values():
            chains.clear()
        self._dirty_words.clear()

    def flush(self) -> None:
        if self._db is None or not (self._dirty_words or any(c.dirty for c in self._chains.values())):
            return
        try:
            with self._db:
                self._db.executemany(
                    "INSERT INTO words (word, n) VALUES (?, ?) ON CONFLICT(word) DO UPDATE SET n = excluded.n",
                    [(w, self._words[w]) for w in self._dirty_words],
                )
                for chains in self._chains.values():
                    cols = ", ".join(chains.columns)
                    marks = ", ".join("?" * (chains.order + 2))
                    self._db.executemany(
                        f"INSERT INTO {chains.table} ({cols}, word, n) VALUES ({marks})"
                        f" ON CONFLICT({cols}, word) DO UPDATE SET n = excluded.n",
                        [(*p, w, chains.rows[p][w]) for p, w in chains.dirty],
                    )
            self._dirty_words.clear()
            for chains in self._chains.values():
                chains.dirty.clear()
            self._prune_chains()
        except sqlite3.Error as err:
            logger.warning("could not save the personal store: %s", err)  # keep the counts dirty; retry next time

    def _prune_chains(self) -> None:
        for chains in self._chains.values():
            if chains.size() <= self._max_bigrams:
                continue
            chains.prune_singles()
            if self._db is not None:
                with self._db:
                    self._db.execute(f"DELETE FROM {chains.table} WHERE n = 1")

    def words(self, search: str = "", limit: int | None = None) -> list[tuple[str, int]]:
        """The words learned and how often each was used, most used first; `search` keeps words containing it."""
        search = search.lower()
        found = [(w, n) for w, n in self._words.items() if search in w]
        found.sort(key=lambda item: (-item[1], item[0]))
        return found if limit is None else found[:limit]

    def trigrams(self, search: str = "", limit: int | None = None) -> list[tuple[tuple[str, str, str], int]]:
        """The three-word sequences learned and how often each was typed, most used first.

        `search` keeps the ones that contain it (any word, or a phrase like "thank you").
        """
        search = search.lower().strip()
        found = [
            ((a, b, c), n)
            for (a, b), row in self._chains[2].rows.items()
            for c, n in row.items()
            if search in f"{a} {b} {c}"
        ]
        found.sort(key=lambda item: (-item[1], item[0]))
        return found if limit is None else found[:limit]

    def forget_trigram(self, first: str, second: str, third: str) -> bool:
        """Forget one three-word sequence. The words and the two-word pairs stay, since other sentences use them.

        False if it wasn't known.
        """
        first, second, third = first.lower(), second.lower(), third.lower()
        if not self._chains[2].remove((first, second), third):
            return False
        if self._db is not None:
            try:
                with self._db:
                    self._db.execute("DELETE FROM trigrams WHERE prev2 = ? AND prev1 = ? AND word = ?", (first, second, third))
            except sqlite3.Error as err:
                logger.warning("could not remove a trigram from the personal store file: %s", err)
        return True

    def forget(self, word: str) -> bool:
        """Forget a word, and every pair and triple it is part of, in memory and in the file.

        False if the word wasn't known.
        """
        word = word.lower()
        if word not in self._words:
            return False
        self._total -= self._words.pop(word)
        self._sorted.remove(word)
        self._dirty_words.discard(word)
        for chains in self._chains.values():
            chains.forget({word})
        if self._db is not None:
            try:
                with self._db:
                    self._db.execute("DELETE FROM words WHERE word = ?", (word,))
                    for chains in self._chains.values():
                        tests = " OR ".join(f"{c} = ?" for c in [*chains.columns, "word"])
                        self._db.execute(f"DELETE FROM {chains.table} WHERE {tests}", (word,) * (chains.order + 1))
            except sqlite3.Error as err:
                logger.warning("could not remove %r from the personal store file: %s", word, err)
        return True

    def clear(self) -> None:
        """Forget everything, including what's saved."""
        self._reset_memory()
        if self._db is not None:
            with self._db:
                self._db.execute("DELETE FROM words")
                for chains in self._chains.values():
                    self._db.execute(f"DELETE FROM {chains.table}")

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
