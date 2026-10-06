# Glass Handoff Logging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** For any app and any keystroke, one log file and one command show which Completionist build is loaded, whether that app tried the glass popup, what the renderer answered and why, and whether the flat popup was shown.

**Architecture:** One header-only logger (`tip/src/glass_log.h`) is shared by the TSF DLL and the renderer. Every line goes to `%LOCALAPPDATA%\Completionist\glass.log` in one `key=value` format. If that fails, it goes to a per-process fallback file. It is always sent to `OutputDebugString` too. Both sides log every handoff step. `scripts/glass_report.py` reads the log and prints a per-app summary that names the first step that didn't happen.

**Tech Stack:** C++20 (VS 2022 Build Tools, `/MT`, x64), Win32; Python 3 standard library for the report.

**Spec:** `docs/superpowers/plans/2026-10-05-glass-popup-recovery.md` (Step 1, plus the Step 2 test procedure).

## Global Constraints

- C++ builds: `tip\build.cmd` (DLL) and `tip\renderer\build.cmd` (renderer). Static CRT `/MT`, x64, CLI-only toolchain. Both use `/utf-8`.
- No new native test suites, harnesses or fixtures, and don't run the existing native ones. DLL and renderer changes are verified in real life by the owner, using the checklist in Task 5.
- The report script gets one small `unittest` file next to it, like `scripts/test_roadmap_cli.py`.
- Glass logging is **always on** (not gated by the `verbose` file) until the owner says the glass works. Never log typed text, words, phrases or the API key: only counts, ids, handles, pids and reasons.
- A loaded DLL is locked: install by renaming `tip\out\CompletionistTip.dll` aside, then copying. An app only loads a new DLL after its **process** restarts. Win11 Notepad keeps one process for all its windows.
- Every commit updates the roadmap (`python scripts/roadmap.py log "..."`, then `python scripts/check_roadmap.py` must print `roadmap ok`).

## Log line format (all tasks rely on this)

```
HH:MM:SS.mmm glass <exe>[<pid>] step=<name> key=value key=value ...
```

- `<exe>` is the process's executable name, e.g. `Notepad.exe` or `CompletionistRenderer.exe`.
- Renderer lines about an app carry `app=<pid>` so the report can attach them to that app.
- Values have no spaces. Use `-` for empty values.

## File Structure

- Create `tip/src/glass_log.h`: the shared logger and the build banner. Used by both the DLL and the renderer.
- Modify `tip/src/tsf_service.cpp`: banner on first activation, eligibility reasons, render decisions, publish, notices.
- Modify `tip/src/render_client.cpp`: connect, disconnect, ack and timeout lines (replacing the current `LogDebug` renderer lines).
- Modify `tip/renderer/session.h`, `tip/renderer/session.cpp`: record why `Accept` refused a request.
- Modify `tip/renderer/production_renderer_pipe.cpp`, `tip/renderer/production_service.cpp`, `tip/renderer/main.cpp`: renderer steps and banner.
- Delete `tip/renderer/renderer_log.h` (replaced by `glass_log.h`).
- Create `scripts/glass_report.py` and `scripts/test_glass_report.py`: the report and its test.

---

### Task 1: Shared glass logger with build banner

**Files:**
- Create: `tip/src/glass_log.h`
- Modify: `tip/renderer/main.cpp` (banner at service start)
- Modify: `tip/src/tsf_service.cpp` (banner once per process in `ActivateEx`, near line 335)

**Interfaces:**
- Produces: `void completionist::GlassLog(const wchar_t* format, ...)` (writes one line; the format is the part after `<exe>[<pid>] `). Also `void completionist::GlassBanner(const wchar_t* component, const wchar_t* build)`, which writes `step=loaded component=<component> build=<build> path=<module path>`.

- [x] **Step 1: Create `tip/src/glass_log.h`**

