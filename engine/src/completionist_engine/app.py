"""`completionist-engine`: load the vocabulary and serve suggestions until stopped."""

import argparse
import asyncio
import logging
import time
from collections.abc import Callable
from pathlib import Path

from completionist_engine.assemble import Assembled, assemble_engine
from completionist_engine.config import Config, ConfigError, default_config_path, load_config, migrate_legacy_dirs
from completionist_engine.config_watch import ConfigWatcher
from completionist_engine.hotkeys import GlobalHotkey
from completionist_engine.logsetup import setup_logging
from completionist_engine.protocol import DEFAULT_PIPE_NAME
from completionist_engine.screen_context import ScreenContext
from completionist_engine.server import start_server
from completionist_engine.viewer import ViewerServer
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


def screen_wanted(engine, phrases) -> Callable[[str], bool]:
    """Whether the window of `app` may be read for screen text right now. It looks at the live settings each time.

    Never without phrases (nothing would be sent), while paused, when switched off, or in an app the person blocked."""

    def wanted(app: str) -> bool:
        return phrases.available and phrases.config.screen_context and not engine.paused and app.lower() not in engine.config.block

    return wanted


async def _serve(assembled: Assembled, args: argparse.Namespace, config: Config) -> None:
    engine = assembled.engine
    loop = asyncio.get_running_loop()
    stop = asyncio.Event()
    tray = None
    hotkey = None
    viewer = None

    def toggle_pause() -> bool:
        engine.paused = not engine.paused
        logger.info("Completionist %s", "paused" if engine.paused else "resumed")
        if tray is not None:
            tray.refresh()
        return engine.paused

    server = await start_server(engine, args.pipe)  # first: if the pipe is taken, nothing else has started
    logger.info("listening on %s", args.pipe)

    screen_task = None
    if assembled.phrases is not None:
        from completionist_engine.screen_windows import WindowsOcr, WindowsSource  # Windows only, so imported here

        screen = ScreenContext(WindowsSource(), WindowsOcr(), wanted=screen_wanted(engine, assembled.phrases))
        assembled.phrases.screen = screen
        screen_task = asyncio.create_task(screen.run())

    def run_on_loop(fn: Callable[[], str]) -> str:
        async def call() -> str:
            return fn()

        return asyncio.run_coroutine_threadsafe(call(), loop).result(timeout=5)

    if not args.no_tray:
        from completionist_engine.tray import Tray  # imported here: it needs a desktop, which the tests don't have

        viewer = ViewerServer(
            assembled.personal, assembled.metrics, args.config, request_log=assembled.phrases.log if assembled.phrases else None
        )
        await viewer.start()
        tray = Tray(
            toggle_pause=toggle_pause,
            is_paused=lambda: engine.paused,
            metrics=assembled.metrics,
            config_path=args.config,
            log_dir=config.data_dir,
            quit_engine=lambda: loop.call_soon_threadsafe(stop.set),
            viewer_url=lambda: viewer.url,
            on_loop=run_on_loop,
        )
        tray.start()
        if config.pause_hotkey:
            hotkey = GlobalHotkey(config.pause_hotkey, toggle_pause)
            hotkey.start()

    watcher = ConfigWatcher(args.config, config, engine.set_config)
    watch_task = asyncio.create_task(watcher.run())
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
        if screen_task is not None:
            screen_task.cancel()
        if hotkey is not None:
            hotkey.stop()
        if tray is not None:
            tray.stop()
        if viewer is not None:
            viewer.close()
        server.close()
        await assembled.aclose()
