#include "surfaces.h"

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace renderer {
namespace {
using Microsoft::WRL::ComPtr;
LRESULT CALLBACK windowProc(HWND w, UINT m, WPARAM a, LPARAM b) {
    if (m == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (m == WM_NCHITTEST) return HTTRANSPARENT;
    if (m == WM_ERASEBKGND) return 1;
    return DefWindowProcW(w, m, a, b);
}

bool makePanel(ID3D11Device* device, ID2D1Device* d2d, IDCompositionDevice* composition,
               HWND window, SurfaceWindows::Panel& panel) {
    panel.window = window;
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) ||
        FAILED(dxgiDevice->GetAdapter(&adapter)) ||
        FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return false;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = 1;
    desc.Height = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    if (FAILED(factory->CreateSwapChainForComposition(device, &desc, nullptr, &panel.swapChain)) ||
        FAILED(composition->CreateTargetForHwnd(window, TRUE, &panel.target)) ||
        FAILED(composition->CreateVisual(&panel.visual)) ||
        FAILED(panel.visual->SetContent(panel.swapChain.Get())) ||
        FAILED(panel.target->SetRoot(panel.visual.Get())) ||
        FAILED(d2d->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &panel.drawing))) return false;
    return true;
}

bool resizePanel(SurfaceWindows::Panel& panel, ID2D1DeviceContext* drawing, UINT width, UINT height) {
    if (!panel.swapChain || !width || !height) return false;
    panel.bitmap.Reset();
    if (FAILED(panel.swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) return false;
    ComPtr<IDXGISurface> surface;
    if (FAILED(panel.swapChain->GetBuffer(0, IID_PPV_ARGS(&surface)))) return false;
    const auto properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    return SUCCEEDED(drawing->CreateBitmapFromDxgiSurface(surface.Get(), &properties, &panel.bitmap));
}
}

bool SurfaceWindows::create(HINSTANCE instance, ID3D11Device* device) {
    if (!device) return false;
    D2D1_FACTORY_OPTIONS options{};
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, d2dFactory.GetAddressOf())) ||
        FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(writeFactory.GetAddressOf())))) return false;
    ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) ||
        FAILED(DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&composition))) ||
        FAILED(d2dFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice))) return false;

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"CompletionistRendererV2";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    constexpr DWORD style = WS_POPUP;
    constexpr DWORD ex = WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
    menuPanel.window = CreateWindowExW(ex, wc.lpszClassName, L"Completionist menu", style,
                                      0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
    dockPanel.window = CreateWindowExW(ex, wc.lpszClassName, L"Completionist dock", style,
                                      0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
    if (!menuPanel.window || !dockPanel.window) { destroy(); return false; }
    if (!SetWindowDisplayAffinity(menuPanel.window, WDA_EXCLUDEFROMCAPTURE) ||
        !SetWindowDisplayAffinity(dockPanel.window, WDA_EXCLUDEFROMCAPTURE)) {
        destroy();
        return false;
    }
    if (!makePanel(device, d2dDevice.Get(), composition.Get(), menuPanel.window, menuPanel) ||
        !makePanel(device, d2dDevice.Get(), composition.Get(), dockPanel.window, dockPanel) ||
        FAILED(composition->Commit())) { destroy(); return false; }
    return true;
}

bool SurfaceWindows::present(Panel& panel, const wchar_t* title, float width, float height) {
    if (!panel.window || !panel.swapChain || !panel.drawing || !title || width < 1 || height < 1) return false;
    const UINT pixelWidth = static_cast<UINT>(std::ceil(width));
    const UINT pixelHeight = static_cast<UINT>(std::ceil(height));
    if (!panel.bitmap || pixelWidth != static_cast<UINT>(panel.bitmap->GetSize().width) ||
        pixelHeight != static_cast<UINT>(panel.bitmap->GetSize().height)) {
        if (!resizePanel(panel, panel.drawing.Get(), pixelWidth, pixelHeight)) return false;
    }
    panel.drawing->SetTarget(panel.bitmap.Get());
    panel.drawing->BeginDraw();
    panel.drawing->Clear(D2D1::ColorF(0, 0.0f));
    ComPtr<ID2D1SolidColorBrush> fill;
    ComPtr<ID2D1SolidColorBrush> text;
    ComPtr<IDWriteTextFormat> format;
    const bool ready = SUCCEEDED(panel.drawing->CreateSolidColorBrush(
            D2D1::ColorF(10.f / 255.f, 90.f / 255.f, 61.f / 255.f, 0.78f), &fill)) &&
        SUCCEEDED(panel.drawing->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &text)) &&
        SUCCEEDED(writeFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &format));
    if (ready) {
        const auto rect = D2D1::RoundedRect(D2D1::RectF(1, 1, width - 1, height - 1), 25, 25);
        panel.drawing->FillRoundedRectangle(rect, fill.Get());
        panel.drawing->DrawTextW(title, static_cast<UINT32>(wcslen(title)), format.Get(),
                                 D2D1::RectF(16, 12, width - 16, height - 8), text.Get(),
                                 D2D1_DRAW_TEXT_OPTIONS_CLIP, DWRITE_MEASURING_MODE_NATURAL);
    }
    const HRESULT drawResult = panel.drawing->EndDraw();
    panel.drawing->SetTarget(nullptr);
    if (!ready || FAILED(drawResult) || FAILED(panel.swapChain->Present(1, 0))) return false;
    return SUCCEEDED(composition->Commit());
}

bool SurfaceWindows::showDemo(const RECT& menuBounds, const RECT& dockBounds, float dpi) {
    const float scale = dpi / 96.0f;
    const int menuWidth = std::max(1L, menuBounds.right - menuBounds.left);
    const int menuHeight = std::max(1L, menuBounds.bottom - menuBounds.top);
    const int dockWidth = std::max(1L, dockBounds.right - dockBounds.left);
    const int dockHeight = std::max(1L, dockBounds.bottom - dockBounds.top);
    if (!present(menuPanel, L"FIXTURE  completionist", static_cast<float>(menuWidth), static_cast<float>(menuHeight)) ||
        !present(dockPanel, L"FIXTURE  Connected     Tense —", static_cast<float>(dockWidth), static_cast<float>(dockHeight))) return false;
    SetWindowPos(menuPanel.window, HWND_TOPMOST, menuBounds.left, menuBounds.top, menuWidth, menuHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetWindowPos(dockPanel.window, HWND_TOPMOST, dockBounds.left, dockBounds.top, dockWidth, dockHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    (void)scale;
    return SUCCEEDED(composition->Commit());
}

void SurfaceWindows::hide() {
    if (menuPanel.window) ShowWindow(menuPanel.window, SW_HIDE);
    if (dockPanel.window) ShowWindow(dockPanel.window, SW_HIDE);
}

void SurfaceWindows::destroy() {
    hide();
    if (composition) { composition->Commit(); composition.Reset(); }
    menuPanel.bitmap.Reset(); menuPanel.drawing.Reset(); menuPanel.visual.Reset(); menuPanel.target.Reset(); menuPanel.swapChain.Reset();
    dockPanel.bitmap.Reset(); dockPanel.drawing.Reset(); dockPanel.visual.Reset(); dockPanel.target.Reset(); dockPanel.swapChain.Reset();
    if (menuPanel.window) { DestroyWindow(menuPanel.window); menuPanel.window = nullptr; }
    if (dockPanel.window) { DestroyWindow(dockPanel.window); dockPanel.window = nullptr; }
    d2dDevice.Reset(); writeFactory.Reset(); d2dFactory.Reset();
}
}
