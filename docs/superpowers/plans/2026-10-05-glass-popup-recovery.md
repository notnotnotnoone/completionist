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
