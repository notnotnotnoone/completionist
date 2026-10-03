#include "../src/protocol.h"
#include "test_harness.h"

using namespace completionist::protocol;

namespace {
std::string Frame(const std::string& body) {
    std::string frame;
    auto n = static_cast<std::uint32_t>(body.size());
    for (int s = 0; s < 32; s += 8) frame += static_cast<char>((n >> s) & 0xFF);
    return frame + body;
}

std::string BodyOf(const std::string& frame) { return frame.substr(4); }
}  // namespace

TEST(utf8_round_trips_ascii_accents_and_emoji) {
    for (std::wstring text : {std::wstring(L"hello"), std::wstring(L"café naïve"), std::wstring(L"中文"),
                              std::wstring(L"party \U0001F389!")}) {
        CHECK(FromUtf8(ToUtf8(text)) == text);
    }
    CHECK_EQ(ToUtf8(L"é"), std::string("\xC3\xA9"));
    CHECK_EQ(ToUtf8(L"\U0001F389"), std::string("\xF0\x9F\x8E\x89"));
}

TEST(lone_surrogates_and_bad_utf8_become_the_replacement_character) {
    std::wstring lone = L"a";
    lone += static_cast<wchar_t>(0xD800);
    lone += L"b";
    CHECK(FromUtf8(ToUtf8(lone)) == L"a�b");
    CHECK(FromUtf8(std::string("a\xFF" "b")) == L"a�b");
    CHECK(FromUtf8(std::string("a\xC3")) == L"a�");           // truncated sequence
    CHECK(FromUtf8(std::string("\xC0\xAF")) == L"��");   // overlong '/'
}

TEST(a_request_encodes_to_a_length_prefixed_json_object) {
    Request request;
    request.id = 7;
    request.event = "keystroke";
    request.app = L"notepad.exe";
    request.title = L"Untitled";
    request.input_scope = {"IS_DEFAULT"};
    request.before = L"I'd like to";
    std::string frame = EncodeRequest(request);

    std::uint32_t length = 0;
    for (int i = 0; i < 4; ++i) length |= static_cast<std::uint32_t>(static_cast<unsigned char>(frame[i])) << (8 * i);
    CHECK_EQ(static_cast<std::size_t>(length), frame.size() - 4);
    CHECK_EQ(BodyOf(frame),
             std::string("{\"id\":7,\"event\":\"keystroke\",\"app\":\"notepad.exe\",\"title\":\"Untitled\","
                         "\"input_scope\":[\"IS_DEFAULT\"],\"before\":\"I'd like to\",\"after\":\"\"}"));
}

TEST(requests_escape_quotes_backslashes_and_control_characters) {
    Request request;
    request.id = 1;
    request.event = "keystroke";
    request.before = L"say \"hi\"\\\n\t\x01";
    std::string body = BodyOf(EncodeRequest(request));
    CHECK(body.find("\"before\":\"say \\\"hi\\\"\\\\\\n\\t\\u0001\"") != std::string::npos);
}

TEST(accept_requests_carry_the_accepted_word) {
    Request request;
    request.id = 2;
    request.event = "accept";
    request.accepted = L"recommend";
    CHECK(BodyOf(EncodeRequest(request)).find("\"accepted\":\"recommend\"") != std::string::npos);

    Request plain;
    plain.event = "keystroke";
    CHECK(BodyOf(EncodeRequest(plain)).find("accepted") == std::string::npos);
}

TEST(a_word_reply_is_parsed) {
    auto reply = ParseWordReply(R"({"id":12,"type":"words","replace":3,"words":["recommend","record"]})");
    CHECK(reply.has_value());
    CHECK_EQ(reply->id, 12u);
    CHECK_EQ(reply->replace, 3);
    CHECK_EQ(reply->words.size(), 2u);
    CHECK(reply->words[0] == L"recommend");
    CHECK(reply->words[1] == L"record");
}

