# V2 desktop overhaul — final implementation specification

Date: 2026-10-02. Status: complete proposal for owner review; implementation is not authorized by this document alone.

This document supersedes the renderer, architecture and review-handoff sections of `2026-10-02-native-autocomplete-v2-design.md`. In particular, Acrylic is no longer the primary material, and the earlier prohibition on a rendering-only desktop capture is replaced by the explicit contract below. The browser prototype and C++ glass spike remain visual references, not production code.

## Goal and release boundary

Deliver the V2 desktop autocomplete experience: a polished, soft liquid-glass suggestion menu at the caret and a separate lightweight information dock at the bottom-left of the active typing monitor. Preserve the reliability and keyboard behavior of the existing text service. Python remains the suggestion/AI engine; rendering is native C++.

The owner's response “For V2” establishes the release target. The design decisions in this conversation cover the desktop surfaces, not a new dashboard layout. V2 includes the separately tracked viewer overhaul, but this specification defines only its integration obligations. Its Words/Stats/Requests/Settings layout needs its own design and plan before that work is executed. Completion of this plan does not by itself complete the entire 2.0.0 release.

### Included

- Native caret menu, correction comparison, truthful suggestion origins, and partial-accept underline inside the phrase.
- Actual AI idle countdown, request/streaming state and compact trigger reason.
- Separate minimizable information dock: small connection light and smaller tense field.
- Live blurred/reflected/refracted material, soft shadows, Evergreen themes, accessibility and reliable fallback.
- Separate renderer process, engine lifecycle integration, focus arbitration, IPC and packaging.

### Excluded

- Real tense classification/ranking: V2.1. V2 displays `Tense —`.
- New ranking algorithms, AI providers, spend settings, keyboard shortcuts or mouse acceptance.
- Context receipt in the dock, insertion preview, duplicate next-word preview, descriptions or extra status panels.
- Electron, React, WebView, managed WinUI, internal composition hooks, system-wide font installation.
- Desktop recording, cloud vision, OCR, or capture-based caret tracking in the renderer.

## Validated direction and remaining uncertainty

The owner verified the standalone C++ spike runs, requested softer treatment, then required genuine blur rather than pixelized sampling. The latest spike uses a true separable Gaussian convolution of an app-owned fixture, keeps foreground text sharp, and has successfully rendered light/dark/frost images. The latest blur revision has not yet received explicit visual approval.

The spike does not prove live desktop sampling, transparent composition, mixed-DPI positioning, HDR, multi-monitor behavior, focus safety or production performance. Those are acceptance gates, not assumptions. No premade library is selected: use supported Windows APIs and a small custom material pipeline; evaluate licensed source references only when they save work. Do not import a demo's widget toolkit or bundled fonts.

## Architecture

```text
Host app: C++ TSF DLL                 Python engine
  caret / focus / keyboard  <------>  suggestions / AI / settings
           |
           | async local display snapshots + acknowledgements
           v
  CompletionistRenderer.exe (one per user desktop session)
    focus lease -> layout -> GPU background -> sharp foreground
    caret HWND + information HWND
```

The DLL retains text insertion, `PopupModel`, TSF context reads and request-id validation. It sends render-ready snapshots; the renderer cannot insert text or intercept typing. Keep the existing popup HWND as the message/timer endpoint for engine replies, even while its painting is suppressed.

The renderer is a separate x64 C++20 executable with static CRT, Win32 windows, D3D11, DXGI, DirectComposition, Direct2D and DirectWrite. Graphics failure must not take down an app the user is typing in. The engine supervises its lifecycle through `renderer_process.py`, launches only a known local build/install path with no console, and stops it on engine exit. One named mutex per logon session prevents duplicate instances. Restart at most three times in 60 seconds; after that retain the host fallback until explicit engine restart. Renderer and DLL do not recursively launch each other.

Renderer failure does not stop ranking or text insertion. Until a current snapshot is acknowledged, use the in-host opaque Evergreen popup. Hide it only after the renderer has successfully presented that snapshot. If acknowledgement is lost or graphics fail, the renderer hides both windows and the host restores its fallback. A broken dock may disappear; it must never leave a stale topmost window.

## Material pipeline

