#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <dxgi1_6.h>
#include <d3d11.h>
#include <shellscalingapi.h>
#include <shcore.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <filesystem>

#include "production_service.h"
#include "production_renderer_pipe.h"
#include "capture.h"
#include "material.h"
#include "surfaces.h"
#include "text.h"
#include "live_policy.h"
#include "output_color_space.h"
#include "system_change.h"
#include "fixture.h"

namespace renderer {
namespace {
using Microsoft::WRL::ComPtr;
constexpr UINT kRenderMessage = WM_APP + 91;
constexpr UINT kAnimationTimer = 0xC051;

struct Request {
    std::atomic<bool> cancelled{false};
    bool show = false;
    completionist::render::Snapshot snapshot{};
    std::promise<bool> completed;
};

class HostRenderer {
public:
    explicit HostRenderer(const std::wstring& screenshotDirectory) { surfaces_.screenshotDirectory=screenshotDirectory; }
    bool Present(const completionist::render::Snapshot& snapshot) {
        const HWND host = reinterpret_cast<HWND>(static_cast<UINT_PTR>(snapshot.owner.hostHwnd));
        DWORD pid = 0;
        const HWND foreground = GetForegroundWindow();
        if (foreground) GetWindowThreadProcessId(foreground, &pid);
        if (foreground != host || pid != snapshot.owner.pid || !IsWindowVisible(host)) { Hide(); return false; }
        if (!DpiReady()) { Hide(); return false; }
        const RECT caret{snapshot.caret.left,snapshot.caret.top,snapshot.caret.right,snapshot.caret.bottom};
        const HMONITOR monitor = MonitorFromRect(&caret, MONITOR_DEFAULTTONEAREST);
        if (!monitor || (graphicsReady_ && monitor != activeMonitor_)) ResetGraphics();
        if (!graphicsReady_ && !CreateGraphics(monitor)) return PresentOpaque(snapshot, host, monitor, false);

        unsigned dpi=96;
        UINT dpiX=96,dpiY=96;
        if (FAILED(GetDpiForMonitor(monitor,MDT_EFFECTIVE_DPI,&dpiX,&dpiY))) { Hide(); return false; }
        dpi=dpiX;
        if (dpi < 48 || dpi > 768) { Hide(); return false; }
        MONITORINFO info{sizeof(info)};
        if (!GetMonitorInfoW(monitor,&info)) { Hide(); return false; }
        completionist::layout::WorkArea work{{info.rcWork.left,info.rcWork.top,info.rcWork.right,info.rcWork.bottom},dpi};
        prepared_.Reset();
        text::TextRenderer renderer(surfaces_.writeFactory.Get());
        if (!renderer.Prepare(snapshot,ContentWidth(snapshot,work),&prepared_)) return PresentOpaque(snapshot,host,monitor,false);
        auto layout = completionist::layout::Place(snapshot,work,prepared_.metrics);
        UpdateDockGeometry(layout);
        bool highContrast = false;
        HIGHCONTRASTW hc{sizeof(hc)};
        if (SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0)) highContrast=(hc.dwFlags&HCF_HIGHCONTRASTON)!=0;
        BOOL compositionEnabled=FALSE;
        const bool composition=SUCCEEDED(DwmIsCompositionEnabled(&compositionEnabled)) && compositionEnabled;
        MaterialConditions conditions{};
        conditions.visible=true;
        conditions.highContrast=highContrast;
        conditions.transparencyEnabled=composition;
        conditions.outputColorSpace=outputColorSpace_;
        conditions.supportedSession=GetSystemMetrics(SM_REMOTESESSION)==0;
        conditions.windowsExcluded=surfaces_.captureExcluded();
        conditions.captureAvailable=captureReady_;

