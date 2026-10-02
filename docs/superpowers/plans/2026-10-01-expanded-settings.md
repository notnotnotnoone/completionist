# Expanded Settings Implementation Plan

> User approved all 30 settings. Integration and verification completed by the primary agent after the user requested no further delegation.

**Goal:** Add all 30 approved settings with working behavior and grouped collapsible Advanced tiles.

**Architecture:** Extend existing frozen configuration, settings API, phrase service and optional reply fields. Keep native UI changes backward compatible and keep temporary privacy state in the engine.

**Tech Stack:** Python 3.12, pytest, inline HTML/JS, Windows TSF C++.

**Spec:** ../specs/2026-10-01-expanded-settings-design.md

## Global Constraints

- Keep defaults, API-key secrecy, Evergreen tokens and protected fields.
- Do not run the TSF focus harness or change Windows settings.
- Keep all changes on codex/expanded-settings; no push or merge.
- All new config numbers must be finite, bounded where specified, and reject booleans.

## Task 1: Config, persistence and routing

Own config.py, settings.py, phrase_provider.py and their tests. Implement the exact dataclasses and fields in the spec. Public helpers: effective_config(config: Config, app: str) -> Config applies profiles; phrase_instructions(config: PhraseConfig) -> str is implemented in task 4. Config fields partial_accept_hotkey, dismiss_hotkey, private_mode, pause_minutes, popup: PopupConfig, app_profiles: tuple[AppProfile,...]. PopupConfig and AppProfile are frozen. WordReply popup wire values use font_size, width_scale, partial_accept, dismiss.

- [x] Write tests proving valid settings round-trip, invalid numbers/enums/profiles leave original file untouched, API key stays hidden, routing sends exact provider object and explicit sort excludes order.
- [x] Run new tests and observe failures.
- [x] Implement schemas, validation, profile application, settings serialization and routing payload.
- [x] Run config/settings/provider suites and self-review.

## Task 2: Native popup and wire protocol

Own tip sources/native tests and Python protocol.py. Add WordReply.popup: dict[str, Any] | None default None; omit when None. Native parser validates optional font_size, width_scale, partial_accept, dismiss; malformed optional settings retain safe defaults. Use settings in popup drawing and key routing, retain all existing key actions.

- [x] Add protocol/key tests for settings, malformed optional values and alternate partial/dismiss shortcuts; observe failures.
- [x] Implement optional wire settings, scale drawing, dynamic shortcut matching.
- [x] Run native unit tests/build only; no loaded-DLL rebuild or TSF harness.

## Task 3: Settings tiles and app-profile editor

Own viewer.html and a dedicated frontend test file. Consume settings contract from spec/task 1. Runtime endpoints supplied by task 4: GET /api/runtime -> paused/private_mode/preview; POST /api/runtime with action pause/resume/private_on/private_off, optional minutes; GET /api/context -> available/preview (id,app,title,prompt); POST /api/context with action send/cancel,id.

- [x] Add meaningful frontend checks for controls, select/profile round-trip, collapsed-state retention, nested error disclosure and preview Send/Cancel.
- [x] Implement semantic details tiles, select controls, app-profile row editor, runtime privacy/pause and exact-context preview.
- [x] Verify JS syntax and controls align with read_settings fields; preserve current save/discard behavior.

## Task 4: Runtime behavior and privacy integration

Own phrases.py, engine.py, policy.py, app.py, request_log.py, words.py, screen_context.py, new accessible-context module, viewer.py, runtime tests and docs/roadmap. Use helpers/interfaces from tasks 1-3.

- [x] Test timing gates, writing instruction preferences, suppression of excluded text, log scrubbing/expiry, private learning/cloud suppression, profiles, timed pause and no request before preview Send.
- [x] Implement service/session settings, context source/permissions, safe preview invalidation, native popup reply data and secured viewer endpoints.
- [x] Run engine suite, native suite, frontend checks and roadmap checker; review cross-component privacy and compatibility.
- [x] Record completion, release and manual test checklist in roadmap/docs. Report any unverified live Windows behavior plainly.

## Verification

626 Python tests passed with one skip; 77 native tests and four settings-form tests passed. The x64 DLL built with /MT and /W4 /WX. Live popup rendering and OpenRouter calls remain user checks; see docs/settings.md.
