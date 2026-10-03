#define NOMINMAX
#include <windows.h>
#include <wtsapi32.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <dwmapi.h>
#include <algorithm>
#include <atomic>
#include <iterator>
#include <memory>
#include <thread>
#include "live_fixture.h"
#include "capture.h"
#include "material.h"
#include "surfaces.h"
#include "session.h"
#include "resource_lifetime.h"

namespace renderer {
namespace {
using Microsoft::WRL::ComPtr;
constexpr UINT kWakeMessage=WM_APP+17;
struct HostState { HWND window=nullptr; HANDLE wake=nullptr; std::atomic<bool> running{true}; std::atomic<uint64_t> layoutRevision{0}; };
HostState* gHost=nullptr;

LRESULT CALLBACK HostProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto* state=reinterpret_cast<HostState*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if (message==WM_NCCREATE) {
        auto* create=reinterpret_cast<CREATESTRUCTW*>(l);
        state=static_cast<HostState*>(create->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));
        if (state) state->window=window;
    }
    if (message==WM_PAINT) {
        PAINTSTRUCT ps{}; HDC dc=BeginPaint(window,&ps);
        RECT client{}; GetClientRect(window,&client);
        FillRect(dc,&client,GetSysColorBrush(COLOR_WINDOW));
        SetBkMode(dc,TRANSPARENT); SetTextColor(dc,GetSysColor(COLOR_WINDOWTEXT));
        constexpr wchar_t messageText[]=L"CONTROLLED FIXTURE HOST\n\nClick this window to opt into local desktop sampling.\nThe renderer draws a generated caret and suggestion sample.\nCapture is hidden whenever this exact fixture window loses focus.";
        DrawTextW(dc,messageText,static_cast<int>(std::size(messageText)-1),&client,DT_LEFT|DT_TOP|DT_WORDBREAK);
        HPEN caret=CreatePen(PS_SOLID,2,GetSysColor(COLOR_WINDOWTEXT));
        HGDIOBJ oldPen=SelectObject(dc,caret);
        MoveToEx(dc,300,280,nullptr); LineTo(dc,300,304);
        SelectObject(dc,oldPen); DeleteObject(caret);
        EndPaint(window,&ps); return 0;
    }
    if (message==WM_WTSSESSION_CHANGE) {
        if (state && state->wake && (w==WTS_SESSION_LOCK || w==WTS_SESSION_LOGOFF ||
            w==WTS_CONSOLE_DISCONNECT || w==WTS_REMOTE_DISCONNECT)) SetEvent(state->wake);
        return 0;
    }
    if (message==WM_MOVE || message==WM_SIZE || message==WM_DPICHANGED) {
        if(state) { ++state->layoutRevision; if(state->wake) SetEvent(state->wake); }
    }
    if(message==WM_SETTINGCHANGE && state && state->wake) SetEvent(state->wake);
    if (message==WM_CLOSE) { if (state) state->running=false; DestroyWindow(window); return 0; }
    if (message==WM_DESTROY) { if (state) state->running=false; PostQuitMessage(0); return 0; }
    return DefWindowProcW(window,message,w,l);
}

void CALLBACK ForegroundEvent(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD) {
    if (gHost && gHost->wake) SetEvent(gHost->wake);
}

bool SameRect(const RECT& a,const RECT& b) {
    return a.left==b.left && a.top==b.top && a.right==b.right && a.bottom==b.bottom;
}

bool DeviceForMonitor(HMONITOR monitor,ComPtr<IDXGIAdapter1>* selected,UINT* outputIndex,
                      ComPtr<ID3D11Device>* device,ComPtr<ID3D11DeviceContext>* context) {
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return false;
    for (UINT ai=0;;++ai) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(ai,&adapter)==DXGI_ERROR_NOT_FOUND) break;
        for (UINT oi=0;;++oi) {
            ComPtr<IDXGIOutput> output;
            if (adapter->EnumOutputs(oi,&output)==DXGI_ERROR_NOT_FOUND) break;
            DXGI_OUTPUT_DESC desc{};
            if (SUCCEEDED(output->GetDesc(&desc)) && desc.Monitor==monitor) {
                D3D_FEATURE_LEVEL feature{};
                if (FAILED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,
                    D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,device->GetAddressOf(),
                    &feature,context->GetAddressOf()))) return false;
                *selected=adapter; *outputIndex=oi; return true;
            }
        }
    }
    return false;
}

