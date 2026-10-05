#define NOMINMAX
#include <windows.h>

#include <array>
#include <algorithm>
#include <string>
#include <vector>

#include "production_renderer_pipe.h"
#include "windows.h"
#include "renderer_log.h"

namespace renderer {
namespace {
using completionist::render::Command;
using completionist::render::CommandMessage;
using completionist::render::Snapshot;

constexpr DWORD kPollMs = 20;

uint64_t NowMs() { return GetTickCount64(); }
DWORD ForegroundPid() {
    DWORD pid = 0;
    const HWND foreground = GetForegroundWindow();
    if (foreground) GetWindowThreadProcessId(foreground, &pid);
    return pid;
}

bool ReadAvailable(HANDLE pipe, std::vector<unsigned char>* pending) {
    DWORD available = 0;
    if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) return false;
    if (!available) return true;
    std::array<unsigned char, 4096> bytes{};
    while (available) {
        const DWORD take = std::min<DWORD>(available, static_cast<DWORD>(bytes.size()));
        DWORD read = 0;
        if (!ReadFile(pipe, bytes.data(), take, &read, nullptr) || !read) return false;
        if (pending->size() > completionist::render::kMaxFrameBytes + 4 - read) return false;
        pending->insert(pending->end(), bytes.begin(), bytes.begin() + read);
        available -= read;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) return false;
    }
    return true;
}

bool TakeFrame(std::vector<unsigned char>* pending, std::string* body) {
    if (pending->size() < 4) return false;
    const uint32_t length = static_cast<uint32_t>((*pending)[0]) |
        (static_cast<uint32_t>((*pending)[1]) << 8) | (static_cast<uint32_t>((*pending)[2]) << 16) |
        (static_cast<uint32_t>((*pending)[3]) << 24);
    if (!length || length > completionist::render::kMaxFrameBytes) return false;
    if (pending->size() < static_cast<std::size_t>(length) + 4) return false;
    body->assign(reinterpret_cast<const char*>(pending->data() + 4), length);
    pending->erase(pending->begin(), pending->begin() + length + 4);
    return true;
}

bool WriteFrame(HANDLE pipe, const std::string& frame) {
    std::size_t offset = 0;
    while (offset < frame.size()) {
        DWORD written = 0;
        const DWORD count = static_cast<DWORD>(std::min<std::size_t>(frame.size() - offset, 4096));
        if (!WriteFile(pipe, frame.data() + offset, count, &written, nullptr) || !written) return false;
        offset += written;
    }
    return true;
}

std::wstring PipeName() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return {};
    win::LogonIdentity identity;
    const bool valid = win::QueryLogonIdentity(token, &identity);
    CloseHandle(token);
    if (!valid) return {};
    return L"\\\\.\\pipe\\completionist-renderer-v2-" + std::to_wstring(identity.sessionId) + L"-" +
           std::to_wstring(identity.authenticationId);
}

bool Validate(const completionist::render::Identity& owner, HANDLE pipe) {
    if (!IsWindow(reinterpret_cast<HWND>(static_cast<UINT_PTR>(owner.hostHwnd)))) return false;
    return win::ValidateRendererClient(pipe, owner.pid, owner.hostHwnd);
}
}  // namespace

DWORD ProductionRendererPipe::Run(HANDLE stopEvent) {
    const std::wstring name = PipeName();
    if (name.empty() || !present_ || !hide_) return ERROR_INVALID_PARAMETER;
    while (WaitForSingleObject(stopEvent, 0) != WAIT_OBJECT_0) {
        HANDLE pipe = win::CreateLocalRendererPipe(name.c_str());
        if (pipe == INVALID_HANDLE_VALUE) return GetLastError();
        DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
        const bool configured = SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr) != FALSE;
        if (!configured) { CloseHandle(pipe); return GetLastError(); }
        bool clientConnected = false;
        while (WaitForSingleObject(stopEvent, 0) != WAIT_OBJECT_0) {
            if (ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) { clientConnected = true; break; }
            if (GetLastError() != ERROR_PIPE_LISTENING) break;
            Sleep(kPollMs);
        }
        if (WaitForSingleObject(stopEvent, 0) == WAIT_OBJECT_0) { CloseHandle(pipe); break; }
        if (!clientConnected) { CloseHandle(pipe); continue; }
        ULONG clientPid = 0;
        GetNamedPipeClientProcessId(pipe, &clientPid);
        Log(L"client connected pid=%lu", clientPid);
        ServeClient(pipe, stopEvent);
        Log(L"client released pid=%lu", clientPid);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        // ServeClient owns revocation for a peer it actually accepted. A rejected
        // or already superseded peer must never clear a newer owner's surfaces.
    }
    hide_();
    session_.Revoke();
    return ERROR_SUCCESS;
}

