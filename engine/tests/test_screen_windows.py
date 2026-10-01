"""Checks against the real Windows pieces. They need Windows with its English OCR, and skip themselves without it."""

import sys

import pytest
from PIL import Image, ImageDraw, ImageFont

pytestmark = pytest.mark.skipif(sys.platform != "win32", reason="Windows only")

TEXT = ["Hi Daniel, thanks for sending over the draft.", "Could you confirm whether the deadline is still Friday?"]


def rendered(size: int = 16, fg=(30, 30, 30), bg=(255, 255, 255)) -> Image.Image:
    try:
        font = ImageFont.truetype("segoeui.ttf", size)
    except OSError:
        pytest.skip("Segoe UI is not installed")
    img = Image.new("RGB", (800, 120), bg)
    draw = ImageDraw.Draw(img)
    for i, line in enumerate(TEXT):
        draw.text((20, 20 + i * size * 2), line, font=font, fill=fg)
    return img


@pytest.fixture(scope="module")
def ocr():
    from completionist_engine.screen_windows import WindowsOcr

    reader = WindowsOcr()
    if reader._load() is None:
        pytest.skip("Windows has no English OCR language")
    return reader


@pytest.mark.parametrize("fg,bg", [((30, 30, 30), (255, 255, 255)), ((225, 225, 225), (30, 30, 32))], ids=["light", "dark"])
def test_ordinary_text_is_read_back(ocr, fg, bg):
    text = ocr.read(rendered(16, fg, bg)).lower()
    for word in ("thanks", "draft", "confirm", "deadline", "friday"):
        assert word in text


def test_a_blank_picture_gives_no_text(ocr):
    assert ocr.read(Image.new("RGB", (400, 200), "white")).strip() == ""


def test_the_window_in_front_is_named_and_can_be_captured():
    from completionist_engine.screen_windows import WindowsSource

    source = WindowsSource()
    fg = source.foreground()
    if fg is None:
        pytest.skip("no inspectable window is in front")
    assert fg.hwnd > 0 and fg.app.endswith(".exe")
    image = source.grab(fg.hwnd)
    assert image is None or (image.width >= 100 and image.height >= 100)


def test_reading_works_from_a_worker_thread_the_way_the_engine_calls_it(ocr):
    import asyncio

    from completionist_engine.screen_context import Foreground, ScreenContext

    class Source:
        def foreground(self):
            return Foreground(1, "Mail", "chrome.exe")

        def grab(self, hwnd):
            return rendered(16)

    async def scenario():
        screen = ScreenContext(Source(), ocr, wanted=lambda app: True)
        loop = asyncio.get_running_loop()
        await loop.run_in_executor(None, screen.poll)
        await loop.run_in_executor(None, screen.poll)
        return screen.for_request()

    shot = asyncio.run(scenario())
    assert shot is not None and "deadline" in shot.text.lower()
