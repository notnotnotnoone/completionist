#pragma once

#include <utility>

namespace renderer {

template<class... Resources>
void ResetResources(Resources&... resources) {
    (resources.Reset(),...);
}

template<class HideSurfaces,class StopCapture,class ReleaseDerived>
void RetireCapturedDesktop(HideSurfaces&& hide,StopCapture&& stopCapture,ReleaseDerived&& releaseDerived) {
    std::forward<HideSurfaces>(hide)();
    std::forward<StopCapture>(stopCapture)();
    std::forward<ReleaseDerived>(releaseDerived)();
}

}
