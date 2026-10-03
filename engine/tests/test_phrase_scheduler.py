from completionist_engine.phrase_scheduler import PhraseScheduler
from completionist_engine.protocol import Request


def request(request_id: int, before: str, event: str = "keystroke") -> Request:
    return Request(id=request_id, event=event, app="notepad.exe", before=before)


def test_status_counts_down_from_the_scheduler_due_time_and_resets_on_typing():
    scheduler = PhraseScheduler(debounce=0.35)
    scheduler.request(request(1, "Hello wor"), "auto", now=10.0, usable=True)
    assert scheduler.display_status(10.1) == {
        "phrase_state": "scheduled", "phrase_wait_ms": 250,
        "phrase_elapsed_ms": 0, "trigger_reason": "idle",
    }
    scheduler.request(request(2, "Hello worl"), "auto", now=10.2, usable=True)
    assert scheduler.display_status(10.2)["phrase_wait_ms"] == 350


def test_manual_disabled_and_cancelled_states_do_not_claim_provider_work():
    scheduler = PhraseScheduler(debounce=0.35)
    scheduler.request(request(1, "Hello wor"), "hotkey", now=1.0, usable=True)
    manual = scheduler.display_status(1.2)
    assert manual["phrase_state"] == "manual" and manual["phrase_wait_ms"] == 0
    scheduler.request(request(2, "Hello wor"), "off", now=1.3, usable=True)
    assert scheduler.display_status(1.3)["phrase_state"] == "off"
    scheduler.request(request(3, "Hello wor"), "auto", now=2.0, usable=False)
    assert scheduler.display_status(2.0)["phrase_state"] != "working"


def test_status_tracks_real_provider_start_first_chunk_finish_and_failure():
    scheduler = PhraseScheduler(debounce=0.35)
    scheduler.hotkey(request(7, "Hello wor", "hotkey"), "hotkey", now=10.0, usable=True)
    assert scheduler.display_status(10.2)["phrase_state"] == "manual"
    assert scheduler.provider_started(7, now=10.4)
    assert scheduler.display_status(10.9) == {
        "phrase_state": "working", "phrase_wait_ms": 0,
        "phrase_elapsed_ms": 500, "trigger_reason": "manual",
    }
    scheduler.provider_chunk(7)
    assert scheduler.display_status(11.0)["phrase_state"] == "streaming"
    scheduler.finished(7)
    assert scheduler.display_status(11.1)["phrase_state"] == "ready"

    scheduler.hotkey(request(8, "Other text", "hotkey"), "hotkey", now=12.0, usable=True)
    scheduler.provider_started(8, now=12.1)
    scheduler.failed(8)
    assert scheduler.display_status(12.2)["phrase_state"] == "unavailable"


def test_stale_provider_lifecycle_events_cannot_revive_a_new_request():
    scheduler = PhraseScheduler(debounce=0.35)
    scheduler.hotkey(request(1, "Hello wor", "hotkey"), "hotkey", now=0.0, usable=True)
    scheduler.request(request(2, "Changed"), "auto", now=0.1, usable=True)
    assert not scheduler.provider_started(1, now=0.2)
    assert scheduler.finished(1) is None
    assert scheduler.failed(1) is None
    assert not scheduler.provider_chunk(1)
    assert scheduler.display_status(0.2)["phrase_state"] == "scheduled"