completionist::render::Rect RenderRect(const RECT& rect) {
    return {rect.left,rect.top,rect.right,rect.bottom};
}

completionist::render::Snapshot FixtureSnapshot(HWND host,uint64_t generation,uint64_t revision) {
    completionist::render::Snapshot snapshot{};
    snapshot.owner.pid=GetCurrentProcessId();
    snapshot.owner.hostHwnd=reinterpret_cast<uint64_t>(host);
    snapshot.owner.session="controlled-glass-fixture";
    snapshot.owner.generation=generation;
    snapshot.revision=revision;
    POINT caret{300,280};
    ClientToScreen(host,&caret);
    snapshot.caret={caret.x,caret.y,caret.x+2,caret.y+24};
    snapshot.words={{L"completionist","fixture",{}},{L"completion","fixture",{}},{L"completing","fixture",{}}};
    snapshot.selection=0;
    snapshot.typedFragment=L"comple";
    snapshot.phrase=L"tion sample";
    snapshot.phraseLead=L"comple";
    snapshot.partialBegin=0;
    snapshot.partialLength=6;
    snapshot.ai=completionist::render::AiState::Off;
    snapshot.engineConnected=false;
    return snapshot;
}

uint64_t MonotonicMilliseconds() { return GetTickCount64(); }

void PresentOpaqueFixture(SurfaceWindows& surfaces,HWND host,HMONITOR monitor,bool systemColors) {
    MONITORINFO info{sizeof(info)}; RECT hostBounds{};
    if (!GetMonitorInfoW(monitor,&info) || !GetWindowRect(host,&hostBounds)) { surfaces.hide(); return; }
    surfaces.clearBackdrop();
    const unsigned dpi=GetDpiForWindow(host);
    const RECT menu{hostBounds.left+90,hostBounds.top+160,hostBounds.left+420,hostBounds.top+314};
    const RECT dock{info.rcWork.left+20,info.rcWork.bottom-74,info.rcWork.left+210,info.rcWork.bottom-20};
    surfaces.showDemo(menu,dock,static_cast<float>(dpi),systemColors);
}

