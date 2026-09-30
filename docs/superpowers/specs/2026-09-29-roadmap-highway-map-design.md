# Roadmap highway map: design

**Status:** approved by the user on 2026-09-29. Built partly (see "What already exists"). The rest is planned in `docs/superpowers/plans/2026-09-29-roadmap-highway-map.md`.
**Roadmap task:** M1.15.
**Branch:** `worktree-roadmap-map` (worktree `.claude/worktrees/roadmap-map`), based on `main` at the 0.0.5 merge.

## Goal

Replace the two roadmap pages with one much clearer, heavily styled **highway map** of every release, plus the existing task board restyled to match. Make **Evergreen** the official color scheme of the assistant. The product was named Completionist after this spec was written; it was called Typer (a working name) before that. Write `CONTEXT.md` as the project's reference for everything important.

The user asked for a **maximalist** style. Be bold and rich: signs, shields, textures, big type and small details everywhere. It still has to be readable and accessible.

## Decisions the user made

1. **The map is a horizontal highway or metro map**, like a transit diagram: time runs from left to right, and roads branch and merge on smooth curves.
2. **Each part of the app is its own highway.** The highways **sync at major releases**, where they all merge into one interchange and then spread out again.
3. **Minor releases** are stations across the highways they touched. **Patches** are stops, and **bug-fix patches are small cul-de-sacs**.
4. **The color scheme is Evergreen**, with no orange-and-white and no blue-and-black. The final values are below, and they have been checked for color-blind safety.
5. **Maximalist styling.**
6. **`CLAUDE.md` says every commit updates the roadmap.** This is done.
7. **Don't touch the M4 tasks.** Another session owns them. Only this work's own entries change: M1.15, the log, decisions, `updated`, and the new fields.

## Non-goals

- Nothing is published or hosted. The pages are opened from disk (`file://`), so there is no build step, no bundler, no framework and no npm packages.
- No changes to the engine or DLL code.
- No changes to M4 task statuses, and none to anything else another session owns.

## What already exists (built in this branch, don't rebuild)

| File | State |
|---|---|
| `docs/roadmap/roadmap.js` | Schema 3. Has `highways` (below), `highways` on every patch release, task **M1.15** (doing), a decision entry and a log entry. |
| `scripts/check_roadmap.py` | Enforces the highway rules (below). A scratch negative test confirmed 11 kinds of breakage are caught. |
| `docs/roadmap/theme.css` | **Done.** The Evergreen tokens for light and dark, fonts, paper grain and topographic texture, top bar, `.sign`, `.exit-tab`, `.diamond`, `.pill`, `.meter`, section headings and `.lane-rule`. Pages must use these tokens and not add their own hex values. |
| `docs/roadmap/common.js` | **Done, but not yet run in a browser.** Exposes `window.RM` (API below) and handles the Day/Night toggle. |
| `CLAUDE.md` | Has the every-commit rule and the highway rules. The page names in its "roadmap" section and layout table still describe the old pages. Task 6 of the plan fixes that. |
| `docs/roadmap/index.html`, `docs/roadmap/releases.html` | **Old pages, now broken.** They use the old token names (`--bg`, `--accent` meaning blue, Atkinson and Martian fonts), which `theme.css` no longer defines. The plan replaces them. |

## Data model (live now)

```jsonc
"highways": [
  { "id": "engine",  "title": "Engine",            "blurb": "Python: words, phrases, learning, metrics", "areas": ["engine", "data", "bench"] },
  { "id": "tsf",     "title": "Text service",      "blurb": "C++ TSF DLL inside every app",           "areas": ["dll"] },
  { "id": "tooling", "title": "Tooling",           "blurb": "Install, tests, docs and the repo",       "areas": ["install", "test", "docs", "repo", "toolchain"] },
  { "id": "browser", "title": "Browser extension", "blurb": "Chrome, over Native Messaging",          "areas": ["extension"], "opens": "1.0.0" }
]
```

- A task's `area` decides its highway. Every area belongs to exactly one highway.
- **Patch releases** have `"highways": [...]`, which is required and non-empty. Bug fixes add `"kind": "fix"`.
- **Minor releases** get their highways from their milestone's tasks that aren't dropped. They can list `highways` explicitly, but don't need to.
- **Major releases** take every highway that is open at that version.
- `opens` is a major version that appears in `releases`. A highway doesn't exist before it.
- The highway colors are the CSS variables `--hw-<id>`, for example `--hw-engine`. A new highway needs a new token, validated as described in the color section.

