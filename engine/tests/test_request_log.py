from completionist_engine.phrase_provider import Attempt
from completionist_engine.request_log import RequestLog


def add(log: RequestLog, outcome="ok", app="notepad.exe", reply="ld is", prompt="Hello wor", **extra):
    return log.add(app=app, prompt=prompt, reply=reply, outcome=outcome, attempts=extra.pop("attempts", []), ttft_ms=120, total_ms=300, **extra)


def test_newest_request_comes_first_and_ids_count_up():
    log = RequestLog()
    first, second = add(log), add(log)
    rows, total = log.recent()
    assert [r.id for r in rows] == [second.id, first.id] and total == 2
    assert second.id == first.id + 1


def test_only_the_most_recent_requests_are_kept():
    log = RequestLog(capacity=3)
    ids = [add(log).id for _ in range(5)]
    rows, total = log.recent()
    assert [r.id for r in rows] == ids[:1:-1] and total == 3
    assert log.get(ids[0]) is None and log.get(ids[-1]) is not None


def test_filtering_by_result_and_by_search_text():
    log = RequestLog()
    add(log, "ok", reply="alpha")
    add(log, "failed", reply="", app="chrome.exe")
    add(log, "ok", reply="beta", prompt="Dear Sam, wor")
    assert [r.reply for r in log.recent(result="ok")[0]] == ["beta", "alpha"]
    assert [r.app for r in log.recent(result="failed")[0]] == ["chrome.exe"]
    assert [r.reply for r in log.recent(q="dear sam")[0]] == ["beta"]  # searches the prompt, any case
    assert [r.app for r in log.recent(q="CHROME")[0]] == ["chrome.exe"]  # and the app


def test_limit_trims_the_rows_but_not_the_total():
    log = RequestLog()
    for _ in range(4):
        add(log)
    rows, total = log.recent(limit=2)
    assert len(rows) == 2 and total == 4


def test_full_text_and_event_history_are_kept():
    log = RequestLog()
    entry = add(log, prompt="x" * 5000 + "TAIL", reply="y" * 5000, suffix="after",
                settings={"model": "test"}, events=[{"ms": 0, "kind": "started"}])
    assert entry.prompt == "x" * 5000 + "TAIL"
    assert entry.reply == "y" * 5000
    assert entry.suffix == "after"
    assert entry.settings == {"model": "test"}
    assert entry.events == ({"ms": 0, "kind": "started"},)


def test_attempts_are_kept_and_clear_empties_the_log():
    log = RequestLog()
    entry = add(log, "failover", attempts=[Attempt("a/first", False, "provider timed out", 4000), Attempt("b/second", True, "", 300)])
    assert [a.model for a in log.get(entry.id).attempts] == ["a/first", "b/second"]
    log.clear()
    assert log.recent() == ([], 0)


def test_timings_mode_scrubs_full_detail_from_existing_entries():
    log = RequestLog()
    entry = add(log, suffix="private", settings={"model": "test"},
                events=[{"ms": 1, "kind": "text_chunk", "text": "private"}])
    log.configure("timings", 0)
    scrubbed = log.get(entry.id)
    assert scrubbed is not None
    assert scrubbed.prompt == scrubbed.reply == scrubbed.suffix == ""
    assert scrubbed.settings == {}
    assert scrubbed.events == ()
