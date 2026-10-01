"""What is on the screen next to the text being typed, read in the background so a reply can see what it answers.

A watcher looks at the window in front twice a second. When a window (or a new title in it, as a browser
tab or a chat changes) has stayed in front for two looks, its picture is taken and read with OCR, and the
text and a small JPEG are kept as the *capture* for that window. A phrase request then asks for the capture
and gets it only if it is of the window that is still in front, so text from another app can never leak in.
Reading takes about a tenth of a second and never happens on the typing path.

Nothing here touches the screen or OCR itself: `source` and `reader` are small interfaces (the Windows ones
live in `screen_windows.py`), so the logic is tested with fakes. Captures are held in memory only.
"""

import asyncio
import io
import logging
import time
from collections.abc import Callable
from dataclasses import dataclass
from typing import Protocol

from PIL import Image

logger = logging.getLogger("completionist_engine.screen")

TEXT_CAP = 2000  # characters of screen text kept; the end of the window is nearest the box being typed in
_JPEG_WIDTH = 1280
_JPEG_QUALITY = 60


@dataclass(frozen=True)
class Foreground:
    """The window in front: its handle, its title and the program it belongs to (like `chrome.exe`)."""

    hwnd: int
    title: str
    app: str

    @property
    def key(self) -> tuple[int, str]:
        return (self.hwnd, self.title)


class ScreenSource(Protocol):
    def foreground(self) -> Foreground | None: ...

    def grab(self, hwnd: int) -> Image.Image | None:
        """A picture of that window, or None when it can't be captured (minimised, gone, too small)."""
        ...


class TextReader(Protocol):
    def read(self, image: Image.Image) -> str: ...


@dataclass(frozen=True)
class Capture:
    id: int
    at: float  # on the context's clock; used only to tell how old it is
    app: str
    title: str
    key: tuple[int, str]
    text: str
    jpeg: bytes  # a downscaled picture of what the text was read from, for the viewer


class ScreenContext:
    def __init__(
        self,
        source: ScreenSource,
        reader: TextReader,
        *,
        wanted: Callable[[str], bool],
        clock: Callable[[], float] = time.monotonic,
        cap: int = TEXT_CAP,
        stale: float = 30.0,
        max_age: float = 600.0,
    ) -> None:
        """`wanted(app)` says whether reading is on for the program in front (it is off for blocked apps,
        while paused, and without phrases). A capture older than `stale` seconds is read again in the
        background; one older than `max_age` is not used at all."""
        self._source = source
        self._reader = reader
        self._wanted = wanted
        self._clock = clock
        self._cap = cap
        self._stale = stale
        self._max_age = max_age
        self._seen: tuple[int, str] | None = None
        self._tried: tuple[int, str] | None = None  # the window last read, or last tried and failed
        self._refresh = False
        self._current: Capture | None = None
        self._next_id = 1

    def poll(self) -> None:
        """One look at the window in front; reads it if it has stayed there since the look before."""
        fg = self._source.foreground()
        if fg is None or not self._wanted(fg.app):
            self._seen = None
            return
        if fg.key != self._seen:
            self._seen = fg.key  # first sight: wait a look, so a window still opening isn't read half drawn
            return
        if self._tried == fg.key and not self._refresh:
            return
        self._refresh = False
        self._read(fg)

    def for_request(self) -> Capture | None:
        """The capture of the window in front, if there is a fresh enough one; never one of another window."""
        capture = self._current
        fg = self._source.foreground()
        if capture is None or fg is None or fg.key != capture.key or not self._wanted(fg.app):
            return None
        age = self._clock() - capture.at
        if age > self._max_age:
            return None
        if age > self._stale:
            self._refresh = True  # this request uses the old one; the next look reads the window again
        return capture

    async def run(self, interval: float = 0.5) -> None:
        loop = asyncio.get_running_loop()
        while True:
            await asyncio.sleep(interval)
            try:
                await loop.run_in_executor(None, self.poll)
            except Exception as err:  # a window vanishing mid-read must not end the watcher
                logger.warning("screen watcher: %s", type(err).__name__)

    def _read(self, fg: Foreground) -> None:
        image = self._source.grab(fg.hwnd)
        if image is None:
            return  # nothing to read yet (minimised, or too small); the next look tries again
        self._tried = fg.key
        started = self._clock()
        try:
            text = self._reader.read(image)
        except Exception as err:
            logger.warning("screen text could not be read in %s: %s", fg.app, type(err).__name__)
            return
        text = _tail(text.strip(), self._cap)
        self._current = Capture(self._next_id, self._clock(), fg.app, fg.title, fg.key, text, _jpeg(image))
        self._next_id += 1
        logger.info("screen read in %s: %s (%.0f ms)", fg.app, f"{len(text)} chars" if text else "no text found", (self._clock() - started) * 1000)


def _tail(text: str, cap: int) -> str:
    """The last `cap` characters or fewer, starting at the beginning of a line."""
    if len(text) <= cap:
        return text
    cut = text[-cap:]
    if text[-cap - 1] != "\n" and "\n" in cut:
        cut = cut.split("\n", 1)[1]
    return cut


def _jpeg(image: Image.Image) -> bytes:
    picture = image.convert("RGB")
    if picture.width > _JPEG_WIDTH:
        picture = picture.resize((_JPEG_WIDTH, round(picture.height * _JPEG_WIDTH / picture.width)), Image.Resampling.LANCZOS)
    out = io.BytesIO()
    picture.save(out, "JPEG", quality=_JPEG_QUALITY)
    return out.getvalue()
