# Release Atlas Implementation Plan

> **For agentic workers:** Execute inline using executing-plans; user already authorized execution. Steps use checkbox syntax.

**Goal:** Replace task pseudo-versions with coherent release destinations throughout the roadmap.

**Architecture:** Flat tasks reference release versions; `releasePlans` retains goal, criteria and scope without rewriting shipped release records. Python helpers own validated mutations. A pure JavaScript layout calculates scope regions and task stacks; the renderer and page use shared normalized data.

**Tech Stack:** Python standard library, plain JavaScript, SVG, local HTML and existing Evergreen CSS.

**Spec:** `docs/superpowers/specs/2026-10-02-roadmap-release-atlas-design.md`

## Global constraints

- Preserve all shipped release objects, IDs, references, statuses and historical logs.
- Keep local file loading, Evergreen tokens and existing fonts; no framework or dependencies.
- Versions identify releases; completing tasks never releases or tags software.
- No visible M identifiers; retain internal task hashes for existing links.
- Regions express contained/medium/large scope, not time or completion percentages.
- Preserve unrelated dirty work; do not push or manufacture a release.

## 1. Data, validation and helper commands

Files: `scripts/migrate_roadmap.py`, `scripts/check_roadmap.py`, `scripts/roadmap.py`, `scripts/test_roadmap_cli.py`, `scripts/test_roadmap_model.py`, `docs/roadmap/roadmap.js`.

- [x] Write behavioral tests for migration preserving released objects, shared targets, task completion without releasing, explicit shipping, invalid edits rollback and reliable briefs. Run `python -m unittest scripts.test_roadmap_model scripts.test_roadmap_cli` and observe missing-model failures.
- [x] Implement `migrate(data: dict) -> dict` as an idempotent copy; keep historical releases unchanged, map groups to `releasePlans`, flatten tasks, replace versions with targets and actual shipping references.
- [x] Update `check(data: dict) -> list[str]` for schema 4 and implement `start`, `done`, `log`, `add-task RELEASE`, `move TASK --before TASK`, `retarget TASK RELEASE`, `release VERSION --tasks ... --validated ...`.
- [x] Validate mutations before saving; serialize deterministically. `done` keeps the releases list unchanged; release commands never touch Git.
- [x] Run tests; migrate the live data while asserting all prior released objects and task IDs/content survive. Add this implementation as a doing task targeting 2.0, with no reserved patch number.

## 2. Shared model, atlas geometry and rendering

Files: `docs/roadmap/common.js`, `map-layout.js`, `map.js`, `page.js`, `index.html`, `scripts/map_layout.test.mjs`.

- [x] Write layout tests: adding tasks does not change destination positions; task markers do not collide; dropped work does not count as built; releases touch only their highways; browser construction precedes launch; compressed history expands without changing records.
- [x] Implement `layoutMap(data, {expandedHistory=false})` returning releases/history/regions/tasks/lanes with stable IDs and coordinates. Scope widths: contained 330, medium 500, large 700. Stack task markers vertically to preserve region width.
- [x] Implement SVG destinations, labeled task stops, status shapes, subdued dropped branches and current-region marker. Retain selection, keyboard activation, highway spotlight and scroll controls.
- [x] Update the guide to select release or task, show actual shipped references, full notes and target criteria. Keep the itinerary readable and historical version hashes usable.
- [x] Run `node --test scripts/map_layout.test.mjs` and syntax checks.

## 3. Task board, briefs and instructions

Files: `docs/roadmap/tasks.html`, `prototype-concepts.js`, `CLAUDE.md`, `CONTEXT.md`, `docs/roadmap/roadmap.js`.

- [x] Adapt task board grouping/filters/deep links to target releases and names; keep story coverage honest about shipping versus completion.
- [x] Adapt read-only prototype projection to the new source so the three comparison concepts remain usable.
- [x] Replace governing task-version and every-merge-release rules with target-release and explicit-shipping rules; retain mandatory roadmap logging and validation.
- [x] Regenerate briefs, run Python and Node suites and checker, and inspect map/task views in the browser, including future releases, task selection, history expansion and narrow layouts.
- [x] Finish the implementation task and record validation evidence without releasing 2.0. Report the outstanding historical tag discrepancy and leave the atlas open.

Validation: 23 Python tests and 8 Node layout tests pass. Browser checks cover task and historical release selection, history expansion, task search, future destinations, and 390px/1440px viewports. JavaScript syntax checks pass. Direct file navigation could not be tested because the browser allows only HTTP/HTTPS; classic local script loading remains in place. No software release or Git tag was created.
