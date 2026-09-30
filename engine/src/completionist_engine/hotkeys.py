"""A system-wide hotkey (used to pause and resume Completionist), via the Win32 RegisterHotKey call."""

import ctypes
import logging
import threading
from collections.abc import Callable
from ctypes import wintypes

logger = logging.getLogger("completionist_engine.hotkeys")

MOD_ALT = 0x1
MOD_CONTROL = 0x2
MOD_SHIFT = 0x4
MOD_WIN = 0x8
MOD_NOREPEAT = 0x4000
_WM_HOTKEY = 0x0312
_WM_QUIT = 0x0012

_MODIFIERS = {"ctrl": MOD_CONTROL, "control": MOD_CONTROL, "alt": MOD_ALT, "shift": MOD_SHIFT, "win": MOD_WIN}
_NAMED = {"space": 0x20, "pause": 0x13, "insert": 0x2D, "delete": 0x2E, "home": 0x24, "end": 0x23, "pageup": 0x21, "pagedown": 0x22}


def parse_hotkey(text: str) -> tuple[int, int]:
    """"ctrl+alt+p" -> (modifier flags, virtual-key code). A modifier is required."""
    parts = [p.strip().lower() for p in text.split("+")]
    if not text.strip() or any(not p for p in parts):
        raise ValueError(f"not a hotkey: {text!r}")
    *mods, key = parts
    flags = 0
    for mod in mods:
        if mod not in _MODIFIERS:
            raise ValueError(f"unknown modifier {mod!r} in {text!r}")
        if flags & _MODIFIERS[mod]:
            raise ValueError(f"modifier {mod!r} repeated in {text!r}")
        flags |= _MODIFIERS[mod]
    if not flags:
        raise ValueError(f"{text!r} needs a modifier (ctrl, alt, shift or win) so it can't swallow normal typing")
    if len(key) == 1 and key.isalnum():
        vk = ord(key.upper())
    elif key in _NAMED:
        vk = _NAMED[key]
    elif key[0] == "f" and key[1:].isdigit() and 1 <= int(key[1:]) <= 24:
        vk = 0x70 + int(key[1:]) - 1
    else:
        raise ValueError(f"unknown key {key!r} in {text!r}")
    return flags | MOD_NOREPEAT, vk


class GlobalHotkey:
    def __init__(self, text: str, callback: Callable[[], None]) -> None:
        self._flags, self._vk = parse_hotkey(text)
        self._text = text
        self._callback = callback
        self._thread: threading.Thread | None = None
        self._thread_id = 0
        self._ready = threading.Event()
        self._registered = False

    def start(self) -> bool:
        """Register the hotkey. False if another program already owns it."""
        if self._thread is not None:
            return self._registered
        self._thread = threading.Thread(target=self._run, name="completionist-hotkey", daemon=True)
        self._thread.start()
        self._ready.wait(timeout=3)
        if not self._registered:
            logger.warning("hotkey %s is taken by another program; pause it from the tray menu instead", self._text)
        return self._registered

    def _run(self) -> None:
        user32 = ctypes.windll.user32
        kernel32 = ctypes.windll.kernel32
        self._thread_id = kernel32.GetCurrentThreadId()
        self._registered = bool(user32.RegisterHotKey(None, 1, self._flags, self._vk))
        self._ready.set()
        if not self._registered:
            return
        message = wintypes.MSG()
        while user32.GetMessageW(ctypes.byref(message), None, 0, 0) > 0:
            if message.message == _WM_HOTKEY:
                try:
                    self._callback()
                except Exception:  # a broken callback must not kill the hotkey thread
                    logger.exception("hotkey callback failed")
        user32.UnregisterHotKey(None, 1)

    def stop(self) -> None:
        if self._thread is None:
            return
        if self._registered:
            ctypes.windll.user32.PostThreadMessageW(self._thread_id, _WM_QUIT, 0, 0)
        self._thread.join(timeout=2)
        self._thread = None
        self._registered = False
