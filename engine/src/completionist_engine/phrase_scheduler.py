"""Decides when to ask for a phrase, and what to show as the writer keeps typing.

Pure logic: no I/O and no clock of its own (callers pass `now`), so every timing rule is tested with
plain numbers. The async service (`phrases.py`) carries out the actions it returns.

Rules:
  * In "auto" mode a request starts after a pause (default 350 ms) with no new keystroke; in "hotkey"
    mode only the hotkey starts one; "off" never does. Nothing starts while phrases are paused (the
    provider kept failing) or the field is empty.
  * Typing what the phrase says trims it in place (no new request). Typing anything else cancels the
    request, clears the phrase, and in auto mode asks again after the next pause.
  * A chunk that arrives after the writer has typed something different is dropped.
"""

from dataclasses import dataclass
from typing import Literal

from completionist_engine.protocol import Request

Mode = Literal["auto", "hotkey", "off"]


@dataclass(frozen=True)
class Start:
    request: Request


@dataclass(frozen=True)
class Cancel:
    request_id: int


Action = Start | Cancel


@dataclass(frozen=True)
class Update:
    actions: tuple[Action, ...]
    phrase: str
    """What to show now: the part of the phrase not yet typed."""
    done: bool
    """False while more text may still arrive."""
    request_id: int | None = None
    """The request the phrase belongs to (the newest one), for pushes."""


_NOTHING = Update((), "", True)
_MAX_SLIDE = 500  # how far the window may have slid and still count as the same text


