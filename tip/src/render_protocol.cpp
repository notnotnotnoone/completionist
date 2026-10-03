#include "render_protocol.h"

#include <cmath>
#include <charconv>
#include <limits>

#include "json_internal.h"
#include "protocol.h"

namespace completionist::render {
namespace {

using completionist::protocol::detail::Json;
using completionist::protocol::detail::Parser;

void JsonString(std::string& out, std::string_view value) {
    out.push_back('"');
    for (unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    constexpr char hex[] = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(hex[c >> 4]);
                    out.push_back(hex[c & 0x0f]);
                } else out.push_back(static_cast<char>(c));
        }
    }
    out.push_back('"');
}

std::string CleanUtf8(std::string_view value) {
    return completionist::protocol::ToUtf8(completionist::protocol::FromUtf8(value));
}

void AddIdentity(std::string& out, const Identity& id) {
    out += "{\"pid\":" + std::to_string(id.pid) + ",\"hwnd\":" + std::to_string(id.hostHwnd) + ",\"session\":";
    JsonString(out, CleanUtf8(id.session));
    out += ",\"generation\":" + std::to_string(id.generation) + "}";
}

void AddSettings(std::string& out, const completionist::PopupSettings& settings) {
    out += "{\"font_size\":" + std::to_string(settings.font_size) + ",\"width_scale\":" + std::to_string(settings.width_scale) + ",\"partial_accept\":";
    JsonString(out, settings.partial_accept == completionist::PartialAccept::AltRight ? "alt+right" :
                      settings.partial_accept == completionist::PartialAccept::CtrlTab ? "ctrl+tab" : "ctrl+right");
    out += ",\"dismiss\":";
    JsonString(out, settings.dismiss == completionist::DismissShortcut::CtrlBackspace ? "ctrl+backspace" :
                      settings.dismiss == completionist::DismissShortcut::AltBackspace ? "alt+backspace" : "escape");
    out.push_back('}');
}

bool Integer(const Json* value, uint64_t min, uint64_t max, uint64_t* out) {
    if (!value || value->type != Json::Type::Number || value->rawNumber.empty()) return false;
    uint64_t parsed = 0;
    const auto result = std::from_chars(value->rawNumber.data(), value->rawNumber.data() + value->rawNumber.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value->rawNumber.data() + value->rawNumber.size() || parsed < min || parsed > max) return false;
    *out = parsed;
    return true;
}

bool SignedInteger(const Json* value, int64_t min, int64_t max, int64_t* out) {
    if (!value || value->type != Json::Type::Number || value->rawNumber.empty()) return false;
    int64_t parsed = 0;
    const auto result = std::from_chars(value->rawNumber.data(), value->rawNumber.data() + value->rawNumber.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value->rawNumber.data() + value->rawNumber.size() || parsed < min || parsed > max) return false;
    *out = parsed;
    return true;
}

bool String(const Json* value, std::string* out, std::size_t maxBytes = kMaxTextBytes) {
    if (!value || value->type != Json::Type::String || value->string.size() > maxBytes) return false;
    *out = value->string;
    return true;
}

bool Boolean(const Json* value, bool* out) {
    if (!value || value->type != Json::Type::Bool) return false;
    *out = value->boolean;
    return true;
}

bool ParseIdentity(const Json* value, Identity* out) {
    if (!value || value->type != Json::Type::Object) return false;
    uint64_t number = 0;
    if (!Integer(value->Find("pid"), 1, UINT32_MAX, &number)) return false;
    out->pid = static_cast<uint32_t>(number);
    if (!Integer(value->Find("hwnd"), 1, UINT64_MAX, &out->hostHwnd)) return false;
    if (!String(value->Find("session"), &out->session, kMaxSessionBytes) || out->session.empty()) return false;
    if (!Integer(value->Find("generation"), 1, UINT64_MAX, &out->generation)) return false;
    return true;
}

