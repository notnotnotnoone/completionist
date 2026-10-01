# Completionist: agent guide

System-wide, VS Code-style English autocomplete for Windows. A C++ TSF text service (`tip/`) runs inside every app and talks to a Python engine (`engine/`) over the named pipe `\\.\pipe\completionist-engine` using length-prefixed JSON. The spec is the PRD in [issue #1](https://github.com/notnotnotnoone/typer/issues/1).

## The roadmap: keep it current (required)

`docs/roadmap/roadmap.js` is the single source of truth for what's done, in progress and next, and for every release. Two plain HTML pages read it. The user opens them straight from disk in a browser; nothing is published.

- `docs/roadmap/index.html`: the highway map. Every release on highways, one for each part of the app, with the release writing, the road work and the itinerary.
- `docs/roadmap/tasks.html`: the task board by release, with filters, story coverage, decisions, risks and the log.

`roadmap.js` is `window.COMPLETIONIST_ROADMAP = { ... };` where everything after the `=` must be strict JSON: double quotes, no comments, no trailing commas.

**Updating the roadmap is part of every task, not a follow-up.** A session that changes code, plans or findings without updating the roadmap is not finished.

**Every commit updates the roadmap. No exceptions.** Every single commit, including small fixes, docs and merges, stages `docs/roadmap/roadmap.js` with at least a new line at the top of `log` saying what that commit did, and `python scripts/check_roadmap.py` must print `roadmap ok` before you commit. If a commit has nothing else to change in the roadmap, the log line is still required.

**Highways (the roadmap map).** The map draws each part of the app as a highway (`highways` in `roadmap.js`), and a task's `area` decides which highway it's on.
- Every task area must belong to exactly one highway. A new area means adding it to a highway's `areas`.
- Every patch release names the highways it touched: `"highways": ["engine"]`. Minor releases get theirs from their milestone's tasks, and major releases take every open highway.
- A bug-fix patch adds `"kind": "fix"` and is drawn as a cul-de-sac. Other patches leave `kind` out.
- A highway with `"opens": "X.0.0"` starts at that major release. Nothing before that version may name it.
- The checker enforces all of this.

The design for the map pages, and the plan to build them, are in `docs/superpowers/specs/2026-09-29-roadmap-highway-map-design.md` and `docs/superpowers/plans/2026-09-29-roadmap-highway-map.md`.

1. **At the start of a session**, read `roadmap.js`. Unless the user asks for something else, work on the tasks marked `doing` or `next`, and tell the user which one you're picking up.
2. **When you start a task**, set its status to `doing`.
3. **When you finish a task**, set it to `done` and add `refs` (branch, key files, version). Promote the next task(s) to `next`.
4. **When plans change**, edit the roadmap in the same session:
   - Add newly discovered work as tasks.
   - Split tasks that grew too large.
   - Mark abandoned work `dropped`. Never delete a task silently.
   - Keep each task's `stories` pointing at the PRD user stories it delivers.
   - Keep the planned release writing in step with what each release's tasks now contain.
5. **When a release's last task is done**, set its group to `done` and make the next one `active`. Exactly one milestone is active at a time.
6. **Record decisions and findings.** Add them to `decisions`, and add or re-status entries in `risks` (`open`, `mitigated`, `retired`).
7. **With every change**, set `updated` to today's date and add a one-line entry at the **top** of `log`: what changed and why.
8. **Validate** with `python scripts/check_roadmap.py`. It must print `roadmap ok`. It also enforces the writing lengths below.
9. **Commit** `roadmap.js` in the same commit as the work it describes. Every commit has a `roadmap.js` change (see the rule above).

Task statuses: `todo`, `next`, `doing`, `blocked`, `done`, `dropped`. Milestone statuses: `planned`, `active`, `done`. Task ids are `<group>.<n>` (e.g. `M9.9`); never reuse or renumber them. The `milestones` list in `roadmap.js` is just the container that groups the tasks leading to one release. **People read versions, not M-ids.** The pages show a group as its release and name ("0.7.0 · Typo-tolerant words") and every task by its `version`. The ids and group names are internal keys, and old log lines keep them.

**Task versions (required on every task).** A task's `version` is the version it ships in:
- **Every change is a task**, including small fixes and docs, and **finishing a task releases it as a patch**. A fix to older work is a new task in the group that is active now.
- **Unshipped work** gets its own patch number, counting up from the last release before it, so the tasks leading to `0.7.0` are `0.6.2`, `0.6.3`, ... `0.6.10`, and `0.7.0` is the finish line. Add the new task with the next number, and renumber the planned ones if it ships first.
- **Already-shipped work** names the release it actually went out in, so several old tasks can share one version.
- The checker enforces this: every task has a `version`, a finish-line release (`X.Y.0`) can't be a task's version until it is released, and two unshipped tasks can't share a number.
- Keep `milestones` in version order (by their release), so the pages read left to right.

### Releases and versions

Completionist uses semantic versioning (`MAJOR.MINOR.PATCH`). Every version in `releases` gets writing whose length matches its size, in plain, standard English prose:

| Release | Example | Writing | Field |
|---|---|---|---|
| **Patch** | 0.0.5, 0.1.1 | Exactly **one sentence** (under 50 words) saying what changed | `text` |
| **Minor** | 0.1.0 | **One paragraph** (50–220 words). Each minor release is one milestone (`milestone` field) | `text` |
| **Major** | 1.0.0 | **An essay** of at least 4 paragraphs and 450 words: what the release is, why it's built this way, what it costs, and what it leaves out | `essay` (list of paragraphs) |

- **Every merge to `main` that changes behaviour, tooling or docs is a patch release.**
  - Add the next patch version after the latest released one (e.g. 0.0.5, or 0.1.1 once 0.1.0 is out).
  - Set it to `released` with today's date and one sentence.
  - Keep `releases` in ascending version order, with all released versions before unreleased ones.
- **Finishing the last task of a group releases its minor version.**
  - Set it to `released` with a date.
  - Rewrite its paragraph in the past tense to describe what actually shipped.
  - Mark the following release `next`.
- **A major release** rewrites its planned essay as a retrospective of what actually shipped.
- **Planned versions** (`planned`, and at most one `next`) describe the intent.
- **Tag every released version** `vX.Y.Z` on `main` and push the tag.
- Package versions (such as `engine/pyproject.toml`) are not release versions. The release version lives only in `roadmap.js`.

## Layout and commands

| Path | What |
|---|---|
| `engine/` | Python engine, a standalone uv project. `cd engine && uv sync && uv run pytest` |
| `tip/spike/` | M0 throwaway TSF spike: `build.cmd`, `register.ps1`, findings in `README.md` |
| `tip/` | The real TSF DLL (M1, adapted from Microsoft's SampleIME) |
| `docs/roadmap/` | Roadmap data (`roadmap.js`), the highway map (`index.html`, `map-layout.js`, `map.js`, `page.js`), the task board (`tasks.html`), shared `theme.css` and `common.js` |
| `CONTEXT.md` | What the product is, its architecture, glossary, the official Evergreen color scheme and typography |
| `scripts/` | `check_roadmap.py`; `install.ps1` / `uninstall.ps1` (user-run: UAC, keyboard, logon task) |

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
- **Tests:** TDD with pytest for engine modules, testing behaviour through public interfaces with small injected fixtures. The DLL's key router is pure logic with native tests; the TSF plumbing is tested manually.
