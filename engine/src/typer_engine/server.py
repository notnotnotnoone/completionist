"""Named-pipe server: one connection per app that has the text service loaded."""

import asyncio
import logging
import time
from asyncio.windows_events import PipeServer

from typer_engine.engine import Engine
from typer_engine.protocol import DEFAULT_PIPE_NAME, FrameDecoder, PhraseUpdate, ProtocolError, encode, parse_request

logger = logging.getLogger("typer_engine.server")

SLOW_REQUEST_SECONDS = 0.025  # answering should take well under this; more is worth a log line


class _Connection(asyncio.Protocol):
    def __init__(self, engine: Engine) -> None:
        self._session = engine.open_session(self._push)
        self._decoder = FrameDecoder()
        self._transport: asyncio.WriteTransport | None = None

    def _push(self, update: PhraseUpdate) -> None:
        """Send a streamed phrase update: something the text service didn't ask for in a reply."""
        if self._transport is not None and not self._transport.is_closing():
            self._transport.write(encode(update.to_message()))

    def connection_made(self, transport: asyncio.BaseTransport) -> None:
        assert isinstance(transport, asyncio.WriteTransport)
        self._transport = transport
        logger.debug("client connected")

    def data_received(self, data: bytes) -> None:
        assert self._transport is not None
        try:
            for message in self._decoder.feed(data):
                request = parse_request(message)
                started = time.perf_counter()
                reply = self._session.handle(request)
                elapsed = time.perf_counter() - started
                if elapsed > SLOW_REQUEST_SECONDS:
                    logger.warning("slow request: %s from %s took %.1f ms", request.event, request.app, elapsed * 1000)
                else:
                    logger.debug("%s from %s took %.2f ms", request.event, request.app, elapsed * 1000)
                if reply is not None:
                    self._transport.write(encode(reply.to_message()))
        except ProtocolError as err:
            logger.warning("dropping client that broke the protocol: %s", err)
            self._transport.close()

    def connection_lost(self, exc: Exception | None) -> None:
        self._session.close()
        logger.debug("client disconnected")


async def start_server(engine: Engine, pipe_name: str = DEFAULT_PIPE_NAME) -> PipeServer:
    loop = asyncio.get_running_loop()
    [server] = await loop.start_serving_pipe(lambda: _Connection(engine), pipe_name)  # type: ignore[attr-defined]
    return server