TEST(a_word_reply_may_be_empty_and_keys_may_come_in_any_order_with_whitespace) {
    auto reply = ParseWordReply(R"( { "words" : [ ] , "replace":0, "type":"words", "id":1 } )");
    CHECK(reply.has_value());
    CHECK(reply->words.empty());
}

TEST(replies_decode_unicode_escapes_and_raw_utf8) {
    auto reply = ParseWordReply("{\"id\":1,\"type\":\"words\",\"replace\":1,\"words\":[\"caf\\u00e9\",\"na\xC3\xAFve\",\"\\ud83c\\udf89\"]}");
    CHECK(reply.has_value());
    CHECK(reply->words[0] == L"café");
    CHECK(reply->words[1] == L"naïve");
    CHECK(reply->words[2] == L"\U0001F389");
}

TEST(malformed_or_unrelated_replies_are_rejected) {
    CHECK(!ParseWordReply("").has_value());
    CHECK(!ParseWordReply("not json").has_value());
    CHECK(!ParseWordReply("[1,2]").has_value());
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0})").has_value());                          // no words
    CHECK(!ParseWordReply(R"({"id":1,"type":"phrase","replace":0,"words":[]})").has_value());             // other type
    CHECK(!ParseWordReply(R"({"id":"1","type":"words","replace":0,"words":[]})").has_value());            // id not a number
    CHECK(!ParseWordReply(R"({"id":-1,"type":"words","replace":0,"words":[]})").has_value());
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":[1]})").has_value());              // word not a string
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":["a"]} trailing)").has_value());
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":["unterminated]})").has_value());
}

TEST(deeply_nested_json_is_rejected_instead_of_overflowing_the_stack) {
    std::string deep(5000, '[');
    CHECK(!ParseWordReply(deep).has_value());
    std::string body = R"({"id":1,"type":"words","replace":0,"words":[],"x":)" + deep + "}";
    CHECK(!ParseWordReply(body).has_value());
}

TEST(a_decoder_returns_a_frame_that_arrives_whole) {
    FrameDecoder decoder;
    std::vector<std::string> bodies;
    std::string frame = Frame("{\"a\":1}");
    CHECK(decoder.Feed(frame.data(), frame.size(), &bodies));
    CHECK_EQ(bodies.size(), 1u);
    CHECK_EQ(bodies[0], std::string("{\"a\":1}"));
}

TEST(a_decoder_reassembles_a_frame_split_at_every_byte) {
    std::string frame = Frame("{\"hello\":\"world\"}");
    for (std::size_t split = 1; split < frame.size(); ++split) {
        FrameDecoder decoder;
        std::vector<std::string> bodies;
        CHECK(decoder.Feed(frame.data(), split, &bodies));
        CHECK(bodies.empty());
        CHECK(decoder.Feed(frame.data() + split, frame.size() - split, &bodies));
        CHECK_EQ(bodies.size(), 1u);
    }
}

TEST(a_decoder_splits_several_frames_delivered_together) {
    std::string data = Frame("{\"n\":1}") + Frame("{\"n\":2}") + Frame("{\"n\":3}");
    FrameDecoder decoder;
    std::vector<std::string> bodies;
    CHECK(decoder.Feed(data.data(), data.size(), &bodies));
    CHECK_EQ(bodies.size(), 3u);
    CHECK_EQ(bodies[2], std::string("{\"n\":3}"));
}

TEST(a_decoder_rejects_an_oversize_frame) {
    std::string header;
    std::uint32_t n = static_cast<std::uint32_t>(kMaxFrameBytes) + 1;
    for (int s = 0; s < 32; s += 8) header += static_cast<char>((n >> s) & 0xFF);
    FrameDecoder decoder;
    std::vector<std::string> bodies;
    CHECK(!decoder.Feed(header.data(), header.size(), &bodies));
}

