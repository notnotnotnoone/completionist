import asyncio
import contextlib
import json
from datetime import date

import pytest

from completionist_engine.metrics import Metrics
from completionist_engine.personal import PersonalStore
from completionist_engine.phrase_provider import Attempt
from completionist_engine.request_log import RequestLog
from completionist_engine.viewer import TOKEN_HEADER, ViewerServer

TOKEN = "test-token-abc"


def store() -> PersonalStore:
    personal = PersonalStore()
    for word, times in [("zebra", 5), ("apple", 9), ("mango", 2)]:
        for _ in range(times):
            personal.record_typed(word, ())
    personal.record_typed("fig", ("apple", "mango"))
    for _ in range(3):
        personal.record_typed("pie", ("apple", "tart"))
    return personal


@contextlib.asynccontextmanager
async def running(tmp_path, personal=None, metrics=None, config_text="[words]\nlimit = 5\n", request_log=None):
    path = tmp_path / "config.toml"
    path.write_text(config_text, encoding="utf-8")
    server = ViewerServer(personal, metrics, path, token=TOKEN, request_log=request_log)
    await server.start()
    try:
        yield server
    finally:
        server.close()


async def http(server, method, path, headers=None, body=None, host=None, raw=None):
    """Send one request and return (status, headers, body text)."""
    sent = {"Host": host or f"127.0.0.1:{server.port}", **(headers or {})}
    payload = b"" if body is None else json.dumps(body).encode()
    if body is not None:
        sent.setdefault("Content-Type", "application/json")
        sent["Content-Length"] = str(len(payload))
    reader, writer = await asyncio.open_connection("127.0.0.1", server.port)
    if raw is None:
        head = f"{method} {path} HTTP/1.1\r\n" + "".join(f"{k}: {v}\r\n" for k, v in sent.items()) + "\r\n"
        writer.write(head.encode() + payload)
    else:
        writer.write(raw)
    await writer.drain()
    data = await reader.read()
    writer.close()
    head, _, text = data.partition(b"\r\n\r\n")
    lines = head.decode().split("\r\n")
    return int(lines[0].split()[1]), {k.lower(): v for k, v in (line.split(": ", 1) for line in lines[1:])}, text.decode()


def api(server, method, path, body=None, **extra):
    headers = {TOKEN_HEADER: TOKEN}
    if method == "POST":
        headers["Origin"] = f"http://127.0.0.1:{server.port}"
    headers.update(extra)
    return http(server, method, path, headers, body)


def run(coro):
    return asyncio.run(coro)


# --- the page and the address ------------------------------------------------------------------


def test_the_address_is_local_and_carries_the_token(tmp_path):
    async def go():
        async with running(tmp_path) as server:
            assert server.url == f"http://127.0.0.1:{server.port}/?t={TOKEN}"
            status, headers, text = await http(server, "GET", f"/?t={TOKEN}")
            assert status == 200 and "Completionist Viewer" in text
            assert headers["cache-control"] == "no-store"
            assert "default-src 'none'" in headers["content-security-policy"]

    run(go())


def test_it_listens_on_the_loopback_address_only(tmp_path):
    async def go():
        async with running(tmp_path) as server:
            assert [s.getsockname()[0] for s in server._server.sockets] == ["127.0.0.1"]

    run(go())


# --- keeping other websites out -----------------------------------------------------------------


def test_the_page_and_the_api_refuse_a_missing_or_wrong_token(tmp_path):
    async def go():
        async with running(tmp_path, store()) as server:
            assert (await http(server, "GET", "/"))[0] == 403
            assert (await http(server, "GET", "/?t=nope"))[0] == 403
            assert (await http(server, "GET", "/api/words"))[0] == 403
            assert (await http(server, "GET", "/api/words", {TOKEN_HEADER: "nope"}))[0] == 403
            assert (await http(server, "GET", f"/api/words?t={TOKEN}"))[0] == 403  # the token in a query only opens the page

    run(go())


def test_a_request_for_another_host_name_is_refused(tmp_path):
    async def go():
        async with running(tmp_path, store()) as server:
            status, _, _ = await http(server, "GET", f"/?t={TOKEN}", host=f"evil.example:{server.port}")
            assert status == 403
            status, _, _ = await http(server, "GET", "/api/words", {TOKEN_HEADER: TOKEN}, host="rebind.example")
            assert status == 403

    run(go())


