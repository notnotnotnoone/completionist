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

TEST(an_accept_request_can_name_a_chunk_or_next_word) {
    for (const char* kind : {"chunk", "next"}) {
        Request request;
        request.event = "accept";
        request.accepted = L"know about";
        request.kind = kind;
        CHECK(BodyOf(EncodeRequest(request)).find(std::string("\"kind\":\"") + kind + "\"") != std::string::npos);
    }
}
