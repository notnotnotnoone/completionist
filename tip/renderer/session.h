#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "../src/render_protocol.h"

namespace renderer {

class Session {
public:
    static constexpr uint64_t kHeartbeatMs = 500;
    static constexpr uint64_t kLeaseMs = 1500;

    // foregroundPid must be supplied from the actual OS foreground query.
    bool Accept(const completionist::render::Snapshot& snapshot, uint32_t foregroundPid, uint64_t nowMs);
    bool Heartbeat(const completionist::render::Identity& owner, uint32_t foregroundPid, uint64_t nowMs);
    bool IsCurrentAck(const completionist::render::Snapshot& snapshot,
                      const completionist::render::Ack& ack) const;
    void Expire(uint64_t nowMs);
    void Revoke();
    bool visible() const { return active_; }
    const completionist::render::Snapshot* current() const { return active_ ? &snapshot_ : nullptr; }

private:
    bool HasUsableContent(const completionist::render::Snapshot& snapshot) const;
    void Retire(const completionist::render::Identity& owner);

    completionist::render::Snapshot snapshot_{};
    uint64_t renewedAtMs_ = 0;
    bool active_ = false;
    std::unordered_map<std::string, uint64_t> retiredGenerations_;
};

}  // namespace renderer
