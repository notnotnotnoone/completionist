# V2 Desktop Overhaul Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Subagent-driven execution is an alternative only if explicitly chosen by the owner. Steps use checkbox syntax for tracking.

**Goal:** Ship the reviewed soft native liquid-glass autocomplete menu and lightweight information dock for V2, with real engine status and safe fallback.

**Architecture:** TSF remains the keyboard/caret/insertion authority. A separately supervised C++ renderer receives full asynchronous display snapshots and owns the two native surfaces. Python retains ranking/AI and exposes truthful scheduler/provenance metadata.

**Tech Stack:** C++20/x64/static CRT; Win32/D3D11/DXGI/DirectComposition/Direct2D/DirectWrite; Python/asyncio; existing CLI-only VS 2022 Build Tools and uv.

**Spec:** `docs/superpowers/specs/2026-10-02-v2-desktop-overhaul.md` (read in full before execution).

Status: complete proposed implementation sequence, awaiting joint document review. The owner explicitly requested both a spec and plan together; neither document is evidence that production implementation is approved. No installation, native capture, source-library download or product-code change has been done by this planning task.

## Global constraints

- Live glass requires Windows 10 2004 / build 19041 or later; unsupported environments retain opaque UI.
- C++20, x64, static CRT; Python remains the engine. No Electron/WebView/managed WinUI/internal hooks/new font installation.
- Use exact canonical Evergreen roles; Segoe UI content and Consolas shortcut text.
- Menu width 330 DIPs; dock 190 DIPs with 20-DIP work-area inset. Honor font/width settings.
- True separable Gaussian blur: sigma 6 DIPs, radius 18 DIPs; refraction strength equivalent to spike 18; radius 26 DIPs capped for short surfaces.
- Real lifecycle/provenance only. `Tense —` in V2; no simulated classification. Context receipt belongs in the viewer.
- Render framing: 4-byte little-endian length, UTF-8 JSON, 1 MiB cap, schema version 1. Current-user/current-logon-session peers only.
- Lease heartbeat 500 ms / expiry 1500 ms; show fallback deadline 250 ms; bounded queue 32 entries.
- Capture is local GPU material only: no CPU readback in production, storage, transmission, OCR or model use. Hidden/blocked/sensitive/paused/locked contexts release capture.
- High contrast/transparency disabled/capture or exclusion failure/HDR unsupported -> opaque; no frozen captured pixels.
- Dock mouse action never activates; keyboard changes are instant. Body opacity 180 ms, translation 220 ms; connection pulse 2.4 s; respect reduced motion.
- Preserve Enter passthrough, after-space Tab passthrough, configured shortcuts, stale-key safety, 150 ms phrase guard and explicit selection.
- No focus-stealing harness unless the owner says they are away. Visual Windows checks are owner-run; no automatic desktop screenshots.
- Every implementation commit updates `docs/roadmap/roadmap.js`; `python scripts/check_roadmap.py` must pass. Installation/system settings and push to main require existing project approvals.

## Review focus

1. Old clients, malformed metadata and out-of-order replies must preserve usable suggestions without showing stale status (Tasks 2–3, 7).
2. Two apps fighting for focus, host exit and dropped heartbeats must not leave duplicate or stale surfaces (Tasks 3, 7).
3. Negative monitor coordinates, DPI-unaware hosts, rotation and tiny work areas must not double-scale or hide the caret (Tasks 1, 4, 7).
4. Exclusion/device/session failures and HDR must never produce capture feedback or frozen desktop content (Tasks 1, 5, 8).
5. Very long Unicode corrections/phrases and pending-only shelves must preserve exact insertion and Tab behavior (Tasks 4, 6–7).

## File map

Existing production files remain responsible for their current jobs:

- `tip/src/tsf_service.cpp`: context eligibility, caret, current request, insertion and keyboard model.
- `tip/src/popup.cpp`, `popup.h`: host fallback and existing message/timer endpoint.
- `tip/src/engine_client.cpp`, `engine_client.h`: engine pipe worker/connection notifications.
- `tip/src/protocol.cpp`, `protocol.h`: engine message decoding.
- `engine/src/completionist_engine/phrase_scheduler.py`, `phrases.py`, `engine.py`, `words.py`, `server.py`: actual engine lifecycle and candidate metadata.
- `engine/src/completionist_engine/app.py`: process supervision.

