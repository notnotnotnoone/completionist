#include "../live_policy.h"
#include "../../tests/test_harness.h"

TEST(material_policy_hides_locked_or_invisible_surfaces) {
    renderer::MaterialConditions conditions{};
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Hidden);
    conditions.visible = true;
    conditions.locked = true;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Hidden);
}

TEST(material_policy_uses_opaque_surface_for_every_unsupported_glass_condition) {
    renderer::MaterialConditions conditions{};
    conditions.visible = true;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Glass);
    conditions.highContrast = true;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Opaque);
    conditions.highContrast = false; conditions.transparencyEnabled = false;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Opaque);
    conditions.transparencyEnabled = true; conditions.supportedHdr = true;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Opaque);
    conditions.supportedHdr = false; conditions.supportedSession = false;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Opaque);
    conditions.supportedSession = true; conditions.captureAvailable = false;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Opaque);
    conditions.captureAvailable = true; conditions.windowsExcluded = false;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Opaque);
}

TEST(material_transition_retires_captured_resources_before_opaque_or_hidden) {
    renderer::MaterialConditions glass{};
    glass.visible = true;
    int retired = 0;
    auto mode = renderer::ApplyMaterialMode(renderer::MaterialMode::Opaque, glass, [&] { ++retired; });
    CHECK(mode == renderer::MaterialMode::Glass);
    glass.windowsExcluded = false;
    mode = renderer::ApplyMaterialMode(mode, glass, [&] { ++retired; });
    CHECK(mode == renderer::MaterialMode::Opaque);
    CHECK_EQ(retired, 1);
    glass.visible = false;
    mode = renderer::ApplyMaterialMode(mode, glass, [&] { ++retired; });
    CHECK(mode == renderer::MaterialMode::Hidden);
    CHECK_EQ(retired, 1);
}