        if (!captureReady_ && outputColorSpace_==OutputColorSpace::Sdr709 && !highContrast && composition && GetSystemMetrics(SM_REMOTESESSION)==0 &&
            surfaces_.captureExcluded()) {
            captureReady_=capture_.initialize(device_.Get(),adapter_.Get(),outputIndex_);
            conditions.captureAvailable=captureReady_;
        }
        MaterialMode mode=ApplyMaterialMode(currentMode_,conditions,[&]{RetireCapture();});
        currentMode_=mode;
        if (mode==MaterialMode::Glass) {
            if (!capture_.acquire(context_.Get(),20)) {
                if (capture_.failure==CaptureFailure::device_removed && !deviceRecreationUsed_) {
                    deviceRecreationUsed_=true;
                    ResetGraphics();
                    if (CreateGraphics(monitor)) {
                        captureReady_=capture_.initialize(device_.Get(),adapter_.Get(),outputIndex_) && capture_.acquire(context_.Get(),20);
                        conditions.captureAvailable=captureReady_;
                        mode=ApplyMaterialMode(currentMode_,conditions,[&]{RetireCapture();});
                        currentMode_=mode;
                    } else {
                        // The single allowed device recreation failed; do not retry
                        // implicitly through the opaque path below.
                        Hide();
                        return false;
                    }
                } else if (capture_.failure!=CaptureFailure::none) {
                    captureReady_=false; RetireCapture();
                    conditions.captureAvailable=false; mode=ChooseMaterialMode(conditions);
                    currentMode_=mode;
                } else if (!capture_.hasFrame) { mode=MaterialMode::Opaque; currentMode_=mode; }
            }
        }
        if (mode!=MaterialMode::Glass || !capture_.hasFrame) return PresentOpaque(snapshot,host,monitor,highContrast);
        return PresentGlass(snapshot,layout,dpi,renderer);
    }

    void Hide() {
        if (surfaces_.dockPanel.window) KillTimer(surfaces_.dockPanel.window,kAnimationTimer);
        surfaces_.hide();
        RetireCapture();
        prepared_.Reset();
        current_ = {};
        hasCurrent_ = false;
        currentMode_=MaterialMode::Hidden;
        capturedRevision_=UINT64_MAX;
    }

    void ToggleDock() {
        if (!surfaces_.ConsumeDockToggleRequest()) return;
        if (hasCurrent_) Present(current_);
        if (surfaces_.DockState().NeedsFrame()) SetTimer(surfaces_.dockPanel.window,kAnimationTimer,16,nullptr);
    }

    void TickAnimation() {
        if (!hasCurrent_) { KillTimer(surfaces_.dockPanel.window,kAnimationTimer); return; }
        if (!Present(current_)) KillTimer(surfaces_.dockPanel.window,kAnimationTimer);
    }

    void ReviewToggleDock() {
        surfaces_.dockToggleRequested_=true;
        ToggleDock();
    }

    void OnSystemChange(UINT message) {
        BOOL animations=TRUE;
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animations,0);
        dock::ApplySystemChange(surfaces_.DockState(),message,GetTickCount64());
        surfaces_.DockState().SetReducedMotion(!animations,GetTickCount64());
        if (hasCurrent_) Present(current_);
    }

private:
    static float ContentWidth(const completionist::render::Snapshot& snapshot,const completionist::layout::WorkArea& work) {
        const float factor=std::isfinite(snapshot.settings.width_scale) && snapshot.settings.width_scale>0
            ? static_cast<float>(snapshot.settings.width_scale) : 1.0f;
        return (std::min)(330.0f*factor,static_cast<float>(work.bounds.right-work.bounds.left)*96.0f/static_cast<float>(work.dpi));
    }
    bool DpiReady() {
        const auto current = GetThreadDpiAwarenessContext();
        if (AreDpiAwarenessContextsEqual(current,DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return true;
        const auto previous = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        return previous != nullptr;
    }

    bool CreateGraphics(HMONITOR monitor) {
        if (!monitor) return false;
        ComPtr<IDXGIFactory1> factory;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return false;
        for (UINT ai=0;;++ai) {
            ComPtr<IDXGIAdapter1> adapter;
            if (factory->EnumAdapters1(ai,&adapter)==DXGI_ERROR_NOT_FOUND) break;
            for (UINT oi=0;;++oi) {
                ComPtr<IDXGIOutput> output;
                if (adapter->EnumOutputs(oi,&output)==DXGI_ERROR_NOT_FOUND) break;
                DXGI_OUTPUT_DESC desc{};
                if (FAILED(output->GetDesc(&desc)) || desc.Monitor!=monitor) continue;
                D3D_FEATURE_LEVEL level{};
                if (FAILED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,
                    D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device_,&level,&context_))) return false;
                adapter_=adapter; outputIndex_=oi; activeMonitor_=monitor;
                outputColorSpace_=QueryOutputColorSpace(adapter_.Get(),outputIndex_);
                if (!surfaces_.create(GetModuleHandleW(nullptr),device_.Get())) { ResetGraphics(); return false; }
                graphicsReady_=true;
                return true;
            }
        }
        return false;
    }