Create focused new units:

| Path | Responsibility |
|---|---|
| `tip/src/render_protocol.h/.cpp` | Versioned full snapshot and acknowledgement codec, pure C++ |
| `tip/src/render_client.h/.cpp` | Nonblocking renderer pipe worker and latest-snapshot coalescing |
| `tip/src/popup_layout.h/.cpp` | Pure layout/placement/partial-accept range geometry |
| `tip/src/popup_palette.h` | Canonical native Evergreen roles |
| `tip/renderer/main.cpp`, `windows.h/.cpp` | Single instance, two HWNDs, messages/accessibility |
| `tip/renderer/session.h/.cpp` | Peer/foreground eligibility, focus lease, revision arbitration |
| `tip/renderer/capture.h/.cpp` | DXGI output lifetime, rotation, dirty regions, exclusion handling |
| `tip/renderer/material.h/.cpp`, `shaders/blur.hlsl`, `shaders/glass.hlsl` | GPU blur/material/composition and device-loss fallback |
| `tip/renderer/text.h/.cpp` | DirectWrite foreground, origins, corrections and underline |
| `tip/renderer/build.cmd`, `test.cmd` | Build executable, shader embedding, isolated fixture checks |
| `engine/src/completionist_engine/renderer_process.py` | Hidden launch/restart/shutdown supervision |

Keep the current spike untouched as a reference. Do not copy its `main.cpp` into the production renderer or restructure unrelated engine modules.

## Shared interfaces

Define these once in Task 3 and reuse exactly; C++ `std::string` wire text is UTF-8, parsed content becomes `std::wstring` for rendering. Times are local monotonic milliseconds; no shared-clock assumption.

```cpp
namespace completionist::render {
struct Identity { uint32_t pid; uint64_t hostHwnd; std::string session; uint64_t generation; };
struct Rect { int32_t left, top, right, bottom; }; // physical virtual-desktop px
enum class Command { Show, Hide, Heartbeat };
enum class AiState { Off, Manual, Scheduled, Working, Streaming, Ready, Unavailable };
struct Candidate { std::wstring text; std::string origin; std::vector<int> marks; };
struct Snapshot {
    Identity owner; uint64_t revision; Rect caret;
    std::vector<Candidate> words; int selection;
    std::wstring typedFragment, phrase, phraseLead;
    uint32_t partialBegin, partialLength; // UTF-16 range in displayed phrase
    AiState ai; uint32_t waitMs, elapsedMs; std::string triggerReason;
    bool engineConnected; PopupSettings settings;
};
struct Ack { Identity owner; uint64_t revision; bool presented; };
std::string EncodeShow(const Snapshot& snapshot); // complete length-prefixed frame
std::optional<Snapshot> ParseShow(std::string_view body);
std::optional<Ack> ParseAck(std::string_view body);
bool IsCurrentAck(const Snapshot& current, const Ack& ack);
}
```

Expose `RenderClient::Publish(Snapshot)`, `Hide(Identity,uint64_t revision)`, and `Heartbeat(Identity)`; all enqueue and return immediately. A UI-thread message carries a validated `Ack`. Renderer `Session::Accept(snapshot, foregroundPid, nowMs)` returns eligibility; `Expire(nowMs)` revokes stale lease. `Material::Render(layout,snapshot)` returns `Presented`, `OpaquePresented` or `Unavailable`; only the first two generate a successful acknowledgement. Implement these enum/type declarations in their owning headers before dependents use them.

`PopupSettings` comes from its existing header, never a duplicate definition. `Layout` is declared by `popup_layout.h`: menu/dock physical bounds, logical content rectangles, row order, text clip regions and collapsed state. No Windows handles in pure protocol/layout tests.

## Task 1: Prove live soft glass in an isolated native renderer

**Files:** create `tip/renderer/main.cpp`, `windows.h/.cpp`, `capture.h/.cpp`, `material.h/.cpp`, shader files, `build.cmd`, `test.cmd`; create `tip/renderer/tests/test_blur.cpp`. Consult `tip/glass-spike/` only for the material profile.

**Consumes:** reviewed material/coordinate/fallback spec. **Produces:** a standalone two-window feasibility executable, offscreen blur fixtures, and recorded pass/fail findings. No TSF integration or engine startup yet.

