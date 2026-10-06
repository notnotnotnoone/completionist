# Glass popup recovery plan

Date: 2026-10-05. Roadmap destination: 2.2.0 (`glass-renderer-takeover`, `fallback-popup-prototype`).

## Where we are (facts, not guesses)

- 2.0.0 was marked shipped before anyone saw it on screen. The first real look (2026-10-05) showed the old flat popup, not glass.
- Three bugs found and fixed so far, each confirmed by logs:
  1. The renderer served one app at a time; an idle background app held the only slot forever. Fixed: one thread per app.
  2. The renderer permanently refused an app after its popup hid once. Fixed: a hide only refuses older requests.
  3. The DLL drew the flat popup first on every keystroke and hid it after glass confirmed (the 0.2 s flash). Fixed in the 18:51 DLL, but **not yet seen working**.
- Open, not understood: a Notepad started after the 18:51 DLL (process 26520) shows the flat popup, never connects to the renderer, and writes **no log lines anywhere**. It is not sandboxed (normal medium-integrity process, checked). Claude, running the older 11:33 DLL, connects and disconnects many times a second.
- Each fix exposed a new problem in a different place. That is a signal to question the design, not to keep patching (step 4 below).

## Step 1: logging that cannot fail silently (do first, before any fix)

Goal: for any app and any keystroke, the logs answer "which build is loaded, did it try glass, what did the renderer say, what was drawn".

- **DLL load banner, always on** (not gated on the `verbose` file): on load, write the DLL build time, its file path, the app name and process id. If this line is missing for an app, the DLL never ran there.
- **Logging never silently drops.** If `%LOCALAPPDATA%\Completionist\tip.log` cannot be opened, try a second file (`%LOCALAPPDATA%\Completionist\tip-<pid>.log`), and always also send the line to `OutputDebugString`. Record the failure code once.
- **Every handoff step logged with one shared id** (window + revision), in both DLL and renderer: popup content ready, eligible or not and why, publish, connect attempt and result, ack received (drawn or refused and why), fallback timer fired, flat popup shown or hidden and why, glass shown or hidden and why.
- **Renderer logs its build time and every refusal reason**, including which check failed (foreground, identity, stale revision, lease expired).
- **A one-command summary**: `python scripts/glass_report.py` reads both logs and prints, per app: build loaded, connected yes/no, requests sent, drawn, refused (by reason), fallbacks. This is the first thing we run after every test.
- Turn the extra logging off with the existing `verbose` file once things work; the banner and errors stay on.

Done when: the user types in a fresh Notepad and the report shows every step for that Notepad, or names the exact step where it stopped.

## Step 2: find the Notepad cause with the new logs

- User restarts Notepad (confirm the old process is gone in Task Manager), types `this is`.
- Run the report. One of: banner missing (DLL not active there), not eligible (which check), connect failed (which error), refused (which reason), drawn but hidden by the DLL.
- Also explain Claude's rapid connect/disconnect loop from the same report.
- No fix until the report names the step.

## Step 3: fix only what the report names, one change at a time

- Each fix gets a before/after report from the same test.
- The user confirms on screen: glass stays glass while typing, no flat-popup flash, in Notepad, Chrome and Claude.

## Step 4: decide whether the two-process design is right (needs the owner)

The glass is drawn by a separate program that every app must reach through a pipe, prove its identity, wait for an answer and fall back on a timer. Every bug so far lived in that handshake.

Options:

- **A. Keep the separate renderer** (live screen capture gives true see-through glass). Cost: the handshake stays the fragile part; needs the logging above permanently.
- **B. Draw a Windows-provided frosted material inside each app's own popup** (Acrylic, no screen capture, no pipe). Simpler and much harder to break, but frosted rather than lens-like. The earlier review called plain Acrylic "too simple".
- **C. Hybrid**: B everywhere by default, A only if it proves stable.

Decide after Step 3, with real reports in hand.

## Step 5: make it look like the approved prototype

Whichever material wins, the layout must match `tip/prototype/index.html`: soft pink AI strip with label, visible typed text, origin pills, shortcut footer, corner dock, readable text over busy backgrounds. Same layout in the flat fallback.

## Rules for this work

- Every test ends with the report, not a guess.
- Nothing is called fixed until the user has seen it on screen.
- An app only picks up a new DLL after its process restarts; the report's banner line proves which build ran.

## 2026-10-05: first logged Notepad result and single follow-up fix

The new log proves fresh Notepad PID 31692 loaded DLL build Oct__5_2026_19:55:15. The owner reported extremely rapid flicker. The initial report recorded 25 confirmed renderer draws, 26 flat-popup shows, no timeout, and no disconnect. At revision 15, publish/host-hidden appeared at 20:04:28.993–994, an acknowledgement-free fallback-visible notice at 20:04:28.994, flat shown at 20:04:29.001, renderer drawn at 20:04:29.032, and successful acknowledgement/flat hidden at 20:04:29.042. The same sequence repeats roughly every 110 ms during status refreshes.

Cause: RenderClientState::Publish sets fallbackVisible_ to true on each revision, even after glass was confirmed. RenderClient::Publish posts the resulting fallback notice, and PopupHook shows the flat popup until the next acknowledgement.

