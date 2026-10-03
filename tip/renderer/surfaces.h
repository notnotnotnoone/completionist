#pragma once
#define NOMINMAX
#include <windows.h>
#include <dcomp.h>
#include <d3d11.h>
#include <wrl/client.h>

namespace renderer {
struct SurfaceWindows {
    HWND menu=nullptr, dock=nullptr;
    Microsoft::WRL::ComPtr<IDCompositionDevice> composition;
    bool create(HINSTANCE instance);
    void hide();
    void destroy();
};
}