def test_a_request_from_another_website_is_refused_even_with_the_token(tmp_path):
    async def go():
        async with running(tmp_path, store()) as server:
            status, _, _ = await api(server, "GET", "/api/words", Origin="https://evil.example")
            assert status == 403
            status, _, _ = await api(server, "POST", "/api/forget", {"word": "zebra"}, Origin="https://evil.example")
            assert status == 403
            assert ("zebra", 5) in server._personal.words()

    run(go())


def test_a_change_needs_an_origin_and_json(tmp_path):
    async def go():
        async with running(tmp_path, store()) as server:
            no_origin = await http(server, "POST", "/api/forget", {TOKEN_HEADER: TOKEN}, {"word": "zebra"})
            assert no_origin[0] == 403
            form = await http(
                server, "POST", "/api/forget", {TOKEN_HEADER: TOKEN, "Origin": f"http://127.0.0.1:{server.port}", "Content-Type": "text/plain"}, {"word": "zebra"}
            )
            assert form[0] == 403
            assert ("zebra", 5) in server._personal.words()

    run(go())


def test_the_token_is_never_in_a_response_or_error(tmp_path):
    async def go():
        async with running(tmp_path, store()) as server:
            for status, _, text in [
                await http(server, "GET", f"/nothing?t={TOKEN}", {TOKEN_HEADER: TOKEN}),
                await api(server, "GET", "/api/words"),
                await http(server, "GET", "/?t=wrong"),
            ]:
                assert TOKEN not in text
            assert status == 403

    run(go())


def test_bad_requests_get_an_error_not_a_crash(tmp_path):
    async def go():
        async with running(tmp_path, store()) as server:
            assert (await http(server, "GET", "/", raw=b"garbage\r\n\r\n"))[0] == 400
            assert (await http(server, "DELETE", "/api/words", {TOKEN_HEADER: TOKEN}))[0] == 405
            assert (await api(server, "GET", "/api/nothing"))[0] == 404
            big = await http(
                server, "POST", "/api/forget",
                {TOKEN_HEADER: TOKEN, "Origin": f"http://127.0.0.1:{server.port}", "Content-Type": "application/json", "Content-Length": "9999999"},
            )  # fmt: skip
            assert big[0] == 413
            bad_json = await http(
                server, "POST", "/api/forget", raw=(
                    f"POST /api/forget HTTP/1.1\r\nHost: 127.0.0.1:{server.port}\r\n{TOKEN_HEADER}: {TOKEN}\r\n"
                    f"Origin: http://127.0.0.1:{server.port}\r\nContent-Type: application/json\r\nContent-Length: 3\r\n\r\n{{{{{{"
                ).encode(),
            )  # fmt: skip
            assert bad_json[0] == 400

    run(go())


# --- words ---------------------------------------------------------------------------------------


def test_words_are_listed_most_used_first_and_can_be_searched_and_sorted(tmp_path):
    async def go():
        async with running(tmp_path, store()) as server:
            _, _, text = await api(server, "GET", "/api/words")
            data = json.loads(text)
            assert data["learning"] is True
            assert [w["word"] for w in data["words"]][:3] == ["apple", "zebra", "pie"]
            _, _, text = await api(server, "GET", "/api/words?sort=alpha")
            assert [w["word"] for w in json.loads(text)["words"]][:2] == ["apple", "fig"]
            _, _, text = await api(server, "GET", "/api/words?q=ZEB")
            assert json.loads(text) == {"learning": True, "total": 1, "words": [{"word": "zebra", "count": 5}]}
            _, _, text = await api(server, "GET", "/api/words?limit=1")
            assert json.loads(text)["total"] == 5 and len(json.loads(text)["words"]) == 1

    run(go())


def test_removing_a_word_forgets_it_and_what_it_was_part_of(tmp_path):
    async def go():
        personal = store()
        async with running(tmp_path, personal) as server:
            status, _, text = await api(server, "POST", "/api/forget", {"word": "apple"})
            assert (status, json.loads(text)) == (200, {"forgotten": True})
            assert "apple" not in [w for w, _ in personal.words()]
            assert json.loads((await api(server, "POST", "/api/forget", {"word": "apple"}))[2]) == {"forgotten": False}
            assert (await api(server, "POST", "/api/forget", {"word": 3}))[0] == 400
            assert (await api(server, "POST", "/api/forget", {}))[0] == 400

    run(go())


