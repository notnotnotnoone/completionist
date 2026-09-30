// Wire protocol with the engine: each frame is a 4-byte little-endian length followed by that many
// bytes of UTF-8 JSON (an object). Mirrors engine/src/completionist_engine/protocol.py.
// No Windows dependencies, so it is tested natively.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace completionist::protocol {

constexpr std::size_t kMaxFrameBytes = 1u << 20;

std::string ToUtf8(std::wstring_view text);    // lone surrogates become U+FFFD
std::wstring FromUtf8(std::string_view text);  // invalid bytes become U+FFFD

struct Request {
    std::uint32_t id = 0;
    std::string event;  // "keystroke", "hotkey", "accept" or "dismiss"
    std::wstring app;
    std::wstring title;
    std::vector<std::string> input_scope;
    std::wstring before;
    std::wstring after;
    std::wstring accepted;  // for "accept": the word that was inserted
    std::string kind;       // for "accept": "word" (default), "phrase" or "phrase_word"
    bool quiet = false;     // the popup is held back here, so the engine should not ask for phrases
};

// A complete frame (length header + JSON body) ready to write to the pipe.
std::string EncodeRequest(const Request& request);

enum class ReplyKind { Words, Phrase };

// Either a reply to a keystroke request (Words), or a phrase update pushed as text streams in (Phrase).
struct WordReply {
    ReplyKind kind = ReplyKind::Words;
    std::uint32_t id = 0;
    int replace = 0;  // characters before the caret that a chosen word replaces
    std::vector<std::wstring> words;
    std::wstring phrase;          // the phrase continuation to show, if any
    bool phrase_done = true;      // false while more phrase text may arrive
    std::string phrase_mode = "off";  // "auto", "hotkey" or "off": whether phrases are available here
};

// Parses a frame body. Returns nothing for anything that isn't a well-formed words or phrase message.
std::optional<WordReply> ParseWordReply(std::string_view body);

// Splits a byte stream into frame bodies, however the bytes are chunked.
class FrameDecoder {
public:
    // Appends the complete frame bodies found to `bodies`. Returns false on an oversize frame,
    // after which the stream can't be trusted and the connection should be dropped.
    bool Feed(const char* data, std::size_t size, std::vector<std::string>* bodies);

private:
    std::string buffer_;
};

}  // namespace completionist::protocol