- [ ] Add a headless fixture mode with an opaque pattern/point source and separately rendered glyphs. Its exported image contains only generated content. Add assertions that Gaussian output is symmetric, weights sum to 1 within 1e-5, and a step edge transitions monotonically over several pixels rather than repeated ghost edges.
- [ ] Run `tip\renderer\test.cmd`; confirm these checks fail before blur exists.
- [ ] Implement horizontal/vertical GPU convolution. Compile embedded shaders through SDK tooling discovered from `vcvars64.bat`; use an explicit `/Fh` generated header included by the renderer, not runtime source-file lookup. Use original full-size crop/padding; no sparse sampling.
- [ ] Implement two nonactivating transparent composition windows with exclusion, supported capture, lens pass and sharp foreground. Pair every successful acquired DXGI frame with `ReleaseFrame` through a scope guard; never hold it across shutdown or resize. Hide/release on desktop/session loss.
- [ ] Add injected exclusion/access-lost/device-removed/HDR-unsupported failure cases. Assert the capture resource count returns to zero and the next draw is opaque or hidden, never a previous desktop frame.
- [ ] Run fixture tests/build without opening live windows. Then give the owner the live feasibility command and short matrix: move between monitors, inspect blur/refraction, click dock without focus loss, lock/unlock, and verify capture exclusion. Do not launch a foreground demo while they work.
- [ ] Record static/dirty-region work, GPU pass timing and presentation latency against the spec's 4/32 ms targets. Fix any self-repainting loop.
- [ ] Gate: proceed only after live capture/composition/focus are verified. If not feasible, preserve the current production popup and present the specific failing contract before revising architecture. Update roadmap and commit feasibility work together.

## Task 2: Supply truthful AI status and origins

**Files:** modify engine `phrase_scheduler.py`, `phrases.py`, `words.py`, `engine.py`, `server.py`; modify `tip/src/protocol.h/.cpp`; create `engine/tests/test_phrase_scheduler.py`; extend existing `engine/tests/test_phrases_pipe.py`, `engine/tests/test_engine.py`, `tip/tests/test_protocol.cpp`.

**Consumes:** existing scheduler due time/provider events and vocabulary membership. **Produces:** optional engine reply metadata without changing ranking or old fields.

- [ ] Add fake-clock tests that schedule at `now=10.0`, due `10.35`; after 100 ms remaining is 250 ms; new typing resets due; manual mode has no countdown; disabled/cancelled request does not publish working. Test provider starts/first chunk/finish/failure and stale request suppression.
- [ ] Add candidate tests where a boosted base word is `local`, a promoted out-of-base word is `learned`, and deduplication leaves origin aligned with the winning candidate. Existing scores/order must be identical.
- [ ] Run `uv run --project engine pytest engine/tests/test_phrase_scheduler.py engine/tests/test_phrases_pipe.py engine/tests/test_engine.py` and see failing new assertions.
- [ ] Add a read-only scheduler snapshot method with this shape, using the same scheduler state and clock as request scheduling:

```python
def display_status(self, now: float) -> dict[str, object]:
    # Return actual state; do not start/cancel provider work from this method.
    return {
        "phrase_state": state,
        "phrase_wait_ms": max(0, min(600000, round(remaining * 1000))),
        "phrase_elapsed_ms": max(0, min(600000, round(elapsed * 1000))),
        "trigger_reason": reason,
    }
```

Here `state`, `remaining`, `elapsed`, `reason` are derived inside the method from existing pending/due/inflight/stream lifecycle; introduce an explicit started-at value on actual provider start and a terminal failure state, not independent timers. Attach this metadata to current-id replies/pushes through `phrases.py`/`engine.py`; no extra scheduler in `server.py`.

- [ ] Extend `WordReply` optional fields/enum. Add native tests for absent metadata, unknown state, booleans masquerading as durations, negative/oversize values, mismatched origins and valid legacy replies. Invalid optional metadata must not invalidate good words.
- [ ] Run named tests plus `tip\test.cmd`; update roadmap and commit.

## Task 3: Implement render framing, leases and focus arbitration

**Files:** create `tip/src/render_protocol.h/.cpp`, `tip/renderer/session.h/.cpp`, `tip/tests/test_render_protocol.cpp`, `tip/renderer/tests/test_session.cpp`; modify `tip/test.cmd` and renderer `test.cmd` to compile these pure tests.