    bool PresentOpaque(const completionist::render::Snapshot& snapshot,HWND host,HMONITOR monitor,bool highContrast) {
        if (!graphicsReady_) {
            if (!monitor || !CreateGraphics(monitor)) { surfaces_.hide(); return false; }
        }
        const unsigned dpi=GetDpiForWindow(host);
        MONITORINFO info{sizeof(info)};
        if (!GetMonitorInfoW(monitor,&info)) { surfaces_.hide(); return false; }
        completionist::layout::WorkArea work{{info.rcWork.left,info.rcWork.top,info.rcWork.right,info.rcWork.bottom},dpi};
        prepared_.Reset();
        text::TextRenderer renderer(surfaces_.writeFactory.Get());
        if (!renderer.Prepare(snapshot,ContentWidth(snapshot,work),&prepared_)) { surfaces_.hide(); return false; }
        auto layout=completionist::layout::Place(snapshot,work,prepared_.metrics);
        UpdateDockGeometry(layout);
        RetireCapture();
        current_=snapshot; hasCurrent_=true;
        const double pulse=surfaces_.DockState().Frame(GetTickCount64(),true,snapshot.engineConnected).connectionOpacity;
        const bool shown=surfaces_.showOpaqueSnapshot(snapshot,layout,prepared_,renderer,highContrast,
            static_cast<float>(dpi),static_cast<float>(pulse));
        if (shown) ScheduleDockFrames();
        else KillTimer(surfaces_.dockPanel.window,kAnimationTimer);
        return shown;
    }

