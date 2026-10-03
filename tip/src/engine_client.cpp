#include "engine_client.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "log.h"

namespace completionist {

namespace {

constexpr wchar_t kDefaultPipe[] = L"\\\\.\\pipe\\completionist-engine";
constexpr DWORD kFirstBackoffMs = 250;
constexpr DWORD kMaxBackoffMs = 2000;
constexpr DWORD kWriteTimeoutMs = 1000;
constexpr std::size_t kMaxQueued = 64;
constexpr std::size_t kMaxPending = 128;

// The engine must run as the same Windows user, or typed text could be handed to another account.
bool ServerIsCurrentUser(HANDLE pipe) {
    ULONG pid = 0;
    if (!GetNamedPipeServerProcessId(pipe, &pid)) return false;

    auto userSid = [](HANDLE process, std::vector<BYTE>& out) {
        HANDLE token = nullptr;
        if (!OpenProcessToken(process, TOKEN_QUERY, &token)) return false;
        DWORD size = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &size);
        out.resize(size);
        bool ok = size && GetTokenInformation(token, TokenUser, out.data(), size, &size);
        CloseHandle(token);
        return ok;
    };

    static std::vector<BYTE> self;
    static std::once_flag once;
    std::call_once(once, [] {
        std::vector<BYTE> sid;
        HANDLE token = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            DWORD size = 0;
            GetTokenInformation(token, TokenUser, nullptr, 0, &size);
            sid.resize(size);
            if (!size || !GetTokenInformation(token, TokenUser, sid.data(), size, &size)) sid.clear();
            CloseHandle(token);
        }
        self = std::move(sid);
    });
    if (self.empty()) return false;

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    std::vector<BYTE> theirs;
    bool ok = userSid(process, theirs);
    CloseHandle(process);
    return ok && EqualSid(reinterpret_cast<TOKEN_USER*>(self.data())->User.Sid,
                          reinterpret_cast<TOKEN_USER*>(theirs.data())->User.Sid);
}

}  // namespace

struct EngineClient::State {
    std::mutex lifecycle;  // guards refs and the thread
    std::mutex lock;       // guards outgoing and pending
    std::deque<std::string> outgoing;
    std::map<std::uint32_t, HWND> pending;
    std::vector<HWND> observers;
    std::wstring pipeName = kDefaultPipe;
    HANDLE stopEvent = nullptr;
    HANDLE wakeEvent = nullptr;
    std::thread worker;
    int refs = 0;
    std::atomic<std::uint32_t> nextId{1};
};

EngineClient& EngineClient::Instance() {
    static EngineClient* client = [] {
        auto* c = new EngineClient();  // intentionally never destroyed: threads may outlive static teardown
        c->state_ = new State();
        return c;
    }();
    return *client;
}

void EngineClient::SetPipeName(const wchar_t* name) { state_->pipeName = name; }

std::uint32_t EngineClient::NextId() {
    std::uint32_t id = state_->nextId.fetch_add(1);
    return id ? id : state_->nextId.fetch_add(1);
}

void EngineClient::Acquire() {
    std::lock_guard<std::mutex> guard(state_->lifecycle);
    if (state_->refs++ > 0) return;
    state_->stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    state_->wakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    try {
        state_->worker = std::thread([this] { Run(); });
    } catch (...) {
        LogError(L"could not start the engine client thread");
        CloseHandle(state_->stopEvent);
        CloseHandle(state_->wakeEvent);
        state_->stopEvent = state_->wakeEvent = nullptr;
        state_->refs = 0;
    }
}

void EngineClient::Release() {
    std::lock_guard<std::mutex> guard(state_->lifecycle);
    if (state_->refs == 0 || --state_->refs > 0) return;
    SetEvent(state_->stopEvent);
    if (state_->worker.joinable()) state_->worker.join();
    CloseHandle(state_->stopEvent);
    CloseHandle(state_->wakeEvent);
    state_->stopEvent = state_->wakeEvent = nullptr;
    DropQueued();
}

void EngineClient::Send(protocol::Request request, HWND replyTo) {
    if (!connected_) return;  // the engine isn't there: behave as if Completionist weren't installed
    std::string frame = protocol::EncodeRequest(request);
    {
        std::lock_guard<std::mutex> guard(state_->lock);
        if (state_->outgoing.size() >= kMaxQueued) state_->outgoing.pop_front();
        state_->outgoing.push_back(std::move(frame));
        if (replyTo) {
            state_->pending[request.id] = replyTo;
            while (state_->pending.size() > kMaxPending) state_->pending.erase(state_->pending.begin());
        }
    }
    if (state_->wakeEvent) SetEvent(state_->wakeEvent);
}

void EngineClient::RegisterWindow(HWND window) {
    if (!window) return;
    {
        std::lock_guard<std::mutex> guard(state_->lock);
        if (std::find(state_->observers.begin(), state_->observers.end(), window) == state_->observers.end())
            state_->observers.push_back(window);
    }
    PostMessageW(window, WM_COMPLETIONIST_CONNECTION, connected() ? 1 : 0, 0);
}

void EngineClient::UnregisterWindow(HWND window) {
    std::lock_guard<std::mutex> guard(state_->lock);
    state_->observers.erase(std::remove(state_->observers.begin(), state_->observers.end(), window),
                            state_->observers.end());
}

void EngineClient::NotifyConnection(bool connected) {
    std::vector<HWND> observers;
    {
        std::lock_guard<std::mutex> guard(state_->lock);
        observers = state_->observers;
    }
    for (HWND window : observers) {
        if (!PostMessageW(window, WM_COMPLETIONIST_CONNECTION, connected ? 1 : 0, 0))
            UnregisterWindow(window);
    }
}