void GraphicsWorker(HostState* host,HINSTANCE instance) {
    // Panel HWNDs are created, presented, and destroyed on this worker. Host access is
    // limited to queries; the UI thread can signal stop and join without waiting on
    // a worker SendMessage back into its own shutdown path. AcquireNextFrame is capped at 20 ms.
    ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context; ComPtr<IDXGIAdapter1> adapter;
    Capture capture; SurfaceWindows surfaces; BlurMaterial material; ComPtr<ID3D11ShaderResourceView> frameView;
    text::PreparedText prepared; std::unique_ptr<text::TextRenderer> textRenderer;
    renderer::Session session; uint64_t sessionGeneration=1, snapshotRevision=0;
    completionist::render::Snapshot snapshot{}; completionist::layout::Layout layout{};
    HMONITOR activeMonitor=nullptr; bool ready=false; bool disabled=false; bool systemColors=false;
    RECT lastMenu{},lastDock{}; unsigned lastDpi=0; bool hasPresented=false;
    uint64_t opaqueRevision=UINT64_MAX;
    auto releaseRenderedCapture=[&]() {
        const bool hadSession=session.visible();
        RetireCapturedDesktop([&]{surfaces.hide();},[&]{capture.shutdown();},
            [&]{session.Revoke(); prepared.Reset(); ResetResources(frameView); material.reset();});
        if (hadSession) ++sessionGeneration;
        hasPresented=false;
    };
    while (host->running.load()) {
        WaitForSingleObject(host->wake,24);
        if (!host->running.load()) break;
        HWND foreground=GetForegroundWindow();
        if (foreground!=host->window || !IsWindowVisible(host->window)) {
            if (ready) { releaseRenderedCapture(); textRenderer.reset(); surfaces.destroy();
                context.Reset(); device.Reset(); adapter.Reset(); ready=false; activeMonitor=nullptr; hasPresented=false; }
            continue;
        }
        if (disabled && !ready) WaitForSingleObject(host->wake,1000);
        HMONITOR monitor=MonitorFromWindow(host->window,MONITOR_DEFAULTTONEAREST);
        if (!ready || monitor!=activeMonitor) {
            if (ready) { releaseRenderedCapture(); textRenderer.reset(); surfaces.destroy(); context.Reset(); device.Reset(); adapter.Reset(); }
            UINT outputIndex=0;
            if (!DeviceForMonitor(monitor,&adapter,&outputIndex,&device,&context) || !surfaces.create(instance,device.Get())) {
                surfaces.hide(); capture.shutdown(); textRenderer.reset(); surfaces.destroy(); device.Reset(); context.Reset(); adapter.Reset();
                disabled=true;
                continue;
            }
            textRenderer=std::make_unique<text::TextRenderer>(surfaces.writeFactory.Get());
            HIGHCONTRASTW contrast{sizeof(contrast)};
            BOOL compositionEnabled=FALSE;
            const bool highContrast=SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0) &&
                (contrast.dwFlags&HCF_HIGHCONTRASTON)!=0;
            const bool opaqueOnly=highContrast || GetSystemMetrics(SM_REMOTESESSION)!=0 ||
                FAILED(DwmIsCompositionEnabled(&compositionEnabled)) || !compositionEnabled || !surfaces.captureExcluded();
            if (disabled || opaqueOnly || !capture.initialize(device.Get(),adapter.Get(),outputIndex)) {
                systemColors=highContrast;
                activeMonitor=monitor; ready=true; disabled=true; hasPresented=false; opaqueRevision=UINT64_MAX;
                continue;
            }
            activeMonitor=monitor; ready=true; hasPresented=false;
        }
        if (disabled) {
            const uint64_t revision=host->layoutRevision.load();
            if (revision!=opaqueRevision) {
                PresentOpaqueFixture(surfaces,host->window,monitor,systemColors);
                opaqueRevision=revision;
            }
            continue;
        }
        if (session.visible()) {
            DWORD foregroundPid=0;
            GetWindowThreadProcessId(foreground,&foregroundPid);
            if (!session.Heartbeat(snapshot.owner,foregroundPid,MonotonicMilliseconds())) {
                releaseRenderedCapture();
                continue;
            }
        }
        HIGHCONTRASTW contrast{sizeof(contrast)};
        if (SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0) &&
            (contrast.dwFlags&HCF_HIGHCONTRASTON)!=0) {
            releaseRenderedCapture(); systemColors=true; disabled=true;
            opaqueRevision=UINT64_MAX; continue;
        }
        const bool acquired=capture.acquire(context.Get(),20);
        if (!acquired && capture.failure!=CaptureFailure::none) {
            if (capture.permitDeviceRecreation()) {
                releaseRenderedCapture(); textRenderer.reset(); surfaces.destroy(); device.Reset(); context.Reset(); adapter.Reset();
                ready=false; hasPresented=false;
            } else { releaseRenderedCapture(); systemColors=false; PresentOpaqueFixture(surfaces,host->window,monitor,false); disabled=true; opaqueRevision=host->layoutRevision.load(); }
            continue;
        }
        MONITORINFO info{sizeof(info)};
        if (!GetMonitorInfoW(monitor,&info)) { releaseRenderedCapture(); continue; }
        const unsigned dpi=GetDpiForWindow(host->window);
        RECT work=info.rcWork;
        const bool wasVisible=session.visible();
        if (!wasVisible) ++snapshotRevision;
        snapshot=FixtureSnapshot(host->window,sessionGeneration,snapshotRevision);
        completionist::layout::WorkArea workArea{RenderRect(work),dpi};
        DWORD foregroundPid=0; GetWindowThreadProcessId(foreground,&foregroundPid);
        if (!wasVisible && !session.Accept(snapshot,foregroundPid,MonotonicMilliseconds())) continue;
        if (!textRenderer || (!prepared.phrase && !textRenderer->Prepare(
                snapshot,330.0f*static_cast<float>(dpi)/96.0f,&prepared))) {
            releaseRenderedCapture(); systemColors=false;
            PresentOpaqueFixture(surfaces,host->window,monitor,false); disabled=true;
            opaqueRevision=host->layoutRevision.load(); continue;
        }
        layout=completionist::layout::Place(snapshot,workArea,prepared.metrics);
        const RECT menu{layout.menuBounds.left,layout.menuBounds.top,layout.menuBounds.right,layout.menuBounds.bottom};
        const RECT dock{layout.dockBounds.left,layout.dockBounds.top,layout.dockBounds.right,layout.dockBounds.bottom};
        const bool geometryChanged=!hasPresented || !SameRect(menu,lastMenu) || !SameRect(dock,lastDock) || dpi!=lastDpi;
        if (wasVisible && geometryChanged) {
            snapshot.revision=++snapshotRevision;
            session.Accept(snapshot,foregroundPid,MonotonicMilliseconds());
        }
        if (!capture.hasFrame) continue;
        const UINT padding=static_cast<UINT>(40U*dpi/96U);
        const bool updateMenu=acquired && capture.frameUpdated && capture.updatesPanel(menu,padding);
        const bool updateDock=acquired && capture.frameUpdated && capture.updatesPanel(dock,padding);
        const bool backdropChanged=updateMenu || updateDock;
        if (!backdropChanged && !geometryChanged) continue;
        if (!frameView) {
            D3D11_TEXTURE2D_DESC desc{}; capture.frame->GetDesc(&desc);
            if (!material.create(device.Get(),desc.Width,desc.Height) ||
                FAILED(device->CreateShaderResourceView(capture.frame.Get(),nullptr,&frameView))) {
                capture.invalidate(CaptureFailure::unavailable); releaseRenderedCapture(); systemColors=false; PresentOpaqueFixture(surfaces,host->window,monitor,false); disabled=true; opaqueRevision=host->layoutRevision.load(); continue;
            }
        }
        RECT menuSource{},dockSource{};
        if (!capture.panelCrop(menu,0,&menuSource) || !capture.panelCrop(dock,0,&dockSource)) {
            capture.invalidate(CaptureFailure::unavailable); releaseRenderedCapture(); systemColors=false;
            PresentOpaqueFixture(surfaces,host->window,monitor,false); disabled=true;
            opaqueRevision=host->layoutRevision.load(); continue;
        }
        RECT blurRegions[2]{}; size_t blurRegionCount=0;
        if (geometryChanged || updateMenu) blurRegions[blurRegionCount++]=menuSource;
        if (geometryChanged || updateDock) blurRegions[blurRegionCount++]=dockSource;
        if ((backdropChanged || geometryChanged) && !material.blurRegions(context.Get(),frameView.Get(),
                static_cast<float>(dpi),blurRegions,blurRegionCount)) {
            capture.invalidate(CaptureFailure::unavailable); releaseRenderedCapture(); systemColors=false;
            PresentOpaqueFixture(surfaces,host->window,monitor,false); disabled=true;
            opaqueRevision=host->layoutRevision.load(); continue;
        }
        const D3D11_VIEWPORT menuViewport{static_cast<float>(menuSource.left),static_cast<float>(menuSource.top),
            static_cast<float>(menuSource.right-menuSource.left),static_cast<float>(menuSource.bottom-menuSource.top),0,1};
        const D3D11_VIEWPORT dockViewport{static_cast<float>(dockSource.left),static_cast<float>(dockSource.top),
            static_cast<float>(dockSource.right-dockSource.left),static_cast<float>(dockSource.bottom-dockSource.top),0,1};
        const float tint[4]{0.035f,0.32f,0.20f,0.18f};
        if (!material.renderLens(context.Get(),menuViewport,26,18,static_cast<float>(dpi),tint,true) ||
            !material.renderLens(context.Get(),dockViewport,16,8,static_cast<float>(dpi),tint,false)) {
            capture.invalidate(CaptureFailure::unavailable); releaseRenderedCapture(); systemColors=false;
            PresentOpaqueFixture(surfaces,host->window,monitor,false); disabled=true;
            opaqueRevision=host->layoutRevision.load(); continue;
        }
        context->Flush();
        if (
            !surfaces.showGlassSnapshot(snapshot,layout,prepared,*textRenderer,palette::kLight,
                material.glassTexture.Get(),menuSource,dockSource,static_cast<float>(dpi),capture.rotation)) {
            capture.invalidate(CaptureFailure::unavailable); releaseRenderedCapture(); systemColors=false; PresentOpaqueFixture(surfaces,host->window,monitor,false); disabled=true; opaqueRevision=host->layoutRevision.load();
        } else {
            lastMenu=menu; lastDock=dock; lastDpi=dpi; hasPresented=true;
        }
    }
    releaseRenderedCapture(); textRenderer.reset(); surfaces.destroy();
}
}

