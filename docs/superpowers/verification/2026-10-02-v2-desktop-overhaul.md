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
