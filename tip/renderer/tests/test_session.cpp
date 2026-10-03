#include "../session.h"
#include "../../tests/test_harness.h"

using completionist::render::Snapshot;
using completionist::render::Identity;

namespace {
Snapshot Make(uint32_t pid, uint64_t generation, uint64_t revision, const char* session = "A") {
    Snapshot s{};
    s.owner = {pid, 0x1234 + pid, session, generation};
    s.revision = revision;
    s.words = {{L"word", "local", {}}};
    s.selection = 0;
    return s;
}
}

TEST(session_requires_the_actual_foreground_process) {
    renderer::Session session;
    CHECK(!session.Accept(Make(100, 2, 8), 101, 0));
    CHECK(session.Accept(Make(100, 2, 8), 100, 0));
}

TEST(session_ignores_stale_generation_and_revision) {
    renderer::Session session;
    auto current = Make(100, 2, 8);
    CHECK(session.Accept(current, 100, 0));
    CHECK(!session.Accept(Make(100, 1, 99), 100, 1));
    CHECK(!session.Accept(Make(100, 2, 7), 100, 1));
    CHECK_EQ(session.current()->revision, 8u);
}

TEST(session_lease_expires_after_1500_milliseconds_without_renewal) {
    renderer::Session session;
    auto current = Make(100, 2, 8);
    CHECK(session.Accept(current, 100, 500));
    CHECK(session.Heartbeat(current.owner, 100, 500));
    session.Expire(2000);
    CHECK(session.visible());
    session.Expire(2001);
    CHECK(!session.visible());
}

TEST(revoked_generation_cannot_return_through_heartbeat_or_show) {
    renderer::Session session;
    auto current = Make(100, 2, 8);
    CHECK(session.Accept(current, 100, 0));
    session.Revoke();
    CHECK(!session.Heartbeat(current.owner, 100, 500));
    CHECK(!session.Accept(Make(100, 2, 9), 100, 501));
    CHECK(session.Accept(Make(100, 3, 1), 100, 502));
}

TEST(disconnect_revokes_surfaces_and_content_and_new_owner_takes_over_when_eligible) {
    renderer::Session session;
    auto a = Make(100, 2, 8);
    auto b = Make(200, 1, 1, "B");
    CHECK(session.Accept(a, 100, 0));
    CHECK(!session.Accept(b, 200, 100));
    session.Revoke();
    CHECK(!session.visible());
    CHECK(session.current() == nullptr);
    CHECK(session.Accept(b, 200, 101));
    CHECK_EQ(session.current()->owner.pid, 200u);
}

TEST(acknowledgement_must_match_the_live_snapshot) {
    renderer::Session session;
    auto a = Make(100, 2, 8);
    CHECK(session.Accept(a, 100, 0));
    completionist::render::Ack good{a.owner, 8, true};
    completionist::render::Ack stale{a.owner, 7, true};
    CHECK(session.IsCurrentAck(a, good));
    CHECK(!session.IsCurrentAck(a, stale));
    session.Revoke();
    CHECK(!session.IsCurrentAck(a, good));
}

TEST(empty_pending_snapshot_is_not_made_selectable) {
    renderer::Session session;
    Snapshot pending = Make(100, 2, 8);
    pending.words.clear();
    pending.selection = -2;
    pending.ai = completionist::render::AiState::Working;
    CHECK(session.Accept(pending, 100, 0));
    CHECK_EQ(session.current()->selection, -2);
    CHECK_EQ(session.current()->words.size(), 0u);
    session.Revoke();
    pending.ai = completionist::render::AiState::Off;
    pending.owner.generation = 3;
    CHECK(!session.Accept(pending, 100, 1));
}