    bool PresentGlass(const completionist::render::Snapshot& snapshot,const completionist::layout::Layout& layout,
                      unsigned dpi,text::TextRenderer& renderer) {
        if (!capture_.hasFrame) return false;
        if (!frameView_) {
            D3D11_TEXTURE2D_DESC desc{}; capture_.frame->GetDesc(&desc);
            if (!material_.create(device_.Get(),desc.Width,desc.Height) ||
                FAILED(device_->CreateShaderResourceView(capture_.frame.Get(),nullptr,&frameView_))) return false;
        }
        const RECT menu{layout.menuBounds.left,layout.menuBounds.top,layout.menuBounds.right,layout.menuBounds.bottom};
        const RECT dock{layout.dockBounds.left,layout.dockBounds.top,layout.dockBounds.right,layout.dockBounds.bottom};
        RECT menuSource{},dockSource{};
        // Padding belongs to blur sampling, never the displayed panel. Scaling a
        // padded crop into the HWND shifts and shrinks the desktop underneath it.
        if (!capture_.panelCrop(menu,0,&menuSource) || !capture_.panelCrop(dock,0,&dockSource)) return false;
        const RECT regions[]{menuSource,dockSource};
        if (!material_.blurRegions(context_.Get(),frameView_.Get(),static_cast<float>(dpi),regions,2)) return false;
        const D3D11_VIEWPORT menuViewport{static_cast<float>(menuSource.left),static_cast<float>(menuSource.top),
            static_cast<float>(menuSource.right-menuSource.left),static_cast<float>(menuSource.bottom-menuSource.top),0,1};
        const D3D11_VIEWPORT dockViewport{static_cast<float>(dockSource.left),static_cast<float>(dockSource.top),
            static_cast<float>(dockSource.right-dockSource.left),static_cast<float>(dockSource.bottom-dockSource.top),0,1};
        const uint64_t now=GetTickCount64();
        const bool oldMenuDark=menuDark_,oldDockDark=dockDark_;
        if(now-themeSampleAt_>=250 || menu.left!=lastThemeMenu_.left || menu.top!=lastThemeMenu_.top ||
            dock.top!=lastThemeDock_.top) {
            menuDark_=material_.backdropDark(context_.Get(),menuViewport,menuDark_);
            dockDark_=material_.backdropDark(context_.Get(),dockViewport,dockDark_);
            themeSampleAt_=now; lastThemeMenu_=menu; lastThemeDock_=dock;
        }
        const auto& menuColors=menuDark_ ? palette::kDark : palette::kLight;
        const auto& dockColors=dockDark_ ? palette::kDark : palette::kLight;
        auto tint=[](const palette::Theme& colors,bool dark,float values[4]) {
            values[0]=colors.surface.r/255.0f; values[1]=colors.surface.g/255.0f; values[2]=colors.surface.b/255.0f;
            values[3]=dark ? .35f : .24f;
        };
        float menuTint[4]{},dockTint[4]{}; tint(menuColors,menuDark_,menuTint); tint(dockColors,dockDark_,dockTint);
        POINT cursor{};
        material_.pointer[0]=material_.pointer[1]=-1;
        if(GetCursorPos(&cursor)) {
            RECT cursorCrop{}; const RECT cursorRect{cursor.x,cursor.y,cursor.x+1,cursor.y+1};
            if(capture_.panelCrop(cursorRect,0,&cursorCrop)) {
                material_.pointer[0]=static_cast<float>(cursorCrop.left); material_.pointer[1]=static_cast<float>(cursorCrop.top);
            } else material_.pointer[0]=material_.pointer[1]=-1;
        }
        if (!material_.renderLens(context_.Get(),menuViewport,26,18,static_cast<float>(dpi),menuTint,true) ||
            !material_.renderLens(context_.Get(),dockViewport,26,18,static_cast<float>(dpi),dockTint,false)) return false;
        context_->Flush();
        current_=snapshot; hasCurrent_=true;
        surfaces_.captureNextFrame=!surfaces_.screenshotDirectory.empty() &&
            (snapshot.revision!=capturedRevision_ || snapshot.owner.session!=capturedSession_ ||
             oldMenuDark!=menuDark_ || oldDockDark!=dockDark_ ||
             dock.bottom-dock.top!=capturedDockHeight_);
        if(surfaces_.captureNextFrame) {
            const std::filesystem::path directory(surfaces_.screenshotDirectory);
            saveTextureCropPng((directory/L"menu-backdrop.png").wstring(),context_.Get(),capture_.frame.Get(),menuSource);
            saveTextureCropPng((directory/L"dock-backdrop.png").wstring(),context_.Get(),capture_.frame.Get(),dockSource);
        }
        const bool shown=surfaces_.showGlassSnapshot(snapshot,layout,prepared_,renderer,menuColors,
            material_.glassTexture.Get(),menuSource,dockSource,static_cast<float>(dpi),capture_.rotation,
            static_cast<float>(surfaces_.DockState().Frame(GetTickCount64(),true,snapshot.engineConnected).connectionOpacity),&dockColors);
        if (shown && surfaces_.captureNextFrame) {
            capturedRevision_=snapshot.revision; capturedSession_=snapshot.owner.session; capturedDockHeight_=dock.bottom-dock.top;
        }
        surfaces_.captureNextFrame=false;
        if (shown) captureReady_=true;
        if (shown) ScheduleDockFrames();
        else KillTimer(surfaces_.dockPanel.window,kAnimationTimer);
        return shown;
    }

    void ScheduleDockFrames() {
        if (!surfaces_.dockPanel.window || !hasCurrent_) return;
        const auto frame=surfaces_.DockState().Frame(GetTickCount64(),true,current_.engineConnected);
        if (frame.nextFrameMs) SetTimer(surfaces_.dockPanel.window,kAnimationTimer,frame.nextFrameMs,nullptr);
        else KillTimer(surfaces_.dockPanel.window,kAnimationTimer);
    }

