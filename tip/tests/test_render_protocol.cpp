#include "../src/render_protocol.h"
#include "../src/protocol.h"
#include "test_harness.h"

#include <limits>

using namespace completionist::render;

namespace {
Snapshot Sample() {
    Snapshot s{};
    s.owner = {42, 0x1234, "test-session", 2};
    s.revision = 8;
    s.caret = {-1920, -40, -1918, -20};
    s.words = {{L"café", "local", {1}}, {L"party 🎉", "learned", {}}};
    s.selection = 0;
    s.typedFragment = L"ca";
    s.phrase = L"fé and tea";
    s.phraseLead = L"ca";
    s.partialBegin = 2;
    s.partialLength = 2;
    s.ai = AiState::Ready;
    s.waitMs = 120;
    s.elapsedMs = 88;
    s.triggerReason = "idle";
    s.engineConnected = true;
    s.settings.font_size = 12;
    s.settings.width_scale = 1.25;
    s.settings.partial_accept = completionist::PartialAccept::AltRight;
    s.settings.dismiss = completionist::DismissShortcut::CtrlBackspace;
    return s;
}
std::string Body(const std::string& frame) { return frame.substr(4); }
std::string With(std::string body, std::string addition) {
    auto at = body.rfind('}');
    body.insert(at, "," + addition);
    return body;
}
}  // namespace

TEST(render_show_round_trips_schema_and_utf8_values) {
    Snapshot input = Sample();
    const std::string frame = EncodeShow(input);
    CHECK(frame.size() > 4);
    uint32_t length = 0;
    for (unsigned i = 0; i < 4; ++i) length |= static_cast<uint32_t>(static_cast<unsigned char>(frame[i])) << (i * 8);
    CHECK_EQ(length, frame.size() - 4);
    auto parsed = ParseShow(Body(frame));
    CHECK(parsed.has_value());
    CHECK(SameIdentity(parsed->owner, input.owner));
    CHECK_EQ(parsed->revision, 8u);
    CHECK_EQ(parsed->caret.left, -1920);
    CHECK_EQ(parsed->caret.top, -40);
    CHECK(parsed->words[0].text == L"café");
    CHECK(parsed->words[1].text == L"party 🎉");
    CHECK_EQ(parsed->settings.width_scale, 1.25);
    CHECK_EQ(parsed->settings.dismiss, completionist::DismissShortcut::CtrlBackspace);
}

TEST(render_ack_requires_matching_owner_generation_and_revision) {
    Snapshot current = Sample();
    auto ack = ParseAck(R"({"schema":1,"type":"ack","owner":{"pid":42,"hwnd":4660,"session":"test-session","generation":2},"revision":8,"presented":true})");
    CHECK(ack.has_value() && IsCurrentAck(current, *ack));
    auto stale = ParseAck(R"({"schema":1,"type":"ack","owner":{"pid":42,"hwnd":4660,"session":"test-session","generation":1},"revision":99,"presented":true})");
    CHECK(stale.has_value() && !IsCurrentAck(current, *stale));
    auto oldRevision = ParseAck(R"({"schema":1,"type":"ack","owner":{"pid":42,"hwnd":4660,"session":"test-session","generation":2},"revision":7,"presented":true})");
    CHECK(oldRevision.has_value() && !IsCurrentAck(current, *oldRevision));
    auto unavailable = ParseAck(R"({"schema":1,"type":"ack","owner":{"pid":42,"hwnd":4660,"session":"test-session","generation":2},"revision":8,"presented":false})");
    CHECK(unavailable.has_value() && !IsCurrentAck(current, *unavailable));
}

TEST(render_parser_replaces_invalid_utf8_and_keeps_negative_coordinates) {
    Snapshot input = Sample();
    input.words = {{std::wstring(L"x�y"), "local", {}}};
    auto body = Body(EncodeShow(input));
    auto parsed = ParseShow(body);
    CHECK(parsed.has_value());
    CHECK(parsed->words[0].text == L"x�y");
    CHECK_EQ(parsed->caret.left, -1920);
    const auto field = body.find("\"typed_fragment\":\"ca\"");
    CHECK(field != std::string::npos);
    body.replace(field, std::string("\"typed_fragment\":\"ca\"").size(), "\"typed_fragment\":\"c\xFF\"");
    auto replacement = ParseShow(body);
    CHECK(replacement.has_value());
    CHECK(replacement->typedFragment == L"c�");
}

TEST(render_parser_ignores_unknown_optional_fields) {
    auto body = With(Body(EncodeShow(Sample())), R"("future_flag":{"x":true})");
    CHECK(ParseShow(body).has_value());
}

