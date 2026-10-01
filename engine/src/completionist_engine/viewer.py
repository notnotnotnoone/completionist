"""The viewer: a page, served to this machine only, that shows what Completionist has learned.

It lists the learned words (and lets you remove one), shows the usage stats, edits the settings and lists
recent phrase requests. Apart from those requests (the in-memory log, which holds the text sent and the
suggestion that came back), only counts are ever served, never typed text. Because it can change things,
it is locked down:

- it listens on 127.0.0.1 only, on a port picked at start;
- every request needs the secret token the tray puts in the address it opens (kept in memory, never logged);
- the Host header must be this machine's own address, which stops DNS-rebinding tricks;
- a request that carries an Origin must come from the viewer's own page, and changes must be JSON POSTs,
  which another website can't send without a cross-origin check the server never approves.

It runs on the engine's event loop, so it never touches the personal store from a second thread.
"""

import asyncio
import hmac
import json
import logging
import secrets
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, urlsplit

from completionist_engine.config import ConfigError
from completionist_engine.metrics import Metrics
from completionist_engine.personal import PersonalStore
from completionist_engine.request_log import LoggedRequest, RequestLog
from completionist_engine.settings import apply_settings, read_settings

logger = logging.getLogger("completionist_engine.viewer")

PAGE = Path(__file__).with_name("viewer.html")
TOKEN_HEADER = "x-completionist-token"
MAX_BODY = 64 * 1024
MAX_HEADER_BYTES = 16 * 1024
TIMEOUT = 5.0
WORD_LIMIT_MAX = 1000

_STATUS = {200: "OK", 400: "Bad Request", 403: "Forbidden", 404: "Not Found", 500: "Internal Server Error", 405: "Method Not Allowed", 413: "Payload Too Large"}