1. Acquire the active output through DXGI Desktop Duplication on a dedicated graphics thread, with bounded waits that do not block input/IPC handling.
2. Retain the current output texture on the GPU. Crop two panel regions with a margin sufficient for the blur kernel and maximum lens displacement. Handle output rotation and desktop origin offsets before sampling.
3. Apply actual separable Gaussian blur: horizontal and vertical shader passes. Initial sigma **6 logical px**, radius **18 logical px**, scaled by panel DPI. Use weighted adjacent texels or equivalent bilinear-paired Gaussian taps, never nine widely spaced samples or nearest-neighbor/downsample artifacts.
4. Sample the blurred texture through a rounded-rectangle lens profile. Initial menu radius **26 logical px**, refraction strength equivalent to spike **18**, restrained RGB dispersion. Center stays readable; most visible curvature lives at the edges.
5. Blend broad specular reflections without clipping to a white outline. Apply a soft downward Gaussian-profile shadow, gentle Evergreen tint and a faint inner meniscus shade. No permanent moving sheen or pointer tracking required in production.
6. Draw foreground text, underlines, selection and controls in a separate sharp layer. Prefer grayscale text antialiasing on translucent surfaces; never apply backdrop blur or displacement to glyphs.
7. Present premultiplied-alpha composition swapchains in two non-activating windows. Clear transparent pixels correctly and include shadow margins in bounds/hit tests.

Use the latest spike for the optical profile, not its CPU blur, GDI glyph atlas, fixture strings, fixed canvas or render-every-frame loop. Shader resources are embedded/compiled as part of the renderer build; runtime must not depend on a source `.hlsl` file sitting beside the executable.

The renderer performs **local GPU desktop sampling for visual material only**. Pixels are not saved, logged, sent to the engine, OCR, viewer or AI, or copied to CPU in production. Desktop Duplication exposes an output-sized image even when only small crops are rendered; do not describe this as an API that captures only a window-sized region. Release capture/textures when both surfaces are hidden, on pause, blocked/sensitive context, lock or desktop switch.

Exclude both renderer HWNDs from its own capture using supported `WDA_EXCLUDEFROMCAPTURE`. If exclusion fails, do not use live refraction; otherwise self-feedback can corrupt the material. The glass windows may be absent in external screenshots/recordings. This is a documented tradeoff to review before shipment, not a promise of seamless capture visibility. Protected content and secure desktop are not bypassed.

On HDR or unsupported display/session/capture/device conditions, use the opaque fallback rather than washed-out colors or frozen desktop images. Correct HDR is not required for the first implementation. Acrylic may be evaluated as an intermediate fallback, but opaque Evergreen is the required dependable one. High contrast always uses system colors with no capture, blur or motion. Disabled Windows transparency disables capture and uses opaque material.

### Event-driven work and targets

No repaint loop for a static desktop and unchanged UI. Acquire with bounded waits only while visible; redraw on relevant backdrop dirtiness, panel geometry, text/selection/status changes or short active animations. Nonintersecting dirty/move regions do not cause blur/presentation work; handle move regions correctly. Initial full-output copy is acceptable, repeated unnecessary full copies are not.

Planning targets, to measure rather than claim: foreground paint within **32 ms p95** of receiving a snapshot on the development PC; GPU glass passes within **4 ms p95** for two normal panels; static visible state has no self-sustaining capture/repaint loop. If these cannot be met, simplify shader/copy work before adding dependencies. Capture heartbeat and animations must stop when hidden.

## Layout and content

Use exact Evergreen roles from `CONTEXT.md`, canonical `docs/roadmap/theme.css`, and viewer theme. Pine is the brand; magenta denotes selection/activity. No new colors. Native fonts: **Segoe UI** for content, **Consolas** for shortcuts. No webfont/network dependency.

| Element | Initial logical-pixel rule |
|---|---|
| Menu width | 330, honoring configured width multiplier and monitor bounds |
| Inner padding | 14–16 |
| Candidate text | Configured font size; origin 10–11 |
| Word rows | Measured font metrics + 12 vertical padding |
| Corner radius | 26, capped for small panels |
| AI shelf | At least 76 while scheduled/in flight; grows to fit measured two-line content |
| Info dock | 190 wide, 20 inset from work-area bottom-left |
| Tense | 9 label / 11 value; dash until V2.1 |
| Connection dot | 6 diameter, accessible status name |

Word-only sessions have no empty AI shelf. Reserve pending shelf space when a request is scheduled so selectable targets do not jump as chunks arrive. Status-only content may show a shelf, but must not make `PopupModel` selectable or consume Tab.

