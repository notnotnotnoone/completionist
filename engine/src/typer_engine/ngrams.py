"""N-gram tables built from a text corpus, and the read-only reader the completer uses.

`build_ngrams` makes a SQLite file with word counts (and how often each word is written lowercase),
bigram and trigram counts pruned to the useful ones. `NgramTable` answers "what follows these words,
starting with this prefix?" and implements the completer's `CountSource`.
"""

import gzip
import re
import sqlite3
from collections import Counter
from collections.abc import Callable, Iterable, Iterator
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path

from typer_engine.counts import Counts

_SCHEMA_VERSION = "1"
_TOKEN = re.compile(r"[A-Za-z]+(?:'[A-Za-z]+)*|\d+")
# Some corpora (WikiText) split contractions: "do n't", "it 's". Join them back before tokenising.
_SPLIT_CLITIC = re.compile(r"\s+(?=(?:'(?:s|re|ve|ll|d|m)|n't)(?![A-Za-z]))")
_SENTENCE_BREAK = re.compile(r"(?<=[.!?])\s+")
_AFTER_LAST_KEY = "\U0010ffff"
_MAX_RESULTS = 50


@dataclass(frozen=True)
class UnigramStat:
    total: int
    """Occurrences in any case."""
    lower: int
    """Occurrences written all lowercase (or as the pronoun "I")."""


@dataclass(frozen=True)
class BuildReport:
    tokens: int
    vocabulary: int
    bigrams: int
    trigrams: int
    size_bytes: int


def _lines(sources: Iterable[Path]) -> Iterator[str]:
    files: list[Path] = []
    for source in sources:
        if source.is_dir():
            files += sorted(p for p in source.rglob("*") if p.is_file() and p.suffix in {".txt", ".gz"})
        else:
            files.append(source)
    for file in files:
        opener = gzip.open if file.suffix == ".gz" else open
        with opener(file, "rt", encoding="utf-8", errors="replace") as handle:  # type: ignore[operator]
            yield from handle


def _sentences(lines: Iterable[str]) -> Iterator[list[str]]:
    for line in lines:
        for sentence in _SENTENCE_BREAK.split(_SPLIT_CLITIC.sub("", line)):
            tokens = _TOKEN.findall(sentence)
            if tokens:
                yield tokens


def _is_lower(token: str) -> bool:
    return token.islower() or token == "I" or (token.startswith("I'") and token[2:].islower())


def _prune(counter: Counter[str], max_entries: int) -> None:
    """Drop the rarest entries (count 1, then 2, ...) until at most half of `max_entries` remain."""
    threshold = 1
    while len(counter) > max_entries // 2:
        for key in [k for k, n in counter.items() if n <= threshold]:
            del counter[key]
        threshold += 1


def _top_per_context(counts: Counter[str], minimum: int, per_context: int) -> Iterator[tuple[str, str, int]]:
    """(context, word, n) rows: entries with `minimum` sightings, best `per_context` for each context."""
    by_context: dict[str, list[tuple[int, str]]] = {}
    for key, n in counts.items():
        if n >= minimum:
            context, _, word = key.rpartition(" ")
            by_context.setdefault(context, []).append((n, word))
    for context, rows in by_context.items():
        rows.sort(key=lambda row: (-row[0], row[1]))
        for n, word in rows[:per_context]:
            yield context, word, n