**Consumes:** Shared interfaces above. **Produces:** version-1 codecs, current-ack check and deterministic session state; no renderer acceptance of untrusted owner claims.

- [ ] Add encode/decode roundtrip, UTF-8 replacement, malformed/truncated JSON, 1 MiB limit, schema mismatch and bounded-array tests. Snapshot with negative coordinates must survive unchanged. Unknown optional fields are ignored; invalid required fields close the connection.
- [ ] Pin identity/revision/lease behavior using the existing native test harness assertion style:

```cpp
// Current session A/gen2/rev8 must ignore A/gen1/rev99 and A/gen2/rev7.
// Accept A only when actual foregroundPid == A.pid.
// Heartbeat at 500 ms; Expire(2001) after last renewal at 500 must hide.
// Revoked generation cannot regain focus through heartbeat.
// Disconnect removes both surfaces and content; B takes over only after eligibility.
```

- [ ] Run pure tests, confirm new cases fail, implement the declared codecs and `Session` transitions, then pass. Treat `hostHwnd` as a validated opaque value, not an arbitrary pointer to dereference.
- [ ] Add Windows pipe endpoint security in `windows.cpp`: user/session ACL, reject remote clients, validate actual client PID/SID/session and foreground HWND. Validate renderer server identity in the later client as well.
- [ ] Add WinEvent foreground-change and session/desktop-change revocation; unregister hooks at shutdown. Inject owner/foreground checks for deterministic tests.
- [ ] Update roadmap and commit.

## Task 4: Build pure native layout and typography

**Files:** create `tip/src/popup_layout.h/.cpp`, `popup_palette.h`, `tip/renderer/text.h/.cpp`, `tip/tests/test_popup_layout.cpp`; modify `tip/test.cmd`.

**Consumes:** `Snapshot`, measured DirectWrite font metrics, physical work area and DPI. **Produces:** `Layout`, text drawing and palette parity verification.

- [ ] Write geometry tests for 96/144/192 DPI, negative work areas, bottom/right edges, portrait monitor, narrow work area, custom font/width, word-only and pending-only content. Assert menu stays in work area, caret clearance >=6 DIPs when possible, and dock always uses the same monitor.
- [ ] Add Unicode/long-string cases: marks outside text ignored, no split surrogate pair at ellipsis, displayed underline equals the actual partial-accept segment; insertion data is not truncated. Pending-only shelf has no selectable row.
- [ ] Run pure tests to failure; implement placement in physical pixels from canonical boundary data and DIP layout. Export named palette constants from canonical roles and add a small parity check that compares them with the existing theme values.
- [ ] Implement DirectWrite glyphs, origin labels, dotted correction marks/comparison, measured wrapping/ellipsis, compact shelf and shortcut hints. Selection uses shape plus magenta; text remains a separate sharp layer.
- [ ] Render fixture exports at each scale/theme and inspect generated images, not the user's desktop. Confirm stable pending shelf and no clipped rows at largest configured font.
- [ ] Update roadmap and commit.

## Task 5: Complete renderer windows, motion and recovery

**Files:** extend `tip/renderer/windows.cpp`, `material.cpp`, `capture.cpp`, `session.cpp`; create `tip/renderer/accessibility.h/.cpp`, `tests/test_material_policy.cpp`, `tests/test_dock_state.cpp`.

**Consumes:** `Layout`/`Snapshot`, validated foreground lease and material pipeline. **Produces:** final caret/dock UI, truthful acknowledgements and opaque failure path.

- [ ] Test policy table: hidden/locked -> hidden/no capture; high contrast/transparency off/HDR/exclusion failure -> opaque/no capture; eligible supported SDR -> glass. Device removal transitions to opaque, clears textures, allows one recreation attempt and never loops.
- [ ] Test collapse state: manual minimize survives auto conditions, automatic minimize restores when overlap ends, reversal continues from current progress, hidden body cannot hit-test. Reduced motion and theme/DPI changes complete immediately.
- [ ] Implement cubic-bezier sampling for body translation/opacity with 180/220 ms durations, persistent bottom anchor and clipped body. Request frames only while animation active. No keyboard selection/appearance animation or pointer-responsive production sheen.
- [ ] Implement dock toggle only as interactive hit area, `MA_NOACTIVATE`, click-through menu, correct capture/WM_CANCELMODE cleanup and focus lease retention on dock click.
- [ ] Implement UIA tree/names/selected and expanded state with read-only candidates and accessible toggle; no countdown live-announcement loop. Implement graceful close and cleanup of timers/hooks/COM resources.
- [ ] Acknowledge only a successfully drawn current snapshot, including opaque presentation. On unavailable hide both surfaces before notifying the client.
- [ ] Run renderer fixture/policy tests and owner-run focus/motion/high-contrast matrix. Update roadmap and commit.

