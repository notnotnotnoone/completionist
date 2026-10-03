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
#include <future>
#include <memory>
#include <mutex>
#include <thread>

#include "production_service.h"
#include "production_renderer_pipe.h"
#include "capture.h"
#include "material.h"
#include "surfaces.h"
#include "text.h"
#include "live_policy.h"

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
        if (!renderer.Prepare(snapshot,330.0f*static_cast<float>(dpi)/96.0f,&prepared_)) return PresentOpaque(snapshot,host,monitor,false);
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
        conditions.supportedSession=GetSystemMetrics(SM_REMOTESESSION)==0;
        conditions.windowsExcluded=surfaces_.captureExcluded();
        conditions.captureAvailable=captureReady_;

        if (!captureReady_ && !highContrast && composition && GetSystemMetrics(SM_REMOTESESSION)==0 &&
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
        } else { mode=MaterialMode::Opaque; currentMode_=mode; }
            }
        }
        if (mode!=MaterialMode::Glass || !capture_.hasFrame) return PresentOpaque(snapshot,host,monitor,highContrast);
        return PresentGlass(snapshot,layout,dpi,renderer);
    }

    void Hide() {
        surfaces_.hide();
        RetireCapture();
        prepared_.Reset();
        current_ = {};
        hasCurrent_ = false;
        currentMode_=MaterialMode::Hidden;
    }

    void ToggleDock() {
        if (!surfaces_.ConsumeDockToggleRequest()) return;
        if (hasCurrent_) Present(current_);
        if (surfaces_.DockState().NeedsFrame()) SetTimer(surfaces_.dockPanel.window,kAnimationTimer,16,nullptr);
    }

    void TickAnimation() {
        if (!hasCurrent_) { KillTimer(surfaces_.dockPanel.window,kAnimationTimer); return; }
        if (!surfaces_.DockState().NeedsFrame()) { KillTimer(surfaces_.dockPanel.window,kAnimationTimer); Present(current_); return; }
        Present(current_);
    }

    void OnSystemChange() {
        BOOL animations=TRUE;
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animations,0);
        surfaces_.DockState().SetReducedMotion(!animations,GetTickCount64());
        if (hasCurrent_) Present(current_);
    }

private:
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
        if (!renderer.Prepare(snapshot,330.0f*static_cast<float>(dpi)/96.0f,&prepared_)) { surfaces_.hide(); return false; }
        auto layout=completionist::layout::Place(snapshot,work,prepared_.metrics);
        UpdateDockGeometry(layout);
        RetireCapture();
        current_=snapshot; hasCurrent_=true;
        return surfaces_.showOpaqueSnapshot(snapshot,layout,prepared_,renderer,highContrast,static_cast<float>(dpi));
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
        const UINT padding=static_cast<UINT>(40U*dpi/96U);
        if (!capture_.panelCrop(menu,padding,&menuSource) || !capture_.panelCrop(dock,padding,&dockSource)) return false;
        const RECT regions[]{menuSource,dockSource};
        if (!material_.blurRegions(context_.Get(),frameView_.Get(),static_cast<float>(dpi),regions,2)) return false;
        const D3D11_VIEWPORT menuViewport{static_cast<float>(menuSource.left),static_cast<float>(menuSource.top),
            static_cast<float>(menuSource.right-menuSource.left),static_cast<float>(menuSource.bottom-menuSource.top),0,1};
        const D3D11_VIEWPORT dockViewport{static_cast<float>(dockSource.left),static_cast<float>(dockSource.top),
            static_cast<float>(dockSource.right-dockSource.left),static_cast<float>(dockSource.bottom-dockSource.top),0,1};
        const float tint[]{0.035f,0.32f,0.20f,0.18f};
        if (!material_.renderLens(context_.Get(),menuViewport,26,18,static_cast<float>(dpi),tint,true) ||
            !material_.renderLens(context_.Get(),dockViewport,16,8,static_cast<float>(dpi),tint,false)) return false;
        context_->Flush();
        current_=snapshot; hasCurrent_=true;
        const bool shown=surfaces_.showGlassSnapshot(snapshot,layout,prepared_,renderer,palette::kLight,
            material_.glassTexture.Get(),menuSource,dockSource,static_cast<float>(dpi),capture_.rotation);
        if (shown) captureReady_=true;
        return shown;
    }

    void UpdateDockGeometry(const completionist::layout::Layout& layout) {
        const uint64_t now=GetTickCount64();
        const bool collapsed=layout.dockCollapsed;
        if (!dockGeometryReady_ || collapsed!=lastAutoCollapsed_) {
            surfaces_.DockState().Immediate(!collapsed,collapsed,now);
            lastAutoCollapsed_=collapsed;
            dockGeometryReady_=true;
        }
    }

    void RetireCapture() {
        surfaces_.clearBackdrop();
        capture_.shutdown();
        captureReady_=false;
        frameView_.Reset();
        material_.reset();
    }

    void ResetGraphics() {
        surfaces_.destroy();
        RetireCapture();
        prepared_.Reset();
        context_.Reset(); device_.Reset(); adapter_.Reset();
        activeMonitor_=nullptr; graphicsReady_=false;
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
    bool graphicsReady_=false, captureReady_=false, deviceRecreationUsed_=false;
    MaterialMode currentMode_=MaterialMode::Hidden;
    bool dockGeometryReady_=false, lastAutoCollapsed_=false;
    completionist::render::Snapshot current_{};
    bool hasCurrent_=false;
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
}  // namespace

int runProductionService() {
    const HRESULT comResult=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if (FAILED(comResult)) return ERROR_FUNCTION_FAILED;
    const auto previous=SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!previous && !AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) { CoUninitialize(); return ERROR_ACCESS_DENIED; }
    MSG queued{}; PeekMessageW(&queued,nullptr,WM_USER,WM_USER,PM_NOREMOVE);
    const DWORD threadId=GetCurrentThreadId();
    HANDLE stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if (!stopEvent) return ERROR_NOT_ENOUGH_MEMORY;
    HostRenderer host;
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
            host.OnSystemChange();
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
