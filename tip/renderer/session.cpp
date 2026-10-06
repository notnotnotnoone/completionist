#include "session.h"

namespace renderer {
namespace {

bool SameHost(const completionist::render::Identity& a, const completionist::render::Identity& b) {
    return a.pid == b.pid && a.hostHwnd == b.hostHwnd && a.session == b.session;
}

}  // namespace

bool Session::HasUsableContent(const completionist::render::Snapshot& snapshot) const {
    if (snapshot.phrase.empty() && snapshot.words.empty()) {
        return snapshot.selection == -2 && snapshot.ai != completionist::render::AiState::Off;
    }
    if (snapshot.selection == -2) return true;
    if (snapshot.selection == -1) return !snapshot.phrase.empty();
    return snapshot.selection >= 0 && static_cast<std::size_t>(snapshot.selection) < snapshot.words.size();
}

void Session::Retire(const completionist::render::Identity& owner, uint64_t revision) {
    Floor& floor = floors_[owner.session];
    if (owner.generation > floor.generation) floor = {owner.generation, revision};
    else if (owner.generation == floor.generation && revision > floor.revision) floor.revision = revision;
}

bool Session::Accept(const completionist::render::Snapshot& snapshot, uint32_t foregroundPid, uint64_t nowMs) {
    if (snapshot.owner.pid == 0 || snapshot.owner.hostHwnd == 0 || snapshot.owner.session.empty() ||
        snapshot.owner.generation == 0 || snapshot.revision == 0 || foregroundPid != snapshot.owner.pid ||
        !HasUsableContent(snapshot)) return false;
    Expire(nowMs);
    auto floor = floors_.find(snapshot.owner.session);
    if (floor != floors_.end() && (snapshot.owner.generation < floor->second.generation ||
        (snapshot.owner.generation == floor->second.generation && snapshot.revision <= floor->second.revision))) return false;
    if (active_ && SameHost(snapshot_.owner, snapshot.owner)) {
        if (snapshot.owner.generation < snapshot_.owner.generation) return false;
        if (snapshot.owner.generation == snapshot_.owner.generation && snapshot.revision <= snapshot_.revision) return false;
        if (snapshot.owner.generation > snapshot_.owner.generation) Retire(snapshot_.owner, UINT64_MAX);
    } else if (active_) {
        if (foregroundPid == snapshot_.owner.pid && nowMs - renewedAtMs_ <= kLeaseMs) return false;
        Retire(snapshot_.owner, snapshot_.revision);
    }
    snapshot_ = snapshot;
    renewedAtMs_ = nowMs;
    active_ = true;
    return true;
}

bool Session::Heartbeat(const completionist::render::Identity& owner, uint32_t foregroundPid, uint64_t nowMs) {
    Expire(nowMs);
    if (!active_ || foregroundPid != owner.pid || !completionist::render::SameIdentity(snapshot_.owner, owner)) return false;
    renewedAtMs_ = nowMs;
    return true;
}

bool Session::IsCurrentAck(const completionist::render::Snapshot& snapshot,
                           const completionist::render::Ack& ack) const {
    return active_ && completionist::render::SameIdentity(snapshot_.owner, snapshot.owner) &&
           snapshot_.revision == snapshot.revision && completionist::render::IsCurrentAck(snapshot_, ack);
}

void Session::Expire(uint64_t nowMs) {
    if (active_ && nowMs >= renewedAtMs_ && nowMs - renewedAtMs_ > kLeaseMs) Revoke();
}

void Session::Revoke() {
    if (active_) Retire(snapshot_.owner, snapshot_.revision);
    snapshot_ = {};
    renewedAtMs_ = 0;
    active_ = false;
}

bool Session::RevokeIfCurrent(const completionist::render::Identity& owner) {
    if (!active_ || !completionist::render::SameIdentity(snapshot_.owner, owner)) return false;
    Revoke();
    return true;
}

}  // namespace renderer
