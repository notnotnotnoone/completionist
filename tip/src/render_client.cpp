#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "render_client.h"

#include "glass_log.h"

#include <algorithm>
#include <array>
#include <limits>
#include <sddl.h>
#include <vector>

namespace completionist::render {
namespace {

uint64_t NowMs() { return GetTickCount64(); }
uint64_t AllocateGeneration() {
    static volatile LONG64 generation = 0;
    return static_cast<uint64_t>(InterlockedIncrement64(&generation));
}
std::string FrameCommand(std::string body);
bool SameIdentityBase(const Identity& left, const Identity& right) {
    return left.pid == right.pid && left.hostHwnd == right.hostHwnd && left.session == right.session;
}

bool QueryBuffer(HANDLE token, TOKEN_INFORMATION_CLASS kind, std::vector<unsigned char>* bytes) {
    DWORD size = 0;
    GetTokenInformation(token, kind, nullptr, 0, &size);
    if (!size) return false;
    bytes->resize(size);
    return GetTokenInformation(token, kind, bytes->data(), size, &size) != FALSE;
}

bool SidText(PSID sid, std::wstring* text) {
    LPWSTR value = nullptr;
    if (!ConvertSidToStringSidW(sid, &value)) return false;
    *text = value;
    LocalFree(value);
    return true;
}

bool LogonIdentity(HANDLE token, std::wstring* user, std::wstring* logon,
                   uint64_t* authenticationId, DWORD* sessionId) {
    std::vector<unsigned char> userData, groupData, statsData, sessionData;
    if (!QueryBuffer(token, TokenUser, &userData) || !QueryBuffer(token, TokenGroups, &groupData) ||
        !QueryBuffer(token, TokenStatistics, &statsData) || !QueryBuffer(token, TokenSessionId, &sessionData)) return false;
    auto* tokenUser = reinterpret_cast<TOKEN_USER*>(userData.data());
    auto* groups = reinterpret_cast<TOKEN_GROUPS*>(groupData.data());
    auto* stats = reinterpret_cast<TOKEN_STATISTICS*>(statsData.data());
    if (!SidText(tokenUser->User.Sid, user)) return false;
    bool foundLogon = false;
    for (DWORD i = 0; i < groups->GroupCount; ++i) {
        if ((groups->Groups[i].Attributes & SE_GROUP_LOGON_ID) == SE_GROUP_LOGON_ID) {
            foundLogon = SidText(groups->Groups[i].Sid, logon);
            break;
        }
    }
    *authenticationId = (static_cast<uint64_t>(static_cast<uint32_t>(stats->AuthenticationId.HighPart)) << 32) |
                        stats->AuthenticationId.LowPart;
    *sessionId = *reinterpret_cast<DWORD*>(sessionData.data());
    return foundLogon;
}

bool ValidateRendererServer(HANDLE pipe) {
    ULONG serverPid = 0;
    if (!GetNamedPipeServerProcessId(pipe, &serverPid) || !serverPid) return false;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, serverPid);
    if (!process) return false;
    HANDLE peer = nullptr, self = nullptr;
    bool valid = false;
    if (OpenProcessToken(process, TOKEN_QUERY, &peer) && OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &self)) {
        std::wstring peerUser, peerLogon, selfUser, selfLogon;
        uint64_t peerAuth = 0, selfAuth = 0;
        DWORD peerSession = 0, selfSession = 0;
        valid = LogonIdentity(peer, &peerUser, &peerLogon, &peerAuth, &peerSession) &&
                LogonIdentity(self, &selfUser, &selfLogon, &selfAuth, &selfSession) &&
                peerUser == selfUser && peerLogon == selfLogon && peerAuth == selfAuth && peerSession == selfSession;
    }
    if (peer) CloseHandle(peer);
    if (self) CloseHandle(self);
    CloseHandle(process);
    return valid;
}

std::wstring PipeName() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return {};
    std::wstring user, logon;
    uint64_t auth = 0;
    DWORD session = 0;
    const bool valid = LogonIdentity(token, &user, &logon, &auth, &session);
    CloseHandle(token);
    if (!valid) return {};
    return L"\\\\.\\pipe\\completionist-renderer-v2-" + std::to_wstring(session) + L"-" + std::to_wstring(auth);
}

// Why the last connection attempt failed, so a run of identical failures is logged once.
std::wstring g_lastConnectFailure;  // worker threads only; a benign race at worst repeats a log line

