#include "../src/render_client.h"
#include "../src/protocol.h"
#include "fake_renderer_pipe.h"
#include "test_harness.h"

using namespace completionist::render;
using completionist::render::tests::FakeRendererPipe;

namespace {
Snapshot Sample(uint64_t revision = 1) {
    Snapshot snapshot{};
    snapshot.owner = {77, 0x2020, "fixture-session", 5};
    snapshot.revision = revision;
    snapshot.caret = {100, 200, 102, 220};
    snapshot.words = {{L"completion", "local", {}}};
    snapshot.selection = 0;
    snapshot.typedFragment = L"comp";
    snapshot.settings.font_size = 12;
    return snapshot;
}

}  // namespace

TEST(render_client_rejects_unencodable_status_instead_of_queueing_a_reconnect_loop) {
    RenderClientState state;
    auto invalid = Sample();
    invalid.words.clear();
    invalid.ai = AiState::Working;
    invalid.selection = 0;
    CHECK(!state.Publish(invalid, 1000));
    CHECK(!state.current().has_value());
    CHECK_EQ(state.queued(), 0u);
    invalid.selection = -2;
    CHECK(state.Publish(invalid, 1010));
    CHECK_EQ(state.queued(), 1u);
    auto next = state.Take();
    CHECK(next.has_value() && std::holds_alternative<ShowRequest>(*next));
    if (next && std::holds_alternative<ShowRequest>(*next))
        CHECK(!EncodeShow(std::get<ShowRequest>(*next).snapshot).empty());
}

TEST(render_client_updates_preserve_glass_and_bound_the_pending_deadline) {
    RenderClientState state;
    CHECK(state.Publish(Sample(1), 1000));
    CHECK(state.OnAck({Sample().owner, 1, true}));
    CHECK(state.Publish(Sample(2), 1100));
    CHECK(!state.fallbackVisible());
    CHECK(state.Publish(Sample(3), 1200));
    CHECK(!state.fallbackVisible());
    CHECK(!state.OnAck({Sample().owner, 2, true}));
    CHECK(!state.Tick(1349));
    CHECK(state.Tick(1350));
    CHECK(state.fallbackVisible());
    CHECK(state.OnAck({Sample().owner, 3, true}));
    CHECK(!state.fallbackVisible());
    auto changed = Sample(4);
    ++changed.owner.generation;
    CHECK(state.Publish(changed, 1400));
    CHECK(state.fallbackVisible());
}

TEST(current_presentation_state_supersedes_queued_failure_and_success_notices) {
    RenderClientState state;
    const auto first = Sample(1);
    CHECK(state.Publish(first, 1000));
    CHECK(state.OnAck({first.owner, 1, true}));
    const auto queuedSuccess = state.PresentationState();
    CHECK(!queuedSuccess.fallbackVisible);
    CHECK(state.Publish(Sample(2), 1100));
    CHECK(state.Tick(1350));
    const auto queuedFailure = state.PresentationState();
    CHECK(queuedFailure.fallbackVisible);
    CHECK(state.Publish(Sample(3), 1360));
    CHECK(state.PresentationState().fallbackVisible);  // revision changes cannot discard a live failure
    CHECK_EQ(state.PresentationState().revision, 3u);
    CHECK(state.OnAck({first.owner, 3, true}));
    CHECK(!state.PresentationState().fallbackVisible);  // delayed old failure must not cover fresh glass
    CHECK(state.Disconnect());
    CHECK(state.PresentationState().fallbackVisible);  // delayed old success must not conceal a disconnect
    CHECK(state.Hide(first.owner, 4));
    CHECK_EQ(state.PresentationState().revision, 0u);
    CHECK_EQ(state.PresentationState().owner.pid, 0u);
}

TEST(render_client_delayed_ack_keeps_host_fallback_until_current_presentation) {
    RenderClientState state;
    CHECK(state.Publish(Sample(1), 1000));
    CHECK(state.fallbackVisible());
    CHECK(!state.Tick(1249));
    CHECK(state.Tick(1250));
    CHECK(state.fallbackVisible());
    FakeRendererPipe peer;
    auto ack = peer.Present(Sample(1));
    CHECK(ack.has_value() && state.OnAck(*ack));
    CHECK(!state.fallbackVisible());
}

