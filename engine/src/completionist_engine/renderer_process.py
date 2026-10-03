"""Supervise the optional native renderer without affecting engine fallback."""

from __future__ import annotations

import ctypes
import logging
import os
import struct
import subprocess
import time
from collections import deque
from collections.abc import Callable
from pathlib import Path
from typing import Protocol

logger = logging.getLogger("completionist_engine.renderer")

_FAILURE_WINDOW_SECONDS = 60.0
_MAX_FAILURES = 3
_POLL_SECONDS = 10.0
_STOP_WAIT_SECONDS = 1.0
_RENDERER_NAME = "CompletionistRenderer.exe"


class ChildProcess(Protocol):
    def poll(self) -> int | None: ...

    def terminate(self) -> None: ...

    def kill(self) -> None: ...

    def wait(self, timeout: float | None = None) -> int: ...


Launcher = Callable[[Path], ChildProcess]
SessionProbe = Callable[[], bool | None]


def renderer_executable() -> Path:
    """Return the renderer at the fixed paired-artifact location for this checkout/install."""
    repository = Path(__file__).resolve().parents[3]
    return repository / "tip" / "out" / _RENDERER_NAME


def _launch_hidden(renderer_path: Path) -> ChildProcess:
    return subprocess.Popen(
        [str(renderer_path), "--serve"],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        creationflags=subprocess.CREATE_NO_WINDOW,
    )


def _renderer_pipe_name(session_id: int, authentication_id: int) -> str:
    return rf"\\.\pipe\completionist-renderer-v2-{session_id}-{authentication_id}"


def _logon_pipe_name() -> str | None:
    """Match the renderer's existing per-logon pipe ownership convention."""
    if os.name != "nt":
        return None
    advapi32 = ctypes.WinDLL("advapi32", use_last_error=True)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.GetCurrentProcess.restype = ctypes.c_void_p
    kernel32.CloseHandle.argtypes = (ctypes.c_void_p,)
    kernel32.CloseHandle.restype = ctypes.c_int
    advapi32.OpenProcessToken.argtypes = (ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_void_p))
    advapi32.OpenProcessToken.restype = ctypes.c_int
    advapi32.GetTokenInformation.argtypes = (
        ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)
    )
    advapi32.GetTokenInformation.restype = ctypes.c_int
    token = ctypes.c_void_p()
    process = kernel32.GetCurrentProcess()
    if not advapi32.OpenProcessToken(process, 0x0008, ctypes.byref(token)):  # TOKEN_QUERY
        return None
    try:
        needed = ctypes.c_uint32()
        advapi32.GetTokenInformation(token, 10, None, 0, ctypes.byref(needed))  # TokenStatistics
        if not needed.value:
            return None
        buffer = ctypes.create_string_buffer(needed.value)
        if not advapi32.GetTokenInformation(token, 10, buffer, needed, ctypes.byref(needed)):
            return None
        # TOKEN_STATISTICS starts with TokenId then AuthenticationId, both LUIDs.
        authentication_id = struct.unpack_from("<Q", buffer.raw, 8)[0]
        session = ctypes.c_uint32()
        session_size = ctypes.c_uint32()
        if not advapi32.GetTokenInformation(token, 12, ctypes.byref(session), 4, ctypes.byref(session_size)):
            return None
        return _renderer_pipe_name(session.value, authentication_id)
    finally:
        kernel32.CloseHandle(token)


def _renderer_session_is_running() -> bool | None:
    """Check the renderer pipe without connecting, which would revoke its active lease."""
    pipe = _logon_pipe_name()
    if pipe is None:
        return None
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.WaitNamedPipeW.argtypes = (ctypes.c_wchar_p, ctypes.c_uint32)
    kernel32.WaitNamedPipeW.restype = ctypes.c_int
    if kernel32.WaitNamedPipeW(pipe, 0):
        return True
    error = ctypes.get_last_error()
    # ERROR_FILE_NOT_FOUND means no server has created this session pipe.
    # ERROR_SEM_TIMEOUT means the named server exists but has no free instance.
    if error == 2:
        return False
    if error == 121:
        return True
    logger.warning("Could not check current-session renderer ownership (Windows error %s)", error)
    return None


class RendererProcess:
    """One optional renderer child, with bounded restart attempts and owned-child cleanup."""

    def __init__(
        self,
        renderer_path: Path | None = None,
        *,
        launcher: Launcher = _launch_hidden,
        clock: Callable[[], float] = time.monotonic,
        session_probe: SessionProbe = _renderer_session_is_running,
    ) -> None:
        self.renderer_path = renderer_path or renderer_executable()
        self._launcher = launcher
        self._clock = clock
        self._session_probe = session_probe
        self._child: ChildProcess | None = None
        self._reused = False
        self._stopped = False
        self._retries_exhausted = False
        self._failures: deque[float] = deque()
        self._last_poll = float("-inf")

    @property
    def enabled(self) -> bool:
        return not self._stopped and not self._retries_exhausted

    def start(self) -> None:
        """Start the fixed-path renderer if enabled and no current-session owner exists."""
        if self._stopped or self._child is not None or self._reused or self._retries_exhausted:
            return
        now = self._clock()
        self._prune_failures(now)
        if self._retries_exhausted:
            return
        if not self.renderer_path.is_file():
            logger.info("Native renderer is unavailable at %s; using host popup", self.renderer_path)
            return
        existing = self._session_probe()
        if existing is None:
            logger.warning("Could not verify renderer session ownership; keeping host popup active")
            return
        if existing:
            self._reused = True
            logger.info("Reusing the existing renderer for this logon session")
            return
        try:
            self._child = self._launcher(self.renderer_path)
            logger.info("Started native renderer")
        except (OSError, subprocess.SubprocessError) as err:
            logger.warning("Could not start native renderer; using host popup: %s", err)
            self._record_failure(now)

    def poll(self, now: float) -> None:
        """Notice child exit and apply bounded restart policy from engine maintenance."""
        if now - self._last_poll < _POLL_SECONDS:
            return
        self._last_poll = now
        self._prune_failures(now)
        if self._child is None:
            if not self._reused and self.enabled:
                self.start()
            return
        exit_code = self._child.poll()
        if exit_code is None:
            return
        self._child = None
        self._record_failure(now)
        logger.warning("Native renderer exited with code %s; host popup remains available", exit_code)
        if self.enabled:
            self.start()
        else:
            logger.warning("Renderer restart limit reached; keeping the host popup for this engine session")

    def stop(self) -> None:
        """Stop only our owned process, with bounded waits before force termination."""
        if self._stopped:
            return
        self._stopped = True
        child, self._child = self._child, None
        if child is None:
            return
        if child.poll() is not None:
            return
        try:
            child.terminate()
            child.wait(timeout=_STOP_WAIT_SECONDS)
        except subprocess.TimeoutExpired:
            child.kill()
            try:
                child.wait(timeout=_STOP_WAIT_SECONDS)
            except subprocess.TimeoutExpired:
                logger.warning("Owned renderer did not exit after bounded termination")
        except OSError as err:
            logger.warning("Could not stop owned renderer: %s", err)

    def _prune_failures(self, now: float) -> None:
        while self._failures and now - self._failures[0] >= _FAILURE_WINDOW_SECONDS:
            self._failures.popleft()

    def _record_failure(self, now: float) -> None:
        self._failures.append(now)
        self._prune_failures(now)
        if len(self._failures) >= _MAX_FAILURES:
            self._retries_exhausted = True