bool ProductionRendererPipe::ServeClient(HANDLE pipe, HANDLE stopEvent) {
    std::vector<unsigned char> pending;
    pending.reserve(4096);
    bool connected = true;
    std::optional<completionist::render::Identity> clientOwner;
    while (connected && WaitForSingleObject(stopEvent, 0) != WAIT_OBJECT_0) {
        if (const auto* current = session_.current()) {
            const HWND foreground = GetForegroundWindow();
            DWORD foregroundPid = 0;
            if (foreground) GetWindowThreadProcessId(foreground, &foregroundPid);
            if (foregroundPid != current->owner.pid ||
                reinterpret_cast<uint64_t>(foreground) != current->owner.hostHwnd) {
                Log(L"owner pid=%lu lost the foreground (now pid=%lu)", current->owner.pid, foregroundPid);
                if (session_.RevokeIfCurrent(current->owner)) hide_();
                connected = false;
                break;
            }
        }
        if (!ReadAvailable(pipe, &pending)) break;
        std::string body;
        while (!pending.empty() && pending.size() >= 4) {
            uint32_t length = static_cast<uint32_t>(pending[0]) |
                (static_cast<uint32_t>(pending[1]) << 8) | (static_cast<uint32_t>(pending[2]) << 16) |
                (static_cast<uint32_t>(pending[3]) << 24);
            if (!length || length > completionist::render::kMaxFrameBytes) { connected = false; break; }
            if (pending.size() < static_cast<std::size_t>(length) + 4) break;
            if (!TakeFrame(&pending, &body)) { connected = false; break; }
            if (auto snapshot = completionist::render::ParseShow(body)) {
                if (!Validate(snapshot->owner, pipe)) {
                    Log(L"show rev=%llu from pid=%lu rejected: client validation failed", snapshot->revision, snapshot->owner.pid);
                    connected = false; break;
                }
                const DWORD foreground = ForegroundPid();
                if (!session_.Accept(*snapshot, foreground, NowMs())) {
                    Log(L"show rev=%llu from pid=%lu refused by session (foreground pid=%lu, words=%zu, selection=%d)",
                        snapshot->revision, snapshot->owner.pid, foreground, snapshot->words.size(), snapshot->selection);
                    const auto ack = completionist::render::EncodeAck({snapshot->owner, snapshot->revision, false});
                    if (ack.empty() || !WriteFrame(pipe, ack)) { connected = false; break; }
                    continue;
                }
                clientOwner = snapshot->owner;
                bool drawn = false;
                try { drawn = present_(*snapshot); } catch (...) { drawn = false; }
                const bool stillCurrent = session_.current() &&
                    completionist::render::IsCurrentAck(*session_.current(), {snapshot->owner, snapshot->revision, true}) &&
                    Validate(snapshot->owner, pipe) && session_.Heartbeat(snapshot->owner, ForegroundPid(), NowMs());
                Log(L"show rev=%llu from pid=%lu drawn=%d current=%d", snapshot->revision, snapshot->owner.pid,
                    drawn ? 1 : 0, stillCurrent ? 1 : 0);
                if (!drawn || !stillCurrent) {
                    if (session_.RevokeIfCurrent(snapshot->owner)) hide_();
                }
                const std::string ack = completionist::render::EncodeAck(
                    {snapshot->owner, snapshot->revision, drawn && stillCurrent});
                if (ack.empty() || !WriteFrame(pipe, ack)) { connected = false; break; }
                continue;
            }
            const auto command = completionist::render::ParseCommand(body);
            if (!command || !Validate(command->owner, pipe)) {
                Log(L"command rejected (parsed=%d)", command ? 1 : 0);
                connected = false; break;
            }
            const DWORD foreground = ForegroundPid();
            if (command->command == Command::Heartbeat) {
                if (!session_.Heartbeat(command->owner, foreground, NowMs())) { connected = false; break; }
                clientOwner = command->owner;
            } else if (command->command == Command::Hide) {
                const auto* current = session_.current();
                if (current && completionist::render::SameIdentity(current->owner, command->owner) &&
                    command->revision >= current->revision) {
                    hide_();
                    session_.RevokeIfCurrent(command->owner);
                    clientOwner.reset();
                }
            } else {
                connected = false;
            }
        }
        session_.Expire(NowMs());
        if (clientOwner && !session_.visible()) {
            hide_();
            connected = false;
        }
        Sleep(kPollMs);
    }
    if (clientOwner) {
        if (session_.RevokeIfCurrent(*clientOwner)) hide_();
    }
    return connected;
}

}  // namespace renderer