Words have `Local` or `Learned` origin, and phrases `AI`. `Learned` means an actual personally promoted out-of-base-vocabulary candidate; a boosted dictionary word stays `Local`. Missing/invalid origin metadata omits the label instead of fabricating provenance. Deduplication preserves candidate identity and origin.

Show correction comparison only for the relevant selected correction: original typed fragment -> accepted replacement, with guessed letters dotted in the candidate. Preserve full original replacement metadata for insertion. Phrase underlining marks exactly what the existing configured partial-accept action inserts; wrapping, ellipsis and Unicode must not change acceptance. No separate next-word or insertion preview.

The AI shelf owns countdown/status. Scheduled: `0.4s until AI`; working: `Working · 0.8s`; streaming: the actual partial text and compact indicator; ready: actual continuation; manual: concise configured hotkey hint; unavailable: concise status. `Working` measures request elapsed time, not model reasoning. Trigger reason is a short label/tooltip (`Idle pause`, `Manual shortcut`, `Paused`, `Unavailable`), not explanatory paragraphs.

The dock shows only local engine connection and tense. Connection does not claim cloud health. Filled dot gently varies opacity on a **2.4 s** cycle when connected; offline is an unfilled ring with a distinct accessible name. Tense classification is not simulated in production.

## Motion and interaction

Candidate menu is click-through and never takes focus; keyboard navigation remains instant. Only the dock minimize control is mouse-interactive. `WM_MOUSEACTIVATE` returns `MA_NOACTIVATE`; clicking it must preserve the host caret/selection and cause no activation.

Dock collapse: one persistent shell; body opacity **180 ms** and vertical translation **220 ms**, ease-out `cubic-bezier(0.22,1,0.36,1)`. Collapse upward from the lower anchor using clipping/translation, not independent expanding windows. Restore reverses from current progress; interruption is continuous. Hidden details do not accept input or appear in the accessibility tree. Collapse button remains available and exposes name/expanded state.

Auto-minimize when the dock overlaps the caret menu or cannot fit its expanded body. Auto-restore when space returns unless the user manually minimized it. Manual preference lasts for the renderer session; do not add new persisted settings for this first version. Automatic geometry changes and all keyboard actions are immediate. Theme/high-contrast/DPI changes are immediate. Respect Windows reduced-motion preference; stop opacity pulses and collapse animations.

## Focus, coordinates and lifetime

Use physical virtual-desktop pixels at the DLL/renderer boundary; use DIPs inside layout. Normalize TSF coordinates according to the host's DPI-awareness context, using supported logical/physical conversion where needed. Do not blindly multiply all coordinates by DPI. The renderer uses Per-Monitor V2 awareness and recalculates fonts, blur and dimensions when crossing monitors.

Only one eligible foreground text-service session owns both surfaces. A snapshot includes host PID/HWND, random client session id, focus generation, monotonically increasing display revision and actual caret rectangle. Validate foreground owner before granting a lease. Deny stale generations/revisions. Focus loss, pause, disallowed/sensitive scope, empty usable content and disconnect produce an explicit hide; do not wait for timeout in normal operation.

Active focus lease: heartbeat every **500 ms**, expires after **1500 ms** without renewal. The renderer also checks foreground ownership on WinEvent foreground changes. Heartbeats never revive a revoked generation. Host process exit, pipe disconnect, renderer shutdown or desktop switch hides both surfaces and clears cached content. Visibility must not depend solely on a periodic foreground poll.

Place menu below caret, flip above when needed, keep at least **6 logical px** clearance and clamp within the caret monitor's work area. Dock anchors to that same work area above the taskbar; both surfaces migrate together. Negative monitor coordinates, 100/150/200% scaling, portrait rotation and changed taskbar work areas are supported. If available area is too small, clamp width, clip/ellipsis visible text and minimize dock; never extend into another monitor or obscure the caret.

## Wire contracts

Existing engine framing remains 4-byte little-endian length + UTF-8 JSON, **1 MiB** cap. Add optional fields to existing words/phrase messages:

```json
{"id":42,"phrase_state":"scheduled","phrase_wait_ms":350,
 "phrase_elapsed_ms":0,"trigger_reason":"idle","origins":["local","learned"]}
```