class ViewerServer:
    def __init__(
        self,
        personal: PersonalStore | None,
        metrics: Metrics | None,
        config_path: Path,
        token: str | None = None,
        request_log: RequestLog | None = None,
    ) -> None:
        self._request_log = request_log
        self._personal = personal
        self._metrics = metrics
        self._config_path = config_path
        self._token = token or secrets.token_urlsafe(32)
        self._server: asyncio.Server | None = None
        self.port = 0

    async def start(self) -> None:
        self._server = await asyncio.start_server(self._serve, "127.0.0.1", 0)
        self.port = self._server.sockets[0].getsockname()[1]
        logger.info("viewer listening on 127.0.0.1:%d", self.port)

    def close(self) -> None:
        if self._server is not None:
            self._server.close()

    @property
    def url(self) -> str:
        """The address the tray opens; the only place the token appears."""
        return f"http://127.0.0.1:{self.port}/?t={self._token}"

    # -- HTTP ------------------------------------------------------------------------------------

    async def _serve(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        try:
            status, body, kind = await asyncio.wait_for(self._respond(reader), TIMEOUT)
        except (TimeoutError, asyncio.IncompleteReadError, asyncio.LimitOverrunError, ConnectionError, ValueError):
            status, body, kind = 400, b'{"error": "bad request"}', "application/json"
        except Exception as err:  # the message can carry a path or text, so only the type is logged
            logger.warning("viewer request failed: %s", type(err).__name__)
            status, body, kind = 500, b'{"error": "something went wrong"}', "application/json"
        try:
            head = (
                f"HTTP/1.1 {status} {_STATUS.get(status, 'Error')}\r\n"
                f"Content-Type: {kind}; charset=utf-8\r\n"
                f"Content-Length: {len(body)}\r\n"
                "Cache-Control: no-store\r\n"
                "X-Content-Type-Options: nosniff\r\n"
                "Referrer-Policy: no-referrer\r\n"
                "Content-Security-Policy: default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
                "connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'\r\n"
                "Connection: close\r\n\r\n"
            )
            writer.write(head.encode("ascii") + body)
            await writer.drain()
        except ConnectionError:
            pass
        finally:
            writer.close()

    async def _respond(self, reader: asyncio.StreamReader) -> tuple[int, bytes, str]:
        request_line = (await reader.readline()).decode("latin-1").strip()
        parts = request_line.split(" ")
        if len(parts) != 3:
            raise ValueError("not an HTTP request")
        method, target, _version = parts
        headers: dict[str, str] = {}
        size = 0
        while True:
            line = (await reader.readline()).decode("latin-1")
            size += len(line)
            if size > MAX_HEADER_BYTES:
                raise ValueError("headers too long")
            if line in ("\r\n", "\n", ""):
                break
            name, _, value = line.partition(":")
            headers[name.strip().lower()] = value.strip()

        if not self._host_ok(headers.get("host", "")):
            return _json(403, {"error": "forbidden"})
        if method not in ("GET", "POST"):
            return _json(405, {"error": "method not allowed"})
        origin = headers.get("origin")
        if origin is not None and origin.lower() not in (f"http://127.0.0.1:{self.port}", f"http://localhost:{self.port}"):
            return _json(403, {"error": "forbidden"})

        url = urlsplit(target)
        if url.path == "/":
            given = parse_qs(url.query).get("t", [""])[0]
            if method != "GET" or not self._token_ok(given):
                return _json(403, {"error": "forbidden"})
            return 200, PAGE.read_bytes(), "text/html"
        if not url.path.startswith("/api/"):
            return _json(404, {"error": "not found"})
        if not self._token_ok(headers.get(TOKEN_HEADER, "")):
            return _json(403, {"error": "forbidden"})

        body: Any = None
        if method == "POST":
            if origin is None or not headers.get("content-type", "").startswith("application/json"):
                return _json(403, {"error": "forbidden"})
            length = int(headers.get("content-length", "0") or 0)
            if length > MAX_BODY:
                return _json(413, {"error": "too large"})
            try:
                body = json.loads(await reader.readexactly(length)) if length else {}
            except json.JSONDecodeError:
                return _json(400, {"error": "that isn't JSON"})
        return self._route(method, url.path, parse_qs(url.query), body)

    def _host_ok(self, host: str) -> bool:
        return host.lower() in (f"127.0.0.1:{self.port}", f"localhost:{self.port}")

    def _token_ok(self, given: str) -> bool:
        return hmac.compare_digest(given.encode(), self._token.encode())

    # -- the JSON interface ----------------------------------------------------------------------

    def _route(self, method: str, path: str, query: dict[str, list[str]], body: Any) -> tuple[int, bytes, str]:
        if (method, path) == ("GET", "/api/words"):
            return _json(200, self._words(query))
        if (method, path) == ("POST", "/api/forget"):
            return self._forget(body)
        if (method, path) == ("GET", "/api/trigrams"):
            return _json(200, self._trigrams(query))
        if (method, path) == ("POST", "/api/forget-trigram"):
            return self._forget_trigram(body)
        if (method, path) == ("GET", "/api/stats"):
            return _json(200, self._stats(query))
        if (method, path) == ("GET", "/api/requests"):
            return _json(200, self._requests(query))
        if (method, path) == ("POST", "/api/requests/clear"):
            if self._request_log is not None:
                self._request_log.clear()
            return _json(200, {"cleared": True})
        if method == "GET" and path.startswith("/api/requests/"):
            return self._request_detail(path.removeprefix("/api/requests/"))
        if (method, path) == ("GET", "/api/settings"):
            return self._settings()
        if (method, path) == ("POST", "/api/settings"):
            return self._change_settings(body)
        return _json(404, {"error": "not found"})

    def _words(self, query: dict[str, list[str]]) -> dict[str, Any]:
        search = query.get("q", [""])[0]
        limit = max(1, min(_number(query.get("limit", ["200"])[0], 200), WORD_LIMIT_MAX))
        found = self._personal.words(search) if self._personal is not None else []
        if query.get("sort", ["count"])[0] == "alpha":
            found = sorted(found)
        return {"learning": self._personal is not None, "total": len(found), "words": [{"word": w, "count": n} for w, n in found[:limit]]}

    def _forget(self, body: Any) -> tuple[int, bytes, str]:
        word = body.get("word") if isinstance(body, dict) else None
        if not isinstance(word, str) or not word:
            return _json(400, {"error": "say which word"})
        if self._personal is None:
            return _json(404, {"error": "learning is off"})
        return _json(200, {"forgotten": self._personal.forget(word)})

    def _trigrams(self, query: dict[str, list[str]]) -> dict[str, Any]:
        search = query.get("q", [""])[0]
        limit = max(1, min(_number(query.get("limit", ["200"])[0], 200), WORD_LIMIT_MAX))
        found = self._personal.trigrams(search) if self._personal is not None else []
        if query.get("sort", ["count"])[0] == "alpha":
            found = sorted(found)
        return {"learning": self._personal is not None, "total": len(found), "trigrams": [{"words": list(t), "count": n} for t, n in found[:limit]]}

    def _forget_trigram(self, body: Any) -> tuple[int, bytes, str]:
        words = body.get("words") if isinstance(body, dict) else None
        if not (isinstance(words, list) and len(words) == 3 and all(isinstance(w, str) and w for w in words)):
            return _json(400, {"error": "say which three words"})
        if self._personal is None:
            return _json(404, {"error": "learning is off"})
        return _json(200, {"forgotten": self._personal.forget_trigram(*words)})

    def _stats(self, query: dict[str, list[str]]) -> dict[str, Any]:
        days = max(1, min(_number(query.get("days", ["7"])[0], 7), 365))
        if self._metrics is None:
            return {"days": days, "available": False}
        self._metrics.flush()
        s = self._metrics.summary(days)
        return {
            "days": days,
            "available": True,
            "keystrokes_saved": s.keystrokes_saved,
            "words": {"shown": s.shown_words, "accepted": s.accepted_words, "rate": s.word_acceptance_rate},
            "phrases": {"shown": s.shown_phrases, "accepted": s.accepted_phrases, "rate": s.phrase_acceptance_rate, "partial": s.accepted_partial},
            "dismissed": s.dismissed,
            "apps": [
                {"app": app, "shown": a.shown, "accepted": a.accepted, "saved": a.saved}
                for app, a in sorted(s.per_app.items(), key=lambda item: -item[1].saved)
            ],
            "daily": [{"day": d, "shown": shown, "accepted": acc, "saved": saved} for d, shown, acc, saved in self._metrics.daily(days)],
            "providers": [
                {"name": name, "requests": p.requests, "errors": p.errors, "p50_ms": round(p.ttft_p50 * 1000), "p95_ms": round(p.ttft_p95 * 1000)}
                for name, p in s.providers.items()
            ],
        }  # fmt: skip

    def _requests(self, query: dict[str, list[str]]) -> dict[str, Any]:
        if self._request_log is None:
            return {"available": False, "total": 0, "requests": []}
        limit = max(1, min(_number(query.get("limit", ["50"])[0], 50), 200))
        found, total = self._request_log.recent(result=query.get("result", [""])[0], q=query.get("q", [""])[0], limit=limit)
        return {"available": True, "total": total, "requests": [_request_row(r) for r in found]}

    def _request_detail(self, raw_id: str) -> tuple[int, bytes, str]:
        found = self._request_log.get(int(raw_id)) if self._request_log is not None and raw_id.isdigit() else None
        if found is None:
            return _json(404, {"error": "that request is no longer in the log"})
        return _json(200, {**_request_row(found), "prompt": found.prompt, "attempts": [vars(a) for a in found.attempts]})

    def _settings(self) -> tuple[int, bytes, str]:
        try:
            return _json(200, read_settings(self._config_path))
        except ConfigError as err:
            return _json(400, {"error": str(err)})

    def _change_settings(self, body: Any) -> tuple[int, bytes, str]:
        try:
            apply_settings(self._config_path, body)
            return _json(200, read_settings(self._config_path))
        except ConfigError as err:
            return _json(400, {"error": str(err)})


def _json(status: int, payload: dict[str, Any]) -> tuple[int, bytes, str]:
    return status, json.dumps(payload).encode("utf-8"), "application/json"


def _request_row(r: LoggedRequest) -> dict[str, Any]:
    return {
        "id": r.id, "at": r.at, "app": r.app, "outcome": r.outcome, "model": r.answered_by, "reply": r.reply,
        "ttft_ms": r.ttft_ms, "total_ms": r.total_ms, "tries": len(r.attempts),
    }  # fmt: skip


def _number(text: str, default: int) -> int:
    try:
        return int(text)
    except ValueError:
        return default