HANDLE ConnectFailed(const std::wstring& reason, DWORD error) {
    const std::wstring key = reason + L":" + std::to_wstring(error);
    if (key != g_lastConnectFailure) {
        g_lastConnectFailure = key;
        completionist::GlassLog(L"step=connect result=fail reason=%s error=%lu", reason.c_str(), error);
    }
    return INVALID_HANDLE_VALUE;
}

HANDLE Connect() {
    const std::wstring name = PipeName();
    if (name.empty()) return ConnectFailed(L"no-pipe-name", GetLastError());
    if (!WaitNamedPipeW(name.c_str(), 50)) return ConnectFailed(L"wait", GetLastError());
    HANDLE pipe = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) return ConnectFailed(L"open", GetLastError());
    DWORD mode = PIPE_READMODE_BYTE;
    if (!SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
        const DWORD error = GetLastError();
        CloseHandle(pipe);
        return ConnectFailed(L"pipe-mode", error);
    }
    if (!ValidateRendererServer(pipe)) {
        const DWORD error = GetLastError();
        CloseHandle(pipe);
        return ConnectFailed(L"server-identity", error);
    }
    g_lastConnectFailure.clear();
    completionist::GlassLog(L"step=connect result=ok");
    return pipe;
}

bool WriteFrame(HANDLE pipe, const std::string& frame) {
    size_t offset = 0;
    while (offset < frame.size()) {
        DWORD written = 0;
        const DWORD count = static_cast<DWORD>(std::min<size_t>(frame.size() - offset, 4096));
        if (!WriteFile(pipe, frame.data() + offset, count, &written, nullptr)) return false;
        if (!written) { SetLastError(ERROR_WRITE_FAULT); return false; }
        offset += written;
    }
    return true;
}

bool ReadAck(HANDLE pipe, std::vector<unsigned char>* pending, std::optional<Ack>* ack) {
    DWORD available = 0;
    if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) return false;
    if (available) {
        std::array<unsigned char, 4096> bytes{};
        while (available) {
            DWORD read = 0;
            const DWORD count = std::min<DWORD>(available, static_cast<DWORD>(bytes.size()));
            if (!ReadFile(pipe, bytes.data(), count, &read, nullptr)) return false;
            if (!read) { SetLastError(ERROR_BROKEN_PIPE); return false; }
            if (pending->size() > kMaxFrameBytes + 4 - read) { SetLastError(ERROR_INVALID_DATA); return false; }
            pending->insert(pending->end(), bytes.begin(), bytes.begin() + read);
            if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) return false;
        }
    }
    if (pending->size() < 4) return true;
    const uint32_t length = static_cast<uint32_t>((*pending)[0]) |
        (static_cast<uint32_t>((*pending)[1]) << 8) | (static_cast<uint32_t>((*pending)[2]) << 16) |
        (static_cast<uint32_t>((*pending)[3]) << 24);
    if (!length || length > kMaxFrameBytes) { SetLastError(ERROR_INVALID_DATA); return false; }
    if (pending->size() < static_cast<size_t>(length) + 4) return true;
    std::string body(reinterpret_cast<const char*>(pending->data() + 4), length);
    pending->erase(pending->begin(), pending->begin() + length + 4);
    *ack = ParseAck(body);
    if (!ack->has_value()) SetLastError(ERROR_INVALID_DATA);
    return ack->has_value();
}

std::string EncodeCommand(const ClientRequest& request) {
    if (const auto* hide = std::get_if<HideRequest>(&request)) {
        std::string body = "{\"schema\":1,\"command\":\"hide\",\"owner\":{";
        body += "\"pid\":" + std::to_string(hide->owner.pid) + ",\"hwnd\":" + std::to_string(hide->owner.hostHwnd) +
                ",\"session\":\"" + hide->owner.session + "\",\"generation\":" + std::to_string(hide->owner.generation) +
                "},\"revision\":" + std::to_string(hide->revision) + "}";
        return FrameCommand(std::move(body));
    }
    const Identity* owner = nullptr;
    if (const auto* heartbeat = std::get_if<HeartbeatRequest>(&request)) owner = &heartbeat->owner;
    if (!owner) return {};
    std::string body = "{\"schema\":1,\"command\":\"heartbeat\",\"owner\":{";
    body += "\"pid\":" + std::to_string(owner->pid) + ",\"hwnd\":" + std::to_string(owner->hostHwnd) +
            ",\"session\":\"" + owner->session + "\",\"generation\":" + std::to_string(owner->generation) + "}}";
    return FrameCommand(std::move(body));
}

