"""Logging for the engine: the console, plus a rotating file in the data folder (`engine.log`)."""

import logging
import logging.handlers
from pathlib import Path

_FORMAT = "%(asctime)s %(levelname)s %(name)s: %(message)s"


def setup_logging(level: str, log_dir: Path, max_bytes: int = 1_000_000, backups: int = 3) -> None:
    """Send logs to the console and to `log_dir/engine.log`. Safe to call more than once."""
    root = logging.getLogger()
    for handler in list(root.handlers):
        if getattr(handler, "_completionist", False):
            handler.close()
            root.removeHandler(handler)
    root.setLevel(level)

    console = logging.StreamHandler()
    console.setFormatter(logging.Formatter(_FORMAT))
    console._completionist = True  # type: ignore[attr-defined]
    root.addHandler(console)
    try:
        log_dir.mkdir(parents=True, exist_ok=True)
        file = logging.handlers.RotatingFileHandler(log_dir / "engine.log", maxBytes=max_bytes, backupCount=backups, encoding="utf-8")
    except OSError as err:
        logging.getLogger("completionist_engine").warning("can't write a log file in %s (%s); logging to the console only", log_dir, err)
        return
    file.setFormatter(logging.Formatter(_FORMAT))
    file._completionist = True  # type: ignore[attr-defined]
    root.addHandler(file)