    void UpdateDockGeometry(completionist::layout::Layout& layout) {
        const uint64_t now=GetTickCount64();
        const bool collapsed=layout.dockCollapsed;
        if (!dockGeometryReady_ || collapsed!=lastAutoCollapsed_) {
            surfaces_.DockState().Immediate(!collapsed,collapsed,now);
            lastAutoCollapsed_=collapsed;
            dockGeometryReady_=true;
        }
        const double progress=surfaces_.DockState().Sample(now);
        const float scale=layout.dockDip.height()>0
            ? static_cast<float>(layout.dockBounds.bottom-layout.dockBounds.top)/layout.dockDip.height() : 1.0f;
        const float height=collapsed ? layout.dockDip.height() : 32.0f+22.0f*static_cast<float>(progress);
        layout.dockBounds.top=layout.dockBounds.bottom-static_cast<LONG>(std::lround(height*scale));
        layout.dockDip.bottom=height; layout.dockContent.bottom=height-6.0f;
        layout.dockCollapsed=collapsed || progress<.95;
    }

    void RetireCapture() {
        surfaces_.clearBackdrop();
        capture_.shutdown();
        captureReady_=false;
        frameView_.Reset();
        material_.reset();
        themeSampleAt_=0;
    }

    void ResetGraphics() {
        surfaces_.destroy();
        RetireCapture();
        prepared_.Reset();
        context_.Reset(); device_.Reset(); adapter_.Reset();
        activeMonitor_=nullptr; outputColorSpace_=OutputColorSpace::Unknown; graphicsReady_=false;
    }

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGIAdapter1> adapter_;
    ComPtr<ID3D11ShaderResourceView> frameView_;
    Capture capture_;
    BlurMaterial material_;
    SurfaceWindows surfaces_;
    text::PreparedText prepared_;
    HMONITOR activeMonitor_=nullptr;
    UINT outputIndex_=0;
    OutputColorSpace outputColorSpace_=OutputColorSpace::Unknown;
    bool graphicsReady_=false, captureReady_=false, deviceRecreationUsed_=false;
    MaterialMode currentMode_=MaterialMode::Hidden;
    bool dockGeometryReady_=false, lastAutoCollapsed_=false;
    completionist::render::Snapshot current_{};
    bool hasCurrent_=false;
    uint64_t capturedRevision_=UINT64_MAX;
    std::string capturedSession_;
    LONG capturedDockHeight_=-1;
    bool menuDark_=false,dockDark_=false;
    uint64_t themeSampleAt_=0;
    RECT lastThemeMenu_{},lastThemeDock_{};
};

bool DispatchRequest(DWORD threadId,std::unique_ptr<Request> request) {
    auto future=request->completed.get_future();
    auto shared=std::shared_ptr<Request>(std::move(request));
    auto* raw=new(std::nothrow) std::shared_ptr<Request>(shared);
    if (!raw) return false;
    if (!PostThreadMessageW(threadId,kRenderMessage,0,reinterpret_cast<LPARAM>(raw))) { delete raw; return false; }
    if (future.wait_for(std::chrono::seconds(5))!=std::future_status::ready) { shared->cancelled=true; return false; }
    return future.get();
}

LRESULT CALLBACK ServiceWindowProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    return DefWindowProcW(window,message,w,l);
}

struct ReviewState {
    HostRenderer renderer;
    bool dark=false;
    uint64_t revision=0;
    explicit ReviewState(const std::wstring& directory) : renderer(directory) {}
};