std::string FrameCommand(std::string body) {
    if (body.size() > kMaxFrameBytes) return {};
    std::string frame;
    const auto length = static_cast<uint32_t>(body.size());
    for (unsigned shift = 0; shift < 32; shift += 8) frame.push_back(static_cast<char>((length >> shift) & 0xff));
    frame += body;
    return frame;
}

}  // namespace

uint64_t NextRenderGeneration() { return AllocateGeneration(); }

bool RenderClientState::MakeRoomForPriority() {
    if (queue_.size() < kQueueLimit) return true;
    auto show = std::find_if(queue_.begin(), queue_.end(), [](const ClientRequest& item) {
        return std::holds_alternative<ShowRequest>(item) || std::holds_alternative<HeartbeatRequest>(item);
    });
    if (show != queue_.end()) queue_.erase(show);
    else queue_.pop_back();
    return true;
}

bool RenderClientState::Publish(Snapshot snapshot, uint64_t nowMs) {
    if (snapshot.owner.pid == 0 || snapshot.owner.hostHwnd == 0 || snapshot.owner.session.empty() ||
        snapshot.owner.generation == 0 || snapshot.revision == 0) return false;
    if (current_ && SameIdentity(current_->owner, snapshot.owner) && snapshot.revision <= current_->revision) return false;
    if (queue_.size() >= kQueueLimit) {
        auto expendable = std::find_if(queue_.begin(), queue_.end(), [](const ClientRequest& item) {
            return std::holds_alternative<ShowRequest>(item) || std::holds_alternative<HeartbeatRequest>(item);
        });
        if (expendable != queue_.end()) queue_.erase(expendable);
        else queue_.pop_back();
    }
    queue_.erase(std::remove_if(queue_.begin(), queue_.end(), [](const ClientRequest& item) {
        return std::holds_alternative<ShowRequest>(item);
    }), queue_.end());
    current_ = std::move(snapshot);
    showQueuedAtMs_ = nowMs;
    awaitingAck_ = true;
    fallbackVisible_ = true;
    queue_.push_back(ShowRequest{*current_, nowMs});
    return true;
}

bool RenderClientState::Hide(Identity owner, uint64_t revision) {
    queue_.erase(std::remove_if(queue_.begin(), queue_.end(), [&](const ClientRequest& item) {
        const auto* show = std::get_if<ShowRequest>(&item);
        return show && SameIdentity(show->snapshot.owner, owner) && show->snapshot.revision <= revision;
    }), queue_.end());
    if (current_ && SameIdentity(current_->owner, owner) && revision >= current_->revision) {
        current_.reset();
        awaitingAck_ = false;
        fallbackVisible_ = true;
    }
    if (!MakeRoomForPriority()) return false;
    queue_.push_front(HideRequest{std::move(owner), revision});
    return true;
}

bool RenderClientState::Heartbeat(Identity owner) {
    if (queue_.size() >= kQueueLimit) return false;
    queue_.push_back(HeartbeatRequest{std::move(owner)});
    return true;
}

std::optional<ClientRequest> RenderClientState::Take() {
    if (queue_.empty()) return std::nullopt;
    ClientRequest next = std::move(queue_.front());
    queue_.pop_front();
    return next;
}

bool RenderClientState::OnAck(const Ack& ack) {
    if (!current_ || !SameIdentity(current_->owner, ack.owner) || current_->revision != ack.revision) return false;
    awaitingAck_ = false;
    fallbackVisible_ = !ack.presented;
    return true;
}

bool RenderClientState::Tick(uint64_t nowMs) {
    if (!awaitingAck_ || nowMs < showQueuedAtMs_ || nowMs - showQueuedAtMs_ < kFallbackDeadlineMs) return false;
    awaitingAck_ = false;
    fallbackVisible_ = true;
    return true;
}

bool RenderClientState::Disconnect() {
    const bool changed = !fallbackVisible_;
    fallbackVisible_ = true;
    awaitingAck_ = false;
    queue_.clear();
    return changed;
}

