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
#include "dock_state.h"
#include "accessibility.h"

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
        Microsoft::WRL::ComPtr<IDCompositionEffectGroup> opacity;
        Microsoft::WRL::ComPtr<ID2D1DeviceContext> drawing;
        Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;
    } menuPanel, dockPanel;
    bool captureExcluded_=false;
    bool dockToggleRequested_=false;
    int dockExpandedRequest_=-1;
    dock::State dockState_;
    accessibility::Trees accessibility_{};
    // Explicit diagnostic mode only; normal production never writes screenshots.
    std::wstring screenshotDirectory;
    bool captureNextFrame=false;
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
                     const palette::Theme& colors,text::Surface surface,float connectionOpacity = 1.0f);
    bool showGlassSnapshot(const completionist::render::Snapshot& snapshot,
                       const completionist::layout::Layout& layout, const text::PreparedText& prepared,
                       text::TextRenderer& textRenderer, const palette::Theme& colors,
                       ID3D11Texture2D* glass, const RECT& menuSource, const RECT& dockSource,
                       float dpi, DXGI_MODE_ROTATION rotation,float connectionOpacity = 1.0f,
                       const palette::Theme* dockColors = nullptr);
    bool showOpaqueSnapshot(const completionist::render::Snapshot& snapshot,
                       const completionist::layout::Layout& layout, const text::PreparedText& prepared,
                       text::TextRenderer& textRenderer, bool systemColors, float dpi,
                       float connectionOpacity = 1.0f);
    bool ApplyDockMotion(double expandedProgress,double bodyOpacity,float dpi);
    bool showDemo(const RECT& menuBounds, const RECT& dockBounds, float dpi, bool systemColors = false);
    bool ConsumeDockToggleRequest() {
        const bool value=dockToggleRequested_; dockToggleRequested_=false;
        if (dockExpandedRequest_>=0) { dockState_.SetExpanded(dockExpandedRequest_!=0,GetTickCount64()); dockExpandedRequest_=-1; }
        else if (value) dockState_.Toggle(GetTickCount64());
        return value;
    }
    dock::State& DockState() { return dockState_; }
    void hide();
    void clearBackdrop();
    void destroy();
};
}
