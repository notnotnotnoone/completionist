"""The tray icon: pause and resume Completionist, see what it's saved, open the config, quit."""

import logging
import os
from collections.abc import Callable
from pathlib import Path

import pystray
from PIL import Image, ImageDraw

from completionist_engine.metrics import Metrics, Summary

logger = logging.getLogger("completionist_engine.tray")

_BLUE = (44, 71, 201, 255)
_GREY = (110, 116, 130, 255)
_WHITE = (255, 255, 255, 255)

CONFIG_TEMPLATE = """# Completionist settings. Everything here is optional; changes apply within a couple of seconds.
# The full list of settings is in engine/README.md.

[apps]
# block = ["code.exe"]          # apps where Completionist stays silent
# allow = ["notepad.exe"]       # apps where phrases appear on their own (Ctrl+Space works everywhere else)

[words]
# limit = 5

[phrase]
# enabled = true
# daily_budget_usd = 0.50       # phrases stop for the day once this much is spent
# The API key is never stored here: set the environment variable named by api_key_env
# (default DEEPSEEK_API_KEY) with:  setx DEEPSEEK_API_KEY "your key"

[hotkeys]
# pause = "ctrl+alt+p"
"""


def icon_image(paused: bool = False, size: int = 64) -> Image.Image:
    """A rounded square with a T and a caret bar: blue when Completionist is on, grey when paused."""
    scale = 4  # draw big, then shrink, for smooth edges
    big = size * scale
    image = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((0, 0, big - 1, big - 1), radius=int(big * 0.22), fill=_GREY if paused else _BLUE)
    draw.rectangle((big * 0.24, big * 0.24, big * 0.76, big * 0.38), fill=_WHITE)  # top of the T
    draw.rectangle((big * 0.43, big * 0.24, big * 0.57, big * 0.80), fill=_WHITE)  # stem
    draw.rectangle((big * 0.69, big * 0.50, big * 0.75, big * 0.80), fill=_WHITE)  # caret
    return image.resize((size, size), Image.LANCZOS)


def pause_label(paused: bool) -> str:
    return "Resume Completionist" if paused else "Pause Completionist"


def stats_line(summary: Summary, days: int = 7) -> str:
    """One line for a notification: what Completionist saved recently."""
    if not (summary.shown_words or summary.accepted_words or summary.accepted_phrases):
        return "Nothing recorded yet."
    return (
        f"Last {days} days: {summary.keystrokes_saved:,} keystrokes saved, "
        f"{summary.word_acceptance_rate:.0%} of word suggestions taken."
    )


class Tray:
    """Runs the icon on its own thread. `toggle_pause` flips the engine's paused state and returns it."""

    def __init__(
        self,
        toggle_pause: Callable[[], bool],
        is_paused: Callable[[], bool],
        metrics: Metrics | None,
        config_path: Path,
        log_dir: Path,
        quit_engine: Callable[[], None],
    ) -> None:
        self._toggle = toggle_pause
        self._is_paused = is_paused
        self._metrics = metrics
        self._config_path = config_path
        self._log_dir = log_dir
        self._quit = quit_engine
        self._icon = pystray.Icon(
            "completionist",
            icon_image(is_paused()),
            "Completionist",
            menu=pystray.Menu(
                pystray.MenuItem(lambda _item: pause_label(self._is_paused()), self._on_toggle, default=True),
                pystray.MenuItem("Show stats", self._on_stats),
                pystray.Menu.SEPARATOR,
                pystray.MenuItem("Open settings file", self._on_config),
                pystray.MenuItem("Open log folder", self._on_logs),
                pystray.Menu.SEPARATOR,
                pystray.MenuItem("Quit Completionist", self._on_quit),
            ),
        )

    def start(self) -> None:
        self._icon.run_detached()

    def stop(self) -> None:
        self._icon.stop()

    def refresh(self) -> None:
        """Redraw after the paused state changed (from the hotkey, say)."""
        paused = self._is_paused()
        self._icon.icon = icon_image(paused)
        self._icon.title = "Completionist (paused)" if paused else "Completionist"
        self._icon.update_menu()

    def _on_toggle(self, _icon=None, _item=None) -> None:
        self._toggle()
        self.refresh()

    def _on_stats(self, _icon=None, _item=None) -> None:
        if self._metrics is None:
            self._icon.notify("Stats are off.", "Completionist")
            return
        self._metrics.flush()
        self._icon.notify(stats_line(self._metrics.summary(7)), "Completionist stats")

    def _on_config(self, _icon=None, _item=None) -> None:
        try:
            if not self._config_path.exists():
                self._config_path.parent.mkdir(parents=True, exist_ok=True)
                self._config_path.write_text(CONFIG_TEMPLATE, encoding="utf-8")
            os.startfile(self._config_path)  # type: ignore[attr-defined]
        except OSError as err:
            logger.warning("could not open %s: %s", self._config_path, err)

    def _on_logs(self, _icon=None, _item=None) -> None:
        try:
            os.startfile(self._log_dir)  # type: ignore[attr-defined]
        except OSError as err:
            logger.warning("could not open %s: %s", self._log_dir, err)

    def _on_quit(self, _icon=None, _item=None) -> None:
        self._quit()
