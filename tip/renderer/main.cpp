#define NOMINMAX
#include <windows.h>
#include <cwchar>

// Until the DirectComposition presentation path lands, the executable is inert.
// This prevents an accidental desktop capture or an empty foreground window.
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    return std::wcsstr(GetCommandLineW(),L"--live") ? ERROR_NOT_SUPPORTED : ERROR_SUCCESS;
}
