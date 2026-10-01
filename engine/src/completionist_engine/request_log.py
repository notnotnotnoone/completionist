"""The requests log: the last few phrase requests, for the viewer's Requests tab.

Unlike everything else Completionist keeps, this holds text: the prompt sent (the words before the caret)
and the reply that came back, and the screenshot and screen text a request was shown. So it lives in memory only, never on disk, and is gone when the engine stops.
It keeps the newest `capacity` requests; the viewer's Clear button empties it sooner.
"""

import time
from collections import deque
from collections.abc import Callable
from dataclasses import dataclass, field

from completionist_engine.phrase_provider import Attempt
from completionist_engine.screen_context import Capture

OUTCOMES = ("ok", "failover", "failed", "cancelled")  # cancelled: the user kept typing and the request was dropped
_PROMPT_KEEP = 2000  # characters; the end is kept, since that is the text nearest the caret
_REPLY_KEEP = 1000


@dataclass(frozen=True)
class LoggedRequest:
    id: int
    at: float  # seconds since the epoch
    app: str
    prompt: str
    reply: str
    outcome: str
    attempts: tuple[Attempt, ...] = field(default=())
    ttft_ms: int | None = None
    total_ms: int | None = None
    screen: Capture | None = None  # what the screen reader captured for this request: the picture and the text it read
    screen_sent: str = ""  # the part of that text sent to the model (the lines the person typed are left out)

    @property
    def answered_by(self) -> str:
        """The model that produced the reply, if one did."""
        winners = [a.model for a in self.attempts if a.ok]
        return winners[-1] if winners else ""


class RequestLog:
    def __init__(self, capacity: int = 200, clock: Callable[[], float] = time.time) -> None:
        self._items: deque[LoggedRequest] = deque(maxlen=capacity)
        self._clock = clock
        self._next_id = 1

    def add(
        self,
        *,
        app: str,
        prompt: str,
        reply: str,
        outcome: str,
        attempts: list[Attempt],
        ttft_ms: int | None,
        total_ms: int | None,
        screen: Capture | None = None,
        screen_sent: str = "",
    ) -> LoggedRequest:
        entry = LoggedRequest(
            id=self._next_id,
            at=self._clock(),
            app=app,
            prompt=prompt[-_PROMPT_KEEP:],
            reply=reply[:_REPLY_KEEP],
            outcome=outcome,
            attempts=tuple(attempts),
            ttft_ms=ttft_ms,
            total_ms=total_ms,
            screen=screen,
            screen_sent=screen_sent,
        )
        self._next_id += 1
        self._items.append(entry)
        return entry

    def recent(self, *, result: str = "", q: str = "", limit: int = 50) -> tuple[list[LoggedRequest], int]:
        """Matching requests, newest first (at most `limit`), and how many matched in all."""
        q = q.strip().lower()
        matched = [
            r
            for r in reversed(self._items)
            if (not result or r.outcome == result) and (not q or q in f"{r.app} {r.prompt} {r.reply} {r.answered_by}".lower())
        ]
        return matched[:limit], len(matched)

    def get(self, request_id: int) -> LoggedRequest | None:
        return next((r for r in self._items if r.id == request_id), None)

    def clear(self) -> None:
        self._items.clear()