LRESULT CALLBACK ReviewProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto* state=reinterpret_cast<ReviewState*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if (message==WM_NCCREATE) {
        state=static_cast<ReviewState*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));
    }
    if (!state) return DefWindowProcW(window,message,w,l);
    if (message==WM_PAINT) {
        PAINTSTRUCT paint{}; HDC dc=BeginPaint(window,&paint);
        RECT client{}; GetClientRect(window,&client);
        const auto& colors=state->dark ? palette::kDark : palette::kLight;
        auto rgb=[](palette::Color c) { return RGB(c.r,c.g,c.b); };
        HBRUSH paper=CreateSolidBrush(rgb(colors.paper)); FillRect(dc,&client,paper); DeleteObject(paper);
        SetBkMode(dc,TRANSPARENT);
        const int scale=static_cast<int>(GetDpiForWindow(window));
        auto label=[&](int x,int y,const wchar_t* text,int size,palette::Color color,int weight=FW_NORMAL) {
            HFONT font=CreateFontW(-MulDiv(size,scale,96),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
            HGDIOBJ previous=SelectObject(dc,font); SetTextColor(dc,rgb(color));
            TextOutW(dc,MulDiv(x,scale,96),MulDiv(y,scale,96),text,static_cast<int>(wcslen(text)));
            SelectObject(dc,previous); DeleteObject(font);
        };
        label(44,30,L"Completionist / production glass review",19,colors.ink,FW_SEMIBOLD);
        label(44,62,L"D: light/dark. C: collapse/restore dock. Drag to check alignment. Esc: close.",13,colors.muted);
        label(65,150,L"A clearer thought.",48,colors.ink,FW_SEMIBOLD);
        label(65,222,L"I want to separate the idea from the noise.",23,colors.ink);
        const wchar_t* lines[]{L"The surface should feel like a lens: clear through its center,",
            L"curved at the edge, with light resting along the rim.",L"Words stay sharp. The world underneath bends.",
            L"A little refraction reveals the material. Too much hides the work."};
        for(int i=0;i<4;++i) label(65,282+i*43,lines[i],18,colors.muted);
        HPEN pen=CreatePen(PS_SOLID,1,rgb(colors.line)); HGDIOBJ old=SelectObject(dc,pen);
        for(int y=314;y<490;y+=43) { MoveToEx(dc,MulDiv(60,scale,96),MulDiv(y,scale,96),nullptr); LineTo(dc,client.right-40,MulDiv(y,scale,96)); }
        SelectObject(dc,old); DeleteObject(pen);
        EndPaint(window,&paint); return 0;
    }
    if (message==WM_KEYDOWN) {
        if(w==VK_ESCAPE) { DestroyWindow(window); return 0; }
        if(w=='D') { state->dark=!state->dark; ++state->revision; InvalidateRect(window,nullptr,FALSE); return 0; }
        if(w=='C') { state->renderer.ReviewToggleDock(); return 0; }
    }
    if (message==WM_MOVE || message==WM_SIZE) ++state->revision;
    if (message==WM_TIMER) {
        completionist::render::Snapshot snapshot{};
        snapshot.owner.pid=GetCurrentProcessId(); snapshot.owner.hostHwnd=reinterpret_cast<uint64_t>(window);
        snapshot.owner.session="production-glass-review"; snapshot.owner.generation=1; snapshot.revision=state->revision;
        POINT caret{MulDiv(260,static_cast<int>(GetDpiForWindow(window)),96),MulDiv(190,static_cast<int>(GetDpiForWindow(window)),96)};
        ClientToScreen(window,&caret); snapshot.caret={caret.x,caret.y,caret.x+2,caret.y+24};
        snapshot.words={{L"separate","local",{}},{L"separation","local",{}},{L"separately","learned",{}}};
        snapshot.selection=0; snapshot.typedFragment=L"seper";
        snapshot.phrase=L"the idea from the noise"; snapshot.ai=completionist::render::AiState::Scheduled;
        snapshot.waitMs=400; snapshot.engineConnected=true; snapshot.settings.font_size=12;
        state->renderer.Present(snapshot); state->renderer.ToggleDock(); return 0;
    }
    if (message==WM_DESTROY) { state->renderer.Hide(); PostQuitMessage(0); return 0; }
    return DefWindowProcW(window,message,w,l);
}
}  // namespace

