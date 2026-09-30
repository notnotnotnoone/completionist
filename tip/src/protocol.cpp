#include "protocol.h"

#include <cstdio>
#include <cstdlib>
#include <utility>

namespace typer::protocol {

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

struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    double number = 0;
    bool boolean = false;
    std::string string;
    std::vector<Json> array;
    std::vector<std::pair<std::string, Json>> object;

    const Json* Find(std::string_view key) const {
        for (auto& [k, v] : object)
            if (k == key) return &v;
        return nullptr;
    }
};

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    std::optional<Json> ParseDocument() {
        auto value = ParseValue(0);
        SkipSpace();
        if (!value || pos_ != text_.size()) return std::nullopt;
        return value;
    }

private:
    static constexpr int kMaxDepth = 16;

    void SkipSpace() {
        while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r'))
            ++pos_;
    }

    bool Consume(char c) {
        SkipSpace();
        if (pos_ < text_.size() && text_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    bool ConsumeWord(std::string_view word) {
        if (text_.substr(pos_, word.size()) != word) return false;
        pos_ += word.size();
        return true;
    }

    std::optional<Json> ParseValue(int depth) {
        if (depth > kMaxDepth) return std::nullopt;
        SkipSpace();
        if (pos_ >= text_.size()) return std::nullopt;
        Json value;
        char c = text_[pos_];
        if (c == '{') {
            ++pos_;
            value.type = Json::Type::Object;
            if (Consume('}')) return value;
            for (;;) {
                SkipSpace();
                auto key = ParseString();
                if (!key || !Consume(':')) return std::nullopt;
                auto member = ParseValue(depth + 1);
                if (!member) return std::nullopt;
                value.object.emplace_back(std::move(*key), std::move(*member));
                if (Consume(',')) continue;
                if (Consume('}')) return value;
                return std::nullopt;
            }
        }
        if (c == '[') {
            ++pos_;
            value.type = Json::Type::Array;
            if (Consume(']')) return value;
            for (;;) {
                auto element = ParseValue(depth + 1);
                if (!element) return std::nullopt;
                value.array.push_back(std::move(*element));
                if (Consume(',')) continue;
                if (Consume(']')) return value;
                return std::nullopt;
            }
        }
        if (c == '"') {
            auto s = ParseString();
            if (!s) return std::nullopt;
            value.type = Json::Type::String;
            value.string = std::move(*s);
            return value;
        }
        if (ConsumeWord("true")) {
            value.type = Json::Type::Bool;
            value.boolean = true;
            return value;
        }
        if (ConsumeWord("false")) {
            value.type = Json::Type::Bool;
            return value;
        }
        if (ConsumeWord("null")) return value;
        return ParseNumber();
    }

    std::optional<Json> ParseNumber() {
        std::size_t start = pos_;
        while (pos_ < text_.size() && (std::string_view("+-.eE0123456789").find(text_[pos_]) != std::string_view::npos)) ++pos_;
        if (pos_ == start) return std::nullopt;
        std::string digits(text_.substr(start, pos_ - start));
        char* end = nullptr;
        double number = std::strtod(digits.c_str(), &end);
        if (end != digits.c_str() + digits.size()) return std::nullopt;
        Json value;
        value.type = Json::Type::Number;
        value.number = number;
        return value;
    }

    std::optional<unsigned> ParseHex4() {
        if (pos_ + 4 > text_.size()) return std::nullopt;
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            char h = text_[pos_++];
            v <<= 4;
            if (h >= '0' && h <= '9') v |= h - '0';
            else if (h >= 'a' && h <= 'f') v |= h - 'a' + 10;
            else if (h >= 'A' && h <= 'F') v |= h - 'A' + 10;
            else return std::nullopt;
        }
        return v;
    }

    std::optional<std::string> ParseString() {
        if (pos_ >= text_.size() || text_[pos_] != '"') return std::nullopt;
        ++pos_;
        std::string out;
        while (pos_ < text_.size()) {
            char c = text_[pos_++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20) return std::nullopt;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos_ >= text_.size()) return std::nullopt;
            char e = text_[pos_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    auto hi = ParseHex4();
                    if (!hi) return std::nullopt;
                    char32_t cp = *hi;
                    if (cp >= 0xD800 && cp < 0xDC00) {
                        // A high surrogate needs its low half; otherwise it can't be text.
                        if (text_.substr(pos_, 2) == "\\u") {
                            pos_ += 2;
                            auto lo = ParseHex4();
                            if (!lo) return std::nullopt;
                            cp = (*lo >= 0xDC00 && *lo < 0xE000) ? 0x10000 + ((cp - 0xD800) << 10) + (*lo - 0xDC00) : kReplacement;
                        } else {
                            cp = kReplacement;
                        }
                    } else if (cp >= 0xDC00 && cp < 0xE000) {
                        cp = kReplacement;
                    }
                    AppendUtf8(out, cp);
                    break;
                }
                default: return std::nullopt;
            }
        }
        return std::nullopt;  // unterminated
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

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
    auto document = Parser(body).ParseDocument();
    if (!document || document->type != Json::Type::Object) return std::nullopt;
    const Json* type = document->Find("type");
    const Json* id = document->Find("id");
    if (!type || type->type != Json::Type::String) return std::nullopt;
    if (!id || id->type != Json::Type::Number || id->number < 0 || id->number > 4294967295.0) return std::nullopt;

    WordReply reply;
    reply.id = static_cast<std::uint32_t>(id->number);

    if (type->string == "phrase") {
        const Json* text = document->Find("text");
        const Json* done = document->Find("done");
        if (!text || text->type != Json::Type::String || !done || done->type != Json::Type::Bool) return std::nullopt;
        reply.kind = ReplyKind::Phrase;
        reply.phrase = FromUtf8(text->string);
        reply.phrase_done = done->boolean;
        return reply;
    }
    if (type->string != "words") return std::nullopt;

    const Json* replace = document->Find("replace");
    const Json* words = document->Find("words");
    if (!replace || replace->type != Json::Type::Number || replace->number < 0 || replace->number > 100000) return std::nullopt;
    if (!words || words->type != Json::Type::Array) return std::nullopt;
    reply.replace = static_cast<int>(replace->number);
    for (const Json& word : words->array) {
        if (word.type != Json::Type::String) return std::nullopt;
        reply.words.push_back(FromUtf8(word.string));
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

}  // namespace typer::protocol