def test_with_learning_off_there_are_no_words(tmp_path):
    async def go():
        async with running(tmp_path, None) as server:
            data = json.loads((await api(server, "GET", "/api/words"))[2])
            assert data == {"learning": False, "total": 0, "words": []}
            assert (await api(server, "POST", "/api/forget", {"word": "x"}))[0] == 404

    run(go())


# --- stats: counts only --------------------------------------------------------------------------


def test_stats_hold_counts_and_app_names_only(tmp_path):
    async def go():
        metrics = Metrics(today=lambda: date(2026, 9, 29))
        metrics.record_shown("discord.exe", "word")
        metrics.record_shown("discord.exe", "word")
        metrics.record_accept("discord.exe", "word", chars=7)
        async with running(tmp_path, store(), metrics) as server:
            data = json.loads((await api(server, "GET", "/api/stats?days=3"))[2])
        assert data["keystrokes_saved"] == 6
        assert data["words"] == {"shown": 2, "accepted": 1, "rate": 0.5}
        assert data["apps"] == [{"app": "discord.exe", "shown": 2, "accepted": 1, "saved": 6}]
        assert [d["day"] for d in data["daily"]] == ["2026-09-27", "2026-09-28", "2026-09-29"]
        assert data["daily"][-1] == {"day": "2026-09-29", "shown": 2, "accepted": 1, "saved": 6}

    run(go())


def test_stats_off_is_reported_not_an_error(tmp_path):
    async def go():
        async with running(tmp_path) as server:
            assert json.loads((await api(server, "GET", "/api/stats"))[2])["available"] is False

    run(go())


# --- settings --------------------------------------------------------------------------------------


def test_settings_are_read_and_changed_through_the_api(tmp_path):
    async def go():
        async with running(tmp_path) as server:
            data = json.loads((await api(server, "GET", "/api/settings"))[2])
            assert data["words"]["limit"] == 5 and data["phrase"]["api_key_set"] is False
            status, _, text = await api(server, "POST", "/api/settings", {"words": {"limit": 9}, "phrase": {"api_key": "sk-secret-key-99"}})
            assert status == 200
            assert json.loads(text)["words"]["limit"] == 9
            assert json.loads(text)["phrase"]["api_key_set"] is True
            assert "secret" not in text and "secret" not in (await api(server, "GET", "/api/settings"))[2]
            assert "limit = 9" in (tmp_path / "config.toml").read_text(encoding="utf-8")

    run(go())


@pytest.mark.parametrize("body", [{"words": {"limit": 0}}, {"data": {"dir": "C:/x"}}, {"words": {"bogus": 1}}, []])
def test_a_bad_settings_change_is_a_clear_error_and_changes_nothing(tmp_path, body):
    async def go():
        async with running(tmp_path) as server:
            before = (tmp_path / "config.toml").read_text(encoding="utf-8")
            status, _, text = await api(server, "POST", "/api/settings", body)
            assert status == 400 and json.loads(text)["error"]
            assert (tmp_path / "config.toml").read_text(encoding="utf-8") == before

    run(go())


def test_a_bad_key_error_does_not_echo_the_key(tmp_path):
    async def go():
        async with running(tmp_path) as server:
            _, _, text = await api(server, "POST", "/api/settings", {"phrase": {"api_key": 424242424242}})
            assert "424242424242" not in text

    run(go())


# --- trigrams ------------------------------------------------------------------------------------


def test_trigrams_are_listed_with_counts_and_can_be_searched(tmp_path):
    async def go():
        async with running(tmp_path, store()) as server:
            data = json.loads((await api(server, "GET", "/api/trigrams"))[2])
            assert data["learning"] is True and data["total"] == 2
            assert data["trigrams"][0] == {"words": ["apple", "tart", "pie"], "count": 3}
            found = json.loads((await api(server, "GET", "/api/trigrams?q=mango+fig"))[2])
            assert found["trigrams"] == [{"words": ["apple", "mango", "fig"], "count": 1}]
            assert len(json.loads((await api(server, "GET", "/api/trigrams?limit=1"))[2])["trigrams"]) == 1

    run(go())


