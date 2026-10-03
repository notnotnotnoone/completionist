#include "../capture.h"
#include "../surfaces.h"
#include "../resource_lifetime.h"
#include "../../tests/test_harness.h"
#include <vector>

namespace {
struct ResetProbe {
    std::vector<int>* sequence;
    int value;
    void Reset() { sequence->push_back(value); }
};
}

TEST(capture_crop_accounts_for_negative_desktop_origin_and_padding) {
    renderer::Capture capture;
    capture.desktop={-1920,0,0,1080};
    capture.outputWidth=1920; capture.outputHeight=1080;
    RECT crop{};
    CHECK(capture.panelCrop(RECT{-1800,100,-1700,200},10,&crop));
    CHECK_EQ(crop.left,110L); CHECK_EQ(crop.top,90L);
    CHECK_EQ(crop.right,260L); CHECK_EQ(crop.bottom,210L);
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
    CHECK_EQ(crop.left,1520L); CHECK_EQ(crop.top,200L);
    CHECK_EQ(crop.right,1720L); CHECK_EQ(crop.bottom,400L);
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

TEST(capture_failures_release_retained_gpu_frame_and_bound_device_retry) {
    for (renderer::CaptureFailure reason : {renderer::CaptureFailure::unsupported_hdr,
            renderer::CaptureFailure::access_lost,renderer::CaptureFailure::unavailable}) {
        renderer::Capture capture;
        capture.hasFrame=true; capture.frameUpdated=true; capture.changedRects={{0,0,20,20}};
        capture.invalidate(reason);
        CHECK(!capture.hasFrame); CHECK(!capture.frameUpdated);
        CHECK(capture.changedRects.empty()); CHECK_EQ(capture.failure,reason);
    }
    renderer::Capture capture;
    capture.hasFrame=true; capture.changedRects={{0,0,1,1}};
    capture.invalidate(renderer::CaptureFailure::device_removed);
    CHECK(!capture.hasFrame); CHECK(capture.permitDeviceRecreation());
    CHECK(!capture.permitDeviceRecreation());
}

TEST(live_sampling_requires_both_surface_exclusions) {
    CHECK(renderer::SurfaceWindows::CaptureExcluded(true,true));
    CHECK(!renderer::SurfaceWindows::CaptureExcluded(true,false));
    CHECK(!renderer::SurfaceWindows::CaptureExcluded(false,true));
    CHECK(!renderer::SurfaceWindows::CaptureExcluded(false,false));
}

TEST(hidden_and_failed_capture_uses_shared_surface_capture_derived_retirement_order) {
    std::vector<int> sequence;
    ResetProbe frame{&sequence,2},material{&sequence,3};
    renderer::RetireCapturedDesktop(
        [&]{sequence.push_back(1);},
        [&]{renderer::ResetResources(frame);},
        [&]{renderer::ResetResources(material);});
    CHECK_EQ(sequence.size(),3U);
    CHECK_EQ(sequence[0],1); CHECK_EQ(sequence[1],2); CHECK_EQ(sequence[2],3);
}