const char* AiName(AiState state) {
    switch (state) {
        case AiState::Off: return "off";
        case AiState::Manual: return "manual";
        case AiState::Scheduled: return "scheduled";
        case AiState::Working: return "working";
        case AiState::Streaming: return "streaming";
        case AiState::Ready: return "ready";
        case AiState::Unavailable: return "unavailable";
    }
    return "unavailable";
}

bool ParseAi(std::string_view value, AiState* out) {
    if (value == "off") *out = AiState::Off;
    else if (value == "manual") *out = AiState::Manual;
    else if (value == "scheduled") *out = AiState::Scheduled;
    else if (value == "working") *out = AiState::Working;
    else if (value == "streaming") *out = AiState::Streaming;
    else if (value == "ready") *out = AiState::Ready;
    else if (value == "unavailable") *out = AiState::Unavailable;
    else return false;
    return true;
}

bool ParseSettings(const Json* value, completionist::PopupSettings* out) {
    if (!value || value->type != Json::Type::Object) return false;
    int64_t font = 0;
    if (!SignedInteger(value->Find("font_size"), 7, 24, &font)) return false;
    out->font_size = static_cast<int>(font);
    const Json* width = value->Find("width_scale");
    if (!width || width->type != Json::Type::Number || !std::isfinite(width->number) || width->number < .5 || width->number > 2) return false;
    out->width_scale = width->number;
    std::string partial, dismiss;
    if (!String(value->Find("partial_accept"), &partial, 32) || !String(value->Find("dismiss"), &dismiss, 32)) return false;
    if (partial == "ctrl+right") out->partial_accept = completionist::PartialAccept::CtrlRight;
    else if (partial == "alt+right") out->partial_accept = completionist::PartialAccept::AltRight;
    else if (partial == "ctrl+tab") out->partial_accept = completionist::PartialAccept::CtrlTab;
    else return false;
    if (dismiss == "escape") out->dismiss = completionist::DismissShortcut::Escape;
    else if (dismiss == "ctrl+backspace") out->dismiss = completionist::DismissShortcut::CtrlBackspace;
    else if (dismiss == "alt+backspace") out->dismiss = completionist::DismissShortcut::AltBackspace;
    else return false;
    return true;
}

std::optional<Json> Document(std::string_view body) {
    if (body.size() > kMaxFrameBytes) return std::nullopt;
    auto value = Parser(body).ParseDocument({
        "schema", "type", "owner", "revision", "caret", "words", "selection", "typed_fragment", "phrase",
        "phrase_lead", "partial_begin", "partial_length", "ai", "wait_ms", "elapsed_ms", "trigger_reason",
        "engine_connected", "settings", "presented"});
    if (!value || value->type != Json::Type::Object) return std::nullopt;
    return value;
}

bool Frame(std::string body, std::string* result) {
    if (body.size() > kMaxFrameBytes) return false;
    result->clear();
    uint32_t length = static_cast<uint32_t>(body.size());
    for (unsigned shift = 0; shift < 32; shift += 8) result->push_back(static_cast<char>((length >> shift) & 0xff));
    result->append(body);
    return true;
}

}  // namespace

bool SameIdentity(const Identity& a, const Identity& b) {
    return a.pid == b.pid && a.hostHwnd == b.hostHwnd && a.session == b.session && a.generation == b.generation;
}

