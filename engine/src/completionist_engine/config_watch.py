"""Applies edits to the config file while the engine runs."""

import asyncio
import logging
from collections.abc import Callable
from dataclasses import replace
from pathlib import Path

from completionist_engine.config import Config, ConfigError, load_config

logger = logging.getLogger("completionist_engine.config")


class ConfigWatcher:
    def __init__(self, path: Path, current: Config, apply: Callable[[Config], None]) -> None:
        self._path = path
        self._current = current
        self._apply = apply
        self._seen = self._stamp()
        self.last_error: str | None = None
        self.restart_needed = False
        """Set when the file asks for something that only takes effect at startup (a different data folder)."""

    def _stamp(self) -> int | None:
        try:
            return self._path.stat().st_mtime_ns
        except OSError:
            return None

    def check(self) -> bool:
        """Reload if the file changed. True when a new config was applied."""
        stamp = self._stamp()
        if stamp == self._seen:
            return False
        self._seen = stamp
        try:
            config = load_config(self._path)
        except ConfigError as err:
            self.last_error = str(err)
            logger.error("config not reloaded, keeping the current settings: %s", err)
            return False
        self.last_error = None
        if config.data_dir != self._current.data_dir:
            self.restart_needed = True
            logger.warning("[data] dir changed: restart Completionist for it to take effect")
            config = replace(config, data_dir=self._current.data_dir)
        self._current = config
        self._apply(config)
        logger.info("config reloaded from %s", self._path)
        return True

    async def run(self, interval: float = 2.0) -> None:
        while True:
            await asyncio.sleep(interval)
            self.check()