TEST(the_encoded_frame_matches_what_the_python_engine_expects) {
    // Same shape as engine/tests/test_protocol.py: 4-byte little-endian length, then compact JSON.
    Request request;
    request.id = 258;  // 0x102, so the id itself is multi-digit
    request.event = "keystroke";
    std::string frame = EncodeRequest(request);
    CHECK_EQ(static_cast<unsigned char>(frame[1]), 0u);
    CHECK_EQ(static_cast<std::size_t>(static_cast<unsigned char>(frame[0])), frame.size() - 4);
}

TEST(a_words_reply_may_carry_a_phrase_and_the_phrase_mode) {
    auto reply = ParseWordReply(R"({"id":4,"type":"words","replace":3,"words":["world"],"phrase_mode":"auto","phrase":"d is big","phrase_done":false})");
    CHECK(reply.has_value());
    CHECK(reply->kind == ReplyKind::Words);
    CHECK(reply->phrase == L"d is big");
    CHECK(!reply->phrase_done);
    CHECK_EQ(reply->phrase_mode, std::string("auto"));
}

TEST(a_plain_words_reply_has_no_phrase_and_phrases_off) {
    auto reply = ParseWordReply(R"({"id":4,"type":"words","replace":3,"words":["world"]})");
    CHECK(reply.has_value());
    CHECK(reply->phrase.empty());
    CHECK(reply->phrase_done);
    CHECK_EQ(reply->phrase_mode, std::string("off"));
}

TEST(a_phrase_push_is_parsed) {
    auto push = ParseWordReply(R"({"id":9,"type":"phrase","text":"ld is big","done":true})");
    CHECK(push.has_value());
    CHECK(push->kind == ReplyKind::Phrase);
    CHECK_EQ(push->id, 9u);
    CHECK(push->phrase == L"ld is big");
    CHECK(push->phrase_done);
    auto partial = ParseWordReply(R"({"id":9,"type":"phrase","text":"","done":false})");
    CHECK(partial.has_value() && partial->phrase.empty() && !partial->phrase_done);
}

TEST(malformed_phrase_messages_and_modes_are_rejected) {
    CHECK(!ParseWordReply(R"({"id":9,"type":"phrase","text":5,"done":true})").has_value());
    CHECK(!ParseWordReply(R"({"id":9,"type":"phrase","text":"x"})").has_value());
    CHECK(!ParseWordReply(R"({"id":9,"type":"phrase","text":"x","done":"yes"})").has_value());
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":[],"phrase_mode":"sometimes"})").has_value());
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":[],"phrase":3})").has_value());
}

TEST(a_quiet_request_says_so) {
    Request request;
    request.id = 3;
    request.event = "keystroke";
    request.quiet = true;
    CHECK(BodyOf(EncodeRequest(request)).find("\"quiet\":true") != std::string::npos);
    Request loud;
    loud.event = "keystroke";
    CHECK(BodyOf(EncodeRequest(loud)).find("quiet") == std::string::npos);
}

TEST(an_accept_request_can_name_its_kind) {
    Request request;
    request.event = "accept";
    request.kind = "phrase_word";
    request.accepted = L"the ";
    CHECK(BodyOf(EncodeRequest(request)).find("\"kind\":\"phrase_word\"") != std::string::npos);
    Request plain;
    plain.event = "accept";
    CHECK(BodyOf(EncodeRequest(plain)).find("kind") == std::string::npos);
}

TEST(a_words_reply_names_the_kind_of_each_suggestion) {
    auto reply = ParseWordReply(R"({"id":4,"type":"words","replace":2,"words":["know about","know","known"],"kinds":["chunk","word","word"]})");
    CHECK(reply.has_value());
    CHECK_EQ(reply->kinds.size(), 3u);
    CHECK_EQ(reply->kinds[0], std::string("chunk"));
    CHECK_EQ(reply->kinds[1], std::string("word"));
    auto next = ParseWordReply(R"({"id":4,"type":"words","replace":0,"words":["be"],"kinds":["next"]})");
    CHECK(next.has_value() && next->kinds.size() == 1 && next->kinds[0] == "next");
}