def test_removing_a_trigram_keeps_its_words(tmp_path):
    async def go():
        personal = store()
        async with running(tmp_path, personal) as server:
            status, _, text = await api(server, "POST", "/api/forget-trigram", {"words": ["apple", "tart", "pie"]})
            assert (status, json.loads(text)) == (200, {"forgotten": True})
            assert [t for t, _ in personal.trigrams()] == [("apple", "mango", "fig")]
            assert "pie" in [w for w, _ in personal.words()]
            assert json.loads((await api(server, "POST", "/api/forget-trigram", {"words": ["apple", "tart", "pie"]}))[2]) == {"forgotten": False}
            for bad in ({"words": ["a", "b"]}, {"words": "a b c"}, {"words": ["a", "b", 3]}, {}):
                assert (await api(server, "POST", "/api/forget-trigram", bad))[0] == 400

    run(go())


def test_trigram_changes_are_guarded_like_the_rest(tmp_path):
    async def go():
        async with running(tmp_path, store()) as server:
            assert (await http(server, "GET", "/api/trigrams"))[0] == 403
            status, _, _ = await api(server, "POST", "/api/forget-trigram", {"words": ["apple", "tart", "pie"]}, Origin="https://evil.example")
            assert status == 403
            assert len(server._personal.trigrams()) == 2
        async with running(tmp_path, None) as server:
            assert json.loads((await api(server, "GET", "/api/trigrams"))[2]) == {"learning": False, "total": 0, "trigrams": []}
            assert (await api(server, "POST", "/api/forget-trigram", {"words": ["a", "b", "c"]}))[0] == 404

    run(go())


# --- the requests log -------------------------------------------------------------------------------


def logged() -> RequestLog:
    log = RequestLog()
    log.add(app="notepad.exe", prompt="Hello wor", reply="ld is", outcome="ok", attempts=[Attempt("m/one", True, "", 280)], ttft_ms=120, total_ms=300)
    log.add(
        app="chrome.exe", prompt="Dear Sam, thx", reply="", outcome="failed", attempts=[Attempt("m/one", False, "provider timed out", 4000)], ttft_ms=None, total_ms=4000
    )
    return log


def test_the_request_list_is_newest_first_and_filterable(tmp_path):
    async def go():
        async with running(tmp_path, request_log=logged()) as server:
            data = json.loads((await api(server, "GET", "/api/requests"))[2])
            assert data["total"] == 2 and [r["app"] for r in data["requests"]] == ["chrome.exe", "notepad.exe"]
            row = data["requests"][1]
            assert row["outcome"] == "ok" and row["model"] == "m/one" and row["reply"] == "ld is" and row["total_ms"] == 300
            assert "prompt" not in row  # the list stays light; the prompt is in the detail
            failed = json.loads((await api(server, "GET", "/api/requests?result=failed"))[2])
            assert [r["app"] for r in failed["requests"]] == ["chrome.exe"]
            found = json.loads((await api(server, "GET", "/api/requests?q=dear+sam"))[2])
            assert found["total"] == 1

    run(go())


def test_one_request_opens_with_its_prompt_reply_and_attempts(tmp_path):
    async def go():
        async with running(tmp_path, request_log=logged()) as server:
            detail = json.loads((await api(server, "GET", "/api/requests/2"))[2])
            assert detail["prompt"] == "Dear Sam, thx" and detail["outcome"] == "failed"
            assert detail["attempts"] == [{"model": "m/one", "ok": False, "error": "provider timed out", "ms": 4000}]
            assert (await api(server, "GET", "/api/requests/99"))[0] == 404
            assert (await api(server, "GET", "/api/requests/abc"))[0] == 404

    run(go())


def test_the_log_can_be_cleared_and_needs_the_token(tmp_path):
    async def go():
        log = logged()
        async with running(tmp_path, request_log=log) as server:
            assert (await http(server, "GET", "/api/requests"))[0] == 403  # no token
            assert (await api(server, "POST", "/api/requests/clear", {}))[0] == 200
            assert json.loads((await api(server, "GET", "/api/requests"))[2])["total"] == 0

    run(go())


def test_without_a_log_the_requests_list_is_empty(tmp_path):
    async def go():
        async with running(tmp_path) as server:
            data = json.loads((await api(server, "GET", "/api/requests"))[2])
            assert data["available"] is False and data["requests"] == []

    run(go())
