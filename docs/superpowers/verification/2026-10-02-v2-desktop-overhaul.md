# V2 overhaul verification

Work branch: `codex/v2-overhaul`. Date: 2026-10-02.

This record separates automated implementation checks from live Windows acceptance. No V2 release, installation, registration or external-renderer default enablement is implied.

## Baseline

Before product changes:

- `cmd /c tip\test.cmd`: 77 tests, 0 failed.
- `.\engine\.venv\Scripts\python.exe -m pytest engine/tests --basetemp=.superpowers/pytest-baseline -p no:cacheprovider -q`: 626 passed, 1 skipped in 31.33s.
- `python scripts/check_roadmap.py`: roadmap ok.

The initial `uv run --project engine pytest engine/tests` was blocked from accessing the existing user-level uv cache. The initial direct pytest invocation produced temporary/cache-directory permission errors. Re-running with the existing project environment, workspace temporary files and disabled pytest cache passed cleanly; these failures were environment failures, not product test regressions.

## Live acceptance remains required

The standalone material stage requires owner-run confirmation of actual desktop blur/refraction, two windows, focus retention, exclusion without feedback, mixed DPI/rotation and failure cleanup. Generated fixtures cannot establish these facts.

Later integration requires Notepad, Chrome and an Electron app at 100/150/200%, both monitors and edges; light/dark and accessibility preferences; real AI lifecycle; correction and partial acceptance; dock minimization; engine/renderer exit and recovery; lock/unlock; unsupported/HDR opaque fallback. The focus-stealing TSF harness must wait until the owner explicitly says they are away.

GPU 4ms p95, foreground 32ms p95 and static-desktop idle behavior are unmeasured until actual runtime evidence is recorded. No automatic desktop capture or user-config inspection has been performed.

## Packaging and rollback

Do not register an unverified DLL. Preserve the last working DLL and renderer as a pair before installation. Production `tip/build.cmd` currently renames the existing DLL, so review builds must avoid replacing an installed artifact inadvertently. Specific staged paths and rollback commands will be added when a validated pair exists.

Full 2.0.0 readiness also requires the independently tracked viewer overhaul and its verification.

## Renderer foundations and interrupted runtime checks

Task 1a implements a separable GPU Gaussian shader. WARP generated-content assertions compare actual shader output to the specified sigma-6/radius-18 impulse and step profiles and check constant-field preservation. A temporary wrong-kernel mutation failed the impulse assertion; restoring the kernel passed `tip/renderer/test.cmd`. This proves the generated WARP shader profile, not hardware timing or live glass.

Task 1b contains partial composition surfaces and a generated PNG exporter. Three exporter launches faulted during cleanup and raised Windows access-violation dialogs. The owner reported that these interrupted other work. Native executable/debugger testing was stopped immediately; the owned renderer processes were terminated, and a filtered process inspection found no remaining renderer, test, fixture or WER processes.

Both native executable entrypoints now set process-local error mode and WER no-UI flags before graphics initialization and fail closed if setup fails. Global Windows settings were not changed. The exporter COM lifetime was corrected from source. Independent source review also identified an incorrect WIC row stride; `cbf655e` fixes the mapped row pitch versus total buffer size, checks dimensions/overflow, and releases WIC objects before unmapping. Compile-time checks exercise the production layout helper's valid and invalid cases.

After the interruption, only `tip/renderer/build.cmd` and `tip/renderer/test.cmd --compile-only` were run; both passed `/W4 /WX /MT`. The dialog guard and WIC memory-layout fix received independent source reviews. No produced executable was run after these changes. The PNG export and crash correction remain runtime-unverified, and the lens shader, live capture worker, dirty/output geometry and failure policies remain incomplete. M10.14 stays doing. Native runtime checks must wait until they can be run without interrupting the owner.

## Engine metadata implementation

Task 2 source/Python scope reviewed through `39b1cb8`: truthful scheduler lifecycle, current provider identity, optional wire status/durations and aligned local/learned origins. Full engine suite: 633 passed, 1 skipped in 18.46 s, exit 0. Focused final coverage: 131 passed; request-start timing fix: 41 passed in 6.35 s. Independent scoped re-review found the delayed working push addressed and no new Critical/Important findings. Native protocol tests compiled under /W4 /WX; their assertions remain runtime-unverified while native executions are stopped. M10.15 remains doing until that gate passes.

## Viewer automated implementation

Viewer M10.3 committed as `f2750b3`, with its reviewed Clear Log race fixed in `ca0f68a`. The retained synthetic headless browser suite now passes 12 checks, including delayed clear ordering and failed-clear recovery. It covers stale list/detail/screenshot responses, modal keyboard/focus, clear-log cleanup, dirty-navigation confirmation, hidden polling, private/pause guards, light/dark responsive layouts, reduced motion and narrow zoom-equivalent overflow. Named viewer Python tests: 31 passed in 0.42 s. Full engine before the bounded JavaScript-only fix: 634 passed, 1 skipped in 26.33 s. Roadmap checker passed. Independent re-review found the clear race addressed with no new Critical or Important findings. Generated screenshots contain synthetic fixtures only and are retained in the ignored SDD workspace; this evidence does not establish live-user-data behavior.