TEST(a_words_reply_without_kinds_means_every_suggestion_is_a_word) {
    auto reply = ParseWordReply(R"({"id":4,"type":"words","replace":3,"words":["world","work"]})");
    CHECK(reply.has_value());
    CHECK_EQ(reply->kinds.size(), 2u);
    CHECK_EQ(reply->kinds[0], std::string("word"));
    CHECK_EQ(reply->kinds[1], std::string("word"));
    auto none = ParseWordReply(R"({"id":4,"type":"words","replace":0,"words":[]})");
    CHECK(none.has_value() && none->kinds.empty());
}

TEST(malformed_kinds_are_rejected) {
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":["a","b"],"kinds":["word"]})").has_value());         // too few
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":["a"],"kinds":["sentence"]})").has_value());         // unknown
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":["a"],"kinds":[1]})").has_value());                  // not text
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":["a"],"kinds":"word"})").has_value());               // not a list
}

TEST(a_words_reply_may_mark_the_guessed_letters_of_each_suggestion) {
    auto reply = ParseWordReply(R"({"id":4,"type":"words","replace":7,"words":["motion","mountain"],"marks":[[],[3,5]]})");
    CHECK(reply.has_value());
    CHECK(reply->marks.size() == 2);
    CHECK(reply->marks[0].empty());
    CHECK_EQ(reply->marks[1].size(), 2u);
    CHECK_EQ(reply->marks[1][0], 3);
    CHECK_EQ(reply->marks[1][1], 5);
    auto none = ParseWordReply(R"({"id":4,"type":"words","replace":3,"words":["work"]})");
    CHECK(none.has_value() && none->marks.empty());
}

TEST(malformed_marks_are_rejected) {
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":["a","b"],"marks":[[]]})").has_value());       // too few
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":["ab"],"marks":[[0,2]]})").has_value());      // past the end
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":["ab"],"marks":[[-1]]})").has_value());       // negative
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":["ab"],"marks":[[0.5]]})").has_value());      // not whole
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":["ab"],"marks":[0]})").has_value());          // not a list
    CHECK(!ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":["ab"],"marks":"x"})").has_value());          // not a list
}

TEST(optional_phrase_status_and_origins_are_parsed_without_affecting_words) {
    auto reply = ParseWordReply(R"({"id":4,"type":"words","replace":3,"words":["Work","worldwide"],"phrase_state":"streaming","phrase_wait_ms":0,"phrase_elapsed_ms":123,"trigger_reason":"idle","origins":["local","learned"]})");
    CHECK(reply.has_value());
    CHECK_EQ(reply->phrase_state, std::string("streaming"));
    CHECK(reply->phrase_wait_ms.has_value() && *reply->phrase_wait_ms == 0);
    CHECK(reply->phrase_elapsed_ms.has_value() && *reply->phrase_elapsed_ms == 123);
    CHECK_EQ(reply->trigger_reason, std::string("idle"));
    CHECK_EQ(reply->origins.size(), 2u);
    CHECK_EQ(reply->origins[1], std::string("learned"));

    auto legacy = ParseWordReply(R"({"id":4,"type":"words","replace":3,"words":["world"]})");
    CHECK(legacy.has_value() && legacy->origins.empty() && legacy->phrase_state.empty());
    auto bad_origin_count = ParseWordReply(R"({"id":4,"type":"words","replace":3,"words":["world","work"],"origins":["learned"]})");
    CHECK(bad_origin_count.has_value() && bad_origin_count->words.size() == 2 && bad_origin_count->origins.empty());
    auto bad_origin_value = ParseWordReply(R"({"id":4,"type":"words","replace":3,"words":["world"],"origins":["ai"]})");
    CHECK(bad_origin_value.has_value() && bad_origin_value->words.size() == 1 && bad_origin_value->origins.empty());
}

