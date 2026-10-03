#include "protocol.h"
#include "json_internal.h"

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace completionist::protocol {

using detail::Json;

namespace {

constexpr char32_t kReplacement = 0xFFFD;

void AppendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

void AppendUtf16(std::wstring& out, char32_t cp) {
    if (cp >= 0x10000) {
        cp -= 0x10000;
        out += static_cast<wchar_t>(0xD800 + (cp >> 10));
        out += static_cast<wchar_t>(0xDC00 + (cp & 0x3FF));
    } else {
        out += static_cast<wchar_t>(cp);
    }
}

void AppendJsonString(std::string& out, std::string_view utf8) {
    out += '"';
    for (char c : utf8) {
        auto byte = static_cast<unsigned char>(c);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (byte < 0x20) {
                    char escape[8];
                    std::snprintf(escape, sizeof(escape), "\\u%04x", byte);
                    out += escape;
                } else {
                    out += c;
                }
        }
    }
    out += '"';
}

// --- A small JSON reader: just enough for engine replies. -------------------------------------

}  // namespace

std::string ToUtf8(std::wstring_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        char32_t c = static_cast<char16_t>(text[i]);
        if (c >= 0xD800 && c < 0xDC00) {
            if (i + 1 < text.size() && static_cast<char16_t>(text[i + 1]) >= 0xDC00 && static_cast<char16_t>(text[i + 1]) < 0xE000) {
                c = 0x10000 + ((c - 0xD800) << 10) + (static_cast<char16_t>(text[i + 1]) - 0xDC00);
                ++i;
            } else {
                c = kReplacement;
            }
        } else if (c >= 0xDC00 && c < 0xE000) {
            c = kReplacement;
        }
        AppendUtf8(out, c);
    }
    return out;
}

std::wstring FromUtf8(std::string_view text) {
    std::wstring out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        auto lead = static_cast<unsigned char>(text[i]);
        int extra = lead < 0x80 ? 0 : (lead >> 5) == 0x6 ? 1 : (lead >> 4) == 0xE ? 2 : (lead >> 3) == 0x1E ? 3 : -1;
        char32_t cp = extra == 0 ? lead : extra == 1 ? lead & 0x1F : extra == 2 ? lead & 0x0F : lead & 0x07;
        bool valid = extra >= 0 && i + extra < text.size() + (extra == 0 ? 1 : 0);
        if (valid) {
            for (int k = 1; k <= extra; ++k) {
                auto next = static_cast<unsigned char>(text[i + k]);
                if ((next >> 6) != 0x2) {
                    valid = false;
                    break;
                }
                cp = (cp << 6) | (next & 0x3F);
            }
        }
        // Reject overlong forms, surrogates and out-of-range values.
        static constexpr char32_t kMin[] = {0, 0x80, 0x800, 0x10000};
        if (valid && (cp < kMin[extra] || cp > 0x10FFFF || (cp >= 0xD800 && cp < 0xE000))) valid = false;
        if (!valid) {
            AppendUtf16(out, kReplacement);
            ++i;
            continue;
        }
        AppendUtf16(out, cp);
        i += extra + 1;
    }
    return out;
}

std::string EncodeRequest(const Request& r) {
    std::string body = "{\"id\":" + std::to_string(r.id) + ",\"event\":";
    AppendJsonString(body, r.event);
    body += ",\"app\":";
    AppendJsonString(body, ToUtf8(r.app));
    body += ",\"title\":";
    AppendJsonString(body, ToUtf8(r.title));
    body += ",\"input_scope\":[";
    for (std::size_t i = 0; i < r.input_scope.size(); ++i) {
        if (i) body += ',';
        AppendJsonString(body, r.input_scope[i]);
    }
    body += "],\"before\":";
    AppendJsonString(body, ToUtf8(r.before));
    body += ",\"after\":";
    AppendJsonString(body, ToUtf8(r.after));
    if (!r.accepted.empty()) {
        body += ",\"accepted\":";
        AppendJsonString(body, ToUtf8(r.accepted));
    }
    if (!r.kind.empty()) {
        body += ",\"kind\":";
        AppendJsonString(body, r.kind);
    }
    if (r.quiet) body += ",\"quiet\":true";
    body += '}';

    auto length = static_cast<std::uint32_t>(body.size());
    std::string frame;
    frame.reserve(4 + body.size());
    for (int shift = 0; shift < 32; shift += 8) frame += static_cast<char>((length >> shift) & 0xFF);
    frame += body;
    return frame;
}