TEST(render_parser_rejects_truncated_malformed_and_wrong_schema_messages) {
    CHECK(!ParseShow("").has_value());
    CHECK(!ParseShow(R"({"schema":1,"type":"show")").has_value());
    CHECK(!ParseShow(R"({"schema":2,"type":"show"})").has_value());
    auto truncated = Body(EncodeShow(Sample()));
    truncated.pop_back();
    CHECK(!ParseShow(truncated).has_value());
    CHECK(!ParseAck(R"({"schema":1,"type":"ack","owner":{},"revision":1,"presented":true})").has_value());
    CHECK(!ParseShow(R"({"schema":1,"schema":1,"type":"show"})").has_value());
    CHECK(!ParseShow(R"({"schema":1,"type":"show","owner":{"pid":1,"hwnd":+1,"session":"x","generation":1}})").has_value());
}

TEST(render_parser_enforces_frame_and_bounded_array_limits) {
    CHECK(!ParseShow(std::string(kMaxFrameBytes + 1, ' ')).has_value());
    Snapshot tooMany = Sample();
    tooMany.words.resize(kMaxCandidates + 1, {L"x", "local", {}});
    CHECK(EncodeShow(tooMany).empty());
    auto body = Body(EncodeShow(Sample()));
    const auto words = body.find("\"words\":[");
    CHECK(words != std::string::npos);
    std::string many = "[";
    for (std::size_t i = 0; i <= kMaxCandidates; ++i) {
        if (i) many.push_back(',');
        many += R"({"text":"x","origin":"local","marks":[]})";
    }
    many.push_back(']');
    const auto start = words + 8;
    const auto end = body.find(']', start);
    body.replace(start, end - start + 1, many);
    CHECK(!ParseShow(body).has_value());

    std::string beyondParserCap = "[";
    for (std::size_t i = 0; i <= 4096; ++i) {
        if (i) beyondParserCap.push_back(',');
        beyondParserCap += R"({"text":"x","origin":"local","marks":[]})";
    }
    beyondParserCap.push_back(']');
    auto oversizedBody = Body(EncodeShow(Sample()));
    const auto oversizedWords = oversizedBody.find("\"words\":[");
    const auto oversizedStart = oversizedWords + 8;
    const auto oversizedEnd = oversizedBody.find(']', oversizedStart);
    oversizedBody.replace(oversizedStart, oversizedEnd - oversizedStart + 1, beyondParserCap);
    CHECK(!ParseShow(oversizedBody).has_value());

    Snapshot tooManyMarks = Sample();
    tooManyMarks.words = {{L"x", "local", {}}};
    for (std::size_t i = 0; i < kMaxMarksPerCandidate + 1; ++i) tooManyMarks.words[0].marks.push_back(static_cast<int>(i));
    CHECK(EncodeShow(tooManyMarks).empty());
    Snapshot tooMuchText = Sample();
    tooMuchText.words = {{std::wstring(kMaxTextBytes + 1, L'x'), "local", {}}};
    CHECK(EncodeShow(tooMuchText).empty());
}

TEST(render_encoder_replaces_invalid_utf8_metadata) {
    Snapshot input = Sample();
    input.owner.session = std::string("id\xFF", 3);
    input.triggerReason = std::string("reason\xFF", 7);
    auto parsed = ParseShow(Body(EncodeShow(input)));
    CHECK(parsed.has_value());
    CHECK(parsed->owner.session == "id\xEF\xBF\xBD");
    CHECK(parsed->triggerReason == "reason\xEF\xBF\xBD");
}

TEST(render_encoder_never_emits_text_or_settings_rejected_by_parser) {
    Snapshot textTooLong = Sample();
    textTooLong.typedFragment.assign(kMaxTextBytes + 1, L'x');
    CHECK(EncodeShow(textTooLong).empty());
    Snapshot phraseTooLong = Sample();
    phraseTooLong.phrase.assign(kMaxTextBytes + 1, L'x');
    CHECK(EncodeShow(phraseTooLong).empty());
    Snapshot invalidSettings = Sample();
    invalidSettings.settings.width_scale = std::numeric_limits<double>::quiet_NaN();
    CHECK(EncodeShow(invalidSettings).empty());
    invalidSettings.settings.width_scale = std::numeric_limits<double>::infinity();
    CHECK(EncodeShow(invalidSettings).empty());
    invalidSettings.settings.width_scale = 2.01;
    CHECK(EncodeShow(invalidSettings).empty());
    invalidSettings.settings.width_scale = 1.0;
    invalidSettings.settings.font_size = 6;
    CHECK(EncodeShow(invalidSettings).empty());
    invalidSettings.settings.font_size = 25;
    CHECK(EncodeShow(invalidSettings).empty());
    Snapshot invalidUtf8Limit = Sample();
    invalidUtf8Limit.owner.session = std::string(128, '\xFF');
    CHECK(EncodeShow(invalidUtf8Limit).empty());
    invalidUtf8Limit = Sample();
    invalidUtf8Limit.triggerReason = std::string(256, '\xFF');
    CHECK(EncodeShow(invalidUtf8Limit).empty());
    Snapshot invalidDuration = Sample();
    invalidDuration.waitMs = 600001;
    CHECK(EncodeShow(invalidDuration).empty());
    invalidDuration = Sample();
    invalidDuration.elapsedMs = 600001;
    CHECK(EncodeShow(invalidDuration).empty());
    Snapshot maximumDuration = Sample();
    maximumDuration.waitMs = 600000;
    maximumDuration.elapsedMs = 600000;
    CHECK(ParseShow(Body(EncodeShow(maximumDuration))).has_value());
}

