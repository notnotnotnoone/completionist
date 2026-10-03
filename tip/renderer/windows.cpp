#define NOMINMAX
#include "windows.h"

#include <sddl.h>
#include <securitybaseapi.h>

#include <limits>
#include <vector>

namespace renderer::win {
namespace {

uint64_t AuthenticationId(const LUID& luid) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(luid.HighPart)) << 32) | luid.LowPart;
}

bool QueryTokenBuffer(HANDLE token, TOKEN_INFORMATION_CLASS type, std::vector<unsigned char>* bytes) {
    DWORD required = 0;
    GetTokenInformation(token, type, nullptr, 0, &required);
    if (required == 0) return false;
    bytes->resize(required);
    return GetTokenInformation(token, type, bytes->data(), required, &required) != FALSE;
}

bool SidString(PSID sid, std::wstring* value) {
    LPWSTR text = nullptr;
    if (!ConvertSidToStringSidW(sid, &text)) return false;
    *value = text;
    LocalFree(text);
    return true;
}

bool TokenLogonSid(HANDLE token, std::wstring* sidText) {
    std::vector<unsigned char> data;
    if (!QueryTokenBuffer(token, TokenGroups, &data)) return false;
    auto* groups = reinterpret_cast<TOKEN_GROUPS*>(data.data());
    for (DWORD i = 0; i < groups->GroupCount; ++i) {
        const auto& group = groups->Groups[i];
        if ((group.Attributes & SE_GROUP_LOGON_ID) == SE_GROUP_LOGON_ID) return SidString(group.Sid, sidText);
    }
    return false;
}

bool IsForegroundClaim(uint64_t declaredHwnd, DWORD declaredPid, uint64_t foregroundHwnd, DWORD foregroundPid) {
    return declaredHwnd != 0 && declaredHwnd == foregroundHwnd && declaredPid != 0 && declaredPid == foregroundPid;
}

}  // namespace

RevocationHooks* RevocationHooks::active_ = nullptr;

bool SameLogon(const LogonIdentity& peer, const LogonIdentity& current) {
    return !peer.userSid.empty() && peer.userSid == current.userSid && !peer.logonSid.empty() &&
           peer.logonSid == current.logonSid && peer.authenticationId == current.authenticationId &&
           peer.sessionId == current.sessionId;
}

bool ValidatePeerClaim(const PeerClaim& claim) {
    return claim.declaredPid != 0 && claim.actualPipePid == claim.declaredPid &&
           claim.declaredHwnd != 0 && SameLogon(claim.peer, claim.current) &&
           IsForegroundClaim(claim.declaredHwnd, claim.declaredPid,
                             claim.actualForegroundHwnd, claim.actualForegroundPid);
}

bool QueryLogonIdentity(HANDLE token, LogonIdentity* identity) {
    if (!identity || token == nullptr || token == INVALID_HANDLE_VALUE) return false;
    std::vector<unsigned char> userData, statisticsData, sessionData;
    if (!QueryTokenBuffer(token, TokenUser, &userData) ||
        !QueryTokenBuffer(token, TokenStatistics, &statisticsData) ||
        !QueryTokenBuffer(token, TokenSessionId, &sessionData)) return false;
    auto* user = reinterpret_cast<TOKEN_USER*>(userData.data());
    auto* statistics = reinterpret_cast<TOKEN_STATISTICS*>(statisticsData.data());
    const DWORD session = *reinterpret_cast<const DWORD*>(sessionData.data());
    LogonIdentity result;
    if (!SidString(user->User.Sid, &result.userSid) || !TokenLogonSid(token, &result.logonSid)) return false;
    result.authenticationId = AuthenticationId(statistics->AuthenticationId);
    result.sessionId = session;
    *identity = std::move(result);
    return true;
}

HANDLE CreateLocalRendererPipe(const wchar_t* name) {
    if (!name || !*name) return INVALID_HANDLE_VALUE;
    HANDLE processToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &processToken)) return INVALID_HANDLE_VALUE;
    LogonIdentity current;
    const bool queried = QueryLogonIdentity(processToken, &current);
    CloseHandle(processToken);
    if (!queried) return INVALID_HANDLE_VALUE;

    std::wstring sddl = L"D:P(A;;GA;;;" + current.logonSid + L")";
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr))
        return INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.lpSecurityDescriptor = descriptor;
    attributes.bInheritHandle = FALSE;
    HANDLE pipe = CreateNamedPipeW(name, PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        PIPE_UNLIMITED_INSTANCES, static_cast<DWORD>(completionist::render::kMaxFrameBytes + 4),
        static_cast<DWORD>(completionist::render::kMaxFrameBytes + 4), 0, &attributes);
    LocalFree(descriptor);
    return pipe;
}

