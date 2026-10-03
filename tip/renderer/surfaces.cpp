#include "surfaces.h"
#include "resource_lifetime.h"

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

bool resizePanel(SurfaceWindows::Panel& panel, ID2D1DeviceContext* drawing, UINT width, UINT height, float dpi) {
    if (!panel.swapChain || !width || !height || dpi<48.0f || dpi>768.0f) return false;
    drawing->SetTarget(nullptr);
    drawing->Flush();
    panel.bitmap.Reset();
    if (FAILED(panel.swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) return false;
    ComPtr<IDXGISurface> surface;
    if (FAILED(panel.swapChain->GetBuffer(0, IID_PPV_ARGS(&surface)))) return false;
    const auto properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),dpi,dpi);
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
    const bool menuExcluded=SetWindowDisplayAffinity(menuPanel.window,WDA_EXCLUDEFROMCAPTURE)!=FALSE;
    const bool dockExcluded=SetWindowDisplayAffinity(dockPanel.window,WDA_EXCLUDEFROMCAPTURE)!=FALSE;
    captureExcluded_=CaptureExcluded(menuExcluded,dockExcluded);
    if (!makePanel(device, d2dDevice.Get(), composition.Get(), menuPanel.window, menuPanel) ||
        !makePanel(device, d2dDevice.Get(), composition.Get(), dockPanel.window, dockPanel) ||
        FAILED(composition->Commit())) { destroy(); return false; }
    return true;
}

bool SurfaceWindows::present(Panel& panel, const wchar_t* title, float width, float height, bool systemColors, float dpi) {
    if (!panel.window || !panel.swapChain || !panel.drawing || !title || width < 1 || height < 1) return false;
    const UINT pixelWidth = static_cast<UINT>(std::ceil(width));
    const UINT pixelHeight = static_cast<UINT>(std::ceil(height));
    if (!panel.bitmap || pixelWidth != panel.bitmap->GetPixelSize().width ||
        pixelHeight != panel.bitmap->GetPixelSize().height) {
        if (!resizePanel(panel, panel.drawing.Get(), pixelWidth, pixelHeight,dpi)) return false;
    }
    panel.drawing->SetTarget(panel.bitmap.Get());
    panel.drawing->SetDpi(dpi,dpi);
    panel.drawing->BeginDraw();
    panel.drawing->Clear(D2D1::ColorF(0, 0.0f));
    ComPtr<ID2D1SolidColorBrush> fill;
    ComPtr<ID2D1SolidColorBrush> text;
    ComPtr<IDWriteTextFormat> format;
    const COLORREF systemFill=GetSysColor(COLOR_WINDOW),systemText=GetSysColor(COLOR_WINDOWTEXT);
    const D2D1_COLOR_F fillColor=systemColors ? D2D1::ColorF(GetRValue(systemFill)/255.0f,
        GetGValue(systemFill)/255.0f,GetBValue(systemFill)/255.0f,1.0f) :
        D2D1::ColorF(10.f/255.f,90.f/255.f,61.f/255.f,0.78f);
    const D2D1_COLOR_F textColor=systemColors ? D2D1::ColorF(GetRValue(systemText)/255.0f,
        GetGValue(systemText)/255.0f,GetBValue(systemText)/255.0f,1.0f) : D2D1::ColorF(1,1,1,1);
    const bool ready = SUCCEEDED(panel.drawing->CreateSolidColorBrush(fillColor, &fill)) &&
        SUCCEEDED(panel.drawing->CreateSolidColorBrush(textColor, &text)) &&
        SUCCEEDED(writeFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &format));
    if (ready) {
        const float scale=dpi/96.0f;
        const float dipWidth=width/scale,dipHeight=height/scale;
        const auto rect = D2D1::RoundedRect(D2D1::RectF(1, 1, dipWidth - 1, dipHeight - 1), 25, 25);
        panel.drawing->FillRoundedRectangle(rect, fill.Get());
        panel.drawing->DrawTextW(title, static_cast<UINT32>(wcslen(title)), format.Get(),
                                 D2D1::RectF(16, 12, dipWidth - 16, dipHeight - 8), text.Get(),
                                 D2D1_DRAW_TEXT_OPTIONS_CLIP, DWRITE_MEASURING_MODE_NATURAL);
    }
    const HRESULT drawResult = panel.drawing->EndDraw();
    panel.drawing->SetTarget(nullptr);
    if (!ready || FAILED(drawResult) || FAILED(panel.swapChain->Present(1, 0))) return false;
    return SUCCEEDED(composition->Commit());
}

