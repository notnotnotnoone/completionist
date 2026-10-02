"""The requests log: the last few phrase requests, for the viewer's Requests tab.

Unlike everything else Completionist keeps, this holds text: the prompt sent (the words before the caret)
and the reply that came back, and the screenshot and screen text a request was shown. So it lives in memory only, never on disk, and is gone when the engine stops.
It keeps the newest `capacity` requests; the viewer's Clear button empties it sooner.
"""

import time
from collections import deque
from collections.abc import Callable
from dataclasses import dataclass, field, replace

from completionist_engine.phrase_provider import Attempt
from completionist_engine.screen_context import Capture

OUTCOMES = ("ok", "failover", "failed", "cancelled")  # cancelled: the user kept typing and the request was dropped
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
    suffix: str = ""
    settings: dict = field(default_factory=dict)  # request settings, never credentials
    events: tuple[dict, ...] = field(default=())

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
        self._mode = "full"
        self._retention = 0.0

    def configure(self, mode: str, retention_minutes: float) -> None:
        """Privacy changes apply to entries already held, as well as future requests."""
        self._mode = mode
        self._retention = retention_minutes * 60
        if mode == "off":
            self.clear()
        elif mode == "timings":
            self._items = deque(
                (replace(r, prompt="", reply="", screen=None, screen_sent="", suffix="", settings={}, events=()) for r in self._items),
                maxlen=self._items.maxlen,
            )
        self._expire()

    def _expire(self) -> None:
        if self._retention:
            cutoff = self._clock() - self._retention
            while self._items and self._items[0].at <= cutoff:
                self._items.popleft()

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
        suffix: str = "",
        settings: dict | None = None,
        events: list[dict] | None = None,
    ) -> LoggedRequest:
        self._expire()
        if self._mode != "full":
            prompt, reply, screen, screen_sent, suffix, settings, events = "", "", None, "", "", None, None
        entry = LoggedRequest(
            id=self._next_id,
            at=self._clock(),
            app=app,
            prompt=prompt,
            reply=reply,
            outcome=outcome,
            attempts=tuple(attempts),
            ttft_ms=ttft_ms,
            total_ms=total_ms,
            screen=screen,
            screen_sent=screen_sent,
            suffix=suffix,
            settings=dict(settings or {}),
            events=tuple(events or ()),
        )
        self._next_id += 1
        if self._mode != "off":
            self._items.append(entry)
        return entry

    def recent(self, *, result: str = "", q: str = "", limit: int = 50) -> tuple[list[LoggedRequest], int]:
        """Matching requests, newest first (at most `limit`), and how many matched in all."""
        self._expire()
        q = q.strip().lower()
        matched = [
            r
            for r in reversed(self._items)
            if (not result or r.outcome == result) and (not q or q in f"{r.app} {r.prompt} {r.reply} {r.answered_by}".lower())
        ]
        return matched[:limit], len(matched)

    def get(self, request_id: int) -> LoggedRequest | None:
        self._expire()
        return next((r for r in self._items if r.id == request_id), None)

    def clear(self) -> None:
        self._items.clear()
