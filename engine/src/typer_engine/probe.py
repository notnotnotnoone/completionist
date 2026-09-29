"""`typer-probe`: ask a running engine for suggestions, without the text service."""

import argparse
import asyncio
import sys
from typing import Any

from typer_engine.client import EngineClient
from typer_engine.protocol import DEFAULT_PIPE_NAME


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="typer-probe", description="Ask a running Typer engine for suggestions.")
    parser.add_argument("before", help="the text before the caret")
    parser.add_argument("--after", default="", help="the text after the caret")
    parser.add_argument("--app", default="probe.exe", help="process name to pretend the text comes from")
    parser.add_argument("--pipe", default=DEFAULT_PIPE_NAME)
    parser.add_argument("--timeout", type=float, default=2.0, help="seconds to wait for the engine")
    args = parser.parse_args(argv)

    try:
        reply = asyncio.run(_ask(args))
    except (OSError, TimeoutError, asyncio.IncompleteReadError) as err:
        print(f"typer-probe: can't reach the engine on {args.pipe}: {err!r}", file=sys.stderr)
        return 1
    for word in reply["words"]:
        print(word)
    return 0


async def _ask(args: argparse.Namespace) -> dict[str, Any]:
    client = await EngineClient.connect(args.pipe, timeout=args.timeout)
    try:
        request = {"id": 1, "event": "keystroke", "app": args.app, "before": args.before, "after": args.after}
        return await client.request(request, timeout=args.timeout)
    finally:
        await client.close()
