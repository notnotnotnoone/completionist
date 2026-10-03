#include "../dock_state.h"
#include "../../tests/test_harness.h"

TEST(dock_geometry_auto_minimize_restores_when_space_returns) {
    renderer::dock::State state;
    state.SetGeometry(false, false, 0);
    CHECK(!state.TargetExpanded());
    CHECK(state.NeedsFrame());
    CHECK(state.Sample(220) == 0.0);
    state.SetGeometry(true, false, 300);
    CHECK(state.TargetExpanded());
    CHECK(state.Sample(520) == 1.0);
}

TEST(dock_manual_preference_survives_geometry_and_toggle_clears_it_only_by_toggle) {
    renderer::dock::State state;
    state.Toggle(0);
    CHECK(state.ManuallyControlled());
    CHECK(!state.TargetExpanded());
    state.SetGeometry(false, true, 220);
    state.SetGeometry(true, false, 500);
    CHECK(!state.TargetExpanded());
    state.Toggle(500);
    CHECK(state.TargetExpanded());
    state.Immediate(true, false, 720);
    CHECK(state.BodyHitTestable(720));
}

TEST(dock_reversal_is_continuous_and_hidden_body_cannot_hit_test) {
    renderer::dock::State state;
    state.Toggle(0);
    const double midway = state.Sample(80);
    CHECK(midway > 0.0 && midway < 1.0);
    state.Toggle(80);
    CHECK(std::abs(state.Sample(80) - midway) < .00001);
    CHECK(!state.BodyHitTestable(80));
    CHECK(state.Sample(400) == 1.0);
}

TEST(dock_reduced_motion_and_immediate_geometry_are_synchronous) {
    renderer::dock::State state;
    state.SetReducedMotion(true, 0);
    state.Toggle(10);
    CHECK(state.Sample(10) == 0.0);
    CHECK(!state.NeedsFrame());
    state.Immediate(true, false, 20);
    CHECK(state.Sample(20) == 1.0);
    CHECK(state.Opacity(20) == 1.0);
}

TEST(connection_pulse_runs_only_for_visible_connected_reduced_motion_enabled_dock) {
    const auto a = renderer::dock::State::ConnectionPulse(0, true, true, false);
    const auto b = renderer::dock::State::ConnectionPulse(600, true, true, false);
    CHECK(a != b);
    CHECK(renderer::dock::State::ConnectionPulse(600, false, true, false) == 1.0);
    CHECK(renderer::dock::State::ConnectionPulse(600, true, false, false) == 1.0);
    CHECK(renderer::dock::State::ConnectionPulse(600, true, true, true) == 1.0);
}
