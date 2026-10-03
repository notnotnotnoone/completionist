#pragma once

namespace renderer {

template<class... Resources>
void ResetResources(Resources&... resources) {
    (resources.Reset(),...);
}

template<class Surfaces,class Capture,class Session,class PreparedText,class FrameView,class Material>
void RetireCapturedDesktop(Surfaces& surfaces,Capture& capture,Session& session,PreparedText& prepared,
                           FrameView& frameView,Material& material) {
    surfaces.hide();
    capture.shutdown();
    session.Revoke();
    prepared.Reset();
    frameView.Reset();
    material.reset();
}

}