State values: `off`, `manual`, `scheduled`, `working`, `streaming`, `ready`, `unavailable`. Waiting time comes from the real scheduler's due time. Renderer estimates decreasing wait from local monotonic receipt time, clamps to zero, and never invents a provider request at zero. Only an engine transition starts `working`. Unknown optional fields are ignored; unknown state maps to unavailable/legacy behavior, not a crash. Numeric fields are finite integer durations 0–600000; invalid values are discarded. Missing fields retain old-client behavior. Engine status updates use the same current request id and cancellation rules as content.

New render pipe: `\\.\pipe\completionist-renderer-v2-<logon-session-id>`. Same length framing/1 MiB limit, separate schema **version 1**. Commands: `show`, `hide`, `heartbeat`; response: `presented` or `unavailable`. `show` is a full snapshot, not a patch: identity/lease metadata, physical caret rectangle, display revision, candidates with text/origin/marks, selected row, phrase/partial-accept range, AI status/durations/reason, local-engine connection and popup settings. No API key, surrounding full document or captured pixels in this channel.

Renderer snapshots are presentation only: TSF keys always use the DLL's current model. Out-of-order replies and old `presented` acknowledgements cannot dismiss newer host fallback. Host suppresses fallback only for an acknowledgement matching current session/generation/revision; restores it on unavailable or lost connection. A **250 ms** unacknowledged show deadline triggers fallback, without blocking input. Renderer confirms successful present rather than merely queue receipt.

Both pipe ends validate peer identity against current user's SID and logon session; renderer also matches named-pipe client PID to declared host PID and foreground HWND owner. Use same-user/session ACL and reject remote clients. Decode malformed frames with bounded memory, reject oversize before allocation, close offending connection, and log only error category/id, never suggestion strings. Coalesce unsent show snapshots to newest revision, preserve hide priority, cap queue at **32** entries. IPC stays off host UI thread.

## Accessibility and failure behavior

Provide read-only UI Automation elements for visible candidates, selected state, AI status, connection name and tense, plus the dock toggle's expanded state/action. No repeated live announcements for every countdown tick. Visible row ordering must match keyboard order even though the AI shelf is spatially separate.

Fallback keeps the same text, selection, origins, compact status and layout using opaque Evergreen/system surfaces. Keep a host GDI fallback for renderer outage and a renderer opaque path for capture failure. No stale frozen screen underneath words. Device removal clears capture and tries recreation once; repeated failure stays opaque for the session. Locked/secure desktop hides rather than showing cached content. Unsupported HDR remains opaque until a separately validated color pipeline exists.

## Verification and release gates

1. Pure C++ tests: protocol/lease/out-of-order data, geometry and DIP conversion rules, high-contrast/material selection, partial-accept ranges and the existing complete `PopupModel` key contract.
2. Python behavior tests with fake clocks/providers: actual lifecycle, wait reset/cancel, manual/off modes, provider failure, settings/privacy changes, stale updates and truthful origins.
3. Offscreen renderer fixture tests: Gaussian point/edge response, premultiplied alpha, sharp glyphs, themes and software-rendered fallback. Export uses fixtures only; never automatic desktop screenshots.
4. Native capture feasibility: actual desktop behind two windows, exclusion/no feedback, focus preservation, rotation/DPI, dirty-region inactivity and failure cleanup. Owner performs visual app checks; do not run focus-stealing harness while they work.
5. Manual matrix: Notepad, Chrome and one Electron app; 100/150/200%; both monitors/all edges; light/dark; long phrases/corrections; manual collapse and interruption; high contrast/reduced motion; engine/renderer kill/restart; lock/unlock and an unsupported session fallback.
6. Measure the stated paint/GPU/inactivity targets on the development machine. Record hardware and results; do not claim broad compatibility from one PC.
7. Build review artifacts before installation. Preserve last working DLL/renderer for rollback, keep registration/UAC and system keyboard changes user-run, explain host restart. Every implementation commit updates roadmap and passes its checker. Do not mark all of V2 released until the viewer track and release checks also finish.

## Sources and unresolved acceptance decisions

Supported capture surface/dirty regions/rotation: [Microsoft Desktop Duplication](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api). Capture exclusion behavior/build constraints: [Microsoft SetWindowDisplayAffinity](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity). Require Windows 10 **2004 / build 19041** or later for live glass; older environments retain opaque UI.

Before production execution the owner reviews this spec and plan, including local rendering-only capture and its screenshot tradeoff. First live-material milestone must demonstrate these contracts before the host switches to external rendering. The latest static blur pass is a reference awaiting visual confirmation; it does not waive that milestone.