## Task 6: Ship matching host fallback and connection events

**Files:** modify `tip/src/popup.cpp/.h`, `engine_client.cpp/.h`, `tsf_service.cpp`; reuse new layout/palette; extend `tip/tests/test_popup_model.cpp` and host fixture tests.

**Consumes:** layout/status metadata. **Produces:** matching opaque Evergreen host UI and explicit pipe connection notifications; external renderer still disabled.

- [ ] Preserve existing popup HWND hooks/timers. Add connection-transition UI messages from the pipe worker with safe teardown; do not poll hidden contexts or call UI from that worker.
- [ ] Feed real AI state into the host snapshot builder without modifying `PopupModel` visible/selectable rules. Default tense to unavailable and connection to local pipe health.
- [ ] Use shared layout/palette to redraw the host GDI fallback. Add correction/origin/partial underline/state shelf, system-color high contrast and no dependency on any GPU DLL in host initialization.
- [ ] Run `tip\test.cmd` with new regression assertions: status-only does not consume Tab; Enter always passes; after-space unselected Tab passes; phrase takeover waits 150 ms; arrow choice survives phrase arrival; stale keys and configured actions retain exact behavior.
- [ ] Build via `tip\build.cmd`, without registration/installation. Owner checks fallback in real apps during later rollout. Update roadmap and commit.

## Task 7: Connect TSF snapshots to renderer with failover

**Files:** create `tip/src/render_client.h/.cpp`; modify `tip/src/tsf_service.cpp`, `popup.cpp/.h`, `tip/build.cmd`; create `tip/tests/test_render_client.cpp` and a fake renderer pipe fixture.

**Consumes:** Task 3 codec/lease and Task 6 fallback. **Produces:** asynchronous external rendering with deterministic failover, no key handling in renderer.

- [ ] Write fake-peer tests for delayed ack, invalid peer identity, renderer disconnect/kill, old-generation/revision ack, show coalescing, hide priority and bounded queue. Foreground process change must revoke before rendering another snapshot.
- [ ] Normalize TSF screen rectangle at the boundary under actual host DPI-awareness. Verify DPI-aware, system-aware and unaware hosts manually at 150/200%; do not infer this only from pure multiplication tests.
- [ ] Implement worker-only I/O and UI-thread ack notifications. Latest unsent show wins; queue cap32; hide removes older unsent shows for that generation. Never wait on a pipe from `OnKeyDown`/TSF COM callbacks.
- [ ] Implement current-ack-only fallback suppression and 250 ms fallback deadline. On renderer outage hide external UI and restore host fallback; stop heartbeats when eligibility ends. Retain current `PopupModel` as the only acceptance authority even if UI rendering is late.
- [ ] Wire show/hide from actual focus/scope/pause/content/disconnect events. Send initial state only for the active eligible text service, not all instances in a process.
- [ ] Run pure/fake-pipe tests; verify paired host fallback/external windows do not duplicate on handoff. Update roadmap and commit.

## Task 8: Integrate process lifecycle, packaging and viewer contracts

**Files:** create `engine/src/completionist_engine/renderer_process.py`, `engine/tests/test_renderer_process.py`; modify `app.py`, `scripts/install.ps1`, `scripts/uninstall.ps1`, `tip/README.md`; document viewer contract in `engine/src/completionist_engine/viewer.py` only if runtime status needs exposing.

**Consumes:** known renderer build path and engine startup/shutdown. **Produces:** one supervised renderer, recoverable installation and documented integration. No dashboard layout changes.

- [ ] Test supervision with injected launcher/clock: missing executable -> host fallback; clean engine shutdown stops child; three failures in60s disable retries; existing session instance is reused; spawn path is fixed, no download/search through PATH.
- [ ] Implement lifecycle with an explicit service object:

