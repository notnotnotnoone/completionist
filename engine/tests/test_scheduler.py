from completionist_engine.phrase_scheduler import Cancel, PhraseScheduler, Start
from completionist_engine.protocol import Request

DEBOUNCE = 0.35


def req(request_id: int, before: str, event: str = "keystroke") -> Request:
    return Request(id=request_id, event=event, app="notepad.exe", before=before)


def starts(actions):
    return [a for a in actions if isinstance(a, Start)]


def cancels(actions):
    return [a for a in actions if isinstance(a, Cancel)]


def new(mode="auto"):
    return PhraseScheduler(debounce=DEBOUNCE), mode


def test_in_auto_mode_a_request_starts_only_after_a_pause():
    s, mode = new()
    update = s.request(req(1, "Hello wor"), mode, now=0.0, budget_ok=True)
    assert update.actions == () and update.phrase == ""
    assert s.next_due() == DEBOUNCE
    assert s.tick(0.2).actions == ()  # not yet
    fired = s.tick(0.36)
    assert [a.request.id for a in starts(fired.actions)] == [1]
    assert s.next_due() is None


def test_every_keystroke_restarts_the_pause():
    s, mode = new()
    s.request(req(1, "Hello w"), mode, now=0.0, budget_ok=True)
    s.request(req(2, "Hello wo"), mode, now=0.3, budget_ok=True)
    assert s.tick(0.5).actions == ()  # 0.3 + 0.35 = 0.65
    fired = s.tick(0.66)
    assert [a.request.id for a in starts(fired.actions)] == [2]  # asks about the latest text only


def test_hotkey_mode_never_asks_on_its_own():
    s, mode = new("hotkey")
    s.request(req(1, "Hello wor"), mode, now=0.0, budget_ok=True)
    assert s.next_due() is None
    assert s.tick(5.0).actions == ()


def test_hotkey_asks_immediately_in_any_non_off_mode():
    for mode in ("auto", "hotkey"):
        s = PhraseScheduler(debounce=DEBOUNCE)
        update = s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
        assert [a.request.id for a in starts(update.actions)] == [1]
        assert update.phrase == "" and update.done is False