std::string EncodeShow(const Snapshot& s) {
    if (s.owner.pid == 0 || s.owner.hostHwnd == 0 || s.owner.generation == 0 || s.revision == 0 ||
        s.owner.session.empty() || s.owner.session.size() > kMaxSessionBytes ||
        s.words.size() > kMaxCandidates || s.triggerReason.size() > 256) return {};
    const bool hasPhrase = !s.phrase.empty();
    if ((s.selection == -1 && !hasPhrase) || (s.selection >= 0 && static_cast<std::size_t>(s.selection) >= s.words.size()) ||
        s.selection < -2 || static_cast<uint64_t>(s.partialBegin) + s.partialLength > s.phrase.size()) return {};
    std::string body = "{\"schema\":1,\"type\":\"show\",\"owner\":";
    AddIdentity(body, s.owner);
    body += ",\"revision\":" + std::to_string(s.revision) + ",\"caret\":[" + std::to_string(s.caret.left) + "," +
            std::to_string(s.caret.top) + "," + std::to_string(s.caret.right) + "," + std::to_string(s.caret.bottom) +
            "],\"words\":[";
    for (std::size_t i = 0; i < s.words.size(); ++i) {
        if (i) body.push_back(',');
        const auto& word = s.words[i];
        if (word.text.size() * 4 > kMaxTextBytes || word.marks.size() > kMaxMarksPerCandidate ||
            (word.origin != "local" && word.origin != "learned")) return {};
        body += "{\"text\":";
        JsonString(body, completionist::protocol::ToUtf8(word.text));
        body += ",\"origin\":";
        JsonString(body, word.origin);
        body += ",\"marks\":[";
        for (std::size_t m = 0; m < word.marks.size(); ++m) {
            if (word.marks[m] < 0 || static_cast<std::size_t>(word.marks[m]) >= word.text.size() ||
                (m && word.marks[m] <= word.marks[m - 1])) return {};
            if (m) body.push_back(',');
            body += std::to_string(word.marks[m]);
        }
        body += "]}";
    }
    body += "],\"selection\":" + std::to_string(s.selection);
    body += ",\"typed_fragment\":";
    JsonString(body, completionist::protocol::ToUtf8(s.typedFragment));
    body += ",\"phrase\":";
    JsonString(body, completionist::protocol::ToUtf8(s.phrase));
    body += ",\"phrase_lead\":";
    JsonString(body, completionist::protocol::ToUtf8(s.phraseLead));
    body += ",\"partial_begin\":" + std::to_string(s.partialBegin) + ",\"partial_length\":" + std::to_string(s.partialLength);
    body += ",\"ai\":";
    JsonString(body, AiName(s.ai));
    body += ",\"wait_ms\":" + std::to_string(s.waitMs) + ",\"elapsed_ms\":" + std::to_string(s.elapsedMs);
    body += ",\"trigger_reason\":";
    JsonString(body, CleanUtf8(s.triggerReason));
    body += std::string(",\"engine_connected\":") + (s.engineConnected ? "true" : "false") + ",\"settings\":";
    AddSettings(body, s.settings);
    body.push_back('}');
    std::string frame;
    return Frame(std::move(body), &frame) ? frame : std::string{};
}

