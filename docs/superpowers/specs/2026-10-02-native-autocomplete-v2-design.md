# V2 native autocomplete — production design for review

Superseded for implementation by [V2 desktop overhaul specification](2026-10-02-v2-desktop-overhaul.md) and its [implementation plan](../plans/2026-10-02-v2-desktop-overhaul.md). This file is design history; its Acrylic recommendation and no-capture restriction are not the current proposal.

Status: proposed production design; implementation has not started.
Date: 2026-10-02.
Reference: the reviewed `tip/prototype/index.html` and latest compact revision of `2026-10-02-v2-autocomplete-design.md`.

## Outcome

Replace the existing native blue/amber rectangular popup with the reviewed compact Evergreen design. Keep suggestions at the caret and put a separate, minimizable information dock at the bottom-left of the active typing monitor's work area, above its taskbar. Countdown and AI activity belong inside the caret menu. The information dock contains only a small connection light and a small collapsible tense row. No descriptions, context receipt, insertion preview or duplicate next-word display.

Production must use real engine state and origin metadata. No fixture timers, fake connection states or simulated tense classifications ship. Tense detection remains V2.1; V2 shows `Tense —` until the engine can supply a classification. The dash means unavailable, not uncertain detection.

The release is the native popup component of V2, not a declaration that the entire 2.0.0 roadmap is complete. Dashboard work remains separate.

## Material decision requiring review

Update after owner review: plain Acrylic was rejected as too simple. The proposed direction is now a custom lens-like material, with clearer center, rounded thick-looking rim, paired specular highlights, Evergreen tint and separate sharp foreground text. A disposable native material probe must establish what can be achieved before selecting the production renderer. The earlier Acrylic recommendation below is superseded; it is only a fallback candidate, not the accepted target.

Direct2D supports displacement of an input image, but that alone does not provide live pixels behind another app's window. Win2D's DisplacementMapEffect is explicitly unsupported by Windows.UI.Composition. Distinguish a custom glass approximation made from transparency/highlights from genuine live-backdrop refraction, which requires its own validated image-source/rendering route. Do not promise arbitrary backdrop displacement through an ordinary composition effect brush.

### Premade renderer research (2026-10-02)

Research shortlist; none has been installed, built or visually/performance-tested here. These findings do not approve a renderer or change the implementation contract below.

- **electron-liquid-glass**: MIT, packaged Node N-API addon with a prebuilt Windows x64 binary. Its native panel uses DXGI Desktop Duplication, D3D11 and DirectComposition for live desktop refraction and chromatic dispersion, with non-activating click-through windows and adaptive contrast sampling. This is the closest packaged candidate found, but its public integration targets Electron, which Completionist does not use. Performance/energy claims are the author's, not independently verified. Capture exclusion and a capture session following the first visible panel need evaluation for two panels and multiple monitors. Source: https://github.com/hicccc77/electron-liquid-glass
- **liquidDX11**: native Win32/C++/D3D11 desktop-refraction source, including rim/specular lighting, blur and adjustable shader parameters. Original app code is MIT; bundled dependencies retain separate licenses. Best candidate to evaluate for native source reuse, but the author explicitly calls it a source release rather than a packaged SDK. The shipped showcase is x64-only; desktop duplication can fail in remote/unsupported sessions. Source: https://github.com/poncippg-spec/liquidDX11
- **LiquidGlassWinUI**: a packaged .NET 8 / WinUI 3 brush with custom shaders. Its documentation requires exactly Windows App SDK 2.2.0 because it hooks internal composition symbols. This dependency is too brittle to recommend for the TSF DLL; cross-app desktop refraction is also not established by its example. Source: https://github.com/luckyelysia/LiquidGlassWinUI

Recommendation: evaluate liquidDX11's material in a disposable separate native process first; retain electron-liquid-glass as the packaged alternative if a separate Electron UI is deliberately chosen. Neither should be loaded wholesale into every TSF host. Actual live-desktop refraction in the first two candidates requires local GPU desktop capture; this is a different architecture from the original no-capture Acrylic proposal. A selected approach needs an updated design, device/session failure fallback, capture-policy review and measurements before production integration. No mature, verified drop-in C++ liquid-glass SDK was established by this search.

Additional references:
- https://learn.microsoft.com/en-us/windows/win32/direct2d/displacement-map
- https://microsoft.github.io/Win2D/WinUI3/html/T_Microsoft_Graphics_Canvas_Effects_DisplacementMapEffect.htm

The recommended initial native material is supported Windows **Desktop Acrylic**, with Evergreen foreground colors, translucent tint where supported, rounded edges and restrained specular highlights. This is visibly glass-like, but does **not** reproduce the browser prototype's lens displacement. Exact refractive liquid glass would require a separate custom composition/rendering design; it must not be claimed as delivered by an Acrylic backdrop.