```python
# Hidden-process launch primitive used by RendererProcess.start():
import subprocess
process = subprocess.Popen(
    [str(renderer_path)],
    stdin=subprocess.DEVNULL,
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
    creationflags=subprocess.CREATE_NO_WINDOW,
)
```

Expose `RendererProcess.start() -> None`, `poll(now: float) -> None` and `stop() -> None`. Derive `renderer_path` only from the known install/build path. Implement these methods using the hidden launch primitive, bounded crash history, current-session mutex detection and bounded graceful-stop/terminate of only the owned child. `poll` is called by existing engine maintenance, not a new always-busy thread. Test using injected fakes; never terminate an arbitrary process by name.

- [ ] Build/copy renderer in install flow before activating new TSF DLL. Preserve prior artifacts and cleanup only verified install-owned paths. Keep keyboard/registry/UAC steps user-run. Stop only owned renderer before replacing locked binary.
- [ ] Keep viewer Requests/context receipt in viewer, not native IPC/dock. Existing settings remain source of font, width, shortcuts and pause. Do not invent new glass sliders or viewer routes. Record separate viewer-design work still required for full V2.
- [ ] Run engine supervision tests and source-level install-path review; no installer execution in this task. Update roadmap and commit.

## Task 9: Validate and stage the desktop overhaul

**Files:** update `tip/README.md`, this plan's checkboxes, `docs/roadmap/roadmap.js`; create `docs/superpowers/verification/2026-10-02-v2-desktop-overhaul.md` with actual results and rollback commands during execution.

- [ ] Run `uv run --project engine pytest engine/tests`, `tip\test.cmd`, `tip\renderer\test.cmd`, `tip\build.cmd`, `tip\renderer\build.cmd`, and `python scripts/check_roadmap.py`. Record exact command/output and unrun checks.
- [ ] Stage both binaries for owner review. Supply manual matrix from spec; gather results and logs without typed-text/capture dumps. Run focus-stealing TSF harness only once owner says away.
- [ ] Verify actual automatic350ms/default-config countdown, manual shortcut, cancellation/new typing, streaming/ready/error, correction insert, local/learned origins, partial accept, minimized dock, both themes and reduced motion. Never use spike fixtures as proof of engine behavior.
- [ ] Verify two apps/monitors, process termination, unsupported/HDR fallback, lock/unlock and no retained topmost UI. Measure 4ms GPU/32ms foreground p95 and idle behavior on recorded hardware.
- [ ] Resolve failures before enabling the external renderer by default. If live material still fails its gate, do not label Acrylic or opaque fallback as completed liquid glass.
- [ ] Write rollback steps to restore previous DLL/renderer pair and restart affected apps; registration remains user-approved. Check remaining viewer work before any V2 release claim.
- [ ] Update roadmap refs/status only for actually completed tasks; do not create extra patch releases merely for checking plan boxes. Final commit includes roadmap validation; push/merge/release only within owner authorization.

## Coverage and execution handoff

Material feasibility -> Task1; actual engine data -> Task2; safety/IPC -> Task3; sizing/typography -> Task4; dock/motion/accessibility/fallback -> Task5; host keyboard/fallback -> Task6; TSF handoff -> Task7; supervision/install/viewer boundary -> Task8; real-app validation/rollout -> Task9. Review-focus cases each have an owning task above.

Recommended execution: implement sequentially in the current native workflow because capture, snapshot identity and fallback share tightly coupled interfaces. Use the existing `codex/native-autocomplete-v2` branch if it still contains this work; inspect current status before editing and preserve concurrent changes. Joint spec/plan review comes before implementation. A separate viewer design/plan is needed to finish the full V2 overhaul.

## Roadmap tracking

Each Task 1-9 is tracked as a local desktop stage targeting 2.0.0, with stable internal task IDs M10.14 through M10.22. Stages have no software release numbers; completion records roadwork toward the shared destination. M10.2 remains dropped as the superseded broad implementation task, with its notes and historical log references preserved. M10.1 retains scope/document review, and M10.3 retains the separate viewer track. Dependencies and acceptance checks are visible in each stage note, with links back to this plan and the specification. Shipping 2.0.0 is a separate action after both tracks and release validation are complete.
