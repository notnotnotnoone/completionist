"""`typer-build-ngrams`: make the n-gram file the engine uses from plain-text files."""

import argparse
from pathlib import Path

from typer_engine.assemble import NGRAMS_FILE
from typer_engine.config import default_data_dir
from typer_engine.ngrams import build_ngrams


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="typer-build-ngrams", description="Build Typer's n-gram tables from text files.")
    parser.add_argument("sources", nargs="+", type=Path, help="text files (.txt or .gz) or folders of them")
    parser.add_argument("--out", type=Path, default=default_data_dir() / NGRAMS_FILE)
    parser.add_argument("--vocab-size", type=int, default=100_000, help="most common words to keep")
    parser.add_argument("--min-bigram", type=int, default=3, help="drop bigrams seen fewer times")
    parser.add_argument("--min-trigram", type=int, default=3, help="drop trigrams seen fewer times")
    parser.add_argument("--per-context", type=int, default=40, help="most frequent continuations kept per context")
    args = parser.parse_args(argv)
    missing = [str(p) for p in args.sources if not p.exists()]
    if missing:
        parser.error(f"not found: {', '.join(missing)}")

    report = build_ngrams(
        args.sources,
        args.out,
        vocab_size=args.vocab_size,
        min_bigram=args.min_bigram,
        min_trigram=args.min_trigram,
        per_context=args.per_context,
        progress=lambda message: print(message, flush=True),
    )
    print(
        f"wrote {args.out}: {report.tokens:,} words counted, {report.vocabulary:,} vocabulary, "
        f"{report.bigrams:,} bigrams, {report.trigrams:,} trigrams, {report.size_bytes / 1e6:.1f} MB"
    )
    return 0