def test_off_mode_never_asks_even_on_hotkey():
    s, mode = new("off")
    assert s.request(req(1, "Hello wor"), mode, now=0.0, budget_ok=True).actions == ()
    assert s.next_due() is None
    assert s.hotkey(req(2, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True).actions == ()


def test_nothing_is_asked_once_the_budget_is_spent():
    s, mode = new()
    s.request(req(1, "Hello wor"), mode, now=0.0, budget_ok=False)
    assert s.next_due() is None
    assert s.hotkey(req(2, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=False).actions == ()


def test_an_empty_field_asks_for_nothing():
    s, mode = new()
    s.request(req(1, "   "), mode, now=0.0, budget_ok=True)
    assert s.next_due() is None


def test_chunks_build_up_the_phrase_for_the_latest_request():
    s, mode = new()
    s.hotkey(req(7, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
    first = s.chunk("ld is", now=0.1)
    assert (first.request_id, first.phrase, first.done) == (7, "ld is", False)
    second = s.chunk(" big", now=0.2)
    assert second.phrase == "ld is big"
    end = s.finished()
    assert (end.phrase, end.done) == ("ld is big", True)


def test_typing_along_with_the_phrase_trims_it_instead_of_asking_again():
    s, mode = new()
    s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
    s.chunk("ld is big", now=0.1)
    s.finished()
    update = s.request(req(2, "Hello worl"), mode, now=0.5, budget_ok=True)
    assert update.phrase == "d is big" and update.actions == () and update.done is True
    assert s.next_due() is None
    update = s.request(req(3, "Hello world i"), mode, now=0.6, budget_ok=True)
    assert update.phrase == "s big"  # skipped "d " in one go
    assert s.next_due() is None


def test_typing_along_while_the_phrase_is_still_streaming_keeps_the_request_running():
    s, mode = new()
    s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
    s.chunk("ld", now=0.1)
    update = s.request(req(2, "Hello worl"), mode, now=0.15, budget_ok=True)
    assert update.actions == () and update.phrase == "d" and update.done is False
    more = s.chunk(" is big", now=0.2)
    assert (more.request_id, more.phrase) == (2, "d is big")  # pushes now belong to the newest request


def test_typing_ahead_of_a_phrase_that_has_not_started_yet_waits_for_it():
    s, mode = new()
    s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
    update = s.request(req(2, "Hello worl"), mode, now=0.1, budget_ok=True)
    assert update.actions == () and update.phrase == ""
    more = s.chunk("ld is", now=0.2)  # the model's phrase begins with what was typed
    assert more.phrase == "d is"


def test_typing_something_different_cancels_the_request_and_clears_the_phrase():
    s, mode = new()
    s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
    s.chunk("ld is big", now=0.1)
    update = s.request(req(2, "Hello wox"), mode, now=0.2, budget_ok=True)
    assert [a.request_id for a in cancels(update.actions)] == [1]
    assert update.phrase == ""
    assert s.next_due() == 0.2 + DEBOUNCE  # and asks again after a pause (auto mode)


def test_a_late_chunk_that_contradicts_what_was_typed_is_dropped_and_asked_again():
    s, mode = new()
    s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
    s.request(req(2, "Hello worx"), mode, now=0.1, budget_ok=True)  # typed ahead: 'x'
    late = s.chunk("ld is", now=0.2)  # the phrase starts with 'l', not 'x'
    assert [a.request_id for a in cancels(late.actions)] == [1]
    assert late.phrase == ""
    assert s.next_due() is not None


def test_a_chunk_after_the_request_was_cancelled_is_ignored():
    s, mode = new()
    s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
    s.request(req(2, "Goodbye"), mode, now=0.1, budget_ok=True)
    assert s.chunk("ld", now=0.2) is None
    assert s.finished() is None


def test_moving_the_caret_elsewhere_clears_the_phrase():
    s, mode = new()
    s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
    s.chunk("ld", now=0.1)
    s.finished()
    update = s.request(req(2, "Hello"), mode, now=0.5, budget_ok=True)  # caret moved back a few characters
    assert update.phrase == ""


def test_typing_past_the_end_of_a_finished_phrase_asks_again_after_a_pause():
    s, mode = new()
    s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
    s.chunk("ld", now=0.1)
    s.finished()
    update = s.request(req(2, "Hello world"), mode, now=0.5, budget_ok=True)  # typed it all
    assert update.phrase == ""
    assert s.next_due() == 0.5 + DEBOUNCE
    update = s.request(req(3, "Hello world!"), mode, now=0.6, budget_ok=True)
    assert update.phrase == "" and s.next_due() == 0.6 + DEBOUNCE


def test_a_new_hotkey_press_reuses_a_phrase_that_is_already_there():
    s, mode = new("hotkey")
    s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
    s.chunk("ld", now=0.1)
    s.finished()
    again = s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=1.0, budget_ok=True)
    assert again.actions == () and again.phrase == "ld"


def test_a_hotkey_press_on_new_text_replaces_the_old_request():
    s, mode = new("hotkey")
    s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
    update = s.hotkey(req(2, "Bye for now th", "hotkey"), mode, now=0.5, budget_ok=True)
    assert [a.request_id for a in cancels(update.actions)] == [1]
    assert [a.request.id for a in starts(update.actions)] == [2]


def test_dismissing_clears_the_phrase_and_cancels_any_request():
    s, mode = new()
    s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
    s.chunk("ld", now=0.1)
    actions = s.dismiss()
    assert [a.request_id for a in cancels(actions)] == [1]
    assert s.chunk("more", now=0.2) is None


def test_a_failed_request_keeps_what_had_arrived_and_finishes():
    s, mode = new()
    s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
    s.chunk("ld", now=0.1)
    end = s.failed()
    assert (end.phrase, end.done) == ("ld", True)


def test_a_failed_request_with_nothing_shows_nothing():
    s, mode = new()
    s.hotkey(req(1, "Hello wor", "hotkey"), mode, now=0.0, budget_ok=True)
    end = s.failed()
    assert (end.phrase, end.done) == ("", True)


def test_going_silent_clears_a_pending_request():
    s, mode = new()
    s.request(req(1, "Hello wor"), mode, now=0.0, budget_ok=True)
    s.request(req(2, "Hello wor"), "off", now=0.1, budget_ok=True)
    assert s.next_due() is None
    assert s.tick(1.0).actions == ()