bool SurfaceWindows::presentGlass(Panel& panel,const RECT& bounds,ID3D11Texture2D* glass,const RECT& source,
                                  float dpi,DXGI_MODE_ROTATION rotation,
                                  const completionist::render::Snapshot& snapshot,
                                  const completionist::layout::Layout& layout,const text::PreparedText& prepared,
                                  text::TextRenderer& textRenderer,const palette::Theme& colors,text::Surface surface) {
    if (!panel.window || !panel.swapChain || !panel.drawing || !glass) return false;
    const LONG width=bounds.right-bounds.left, height=bounds.bottom-bounds.top;
    if (width<=0 || height<=0 || source.right<=source.left || source.bottom<=source.top) return false;
    if (!panel.bitmap || static_cast<UINT>(width)!=panel.bitmap->GetPixelSize().width ||
        static_cast<UINT>(height)!=panel.bitmap->GetPixelSize().height) {
        if (!resizePanel(panel,panel.drawing.Get(),static_cast<UINT>(width),static_cast<UINT>(height),dpi)) return false;
    }
    if (glassSourceTexture.Get()!=glass) {
        glassSourceBitmap.Reset(); glassSourceTexture=glass;
        ComPtr<IDXGISurface> sourceSurface;
        const auto sourceProperties=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
        if (FAILED(glass->QueryInterface(IID_PPV_ARGS(&sourceSurface))) ||
            FAILED(panel.drawing->CreateBitmapFromDxgiSurface(sourceSurface.Get(),&sourceProperties,&glassSourceBitmap))) return false;
    }
    panel.drawing->SetTarget(panel.bitmap.Get());
    panel.drawing->SetDpi(dpi,dpi);
    panel.drawing->BeginDraw();
    panel.drawing->Clear(D2D1::ColorF(0,0.0f));
    const float panelWidthDip=static_cast<float>(width)*96.0f/dpi;
    const float panelHeightDip=static_cast<float>(height)*96.0f/dpi;
    float destinationWidthDip=panelWidthDip,destinationHeightDip=panelHeightDip,angle=0;
    if(rotation==DXGI_MODE_ROTATION_ROTATE90) { destinationWidthDip=panelHeightDip; destinationHeightDip=panelWidthDip; angle=-90.0f; }
    else if(rotation==DXGI_MODE_ROTATION_ROTATE270) { destinationWidthDip=panelHeightDip; destinationHeightDip=panelWidthDip; angle=90.0f; }
    else if(rotation==DXGI_MODE_ROTATION_ROTATE180) angle=180.0f;
    const D2D1_POINT_2F center{panelWidthDip*0.5f,panelHeightDip*0.5f};
    panel.drawing->SetTransform(D2D1::Matrix3x2F::Rotation(angle,center));
    const auto destination=D2D1::RectF(center.x-destinationWidthDip*0.5f,center.y-destinationHeightDip*0.5f,
        center.x+destinationWidthDip*0.5f,center.y+destinationHeightDip*0.5f);
    const auto sourceRect=D2D1::RectF(static_cast<float>(source.left),static_cast<float>(source.top),
        static_cast<float>(source.right),static_cast<float>(source.bottom));
    panel.drawing->DrawBitmap(glassSourceBitmap.Get(),destination,1.0f,D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,&sourceRect);
    panel.drawing->SetTransform(D2D1::Matrix3x2F::Identity());
    const bool ready=textRenderer.Draw(panel.drawing.Get(),snapshot,layout,prepared,colors,surface);
    const HRESULT drawn=panel.drawing->EndDraw();
    panel.drawing->SetTarget(nullptr);
    if (!ready || FAILED(drawn) || FAILED(panel.swapChain->Present(1,0))) return false;
    SetWindowPos(panel.window,HWND_TOPMOST,bounds.left,bounds.top,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    return SUCCEEDED(composition->Commit());
}

bool SurfaceWindows::showGlassSnapshot(const completionist::render::Snapshot& snapshot,
                                  const completionist::layout::Layout& layout,const text::PreparedText& prepared,
                                  text::TextRenderer& textRenderer,const palette::Theme& colors,
                                  ID3D11Texture2D* glass,const RECT& menuSource,const RECT& dockSource,
                                  float dpi,DXGI_MODE_ROTATION rotation) {
    const auto rect=[](const completionist::render::Rect& r) { return RECT{r.left,r.top,r.right,r.bottom}; };
    if (!presentGlass(menuPanel,rect(layout.menuBounds),glass,menuSource,dpi,rotation,snapshot,layout,prepared,
                      textRenderer,colors,text::Surface::Menu) ||
        !presentGlass(dockPanel,rect(layout.dockBounds),glass,dockSource,dpi,rotation,snapshot,layout,prepared,
                      textRenderer,colors,text::Surface::Dock)) {
        hide();
        return false;
    }
    return SUCCEEDED(composition->Commit());
}

bool SurfaceWindows::showDemo(const RECT& menuBounds, const RECT& dockBounds, float dpi, bool systemColors) {
    const int menuWidth = std::max(1L, menuBounds.right - menuBounds.left);
    const int menuHeight = std::max(1L, menuBounds.bottom - menuBounds.top);
    const int dockWidth = std::max(1L, dockBounds.right - dockBounds.left);
    const int dockHeight = std::max(1L, dockBounds.bottom - dockBounds.top);
    if (!present(menuPanel, L"FIXTURE  completionist", static_cast<float>(menuWidth), static_cast<float>(menuHeight),systemColors,dpi) ||
        !present(dockPanel, L"FIXTURE  Connected     Tense —", static_cast<float>(dockWidth), static_cast<float>(dockHeight),systemColors,dpi)) return false;
    SetWindowPos(menuPanel.window, HWND_TOPMOST, menuBounds.left, menuBounds.top, menuWidth, menuHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetWindowPos(dockPanel.window, HWND_TOPMOST, dockBounds.left, dockBounds.top, dockWidth, dockHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    return SUCCEEDED(composition->Commit());
}

void SurfaceWindows::hide() {
    if (menuPanel.window) ShowWindow(menuPanel.window, SW_HIDE);
    if (dockPanel.window) ShowWindow(dockPanel.window, SW_HIDE);
    clearBackdrop();
}

void SurfaceWindows::clearBackdrop() {
    ResetResources(glassSourceBitmap,glassSourceTexture);
}

void SurfaceWindows::destroy() {
    hide();
    if (composition) { composition->Commit(); composition.Reset(); }
    menuPanel.bitmap.Reset(); menuPanel.drawing.Reset(); menuPanel.visual.Reset(); menuPanel.target.Reset(); menuPanel.swapChain.Reset();
    dockPanel.bitmap.Reset(); dockPanel.drawing.Reset(); dockPanel.visual.Reset(); dockPanel.target.Reset(); dockPanel.swapChain.Reset();
    if (menuPanel.window) { DestroyWindow(menuPanel.window); menuPanel.window = nullptr; }
    if (dockPanel.window) { DestroyWindow(dockPanel.window); dockPanel.window = nullptr; }
    d2dDevice.Reset(); writeFactory.Reset(); d2dFactory.Reset();
    clearBackdrop();
    captureExcluded_=false;
}
}
