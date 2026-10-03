// Talks to the engine over \\.\pipe\completionist-engine from a worker thread, so the app's UI thread never
// blocks on I/O. One client per process, shared by every text-service instance in it.
//
//  * Send() only queues; it never blocks.
//  * Replies to keystroke requests are posted to the requester's window as WM_COMPLETIONIST_REPLY, with a
//    heap-allocated protocol::WordReply* in lParam that the receiver must delete.
//  * If the engine isn't running, requests are dropped and the client reconnects with backoff
//    (250 ms doubling to 5 s), so apps behave as if Completionist weren't there until it returns.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdint>

#include "protocol.h"

namespace completionist {

constexpr UINT WM_COMPLETIONIST_REPLY = WM_APP + 0x41;
constexpr UINT WM_COMPLETIONIST_CONNECTION = WM_APP + 0x42;

class EngineClient {
public:
    static EngineClient& Instance();

    // Reference-counted: the worker thread runs while at least one text service is active.
    void Acquire();
    void Release();

    std::uint32_t NextId();

    // Queues `request`. When `replyTo` is set, the engine's reply is posted to it as WM_COMPLETIONIST_REPLY.
    void Send(protocol::Request request, HWND replyTo);

    // Connection transitions are posted to registered popup windows; the worker never calls UI code.
    std::uint64_t RegisterWindow(HWND window);
    void UnregisterWindow(HWND window, std::uint64_t registration);

    bool connected() const { return connected_; }

    void SetPipeName(const wchar_t* name);  // for tests

private:
    EngineClient() = default;
    struct State;

    static DWORD WINAPI ThreadMain(void* self);
    void Run();
    bool Session(HANDLE pipe);  // true if the engine went away (reconnect), false to stop
    HANDLE TryConnect();
    void DispatchReply(const std::string& body);
    void DropQueued();
    void NotifyConnection(bool connected);

    State* state_ = nullptr;
    volatile LONG connected_ = 0;
};

}  // namespace completionist