Use `DwmSetWindowAttribute` with `DWMWA_SYSTEMBACKDROP_TYPE` / `DWMSBT_TRANSIENTWINDOW` where available. Microsoft's documentation identifies this as Desktop Acrylic on Windows 11 and gives a minimum supported build of 22621. The inspected development machine reports build 26200. API success alone does not prove visible material on a non-activating popup: verify the actual windows before accepting this rendering route.

Sources:
- https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type
- https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute

If a backdrop is unavailable, disabled, fails, or is incompatible with the popup's non-activation behavior, use an opaque Evergreen surface with the same content and sizing. High contrast uses system colors and no transparency. Do not introduce undocumented composition attributes or make an embedded browser run inside every host app. Do not capture the screen to manufacture a refracted backdrop in this implementation.

The native implementation should use Direct2D/DirectWrite for sharp text and alpha-aware foreground rendering over the DWM material. Keep a functioning GDI fallback for drawing initialization or device failures. Validate a disposable window first; do not rewrite the whole popup around an unverified material path. The fallback is part of the implementation, not a promise inferred from catching exceptions.

## Visual contract

Use the exact light/dark Evergreen roles already present in `CONTEXT.md`, `docs/roadmap/theme.css` and the real viewer. The native palette explicitly mirrors those values and has a parity check against the canonical tokens. Pine green is the brand role; magenta denotes selection/in-progress. Correction characters use dotted underlines and a compact correction comparison, not the old amber palette.

Menu default width: 330 logical px; information dock: 190 logical px. Honor the configured font size and width multiplier. The AI area reserves sufficient space for its two-line continuation when a request is genuinely scheduled/in flight; word-only sessions have no empty AI area. The reviewed browser default was 76 logical px, but native layout derives the required height from measured font metrics so larger configured text does not clip. End long text with an ellipsis; accept inserts the original full text.

Native UI text uses the viewer's Segoe UI system stack for the initial port, with monospace shortcuts. Do not introduce a network font dependency or install fonts system-wide. Keep the prototype's type hierarchy and spacing; font metrics will differ from its optional Overpass web font and require native review.

The information dock has no heading, help paragraphs or connection description. Connected is a small filled light with a slow, subtle brightness cycle. Disconnected is an unfilled light; give it an accessible status label so status is not conveyed by color alone. Its connection means the local engine pipe, not an assertion that the cloud provider is healthy. Provider unavailability appears as a short state in the AI section.

The dock's single toggle hides/restores only the tense row, retaining the brand/light/toggle strip. Default expanded; retain the user's choice in memory for the active session. No new settings page or persistence option is needed for this port. Animation uses opacity and a slight visual translation, about 180–220 ms, without moving the caret menu. Respect Windows animation preferences; keyboard actions and caret-menu selection are instant. Stop animation timers when the surfaces are hidden or destroyed.

## Real request status

Extend the existing length-prefixed JSON protocol with optional status data, keyed by the same request id as the word/phrase response. Older clients ignore the new fields; newer clients accept responses without them and do not invent countdowns.

Proposed status contract:

- `phrase_state`: `off`, `manual`, `scheduled`, `working`, `streaming`, `ready`, or `unavailable`.
- `phrase_wait_ms`: non-negative remaining idle delay, meaningful only for `scheduled`; calculated from `PhraseScheduler.next_due()` and the engine's monotonic clock at message creation.
- `origins`: one label per word, `local` or `learned`; missing/invalid arrays safely default to `local`.

The scheduler remains authoritative. Carry status in word replies and phrase pushes, including the working transition before the first provider text and a terminal transition after failures/cancellation. Do not infer `working` from an empty phrase, which can also mean off, manual, cancelled or failed.

The client maintains a local display deadline from the received remaining duration, redraws at most ten times per second while scheduled, and displays tenths of a second. Reaching zero does not start a request or declare a response ready; the engine's next status owns that transition. A short label such as `Waiting` may be used at zero while a status update is in transit. Never compare clock epochs across processes. A queued message may make the displayed idle deadline approximate; this is a countdown display, not a new scheduling authority.

When the request is actually working, show `Working` and optionally elapsed local time since that status arrived. Streaming shows `Receiving`; a complete available phrase shows `Ready`. No invented percent complete and no claim to display internal reasoning. In manual mode show a compact configured phrase-request shortcut. Off mode has no AI area. Provider failure/unavailability keeps available local words usable.

Typing/caret changes invalidate the old status together with the old reply. Explicit dismissal cancels the display and the existing engine request behavior. Matching typed continuation retains the scheduler's existing reuse/trim behavior. Changes to settings or privacy must clear incompatible pending status immediately.

## Honest origins and corrections

