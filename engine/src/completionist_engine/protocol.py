"""Wire protocol between the TSF text service and the engine.

Each frame is a 4-byte little-endian length followed by that many bytes of UTF-8 JSON (an object).
"""

import json
import struct
from dataclasses import dataclass, field
from typing import Any, Literal, get_args

DEFAULT_PIPE_NAME = r"\\.\pipe\completionist-engine"
MAX_FRAME_BYTES = 1 << 20
_HEADER = struct.Struct("<I")

EventType = Literal["keystroke", "hotkey", "accept", "dismiss"]
_EVENT_TYPES = frozenset(get_args(EventType))
PhraseState = Literal["off", "manual", "scheduled", "working", "streaming", "ready", "unavailable"]


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
    accepted: str = ""
    """For `accept` events: the word that was inserted."""
    quiet: bool = False
    """The text service is holding its popup back here, so there's no point asking for a phrase."""
    kind: str = "word"
    """For `accept` events: "word", "chunk", "next", "phrase" (all of it) or "phrase_word" (one word of it)."""


def parse_request(message: dict[str, Any]) -> Request:
    request_id = message.get("id")
    if not isinstance(request_id, int) or isinstance(request_id, bool):
        raise ProtocolError(f"request id must be an integer, got {request_id!r}")
    event = message.get("event")
    if event not in _EVENT_TYPES:
        raise ProtocolError(f"unknown event {event!r}")
    strings = {}
    for field in ("app", "title", "before", "after", "accepted"):
        value = message.get(field, "")
        if not isinstance(value, str):
            raise ProtocolError(f"{field} must be a string, got {type(value).__name__}")
        strings[field] = value
    kind = message.get("kind", "word")
    if kind not in ("word", "chunk", "next", "phrase", "phrase_word"):
        raise ProtocolError(f"unknown accept kind {kind!r}")
    quiet = message.get("quiet", False)
    if not isinstance(quiet, bool):
        raise ProtocolError("quiet must be true or false")
    input_scope = message.get("input_scope", [])
    if not isinstance(input_scope, list) or not all(isinstance(s, str) for s in input_scope):
        raise ProtocolError("input_scope must be a list of strings")
    return Request(id=request_id, event=event, input_scope=tuple(input_scope), quiet=quiet, kind=kind, **strings)


@dataclass(frozen=True)
class WordReply:
    id: int
    replace: int
    words: tuple[str, ...]
    kinds: tuple[str, ...] = ()
    """What each of `words` is: "word", "chunk" (two or three words) or "next" (offered before a letter is typed). Empty means all words."""
    marks: tuple[tuple[int, ...], ...] = ()
    """Per word, the letter positions the typed fragment did not earn (a typo correction's guessed letters). Empty means none are marked."""
    phrase: str = ""
    """The phrase continuation to show as the top row, if one is ready."""
    phrase_done: bool = True
    """False while more phrase text may still be pushed."""
    phrase_mode: str = "off"
    """"auto", "hotkey" or "off": whether phrases are available in this field."""
    popup: dict[str, Any] | None = None
    """Optional native popup appearance and shortcut preferences; older clients ignore this."""
    phrase_state: PhraseState | None = None
    phrase_wait_ms: int | None = None
    phrase_elapsed_ms: int | None = None
    trigger_reason: str | None = None
    origins: tuple[str, ...] = field(default=(), compare=False)

    def to_message(self) -> dict[str, Any]:
        message: dict[str, Any] = {"id": self.id, "type": "words", "replace": self.replace, "words": list(self.words)}
        if len(self.kinds) == len(self.words) and any(kind != "word" for kind in self.kinds):
            message["kinds"] = list(self.kinds)
        if len(self.marks) == len(self.words) and any(self.marks):
            message["marks"] = [list(positions) for positions in self.marks]
        if self.phrase_mode != "off":
            message["phrase_mode"] = self.phrase_mode
        if self.phrase or not self.phrase_done:
            message["phrase"] = self.phrase
            message["phrase_done"] = self.phrase_done
        if self.popup is not None:
            message["popup"] = self.popup
        _add_status(message, self.phrase_state, self.phrase_wait_ms, self.phrase_elapsed_ms, self.trigger_reason)
        if len(self.origins) == len(self.words) and self.origins:
            message["origins"] = list(self.origins)
        return message


@dataclass(frozen=True)
class PhraseUpdate:
    """Pushed to the text service as a phrase streams in, keyed by the newest request it belongs to."""

    id: int
    text: str
    done: bool
    phrase_state: PhraseState | None = None
    phrase_wait_ms: int | None = None
    phrase_elapsed_ms: int | None = None
    trigger_reason: str | None = None

    def to_message(self) -> dict[str, Any]:
        message: dict[str, Any] = {"id": self.id, "type": "phrase", "text": self.text, "done": self.done}
        _add_status(message, self.phrase_state, self.phrase_wait_ms, self.phrase_elapsed_ms, self.trigger_reason)
        return message


def _add_status(
    message: dict[str, Any], state: PhraseState | None, wait_ms: int | None,
    elapsed_ms: int | None, reason: str | None,
) -> None:
    if state is not None:
        message["phrase_state"] = state
    if wait_ms is not None:
        message["phrase_wait_ms"] = wait_ms
    if elapsed_ms is not None:
        message["phrase_elapsed_ms"] = elapsed_ms
    if reason is not None:
        message["trigger_reason"] = reason
