// Asynchronous host-side client for the renderer's logon-scoped named pipe.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <variant>

#include "render_protocol.h"

namespace completionist::render {

constexpr UINT kRenderClientNoticeMessage = WM_APP + 0x371;
// V2 is enabled for owner-led daily-use testing; fallback remains available.
constexpr bool kExternalRendererActivationEnabled = true;
constexpr uint64_t kFallbackDeadlineMs = 250;
constexpr uint64_t kHeartbeatIntervalMs = 500;
uint64_t NextRenderGeneration();

struct ClientNotice {
    std::optional<Ack> ack;
    bool fallbackVisible = true;
    Identity owner{};
    uint64_t revision = 0;
};

struct ShowRequest { Snapshot snapshot; uint64_t queuedAtMs = 0; };
struct HideRequest { Identity owner; uint64_t revision = 0; };
struct HeartbeatRequest { Identity owner; };
using ClientRequest = std::variant<ShowRequest, HideRequest, HeartbeatRequest>;

// This is the exact queue and acknowledgement state used by RenderClient's worker.
// Kept transport-free so compile-only native assertions can exercise production logic.
class RenderClientState {
public:
    static constexpr std::size_t kQueueLimit = 32;
    bool Publish(Snapshot snapshot, uint64_t nowMs);
    bool Hide(Identity owner, uint64_t revision);
    bool Heartbeat(Identity owner);
    std::optional<ClientRequest> Take();
    bool OnAck(const Ack& ack);
    bool Tick(uint64_t nowMs);
    bool Disconnect();
    void Reconnect(Identity owner, uint64_t nowMs);
    bool fallbackVisible() const { return fallbackVisible_; }
    ClientNotice PresentationState() const;
    const std::optional<Snapshot>& current() const { return current_; }
    std::size_t queued() const { return queue_.size(); }

private:
    bool MakeRoomForPriority();
    std::deque<ClientRequest> queue_;
    std::optional<Snapshot> current_;
    uint64_t showQueuedAtMs_ = 0;
    bool awaitingAck_ = false;
    bool fallbackVisible_ = true;
};

class RenderClient {
public:
    RenderClient(HWND notificationWindow, const Identity& identity);
    ~RenderClient();
    RenderClient(const RenderClient&) = delete;
    RenderClient& operator=(const RenderClient&) = delete;

    void Start();
    void Stop();
    bool Publish(Snapshot snapshot);
    void Hide(Identity owner, uint64_t revision);
    void Heartbeat(Identity owner);
    ClientNotice PresentationState();

private:
    void Worker();
    void Notify(std::optional<Ack> ack = std::nullopt);
    HWND notificationWindow_ = nullptr;
    Identity identity_{};
    std::mutex mutex_;
    std::condition_variable wake_;
    RenderClientState state_;
    bool started_ = false;
    bool stopping_ = false;
    std::thread worker_;
};

}  // namespace completionist::render