void EngineClient::DropQueued() {
    std::lock_guard<std::mutex> guard(state_->lock);
    state_->outgoing.clear();
    state_->pending.clear();
}

void EngineClient::Run() {
    DWORD backoff = kFirstBackoffMs;
    while (WaitForSingleObject(state_->stopEvent, 0) != WAIT_OBJECT_0) {
        HANDLE pipe = TryConnect();
        if (pipe == INVALID_HANDLE_VALUE) {
            DropQueued();
            if (WaitForSingleObject(state_->stopEvent, backoff) == WAIT_OBJECT_0) break;
            backoff = backoff * 2 > kMaxBackoffMs ? kMaxBackoffMs : backoff * 2;
            continue;
        }
        backoff = kFirstBackoffMs;
        if (InterlockedExchange(&connected_, 1) == 0) NotifyConnection(true);
        LogDebug(L"connected to the engine");
        bool reconnect = Session(pipe);
        if (InterlockedExchange(&connected_, 0) != 0) NotifyConnection(false);
        CloseHandle(pipe);
        DropQueued();
        LogDebug(L"disconnected from the engine");
        if (!reconnect) break;
    }
}

HANDLE EngineClient::TryConnect() {
    for (int attempt = 0; attempt < 2; ++attempt) {
        HANDLE pipe = CreateFileW(state_->pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                  FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) {
            if (ServerIsCurrentUser(pipe)) return pipe;
            LogError(L"the engine pipe is served by a different user; ignoring it");
            CloseHandle(pipe);
            return INVALID_HANDLE_VALUE;
        }
        if (GetLastError() != ERROR_PIPE_BUSY || !WaitNamedPipeW(state_->pipeName.c_str(), 100)) break;
    }
    return INVALID_HANDLE_VALUE;
}

bool EngineClient::Session(HANDLE pipe) {
    HANDLE readEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE writeEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::vector<char> buffer(64 * 1024);
    protocol::FrameDecoder decoder;
    OVERLAPPED read = {};
    bool readInFlight = false;
    bool reconnect = true;

    auto startRead = [&] {
        read = {};
        read.hEvent = readEvent;
        ResetEvent(readEvent);
        DWORD n = 0;
        if (!ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &n, &read) &&
            GetLastError() != ERROR_IO_PENDING)
            return false;
        readInFlight = true;  // completed reads still signal the event, and are collected in the loop
        return true;
    };

    auto writeAll = [&](const std::string& frame) {
        OVERLAPPED write = {};
        write.hEvent = writeEvent;
        ResetEvent(writeEvent);
        DWORD written = 0;
        if (!WriteFile(pipe, frame.data(), static_cast<DWORD>(frame.size()), &written, &write)) {
            if (GetLastError() != ERROR_IO_PENDING) return false;
            HANDLE handles[2] = {writeEvent, state_->stopEvent};
            if (WaitForMultipleObjects(2, handles, FALSE, kWriteTimeoutMs) != WAIT_OBJECT_0) {
                CancelIoEx(pipe, &write);
                GetOverlappedResult(pipe, &write, &written, TRUE);
                return false;
            }
            if (!GetOverlappedResult(pipe, &write, &written, FALSE)) return false;
        }
        return written == frame.size();
    };

    if (startRead()) {
        HANDLE handles[3] = {state_->stopEvent, readEvent, state_->wakeEvent};
        for (;;) {
            DWORD which = WaitForMultipleObjects(3, handles, FALSE, INFINITE);
            if (which == WAIT_OBJECT_0) {
                reconnect = false;
                break;
            }
            if (which == WAIT_OBJECT_0 + 1) {
                DWORD n = 0;
                readInFlight = false;
                if (!GetOverlappedResult(pipe, &read, &n, FALSE) && GetLastError() != ERROR_MORE_DATA) break;
                std::vector<std::string> bodies;
                if (!decoder.Feed(buffer.data(), n, &bodies)) {
                    LogError(L"the engine sent an oversize frame; dropping the connection");
                    break;
                }
                for (const std::string& body : bodies) DispatchReply(body);
                if (!startRead()) break;
            } else if (which == WAIT_OBJECT_0 + 2) {
                std::deque<std::string> batch;
                {
                    std::lock_guard<std::mutex> guard(state_->lock);
                    batch.swap(state_->outgoing);
                }
                bool ok = true;
                for (const std::string& frame : batch) {
                    if (!writeAll(frame)) {
                        ok = false;
                        break;
                    }
                }
                if (!ok) break;
            } else {
                break;
            }
        }
    }

    if (readInFlight) {
        CancelIoEx(pipe, &read);
        DWORD n = 0;
        GetOverlappedResult(pipe, &read, &n, TRUE);
    }
    CloseHandle(readEvent);
    CloseHandle(writeEvent);
    return reconnect;
}

void EngineClient::DispatchReply(const std::string& body) {
    auto reply = protocol::ParseWordReply(body);
    if (!reply) {
        LogDebug(L"ignoring an engine message that isn't a words reply");
        return;
    }
    HWND target = nullptr;
    {
        std::lock_guard<std::mutex> guard(state_->lock);
        // Kept after the reply: a phrase streams in later, as more messages with the same id.
        auto it = state_->pending.find(reply->id);
        if (it == state_->pending.end()) return;
        target = it->second;
    }
    auto* heap = new (std::nothrow) protocol::WordReply(std::move(*reply));
    if (!heap) return;
    if (!PostMessageW(target, WM_COMPLETIONIST_REPLY, 0, reinterpret_cast<LPARAM>(heap))) delete heap;
}

}  // namespace completionist