std::optional<Snapshot> ParseShow(std::string_view body) {
    auto document = Document(body);
    if (!document) return std::nullopt;
    uint64_t number = 0;
    std::string type;
    if (!Integer(document->Find("schema"), 1, 1, &number) || !String(document->Find("type"), &type, 16) || type != "show") return std::nullopt;
    Snapshot result{};
    if (!ParseIdentity(document->Find("owner"), &result.owner)) return std::nullopt;
    if (!Integer(document->Find("revision"), 1, UINT64_MAX, &result.revision)) return std::nullopt;
    const Json* caret = document->Find("caret");
    if (!caret || caret->type != Json::Type::Array || caret->array.size() != 4) return std::nullopt;
    int64_t coord = 0;
    int32_t* coords[] = {&result.caret.left, &result.caret.top, &result.caret.right, &result.caret.bottom};
    for (std::size_t i = 0; i < 4; ++i) {
        if (!SignedInteger(&caret->array[i], INT32_MIN, INT32_MAX, &coord)) return std::nullopt;
        *coords[i] = static_cast<int32_t>(coord);
    }
    const Json* words = document->Find("words");
    if (!words || words->type != Json::Type::Array || words->truncated || words->array.size() > kMaxCandidates) return std::nullopt;
    for (const Json& item : words->array) {
        if (item.type != Json::Type::Object) return std::nullopt;
        std::string text, origin;
        if (!String(item.Find("text"), &text) || !String(item.Find("origin"), &origin, 16) ||
            (origin != "local" && origin != "learned")) return std::nullopt;
        Candidate candidate{completionist::protocol::FromUtf8(text), std::move(origin), {}};
        const Json* marks = item.Find("marks");
        if (!marks || marks->type != Json::Type::Array || marks->truncated || marks->array.size() > kMaxMarksPerCandidate) return std::nullopt;
        int previous = -1;
        for (const Json& mark : marks->array) {
            int64_t position = 0;
            if (!SignedInteger(&mark, 0, static_cast<int64_t>(candidate.text.size()), &position) || position <= previous) return std::nullopt;
            candidate.marks.push_back(static_cast<int>(position));
            previous = static_cast<int>(position);
        }
        result.words.push_back(std::move(candidate));
    }
    int64_t selection = 0;
    if (!SignedInteger(document->Find("selection"), -2, static_cast<int64_t>(kMaxCandidates), &selection)) return std::nullopt;
    result.selection = static_cast<int>(selection);
    std::string text;
    if (!String(document->Find("typed_fragment"), &text)) return std::nullopt;
    result.typedFragment = completionist::protocol::FromUtf8(text);
    if (!String(document->Find("phrase"), &text)) return std::nullopt;
    result.phrase = completionist::protocol::FromUtf8(text);
    if (!String(document->Find("phrase_lead"), &text)) return std::nullopt;
    result.phraseLead = completionist::protocol::FromUtf8(text);
    if (!Integer(document->Find("partial_begin"), 0, UINT32_MAX, &number)) return std::nullopt;
    result.partialBegin = static_cast<uint32_t>(number);
    if (!Integer(document->Find("partial_length"), 0, UINT32_MAX, &number)) return std::nullopt;
    result.partialLength = static_cast<uint32_t>(number);
    if (!String(document->Find("ai"), &text, 16) || !ParseAi(text, &result.ai)) return std::nullopt;
    if (!Integer(document->Find("wait_ms"), 0, 600000, &number)) return std::nullopt;
    result.waitMs = static_cast<uint32_t>(number);
    if (!Integer(document->Find("elapsed_ms"), 0, 600000, &number)) return std::nullopt;
    result.elapsedMs = static_cast<uint32_t>(number);
    if (!String(document->Find("trigger_reason"), &result.triggerReason, 256)) return std::nullopt;
    if (!Boolean(document->Find("engine_connected"), &result.engineConnected)) return std::nullopt;
    if (!ParseSettings(document->Find("settings"), &result.settings)) return std::nullopt;
    const bool hasPhrase = !result.phrase.empty();
    if (result.selection == -1 && !hasPhrase) return std::nullopt;
    if (result.selection >= 0 && static_cast<std::size_t>(result.selection) >= result.words.size()) return std::nullopt;
    if (static_cast<uint64_t>(result.partialBegin) + result.partialLength > result.phrase.size()) return std::nullopt;
    return result;
}

std::optional<Ack> ParseAck(std::string_view body) {
    auto document = Document(body);
    if (!document) return std::nullopt;
    uint64_t number = 0;
    std::string type;
    if (!Integer(document->Find("schema"), 1, 1, &number) || !String(document->Find("type"), &type, 16) || type != "ack") return std::nullopt;
    Ack result{};
    if (!ParseIdentity(document->Find("owner"), &result.owner)) return std::nullopt;
    if (!Integer(document->Find("revision"), 1, UINT64_MAX, &result.revision)) return std::nullopt;
    if (!Boolean(document->Find("presented"), &result.presented)) return std::nullopt;
    return result;
}

bool IsCurrentAck(const Snapshot& current, const Ack& ack) {
    return ack.presented && SameIdentity(current.owner, ack.owner) && current.revision == ack.revision;
}

}  // namespace completionist::render
