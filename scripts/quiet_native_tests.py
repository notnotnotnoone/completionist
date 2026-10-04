"""Guard and run one allowlisted staged native test executable on Windows."""

from __future__ import annotations

import argparse
import ctypes
import os
from pathlib import Path
import subprocess
import sys


SEM_FAILCRITICALERRORS = 0x0001
SEM_NOGPFAULTERRORBOX = 0x0002
SEM_NOOPENFILEERRORBOX = 0x8000
REQUIRED_ERROR_MODE = (
    SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX
)
CREATE_NO_WINDOW = 0x08000000
ALLOWED_TESTS = frozenset(
    {
        "tests.exe",
        "test_popup_layout.exe",
        "test_session.exe",
        "test_capture_policy.exe",
        "test_blur.exe",
    }
)


def set_inherited_error_mode() -> None:
    if os.name != "nt":
        raise RuntimeError("quiet native tests require Windows")

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.GetErrorMode.argtypes = []
    kernel32.GetErrorMode.restype = ctypes.c_uint
    kernel32.SetErrorMode.argtypes = [ctypes.c_uint]
    kernel32.SetErrorMode.restype = ctypes.c_uint

    previous = kernel32.GetErrorMode()
    kernel32.SetErrorMode(previous | REQUIRED_ERROR_MODE)
    actual = kernel32.GetErrorMode()
    if actual & REQUIRED_ERROR_MODE != REQUIRED_ERROR_MODE:
        raise OSError("could not enable inherited Windows error-mode flags")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run one staged native test under a bounded Windows job."
    )
    parser.add_argument("test", nargs="?", choices=sorted(ALLOWED_TESTS))
    parser.add_argument("--self-check", action="store_true")
    parser.add_argument("--timeout-ms", type=int, default=120_000)
    args = parser.parse_args()
    if not 100 <= args.timeout_ms <= 600_000:
        parser.error("--timeout-ms must be between 100 and 600000")
    if args.self_check == (args.test is not None):
        parser.error("choose exactly one allowlisted test or --self-check")
    return args


def main() -> int:
    try:
        set_inherited_error_mode()
    except (OSError, RuntimeError) as error:
        print(f"quiet-native-tests: fail closed: {error}", file=sys.stderr)
        return 2

    args = parse_args()
    root = Path(__file__).resolve().parents[1]
    launcher = root / "scripts" / "out" / "quiet_native_runner.exe"
    if not launcher.is_file():
        print(f"quiet-native-tests: launcher is missing: {launcher}", file=sys.stderr)
        return 2

    command = [str(launcher)]
    if args.self_check:
        command.append("--self-check")
    else:
        command.extend((args.test, str(args.timeout_ms)))

    try:
        completed = subprocess.run(
            command,
            cwd=root,
            check=False,
            creationflags=CREATE_NO_WINDOW,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=args.timeout_ms / 1000 + 30,
        )
        if completed.stdout:
            print(completed.stdout, end="")
        if completed.stderr:
            print(completed.stderr, end="", file=sys.stderr)
    except subprocess.TimeoutExpired:
        print("quiet-native-tests: launcher exceeded its outer safety timeout", file=sys.stderr)
        return 124
    except OSError as error:
        print(f"quiet-native-tests: launcher failed to start: {error}", file=sys.stderr)
        return 2
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
