"""The Windows side of `screen_context`: which window is in front, a picture of it, and Windows' own OCR.

The OCR is the engine built into Windows (`Windows.Media.Ocr`): it runs on this machine, uses no AI model
and sends nothing anywhere. It needs the English language pack, which a normal English Windows has.
"""

import asyncio
import ctypes
import logging
import os
from ctypes import wintypes

from PIL import Image, ImageGrab

from completionist_engine.screen_context import Foreground

logger = logging.getLogger("completionist_engine.screen")

_user32 = ctypes.WinDLL("user32", use_last_error=True)
_kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
_dwmapi = ctypes.WinDLL("dwmapi", use_last_error=True)

_user32.GetForegroundWindow.restype = wintypes.HWND
_user32.IsIconic.argtypes = [wintypes.HWND]
_user32.GetWindowTextLengthW.argtypes = [wintypes.HWND]
_user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
_user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
_user32.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
_kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
_kernel32.OpenProcess.restype = wintypes.HANDLE
_kernel32.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
_kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
_dwmapi.DwmGetWindowAttribute.argtypes = [wintypes.HWND, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD]

_PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
_DWMWA_EXTENDED_FRAME_BOUNDS = 9  # the visible window, without its invisible resize border and shadow
_PER_MONITOR_AWARE_V2 = -4
_MIN_SIDE = 100  # pixels; anything smaller is a tooltip or a sliver, not a window to read


class WindowsSource:
    def __init__(self) -> None:
        # Without this, the sizes Windows reports are scaled on a high-DPI screen and the picture is cropped wrong.
        # It fails harmlessly if the process has already chosen.
        _user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(_PER_MONITOR_AWARE_V2))

    def foreground(self) -> Foreground | None:
        hwnd = _user32.GetForegroundWindow()
        if not hwnd or _user32.IsIconic(hwnd):
            return None
        pid = wintypes.DWORD()
        _user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if pid.value == os.getpid():
            return None  # Completionist's own windows are never read
        app = _process_name(pid.value)
        if not app:
            return None  # a program running as administrator can't be inspected, so it isn't read
        return Foreground(hwnd=int(hwnd), title=_title(hwnd), app=app)

    def grab(self, hwnd: int) -> Image.Image | None:
        rect = wintypes.RECT()
        if _dwmapi.DwmGetWindowAttribute(hwnd, _DWMWA_EXTENDED_FRAME_BOUNDS, ctypes.byref(rect), ctypes.sizeof(rect)) != 0:
            if not _user32.GetWindowRect(hwnd, ctypes.byref(rect)):
                return None
        if rect.right - rect.left < _MIN_SIDE or rect.bottom - rect.top < _MIN_SIDE:
            return None
        try:
            return ImageGrab.grab(bbox=(rect.left, rect.top, rect.right, rect.bottom), all_screens=True)
        except OSError:  # the window moved off every screen between the look and the capture
            return None


class WindowsOcr:
    """Reads text from a picture with the OCR built into Windows. Gives back nothing, once logged, if it is missing."""

    def __init__(self) -> None:
        self._engine = None
        self._tried = False

    def read(self, image: Image.Image) -> str:
        engine = self._load()
        if engine is None:
            return ""
        return asyncio.run(self._recognise(engine, image))

    def _load(self):
        if not self._tried:
            self._tried = True
            try:
                from winrt.windows.globalization import Language
                from winrt.windows.media.ocr import OcrEngine

                self._engine = OcrEngine.try_create_from_language(Language("en-US"))
            except Exception as err:
                logger.warning("Windows OCR is not available (%s); screen text is off", type(err).__name__)
            if self._engine is None:
                logger.warning("Windows has no English OCR language installed; screen text is off")
        return self._engine

    @staticmethod
    async def _recognise(engine, image: Image.Image) -> str:
        from winrt.windows.graphics.imaging import BitmapPixelFormat, SoftwareBitmap
        from winrt.windows.media.ocr import OcrEngine
        from winrt.windows.storage.streams import DataWriter

        limit = OcrEngine.max_image_dimension
        if max(image.size) > limit:
            scale = limit / max(image.size)
            image = image.resize((int(image.width * scale), int(image.height * scale)), Image.Resampling.LANCZOS)
        writer = DataWriter()
        writer.write_bytes(image.convert("RGBA").tobytes("raw", "BGRA"))
        bitmap = SoftwareBitmap.create_copy_from_buffer(writer.detach_buffer(), BitmapPixelFormat.BGRA8, image.width, image.height)
        result = await engine.recognize_async(bitmap)
        return "\n".join(line.text for line in result.lines)


def _title(hwnd: int) -> str:
    length = _user32.GetWindowTextLengthW(hwnd)
    buffer = ctypes.create_unicode_buffer(length + 1)
    _user32.GetWindowTextW(hwnd, buffer, length + 1)
    return buffer.value


def _process_name(pid: int) -> str:
    handle = _kernel32.OpenProcess(_PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        return ""
    try:
        size = wintypes.DWORD(1024)
        buffer = ctypes.create_unicode_buffer(size.value)
        if not _kernel32.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
            return ""
        return os.path.basename(buffer.value).lower()
    finally:
        _kernel32.CloseHandle(handle)