TEST(oversized_or_malformed_optional_metadata_does_not_discard_engine_words) {
    std::string oversizedOrigins = R"({"id":4,"type":"words","replace":3,"words":["world"],"origins":[)";
    for (int i = 0; i < 4097; ++i) {
        if (i) oversizedOrigins.push_back(',');
        oversizedOrigins += "\"learned\"";
    }
    oversizedOrigins += "]}";
    auto oversized = ParseWordReply(oversizedOrigins);
    CHECK(oversized.has_value());
    CHECK_EQ(oversized->words.size(), 1u);
    CHECK(oversized->origins.empty());

    std::string malformedOrigins = R"({"id":5,"type":"words","replace":3,"words":["work"],"origins":[)";
    for (int i = 0; i < 4097; ++i) {
        if (i) malformedOrigins.push_back(',');
        malformedOrigins += (i == 4096) ? "42" : "\"local\"";
    }
    malformedOrigins += "]}";
    auto malformed = ParseWordReply(malformedOrigins);
    CHECK(malformed.has_value());
    CHECK_EQ(malformed->words.size(), 1u);
    CHECK(malformed->origins.empty());

    std::string unknownMetadata = R"({"id":6,"type":"words","replace":3,"words":["word"],"future":{"items":[)";
    for (int i = 0; i < 4097; ++i) {
        if (i) unknownMetadata.push_back(',');
        unknownMetadata += "null";
    }
    unknownMetadata += "]}}";
    auto unknown = ParseWordReply(unknownMetadata);
    CHECK(unknown.has_value());
    CHECK_EQ(unknown->words.size(), 1u);
    CHECK(unknown->words[0] == L"word");

    std::string oversizedUnknownObject = R"({"id":7,"type":"words","replace":3,"words":["words"],"future_object":{)";
    for (int i = 0; i < 4097; ++i) {
        if (i) oversizedUnknownObject.push_back(',');
        oversizedUnknownObject += "\"field" + std::to_string(i) + "\":null";
    }
    oversizedUnknownObject += "}}";
    auto unknownObject = ParseWordReply(oversizedUnknownObject);
    CHECK(unknownObject.has_value());
    CHECK_EQ(unknownObject->words.size(), 1u);
    CHECK(unknownObject->words[0] == L"words");
}

TEST(engine_required_fields_after_many_unknown_root_fields_are_preserved) {
    std::string body = "{";
    for (int i = 0; i < 4100; ++i) {
        if (i) body.push_back(',');
        body += "\"unknown" + std::to_string(i) + "\":null";
    }
    body += R"(,"id":17,"type":"words","replace":2,"words":["after"],"origins":["learned"]})";

    auto reply = ParseWordReply(body);
    CHECK(reply.has_value());
    CHECK_EQ(reply->id, 17u);
    CHECK_EQ(reply->words.size(), 1u);
    CHECK(reply->words[0] == L"after");
    CHECK_EQ(reply->origins.size(), 1u);
    CHECK_EQ(reply->origins[0], std::string("learned"));

    body.insert(body.size() - 1, R"(,"id":18)");
    CHECK(!ParseWordReply(body).has_value());
}

TEST(optional_status_rejects_invalid_durations_and_unknown_state_safely) {
    for (const char* invalid : {"true", "-1", "600001", "1.5", "1e999"}) {
        std::string body = R"({"id":4,"type":"words","replace":0,"words":["a"],"phrase_wait_ms":)" + std::string(invalid) + "}";
        auto reply = ParseWordReply(body);
        CHECK(reply.has_value() && reply->words.size() == 1 && !reply->phrase_wait_ms.has_value());
    }
    auto unknown = ParseWordReply(R"({"id":4,"type":"words","replace":0,"words":["a"],"phrase_state":"simulating"})");
    CHECK(unknown.has_value() && unknown->phrase_state == "unavailable" && unknown->words.size() == 1);
    auto push = ParseWordReply(R"({"id":9,"type":"phrase","text":"","done":false,"phrase_state":"working","phrase_elapsed_ms":88})");
    CHECK(push.has_value() && push->kind == ReplyKind::Phrase && push->phrase_state == "working");
    CHECK(push->phrase_elapsed_ms.has_value() && *push->phrase_elapsed_ms == 88);
}

