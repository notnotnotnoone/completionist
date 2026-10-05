# Completionist: agent guide

System-wide, VS Code-style English autocomplete for Windows. A C++ TSF text service (`tip/`) runs inside every app and talks to a Python engine (`engine/`) over the named pipe `\\.\pipe\completionist-engine` using length-prefixed JSON. The spec is the PRD in [issue #1](https://github.com/notnotnotnoone/typer/issues/1).

## The roadmap: keep it current (required)

`docs/roadmap/roadmap.js` is the single source of truth. It is `window.COMPLETIONIST_ROADMAP = { ... };` with strict JSON after the assignment. The two plain HTML pages work directly from disk: `index.html` is the Release atlas and `tasks.html` is the task board. Nothing is published.

**Versions are destinations. Tasks are roadwork.** Several tasks share a `targetRelease`. Completing a task never creates a release, allocates a patch number or tags Git. Every change, including docs and fixes, is a task.

**Every commit updates the roadmap. No exceptions.** Stage `roadmap.js` with at least a new line at the top of `log` describing the commit, and run `python scripts/check_roadmap.py` before committing. It must print `roadmap ok`. Plans and findings also update the roadmap in the same session.

### Model and routine edits

- Schema 4 has flat `tasks`, `releases`, `releasePlans`, `highways` and one `activeRelease` (or null if no future destination remains).
- `releases` contains actual shipped versions and coherent planned destinations, in ascending version order, with shipped versions first. Preserve genuine shipped records, tags and historical writing.
- `releasePlans[VERSION]` stores the destination title, goal, `done_when` and broad `scope`: `contained`, `medium` or `large`. Existing `milestone` fields on old release records are historical metadata only.
- Tasks have stable internal `id`, full `title`, `status`, `area`, `targetRelease`, `stories`, optional `refs`/`notes`, optional concise `label` and optional local numeric `stage`. No task has a `version`. Existing M-style IDs remain internal for links; new IDs are descriptive slugs. Never reuse or renumber IDs.
- A done task can have a `completed` date. `shippedIn` names the actual released version carrying its work; absence means not yet shipped. Completed work and shipped software are different facts.
- Task array order controls local order. Stages are local sequence information; reorder tasks without renumbering IDs or releases. Retargeting logs the old and new destination, preserves the task and removes its old local stage.
- Statuses: `todo`, `next`, `doing`, `blocked`, `done`, `dropped`. **When the code is written, the task is `done`, not `doing`:** "complete, waiting for verification" is the default state. `doing` means code is still being written. Use `--note` to say IRL verification is pending, and add the result when the owner reports back. Dropped work stays in history, does not count toward progress and never silently disappears.

**Start every session with** `python scripts/roadmap.py brief`. Generated briefs are ignored by Git; never edit them directly. Choose the smallest view:

- **Minimal:** current `doing`, `next` and `blocked` work with IDs, target releases, areas and local stages.
- **Medium:** current goals and criteria, all remaining active-release work with full notes, five recent dated completions, and the five latest shipped releases matched by `shippedIn`.
- **Maximum:** a lossless JSON snapshot of every field and historical log.

Run `python scripts/roadmap.py brief --level minimal|medium|maximum` to print a view; all views regenerate together. `roadmap-brief.md` is the minimal alias.

Routine commands:

```text
python scripts/roadmap.py add-task 2.0.0 --area dll --title "Build renderer IPC" --label "Renderer IPC" --stories 1
python scripts/roadmap.py start TASK_ID
python scripts/roadmap.py done TASK_ID --refs branch,key-file --note "What was checked."
python scripts/roadmap.py move TASK_ID --before OTHER_ID
python scripts/roadmap.py retarget TASK_ID 2.1.0
python scripts/roadmap.py drop TASK_ID --reason "Why it was superseded."
python scripts/roadmap.py log "What changed and why."
```

`add-task` optionally accepts `--id`, `--before` and `--stage`. All edits validate before writing; rejected edits leave the file unchanged. Serialization is deterministic. Use the helper instead of reading thousands of lines. Open a small source range only for release plans, writing, decisions, risks or other fields not covered by the helper.

When starting work, mark the task doing. On completion, add references and mark it done; the helper promotes the next waiting task when appropriate. Keep story links, planned release writing, decisions and risks current. Add new discoveries as tasks; do not make software versions for them. Set `updated` to today's date and prepend a log entry with every change.

### Labels and the map

- **Every task gets a short `--label`.** Use 2-4 words, about 28 characters at most, naming the thing and not the action ("Fuzzy index", not "Build the fuzzy index"). Dots on the map show the label. Without one, the map falls back to the full title, trimmed with an ellipsis.
- **How the map draws work:** each task is a dot on its highway's lane. Shipped tasks are grouped under the release that shipped them, with the release number as a small caption under the first dot. A run of dots between two signs shows how much work a release took. Major and minor releases are signs. A shipped release with no recorded task keeps a release stop.
- **Never create a release to make a dot appear.** A task is already visible as a dot.
- **`--area` chooses the lane.** The areas-to-highway mapping is `highways` in `roadmap.js`, so pick the area on purpose.
- **`shippedIn` places a done task in history.** The `release` helper sets it. Do not set it by hand.
- **After changing map code,** run `node --test scripts/map_layout.test.mjs` and look at the page. Shipped-history behaviour is covered by those tests.

### Shipping and version meaning

PATCH means a real small fix, regression, polish or maintenance release. MINOR means a coherent capability. MAJOR means a major product, interface or architecture expansion. Use judgment, not task counts. Do not reserve patch numbers for future work. Neither a commit, merge nor the last completed task automatically ships a release.

- A release becomes ready when its required work is complete or explicitly dropped. Ship only after validation and integration, using a separate release action.
- For an existing destination: `python scripts/roadmap.py release 2.1.0 --validated "Integration checks passed." --text "Retrospective release paragraph..."`.
- For a concrete patch: `python scripts/roadmap.py release 1.1.15 --tasks TASK_ID,OTHER_ID --title "Fix description" --text "One sentence describing what shipped." --validated "Checks passed."`.
- Major writing can be supplied with `--essay-file PATH`, blank lines separating paragraphs. Rewrite planned writing to describe what actually shipped.
- A patch can carry several done tasks from an upcoming destination, without claiming that destination shipped. Previously shipped contributions retain their original `shippedIn`.
- The helper records shipping and moves `activeRelease` to the next destination; it never tags, pushes or publishes. Tag `vX.Y.Z` on the actual release commit on main as a separate authorized shipping step. Ask before pushing to main.
- Package versions are independent; release versions live in `roadmap.js`.

Writing remains proportional: a patch has one sentence, at most 50 words; a minor has one paragraph of 50-220 words; a major has at least four paragraphs and 450 words. The checker enforces this for planned and shipped releases.

### Atlas and highways

Every area belongs to exactly one highway. Releases connect only participating highways; majors do not automatically involve all lanes. New shipped records name the highways actually touched. Historical release records remain unchanged; historical destinations may derive lanes from their original task contributions.

A highway's `opens` names its major launch. Its construction lane may begin in that launch's planned region, before the release itself; earlier shipped releases cannot claim it.

Future region width follows `scope`, never task count, elapsed time or completion percentage. Adding work increases task stack height without moving release destinations. Small stops display task labels, statuses and optional local stages; dropped tasks use faded spurs. Completed unshipped roadwork remains visible. Older shipped contributions appear in history, not as duplicate new stops. History is never compressed: every shipped task is its own dot on its highway, grouped under the release that shipped it, so a run of dots shows how much work a release took; shipped releases with no recorded task keep a release stop, and every historical release remains selectable. YOU ARE HERE marks the active destination region, while latest shipped is separate.

The current design and implementation plan are `docs/superpowers/specs/2026-10-02-roadmap-release-atlas-design.md` and `docs/superpowers/plans/2026-10-02-roadmap-release-atlas.md`. Earlier highway-map documents describe the historical design.

## Layout and commands

| Path | What |
|---|---|
| `engine/` | Python engine, a standalone uv project. `cd engine && uv sync && uv run pytest` |
| `tip/spike/` | M0 throwaway TSF spike: `build.cmd`, `register.ps1`, findings in `README.md` |
| `tip/` | The real TSF DLL (M1, adapted from Microsoft's SampleIME) |
| `docs/roadmap/` | Roadmap data (`roadmap.js`), the highway map (`index.html`, `map-layout.js`, `map.js`, `page.js`), the task board (`tasks.html`), shared `theme.css` and `common.js` |
| `CONTEXT.md` | What the product is, its architecture, glossary, the official Evergreen color scheme and typography |
| `scripts/` | `roadmap.py` (agent brief and roadmap edit commands, tests in `test_roadmap_cli.py`), `check_roadmap.py`; `install.ps1` / `uninstall.ps1` (user-run: UAC, keyboard, logon task) |

- **C++ builds** use the VS 2022 Build Tools from the command line: `vcvars64.bat`, then `cl`, with the static CRT (`/MT`), x64 only. See `tip/spike/build.cmd` for the pattern.
- **A loaded DLL is locked.** Rename it aside before rebuilding. Apps (Chrome especially: use `chrome://restart`) must restart to pick up a new build.
- **Logs** go to `%LOCALAPPDATA%\Completionist\`.

## Working agreements

- **The user doesn't write C++.** Claude owns all C++ code and the toolchain. Keep the toolchain CLI-only and small on disk.
- **The user does hands-on app testing** to save cost:
  - Give a short checklist, then read the logs.
  - Don't drive the screen with computer use or screenshots unless asked.
- **Keep the user in the review loop, in plain English.** The user is new to Python, tests and Windows internals, and wants to follow the project and take part without being overwhelmed.
  - **Before you change code,** say in a few sentences what you'll change and why, and what the alternatives were. Ask when it's a real decision (design, scope, anything user-visible).
  - **After you change code,** explain it in plain terms: what it does, why this way, and what could go wrong. Point at the one or two files or functions that matter; don't walk through every line unless asked.
  - **Include some technical detail,** but keep it light. Name the real concepts (for example a function, a test, a database table) and say what each one is in a phrase the first time it comes up. Skip jargon, acronyms and internals that don't help the user decide or check anything.
  - **Use analogies sparingly.** One good one beats three. Don't pile on diagrams, pseudocode or long file tours unless asked.
  - **Give the user something to check or decide** at each step: a choice, a behaviour to try, or a question about what they'd expect. Keep it to one or two items, not a quiz.
  - **Go at the user's pace.** If they say they're lost, stop and simplify instead of adding more.
  - **Don't spend their budget on maps.** Skip multi-agent codebase-mapping skills (`/understand`, `/cartographer`); they burned a whole month's budget once. Explain the parts that matter directly.
- **The TSF harness steals focus.** `tip/tests/tsf_e2e.ps1` pops a real window and needs the foreground for TSF focus. Run it only when the user says they are away, never while they work.
- **Engine extras:** `completionist-stats` (usage numbers), the tray's Open viewer (a local page: words, stats, settings), tray icon and Ctrl+Alt+P pause (`--no-tray` to skip), config hot reload, `engine.log` and `metrics.sqlite` in `%LOCALAPPDATA%\Completionist`.
- **Don't over-engineer.** Completionist is a typing tool. Build what the user asked for, the simplest way. Don't add side tools, benchmarks, dashboards or config knobs on your own, and don't turn a choice (like which model) into a project or homework for the user: pick a sensible default, say so, and make it easy to swap. Ask before adding anything beyond the request.
- **Never ask for API keys in chat.**
  - The key lives in `api_key` under `[phrase]` in the user's `config.toml` (`%APPDATA%\Completionist`), typed there by the user, either in the file or in the viewer's settings page. The page only writes it: it is never sent back to the page, never in the repo, never logged, and never printed in errors or settings.
  - Don't read the config file to look at the key.
- **Colors and type:** use the Evergreen tokens and fonts in CONTEXT.md. Don't add colors without validating them as it describes.
- **Don't change Windows system settings yourself.** Give the user the command:
  - enabling or removing the keyboard (`Set-WinUserLanguageList`)
  - registry changes outside `regsvr32`
- **Admin steps** (`regsvr32` via `register.ps1`) raise a UAC prompt the user approves.
- **Phrases go through OpenRouter** (`[phrase] models = [...]`, tried in order). Use cheap models, never premium ones. Completionist doesn't count spend; the user caps the OpenRouter key itself.
- **No AI or vision for caret or screen tracking.** The caret comes from TSF only.
- **uv:** the engine is standalone. Never run `uv init` or `uv sync` from the parent `Experiments` folder, which would turn it into a workspace and clobber its `.venv`. Use `--no-workspace` when creating projects.
- **Git:** work on a branch per chunk of work. Ask before pushing to `main`.
- **Tests (2.0.0 onward):** keep one simple pytest suite for the engine (`cd engine && uv run pytest`) and nothing more automated. Don't write new native test suites, harnesses, fixtures, review artifacts or elaborate verification plans, and don't run the existing native ones. Verify everything else in real life: the user runs the build in real apps and reports back, and the agent reads the logs. New or changed engine behaviour gets a small pytest case through the public interface; DLL and renderer changes get an IRL checklist for the user instead of tests. Roadmap tasks close on IRL verification, with the note saying what the user checked.

## Prompt Rewriting

Before acting on any user prompt, evaluate whether rewriting it for clarity, typo correction, or actionability would meaningfully improve it. **Only rewrite when there is a real improvement to make** — typos to fix, ambiguous references to resolve, or vague intent to specify. If the original prompt is already clear and actionable, proceed without rewriting and without showing a rewrite. When you do rewrite, display the rewritten version with the prefix "**Rewritten prompt:**" and wait for approval before proceeding.

**Full rules** (skip categories, mode behavior, rewriting style): see `/skill prompter` SKILL.md. Do NOT duplicate the skip list here; the skill file is the source of truth.

## FOR CODEX
Whenever a major task is done, relese confitei :)