std::optional<WordReply> ParseWordReply(std::string_view body) {
    auto document = detail::Parser(body).ParseDocument({
        "id", "type", "replace", "words", "popup", "kinds", "marks", "phrase", "phrase_done", "phrase_mode",
        "phrase_state", "phrase_wait_ms", "phrase_elapsed_ms", "trigger_reason", "origins", "text", "done"});
    if (!document || document->type != Json::Type::Object) return std::nullopt;
    const Json* type = document->Find("type");
    const Json* id = document->Find("id");
    if (!type || type->type != Json::Type::String) return std::nullopt;
    if (!id || id->type != Json::Type::Number || id->number < 0 || id->number > 4294967295.0) return std::nullopt;

    WordReply reply;
    reply.id = static_cast<std::uint32_t>(id->number);

    auto optional_status = [&] {
        if (const Json* state = document->Find("phrase_state")) {
            if (state->type == Json::Type::String) {
                const auto& value = state->string;
                if (value == "off" || value == "manual" || value == "scheduled" || value == "working" ||
                    value == "streaming" || value == "ready" || value == "unavailable")
                    reply.phrase_state = value;
                else
                    reply.phrase_state = "unavailable";
            } else {
                reply.phrase_state = "unavailable";
            }
        }
        auto duration = [&](const char* name, std::optional<int>* target) {
            const Json* value = document->Find(name);
            if (value && value->type == Json::Type::Number && std::isfinite(value->number) &&
                value->number >= 0 && value->number <= 600000 && std::floor(value->number) == value->number)
                *target = static_cast<int>(value->number);
        };
        duration("phrase_wait_ms", &reply.phrase_wait_ms);
        duration("phrase_elapsed_ms", &reply.phrase_elapsed_ms);
        if (const Json* reason = document->Find("trigger_reason"); reason && reason->type == Json::Type::String)
            reply.trigger_reason = reason->string;
        if (const Json* origins = document->Find("origins"); origins && origins->type == Json::Type::Array && !origins->truncated &&
            origins->array.size() == reply.words.size()) {
            std::vector<std::string> parsed;
            parsed.reserve(origins->array.size());
            bool valid = true;
            for (const Json& origin : origins->array) {
                if (origin.type != Json::Type::String || (origin.string != "local" && origin.string != "learned")) {
                    valid = false;
                    break;
                }
                parsed.push_back(origin.string);
            }
            if (valid) reply.origins = std::move(parsed);
        }
    };

    if (type->string == "phrase") {
        const Json* text = document->Find("text");
        const Json* done = document->Find("done");
        if (!text || text->type != Json::Type::String || !done || done->type != Json::Type::Bool) return std::nullopt;
        reply.kind = ReplyKind::Phrase;
        reply.phrase = FromUtf8(text->string);
        reply.phrase_done = done->boolean;
        optional_status();
        return reply;
    }
    if (type->string != "words") return std::nullopt;

    // Optional preferences must never discard useful suggestions. Each bad field independently
    // keeps its default; an old engine with no popup object resets to the original behavior.
    if (const Json* popup = document->Find("popup"); popup && popup->type == Json::Type::Object && !popup->truncated) {
        if (const Json* font = popup->Find("font_size"); font && font->type == Json::Type::Number &&
            std::isfinite(font->number) && font->number >= 7 && font->number <= 24 &&
            std::floor(font->number) == font->number)
            reply.popup.font_size = static_cast<int>(font->number);
        if (const Json* width = popup->Find("width_scale"); width && width->type == Json::Type::Number &&
            std::isfinite(width->number) && width->number >= 0.5 && width->number <= 2)
            reply.popup.width_scale = width->number;
        if (const Json* partial = popup->Find("partial_accept"); partial && partial->type == Json::Type::String) {
            if (partial->string == "alt+right") reply.popup.partial_accept = PartialAccept::AltRight;
            else if (partial->string == "ctrl+tab") reply.popup.partial_accept = PartialAccept::CtrlTab;
        }
        if (const Json* dismiss = popup->Find("dismiss"); dismiss && dismiss->type == Json::Type::String) {
            if (dismiss->string == "ctrl+backspace") reply.popup.dismiss = DismissShortcut::CtrlBackspace;
            else if (dismiss->string == "alt+backspace") reply.popup.dismiss = DismissShortcut::AltBackspace;
        }
    }

    const Json* replace = document->Find("replace");
    const Json* words = document->Find("words");
    if (!replace || replace->type != Json::Type::Number || replace->number < 0 || replace->number > 100000) return std::nullopt;
    if (!words || words->type != Json::Type::Array || words->truncated) return std::nullopt;
    reply.replace = static_cast<int>(replace->number);
    for (const Json& word : words->array) {
        if (word.type != Json::Type::String) return std::nullopt;
        reply.words.push_back(FromUtf8(word.string));
    }
    if (const Json* kinds = document->Find("kinds")) {
        if (kinds->type != Json::Type::Array || kinds->truncated || kinds->array.size() != reply.words.size()) return std::nullopt;
        for (const Json& kind : kinds->array) {
            if (kind.type != Json::Type::String || (kind.string != "word" && kind.string != "chunk" && kind.string != "next"))
                return std::nullopt;
            reply.kinds.push_back(kind.string);
        }
    } else {
        reply.kinds.assign(reply.words.size(), "word");
    }
    if (const Json* marks = document->Find("marks")) {
        if (marks->type != Json::Type::Array || marks->truncated || marks->array.size() != reply.words.size()) return std::nullopt;
        for (std::size_t i = 0; i < marks->array.size(); ++i) {
            const Json& row = marks->array[i];
            if (row.type != Json::Type::Array) return std::nullopt;
            std::vector<int> positions;
            for (const Json& position : row.array) {
                if (position.type != Json::Type::Number || position.number < 0 ||
                    position.number != static_cast<int>(position.number) ||
                    static_cast<std::size_t>(position.number) >= reply.words[i].size())
                    return std::nullopt;
                positions.push_back(static_cast<int>(position.number));
            }
            reply.marks.push_back(std::move(positions));
        }
    }
    if (const Json* phrase = document->Find("phrase")) {
        if (phrase->type != Json::Type::String) return std::nullopt;
        reply.phrase = FromUtf8(phrase->string);
    }
    if (const Json* done = document->Find("phrase_done")) {
        if (done->type != Json::Type::Bool) return std::nullopt;
        reply.phrase_done = done->boolean;
    }
    if (const Json* mode = document->Find("phrase_mode")) {
        if (mode->type != Json::Type::String || (mode->string != "auto" && mode->string != "hotkey" && mode->string != "off"))
            return std::nullopt;
        reply.phrase_mode = mode->string;
    }
    optional_status();
    return reply;
}

bool FrameDecoder::Feed(const char* data, std::size_t size, std::vector<std::string>* bodies) {
    buffer_.append(data, size);
    for (;;) {
        if (buffer_.size() < 4) return true;
        std::uint32_t length = 0;
        for (int i = 0; i < 4; ++i) length |= static_cast<std::uint32_t>(static_cast<unsigned char>(buffer_[i])) << (8 * i);
        if (length > kMaxFrameBytes) return false;
        if (buffer_.size() < 4 + static_cast<std::size_t>(length)) return true;
        bodies->push_back(buffer_.substr(4, length));
        buffer_.erase(0, 4 + static_cast<std::size_t>(length));
    }
}

}  // namespace completionist::protocol