TEST(render_protocol_preserves_uint64_identity_and_revision_exactly) {
    Snapshot input = Sample();
    input.owner.hostHwnd = UINT64_MAX;
    input.owner.generation = UINT64_MAX;
    input.revision = UINT64_MAX;
    auto encoded = EncodeShow(input);
    auto parsed = ParseShow(Body(encoded));
    CHECK(parsed.has_value());
    CHECK_EQ(parsed->owner.hostHwnd, UINT64_MAX);
    CHECK_EQ(parsed->owner.generation, UINT64_MAX);
    CHECK_EQ(parsed->revision, UINT64_MAX);

    const auto replace = [](std::string body, std::string_view from, std::string_view to) {
        const auto position = body.find(from);
        if (position != std::string::npos) body.replace(position, from.size(), to);
        return body;
    };
    auto body = Body(EncodeShow(Sample()));
    CHECK(!ParseShow(replace(body, "\"revision\":8", "\"revision\":18446744073709551616")).has_value());
    CHECK(!ParseShow(replace(body, "\"revision\":8", "\"revision\":8.5")).has_value());
    CHECK(!ParseShow(replace(body, "\"revision\":8", "\"revision\":true")).has_value());

    auto maximumAck = ParseAck(R"({"schema":1,"type":"ack","owner":{"pid":42,"hwnd":18446744073709551615,"session":"test-session","generation":18446744073709551615},"revision":18446744073709551615,"presented":true})");
    CHECK(maximumAck.has_value());
    CHECK_EQ(maximumAck->owner.hostHwnd, UINT64_MAX);
    CHECK_EQ(maximumAck->owner.generation, UINT64_MAX);
    CHECK_EQ(maximumAck->revision, UINT64_MAX);
    input.owner = maximumAck->owner;
    CHECK(IsCurrentAck(input, *maximumAck));
    CHECK(!ParseAck(R"({"schema":1,"type":"ack","owner":{"pid":42,"hwnd":1,"session":"x","generation":1},"revision":18446744073709551616,"presented":true})").has_value());
}

TEST(render_selection_supports_phrase_only_and_empty_pending_shelf) {
    Snapshot phraseOnly = Sample();
    phraseOnly.words.clear();
    phraseOnly.selection = -1;
    CHECK(ParseShow(Body(EncodeShow(phraseOnly))).has_value());
    Snapshot shelf = Sample();
    shelf.words.clear();
    shelf.phrase.clear();
    shelf.partialBegin = 0;
    shelf.partialLength = 0;
    shelf.selection = -2;
    shelf.ai = AiState::Working;
    CHECK(ParseShow(Body(EncodeShow(shelf))).has_value());
    shelf.selection = 0;
    CHECK(EncodeShow(shelf).empty());
}

TEST(render_hide_and_heartbeat_commands_are_validated) {
    auto hide = ParseCommand(R"({"schema":1,"command":"hide","owner":{"pid":42,"hwnd":4660,"session":"test-session","generation":2},"revision":9})");
    CHECK(hide.has_value());
    CHECK(hide->command == Command::Hide);
    CHECK_EQ(hide->revision, 9u);
    auto heartbeat = ParseCommand(R"({"schema":1,"command":"heartbeat","owner":{"pid":42,"hwnd":4660,"session":"test-session","generation":2}})");
    CHECK(heartbeat.has_value());
    CHECK(heartbeat->command == Command::Heartbeat);
    CHECK(!ParseCommand(R"({"schema":1,"command":"hide","owner":{"pid":42,"hwnd":4660,"session":"test-session","generation":2}})").has_value());
    CHECK(!ParseCommand(R"({"schema":2,"command":"heartbeat","owner":{"pid":42,"hwnd":4660,"session":"test-session","generation":2}})").has_value());
    CHECK(!ParseCommand(R"({"schema":1,"command":"show","owner":{"pid":42,"hwnd":4660,"session":"test-session","generation":2}})").has_value());
}

TEST(render_ack_encoder_emits_bounded_valid_framed_acknowledgement) {
    Ack ack{Sample().owner, 8, true};
    const auto frame = EncodeAck(ack);
    CHECK(frame.size() > 4);
    uint32_t size = 0;
    for (unsigned i = 0; i < 4; ++i) size |= static_cast<uint32_t>(static_cast<unsigned char>(frame[i])) << (i * 8);
    CHECK_EQ(size, frame.size() - 4);
    auto parsed = ParseAck(Body(frame));
    CHECK(parsed.has_value());
    CHECK(IsCurrentAck(Sample(), *parsed));
    ack.owner.pid = 0;
    CHECK(EncodeAck(ack).empty());
}