def build_ngrams(
    sources: Iterable[Path],
    out: Path,
    *,
    vocab_size: int = 100_000,
    min_unigram: int = 3,
    min_bigram: int = 3,
    min_trigram: int = 3,
    per_context: int = 40,
    max_entries: int = 6_000_000,
    progress: Callable[[str], None] | None = None,
) -> BuildReport:
    """Count words, bigrams and trigrams in the text files under `sources` and write them to `out`.

    Only the `vocab_size` most common words are kept; other words and numbers break a chain, as does
    the end of a sentence. Rare n-grams are pruned while counting so memory stays bounded.
    """
    sources = list(sources)
    say = progress or (lambda _message: None)

    say("pass 1: counting words")
    total: Counter[str] = Counter()
    lower: Counter[str] = Counter()
    for tokens in _sentences(_lines(sources)):
        words = [t for t in tokens if not t[0].isdigit()]
        total.update(w.lower() for w in words)
        lower.update(w.lower() for w in words if _is_lower(w))
    ranked = sorted((w for w, n in total.items() if n >= min_unigram), key=lambda w: (-total[w], w))
    vocabulary = set(ranked[:vocab_size])
    say(f"{len(total)} distinct words, keeping {len(vocabulary)}")

    say("pass 2: counting bigrams and trigrams")
    bigrams: Counter[str] = Counter()
    trigrams: Counter[str] = Counter()
    tokens_seen = 0
    for tokens in _sentences(_lines(sources)):
        run: list[str] = []
        for token in [*tokens, ""]:  # the empty token flushes the last run
            word = token.lower()
            if word in vocabulary:
                run.append(word)
                continue
            tokens_seen += len(run)
            for i in range(1, len(run)):
                bigrams[f"{run[i - 1]} {run[i]}"] += 1
                if i >= 2:
                    trigrams[f"{run[i - 2]} {run[i - 1]} {run[i]}"] += 1
            run = []
        if len(bigrams) > max_entries:
            _prune(bigrams, max_entries)
        if len(trigrams) > max_entries:
            _prune(trigrams, max_entries)

    say("writing tables")
    tmp = out.with_name(out.name + ".building")
    tmp.unlink(missing_ok=True)
    out.parent.mkdir(parents=True, exist_ok=True)
    db = sqlite3.connect(tmp)
    try:
        db.executescript(
            """
            CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL) WITHOUT ROWID;
            CREATE TABLE unigram (word TEXT PRIMARY KEY, total INTEGER NOT NULL, lower INTEGER NOT NULL) WITHOUT ROWID;
            CREATE TABLE bigram (w1 TEXT NOT NULL, w2 TEXT NOT NULL, n INTEGER NOT NULL, PRIMARY KEY (w1, w2)) WITHOUT ROWID;
            CREATE TABLE trigram (ctx TEXT NOT NULL, w3 TEXT NOT NULL, n INTEGER NOT NULL, PRIMARY KEY (ctx, w3)) WITHOUT ROWID;
            CREATE TABLE context2 (ctx TEXT PRIMARY KEY, total INTEGER NOT NULL) WITHOUT ROWID;
            """
        )
        db.execute("INSERT INTO meta VALUES ('version', ?)", (_SCHEMA_VERSION,))
        db.executemany("INSERT INTO unigram VALUES (?, ?, ?)", ((w, total[w], lower[w]) for w in sorted(vocabulary)))
        kept_bigrams = list(_top_per_context(bigrams, min_bigram, per_context))
        db.executemany("INSERT INTO bigram VALUES (?, ?, ?)", kept_bigrams)
        kept_trigrams = list(_top_per_context(trigrams, min_trigram, per_context))
        db.executemany("INSERT INTO trigram VALUES (?, ?, ?)", kept_trigrams)
        db.executemany(
            "INSERT INTO context2 VALUES (?, ?)",
            ((ctx, bigrams[ctx]) for ctx in {ctx for ctx, _, _ in kept_trigrams} if ctx in bigrams),
        )
        db.commit()
        db.execute("VACUUM")
    finally:
        db.close()
    tmp.replace(out)
    return BuildReport(
        tokens=tokens_seen,
        vocabulary=len(vocabulary),
        bigrams=len(kept_bigrams),
        trigrams=len(kept_trigrams),
        size_bytes=out.stat().st_size,
    )


class NgramTable:
    """Read-only view of a file made by `build_ngrams`."""

    def __init__(self, path: Path) -> None:
        if not path.exists():
            raise FileNotFoundError(path)
        try:
            self._db = sqlite3.connect(f"{path.resolve().as_uri()}?mode=ro", uri=True, check_same_thread=False)
            version = self._db.execute("SELECT value FROM meta WHERE key = 'version'").fetchone()
        except sqlite3.DatabaseError as err:
            raise ValueError(f"{path} is not an n-gram file: {err}") from err
        if version is None or version[0] != _SCHEMA_VERSION:
            raise ValueError(f"{path} is not an n-gram file of this version")
        self._cached = lru_cache(maxsize=8192)(self._counts)

    def counts(self, context: tuple[str, ...], prefix: str) -> Counts:
        if not 1 <= len(context) <= 2:
            return Counts({}, 0)
        return self._cached(context, prefix)

    def _counts(self, context: tuple[str, ...], prefix: str) -> Counts:
        high = prefix + _AFTER_LAST_KEY
        if len(context) == 1:
            row = self._db.execute("SELECT total FROM unigram WHERE word = ?", context).fetchone()
            rows = self._db.execute(
                "SELECT w2, n FROM bigram WHERE w1 = ? AND w2 >= ? AND w2 < ? ORDER BY n DESC LIMIT ?",
                (context[0], prefix, high, _MAX_RESULTS),
            ).fetchall()
        else:
            key = " ".join(context)
            row = self._db.execute("SELECT total FROM context2 WHERE ctx = ?", (key,)).fetchone()
            rows = self._db.execute(
                "SELECT w3, n FROM trigram WHERE ctx = ? AND w3 >= ? AND w3 < ? ORDER BY n DESC LIMIT ?",
                (key, prefix, high, _MAX_RESULTS),
            ).fetchall()
        return Counts(dict(rows), row[0] if row else 0)

    def unigram_stats(self) -> dict[str, UnigramStat]:
        return {w: UnigramStat(t, lo) for w, t, lo in self._db.execute("SELECT word, total, lower FROM unigram")}

    def close(self) -> None:
        self._db.close()

    def __enter__(self) -> "NgramTable":
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()
