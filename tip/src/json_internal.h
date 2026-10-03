// Shared internal JSON reader used by the engine and renderer protocols.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace completionist::protocol::detail {
constexpr std::size_t kMaxJsonBytes = 1u << 20;
constexpr char32_t kReplacementJson = 0xFFFD;
inline void AppendUtf8Json(std::string& out, char32_t cp) {
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    else { out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
}

struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    double number = 0;
    std::string rawNumber;
    bool boolean = false;
    std::string string;
    std::vector<Json> array;
    std::vector<std::pair<std::string, Json>> object;
    bool truncated = false;

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
        if (text_.size() > kMaxJsonBytes) return std::nullopt;
        auto value = ParseValue(0);
        SkipSpace();
        if (!value || pos_ != text_.size()) return std::nullopt;
        return value;
    }

private:
    static constexpr int kMaxDepth = 16;
    static constexpr std::size_t kMaxContainerItems = 4096;

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

    std::optional<Json> ParseValue(int depth, bool retain = true) {
        if (depth > kMaxDepth) return std::nullopt;
        SkipSpace();
        if (pos_ >= text_.size()) return std::nullopt;
        Json value;
        char c = text_[pos_];
        if (c == '{') {
            ++pos_;
            value.type = Json::Type::Object;
            if (Consume('}')) return value;
            std::unordered_set<std::string> seenKeys;
            for (;;) {
                SkipSpace();
                auto key = ParseString();
                if (!key || !Consume(':')) return std::nullopt;
                if (!seenKeys.emplace(*key).second) return std::nullopt;
                const bool keepMember = retain && value.object.size() < kMaxContainerItems;
                if (retain && !keepMember) value.truncated = true;
                auto member = ParseValue(depth + 1, keepMember);
                if (!member) return std::nullopt;
                if (keepMember) value.object.emplace_back(std::move(*key), std::move(*member));
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
                const bool keepElement = retain && value.array.size() < kMaxContainerItems;
                if (retain && !keepElement) value.truncated = true;
                auto element = ParseValue(depth + 1, keepElement);
                if (!element) return std::nullopt;
                if (keepElement) value.array.push_back(std::move(*element));
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
        const std::size_t start = pos_;
        if (pos_ < text_.size() && text_[pos_] == '-') ++pos_;
        if (pos_ >= text_.size()) return std::nullopt;
        if (text_[pos_] == '0') ++pos_;
        else {
            if (text_[pos_] < '1' || text_[pos_] > '9') return std::nullopt;
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            const std::size_t fraction = pos_;
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
            if (fraction == pos_) return std::nullopt;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            const std::size_t exponent = pos_;
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
            if (exponent == pos_) return std::nullopt;
        }
        std::string digits(text_.substr(start, pos_ - start));
        char* end = nullptr;
        double number = std::strtod(digits.c_str(), &end);
        if (end != digits.c_str() + digits.size()) return std::nullopt;
        Json value;
        value.type = Json::Type::Number;
        value.number = number;
        value.rawNumber = std::move(digits);
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
                            cp = (*lo >= 0xDC00 && *lo < 0xE000) ? 0x10000 + ((cp - 0xD800) << 10) + (*lo - 0xDC00) : kReplacementJson;
                        } else {
                            cp = kReplacementJson;
                        }
                    } else if (cp >= 0xDC00 && cp < 0xE000) {
                        cp = kReplacementJson;
                    }
                    AppendUtf8Json(out, cp);
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


}  // namespace completionist::protocol::detail
