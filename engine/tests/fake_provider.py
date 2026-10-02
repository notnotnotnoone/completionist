"""A tiny local HTTP server that speaks enough of a chat-completions stream for tests."""

import asyncio
import contextlib
import json
from collections.abc import Callable
from dataclasses import dataclass, field


@dataclass
class Received:
    path: str
    headers: dict[str, str]
    body: dict


@dataclass
class Script:
    """What the fake server does for a request."""

    chunks: list[str] = field(default_factory=lambda: ["hello", " world"])
    usage: dict | None = field(
        default_factory=lambda: {"prompt_cache_hit_tokens": 80, "prompt_cache_miss_tokens": 20, "completion_tokens": 2}
    )
    status: int = 200
    delay: float = 0.0  # seconds before each chunk
    hang: bool = False  # never answer
    error_body: str = '{"error": "boom"}'
    finish_after: int | None = None  # drop the connection after this many chunks


def sse(data: dict | str) -> bytes:
    payload = data if isinstance(data, str) else json.dumps(data)
    return f"data: {payload}\n\n".encode()


@contextlib.asynccontextmanager
async def fake_provider(script: Script | Callable[[Received], Script] | None = None):
    received: list[Received] = []
    handlers: set[asyncio.Task] = set()
    default = script if script is not None else Script()

    async def handle(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        handlers.add(asyncio.current_task())
        try:
            head = await reader.readuntil(b"\r\n\r\n")
            lines = head.decode().split("\r\n")
            path = lines[0].split(" ")[1]
            headers = {k.lower(): v for k, v in (line.split(": ", 1) for line in lines[1:] if ": " in line)}
            body = json.loads(await reader.readexactly(int(headers.get("content-length", "0"))) or b"{}")
            request = Received(path, headers, body)
            received.append(request)
            current = default(request) if callable(default) else default
            if current.hang:
                await asyncio.sleep(30)
                return
            if current.status != 200:
                data = current.error_body.encode()
                writer.write(
                    f"HTTP/1.1 {current.status} X\r\nContent-Type: application/json\r\n"
                    f"Content-Length: {len(data)}\r\nConnection: close\r\n\r\n".encode()
                    + data
                )
                await writer.drain()
                return
            writer.write(b"HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n")
            for i, text in enumerate(current.chunks):
                if current.finish_after is not None and i >= current.finish_after:
                    return
                if current.delay:
                    await asyncio.sleep(current.delay)
                writer.write(sse({"choices": [{"index": 0, "delta": {"content": text}, "finish_reason": None}]}))
                await writer.drain()
            writer.write(sse({"choices": [{"index": 0, "delta": {}, "finish_reason": "stop"}]}))
            if current.usage is not None:
                writer.write(sse({"choices": [], "usage": current.usage}))
            writer.write(sse("[DONE]"))
            await writer.drain()
        except (asyncio.IncompleteReadError, ConnectionError, asyncio.CancelledError):
            pass
        finally:
            writer.close()

    server = await asyncio.start_server(handle, "127.0.0.1", 0)
    port = server.sockets[0].getsockname()[1]
    try:
        yield f"http://127.0.0.1:{port}", received
    finally:
        server.close()
        for task in handlers:
            task.cancel()
        await server.wait_closed()
