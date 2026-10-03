#pragma once

#define NOMINMAX
#include <windows.h>
#include <wtsapi32.h>

#include <cstdint>
#include <string>

#include "session.h"

namespace renderer::win {

struct LogonIdentity {
    std::wstring userSid;
    std::wstring logonSid;
    uint64_t authenticationId = 0;
    DWORD sessionId = 0;
};

struct PeerClaim {
    DWORD declaredPid = 0;
    uint64_t declaredHwnd = 0;
    DWORD actualPipePid = 0;
    LogonIdentity peer;
    LogonIdentity current;
    uint64_t actualForegroundHwnd = 0;
    DWORD actualForegroundPid = 0;
};

bool SameLogon(const LogonIdentity& peer, const LogonIdentity& current);
bool ValidatePeerClaim(const PeerClaim& claim);
bool QueryLogonIdentity(HANDLE token, LogonIdentity* identity);
HANDLE CreateLocalRendererPipe(const wchar_t* name);
bool ValidateRendererClient(HANDLE pipe, DWORD declaredPid, uint64_t declaredHwnd);
bool ValidateRendererServer(HANDLE pipe, DWORD expectedServerPid);

class RevocationHooks {
public:
    RevocationHooks() = default;
    ~RevocationHooks();
    RevocationHooks(const RevocationHooks&) = delete;
    RevocationHooks& operator=(const RevocationHooks&) = delete;
    bool Register(HWND notificationWindow, Session* session);
    void Unregister();
    // Call from the notification window's WM_WTSSESSION_CHANGE branch.
    void OnSessionChange(WPARAM change);

private:
    static void CALLBACK WinEvent(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
    static RevocationHooks* active_;
    void RevokeForSystemChange();
    HWND notificationWindow_ = nullptr;
    Session* session_ = nullptr;
    HWINEVENTHOOK foregroundHook_ = nullptr;
    HWINEVENTHOOK desktopHook_ = nullptr;
};

}  // namespace renderer::win
