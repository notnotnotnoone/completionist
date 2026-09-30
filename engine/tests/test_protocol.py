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


def test_a_quiet_flag_is_parsed_and_defaults_to_false():
    from completionist_engine.protocol import parse_request

    assert parse_request({"id": 1, "event": "keystroke"}).quiet is False
    assert parse_request({"id": 1, "event": "keystroke", "quiet": True}).quiet is True
    with pytest.raises(ProtocolError):
        parse_request({"id": 1, "event": "keystroke", "quiet": "yes"})
