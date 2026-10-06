#include "../dock_state.h"
#include "../system_change.h"
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
    CHECK(state.Sample(20) == 0.0);  // geometry preserves the writer's manual minimization
    state.SetExpanded(true, 20);
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

TEST(production_dock_frame_plan_pulses_and_schedules_only_visible_connected_status) {
    renderer::dock::State state;
    const auto active = state.Frame(0, true, true);  // 600 ms is the pulse's fully opaque peak
    CHECK(active.connectionOpacity < 1.0);
    CHECK_EQ(active.nextFrameMs, 33u);
    const auto hidden = state.Frame(600, false, true);
    CHECK_EQ(hidden.connectionOpacity, 1.0);
    CHECK_EQ(hidden.nextFrameMs, 0u);
    const auto disconnected = state.Frame(600, true, false);
    CHECK_EQ(disconnected.connectionOpacity, 1.0);
    CHECK_EQ(disconnected.nextFrameMs, 0u);

    state.SetReducedMotion(true, 700);
    const auto reduced = state.Frame(1200, true, true);
    CHECK_EQ(reduced.connectionOpacity, 1.0);
    CHECK_EQ(reduced.nextFrameMs, 0u);
}

TEST(production_theme_dpi_and_display_changes_finish_dock_motion_immediately) {
    const UINT changes[]{WM_THEMECHANGED, WM_DPICHANGED, WM_DISPLAYCHANGE};
    for (const UINT message : changes) {
        renderer::dock::State state;
        state.Toggle(0);
        CHECK(state.NeedsFrame());
        renderer::dock::ApplySystemChange(state, message, 40);
        CHECK(!state.NeedsFrame());
        CHECK_EQ(state.Sample(40), 0.0);
        CHECK_EQ(state.Frame(40, true, false).nextFrameMs, 0u);
    }
}
