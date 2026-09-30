"""Tracks what phrase requests cost, per day, and stops them once the daily cap is reached."""

import json
import logging
import os
from collections.abc import Callable
from dataclasses import dataclass
from datetime import date
from pathlib import Path

logger = logging.getLogger("typer_engine.budget")


@dataclass(frozen=True)
class Usage:
    """Tokens billed for one request, from the provider's own usage report."""

    cached: int = 0
    uncached: int = 0
    output: int = 0


@dataclass(frozen=True)
class Prices:
    """US dollars per million tokens."""

    input_per_m: float = 0.30
    cached_per_m: float = 0.006
    output_per_m: float = 1.20


def cost(usage: Usage, prices: Prices) -> float:
    return (usage.uncached * prices.input_per_m + usage.cached * prices.cached_per_m + usage.output * prices.output_per_m) / 1_000_000


class DailyBudget:
    def __init__(
        self,
        limit_usd: float,
        prices: Prices,
        path: Path | None = None,
        today: Callable[[], date] = date.today,
    ) -> None:
        self._limit = limit_usd
        self._prices = prices
        self._path = path
        self._today = today
        self._day = today()
        self._reset()
        if path is not None:
            self._load(path)

    def _reset(self) -> None:
        self._spent = 0.0
        self._requests = 0
        self._cached = 0
        self._uncached = 0
        self._output = 0

    def _roll(self) -> None:
        if self._today() != self._day:
            self._day = self._today()
            self._reset()

    @property
    def spent_today(self) -> float:
        self._roll()
        return self._spent

    @property
    def requests_today(self) -> int:
        self._roll()
        return self._requests

    @property
    def cache_hit_rate(self) -> float:
        self._roll()
        total = self._cached + self._uncached
        return self._cached / total if total else 0.0

    @property
    def exhausted(self) -> bool:
        return self.spent_today >= self._limit

    def record(self, usage: Usage) -> float:
        self._roll()
        spent = cost(usage, self._prices)
        self._spent += spent
        self._requests += 1
        self._cached += usage.cached
        self._uncached += usage.uncached
        self._output += usage.output
        self._save()
        return spent

    def _load(self, path: Path) -> None:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
            if data["date"] != self._day.isoformat():
                return
            self._spent = float(data["spent"])
            self._requests = int(data["requests"])
            self._cached = int(data["cached"])
            self._uncached = int(data["uncached"])
            self._output = int(data["output"])
        except FileNotFoundError:
            return
        except (OSError, ValueError, KeyError, TypeError) as err:
            logger.warning("ignoring unreadable spend file %s: %s", path, err)
            self._reset()

    def _save(self) -> None:
        if self._path is None:
            return
        data = {
            "date": self._day.isoformat(),
            "spent": self._spent,
            "requests": self._requests,
            "cached": self._cached,
            "uncached": self._uncached,
            "output": self._output,
        }
        try:
            self._path.parent.mkdir(parents=True, exist_ok=True)
            tmp = self._path.with_name(self._path.name + ".tmp")
            tmp.write_text(json.dumps(data), encoding="utf-8")
            os.replace(tmp, self._path)
        except OSError as err:
            logger.warning("could not save spend file: %s", err)
