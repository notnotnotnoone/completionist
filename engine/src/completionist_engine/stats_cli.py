"""`completionist-stats`: what Completionist has saved you, and how the phrase provider is doing."""

import argparse
import sys
from pathlib import Path

from completionist_engine.assemble import METRICS_FILE
from completionist_engine.config import default_data_dir
from completionist_engine.metrics import Metrics, format_summary


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="completionist-stats", description="Show Completionist's usage numbers (counts and timings only).")
    parser.add_argument("--days", type=int, default=7, help="how many days back to cover, today included (default 7)")
    parser.add_argument("--data-dir", type=Path, default=default_data_dir())
    args = parser.parse_args(argv)
    if args.days < 1:
        parser.error("--days must be at least 1")

    path = args.data_dir / METRICS_FILE
    if not path.exists():
        print(f"No stats yet: {path} doesn't exist. Run the engine and type for a while.", file=sys.stderr)
        return 1
    metrics = Metrics(path)
    try:
        print(format_summary(metrics.summary(args.days), args.days))
    finally:
        metrics.close()
    return 0
