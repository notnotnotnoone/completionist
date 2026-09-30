"""`completionist-engine`: load the vocabulary and serve suggestions until stopped."""

import argparse
import asyncio
import logging
import time
from pathlib import Path

from completionist_engine.assemble import Assembled, assemble_engine
from completionist_engine.config import Config, ConfigError, default_config_path, load_config, migrate_legacy_dirs
from completionist_engine.config_watch import ConfigWatcher
from completionist_engine.hotkeys import GlobalHotkey
from completionist_engine.logsetup import setup_logging
from completionist_engine.protocol import DEFAULT_PIPE_NAME
from completionist_engine.server import start_server
from completionist_engine.vocabulary import load_wordfreq_vocabulary

logger = logging.getLogger("completionist_engine")

_FLUSH_SECONDS = 10


def main(argv: list[str] | None = None) -> int:
    if argv is None:  # a real launch, not a test: bring over the folders from the app's old name (Typer)
        migrate_legacy_dirs()
    parser = argparse.ArgumentParser(prog="completionist-engine", description="Serve Completionist suggestions over a named pipe.")
    parser.add_argument("--pipe", default=DEFAULT_PIPE_NAME)
    parser.add_argument("--config", type=Path, default=default_config_path())
    parser.add_argument("--log-level", default="INFO", choices=["DEBUG", "INFO", "WARNING", "ERROR"])
    parser.add_argument("--no-tray", action="store_true", help="don't show the tray icon or register the pause hotkey")
    args = parser.parse_args(argv)

    config_error = None
    try:
        config = load_config(args.config)
    except ConfigError as err:
        # A typo in the settings file must not stop Completionist starting: run on defaults, and say why.
        config, config_error = Config(), str(err)
    setup_logging(args.log_level, config.data_dir)
    if config_error:
        logger.error("%s; running on default settings until the file is fixed", config_error)

    started = time.perf_counter()
    assembled = assemble_engine(config, load_wordfreq_vocabulary())
    logger.info("engine ready in %.2fs", time.perf_counter() - started)

    try:
        asyncio.run(_serve(assembled, args, config))
    except KeyboardInterrupt:
        logger.info("stopped")
    finally:
        assembled.close()  # saves what was learned and counted
    return 0


async def _serve(assembled: Assembled, args: argparse.Namespace, config: Config) -> None:
    engine = assembled.engine
    loop = asyncio.get_running_loop()
    stop = asyncio.Event()
    tray = None
    hotkey = None

    def toggle_pause() -> bool:
        engine.paused = not engine.paused
        logger.info("Completionist %s", "paused" if engine.paused else "resumed")
        if tray is not None:
            tray.refresh()
        return engine.paused

    if not args.no_tray:
        from completionist_engine.tray import Tray  # imported here: it needs a desktop, which the tests don't have

        tray = Tray(
            toggle_pause=toggle_pause,
            is_paused=lambda: engine.paused,
            metrics=assembled.metrics,
            config_path=args.config,
            log_dir=config.data_dir,
            quit_engine=lambda: loop.call_soon_threadsafe(stop.set),
        )
        tray.start()
        if config.pause_hotkey:
            hotkey = GlobalHotkey(config.pause_hotkey, toggle_pause)
            hotkey.start()

    watcher = ConfigWatcher(args.config, config, engine.set_config)
    watch_task = asyncio.create_task(watcher.run())
    server = await start_server(engine, args.pipe)
    logger.info("listening on %s", args.pipe)

    async def flush_regularly() -> None:
        while True:
            await asyncio.sleep(_FLUSH_SECONDS)
            assembled.flush()

    flush_task = asyncio.create_task(flush_regularly())
    try:
        await stop.wait()
        logger.info("quit requested")
    finally:
        watch_task.cancel()
        flush_task.cancel()
        if hotkey is not None:
            hotkey.stop()
        if tray is not None:
            tray.stop()
        server.close()
        await assembled.aclose()
