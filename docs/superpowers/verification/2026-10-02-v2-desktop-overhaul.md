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

Viewer M10.3 committed as `f2750b3`. The synthetic headless browser suite passed 10 checks covering stale list/detail/screenshot responses, modal keyboard/focus, clear-log cleanup, dirty-navigation confirmation, hidden polling, private/pause guards, light/dark responsive layouts, reduced motion and narrow zoom-equivalent overflow. Named viewer Python tests: 31 passed in 0.42 s. Full engine: 634 passed, 1 skipped in 26.33 s. Roadmap checker passed. Generated screenshots contain synthetic fixtures only and are retained in the ignored SDD workspace; this evidence does not establish live-user-data behavior. Independent task review is pending.

