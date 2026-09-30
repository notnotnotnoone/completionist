"""Async client for the engine's named pipe (used by the probe tool and tests)."""

import asyncio
import contextlib
import struct
from typing import Any

from completionist_engine.protocol import DEFAULT_PIPE_NAME, FrameDecoder, encode

_HEADER = struct.Struct("<I")


class EngineClient:
    def __init__(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        self._reader = reader
        self._writer = writer

    @classmethod
    async def connect(cls, pipe_name: str = DEFAULT_PIPE_NAME, timeout: float = 2.0) -> "EngineClient":
        """Connect, retrying until `timeout` while the engine is still starting up."""
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout
        while True:
            reader = asyncio.StreamReader()
            protocol = asyncio.StreamReaderProtocol(reader)
            try:
                transport, _ = await loop.create_pipe_connection(lambda: protocol, pipe_name)  # type: ignore[attr-defined]
                break
            except FileNotFoundError:
                if loop.time() >= deadline:
                    raise
                await asyncio.sleep(0.05)
        return cls(reader, asyncio.StreamWriter(transport, protocol, reader, loop))

    async def send(self, message: dict[str, Any]) -> None:
        await self.send_raw(encode(message))

    async def send_raw(self, data: bytes) -> None:
        self._writer.write(data)
        await self._writer.drain()

    async def receive(self, timeout: float = 2.0) -> dict[str, Any]:
        header = await asyncio.wait_for(self._reader.readexactly(_HEADER.size), timeout)
        (length,) = _HEADER.unpack(header)
        body = await asyncio.wait_for(self._reader.readexactly(length), timeout)
        [message] = FrameDecoder().feed(header + body)
        return message

    async def request(self, message: dict[str, Any], timeout: float = 2.0) -> dict[str, Any]:
        await self.send(message)
        return await self.receive(timeout)

    async def at_eof(self, timeout: float) -> bool:
        """Whether the engine closes the connection within `timeout` seconds."""
        try:
            await asyncio.wait_for(self._reader.read(), timeout)
        except TimeoutError:
            return False
        return True

    async def close(self) -> None:
        self._writer.close()
        with contextlib.suppress(OSError):
            await self._writer.wait_closed()