int runLiveFixture() {
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) &&
        !AreDpiAwarenessContextsEqual(GetDpiAwarenessContextForProcess(GetCurrentProcess()),
                                     DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return ERROR_ACCESS_DENIED;
    HostState state{}; state.wake=CreateEventW(nullptr,FALSE,FALSE,nullptr); if (!state.wake) return ERROR_NOT_ENOUGH_MEMORY;
    gHost=&state;
    WNDCLASSEXW wc{sizeof(wc)}; wc.lpfnWndProc=HostProc; wc.hInstance=GetModuleHandleW(nullptr);
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW); wc.lpszClassName=L"CompletionistControlledGlassFixture";
    if (!RegisterClassExW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) { CloseHandle(state.wake); gHost=nullptr; return ERROR_FUNCTION_FAILED; }
    HWND window=CreateWindowExW(0,wc.lpszClassName,L"Completionist live material fixture",WS_OVERLAPPEDWINDOW,
        100,100,700,480,nullptr,nullptr,wc.hInstance,&state);
    if (!window) { CloseHandle(state.wake); gHost=nullptr; return ERROR_FUNCTION_FAILED; }
    WTSRegisterSessionNotification(window,NOTIFY_FOR_THIS_SESSION);
    HWINEVENTHOOK foreground=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,
        ForegroundEvent,0,0,WINEVENT_OUTOFCONTEXT);
    HWINEVENTHOOK desktop=SetWinEventHook(EVENT_SYSTEM_DESKTOPSWITCH,EVENT_SYSTEM_DESKTOPSWITCH,nullptr,
        ForegroundEvent,0,0,WINEVENT_OUTOFCONTEXT);
    if (!foreground || !desktop) { if(foreground)UnhookWinEvent(foreground); if(desktop)UnhookWinEvent(desktop);
        WTSUnRegisterSessionNotification(window); DestroyWindow(window); CloseHandle(state.wake); gHost=nullptr; return ERROR_FUNCTION_FAILED; }
    std::thread worker(GraphicsWorker,&state,wc.hInstance);
    ShowWindow(window,SW_SHOWNOACTIVATE);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message,nullptr,0,0)>0) { TranslateMessage(&message); DispatchMessageW(&message); }
    state.running=false; SetEvent(state.wake); worker.join();
    UnhookWinEvent(foreground); UnhookWinEvent(desktop); WTSUnRegisterSessionNotification(window);
    CloseHandle(state.wake); gHost=nullptr;
    return ERROR_SUCCESS;
}
}