```cpp
// Always-on log of the glass popup handoff, shared by the TSF DLL and the renderer.
// Every line goes to %LOCALAPPDATA%\Completionist\glass.log; if that can't be opened, to
// glass-<pid>.log beside it; and always to OutputDebugString. No typed text is ever logged.
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cwchar>

namespace completionist {

namespace glass_detail {

inline bool AppendUtf8(const wchar_t* path, const char* bytes, int length) {
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes, static_cast<DWORD>(length), &written, nullptr) && written == static_cast<DWORD>(length);
    CloseHandle(file);
    return ok;
}

inline const wchar_t* ExeName() {
    static wchar_t name[MAX_PATH] = {};
    if (!name[0]) {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        const wchar_t* slash = wcsrchr(path, L'\\');
        wcsncpy_s(name, slash ? slash + 1 : path, _TRUNCATE);
    }
    return name;
}

}  // namespace glass_detail

inline void GlassLog(const wchar_t* format, ...) {
    wchar_t message[1200];
    va_list args;
    va_start(args, format);
    vswprintf_s(message, format, args);
    va_end(args);
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t line[1500];
    swprintf_s(line, L"%02u:%02u:%02u.%03u glass %s[%lu] %s\r\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
               glass_detail::ExeName(), GetCurrentProcessId(), message);
    OutputDebugStringW(line);
    char utf8[4500];
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), nullptr, nullptr);
    if (bytes <= 1) return;
    wchar_t dir[MAX_PATH];
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH);
    if (!length || length >= MAX_PATH - 64) return;
    wcscat_s(dir, L"\\Completionist");
    CreateDirectoryW(dir, nullptr);
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\glass.log", dir);
    if (glass_detail::AppendUtf8(path, utf8, bytes - 1)) return;
    const DWORD error = GetLastError();
    swprintf_s(path, L"%s\\glass-%lu.log", dir, GetCurrentProcessId());
    static volatile LONG reported = 0;
    if (InterlockedExchange(&reported, 1) == 0) {
        char note[200];
        const int n = sprintf_s(note, "glass.log could not be opened (error %lu); writing here instead\r\n", error);
        glass_detail::AppendUtf8(path, note, n);
    }
    glass_detail::AppendUtf8(path, utf8, bytes - 1);
}

inline void GlassBanner(const wchar_t* component, const wchar_t* build, HMODULE module) {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(module, path, MAX_PATH);
    for (wchar_t* c = path; *c; ++c) if (*c == L' ') *c = L'?';  // keep values space-free
    GlassLog(L"step=loaded component=%s build=%s path=%s", component, build, path);
}

}  // namespace completionist

// Compile-time build stamp of the file that expands it, e.g. "Oct_5_2026_19:20:01".
#define COMPLETIONIST_GLASS_BUILD (L"" __DATE__ L"_" __TIME__)
```

- [x] **Step 2: Banner in the DLL, once per process**

In `tip/src/tsf_service.cpp`, add `#include "glass_log.h"` beside the other `#include "..."` lines. In `ActivateEx`, directly after `completionist::RefreshLogLevel();`:

```cpp
        static volatile LONG bannerWritten = 0;
        if (InterlockedExchange(&bannerWritten, 1) == 0)
            completionist::GlassBanner(L"dll", COMPLETIONIST_GLASS_BUILD, g_module);
        completionist::GlassLog(L"step=activate thread=%lu", GetCurrentThreadId());
```

