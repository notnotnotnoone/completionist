import pytest

from completionist_engine.protocol import (
    MAX_FRAME_BYTES,
    FrameDecoder,
    ProtocolError,
    Request,
    WordReply,
    encode,
    parse_request,
)


def test_frame_round_trips_a_message():
    decoder = FrameDecoder()
    assert decoder.feed(encode({"id": 1, "text": "héllo"})) == [{"id": 1, "text": "héllo"}]


def test_frame_split_across_chunks_is_reassembled():
    data = encode({"id": 7, "before": "hello wor"})
    decoder = FrameDecoder()
    messages = []
    for i in range(len(data)):
        messages += decoder.feed(data[i : i + 1])
    assert messages == [{"id": 7, "before": "hello wor"}]


def test_several_frames_in_one_chunk_all_decode():
    data = encode({"id": 1}) + encode({"id": 2}) + encode({"id": 3})
    assert FrameDecoder().feed(data) == [{"id": 1}, {"id": 2}, {"id": 3}]


def test_oversize_frame_header_is_rejected():
    header = (MAX_FRAME_BYTES + 1).to_bytes(4, "little")
    with pytest.raises(ProtocolError):
        FrameDecoder().feed(header)


def test_malformed_json_is_rejected():
    body = b"{not json"
    with pytest.raises(ProtocolError):
        FrameDecoder().feed(len(body).to_bytes(4, "little") + body)


def test_non_object_json_is_rejected():
    body = b"[1, 2]"
    with pytest.raises(ProtocolError):
        FrameDecoder().feed(len(body).to_bytes(4, "little") + body)


def test_parse_request_reads_all_fields():
    request = parse_request(
        {
            "id": 5,
            "event": "keystroke",
            "app": "chrome.exe",
            "title": "Inbox",
            "input_scope": ["IS_DEFAULT"],
            "before": "hello wor",
            "after": "ld",
        }
    )
    assert request == Request(
        id=5,
        event="keystroke",
        app="chrome.exe",
        title="Inbox",
        input_scope=("IS_DEFAULT",),
        before="hello wor",
        after="ld",
    )


def test_parse_request_fills_defaults_for_optional_fields():
    assert parse_request({"id": 1, "event": "dismiss"}) == Request(id=1, event="dismiss")


@pytest.mark.parametrize(
    "message",
    [
        {"event": "keystroke"},
        {"id": "1", "event": "keystroke"},
        {"id": 1, "event": "explode"},
        {"id": 1, "event": "keystroke", "before": 42},
        {"id": 1, "event": "keystroke", "input_scope": "IS_URL"},
    ],
)
def test_parse_request_rejects_invalid_messages(message):
    with pytest.raises(ProtocolError):
        parse_request(message)


def test_word_reply_serialises_to_wire_shape():
    reply = WordReply(id=3, replace=3, words=("world", "work"))
    assert reply.to_message() == {"id": 3, "type": "words", "replace": 3, "words": ["world", "work"]}


def test_a_word_reply_carries_phrase_fields_only_when_relevant():
    from completionist_engine.protocol import PhraseUpdate

    plain = WordReply(id=1, replace=0, words=())
    assert "phrase" not in plain.to_message() and "phrase_mode" not in plain.to_message()

    available = WordReply(id=1, replace=2, words=("a",), phrase_mode="hotkey")
    assert available.to_message()["phrase_mode"] == "hotkey"
    assert "phrase" not in available.to_message()

    showing = WordReply(id=1, replace=2, words=("a",), phrase="ld is big", phrase_done=False, phrase_mode="auto")
    message = showing.to_message()
    assert (message["phrase"], message["phrase_done"], message["phrase_mode"]) == ("ld is big", False, "auto")

    assert PhraseUpdate(id=4, text="hi", done=True).to_message() == {"id": 4, "type": "phrase", "text": "hi", "done": True}


def test_optional_status_and_origins_are_serialized_without_changing_legacy_shape():
    from completionist_engine.protocol import PhraseUpdate

    words = WordReply(id=2, replace=3, words=("work", "worldwide"), origins=("local", "learned"),
                      phrase_state="scheduled", phrase_wait_ms=250, phrase_elapsed_ms=0, trigger_reason="idle")
    message = words.to_message()
    assert message["origins"] == ["local", "learned"]
    assert message["phrase_state"] == "scheduled" and message["phrase_wait_ms"] == 250
    assert message["phrase_elapsed_ms"] == 0 and message["trigger_reason"] == "idle"
    update = PhraseUpdate(id=2, text="", done=False, phrase_state="working", phrase_elapsed_ms=12)
    assert update.to_message()["phrase_state"] == "working"


def test_a_quiet_flag_is_parsed_and_defaults_to_false():
    from completionist_engine.protocol import parse_request

    assert parse_request({"id": 1, "event": "keystroke"}).quiet is False
    assert parse_request({"id": 1, "event": "keystroke", "quiet": True}).quiet is True
    with pytest.raises(ProtocolError):
        parse_request({"id": 1, "event": "keystroke", "quiet": "yes"})


def test_a_reply_names_the_kind_of_each_suggestion_only_when_some_are_not_plain_words():
    mixed = WordReply(id=1, replace=2, words=("know about", "know", "known"), kinds=("chunk", "word", "word"))
    assert mixed.to_message()["kinds"] == ["chunk", "word", "word"]
    plain = WordReply(id=1, replace=2, words=("know", "known"), kinds=("word", "word"))
    assert "kinds" not in plain.to_message()
    assert "kinds" not in WordReply(id=1, replace=2, words=("know",)).to_message()
    upcoming = WordReply(id=1, replace=0, words=("be", "see"), kinds=("next", "next"))
    assert upcoming.to_message()["kinds"] == ["next", "next"]


def test_a_reply_marks_the_guessed_letters_only_when_some_word_has_any():
    marked = WordReply(id=1, replace=7, words=("motion", "mountain"), marks=((), (3, 5)))
    assert marked.to_message()["marks"] == [[], [3, 5]]
    assert "marks" not in WordReply(id=1, replace=3, words=("work", "world")).to_message()
    assert "marks" not in WordReply(id=1, replace=3, words=("work",), marks=((),)).to_message()


@pytest.mark.parametrize("kind", ["word", "chunk", "next", "phrase", "phrase_word"])
def test_an_accept_event_may_name_any_suggestion_kind(kind):
    request = parse_request({"id": 1, "event": "accept", "accepted": "x", "kind": kind})
    assert request.kind == kind


def test_an_unknown_accept_kind_is_rejected():
    with pytest.raises(ProtocolError):
        parse_request({"id": 1, "event": "accept", "accepted": "x", "kind": "sentence"})


def test_popup_settings_are_optional_and_round_trip():
    popup = {"font_size": 14, "width_scale": 1.5, "partial_accept": "alt+right", "dismiss": "ctrl+backspace"}
    reply = WordReply(id=3, replace=0, words=("hello",), popup=popup)
    assert FrameDecoder().feed(encode(reply.to_message()))[0]["popup"] == popup
    assert "popup" not in WordReply(id=3, replace=0, words=()).to_message()
    assert WordReply(id=3, replace=0, words=(), popup={}).to_message()["popup"] == {}