AI continuations always carry the `AI` origin label. Local word candidates default to `Local`. Mark `Learned` only for vocabulary introduced from the personal store outside the base dictionary and admitted under existing promotion rules; do not label every personally boosted dictionary word as having a solely learned origin. Context and personal ranking remain unchanged. Maintain word/origin alignment through casing, deduplication and truncation, including next-word/chunk paths. Missing origin data falls back to Local without changing acceptance.

Correction comparison uses the original typed fragment and the highlighted corrected word. Guessed character marks still come from the engine. Underline the next phrase segment that the configured partial-accept shortcut would take; there is no second preview elsewhere. Use the existing partial-accept behavior as authority, rather than a new tokenization rule in drawing code.

## Window ownership, focus and monitor behavior

The caret surface remains a topmost, non-activating, click-through popup. The information surface is a separate non-activating tool window; only its minimize/restore control handles clicks. It must not steal the host editor's focus or cause a text-context change when clicked.

Keep both windows owned by the currently active text-service instance. Hide the information window on deactivation, shutdown, focus loss, sensitive/blocked fields and quiet/ineligible contexts. Two host apps must not leave duplicate information panels behind during focus handoff. Do not display the previous app's state after a new context activates.

Anchor the dock to the monitor containing the active caret and clamp to that monitor's work area. Update its DPI before layout. Preserve the minimized preference when moving between monitors. Hide or minimize expanded detail when necessary to avoid obscuring the caret/menu. Choose placement before drawing and do not reflow word targets every time a phrase chunk arrives. Reserve pending AI height before words become selectable.

Connection is read from the shared engine client and updated through worker-to-UI notifications. Pipe operations stay on the existing worker thread. Hidden contexts do not start display timers just to poll connection. Only the active eligible instance shows the panel. A disconnected client shows no stale countdown and never consumes Tab for stale words.

## Implementation boundaries

- Engine: add optional request-status serialization in `protocol.py`, scheduler/session status in `phrase_scheduler.py` / `phrases.py`, and truthful candidate origins in `words.py` and their mapping in `engine.py`. The named-pipe server forwards existing messages and must not grow a second state machine.
- Wire reader: extend `tip/src/protocol.h` / `protocol.cpp` with bounded optional fields and safe defaults.
- Service: `tsf_service.cpp` owns request-id validation and connects active focus, content, pipe state and display status. It retains the existing keyboard model as authority.
- Drawing: separate pure logical layout/palette/status calculations from Windows rendering. Keep `popup.cpp` focused on window/drawing integration; add an information-window unit and shared material helper instead of turning it into a second TSF service.
- Engine client: deliver connection transitions without blocking or accessing UI from its worker thread.
- Build: link only Windows rendering libraries needed by the selected route. Keep x64 static CRT and the CLI-only VS Build Tools workflow. No React/Tailwind/WebView dependency in the DLL.

No API key/config-secret inspection, provider-spend changes, ranking changes, system-setting changes or dashboard redesign are part of this port.

## Acceptance and rollout

Engine behavior tests must cover automatic/manual/off eligibility, real scheduled/working/streaming/terminal states, stale chunks, cancellation, failures, config/privacy changes, and truthful origins through deduplication. C++ protocol tests cover absent/new/invalid optional fields, mismatched arrays, bounded numeric durations and stale reply ids. Pure layout tests cover font sizes, scaling, monitor edges, pending/phrase-only rows and compact dock geometry.

Existing popup-model tests must still prove Enter passthrough, after-space unselected Tab, 150 ms phrase guard, explicit-selection persistence, configured shortcuts, stale keys and partial acceptance. Status-only pending rows are not selectable and must not arm Tab. Fault tests exercise DWM/renderer failure and fallback, timer teardown and disconnect/reconnect transitions.

Build the DLL to a reviewable artifact before replacing the installed one. Run non-interactive native/engine checks without stealing focus. The focus-stealing TSF harness runs only when the owner says they are away. Hands-on validation in Notepad, Chrome and Electron at 100/150/200%, on both monitors and all edges, is required before calling the native material visually verified. Verify no focus theft or duplicate dock, and check both themes, high contrast and reduced motion.

Do not claim browser tests prove the native result. Preserve the previous DLL for rollback; explain the app restart needed to pick up the new DLL. Any UAC registration step is user-approved. Do not push to main or declare the full 2.0.0 release without the project's release requirements and owner approval.

## Review handoff

The owner has authorized moving the prototype into production. The remaining design review is whether **native Acrylic with an opaque Evergreen fallback** is an acceptable first production interpretation, rather than exact lens refraction. If exact refraction is required, revise the renderer design before implementation. Once this written production design is approved, write the detailed implementation plan and review its execution method under the requested brainstorming workflow.