### `window.RM` (common.js)

```
RM.data                      the roadmap object
RM.highways, RM.hwById[id]   highway list and lookup
RM.hwOfArea[area]            highway object for a task area
RM.hwColor(id)               "var(--hw-<id>)"
RM.hwOpenAt(h, version)      true if highway h exists at that version
RM.tasks                     every task, with t.ms (its milestone) and t.hw (its highway) attached
RM.milestoneById[id], RM.releaseForMilestone[msId]
RM.progress(tasks)           {done, active, total}; active = doing|next|blocked; dropped tasks excluded
RM.tasksOn(milestone, hwId)  that milestone's tasks on one highway
RM.highwaysOf(release)       highway ids a release touches (the rules above)
RM.live(tasks)               tasks that aren't dropped
RM.esc(s), RM.rich(s)        HTML escape; rich also turns `code` into <code>
RM.words(s), RM.parseVer(v), RM.cmpVer(a,b), RM.tierOf(v) -> "patch"|"minor"|"major", RM.fmtDate(iso)
RM.STATUS, RM.RELEASE_STATUS, RM.MS_STATUS, RM.AREA, RM.areaName(a)
RM.bindThemeButton(btn)      wires a Day/Night button; dispatches "rm:theme" on document when the theme changes
RM.isDark()
```

## How to read the map

