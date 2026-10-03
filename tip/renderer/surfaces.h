#pragma once
#define NOMINMAX
#include <windows.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>

namespace renderer {
struct SurfaceWindows {
    HWND menu=nullptr, dock=nullptr;
    Microsoft::WRL::ComPtr<IDCompositionDevice> composition;
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice;
    Microsoft::WRL::ComPtr<IDWriteFactory> writeFactory;
    struct Panel {
        HWND window=nullptr;
        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
        Microsoft::WRL::ComPtr<IDCompositionTarget> target;
        Microsoft::WRL::ComPtr<IDCompositionVisual> visual;
        Microsoft::WRL::ComPtr<ID2D1DeviceContext> drawing;
        Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;
    } menuPanel, dockPanel;
    bool create(HINSTANCE instance, ID3D11Device* device);
    bool present(Panel& panel, const wchar_t* title, float width, float height);
    bool showDemo(const RECT& menuBounds, const RECT& dockBounds, float dpi);
    void hide();
    void destroy();
};
}