void RenderClientState::Reconnect(Identity owner, uint64_t nowMs) {
    if (!current_ || !SameIdentityBase(current_->owner, owner)) return;
    current_->owner = std::move(owner);
    queue_.erase(std::remove_if(queue_.begin(), queue_.end(), [](const ClientRequest& item) {
        return std::holds_alternative<ShowRequest>(item) || std::holds_alternative<HeartbeatRequest>(item);
    }), queue_.end());
    if (queue_.size() >= kQueueLimit) queue_.pop_back();
    showQueuedAtMs_ = nowMs;
    awaitingAck_ = true;
    fallbackVisible_ = true;
    queue_.push_front(ShowRequest{*current_, nowMs});
}

RenderClient::RenderClient(HWND notificationWindow, const Identity& identity)
    : notificationWindow_(notificationWindow), identity_(identity) {}

RenderClient::~RenderClient() { Stop(); }

void RenderClient::Start() {
    std::lock_guard lock(mutex_);
    if (started_) return;
    stopping_ = false;
    started_ = true;
    completionist::GlassLog(L"step=client-start");
    worker_ = std::thread([this] { Worker(); });
}

void RenderClient::Stop() {
    {
        std::lock_guard lock(mutex_);
        if (!started_) return;
        stopping_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable()) {
        CancelSynchronousIo(worker_.native_handle());
        worker_.join();
    }
    std::lock_guard lock(mutex_);
    started_ = false;
}

void RenderClient::Publish(Snapshot snapshot) {
    bool fallbackChanged = false;
    {
        std::lock_guard lock(mutex_);
        const bool before = state_.fallbackVisible();
        if (identity_.pid == 0 || !SameIdentityBase(identity_, snapshot.owner) ||
            snapshot.owner.generation > identity_.generation) identity_ = snapshot.owner;
        else if (SameIdentityBase(identity_, snapshot.owner)) snapshot.owner = identity_;
        state_.Publish(std::move(snapshot), NowMs());
        fallbackChanged = before != state_.fallbackVisible();
    }
    wake_.notify_one();
    if (fallbackChanged) Notify();
}

void RenderClient::Hide(Identity owner, uint64_t revision) {
    bool fallbackChanged = false;
    {
        std::lock_guard lock(mutex_);
        const bool before = state_.fallbackVisible();
        if (SameIdentityBase(identity_, owner)) owner = identity_;
        state_.Hide(std::move(owner), revision);
        fallbackChanged = before != state_.fallbackVisible();
    }
    wake_.notify_one();
    if (fallbackChanged) Notify();
}

void RenderClient::Heartbeat(Identity owner) {
    {
        std::lock_guard lock(mutex_);
        if (SameIdentityBase(identity_, owner)) owner = identity_;
        state_.Heartbeat(std::move(owner));
    }
    wake_.notify_one();
}

void RenderClient::Notify(std::optional<Ack> ack) {
    if (!notificationWindow_) return;
    auto notice = std::make_unique<ClientNotice>();
    notice->ack = std::move(ack);
    {
        std::lock_guard lock(mutex_);
        notice->fallbackVisible = state_.fallbackVisible();
    }
    if (!PostMessageW(notificationWindow_, kRenderClientNoticeMessage, 0, reinterpret_cast<LPARAM>(notice.get()))) return;
    notice.release();
}

