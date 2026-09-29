"""Named-pipe server: one connection per app that has the text service loaded."""

import asyncio
import logging
from asyncio.windows_events import PipeServer

from typer_engine.engine import Engine
from typer_engine.protocol import DEFAULT_PIPE_NAME, FrameDecoder, ProtocolError, encode, parse_request

logger = logging.getLogger("typer_engine.server")


class _Connection(asyncio.Protocol):
    def __init__(self, engine: Engine) -> None:
        self._engine = engine
        self._decoder = FrameDecoder()
        self._transport: asyncio.WriteTransport | None = None

    def connection_made(self, transport: asyncio.BaseTransport) -> None:
        assert isinstance(transport, asyncio.WriteTransport)
        self._transport = transport
        logger.debug("client connected")

    def data_received(self, data: bytes) -> None:
        assert self._transport is not None
        try:
            for message in self._decoder.feed(data):
                reply = self._engine.handle(parse_request(message))
                if reply is not None:
                    self._transport.write(encode(reply.to_message()))
        except ProtocolError as err:
            logger.warning("dropping client that broke the protocol: %s", err)
            self._transport.close()

    def connection_lost(self, exc: Exception | None) -> None:
        logger.debug("client disconnected")


async def start_server(engine: Engine, pipe_name: str = DEFAULT_PIPE_NAME) -> PipeServer:
    loop = asyncio.get_running_loop()
    [server] = await loop.start_serving_pipe(lambda: _Connection(engine), pipe_name)  # type: ignore[attr-defined]
    return server
