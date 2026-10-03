#pragma once
#define NOMINMAX
#include <windows.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include "text.h"

namespace renderer {
struct SurfaceWindows {
    HWND menu=nullptr, dock=nullptr;
    Microsoft::WRL::ComPtr<IDCompositionDevice> composition;
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice;
    Microsoft::WRL::ComPtr<IDWriteFactory> writeFactory;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> glassSourceTexture;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> glassSourceBitmap;
    struct Panel {
        HWND window=nullptr;
        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
        Microsoft::WRL::ComPtr<IDCompositionTarget> target;
        Microsoft::WRL::ComPtr<IDCompositionVisual> visual;
        Microsoft::WRL::ComPtr<ID2D1DeviceContext> drawing;
        Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;
    } menuPanel, dockPanel;
    bool captureExcluded_=false;
    bool create(HINSTANCE instance, ID3D11Device* device);
    static bool CaptureExcluded(bool menu, bool dock) { return menu && dock; }
    bool captureExcluded() const { return captureExcluded_; }
    bool present(Panel& panel, const wchar_t* title, float width, float height,
                 bool systemColors = false, float dpi = 96.0f);
    bool presentGlass(Panel& panel, const RECT& bounds,
                     ID3D11Texture2D* glass, const RECT& source, float dpi,
                     DXGI_MODE_ROTATION rotation,
                     const completionist::render::Snapshot& snapshot,
                     const completionist::layout::Layout& layout,
                     const text::PreparedText& prepared,text::TextRenderer& textRenderer,
                     const palette::Theme& colors,text::Surface surface);
    bool showGlassSnapshot(const completionist::render::Snapshot& snapshot,
                       const completionist::layout::Layout& layout, const text::PreparedText& prepared,
                       text::TextRenderer& textRenderer, const palette::Theme& colors,
                       ID3D11Texture2D* glass, const RECT& menuSource, const RECT& dockSource,
                       float dpi, DXGI_MODE_ROTATION rotation);
    bool showDemo(const RECT& menuBounds, const RECT& dockBounds, float dpi, bool systemColors = false);
    void hide();
    void clearBackdrop();
    void destroy();
};
}