class PhraseScheduler:
    def __init__(self, debounce: float = 0.35) -> None:
        self.debounce = debounce
        self._latest: Request | None = None
        self._mode: Mode = "off"
        self._base: str | None = None  # text before the caret when the running/finished request began
        self._text = ""  # everything the model has produced so far
        self._done = True
        self._inflight: int | None = None
        self._pending: Request | None = None
        self._due: float | None = None
        self._origin_id: int | None = None
        self._started_at: float | None = None
        self._streaming = False
        self._terminal_failure = False
        self._usable = True

    # -- events ----------------------------------------------------------------------------------

    def request(self, request: Request, mode: Mode, now: float, usable: bool) -> Update:
        """A keystroke (or caret move): the text before the caret is now `request.before`."""
        self._latest = request
        self._mode = mode
        self._usable = usable
        actions: list[Action] = []
        if self._base is not None:
            if self._typed_since(request.before) is not None and self._consistent():
                shown = self._shown()
                if shown or not self._done:
                    return Update((), shown, self._done, request.id)
            actions += self._clear()
        self._pending = None
        self._due = None
        if mode == "auto" and usable and request.before.strip():
            self._pending = request
            self._due = now + self.debounce
        return Update(tuple(actions), "", True, request.id)

    def hotkey(self, request: Request, mode: Mode, now: float, usable: bool) -> Update:
        """The writer pressed the phrase hotkey."""
        del now
        self._latest = request
        self._mode = mode
        self._usable = usable
        if self._base is not None and self._typed_since(request.before) is not None and self._consistent():
            shown = self._shown()
            if shown or not self._done:
                return Update((), shown, self._done, request.id)
        actions = self._clear()
        self._pending = None
        self._due = None
        if mode == "off" or not usable or not request.before.strip():
            return Update(tuple(actions), "", True, request.id)
        return Update((*actions, *self._start(request)), "", False, request.id)

    def tick(self, now: float) -> Update:
        """Call when `next_due()` has passed: starts the request that waited out the pause."""
        if self._pending is None or self._due is None or now < self._due:
            return _NOTHING
        pending = self._pending
        self._pending = None
        self._due = None
        return Update(tuple(self._start(pending)), "", False, pending.id)

    def chunk(self, text: str, now: float, origin_id: int | None = None) -> Update | None:
        """The provider produced more text. None if nothing is waiting for it any more."""
        if self._base is None or self._latest is None or not self._matches_origin(origin_id):
            return None
        self._text += text
        self._streaming = True
        if not self._consistent():  # the writer typed something the phrase doesn't say
            actions = self._clear()
            if self._mode == "auto" and self._latest.before.strip():
                self._pending = self._latest
                self._due = now + self.debounce
            return Update(tuple(actions), "", True, self._latest.id)
        return Update((), self._shown(), False, self._latest.id)

    def finished(self, origin_id: int | None = None) -> Update | None:
        """The provider stream ended normally."""
        if self._base is None or self._latest is None or not self._matches_origin(origin_id):
            return None
        self._done = True
        self._inflight = None
        self._origin_id = None
        return Update((), self._shown() if self._consistent() else "", True, self._latest.id)

    def failed(self, origin_id: int | None = None) -> Update | None:
        """The provider failed: show whatever arrived and stop."""
        update = self.finished(origin_id)
        if update is not None:
            self._terminal_failure = True
        return update

    def provider_started(self, origin_id: int, now: float) -> bool:
        """Record a real provider invocation, rejecting a cancelled/stale task."""
        if not self._matches_origin(origin_id) or self._started_at is not None:
            return False
        self._started_at = now
        self._terminal_failure = False
        return True

    def provider_chunk(self, origin_id: int) -> bool:
        """Record that the provider yielded text, even when display filtering buffers it."""
        if not self._matches_origin(origin_id) or self._started_at is None:
            return False
        self._streaming = True
        return True

    def provider_is_active(self, origin_id: int) -> bool:
        return self._matches_origin(origin_id) and self._started_at is not None

    def display_status(self, now: float) -> dict[str, object]:
        """Return a bounded, read-only view of current scheduling and provider lifecycle."""
        due = self._due
        if self._mode == "off":
            state, reason = "off", "paused"
        elif due is not None:
            state, reason = "scheduled", "idle"
        elif self._terminal_failure:
            state, reason = "unavailable", "unavailable"
        elif self._inflight is not None:
            reason = "manual" if self._mode == "hotkey" else "idle"
            if self._started_at is None:
                # The scheduler fired, but async preparation/provider invocation has not begun.
                state = "manual" if self._mode == "hotkey" else "scheduled"
            else:
                state = "streaming" if self._streaming else "working"
        elif self._done and self._started_at is not None:
            state, reason = "ready", "manual" if self._mode == "hotkey" else "idle"
        elif self._mode == "hotkey":
            state, reason = "manual", "manual"
        elif not self._usable:
            state, reason = "unavailable", "unavailable"
        else:
            state, reason = "off", "paused"

        remaining = max(0.0, due - now) if due is not None else 0.0
        elapsed = max(0.0, now - self._started_at) if self._started_at is not None else 0.0
        return {
            "phrase_state": state,
            "phrase_wait_ms": max(0, min(600000, round(remaining * 1000))),
            "phrase_elapsed_ms": max(0, min(600000, round(elapsed * 1000))),
            "trigger_reason": reason,
        }

    def dismiss(self) -> tuple[Action, ...]:
        """The writer dismissed the popup, or accepted the phrase."""
        self._pending = None
        self._due = None
        return tuple(self._clear())

    def next_due(self) -> float | None:
        return self._due

    # -- internals -------------------------------------------------------------------------------

    def _start(self, request: Request) -> list[Action]:
        self._base = request.before
        self._text = ""
        self._done = False
        self._inflight = request.id
        self._origin_id = request.id
        self._started_at = None
        self._streaming = False
        self._terminal_failure = False
        return [Start(request)]

    def _clear(self) -> list[Action]:
        actions: list[Action] = [Cancel(self._inflight)] if self._inflight is not None else []
        self._base = None
        self._text = ""
        self._done = True
        self._inflight = None
        self._origin_id = None
        self._started_at = None
        self._streaming = False
        self._terminal_failure = False
        return actions

    def _matches_origin(self, origin_id: int | None) -> bool:
        return self._origin_id is not None and (origin_id is None or self._origin_id == origin_id)

    def _typed_since(self, before: str) -> str | None:
        """What was typed after the request began, or None if `before` doesn't continue its text.

        The text service sends a sliding window, so once the field is longer than the window the start
        moves with every keystroke: a continuation then matches the base shifted by what was typed.
        """
        base = self._base
        assert base is not None
        if before.startswith(base):
            return before[len(base) :]
        n = len(base)
        if len(before) == n:
            for k in range(1, min(n, _MAX_SLIDE)):
                if base.startswith(before[:32], k) and base[k:] == before[: n - k]:
                    return before[n - k :]
        return None

    def _typed(self) -> str | None:
        assert self._latest is not None
        return self._typed_since(self._latest.before)

    def _consistent(self) -> bool:
        """Whether what the writer has typed since the request agrees with the phrase so far."""
        if self._latest is None or self._base is None:
            return False
        typed = self._typed()
        if typed is None:
            return False
        if self._text.startswith(typed):
            return True
        return not self._done and typed.startswith(self._text)  # still streaming: can't tell yet

    def _shown(self) -> str:
        typed = self._typed()
        return self._text[len(typed) :] if typed is not None and self._text.startswith(typed) else ""