| Thing | Drawn as |
|---|---|
| **Highway** (a part of the app) | A thick horizontal road in its highway color. The highways are stacked, with Engine on top, then Text service, Tooling and Browser extension. |
| **Major release** (X.0.0) | **An interchange.** Every open highway curves in to a tight bundle, runs through one wide capsule, and curves back out to its lane. A shield-shaped marker sits in the space above the bundle, and an overhead gantry sign at the top carries the version and title. A highway that opens at this release starts from the bundle and curves out to its lane. |
| **Project start** (not a release) | An interchange at the far left, where every highway without `opens` begins together. It's labelled "0.x: groundwork and preview". |
| **Minor release** (X.Y.0) | **A station**: a tall capsule across the lanes of the highways it touched, with a dot in the lane color on each of them. Above it is an **exit sign** with a small "EXIT 0.1" tab and the title, on a thin post down to the capsule. Exit signs alternate between two rows so they never overlap. |
| **Patch** (X.Y.Z) | **A stop**: a circle on the one highway it touched, or a small capsule across several. The label underneath is the version in mono type with the title in up to two lines. |
| **Bug-fix patch** (`"kind": "fix"`) | **A cul-de-sac**: a short spur that drops about 36 px from its highway and ends in a round turnaround bulb, labelled "FIX" and the version. |
| **Released station** | Filled solid, with a white check mark. |
| **Up next** (`status: "next"`) | Magenta (`--accent`) ring with a slowly pulsing halo, and a small "NEXT" tag. |
| **Planned** | Surface-colored fill with a dashed outline in `--ghost`. |
| **Road built** (the milestone's tasks on that highway are done) | Solid highway color with a white dashed lane marking down the middle. |
| **Road under construction** (doing, next or blocked) | Diagonal stripes of the highway color and the surface color, with a small barricade (magenta and white bars) where construction starts. |
| **Road not built** (todo) | The highway color at 35% opacity, dashed. |
| **Open road with no work planned** | Thin dotted line at 35% opacity. |
| **Now** | A magenta vertical line across every lane, just after the latest released version, with a map-pin marker labelled "YOU ARE HERE" and "0.0.5 shipped" (the latest version). |
| **The future** | After the last release, the roads continue about 120 px and fade out, with a faint "more to come" label. |

**Road progress:** each milestone's release is an anchor. So is each interchange, and so is the end of the road. The road between two anchors on a highway belongs to the milestone at the right-hand anchor. Its length is split in proportion to that milestone's tasks on that highway: done tasks are built, doing, next and blocked tasks are under construction, and todo tasks are not built. If a stretch has no tasks on that highway, it is built when the right-hand anchor has been released, not built when it's an unreleased interchange, and otherwise an open road with no work planned. This is why road can look built past "now": the work is done but not released yet. The legend must say so.

## Map geometry

All of this is computed by a pure function, `layoutMap(data)` in `docs/roadmap/map-layout.js`, which has node tests. The full code is in the plan. Constants:

```
left gutter 190 (lane labels) · first lane y 250 · lane gap 116 · bundle gap 18
curve length 80 · joint 12 (the straight run inside a capsule) · bottom padding 140
column widths: major 220 · minor 170 · patch 104 · now 120 · end 160
sign rows (top y): gantry 24 · exit row A 100 · exit row B 168
```

- Events run from left to right: the synthetic start interchange (`version "0.0.0"`, `start: true`), then every release in version order, with "now" inserted after the last released one, then "end". Each event gets a column, and its `x` is the middle of that column.
- Each highway is one continuous SVG path, from its opening interchange to the end. The path is drawn several times, each time clipped by a `<clipPath>` rectangle covering one piece's x range, in that piece's style. This is how a single curve can be part built and part planned.
- Under every road, draw a casing: the same path, 6 px wider, in `--surface`. That keeps crossings and curves clean.

## Pages

### `index.html`: the highway map (new)

Files: `index.html` (markup and page CSS), `map-layout.js` (pure geometry), `map.js` (SVG drawing and map interaction, exposes `window.CompletionistMap`), `page.js` (everything else on the page, plus the wiring). Scripts load in this order at the end of `<body>`: `roadmap.js`, `common.js`, `map-layout.js`, `map.js`, `page.js`.

Sections, top to bottom:

1. **Top bar** (`.topbar` in `theme.css`):
   - a brand mark: an SVG shield in `--sign` with a white "T"
   - "Completionist", with the small caps line "autocomplete" underneath
   - `nav.pages` with Map and Tasks (Map is current)
   - "Updated <date>" in mono
   - a `#theme` Day/Night button, wired with `RM.bindThemeButton`
2. **Masthead**: a full-width `.sign` panel, the biggest element on the page.
   - An `.exit-tab` above it reading "AUTOCOMPLETE FOR WINDOWS".
   - The name "Completionist" in huge Overpass 900 type (clamp from 3.5rem to 8rem), with `tagline` under it.
   - Two road-sign arrow lines. One is "↑ NEXT EXIT 0.1.0 · Words everywhere", taken from the release with status `next`. The other is "→ 1.0 · Autocomplete for everything you type", the next major release.
   - Behind the text, the latest version drawn huge (about 16rem) in outline only (`-webkit-text-stroke` in `--sign-dim` at about 25% opacity).
3. **Now sign**: a dark `--vms` panel with `--vms-ink` text in Overpass Mono, like an electronic road sign. It has a "NOW" label and shows `data.now`, with LED dots drawn on top (a small repeating radial-gradient mask). Show the text exactly as it is in the data.
4. **Mile-marker posts**: a row of 5 tall, narrow green posts, each with a white stacked number and a small label:
   - tasks built (done out of total)
   - releases shipped (released out of all)
   - highways open (of how many)
   - the active milestone's progress
   - PRD stories fully delivered (every live task on the story is done)
5. **The map**: a heading ("The *highway*"), then controls:
   - `#go-start`, `#go-now` ("You are here"), `#go-end`
   - `#fit` ("Whole map"), with `aria-pressed`
   - a highway spotlight chip for each highway, with a colored swatch and the title; clicking one dims the others to 20%, and clicking it again clears the spotlight

   The map sits on a "folded road map" panel: `--paper-2`, with 2 or 3 faint vertical fold creases (`linear-gradient`) and a rounded border.
   - Inside the panel, `.map-scroll` scrolls sideways and holds `.map-inner`, which is exactly the layout's width and height.
   - `.map-inner` holds a sticky `.lane-tags` column: one highway marker per lane (a pill in the highway color, white Overpass 900 italic uppercase title, blurb beneath), sticky at `left: 0` with a solid backdrop and shadow. It's only visible once the map is scrolled more than 40 px, because the SVG draws its own lane labels at x < 190.
   - The `svg#map` sits underneath.
   - Under the panel is a **legend cartouche**: a boxed map key with a small compass rose, one entry per item in "How to read the map", each with a mini SVG sample, plus the note that built road can run past "now".
6. **Travel guide** (`#guide`, `aria-live="polite"`): the details for the selected station.
   - **Header:** a sign that matches the station type. A patch gets a small green mile-marker plate with its version. A minor release gets an exit tab ("EXIT 0.1") on a sign. A major release gets a shield.
   - **Content:** the title, a status pill, the date for released versions, and the highway chips.
   - **Writing:**
     - Patch: its one sentence, in Fraunces 1.25rem.
     - Minor: its paragraph, in Fraunces with a big drop cap. Beside it, a milestone box with the goal, "Done when", a meter for each highway (done out of total for its tasks on that highway), and the milestone's tasks grouped by highway. Each task shows a status pill, its id and title, and links to `tasks.html#<id>`.
     - Major: the whole essay, in Fraunces with a drop cap, in two columns on wide screens, with the word count.
     - Start interchange: a short description of the 0.x line, and the list of its minors and patches.
   - **Selection:** the default is the `next` release, or the latest one if there isn't a next. Selection is kept in `location.hash` as `#v0.1.0`, and `#v0.0.0` means the start interchange.
7. **Road work ahead**:
   - A `.diamond` reading "ROAD / WORK / AHEAD", beside the heading.
   - One column per highway, with a stripe in the highway color, listing its tasks marked doing, next or blocked, in that order. Each shows a pill, its id and title, and links to the task board.
   - A highway with none says "Clear road".
8. **Itinerary**: the full text of every release, readable without the map. This is the accessible alternative to the map.
   - Group by major release: a big shield and heading for each ("0.x · Groundwork and preview", "1.0 · Autocomplete for everything you type").
   - Inside each group, list every release in order: version in mono, title, status pill, date, and the full writing.
   - Every entry has a "Show on map" button that selects that station and scrolls the map into view.
9. **Footer** (`footer.site`): the source of truth (`roadmap.js`), links to the task board, `../../CONTEXT.md`, `../../CLAUDE.md`, the PRD and the repo.

### `tasks.html`: the task board (moved and restyled)

Move the current `index.html` here with `git mv` so its history is kept, then:

- Swap its fonts link and old tokens for the Evergreen ones (`--bg` → `--paper`, `--display` → `--mono` wherever it meant ids and numbers, blue `--accent` → `--accent` or `--accent-ink`).
- Use the shared `.topbar`, with Tasks as the current page.
- Load `common.js` and use `RM` where it helps (`RM.esc`, `RM.rich`, `RM.STATUS`, `RM.bindThemeButton`), without rewriting what works.
- Give every task row a 5 px left stripe in its highway color, and show a small highway name next to the area.
- Add a highway filter: chips alongside the status chips. Keep the area selector.
- The "Ships as vX" and "Shipped in vX" links go to `index.html#vX`.
- Remove every link to `releases.html`.
- Keep the "Up next" candidate popup (the Completionist motif), restyled with `--sign` and `--vms` tones.
- Give milestone cards a highway-colored progress strip: one segment per highway, sized by that highway's share of the milestone's tasks.

### `releases.html`

Delete it (`git rm`). Its content lives on in the map's travel guide and the itinerary.

## Evergreen: the official color scheme

The values are already in `theme.css`. `CONTEXT.md` must repeat this table.

| Token | Light | Dark | Role |
|---|---|---|---|
| `--paper` | `#EEF2EC` | `#0E1512` | page background |
| `--paper-2` | `#E3EAE1` | `#121B17` | map panel, bands |
| `--surface` | `#FBFCF8` | `#141C18` | cards, road casing |
| `--ink` | `#16221C` | `#E4EBE6` | text |
| `--muted` | `#55635B` | `#97A69D` | secondary text |
| `--ghost` | `#626E67` | `#7F8C84` | planned outlines, faint text |
| `--line` / `--line-strong` | `#D5DDD6` / `#B7C3BA` | `#25302A` / `#34423A` | borders |
| `--sign` | `#0A5A3D` | `#0F6B48` | guide-sign green: signs, masthead, current page tab |
| `--sign-deep` | `#06402B` | `#094A31` | sign edge |
| `--sign-ink` / `--sign-dim` | `#FFFFFF` / `#CFE6DA` | `#FFFFFF` / `#BFE3CF` | text on signs |
| `--hw-engine` | `#0B6E4B` | `#079461` | Engine highway (pine) |
| `--hw-tsf` | `#5E3BA3` | `#7553BD` | Text service highway (violet) |
| `--hw-tooling` | `#0CA1A7` | `#11A3A9` | Tooling highway (teal) |
| `--hw-browser` | `#787804` | `#6C6C01` | Browser extension highway (olive) |
| `--accent` | `#AB3F84` | `#C0188D` | magenta: now, up next, construction, focus ring |
| `--accent-ink` | `#9A3576` | `#EE8FC6` | magenta text and links |
| `--accent-soft` / `--on-accent` | `#F4E1EC` / `#FFFFFF` | `#3A1830` / `#FFFFFF` | pill background, text on magenta |
| `--ok` / `--ok-soft` | `#1F6B45` / `#DCEDE2` | `#5FCB8E` / `#15301F` | status done |
| `--warn` / `--warn-soft` | `#7A5C00` / `#F3E9C8` | `#E3C052` / `#332A10` | status next |
| `--bad` / `--bad-soft` | `#B3261E` / `#F8DEDA` | `#FF8C7E` / `#3A1916` | status blocked, open risk |
| `--todo` / `--todo-soft` | `#56625B` / `#E1E7E2` | `#A5B0A9` / `#1E2823` | status todo |
| `--vms` / `--vms-ink` | `#101814` / `#FF9ED2` | `#060A08` / `#FF8FCB` | the "now" electronic sign |

**Validation**, done with the dataviz skill's `validate_palette.js` (Machado 2009 color-blindness simulation, OKLab ΔE×100):

- Adjacent highways passed every check, with worst color-blind ΔE 17.3 in light mode and 16.6 in dark.
- The accent against every highway passed every check, with worst ΔE 8.5 in light mode and 8.4 in dark.
- Every text pair is at least 4.5:1. For example, `--accent-ink` on `--paper` is 5.9, and white on `--sign` is 8.3.

**Rules:**

- **Highway colors only ever mean highways:** roads, lane markers, dots and task stripes. Never status.
- **Magenta only means now, up next, in progress, selection or focus, and links.** It's never a highway.
- **Status is never shown by color alone.** There's always a word or a shape too (check, ring, dashes, barricade).
- **Sign green is the brand surface.** Text on it is white.
- **No orange and no blue brand color, anywhere.**
- **Dark mode is its own set of values.** Only the texture is inverted. Change both modes together.
- **A new highway color** must pass `validate_palette.js` in both modes, against the highways next to it (the default adjacent check) and with the accent alternating against every highway. Put the results in `CONTEXT.md`.

## Typography

- **Overpass** (Google Fonts, in the style of Highway Gothic), weights 400 to 900 and italics 800 and 900: UI, signs and headings.
- **Fraunces**, optical sizes 9 to 144, weights 400, 600 and 900, and italic: release writing, essays and drop caps.
- **Overpass Mono**, weights 400, 600 and 700: versions, ids, dates and numbers.

This link goes in the `<head>` of both pages, before `theme.css`:

```html
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Fraunces:ital,opsz,wght@0,9..144,400;0,9..144,600;0,9..144,900;1,9..144,400;1,9..144,600&family=Overpass:ital,wght@0,400;0,600;0,700;0,800;0,900;1,800;1,900&family=Overpass+Mono:wght@400;600;700&display=swap">
```

SVG text measuring (label wrapping) must wait for `document.fonts.ready`, then draw the map again.

## Accessibility, layout and behavior

- **Phones:** everything works at 375 px wide with a 16 px gutter, and the page never scrolls sideways. Only `.map-scroll` scrolls horizontally.
- **Stations are reachable by keyboard:** each is `<g role="button" tabindex="0" aria-label="Version 0.1.0, minor release, Words everywhere, planned">`. Enter or Space selects it. Stations are focusable in left-to-right order.
- **The map scroller** has `role="region"`, an `aria-label`, and `tabindex="0"`. When the scroller itself is focused, the Left and Right arrow keys pan it.
- **Dragging with the mouse pans the map.** Links and buttons are excluded from dragging.
- **Reduced motion:** with `prefers-reduced-motion`, there are no pulses and scrolling isn't smooth. `theme.css` already turns animations off.
- **Two data states each get a readable message instead of a blank page:**
  - When `roadmap.js` fails to load or has a syntax error, say so and give the command `python scripts/check_roadmap.py`.
  - When the data has no highways, say that too.
- **Storage:** only the theme choice goes in `localStorage`, wrapped in try/catch. It already is, in `common.js`.
- **Changing the theme doesn't redraw the SVG**, because the SVG uses only CSS variables (`fill="var(--hw-engine)"` and so on). Canvas measuring doesn't depend on the theme.
- **No console errors** on either page, in either theme.

## Acceptance

1. `python scripts/check_roadmap.py` prints `roadmap ok`.
2. `node --test scripts/map_layout.test.mjs` passes.
3. `index.html` opened from disk shows everything in "Pages", in both themes and at both 1440 px and 375 px wide. The map starts scrolled to "You are here", with 0.1.0 selected.
4. `tasks.html` works as before (filters, search, story coverage, decisions, risks, log) in the new style, with the highway stripes and filter.
5. `releases.html` is gone, and nothing links to it (`git grep releases.html` finds nothing).
6. `CONTEXT.md` exists at the repo root and covers everything under "What CONTEXT.md contains".
7. `CLAUDE.md` and `README.md` describe the new pages.
8. The roadmap has M1.15 marked done with its refs, a log line for each commit, and every commit includes `roadmap.js`.

## What CONTEXT.md contains

1. **Name.** The product is called Completionist (first called Typer). List where the name appears, and the one-time move of the old `Typer` folders.
   - the repo, the `COMPLETIONIST_ROADMAP` global, and the engine package `completionist_engine`
   - the CLI commands `completionist-engine`, `completionist-probe`, `completionist-stats`, `completionist-build-ngrams` and `completionist-bench`
   - the pipe `\\.\pipe\completionist-engine`, `%LOCALAPPDATA%\Completionist\`, `%APPDATA%\Completionist\config.toml` and `CompletionistTip.dll`
   - the keyboard name in Settings

   All of those change if the product gets a real name.
2. **What it is:** system-wide, VS Code-style English autocomplete for Windows, with a word popup at the caret and a greyed phrase row above it.
3. **Keys:**
   - Tab accepts.
   - Up and Down move the highlight.
   - Ctrl+Right takes the next phrase word.
   - Ctrl+Space asks for a phrase.
   - Esc dismisses.
   - Ctrl+Alt+P pauses.
   - Enter is never used.
4. **Architecture:**
   - The C++ TSF text service DLL runs inside every app. It's thin: popup, key router, context reader and pipe client.
   - It talks to the Python engine over the named pipe, using length-prefixed JSON (a 4-byte little-endian length, UTF-8, 1 MB cap).
   - The engine handles word ranking, phrases, learning, metrics, the tray and hot reload.
   - Data lives in `%LOCALAPPDATA%\Completionist`: `ngrams.sqlite`, `personal.sqlite`, `metrics.sqlite`, `spend.json`, `engine.log` and `tip.log`.
   - The config is `%APPDATA%\Completionist\config.toml`.
5. **How words are ranked:** word frequency, then the previous one or two words (n-grams from WikiText-103), then your own habits (counts only, and a new word is learned after 3 uses).
6. **How phrases work:**
   - They come from cheap FIM completion models, never premium chat models. DeepSeek is the default.
   - Context is anchored for cache hits.
   - There's a $0.50 daily cap.
   - Phrases appear on their own in allow-listed apps, and on Ctrl+Space elsewhere.
   - The API key comes from an environment variable.
7. **Privacy and safety:**
   - Password, URL, email and number fields are silent, and so are editors and terminals.
   - Nothing is learned from pasted text.
   - Metrics are counts only.
   - Nothing is used for caret or screen tracking except TSF: no AI and no vision.
8. **Glossary:** TSF, text service or TIP, IME, input scope, edit session, engine, pipe, word completion, phrase, phrase row, FIM, anchored context, allow-list and block-list, silent field, personal store, promotion, n-gram table, budget, metrics, harness, spike, milestone, release tiers, highway, interchange, station, stop, cul-de-sac.
9. **Releases and the roadmap:**
   - Semver.
   - The writing tiers: a sentence for a patch, a paragraph for a minor, an essay for a major.
   - `roadmap.js` is the source of truth, and every commit updates it.
   - The highway-map rules, from "How to read the map".
10. **Evergreen**, in full: the token table, the rules and the validation results, from this spec.
11. **Typography**, from this spec.
12. **Key decisions and constraints:** summarize `roadmap.js` `decisions`, plus the working agreements in CLAUDE.md that shape the product. These include the cost ceiling, keys in environment variables, Google Docs being out of scope, and the user doing hands-on testing.
13. **Where things live:** a short table of paths.
