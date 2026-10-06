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
    bool RevokeIfCurrent(const completionist::render::Identity& owner);
    bool visible() const { return active_; }
    const char* LastRefusal() const { return refusal_; }
    const completionist::render::Snapshot* current() const { return active_ ? &snapshot_ : nullptr; }

private:
    bool HasUsableContent(const completionist::render::Snapshot& snapshot) const;
    // Raises the floor below which this host session's snapshots are stale. A hidden popup only
    // refuses its own older revisions; a superseded generation refuses everything it sends.
    void Retire(const completionist::render::Identity& owner, uint64_t revision);

    completionist::render::Snapshot snapshot_{};
    uint64_t renewedAtMs_ = 0;
    bool active_ = false;
    const char* refusal_ = "none";
    struct Floor { uint64_t generation = 0, revision = 0; };
    std::unordered_map<std::string, Floor> floors_;
};

}  // namespace renderer
