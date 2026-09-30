"""A stand-in phrase provider for tsf_e2e.ps1: prints its URL, then streams "ld", " is", " big" to every request.

    python fake_provider_server.py
"""

import asyncio
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "engine"))

from tests.fake_provider import Script, fake_provider  # noqa: E402


async def main() -> None:
    async with fake_provider(Script(chunks=["ld", " is", " big"], delay=0.05)) as (url, _received):
        print(url, flush=True)
        await asyncio.Event().wait()


asyncio.run(main())