int runMaterialReview(const std::wstring& directory) {
    std::error_code error; std::filesystem::create_directories(directory,error);
    if(error) return ERROR_PATH_NOT_FOUND;
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized)) return ERROR_FUNCTION_FAILED;
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    ReviewState state(directory);
    WNDCLASSEXW cls{sizeof(cls)}; cls.lpfnWndProc=ReviewProc; cls.hInstance=GetModuleHandleW(nullptr);
    cls.hCursor=LoadCursorW(nullptr,IDC_ARROW); cls.lpszClassName=L"CompletionistProductionGlassReview";
    RegisterClassExW(&cls);
    HWND window=CreateWindowExW(0,cls.lpszClassName,L"Completionist production glass review",WS_OVERLAPPEDWINDOW,
        100,100,1000,740,nullptr,nullptr,cls.hInstance,&state);
    if(!window) { CoUninitialize(); return ERROR_FUNCTION_FAILED; }
    ShowWindow(window,SW_SHOW); SetTimer(window,1,50,nullptr);
    MSG message{}; while(GetMessageW(&message,nullptr,0,0)>0) { TranslateMessage(&message); DispatchMessageW(&message); }
    CoUninitialize(); return ERROR_SUCCESS;
}

int runProductionService(const std::wstring& screenshotDirectory) {
    if (!screenshotDirectory.empty()) {
        std::error_code error;
        std::filesystem::create_directories(screenshotDirectory,error);
        if (error) return ERROR_PATH_NOT_FOUND;
    }
    const HRESULT comResult=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if (FAILED(comResult)) return ERROR_FUNCTION_FAILED;
    const auto previous=SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!previous && !AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) { CoUninitialize(); return ERROR_ACCESS_DENIED; }
    MSG queued{}; PeekMessageW(&queued,nullptr,WM_USER,WM_USER,PM_NOREMOVE);
    const DWORD threadId=GetCurrentThreadId();
    HANDLE stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if (!stopEvent) return ERROR_NOT_ENOUGH_MEMORY;
    HostRenderer host(screenshotDirectory);
    ProductionRendererPipe pipe([threadId](const completionist::render::Snapshot& snapshot) {
        auto request=std::make_unique<Request>(); request->show=true; request->snapshot=snapshot;
        return DispatchRequest(threadId,std::move(request));
    },[threadId] {
        auto request=std::make_unique<Request>();
        DispatchRequest(threadId,std::move(request));
    });
    std::thread worker([&] { pipe.Run(stopEvent); PostThreadMessageW(threadId,WM_QUIT,0,0); });
    WNDCLASSEXW wc{sizeof(wc)}; wc.lpfnWndProc=ServiceWindowProc; wc.hInstance=GetModuleHandleW(nullptr);
    wc.lpszClassName=L"CompletionistRendererServiceMessageWindow";
    RegisterClassExW(&wc);
    HWND serviceWindow=CreateWindowExW(0,wc.lpszClassName,L"Completionist renderer service",0,0,0,0,0,
        HWND_MESSAGE,nullptr,wc.hInstance,nullptr);
    (void)serviceWindow;
    int exitCode=ERROR_SUCCESS;
    bool running=true;
    while (running) {
        MSG message{};
        const BOOL result=GetMessageW(&message,nullptr,0,0);
        if (result<=0) { exitCode=result<0 ? ERROR_FUNCTION_FAILED : ERROR_SUCCESS; running=false; continue; }
        if (message.message==kRenderMessage) {
            std::unique_ptr<std::shared_ptr<Request>> wrapped(reinterpret_cast<std::shared_ptr<Request>*>(message.lParam));
            const auto request=*wrapped;
            const bool drawn=request->cancelled ? false :
                (request->show ? host.Present(request->snapshot) : (host.Hide(),true));
            request->completed.set_value(drawn);
        } else if (message.message==WM_TIMER && message.wParam==kAnimationTimer) host.TickAnimation();
        else if (message.message==WM_SETTINGCHANGE || message.message==WM_DISPLAYCHANGE || message.message==WM_DPICHANGED || message.message==WM_THEMECHANGED)
            host.OnSystemChange(message.message);
        else { TranslateMessage(&message); DispatchMessageW(&message); }
        host.ToggleDock();
    }
    SetEvent(stopEvent);
    PostThreadMessageW(threadId,WM_QUIT,0,0);
    worker.join();
    host.Hide();
    if (serviceWindow) DestroyWindow(serviceWindow);
    UnregisterClassW(wc.lpszClassName,wc.hInstance);
    CloseHandle(stopEvent);
    CoUninitialize();
    return exitCode;
}

}  // namespace renderer