## Renderer protocol and focus leases

Task 3 source and compile-only scope was reviewed through `442a774`. It defines shared snapshots and current acknowledgements, exact uint64 numeric decoding, bounded JSON framing, pure owner/revision/generation lease state, and inert Windows same-user/logon-session pipe and foreground validators with revocation hooks. Two reviewed parser corrections preserve valid legacy engine words when optional or unknown metadata exceeds container bounds, including required fields following many unknown root fields. Both host and renderer compile-only targets passed initially; covering host compilation passed after each parser fix. Native assertions, actual Win32 identity/ACL/hook behavior and the prerequisite live-material gate remain UNRUN. M10.16 remains doing. Encoder string/settings validation parity is recorded as a deferred Minor finding for final review; external rendering and capture remain disabled.

## Native layout and sharp text

Task 4 source and compile-only scope was reviewed through `28d5889`. Pure layout consumes physical caret/work-area coordinates and measured DirectWrite content; it preserves configured point sizes, menu/dock placement and insertion strings. The shared text path includes origins, correction marks and UTF-16 partial acceptance. A reviewed fix applies the underline hit-test origin once, with an authored nonzero-origin regression. The generated fixture matrix includes 96/144/192 DPI, both themes and the maximum 24-point font, using the same preparation, placement and drawing path. Both existing host regression and new layout test targets compiled, renderer compilation passed, and all 30 palette roles matched canonical theme values. Scoped re-review found both Important findings addressed and no new breakage. Native assertions, generated images and visual/DPI checks remain UNRUN; M10.17 remains doing. Palette-header placement is a deferred Minor for final review.

## Standalone full material source

Task1b full source scope was reviewed through `dd9e52d`. The embedded DPI-scaled Gaussian/lens pipeline is shared by generated fixtures and the opt-in controlled live fixture. The graphics worker selects the monitor adapter/output, retains GPU textures, applies move/dirty updates, handles rotated crops and uses the shared Session/Layout/TextRenderer path with a labelled generated caret. Hidden and failure transitions use a shared retirement function for captured and derived resources. Reviewed fixes make fallback fully opaque, remove fixture overpaint, guard the 19041 minimum before live setup and exercise the worker retirement helper with initialized owner-shaped resource probes. The covering compile-only suite passed for embedded shaders, renderer, existing blur/session targets and new capture-policy tests; scoped re-review found all four Important findings addressed and no new breakage. No executable, fixture export, live window or capture was launched. Real DXGI/D2D behavior, visual quality, focus/exclusion/rotation/device failure and performance remain UNRUN. No-argument startup remains inert and M10.14 stays doing until original live acceptance passes.

## Renderer service, dock and accessibility

Task 5 source was reviewed once at `7bb435c`; its three Important findings were fixed in `81741fc` with targeted compilation and implementer self-review, following the owner's single-review instruction. The explicit `--serve` path connects validated named-pipe commands to the actual renderer thread, surfaces, text/material pipeline and current-draw acknowledgements. It includes owner-specific revocation, opaque fallback, bounded device recreation, interruptible dock motion and UIA providers. Fixes query the selected output color space and fail opaque for HDR/unknown modes, wire the visible connected pulse into production drawing/frame scheduling, and complete dock motion on theme/DPI/display changes.

Both host and renderer compile-only targets passed for the initial implementation; the covering renderer compile-only target passed after the fixes. Diff and roadmap checks passed. Authored assertions exercise production color-space classification, frame scheduling, system-change handling and provider/state helpers. These native assertions were compiled, not executed. Actual pipe contention, focus behavior, UIA, capture/device recovery, visual quality and real Windows acceptance remain UNRUN. No native executable, fixture, window, capture or debugger was launched; no-argument and external host activation remain gated. M10.18 stays doing pending its runtime gates.
## Matching host fallback and connection notifications

Task 6 source received one review at `6b62553`; three Important findings were fixed in `e41a4eb` with covering compilation and implementer self-review. The host now consumes a shared data-only palette and GDI measurements with shared placement, draws origins/correction/partial/status content and high-contrast system colors, and handles engine connection transitions on the UI thread. Fixes serialize observer posting with unregister, reject queued stale registration tokens, show connection health independently of AI state and reserve independently measured correction context. PopupModel remains the keyboard/selection authority; external rendering remains disabled.

Host regression/layout targets compiled without execution. An alternate review build produced `tip/out/task6-review/CompletionistTip.dll`; the owner's existing DLL was preserved. Targeted checks passed after the fixes, alongside diff and roadmap checks. Native assertions, observer teardown, real-app keyboard behavior, mixed-DPI placement, connection repaint and visual matching remain UNRUN. M10.19 stays doing pending runtime acceptance. The palette-header deferred finding is resolved by host reuse at `tip/src/popup_palette.h`.
