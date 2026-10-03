// Version 1 protocol between the TSF host and the out-of-process renderer.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "popup_settings.h"

namespace completionist::render {

constexpr std::size_t kMaxFrameBytes = 1u << 20;
constexpr std::size_t kMaxCandidates = 64;
constexpr std::size_t kMaxMarksPerCandidate = 256;
constexpr std::size_t kMaxSessionBytes = 128;
constexpr std::size_t kMaxTextBytes = 256u << 10;

struct Identity { uint32_t pid; uint64_t hostHwnd; std::string session; uint64_t generation; };
struct Rect { int32_t left, top, right, bottom; };
enum class Command { Show, Hide, Heartbeat };
enum class AiState { Off, Manual, Scheduled, Working, Streaming, Ready, Unavailable };
struct Candidate { std::wstring text; std::string origin; std::vector<int> marks; };
struct Snapshot {
    Identity owner; uint64_t revision; Rect caret;
    std::vector<Candidate> words; int selection;
    std::wstring typedFragment, phrase, phraseLead;
    uint32_t partialBegin, partialLength;
    AiState ai; uint32_t waitMs, elapsedMs; std::string triggerReason;
    bool engineConnected; completionist::PopupSettings settings;
};
struct Ack { Identity owner; uint64_t revision; bool presented; };
struct CommandMessage { Command command; Identity owner; uint64_t revision = 0; };

// EncodeShow returns a complete 4-byte little-endian length-prefixed frame.
std::string EncodeShow(const Snapshot& snapshot);
std::optional<Snapshot> ParseShow(std::string_view body);
std::optional<Ack> ParseAck(std::string_view body);
std::optional<CommandMessage> ParseCommand(std::string_view body);
std::string EncodeAck(const Ack& ack); // complete length-prefixed frame
bool IsCurrentAck(const Snapshot& current, const Ack& ack);
bool SameIdentity(const Identity& left, const Identity& right);

}  // namespace completionist::render
