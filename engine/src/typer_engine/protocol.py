"""Wire protocol between the TSF text service and the engine.

Each frame is a 4-byte little-endian length followed by that many bytes of UTF-8 JSON (an object).
"""

import json
import struct
from dataclasses import dataclass
from typing import Any, Literal, get_args

DEFAULT_PIPE_NAME = r"\\.\pipe\typer-engine"
MAX_FRAME_BYTES = 1 << 20
_HEADER = struct.Struct("<I")

EventType = Literal["keystroke", "hotkey", "accept", "dismiss"]
_EVENT_TYPES = frozenset(get_args(EventType))


class ProtocolError(Exception):
    """A frame or message that doesn't follow the protocol."""


def encode(message: dict[str, Any]) -> bytes:
    body = json.dumps(message, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if len(body) > MAX_FRAME_BYTES:
        raise ProtocolError(f"message of {len(body)} bytes exceeds the {MAX_FRAME_BYTES}-byte limit")
    return _HEADER.pack(len(body)) + body


class FrameDecoder:
    """Accepts arbitrary byte chunks and yields each complete message once it has fully arrived."""

    def __init__(self) -> None:
        self._buffer = bytearray()

    def feed(self, data: bytes) -> list[dict[str, Any]]:
        self._buffer += data
        messages = []
        while len(self._buffer) >= _HEADER.size:
            (length,) = _HEADER.unpack_from(self._buffer)
            if length > MAX_FRAME_BYTES:
                raise ProtocolError(f"frame of {length} bytes exceeds the {MAX_FRAME_BYTES}-byte limit")
            end = _HEADER.size + length
            if len(self._buffer) < end:
                break
            body = bytes(self._buffer[_HEADER.size : end])
            del self._buffer[:end]
            messages.append(_decode_body(body))
        return messages


def _decode_body(body: bytes) -> dict[str, Any]:
    try:
        message = json.loads(body.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as err:
        raise ProtocolError(f"frame is not valid UTF-8 JSON: {err}") from err
    if not isinstance(message, dict):
        raise ProtocolError(f"frame must hold a JSON object, got {type(message).__name__}")
    return message


@dataclass(frozen=True)
class Request:
    id: int
    event: EventType
    app: str = ""
    title: str = ""
    input_scope: tuple[str, ...] = ()
    before: str = ""
    after: str = ""


def parse_request(message: dict[str, Any]) -> Request:
    request_id = message.get("id")
    if not isinstance(request_id, int) or isinstance(request_id, bool):
        raise ProtocolError(f"request id must be an integer, got {request_id!r}")
    event = message.get("event")
    if event not in _EVENT_TYPES:
        raise ProtocolError(f"unknown event {event!r}")
    strings = {}
    for field in ("app", "title", "before", "after"):
        value = message.get(field, "")
        if not isinstance(value, str):
            raise ProtocolError(f"{field} must be a string, got {type(value).__name__}")
        strings[field] = value
    input_scope = message.get("input_scope", [])
    if not isinstance(input_scope, list) or not all(isinstance(s, str) for s in input_scope):
        raise ProtocolError("input_scope must be a list of strings")
    return Request(id=request_id, event=event, input_scope=tuple(input_scope), **strings)


@dataclass(frozen=True)
class WordReply:
    id: int
    replace: int
    words: tuple[str, ...]

    def to_message(self) -> dict[str, Any]:
        return {"id": self.id, "type": "words", "replace": self.replace, "words": list(self.words)}