TEST(render_client_rejects_invalid_peer_and_old_generation_or_revision_ack) {
    RenderClientState state;
    CHECK(state.Publish(Sample(3), 10));
    FakeRendererPipe peer;
    peer.invalidPeer = true;
    auto wrongPeer = peer.Present(Sample(3));
    CHECK(wrongPeer.has_value() && !state.OnAck(*wrongPeer));
    peer.invalidPeer = false;
    auto oldGeneration = peer.Present(Sample(3));
    oldGeneration->owner.generation++;
    CHECK(!state.OnAck(*oldGeneration));
    auto oldRevision = peer.Present(Sample(2));
    CHECK(oldRevision.has_value() && !state.OnAck(*oldRevision));
    auto current = peer.Present(Sample(3));
    CHECK(current.has_value() && state.OnAck(*current));
}

TEST(render_client_disconnect_and_unavailable_restore_fallback) {
    RenderClientState state;
    CHECK(state.Publish(Sample(), 0));
    FakeRendererPipe peer;
    auto unavailable = peer.Present(Sample(), false);
    CHECK(unavailable.has_value() && state.OnAck(*unavailable));
    CHECK(state.fallbackVisible());
    CHECK(state.Publish(Sample(2), 1));
    auto presented = peer.Present(Sample(2));
    CHECK(presented.has_value() && state.OnAck(*presented));
    CHECK(!state.fallbackVisible());
    peer.disconnect = true;
    CHECK(!peer.Present(Sample(3)).has_value());
    CHECK(state.Disconnect());
    CHECK(state.fallbackVisible());
}

TEST(render_client_reconnect_renews_generation_and_resends_current_snapshot) {
    RenderClientState state;
    CHECK(state.Publish(Sample(7), 20));
    CHECK(state.OnAck({Sample().owner, 7, true}));
    CHECK(state.Disconnect());
    Identity renewed = Sample().owner;
    renewed.generation++;
    state.Reconnect(renewed, 40);
    CHECK(state.fallbackVisible());
    auto replay = state.Take();
    CHECK(replay.has_value() && std::holds_alternative<ShowRequest>(*replay));
    CHECK_EQ(std::get<ShowRequest>(*replay).snapshot.owner.generation, renewed.generation);
    CHECK_EQ(std::get<ShowRequest>(*replay).snapshot.revision, 7u);
}

TEST(render_client_coalesces_shows_prioritizes_hide_and_caps_queue) {
    RenderClientState state;
    CHECK(state.Publish(Sample(1), 0));
    CHECK(state.Publish(Sample(2), 1));
    CHECK_EQ(state.queued(), 1u);
    auto newest = state.Take();
    CHECK(newest.has_value() && std::get<ShowRequest>(*newest).snapshot.revision == 2);

    Identity owner = Sample().owner;
    CHECK(state.Publish(Sample(3), 2));
    CHECK(state.Hide(owner, 3));
    auto priority = state.Take();
    CHECK(priority.has_value() && std::holds_alternative<HideRequest>(*priority));
    CHECK_EQ(state.queued(), 0u);

    RenderClientState bounded;
    for (uint64_t generation = 1; generation <= RenderClientState::kQueueLimit; ++generation) {
        Identity heartbeatOwner = owner;
        heartbeatOwner.generation = generation;
        CHECK(bounded.Heartbeat(std::move(heartbeatOwner)));
    }
    CHECK_EQ(bounded.queued(), RenderClientState::kQueueLimit);
    Identity overflow = owner;
    overflow.generation = 99;
    CHECK(!bounded.Heartbeat(overflow));
    CHECK(bounded.Hide(owner, 9));
    CHECK_EQ(bounded.queued(), RenderClientState::kQueueLimit);
    auto first = bounded.Take();
    CHECK(first.has_value() && std::holds_alternative<HideRequest>(*first));
}
