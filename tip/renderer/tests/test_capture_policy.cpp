#include "../capture.h"
#include "../surfaces.h"
#include "../resource_lifetime.h"
#include "../live_policy.h"
#include "../../tests/test_harness.h"
#include <vector>

namespace {
struct ResourceProbe {
    std::vector<int>* sequence;
    int value;
    unsigned references=1;
    void Reset() { if (references) --references; sequence->push_back(value); }
};
struct SurfaceProbe {
    ResourceProbe desktopSourceBitmap,desktopSourceTexture;
    void hide() { renderer::ResetResources(desktopSourceBitmap,desktopSourceTexture); }
};
struct CaptureProbe {
    ResourceProbe frame,moveScratch,duplication;
    void shutdown() { renderer::ResetResources(frame,moveScratch,duplication); }
};
struct SessionProbe {
    std::vector<int>* sequence;
    int visible=1;
    void Revoke() { visible=0; sequence->push_back(31); }
};
struct PreparedTextProbe {
    ResourceProbe wordLayout,phraseLayout;
    void Reset() { renderer::ResetResources(wordLayout,phraseLayout); }
};
struct FrameViewProbe {
    ResourceProbe shaderView;
    void Reset() { shaderView.Reset(); }
};
struct MaterialProbe {
    ResourceProbe blurTarget,glassTarget,lensShader;
    void reset() { renderer::ResetResources(blurTarget,glassTarget,lensShader); }
};
}

TEST(capture_crop_accounts_for_negative_desktop_origin_and_padding) {
    renderer::Capture capture;
    capture.desktop={-1920,0,0,1080};
    capture.outputWidth=1920; capture.outputHeight=1080;
    RECT crop{};
    CHECK(capture.panelCrop(RECT{-1800,100,-1700,200},10,&crop));
    CHECK_EQ(crop.left,110L); CHECK_EQ(crop.top,90L);
    CHECK_EQ(crop.right,230L); CHECK_EQ(crop.bottom,210L);
}

TEST(capture_crop_maps_each_supported_output_rotation) {
    renderer::Capture capture;
    RECT crop{};
    capture.desktop={0,0,1920,1080}; capture.outputWidth=1920; capture.outputHeight=1080;
    capture.rotation=DXGI_MODE_ROTATION_ROTATE180;
    CHECK(capture.panelCrop(RECT{100,200,300,400},0,&crop));
    CHECK_EQ(crop.left,1620L); CHECK_EQ(crop.top,680L);
    CHECK_EQ(crop.right,1820L); CHECK_EQ(crop.bottom,880L);
    capture.desktop={0,0,1080,1920}; capture.outputWidth=1920; capture.outputHeight=1080;
    capture.rotation=DXGI_MODE_ROTATION_ROTATE90;
    CHECK(capture.panelCrop(RECT{100,200,300,400},0,&crop));
    CHECK_EQ(crop.left,200L); CHECK_EQ(crop.top,780L);
    CHECK_EQ(crop.right,400L); CHECK_EQ(crop.bottom,980L);
    capture.rotation=DXGI_MODE_ROTATION_ROTATE270;
    CHECK(capture.panelCrop(RECT{100,200,300,400},0,&crop));
    CHECK_EQ(crop.left,1520L); CHECK_EQ(crop.top,100L);
    CHECK_EQ(crop.right,1720L); CHECK_EQ(crop.bottom,300L);
}

TEST(capture_dirty_and_move_rectangles_only_invalidate_intersecting_panels) {
    renderer::Capture capture;
    capture.desktop={0,0,800,600}; capture.outputWidth=800; capture.outputHeight=600;
    capture.frameUpdated=true;
    DXGI_OUTDUPL_MOVE_RECT move{}; move.SourcePoint={10,10}; move.DestinationRect={210,210,240,240};
    capture.recordMoveUpdate(move); // production helper records both source and destination
    capture.recordDirtyUpdate(RECT{35,35,45,45});
    CHECK(capture.updatesPanel(RECT{20,20,60,60},0));
    CHECK(capture.updatesPanel(RECT{220,220,230,230},0));
    CHECK(!capture.updatesPanel(RECT{100,100,180,180},0));
    CHECK(capture.intersectingUpdate(RECT{30,30,50,50},RECT{40,40,70,70}));
    CHECK(!capture.intersectingUpdate(RECT{30,30,50,50},RECT{50,50,70,70}));
}

TEST(device_removed_retry_policy_allows_only_one_recreation) {
    renderer::Capture capture;
    capture.failure=renderer::CaptureFailure::device_removed;
    CHECK(capture.permitDeviceRecreation());
    CHECK(!capture.permitDeviceRecreation());
}

TEST(live_sampling_requires_both_surface_exclusions) {
    CHECK(renderer::SurfaceWindows::CaptureExcluded(true,true));
    CHECK(!renderer::SurfaceWindows::CaptureExcluded(true,false));
    CHECK(!renderer::SurfaceWindows::CaptureExcluded(false,true));
    CHECK(!renderer::SurfaceWindows::CaptureExcluded(false,false));
}

TEST(live_capture_requires_the_windows_10_2004_build_floor) {
    CHECK(!renderer::SupportsLiveWindowsBuild(10,0,18363));
    CHECK(renderer::SupportsLiveWindowsBuild(10,0,19041));
    CHECK(renderer::SupportsLiveWindowsBuild(10,0,22631));
    CHECK(renderer::SupportsLiveWindowsBuild(10,1,19040));
    CHECK(renderer::SupportsLiveWindowsBuild(11,0,22000));
}

TEST(hidden_and_failed_capture_retires_each_production_owned_resource) {
    std::vector<int> sequence;
    const auto runRetirement=[&]() {
        SurfaceProbe surfaces{{&sequence,11},{&sequence,12}};
        CaptureProbe capture{{&sequence,21},{&sequence,22},{&sequence,23}};
        SessionProbe session{&sequence};
        PreparedTextProbe prepared{{&sequence,41},{&sequence,42}};
        FrameViewProbe frameView{{&sequence,51}};
        MaterialProbe material{{&sequence,61},{&sequence,62},{&sequence,63}};
        renderer::RetireCapturedDesktop(surfaces,capture,session,prepared,frameView,material);
        CHECK_EQ(surfaces.desktopSourceBitmap.references,0U);
        CHECK_EQ(surfaces.desktopSourceTexture.references,0U);
        CHECK_EQ(capture.frame.references,0U); CHECK_EQ(capture.moveScratch.references,0U);
        CHECK_EQ(capture.duplication.references,0U); CHECK_EQ(session.visible,0);
        CHECK_EQ(prepared.wordLayout.references,0U); CHECK_EQ(prepared.phraseLayout.references,0U);
        CHECK_EQ(frameView.shaderView.references,0U);
        CHECK_EQ(material.blurTarget.references,0U); CHECK_EQ(material.glassTarget.references,0U);
        CHECK_EQ(material.lensShader.references,0U);
    };
    runRetirement(); // hide path
    runRetirement(); // capture/shader failure path
    CHECK_EQ(sequence.size(),24U);
    const int expected[]{11,12,21,22,23,31,41,42,51,61,62,63,
                         11,12,21,22,23,31,41,42,51,61,62,63};
    for (std::size_t index=0;index<std::size(expected);++index)
        CHECK_EQ(sequence[index],expected[index]);
}