void RenderClient::Worker() {
    HANDLE pipe = INVALID_HANDLE_VALUE;
    std::vector<unsigned char> pending;
    uint64_t lastHeartbeat = 0;
    auto disconnect = [&] {
        const bool wasConnected = pipe != INVALID_HANDLE_VALUE;
        if (wasConnected) {
            CloseHandle(pipe);
            completionist::GlassLog(L"step=disconnect reason=connection-lost");
        }
        pipe = INVALID_HANDLE_VALUE;
        bool changed = false;
        {
            std::lock_guard lock(mutex_);
            changed = state_.Disconnect();
            if (wasConnected && state_.current()) {
                identity_ = state_.current()->owner;
                identity_.generation = NextRenderGeneration();
                state_.Reconnect(identity_, NowMs());
                changed = true;
            }
        }
        if (changed) Notify();
    };
    while (true) {
        {
            std::unique_lock lock(mutex_);
            if (stopping_) break;
        }
        if (pipe == INVALID_HANDLE_VALUE) {
            pipe = Connect();
            if (pipe == INVALID_HANDLE_VALUE) {
                std::unique_lock lock(mutex_);
                wake_.wait_for(lock, std::chrono::milliseconds(100), [&] { return stopping_; });
                continue;
            }
            pending.clear();
            lastHeartbeat = 0;
        }
        std::optional<ClientRequest> request;
        {
            std::lock_guard lock(mutex_);
            request = state_.Take();
        }
        bool alive = true;
        if (request) {
            std::string frame;
            if (const auto* show = std::get_if<ShowRequest>(&*request)) frame = EncodeShow(show->snapshot);
            else frame = EncodeCommand(*request);
            if (frame.empty() || !WriteFrame(pipe, frame)) {
                const DWORD error = frame.empty() ? ERROR_INVALID_DATA : GetLastError();
                const Identity* owner = nullptr;
                uint64_t revision = 0;
                if (const auto* show = std::get_if<ShowRequest>(&*request)) {
                    owner = &show->snapshot.owner;
                    revision = show->snapshot.revision;
                } else if (const auto* hide = std::get_if<HideRequest>(&*request)) {
                    owner = &hide->owner;
                    revision = hide->revision;
                } else {
                    owner = &std::get<HeartbeatRequest>(*request).owner;
                }
                completionist::GlassLog(L"step=write-failed empty=%d error=%lu rev=%llu gen=%llu hwnd=%llx",
                    frame.empty() ? 1 : 0, error, revision, owner->generation, owner->hostHwnd);
                alive = false;
            }
            else if (const auto* heartbeat = std::get_if<HeartbeatRequest>(&*request)) { (void)heartbeat; lastHeartbeat = NowMs(); }
            else if (const auto* show = std::get_if<ShowRequest>(&*request)) { (void)show; lastHeartbeat = NowMs(); }
        }
        if (alive) {
            std::optional<Ack> ack;
            alive = ReadAck(pipe, &pending, &ack);
            if (!alive) completionist::GlassLog(L"step=read-failed error=%lu", GetLastError());
            if (alive && ack) {
                bool accepted = false;
                {
                    std::lock_guard lock(mutex_);
                    accepted = state_.OnAck(*ack);
                }
                if (accepted) Notify(*ack);
                completionist::GlassLog(L"step=ack rev=%llu gen=%llu hwnd=%llx presented=%d accepted=%d", ack->revision,
                                        ack->owner.generation, ack->owner.hostHwnd, ack->presented ? 1 : 0, accepted ? 1 : 0);
            }
        }
        bool timedOut = false;
        uint64_t timeoutRevision = 0, timeoutGeneration = 0, timeoutHwnd = 0;
        {
            std::lock_guard lock(mutex_);
            timedOut = state_.Tick(NowMs());
            if (timedOut && state_.current()) {
                timeoutRevision = state_.current()->revision;
                timeoutGeneration = state_.current()->owner.generation;
                timeoutHwnd = state_.current()->owner.hostHwnd;
            }
        }
        if (timedOut) {
            completionist::GlassLog(L"step=timeout rev=%llu gen=%llu hwnd=%llx", timeoutRevision, timeoutGeneration, timeoutHwnd);
            Notify();
        }
        if (!alive) { disconnect(); continue; }
        if (NowMs() - lastHeartbeat >= kHeartbeatIntervalMs) {
            std::optional<Snapshot> current;
            {
                std::lock_guard lock(mutex_);
                current = state_.current();
            }
            if (current) {
                const std::string frame = EncodeCommand(HeartbeatRequest{current->owner});
                if (frame.empty() || !WriteFrame(pipe, frame)) {
                    completionist::GlassLog(L"step=write-failed kind=heartbeat empty=%d error=%lu rev=%llu gen=%llu hwnd=%llx",
                                            frame.empty() ? 1 : 0, frame.empty() ? ERROR_INVALID_DATA : GetLastError(),
                                            current->revision, current->owner.generation, current->owner.hostHwnd);
                    disconnect(); continue;
                }
                lastHeartbeat = NowMs();
            }
        }
        std::unique_lock lock(mutex_);
        wake_.wait_for(lock, std::chrono::milliseconds(10), [&] { return stopping_ || state_.queued() != 0; });
    }
    if (pipe != INVALID_HANDLE_VALUE) {
        // Closing the accepted peer is the renderer's revocation signal; teardown never waits
        // for a final synchronous hide write from the host UI thread.
        CloseHandle(pipe);
        completionist::GlassLog(L"step=disconnect reason=shutdown");
    }
    bool changed = false;
    { std::lock_guard lock(mutex_); changed = state_.Disconnect(); }
    if (changed) Notify();
}

}  // namespace completionist::render