One follow-up change preserves confirmed presentation for updates to the same owner identity. It also preserves the earliest pending acknowledgement deadline so repeated updates cannot defer fallback forever. New ownership, refusal, timeout, hide and disconnect still return to fallback. The DLL build and scoped review passed, and the rebuilt DLL was installed with matching hashes. The renderer was unchanged. The before-log was preserved so a fresh Notepad process can supply a separate after-report. Owner repeat observation and after-report are pending; the flicker is not yet called resolved.

## Overnight continuation requested by owner (2026-10-05)

The owner reports that rapid glass-to-flat alternation improved, but glass still flashes and sometimes remains permanently on the old popup. They explicitly request comprehensive testing, relevant code cleanup, mostly inline work, minimal subagents, and autonomous work overnight. Three subagents have been used; further work is inline unless one broad review is useful. Existing console native suites may be exercised for this repair under the quiet runner. Focus-taking TSF tests still await an explicit away confirmation.

New evidence: Notepad PIDs 17452 and 43960 log status-only publishes with words=0/selection=0 followed by empty=1/error=13 write failures and dozens of immediate reconnects. PID 17044 logs eligibility=0 indefinitely after previously drawing successfully. OnEndEdit explicitly hides the external surface on every ordinary text edit. Regression tests are being added to the existing native files before repairing these paths. An overnight heartbeat in this chat is active under automation id resolve-completionist-glass-flicker-overnight; it must be stopped once independent repair work is complete. Current branch: codex/glass-stability-overnight.

### Repair and verification

Two new existing-suite regressions failed before production changes: an empty status shelf selected word 0, and the render queue accepted that unencodable payload. These now pass. A closed/status-only model selects no row; the client rejects invalid frames before replacing current content and returns whether publication succeeded, so an encoding failure cannot become a reconnect loop.

Ordinary edits now mark rows stale without immediately hiding the last frame. Stale rows cannot be accepted; replies for old text are invalidated before the asynchronous context read. The first edit arms a 250 ms UI timer; repeated edits cannot extend its deadline, fresh word replies cancel it, and delayed timer messages check the current deadline. This is a UI timer, not a hard wall-clock guarantee. If no fresh words arrive, both surfaces hide. Focus loss, invalid caret, sensitive fields, dismissal and acceptance still hide explicitly.

Successful safe inspection rechecks renderer eligibility, preserving generation for the same eligible owner. Focus changes invalidate queued reads. HideAll cancels retry timers, and inspection, rendering and renderer notices check actual TSF thread focus and focused context, including same-process background windows. The host uses current mutex-protected client presentation state instead of stale posted notice state; current failures still reveal fallback, while delayed old failures cannot cover newly confirmed glass. Local presentation flags are synchronized with that state. Connection-error suppression is thread-local rather than a shared unsynchronized string.

Renderer inspection found two additional repaint faults: an ordinary desktop-duplication timeout selected opaque even when a valid captured frame remained, and every opaque repaint hid both surfaces first. The timeout now retains glass and the last valid frame; opaque redraws update visible surfaces directly. Real capture failure, foreground loss and unsupported material conditions retain their existing fallback/hide behavior.

Verification: VS2022 x64 DLL and renderer builds pass with /W4 /WX. The existing quiet runner suppresses fault dialogs and bounds test processes; 105 host tests, 12 layout tests, 23 renderer/session/accessibility tests and 7 capture tests pass, as does the existing WARP GPU Gaussian test. Engine pytest: 659 passed, 1 skipped. Glass-report unittest: 9 passed. Roadmap checker and diff whitespace check pass. Broader runs exposed existing incorrect test expectations (capture-coordinate arithmetic, font metrics constructed before changing settings, explicit-hide replay/foreground takeover, manual dock minimization and the fully opaque pulse peak); expectations were corrected to the established contracts, and the session test no longer dereferences an absent current snapshot after failure. Three unique subagents have been used across the entire process; the existing broad reviewer was reused, and all implementation here was inline.

The before-log is preserved. No fresh app has yet exercised this complete repair, and no focus-taking TSF harness has run because the owner has not explicitly confirmed being away. On-screen stability remains unverified; this task is not called resolved from builds or logic tests alone.

Installation at 23:08 local: the main registered DLL and renderer match the tested worktree SHA-256 hashes (DLL 76AA3B591C941DBBF03CCF390443AC74EADC517762586A4744D95CC9043D3B50; renderer DB9FE15A8194A0A4A6969F00CCF314B7FB9E6CC81D950BD7581FE7BC1CAA5026). The DLL was renamed aside to preserve loaded old processes. The glass before-log was copied to main tip/out/glass-before-overnight-20261005-230827.log, and the original log was not cleared. The first engine launch used the wrong working directory and exited; it was corrected to engine/ before verification. Engine PIDs 15960/23192 remain running, the engine reports ready/listening, and renderer PID 2516 runs the new build. Existing WindowsTerminal PID 29728 still has the old DLL; its reconnect after renderer restart is not evidence for the repaired DLL. The heartbeat is updated to the current branch and waits for fresh-build evidence, without killing user apps or claiming visual success.
