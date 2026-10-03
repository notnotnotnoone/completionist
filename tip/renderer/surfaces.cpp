#include "surfaces.h"
#include <d3d11.h>
#include <dcomp.h>

namespace renderer {
static LRESULT CALLBACK windowProc(HWND w,UINT m,WPARAM a,LPARAM b){if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(m==WM_NCHITTEST)return HTTRANSPARENT;if(m==WM_ERASEBKGND)return 1;return DefWindowProcW(w,m,a,b);}
bool SurfaceWindows::create(HINSTANCE instance){
    WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=windowProc;wc.hInstance=instance;wc.lpszClassName=L"CompletionistRendererV2";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
    constexpr DWORD style=WS_POPUP;constexpr DWORD ex=WS_EX_NOACTIVATE|WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW|WS_EX_TOPMOST;
    menu=CreateWindowExW(ex,wc.lpszClassName,L"Completionist menu",style,0,0,1,1,nullptr,nullptr,instance,nullptr);
    dock=CreateWindowExW(ex,wc.lpszClassName,L"Completionist dock",style,0,0,1,1,nullptr,nullptr,instance,nullptr);
    if(!menu||!dock){destroy();return false;}
    if(FAILED(DCompositionCreateDevice(nullptr,IID_PPV_ARGS(&composition)))){destroy();return false;}
    const bool excluded=SetWindowDisplayAffinity(menu,WDA_EXCLUDEFROMCAPTURE)&&SetWindowDisplayAffinity(dock,WDA_EXCLUDEFROMCAPTURE);
    if(!excluded){destroy();return false;}
    ShowWindow(menu,SW_SHOWNOACTIVATE);ShowWindow(dock,SW_SHOWNOACTIVATE);hide();return true;
}
void SurfaceWindows::hide(){if(menu)ShowWindow(menu,SW_HIDE);if(dock)ShowWindow(dock,SW_HIDE);}
void SurfaceWindows::destroy(){hide();if(composition){composition->Commit();composition.Reset();}if(menu){DestroyWindow(menu);menu=nullptr;}if(dock){DestroyWindow(dock);dock=nullptr;}}
}
