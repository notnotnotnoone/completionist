"""`typer-engine`: load the vocabulary and serve suggestions until stopped."""

import argparse
import asyncio
import logging
import time
from pathlib import Path

from typer_engine.config import ConfigError, default_config_path, load_config
from typer_engine.engine import Engine
from typer_engine.protocol import DEFAULT_PIPE_NAME
from typer_engine.server import start_server
from typer_engine.vocabulary import load_wordfreq_vocabulary
from typer_engine.words import WordCompleter

logger = logging.getLogger("typer_engine")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="typer-engine", description="Serve Typer suggestions over a named pipe.")
    parser.add_argument("--pipe", default=DEFAULT_PIPE_NAME)
    parser.add_argument("--config", type=Path, default=default_config_path())
    parser.add_argument("--log-level", default="INFO", choices=["DEBUG", "INFO", "WARNING", "ERROR"])
    args = parser.parse_args(argv)
    logging.basicConfig(level=args.log_level, format="%(asctime)s %(levelname)s %(name)s: %(message)s")

    try:
        config = load_config(args.config)
    except ConfigError as err:
        logger.error("%s", err)
        return 1

    started = time.perf_counter()
    completer = WordCompleter(load_wordfreq_vocabulary())
    logger.info("vocabulary ready in %.2fs", time.perf_counter() - started)

    try:
        asyncio.run(_serve(Engine(completer, config), args.pipe))
    except KeyboardInterrupt:
        logger.info("stopped")
    return 0


async def _serve(engine: Engine, pipe_name: str) -> None:
    server = await start_server(engine, pipe_name)
    logger.info("listening on %s", pipe_name)
    try:
        await asyncio.Event().wait()
    finally:
        server.close()
