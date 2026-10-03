#include "../windows.h"
#include "../../tests/test_harness.h"

using renderer::win::LogonIdentity;
using renderer::win::PeerClaim;

namespace {
LogonIdentity Identity(const wchar_t* user, const wchar_t* logon, uint64_t auth, DWORD session) {
    return {user, logon, auth, session};
}
PeerClaim Claim() {
    PeerClaim claim{};
    claim.declaredPid = 42;
    claim.declaredHwnd = 0x1234;
    claim.actualPipePid = 42;
    claim.peer = Identity(L"S-1-5-21-user", L"S-1-5-5-1-2", 0x100000002ull, 3);
    claim.current = claim.peer;
    claim.actualForegroundHwnd = 0x1234;
    claim.actualForegroundPid = 42;
    return claim;
}
}

TEST(renderer_peer_requires_same_user_logon_authentication_and_session) {
    auto claim = Claim();
    CHECK(renderer::win::ValidatePeerClaim(claim));
    claim.peer.userSid = L"S-1-5-21-other";
    CHECK(!renderer::win::ValidatePeerClaim(claim));
    claim = Claim();
    claim.peer.logonSid = L"S-1-5-5-9-9";
    CHECK(!renderer::win::ValidatePeerClaim(claim));
    claim = Claim();
    claim.peer.authenticationId++;
    CHECK(!renderer::win::ValidatePeerClaim(claim));
    claim = Claim();
    claim.peer.sessionId++;
    CHECK(!renderer::win::ValidatePeerClaim(claim));
}

TEST(renderer_peer_uses_actual_pipe_pid_and_foreground_window) {
    auto claim = Claim();
    claim.actualPipePid++;
    CHECK(!renderer::win::ValidatePeerClaim(claim));
    claim = Claim();
    claim.actualForegroundPid++;
    CHECK(!renderer::win::ValidatePeerClaim(claim));
    claim = Claim();
    claim.actualForegroundHwnd++;
    CHECK(!renderer::win::ValidatePeerClaim(claim));
}