bool ValidateRendererClient(HANDLE pipe, DWORD declaredPid, uint64_t declaredHwnd) {
    if (pipe == INVALID_HANDLE_VALUE || declaredPid == 0 || declaredHwnd == 0 ||
        declaredHwnd > static_cast<uint64_t>(std::numeric_limits<UINT_PTR>::max())) return false;
    ULONG actualPid = 0;
    if (!GetNamedPipeClientProcessId(pipe, &actualPid) || actualPid != declaredPid) return false;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, actualPid);
    if (!process) return false;
    HANDLE peerToken = nullptr, currentToken = nullptr;
    bool valid = false;
    if (OpenProcessToken(process, TOKEN_QUERY, &peerToken) &&
        OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &currentToken)) {
        PeerClaim claim{};
        claim.declaredPid = declaredPid;
        claim.declaredHwnd = declaredHwnd;
        claim.actualPipePid = actualPid;
        valid = QueryLogonIdentity(peerToken, &claim.peer) && QueryLogonIdentity(currentToken, &claim.current);
        HWND hwnd = reinterpret_cast<HWND>(static_cast<UINT_PTR>(declaredHwnd));
        DWORD foregroundPid = 0;
        HWND foreground = GetForegroundWindow();
        if (foreground) GetWindowThreadProcessId(foreground, &foregroundPid);
        DWORD hostPid = 0;
        if (IsWindow(hwnd)) GetWindowThreadProcessId(hwnd, &hostPid);
        claim.actualForegroundHwnd = reinterpret_cast<uint64_t>(foreground);
        claim.actualForegroundPid = foregroundPid;
        valid = valid && hostPid == declaredPid && ValidatePeerClaim(claim);
    }
    if (peerToken) CloseHandle(peerToken);
    if (currentToken) CloseHandle(currentToken);
    CloseHandle(process);
    return valid;
}

bool ValidateRendererServer(HANDLE pipe, DWORD expectedServerPid) {
    if (pipe == INVALID_HANDLE_VALUE || expectedServerPid == 0) return false;
    ULONG actualPid = 0;
    if (!GetNamedPipeServerProcessId(pipe, &actualPid) || actualPid != expectedServerPid) return false;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, actualPid);
    if (!process) return false;
    HANDLE serverToken = nullptr, currentToken = nullptr;
    bool valid = false;
    if (OpenProcessToken(process, TOKEN_QUERY, &serverToken) &&
        OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &currentToken)) {
        LogonIdentity server, current;
        valid = QueryLogonIdentity(serverToken, &server) && QueryLogonIdentity(currentToken, &current) &&
                SameLogon(server, current);
    }
    if (serverToken) CloseHandle(serverToken);
    if (currentToken) CloseHandle(currentToken);
    CloseHandle(process);
    return valid;
}

RevocationHooks::~RevocationHooks() { Unregister(); }

bool RevocationHooks::Register(HWND notificationWindow, Session* session) {
    Unregister();
    if (!notificationWindow || !session) return false;
    notificationWindow_ = notificationWindow;
    session_ = session;
    active_ = this;
    foregroundHook_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
        WinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    desktopHook_ = SetWinEventHook(EVENT_SYSTEM_DESKTOPSWITCH, EVENT_SYSTEM_DESKTOPSWITCH, nullptr,
        WinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!foregroundHook_ || !desktopHook_ || !WTSRegisterSessionNotification(notificationWindow_, NOTIFY_FOR_THIS_SESSION)) {
        Unregister();
        return false;
    }
    return true;
}

void RevocationHooks::Unregister() {
    if (foregroundHook_) UnhookWinEvent(foregroundHook_);
    if (desktopHook_) UnhookWinEvent(desktopHook_);
    if (notificationWindow_) WTSUnRegisterSessionNotification(notificationWindow_);
    if (active_ == this) active_ = nullptr;
    foregroundHook_ = nullptr;
    desktopHook_ = nullptr;
    notificationWindow_ = nullptr;
    session_ = nullptr;
}

void CALLBACK RevocationHooks::WinEvent(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
    if (active_) active_->RevokeForSystemChange();
}

void RevocationHooks::RevokeForSystemChange() { if (session_) session_->Revoke(); }

void RevocationHooks::OnSessionChange(WPARAM change) {
    switch (change) {
        case WTS_CONSOLE_DISCONNECT:
        case WTS_REMOTE_DISCONNECT:
        case WTS_SESSION_LOGOFF:
        case WTS_SESSION_LOCK:
            RevokeForSystemChange();
            break;
        default:
            break;
    }
}

}  // namespace renderer::win
