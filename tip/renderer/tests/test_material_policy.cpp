#include "../live_policy.h"
#include "../output_color_space.h"
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
    conditions.outputColorSpace = renderer::OutputColorSpace::Sdr709;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Glass);
    conditions.highContrast = true;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Opaque);
    conditions.highContrast = false; conditions.transparencyEnabled = false;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Opaque);
    conditions.transparencyEnabled = true;
    conditions.outputColorSpace = renderer::OutputColorSpace::Unsupported;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Opaque);
    conditions.outputColorSpace = renderer::OutputColorSpace::Unknown;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Opaque);
    conditions.outputColorSpace = renderer::OutputColorSpace::Sdr709; conditions.supportedSession = false;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Opaque);
    conditions.supportedSession = true; conditions.captureAvailable = false;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Opaque);
    conditions.captureAvailable = true; conditions.windowsExcluded = false;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Opaque);
}

TEST(output_color_space_classification_keeps_known_sdr_and_fails_closed_for_unknown_or_hdr) {
    CHECK(renderer::IsSupportedSdr709ColorSpace(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709));
    CHECK(renderer::IsSupportedSdr709ColorSpace(DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709));
    CHECK(!renderer::IsSupportedSdr709ColorSpace(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020));
    CHECK(!renderer::IsSupportedSdr709ColorSpace(static_cast<DXGI_COLOR_SPACE_TYPE>(0xFFFFFFFF)));
    CHECK(renderer::ClassifyOutputColorSpace(true, true) == renderer::OutputColorSpace::Sdr709);
    CHECK(renderer::ClassifyOutputColorSpace(true, false) == renderer::OutputColorSpace::Unsupported);
    CHECK(renderer::ClassifyOutputColorSpace(false, true) == renderer::OutputColorSpace::Unknown);
    renderer::MaterialConditions conditions{};
    conditions.visible = true;
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Opaque);
    conditions.outputColorSpace = renderer::ClassifyOutputColorSpace(true, true);
    CHECK(renderer::ChooseMaterialMode(conditions) == renderer::MaterialMode::Glass);
}

TEST(material_transition_retires_captured_resources_before_opaque_or_hidden) {
    renderer::MaterialConditions glass{};
    glass.visible = true;
    glass.outputColorSpace = renderer::OutputColorSpace::Sdr709;
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