TEST(an_accept_request_can_name_a_chunk_or_next_word) {
    for (const char* kind : {"chunk", "next"}) {
        Request request;
        request.event = "accept";
        request.accepted = L"know about";
        request.kind = kind;
        CHECK(BodyOf(EncodeRequest(request)).find(std::string("\"kind\":\"") + kind + "\"") != std::string::npos);
    }
}

TEST(popup_settings_parse_and_legacy_defaults) {
    auto legacy = ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":[]})");
    CHECK(legacy.has_value());
    CHECK_EQ(legacy->popup.font_size, 9);
    CHECK_EQ(legacy->popup.width_scale, 1.0);
    auto reply = ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":[],"popup":{"font_size":24,"width_scale":0.5,"partial_accept":"ctrl+tab","dismiss":"alt+backspace"}})");
    CHECK(reply.has_value());
    CHECK_EQ(reply->popup.font_size, 24);
    CHECK_EQ(reply->popup.width_scale, 0.5);
    CHECK_EQ(reply->popup.partial_accept, completionist::PartialAccept::CtrlTab);
    CHECK_EQ(reply->popup.dismiss, completionist::DismissShortcut::AltBackspace);
    auto lower = ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":[],"popup":{"font_size":7,"width_scale":2,"partial_accept":"ctrl+right","dismiss":"escape"}})");
    CHECK(lower.has_value());
    CHECK_EQ(lower->popup.font_size, 7);
    CHECK_EQ(lower->popup.width_scale, 2.0);
    CHECK_EQ(lower->popup.partial_accept, completionist::PartialAccept::CtrlRight);
    CHECK_EQ(lower->popup.dismiss, completionist::DismissShortcut::Escape);
}

TEST(malformed_popup_fields_keep_safe_defaults_independently) {
    for (const std::string& popup : {std::string("null"), std::string("[]"), std::string("false"),
        std::string(R"({"font_size":true,"width_scale":false,"partial_accept":1,"dismiss":"unknown"})"),
        std::string(R"({"font_size":25,"width_scale":0.49})"),
        std::string(R"({"font_size":6,"width_scale":2.1})"),
        std::string(R"({"font_size":1e999,"width_scale":-1e999})"),
        std::string(R"({"font_size":9.5,"width_scale":"1.5"})")}) {
        auto reply = ParseWordReply("{\"id\":1,\"type\":\"words\",\"replace\":0,\"words\":[],\"popup\":" + popup + "}");
        CHECK(reply.has_value());
        CHECK_EQ(reply->popup.font_size, 9);
        CHECK_EQ(reply->popup.width_scale, 1.0);
        CHECK_EQ(reply->popup.partial_accept, completionist::PartialAccept::CtrlRight);
        CHECK_EQ(reply->popup.dismiss, completionist::DismissShortcut::Escape);
    }
    auto mixed = ParseWordReply(R"({"id":1,"type":"words","replace":0,"words":[],"popup":{"font_size":"bad","width_scale":2,"partial_accept":"alt+right","dismiss":"ctrl+backspace"}})");
    CHECK(mixed.has_value());
    CHECK_EQ(mixed->popup.font_size, 9);
    CHECK_EQ(mixed->popup.width_scale, 2.0);
    CHECK_EQ(mixed->popup.partial_accept, completionist::PartialAccept::AltRight);
    CHECK_EQ(mixed->popup.dismiss, completionist::DismissShortcut::CtrlBackspace);
}