(`__DATE__` has double spaces for single-digit days, such as `Oct  5 2026`. That's fine, because the report reads `build=` up to the next space. Replace them to be safe: see Step 3.)

- [x] **Step 3: Make the build stamp space-free**

In `glass_log.h`, change `GlassBanner` so the `build` value has its spaces replaced as well:

```cpp
inline void GlassBanner(const wchar_t* component, const wchar_t* build, HMODULE module) {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(module, path, MAX_PATH);
    wchar_t stamp[64] = {};
    wcsncpy_s(stamp, build, _TRUNCATE);
    for (wchar_t* c = path; *c; ++c) if (*c == L' ') *c = L'?';
    for (wchar_t* c = stamp; *c; ++c) if (*c == L' ') *c = L'_';
    GlassLog(L"step=loaded component=%s build=%s path=%s", component, stamp, path);
}
```

- [x] **Step 4: Banner in the renderer**

In `tip/renderer/main.cpp`, add `#include "../src/glass_log.h"`. In the `--serve` branch, before `result = renderer::runProductionService();`:

```cpp
        completionist::GlassBanner(L"renderer", COMPLETIONIST_GLASS_BUILD, nullptr);
```

- [x] **Step 5: Build both**

Run: `cmd /c tip\build.cmd` then `cmd /c tip\renderer\build.cmd` from the worktree root.
Expected: both finish with no `error` lines (`/W4 /WX`, so warnings fail the build too).

- [x] **Step 6: Commit**

```bash
git add tip/src/glass_log.h tip/src/tsf_service.cpp tip/renderer/main.cpp
python scripts/roadmap.py start glass-logging
python scripts/roadmap.py log "Added the shared always-on glass log with a build banner in the DLL and renderer."
python scripts/check_roadmap.py
git add docs/roadmap/roadmap.js
git commit -m "Add the shared glass log and build banner"
```

---

### Task 2: Log every DLL handoff step

**Files:**
- Modify: `tip/src/tsf_service.cpp`: `RefreshRendererOwner` (about lines 566-588), `HideExternal` (about line 560), `Render` (about lines 590-680), and the `kRenderClientNoticeMessage` branch of `PopupHook` (about lines 1055-1080).
- Modify: `tip/src/render_client.cpp`: `ConnectFailed`/`Connect` (about lines 97-130), `Start` (about line 303), the `disconnect` lambda, ack handling and timeout in `Worker` (about lines 375-450).

**Interfaces:**
- Consumes: `completionist::GlassLog` from Task 1.
- Produces these `step=` names, which the report (Task 4) counts: `activate`, `eligible`, `render`, `publish`, `no-caret`, `hide-external`, `client-start`, `connect`, `disconnect`, `ack`, `timeout`, `notice`.

- [x] **Step 1: Eligibility with a reason**

Replace the body of `RefreshRendererOwner` with:

```cpp
    void RefreshRendererOwner(ITfContext* context) {
        HWND foreground = GetForegroundWindow();
        DWORD pid = 0;
        if (foreground) GetWindowThreadProcessId(foreground, &pid);
        ITfContextView* view = nullptr;
        HWND viewWindow = nullptr;
        const wchar_t* reason = L"ok";
        if (!context || FAILED(context->GetActiveView(&view)) || !view) {
            reason = L"no-view";
        } else {
            view->GetWnd(&viewWindow);
            view->Release();
            const HWND foregroundRoot = foreground ? GetAncestor(foreground, GA_ROOT) : nullptr;
            const HWND viewRoot = viewWindow ? GetAncestor(viewWindow, GA_ROOT) : nullptr;
            if (!foreground) reason = L"no-foreground";
            else if (!viewWindow) reason = L"no-view-window";
            else if (foregroundRoot != viewRoot) reason = L"different-root";
            else if (pid != GetCurrentProcessId()) reason = L"foreground-other-process";
            else if (renderSession_.empty()) reason = L"no-session";
        }
        rendererEligible_ = wcscmp(reason, L"ok") == 0;
        if (rendererEligible_)
            renderIdentity_ = {pid, reinterpret_cast<uint64_t>(foreground), renderSession_, completionist::render::NextRenderGeneration()};
        completionist::GlassLog(L"step=eligible value=%d reason=%s fg=%p fgpid=%lu view=%p gen=%llu",
                                rendererEligible_ ? 1 : 0, reason, foreground, pid, viewWindow,
                                rendererEligible_ ? renderIdentity_.generation : 0ull);
    }
```

Delete the `loggedEligible_` member and the `if (publishExternal && rendererEligible_ != loggedEligible_) {...}` block in `Render`, because this line replaces them.

- [x] **Step 2: Publish, missing caret and the render decision**

In `Render`, right after `renderClient_->Publish(std::move(snapshot));` (the snapshot is moved, so take the values first). Replace that line and the following `published = true;` with:

```cpp
                const uint64_t publishedRevision = snapshot.revision;
                const uint64_t publishedGeneration = snapshot.owner.generation;
                const size_t publishedWords = snapshot.words.size();
                const int publishedSelection = snapshot.selection;
                renderClient_->Publish(std::move(snapshot));
                published = true;
                completionist::GlassLog(L"step=publish rev=%llu gen=%llu words=%zu selection=%d",
                                        publishedRevision, publishedGeneration, publishedWords, publishedSelection);
```

Replace the existing `LogDebug(L"renderer skipped: caret position ...")` with:

```cpp
                completionist::GlassLog(L"step=no-caret caret=%ld,%ld,%ld,%ld", caret_.left, caret_.top, caret_.right, caret_.bottom);
```

Replace the two lines `if (published && externalHealthy_) popup_.Hide(); else popup_.Show(...);` with:

```cpp
        const bool hostHidden = published && externalHealthy_;
        if (hostHidden) popup_.Hide();
        else popup_.Show(content, model_.selection(now), caret_);
        completionist::GlassLog(L"step=render external=%d eligible=%d published=%d healthy=%d host=%s",
                                publishExternal ? 1 : 0, rendererEligible_ ? 1 : 0, published ? 1 : 0,
                                externalHealthy_ ? 1 : 0, hostHidden ? L"hidden" : L"shown");
```

At the top of `Render`, in the early-return branch where nothing is visible (`if (!model_.visible() && !hasStatus) {`), add as the first line inside it:

```cpp
            completionist::GlassLog(L"step=render external=%d host=closed", publishExternal ? 1 : 0);
```

- [x] **Step 3: Hide and notices**

In `HideExternal`, replace the body with:

```cpp
    void HideExternal() {
        const bool sent = renderClient_ && renderIdentity_.pid && renderIdentity_.hostHwnd && renderIdentity_.generation;
        if (sent) renderClient_->Hide(renderIdentity_, ++renderRevision_);
        completionist::GlassLog(L"step=hide-external sent=%d rev=%llu", sent ? 1 : 0, renderRevision_);
        externalPresented_ = false;
    }
```

In `PopupHook`, inside `if (message == completionist::render::kRenderClientNoticeMessage) {`, directly after the `notice` unique_ptr is created, add:

```cpp
                completionist::GlassLog(L"step=notice ack=%d rev=%llu presented=%d fallback=%d matches=%d healthy=%d",
                    notice && notice->ack ? 1 : 0, notice && notice->ack ? notice->ack->revision : 0ull,
                    notice && notice->ack && notice->ack->presented ? 1 : 0, notice && notice->fallbackVisible ? 1 : 0,
                    notice && notice->ack && notice->ack->revision == service->renderRevision_ ? 1 : 0,
                    service->externalHealthy_ ? 1 : 0);
```

- [x] **Step 4: Client connection lines**

In `tip/src/render_client.cpp`, replace `#include "log.h"` with `#include "glass_log.h"`, then make these replacements:

```cpp
// in ConnectFailed, replace the LogDebug call:
        completionist::GlassLog(L"step=connect result=fail reason=%s error=%lu", reason.c_str(), error);
// in Connect, replace LogDebug(L"renderer connected"):
    completionist::GlassLog(L"step=connect result=ok");
// in Start, replace LogDebug(L"renderer client started"):
    completionist::GlassLog(L"step=client-start");
// in the disconnect lambda, replace LogDebug(L"renderer disconnected"):
            completionist::GlassLog(L"step=disconnect");
// replace the ack LogDebug:
                completionist::GlassLog(L"step=ack rev=%llu presented=%d accepted=%d", ack->revision,
                                        ack->presented ? 1 : 0, accepted ? 1 : 0);
// replace the timeout LogDebug:
            completionist::GlassLog(L"step=timeout");
```

Values must be space-free, so change the reason strings passed to `ConnectFailed` in `Connect` to exactly: `L"no-pipe-name"`, `L"wait"`, `L"open"`, `L"pipe-mode"`, `L"server-identity"` (drop the pipe name that was appended to `wait`/`open`). Leave the `g_lastConnectFailure` de-duplication unchanged, so a repeating failure is logged once.

Also log when the worker sees a write failure. In `Worker`, change `if (frame.empty() || !WriteFrame(pipe, frame)) alive = false;` to:

```cpp
            if (frame.empty() || !WriteFrame(pipe, frame)) {
                completionist::GlassLog(L"step=write-failed empty=%d error=%lu", frame.empty() ? 1 : 0, GetLastError());
                alive = false;
            }
```

and after `alive = ReadAck(pipe, &pending, &ack);` add:

```cpp
            if (!alive) completionist::GlassLog(L"step=read-failed error=%lu", GetLastError());
```

- [x] **Step 5: Build the DLL**

Run: `cmd /c tip\build.cmd`
Expected: no `error` lines.

- [x] **Step 6: Commit**

```bash
git add tip/src/tsf_service.cpp tip/src/render_client.cpp
python scripts/roadmap.py log "The DLL now logs every glass handoff step: eligibility reason, publish, render decision, connect, ack, notices."
python scripts/check_roadmap.py
git add docs/roadmap/roadmap.js
git commit -m "Log every DLL glass handoff step"
```

---

### Task 3: Log every renderer step with refusal reasons

**Files:**
- Modify: `tip/renderer/session.h`, `tip/renderer/session.cpp`
- Modify: `tip/renderer/production_renderer_pipe.cpp`
- Modify: `tip/renderer/production_service.cpp`
- Delete: `tip/renderer/renderer_log.h`

**Interfaces:**
- Consumes: `completionist::GlassLog` from Task 1.
- Produces `const char* renderer::Session::LastRefusal() const`, which returns why the last `Accept` returned false: one of `bad-identity`, `not-foreground`, `no-content`, `stale`, `other-app-active`, or `none`.
- Produces these `step=` names: `client-connected`, `client-released`, `refused`, `drawn`, `lost-foreground`, `command-ignored`, `hide`, `present-skipped`, `opaque`, `glass-failed`, `unreadable-command`.

- [x] **Step 1: Refusal reason in `Session`**

In `session.h`, add a public accessor and a private member:

```cpp
    const char* LastRefusal() const { return refusal_; }
```

```cpp
    const char* refusal_ = "none";
```

In `session.cpp`, rewrite `Accept` so every `return false` first sets `refusal_`:

```cpp
bool Session::Accept(const completionist::render::Snapshot& snapshot, uint32_t foregroundPid, uint64_t nowMs) {
    refusal_ = "none";
    if (snapshot.owner.pid == 0 || snapshot.owner.hostHwnd == 0 || snapshot.owner.session.empty() ||
        snapshot.owner.generation == 0 || snapshot.revision == 0) { refusal_ = "bad-identity"; return false; }
    if (foregroundPid != snapshot.owner.pid) { refusal_ = "not-foreground"; return false; }
    if (!HasUsableContent(snapshot)) { refusal_ = "no-content"; return false; }
    Expire(nowMs);
    auto floor = floors_.find(snapshot.owner.session);
    if (floor != floors_.end() && (snapshot.owner.generation < floor->second.generation ||
        (snapshot.owner.generation == floor->second.generation && snapshot.revision <= floor->second.revision))) {
        refusal_ = "stale"; return false;
    }
    if (active_ && SameHost(snapshot_.owner, snapshot.owner)) {
        if (snapshot.owner.generation < snapshot_.owner.generation) { refusal_ = "stale"; return false; }
        if (snapshot.owner.generation == snapshot_.owner.generation && snapshot.revision <= snapshot_.revision) { refusal_ = "stale"; return false; }
        if (snapshot.owner.generation > snapshot_.owner.generation) Retire(snapshot_.owner, UINT64_MAX);
    } else if (active_) {
        if (foregroundPid == snapshot_.owner.pid && nowMs - renewedAtMs_ <= kLeaseMs) { refusal_ = "other-app-active"; return false; }
        Retire(snapshot_.owner, snapshot_.revision);
    }
    snapshot_ = snapshot;
    renewedAtMs_ = nowMs;
    active_ = true;
    return true;
}
```

- [x] **Step 2: Pipe steps**

In `production_renderer_pipe.cpp`, replace `#include "renderer_log.h"` with `#include "../src/glass_log.h"`. Then make these replacements:

```cpp
// in Run's client thread:
            completionist::GlassLog(L"step=client-connected app=%lu", clientPid);
            ...
            completionist::GlassLog(L"step=client-released app=%lu", clientPid);
// foreground loss:
                completionist::GlassLog(L"step=lost-foreground app=%lu fgpid=%lu fg=%p", current->owner.pid, foregroundPid, foreground);
```

Replace the refused-show branch with one that logs which check failed:

```cpp
                const DWORD foreground = ForegroundPid();
                const bool identityOk = Validate(snapshot->owner, pipe);
                if (!identityOk || !session_.Accept(*snapshot, foreground, NowMs())) {
                    completionist::GlassLog(L"step=refused app=%lu rev=%llu gen=%llu why=%S fgpid=%lu fg=%p host=%llx words=%zu selection=%d",
                        snapshot->owner.pid, snapshot->revision, snapshot->owner.generation,
                        identityOk ? session_.LastRefusal() : "identity-check", foreground, GetForegroundWindow(),
                        snapshot->owner.hostHwnd, snapshot->words.size(), snapshot->selection);
```

Replace the drawn line:

```cpp
                completionist::GlassLog(L"step=drawn app=%lu rev=%llu gen=%llu drawn=%d current=%d",
                    snapshot->owner.pid, snapshot->revision, snapshot->owner.generation, drawn ? 1 : 0, stillCurrent ? 1 : 0);
```

Replace the unreadable-command log with `completionist::GlassLog(L"step=unreadable-command");`. Before `if (!owns) continue;` add:

```cpp
            if (!owns) completionist::GlassLog(L"step=command-ignored app=%lu kind=%d why=not-owner", command->owner.pid,
                                               static_cast<int>(command->command));
```

In the Hide branch, before `hide_();`, add `completionist::GlassLog(L"step=hide app=%lu rev=%llu", command->owner.pid, command->revision);`.

- [x] **Step 3: Presenter steps**

In `production_service.cpp`, replace `#include "renderer_log.h"` with `#include "../src/glass_log.h"` and every `Log(` call:

```cpp
// not foreground:
            completionist::GlassLog(L"step=present-skipped app=%lu why=not-foreground host=%p fg=%p fgpid=%lu visible=%d",
                snapshot.owner.pid, host, foreground, pid, IsWindowVisible(host) ? 1 : 0);
// DPI:
        if (!DpiReady()) { completionist::GlassLog(L"step=present-skipped app=%lu why=dpi", snapshot.owner.pid); Hide(); return false; }
// graphics:
            completionist::GlassLog(L"step=opaque app=%lu why=no-graphics-device", snapshot.owner.pid);
// opaque material: keep the existing flags, change the prefix to
            completionist::GlassLog(L"step=opaque app=%lu why=material mode=%d frame=%d capture=%d excluded=%d composition=%d hdr=%d contrast=%d failure=%d", snapshot.owner.pid, ...same arguments...);
// glass failure:
        if (!glass) completionist::GlassLog(L"step=glass-failed app=%lu", snapshot.owner.pid);
```

Then delete `tip/renderer/renderer_log.h`.

- [x] **Step 4: Build the renderer**

Run: `cmd /c tip\renderer\build.cmd`
Expected: no `error` lines. `grep -rn "renderer_log.h" tip/renderer` prints nothing.

- [x] **Step 5: Commit**

```bash
git add -A tip/renderer
python scripts/roadmap.py log "The renderer now logs every glass step to glass.log with the exact refusal reason."
python scripts/check_roadmap.py
git add docs/roadmap/roadmap.js
git commit -m "Log every renderer glass step with refusal reasons"
```

---

### Task 4: One-command glass report

**Files:**
- Create: `scripts/glass_report.py`
- Test: `scripts/test_glass_report.py`

**Interfaces:**
- Consumes: the log line format above, and the `step=` names from Tasks 1-3.
- Produces: `summarize(lines: list[str]) -> dict[int, dict]` (per app pid) and `diagnose(app: dict) -> str`, plus the command line `python scripts/glass_report.py [--clear] [--log PATH]`.

- [x] **Step 1: Write the failing test**

```python
"""The glass report turns glass.log lines into a per-app summary and names the first missing step."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import glass_report

LINES = [
    "19:00:00.000 glass Notepad.exe[100] step=loaded component=dll build=Oct__5_2026_19:00:00 path=C:\\x.dll",
    "19:00:00.010 glass Notepad.exe[100] step=client-start",
    "19:00:00.020 glass Notepad.exe[100] step=connect result=ok",
    "19:00:01.000 glass Notepad.exe[100] step=eligible value=1 reason=ok fg=1 fgpid=100 view=2 gen=3",
    "19:00:01.100 glass Notepad.exe[100] step=publish rev=1 gen=3 words=5 selection=0",
    "19:00:01.110 glass CompletionistRenderer.exe[200] step=refused app=100 rev=1 gen=3 why=stale fgpid=100 fg=1 host=1 words=5 selection=0",
    "19:00:01.120 glass Notepad.exe[100] step=ack rev=1 presented=0 accepted=1",
    "19:00:01.130 glass Notepad.exe[100] step=render external=1 eligible=1 published=1 healthy=0 host=shown",
    "19:00:01.900 glass Claude.exe[300] step=loaded component=dll build=b path=p",
    "19:00:02.000 glass Claude.exe[300] step=connect result=fail reason=wait error=2",
]


class GlassReport(unittest.TestCase):
    def test_summary_counts_steps_per_app_and_attaches_renderer_lines(self):
        apps = glass_report.summarize(LINES)
        notepad = apps[100]
        self.assertEqual(notepad["exe"], "Notepad.exe")
        self.assertEqual(notepad["build"], "Oct__5_2026_19:00:00")
        self.assertEqual(notepad["steps"]["publish"], 1)
        self.assertEqual(notepad["refused"], {"stale": 1})
        self.assertEqual(notepad["host"], {"shown": 1})
        self.assertEqual(apps[300]["connect_failures"], {"wait/2": 1})

    def test_diagnosis_names_the_first_missing_step(self):
        apps = glass_report.summarize(LINES)
        self.assertIn("refused", glass_report.diagnose(apps[100]))
        self.assertIn("never connected", glass_report.diagnose(apps[300]))
        self.assertIn("no banner", glass_report.diagnose({"exe": "x", "build": None, "steps": {}, "refused": {},
                                                          "host": {}, "connect_failures": {}, "drawn": 0,
                                                          "connected": False}))


if __name__ == "__main__":
    unittest.main()
```

- [x] **Step 2: Run test to verify it fails**

Run: `python -m unittest scripts/test_glass_report.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'glass_report'`.

- [x] **Step 3: Write the report**

```python
"""Summarize glass.log: per app, which build ran, what it tried, what the renderer answered.

python scripts/glass_report.py          print the report
python scripts/glass_report.py --clear  empty the log before a test
"""
from __future__ import annotations

import argparse
import os
import re
import sys
from collections import Counter
from pathlib import Path

LOG = Path(os.environ.get("LOCALAPPDATA", "")) / "Completionist" / "glass.log"
LINE = re.compile(r"^(\d\d:\d\d:\d\d\.\d{3}) glass (\S+)\[(\d+)\] (.*)$")
PAIR = re.compile(r"(\w+)=(\S+)")
RENDERER = "CompletionistRenderer.exe"


def _app(apps: dict[int, dict], pid: int, exe: str | None = None) -> dict:
    app = apps.setdefault(pid, {"exe": exe, "build": None, "steps": Counter(), "refused": Counter(),
                                "host": Counter(), "connect_failures": Counter(), "drawn": 0, "connected": False,
                                "last": ""})
    if exe and not app["exe"]:
        app["exe"] = exe
    return app


def summarize(lines: list[str]) -> dict[int, dict]:
    apps: dict[int, dict] = {}
    for raw in lines:
        match = LINE.match(raw.strip())
        if not match:
            continue
        time, exe, pid, rest = match.group(1), match.group(2), int(match.group(3)), match.group(4)
        fields = dict(PAIR.findall(rest))
        step = fields.get("step", "?")
        if exe == RENDERER:
            if "app" not in fields:
                continue
            app = _app(apps, int(fields["app"]))
            if step == "refused":
                app["refused"][fields.get("why", "?")] += 1
            elif step == "drawn" and fields.get("drawn") == "1" and fields.get("current") == "1":
                app["drawn"] += 1
            app["steps"]["renderer:" + step] += 1
        else:
            app = _app(apps, pid, exe)
            app["steps"][step] += 1
            if step == "loaded":
                app["build"] = fields.get("build")
            elif step == "connect" and fields.get("result") == "ok":
                app["connected"] = True
            elif step == "connect" and fields.get("result") == "fail":
                app["connect_failures"][f"{fields.get('reason', '?')}/{fields.get('error', '?')}"] += 1
            elif step == "render" and "host" in fields:
                app["host"][fields["host"]] += 1
        app["last"] = time
    for app in apps.values():
        app["steps"] = dict(app["steps"])
        app["refused"] = dict(app["refused"])
        app["host"] = dict(app["host"])
        app["connect_failures"] = dict(app["connect_failures"])
    return apps


def diagnose(app: dict) -> str:
    steps = app["steps"]
    if not app["build"]:
        return "no banner: this process never logged a DLL load (old build, or the DLL never activated here)"
    if not app["connected"]:
        failures = ", ".join(app["connect_failures"]) or "no attempt logged"
        return "never connected to the renderer: " + failures
    if not steps.get("eligible"):
        return "never checked eligibility (no focus change logged since load)"
    if not steps.get("publish"):
        return "eligible checks ran but nothing was published to the renderer (see step=eligible reasons)"
    if app["refused"] and not app["drawn"]:
        return "renderer refused every request: " + ", ".join(f"{k} x{v}" for k, v in app["refused"].items())
    if app["host"].get("shown"):
        return f"glass drawn {app['drawn']} times, but the flat popup was still shown {app['host']['shown']} times"
    return f"glass drawn {app['drawn']} times; flat popup never shown"


def main(argv: list[str] | None = None) -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--clear", action="store_true", help="empty the log before a test")
    parser.add_argument("--log", type=Path, default=LOG)
    args = parser.parse_args(argv)
    if args.clear:
        args.log.write_text("", encoding="utf-8")
        for extra in args.log.parent.glob("glass-*.log"):
            extra.unlink()
        print(f"cleared {args.log}")
        return 0
    lines: list[str] = []
    for path in [args.log, *sorted(args.log.parent.glob("glass-*.log"))]:
        if path.exists():
            lines += path.read_text(encoding="utf-8", errors="replace").splitlines()
    if not lines:
        print(f"no glass log lines in {args.log}")
        return 1
    for pid, app in sorted(summarize(lines).items(), key=lambda item: item[1]["last"]):
        print(f"{app['exe'] or '?'} [{pid}] build={app['build'] or '-'} last={app['last']}")
        print(f"  steps: {app['steps']}")
        if app["refused"]:
            print(f"  refused: {app['refused']}")
        if app["connect_failures"]:
            print(f"  connect failures: {app['connect_failures']}")
        print(f"  host popup: {app['host'] or '-'}   glass drawn: {app['drawn']}")
        print(f"  => {diagnose(app)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [x] **Step 4: Run test to verify it passes**

Run: `python -m unittest scripts/test_glass_report.py -v`
Expected: 2 tests, OK.

- [x] **Step 5: Commit**

```bash
git add scripts/glass_report.py scripts/test_glass_report.py
python scripts/roadmap.py log "Added scripts/glass_report.py: one command summarizes glass.log per app and names the first missing step."
python scripts/check_roadmap.py
git add docs/roadmap/roadmap.js
git commit -m "Add the one-command glass report"
```

---

### Task 5: Install and run the first real test (spec Step 2)

**Files:**
- Modify: `docs/roadmap/roadmap.js` (via the helper only)

**Interfaces:**
- Consumes: the builds from Tasks 1-3 and the report from Task 4.

- [x] **Step 1: Install the DLL and renderer, restart the engine**

From PowerShell (the engine runs from the main checkout at `C:\projects\Experiments\completionist`):

```powershell
$w = "C:\projects\Experiments\completionist\.claude\worktrees\v2-design-review-85fa28\tip"
$out = "C:\projects\Experiments\completionist\tip\out"
Rename-Item "$out\CompletionistTip.dll" ("CompletionistTip.{0}.old" -f (Get-Random))
Copy-Item "$w\out\CompletionistTip.dll" "$out\CompletionistTip.dll"
Get-CimInstance Win32_Process -Filter "Name='python.exe'" | Where-Object { $_.CommandLine -match "completionist.py" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
Get-Process CompletionistRenderer -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 2
Copy-Item "$w\renderer\out\CompletionistRenderer.exe" "$out\CompletionistRenderer.exe" -Force
python C:\projects\Experiments\completionist\.claude\worktrees\v2-design-review-85fa28\scripts\glass_report.py --clear
Start-Process -FilePath "C:\projects\Experiments\completionist\engine\.venv\Scripts\python.exe" -ArgumentList "completionist.py" -WorkingDirectory "C:\projects\Experiments\completionist\engine" -WindowStyle Hidden
```

Expected: `glass.log` gets a `step=loaded component=renderer` line within about 10 seconds.

- [ ] **Step 2: Owner checklist (hand this to the owner exactly)**

1. Close every Notepad window. In Task Manager, confirm no "Notepad" process is left; end it if one is.
2. Open Notepad. Type `this is`, wait two seconds, then type ` separ`.
3. Say what you saw: glass the whole time, flat box the whole time, or switching between them.

- [ ] **Step 3: Read the report**

Run: `python scripts/glass_report.py`
Expected: a Notepad entry whose `build=` matches today's build, with a `=>` diagnosis line. If Notepad has no entry at all, that answers the question: the DLL never activated there. Write that down.

- [ ] **Step 4: Record the finding**

```bash
python scripts/roadmap.py done glass-logging --refs tip/src/glass_log.h,scripts/glass_report.py --note "Owner test <date>: <what the owner saw>. Report: <diagnosis line for Notepad>."
python scripts/check_roadmap.py
git add docs/roadmap/roadmap.js
git commit -m "Record the first glass report"
```

The next fix (spec Step 3) is planned only from that diagnosis line, as one change with a before/after report.

---

## Self-review notes

- Spec Step 1 coverage: the banner (Task 1), logging that never silently drops (Task 1: fallback file plus `OutputDebugString`), every handoff step in both processes (Tasks 2-3), refusal reasons (Task 3), the one-command summary (Task 4). Spec Step 2 is Task 5.
- The spec says to turn the extra logging off with `verbose` later. That's deliberately not done here: the logging stays always on until the owner confirms the glass works, per the Global Constraints.
- Names used consistently: `GlassLog`, `GlassBanner`, `COMPLETIONIST_GLASS_BUILD`, `LastRefusal`, `summarize`, `diagnose`.

## Execution notes (2026-10-05)

Tasks 1–4 are implemented in this worktree. Both native components were built with the prescribed scripts; the report has nine passing unittest cases, and the engine baseline passed 659 tests with one skip. No native test harness was run.

The eligibility rewrite preserves the original foreground-root check. Request identifiers were also added to acknowledgements, notices, presentation, timeout and write-failure lines. Review fixes preserve Windows error codes, assign deterministic codes to malformed acknowledgements and empty frames, and send primary log-file failure codes to debugger output even if the fallback file fails.

The report additionally names eligibility/presentation failures, distinguishes opaque renderer popups from glass, diagnoses published requests without confirmed draws, handles fallback logs, and explains renderer-only logs. These changes close gaps in the provided report example without changing popup behavior.

Task 5 installation is complete. The daily-use DLL and renderer were replaced and the engine restarted. Renderer PID 19884 logged build Oct__5_2026_19:56:11 from the installed tip/out path. The owner received the exact fresh-Notepad checklist. The first report contains renderer connections from existing processes without new DLL banners; no fresh-Notepad result is claimed. The report now treats a renderer client-connected event as connection evidence even when an older DLL has no banner.

Task 5 remains pending until the owner’s fresh-Notepad observation and its report diagnosis are recorded. No popup recovery or visual result is claimed.
