# Roadmap highway map: implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task by task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the maximalist highway-map roadmap page (`docs/roadmap/index.html`), move the task board to `tasks.html` and restyle it in Evergreen, retire `releases.html`, and write `CONTEXT.md`.

**Architecture:**
- Static HTML, CSS and JavaScript, opened from disk with no build step.
- `roadmap.js` holds the data. `common.js` (done) exposes `window.RM` helpers.
- `map-layout.js` is a pure geometry function, tested with `node --test`.
- `map.js` draws the SVG and handles map interaction. `page.js` renders the rest of the page.
- `theme.css` (done) holds the Evergreen tokens and the shared sign components.

**Tech stack:** plain ES2020 in the browser, Node 22's built-in test runner (`node:test`, no npm packages), Python 3 for `scripts/check_roadmap.py`, and Google Fonts (Overpass, Fraunces, Overpass Mono).

**Spec:** `docs/superpowers/specs/2026-09-29-roadmap-highway-map-design.md`. Read it fully before Task 1. It defines every visual element, the colors, the page sections and what counts as finished. This plan says where things go and gives code for the parts that are easy to get wrong.

## Global constraints

- **Work in the worktree** `C:\projects\Experiments\completionist\.claude\worktrees\roadmap-map` on branch `worktree-roadmap-map`. Never switch branches in the main checkout, because another session works there.
- **Every commit stages `docs/roadmap/roadmap.js`**, with a new line at the top of `log`, dated today, saying what that commit did. Run `python scripts/check_roadmap.py`, which must print `roadmap ok`, before every commit. This is a rule in CLAUDE.md.
- **Don't edit M4 tasks** or any roadmap entry other than M1.15, `log`, `updated` and `decisions`. Another session owns the rest.
- **Colors:** only the CSS variables from `theme.css`. No new hex values in pages. SVG colors go in `style="fill:var(--x)"` or `style="stroke:var(--x)"`, not in presentation attributes. No orange, and no blue brand color.
- **Fonts:** the Google Fonts link from the spec, and nothing else external. Everything else is local files next to the page.
- **No `localStorage`** except the theme key that `common.js` already uses.
- **Layout:** it must work at 375 px wide with no horizontal page scroll. Only `.map-scroll` scrolls sideways.
- **Checking your work:** open pages with the built-in browser pane (`mcp__Claude_Browser__*`) at
  `file:///C:/projects/Experiments/completionist/.claude/worktrees/roadmap-map/docs/roadmap/index.html` (and `tasks.html`), and check light, dark, 1440 px and 375 px. That browser pane is fine to use. Don't use computer use on the user's desktop.
- **Writing style** in page copy, commit messages and docs: plain, standard English, sentence case except for road-sign lettering, which is uppercase in CSS.
- **Commit messages** end with the Co-Authored-By line the session gives you.

## Files

| File | Responsibility | Task |
|---|---|---|
| `docs/roadmap/tasks.html` | The task board, moved from `index.html` and restyled | 1 |
| `docs/roadmap/map-layout.js` | Pure geometry: `layoutMap(data)`, returns the layout (no DOM) | 2 |
| `scripts/map_layout.test.mjs` | Node tests for `layoutMap` on a small fixture | 2 |
| `docs/roadmap/map.js` | Draws the SVG from the layout; selection, spotlight, pan, fit (`window.CompletionistMap`) | 3 |
| `docs/roadmap/index.html` | Map page markup and page-specific CSS | 3, 4 |
| `docs/roadmap/page.js` | Masthead, now sign, posts, travel guide, road work, itinerary, hash routing | 4 |
| `docs/roadmap/releases.html` | Deleted | 5 |
| `README.md`, `CLAUDE.md` | New page names | 5 |
| `CONTEXT.md` | Project reference and the official Evergreen scheme | 6 |
| `docs/roadmap/roadmap.js` | Log line each commit; M1.15 done at the end | every task |

---

### Task 1: Move the task board to `tasks.html` and restyle it

**Files:**
- Move: `docs/roadmap/index.html` → `docs/roadmap/tasks.html` (`git mv`)
- Modify: `docs/roadmap/tasks.html`, `docs/roadmap/roadmap.js` (log)

**Interfaces:**
- Consumes: `window.RM` from `common.js`, as described in the spec's "`window.RM`" section.
- Produces: `tasks.html#<taskId>`, which opens and scrolls to that task (this already works through `applyHash`). Tasks 3 and 4 link here.

- [ ] **Step 1: Move the file, keeping its history**

```bash
git mv docs/roadmap/index.html docs/roadmap/tasks.html
```

- [ ] **Step 2: Swap the head for the Evergreen one.** In `tasks.html`:
  - Replace the Google Fonts `<link>` with the three lines in the spec's Typography section.
  - Set `<title>Completionist Tasks</title>`.
  - After `<script src="roadmap.js"></script>`, add `<script src="common.js"></script>`.

- [ ] **Step 3: Rename the old tokens in the page `<style>`.** The only tokens are the ones `theme.css` defines. Mapping:

| Old | New |
|---|---|
| `var(--bg)` | `var(--paper)` |
| `var(--display)` where it styles ids, counts or numbers (`.t-id`, `.count`, `.big`, `.trk-meta`, `.cell`, `time`) | `var(--mono)` |
| `var(--display)` anywhere else | keep it (it's now Overpass) |
| `var(--body)` | `var(--display)` |
| `var(--accent)` used for text or links | `var(--accent-ink)` |
| `var(--pop)`, `--pop-ink`, `--pop-muted`, `--pop-line`, `--pop-hl`, `--pop-hl-ink` | `var(--vms)`, `var(--sign-ink)`, `var(--sign-dim)`, `var(--sign-deep)`, `var(--sign)`, `var(--sign-ink)` |
| `.st-active { color: var(--accent) }` | `var(--accent-ink)` |

  Then run the following. It must print nothing:

```bash
grep -nE "var\(--(bg|body|pop[a-z-]*)\)" docs/roadmap/tasks.html
```

- [ ] **Step 4: Replace the header with the shared top bar.** Swap `<div class="brand-row">…</div>` for this:

```html
<div class="topbar">
  <a class="brand" href="index.html" aria-label="Completionist roadmap, map">
    <svg class="brand-mark" viewBox="0 0 40 40" aria-hidden="true"><path d="M20 2 36 8v12c0 9-7 15-16 18C11 35 4 29 4 20V8Z" style="fill:var(--sign);stroke:var(--sign-ink);stroke-width:2"/><text x="20" y="27" text-anchor="middle" style="fill:var(--sign-ink);font:900 17px var(--display)">T</text></svg>
    <span class="brand-word">Completionist<small>autocomplete</small></span>
  </a>
  <div class="topbar-side">
    <nav class="pages" aria-label="Roadmap pages"><a href="index.html">Map</a><a href="tasks.html" aria-current="page">Tasks</a></nav>
    <p class="meta">Updated <time id="updated">…</time></p>
    <button type="button" class="theme-btn" id="theme"></button>
  </div>
</div>
```

  At the end of the page script, before `index();`, add `RM.bindThemeButton(document.getElementById("theme"));`.

- [ ] **Step 5: Add a highway stripe and label to each task.**
  - In `taskRow(t)`, add `style="--hw:${t.hw ? RM.hwColor(t.hw.id) : 'var(--line-strong)'}"` to the `<details class="task">` element.
  - Put the highway title in the area cell: `<span class="t-area">${esc(t.hw ? t.hw.title : "")} · ${esc(areaName(t.area))}</span>`.
  - Add this CSS:

```css
.task { border-left: 5px solid var(--hw); }
.tasks > li:first-child .task { border-top-left-radius: var(--r); }
```

- [ ] **Step 6: Add a highway filter.**
  - Add `hw: "all"` to `view`, include it in `save()`, and check it in `index()`, the same way `view.area` is checked.
  - In `matches(t)`, add `if (view.hw !== "all" && (!t.hw || t.hw.id !== view.hw)) return false;`.
  - In `renderFilters()`, render a chip for each highway into a new `<div class="chips" id="hw-chips" aria-label="Filter by highway"></div>`, placed after `#status-chips`, using this template:

```js
$("#hw-chips").innerHTML = [["all", "All highways"], ...RM.highways.map((h) => [h.id, h.title])]
  .map(([k, label]) => `<button type="button" class="chip" data-hw="${k}" aria-pressed="${view.hw === k}">${k === "all" ? "" : `<i class="sw" style="background:${RM.hwColor(k)}"></i>`}${esc(label)}</button>`).join("");
```

  In the click handler, add `if (el.dataset.hw) { view.hw = el.dataset.hw; save(); renderView(); return; }`, and add `[data-hw]` to the `closest(...)` selector. Add this CSS: `.chip .sw { display:inline-block; width:.7rem; height:.7rem; border-radius:2px; margin-right:.35rem; vertical-align:-.05rem; }`. The reset handler also sets `view.hw = "all"`.

- [ ] **Step 7: Add a highway strip to each milestone card, and fix the links.**
  - In `renderTrack()`, under the meter of each milestone card, add a strip. It has one `<i>` per highway, sized by that highway's share of the milestone's live tasks and colored with `RM.hwColor`:

```js
const strip = (m) => `<span class="hw-strip">${RM.highways.map((h) => { const n = RM.live(RM.tasksOn(m, h.id)).length; return n ? `<i style="flex:${n};background:${RM.hwColor(h.id)}" title="${esc(h.title)}: ${n}"></i>` : ""; }).join("")}</span>`;
```

  - Add this CSS: `.hw-strip { display:flex; gap:2px; height:5px; } .hw-strip i { border-radius:2px; }`.
  - In `renderTasks()`, change `releases.html#v` to `index.html#v`.

- [ ] **Step 8: Make the candidate popup look like a road sign.**
  - `.pop` gets `background: var(--vms)`, `border: 2px solid var(--sign-deep)` and `border-radius: 8px`.
  - `.pop-row.hl` gets `background: var(--sign)`.
  - `.now-lbl` gets `background: var(--accent)`.
  - `.caret` gets `background: var(--accent)`.

- [ ] **Step 9: Check in the browser.**
  - Open `file:///C:/projects/Experiments/completionist/.claude/worktrees/roadmap-map/docs/roadmap/tasks.html`.
  - Check that the milestones, filters (status, highway, area, search, story) and story coverage work, as do opening a task via `#M1.8`, and the decisions, risks and log.
  - Toggle Day/Night.
  - Resize to 375 px wide and check there's no horizontal page scroll.
  - Read the console: it must have no errors.

- [ ] **Step 10: Log line, check, commit**
  - Add to the top of `log` in `roadmap.js`: `{"date": "<today>", "text": "Task board moved to tasks.html and restyled in Evergreen, with highway stripes, a highway filter and highway strips on milestone cards."}`
  - Set `updated` to today.

```bash
python scripts/check_roadmap.py
git add docs/roadmap/tasks.html docs/roadmap/roadmap.js
git commit -m "Roadmap: task board moves to tasks.html, restyled in Evergreen"
```

---

### Task 2: Map geometry: `map-layout.js` with node tests (TDD)

**Files:**
- Create: `scripts/map_layout.test.mjs`, `docs/roadmap/map-layout.js`
- Modify: `docs/roadmap/roadmap.js` (log)

**Interfaces:**
- Produces:
  - `CompletionistMapLayout.layoutMap(data) -> Layout`, which is `window.CompletionistMapLayout` in the page and `module.exports` in node.
  - `Layout = { G, width, height, roadEnd, events, lanes, paths, pieces, barriers, now }`, where:
    - `events[]`: `{ id, kind: "major"|"minor"|"patch"|"now"|"end", version, release, status, fix, lanes: [hwId], x, width, start?, bundle?: {hwId: y}, row?: "gantry"|"A"|"B" }`
    - `lanes[]`: `{ id, y, opensX, opensId }`
    - `paths`: `{ hwId: svgPathD }`
    - `pieces[]`: `{ hw, x0, x1, style: "built"|"active"|"todo"|"idle" }`
    - `barriers[]`: `{ hw, x, y }`
    - `now`: the "now" event
    - `G`: the constants, including `G.rows = { gantry: 24, A: 100, B: 168 }`
  - The station `id` is the release version. The start interchange is `"0.0.0"`.

- [ ] **Step 1: Write the failing tests** in `scripts/map_layout.test.mjs`:

```js
// Tests for the pure highway-map geometry. Run: node --test scripts/map_layout.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const { layoutMap } = require("../docs/roadmap/map-layout.js");

// Three highways (b opens at 1.0.0), three milestones, six releases including a planned fix.
const fixture = () => ({
  highways: [
    { id: "e", title: "E", areas: ["engine"] },
    { id: "t", title: "T", areas: ["dll"] },
    { id: "b", title: "B", areas: ["extension"], opens: "1.0.0" },
  ],
  milestones: [
    { id: "M0", tasks: [{ id: "M0.1", status: "done", area: "engine" }, { id: "M0.2", status: "done", area: "dll" }] },
    { id: "M1", tasks: [
      { id: "M1.1", status: "done", area: "engine" }, { id: "M1.2", status: "doing", area: "engine" },
      { id: "M1.3", status: "todo", area: "dll" }, { id: "M1.4", status: "dropped", area: "dll" },
    ] },
    { id: "M2", tasks: [{ id: "M2.1", status: "next", area: "extension" }] },
  ],
  releases: [
    { version: "0.0.1", status: "released", highways: ["e"] },
    { version: "0.0.2", status: "released", highways: ["t"], milestone: "M0" },
    { version: "0.0.3", status: "planned", highways: ["e"], kind: "fix" },
    { version: "0.1.0", status: "next", milestone: "M1" },
    { version: "1.0.0", status: "planned" },
    { version: "1.1.0", status: "planned", milestone: "M2" },
  ],
});
const ev = (L, id) => L.events.find((e) => e.id === id);
const piecesOf = (L, hw) => L.pieces.filter((p) => p.hw === hw).map((p) => [p.x0, p.x1, p.style]);

test("events: start interchange, releases in order, now after the last released, then the end", () => {
  const L = layoutMap(fixture());
  assert.deepEqual(L.events.map((e) => `${e.kind}:${e.id}`), [
    "major:0.0.0", "patch:0.0.1", "patch:0.0.2", "now:now", "patch:0.0.3", "minor:0.1.0", "major:1.0.0", "minor:1.1.0", "end:end",
  ]);
  assert.equal(L.now.version, "0.0.2");
  assert.equal(ev(L, "0.0.0").start, true);
  assert.equal(ev(L, "0.0.3").fix, true);
  assert.equal(ev(L, "0.0.1").fix, false);
});

test("columns: each event sits in the middle of its column", () => {
  const L = layoutMap(fixture());
  assert.deepEqual(L.events.map((e) => e.x), [300, 462, 566, 678, 790, 927, 1122, 1317, 1482]);
  assert.equal(L.width, 1562);
  assert.equal(L.roadEnd, 1522);
  assert.equal(L.height, 622);
});

test("lanes: patches name theirs, minors take their milestone's live tasks, majors take every open highway", () => {
  const L = layoutMap(fixture());
  assert.deepEqual(ev(L, "0.0.2").lanes, ["t"]);
  assert.deepEqual(ev(L, "0.1.0").lanes, ["e", "t"]);
  assert.deepEqual(ev(L, "1.0.0").lanes, ["e", "t", "b"]);
  assert.deepEqual(ev(L, "1.1.0").lanes, ["b"]);
  assert.deepEqual(ev(L, "0.0.0").lanes, ["e", "t"]);
  assert.deepEqual(L.lanes.map((l) => [l.id, l.y, l.opensX]), [["e", 250, 300], ["t", 366, 300], ["b", 482, 1122]]);
});

test("interchanges bundle the open highways around their middle", () => {
  const L = layoutMap(fixture());
  assert.deepEqual(ev(L, "0.0.0").bundle, { e: 299, t: 317 });
  assert.deepEqual(ev(L, "1.0.0").bundle, { e: 348, t: 366, b: 384 });
});

test("paths: every highway leaves its opening interchange, passes through later ones, and runs to the end", () => {
  const L = layoutMap(fixture());
  assert.equal(L.paths.e, "M300 299 H312 C352 299 352 250 392 250 H1030 C1070 250 1070 348 1110 348 H1134 C1174 348 1174 250 1214 250 H1522");
  assert.ok(L.paths.b.startsWith("M1122 384 H1134 C1174 384 1174 482 1214 482"));
  assert.ok(L.paths.b.endsWith(" H1522"));
});

test("pieces: each stretch follows its milestone's progress on that highway", () => {
  const L = layoutMap(fixture());
  assert.deepEqual(piecesOf(L, "e"), [[300, 566, "built"], [566, 746.5, "built"], [746.5, 927, "active"], [927, 1122, "todo"], [1122, 1317, "idle"], [1317, 1522, "idle"]]);
  assert.deepEqual(piecesOf(L, "t"), [[300, 566, "built"], [566, 927, "todo"], [927, 1122, "todo"], [1122, 1317, "idle"], [1317, 1522, "idle"]]);
  assert.deepEqual(piecesOf(L, "b"), [[1122, 1317, "active"], [1317, 1522, "idle"]]);
});

test("barricades mark where construction starts, but not on a station or inside a curve", () => {
  const L = layoutMap(fixture());
  assert.deepEqual(L.barriers, [{ hw: "e", x: 746.5, y: 250 }]);
});

test("sign rows: majors on the gantry, minors alternate between rows A and B", () => {
  const L = layoutMap(fixture());
  assert.deepEqual(L.events.filter((e) => e.row).map((e) => `${e.id}:${e.row}`), ["0.0.0:gantry", "0.1.0:A", "1.0.0:gantry", "1.1.0:B"]);
});

test("the input data is not modified", () => {
  const data = fixture();
  const before = JSON.stringify(data);
  layoutMap(data);
  assert.equal(JSON.stringify(data), before);
});
```

- [ ] **Step 2: Run the tests to watch them fail**

Run: `node --test scripts/map_layout.test.mjs`
Expected: FAIL with `Cannot find module '../docs/roadmap/map-layout.js'`.

- [ ] **Step 3: Write `docs/roadmap/map-layout.js`**

```js
// Pure geometry for the roadmap highway map: no DOM, no styling.
// Used by map.js in the page (window.CompletionistMapLayout) and by scripts/map_layout.test.mjs under node.
(function (root, factory) {
  if (typeof module === "object" && module.exports) module.exports = factory();
  else root.CompletionistMapLayout = factory();
})(typeof self !== "undefined" ? self : this, function () {
  const G = {
    left: 190, laneTop: 250, laneGap: 116, bundleGap: 18, curve: 80, joint: 12, bottom: 140, tail: 40,
    col: { major: 220, minor: 170, patch: 104, now: 120, end: 160 },
    rows: { gantry: 24, A: 100, B: 168 },
  };
  const parse = (v) => String(v).split(".").map(Number);
  const cmp = (a, b) => { const x = parse(a), y = parse(b); return x[0] - y[0] || x[1] - y[1] || x[2] - y[2]; };
  const tier = (v) => { const [, minor, patch] = parse(v); return patch ? "patch" : minor ? "minor" : "major"; };
  const ACTIVE = new Set(["doing", "next", "blocked"]);

  function layoutMap(data) {
    const highways = data.highways;
    const hwOfArea = {};
    for (const h of highways) for (const a of h.areas) hwOfArea[a] = h.id;
    const milestones = Object.fromEntries(data.milestones.map((m) => [m.id, m]));
    const isOpen = (h, v) => !h.opens || cmp(v, h.opens) >= 0;
    const live = (m) => m.tasks.filter((t) => t.status !== "dropped");

    function lanesOf(r) {
      if (Array.isArray(r.highways) && r.highways.length) return highways.filter((h) => r.highways.includes(h.id)).map((h) => h.id);
      if (tier(r.version) === "major") return highways.filter((h) => isOpen(h, r.version)).map((h) => h.id);
      const m = r.milestone && milestones[r.milestone];
      const used = new Set(m ? live(m).map((t) => hwOfArea[t.area]) : []);
      return highways.filter((h) => used.has(h.id)).map((h) => h.id);
    }

    // 1. Events from left to right: the start interchange, every release, "now" after the last released one, the end.
    const releases = data.releases.slice().sort((a, b) => cmp(a.version, b.version));
    const events = [{ id: "0.0.0", kind: "major", version: "0.0.0", start: true, release: null, status: "released", fix: false,
      lanes: highways.filter((h) => isOpen(h, "0.0.0")).map((h) => h.id) }];
    for (const r of releases) {
      events.push({ id: r.version, kind: tier(r.version), version: r.version, release: r, status: r.status, fix: r.kind === "fix", lanes: lanesOf(r) });
    }
    let last = 0;
    events.forEach((e, i) => { if (e.release && e.status === "released") last = i; });
    events.splice(last + 1, 0, { id: "now", kind: "now", version: last ? events[last].version : null });
    events.push({ id: "end", kind: "end" });

    // 2. Columns.
    let x = G.left;
    for (const e of events) { e.width = G.col[e.kind]; e.x = x + e.width / 2; x += e.width; }
    const width = x;
    const roadEnd = width - G.tail;

    // 3. Lanes, and each interchange's bundle of open highways.
    const laneY = Object.fromEntries(highways.map((h, i) => [h.id, G.laneTop + i * G.laneGap]));
    const majors = events.filter((e) => e.kind === "major");
    for (const e of majors) {
      const open = highways.filter((h) => isOpen(h, e.version));
      const mid = open.reduce((s, h) => s + laneY[h.id], 0) / open.length;
      e.bundle = Object.fromEntries(open.map((h, k) => [h.id, mid + (k - (open.length - 1) / 2) * G.bundleGap]));
    }
    const lanes = highways.map((h) => {
      const at = h.opens ? majors.find((e) => e.version === h.opens) : majors[0];
      return { id: h.id, y: laneY[h.id], opensX: at.x, opensId: at.id };
    });

    // 4. One path per highway: out of its opening interchange, through every later interchange, to the end.
    const J = G.joint, C = G.curve;
    const paths = {};
    for (const lane of lanes) {
      const y = lane.y;
      const first = majors.find((e) => e.id === lane.opensId);
      const b0 = first.bundle[lane.id];
      let d = `M${first.x} ${b0} H${first.x + J} C${first.x + J + C / 2} ${b0} ${first.x + J + C / 2} ${y} ${first.x + J + C} ${y}`;
      for (const e of majors) {
        if (e.x <= first.x) continue;
        const b = e.bundle[lane.id];
        d += ` H${e.x - J - C} C${e.x - J - C / 2} ${y} ${e.x - J - C / 2} ${b} ${e.x - J} ${b}`
          + ` H${e.x + J} C${e.x + J + C / 2} ${b} ${e.x + J + C / 2} ${y} ${e.x + J + C} ${y}`;
      }
      paths[lane.id] = d + ` H${roadEnd}`;
    }

    // 5. Road pieces: the stretch between two anchors belongs to the milestone at its right-hand end.
    const anchors = events.filter((e) => e.kind === "major" || e.kind === "end" || (e.release && e.release.milestone));
    const pieces = [], barriers = [];
    for (let i = 1; i < anchors.length; i++) {
      const a = anchors[i - 1], b = anchors[i];
      const m = b.release && b.release.milestone ? milestones[b.release.milestone] : null;
      for (const lane of lanes) {
        const x0 = Math.max(a.x, lane.opensX), x1 = b.kind === "end" ? roadEnd : b.x;
        if (x1 <= x0) continue;
        const own = m ? live(m).filter((t) => hwOfArea[t.area] === lane.id) : [];
        if (!own.length) {
          const style = b.status === "released" ? "built" : b.kind === "major" ? "todo" : "idle";
          pieces.push({ hw: lane.id, x0, x1, style });
          continue;
        }
        const span = x1 - x0;
        const xd = x0 + span * own.filter((t) => t.status === "done").length / own.length;
        const xa = xd + span * own.filter((t) => ACTIVE.has(t.status)).length / own.length;
        if (xd > x0) pieces.push({ hw: lane.id, x0, x1: xd, style: "built" });
        if (xa > xd) {
          pieces.push({ hw: lane.id, x0: xd, x1: xa, style: "active" });
          const clearOfCurve = a.kind !== "major" || xd >= a.x + J + C;
          if (xd > x0 + 14 && clearOfCurve) barriers.push({ hw: lane.id, x: xd, y: lane.y });
        }
        if (x1 > xa) pieces.push({ hw: lane.id, x0: xa, x1, style: "todo" });
      }
    }

    // 6. Sign rows: majors on the gantry row, minors alternate between exit rows A and B.
    let n = 0;
    for (const e of events) {
      if (e.kind === "major") e.row = "gantry";
      else if (e.kind === "minor") e.row = n++ % 2 ? "B" : "A";
    }

    const height = G.laneTop + (highways.length - 1) * G.laneGap + G.bottom;
    return { G, width, height, roadEnd, events, lanes, paths, pieces, barriers, now: events.find((e) => e.kind === "now") };
  }

  return { layoutMap, G };
});
```

- [ ] **Step 4: Run the tests to watch them pass**

Run: `node --test scripts/map_layout.test.mjs`
Expected: all 9 tests pass. If a number is off, fix the code, not the test. The test numbers were worked out by hand from the spec's constants.

- [ ] **Step 5: Run it against the real data**

```bash
node -e "global.window={};require('./docs/roadmap/roadmap.js');const L=require('./docs/roadmap/map-layout.js').layoutMap(window.COMPLETIONIST_ROADMAP);console.log(L.width,L.height,L.events.map(e=>e.id).join(' '))"
```

Expected: the width is about 2,200 or more. The events are `0.0.0 0.0.1 0.0.2 0.0.3 0.0.4 0.0.5 now 0.1.0 0.2.0 0.3.0 0.4.0 1.0.0 1.1.0 1.2.0 end` (more if releases were added since).

- [ ] **Step 6: Log line, check, commit**
  - Log: `"Map geometry (map-layout.js) with node tests: events, lanes, interchange bundles, highway paths, road pieces by milestone progress, barricades, sign rows."`
  - Set `updated` to today.

```bash
python scripts/check_roadmap.py
git add scripts/map_layout.test.mjs docs/roadmap/map-layout.js docs/roadmap/roadmap.js
git commit -m "Roadmap map: pure layout with node tests"
```

---

### Task 3: Draw the map (`map.js`) inside a first `index.html`

**Files:**
- Create: `docs/roadmap/map.js`, `docs/roadmap/index.html`
- Modify: `docs/roadmap/roadmap.js` (log)

**Interfaces:**
- Consumes: `CompletionistMapLayout.layoutMap` (Task 2) and `window.RM` (`esc`, `hwById`, `RELEASE_STATUS`, `tierOf`).
- Produces: `window.CompletionistMap.mount(frameEl, layout, { onSelect(id) })`, which returns `{ select(id), spotlight(hwId|null), scrollToX(x, smooth), fit(on), redraw(layout), xOf(id) }`. `onSelect` fires with the station id (a version, or `"0.0.0"`) when a station is clicked or activated from the keyboard. `select` only marks the station; it doesn't call `onSelect`.

- [ ] **Step 1: Create `index.html` with the map frame only.** Page sections come in Task 4. Use this structure:

```html
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Completionist Roadmap</title>
<!-- the three Google Fonts lines from the spec -->
<link rel="stylesheet" href="theme.css">
<style>/* page CSS: Step 3 below, then Task 4 */</style>
</head>
<body>
<header class="wrap topbar"><!-- same top bar as tasks.html, but Map is aria-current --></header>
<main>
  <section class="map-sec" aria-labelledby="map-h">
    <div class="wrap sec-head">
      <div class="sec-title"><span class="lbl">Every release, every part of the app</span><h2 id="map-h">The <em>highway</em></h2></div>
      <div class="map-ctl">
        <button type="button" class="ctl" id="go-start">⇤ Start</button>
        <button type="button" class="ctl ctl-now" id="go-now">You are here</button>
        <button type="button" class="ctl" id="go-end">Future ⇥</button>
        <button type="button" class="ctl" id="fit" aria-pressed="false">Whole map</button>
      </div>
    </div>
    <div class="wrap"><div class="hw-chips" id="hw-chips" aria-label="Spotlight a highway"></div></div>
    <div class="map-frame" id="map-frame">
      <div class="map-scroll" id="map-scroll" tabindex="0" role="region" aria-label="Highway map of every release. Scrolls sideways.">
        <div class="map-inner" id="map-inner">
          <div class="lane-tags" id="lane-tags" aria-hidden="true"></div>
          <svg id="map" role="group" aria-label="Releases on the highway map"></svg>
        </div>
      </div>
    </div>
  </section>
</main>
<script src="roadmap.js"></script>
<script src="common.js"></script>
<script src="map-layout.js"></script>
<script src="map.js"></script>
<script>
  RM.bindThemeButton(document.getElementById("theme"));
  const L = CompletionistMapLayout.layoutMap(RM.data);
  const map = CompletionistMap.mount(document.getElementById("map-frame"), L, { onSelect: (id) => map.select(id) });
  document.fonts.ready.then(() => map.redraw(CompletionistMapLayout.layoutMap(RM.data)));
</script>
</body>
</html>
```

  In Task 4, the inline script is replaced by `page.js`.

- [ ] **Step 2: Write `docs/roadmap/map.js`.** Every color goes through `style=` with CSS variables, so changing the theme needs no redraw.

```js
// Draws the roadmap highway map from CompletionistMapLayout.layoutMap(data) and handles selection, spotlight, panning and fit.
// Exposes window.CompletionistMap.mount(frameEl, layout, { onSelect }).
(() => {
  let ctx;
  const width = (text, font) => { ctx ||= document.createElement("canvas").getContext("2d"); ctx.font = font; return ctx.measureText(text).width; };
  function wrap(text, font, max, lines = 2) {
    const out = []; let cur = "";
    for (const w of String(text).split(/\s+/).filter(Boolean)) {
      const next = cur ? `${cur} ${w}` : w;
      if (!cur || width(next, font) <= max) cur = next; else { out.push(cur); cur = w; }
    }
    if (cur) out.push(cur);
    if (out.length > lines) { out.length = lines; out[lines - 1] = out[lines - 1].replace(/\s*\S+$/, "") + "…"; }
    return out;
  }
  const esc = (s) => window.RM.esc(s);
  const col = (id) => `var(--hw-${id})`;
  const F = {
    sign: "800 14px Overpass", tab: "900 10.5px Overpass", gantry: "800 15px Overpass", small: "800 10.5px Overpass",
    ver: "700 12px 'Overpass Mono'", label: "600 11.5px Overpass",
  };
  const T = (x, y, text, style, attrs = "") => `<text x="${x}" y="${y}" style="${style}" ${attrs}>${esc(text)}</text>`;
  const CHECK = (x, y, s = 1) => `<path d="M${x - 3.6 * s} ${y + .2 * s}l${2.6 * s} ${2.8 * s}l${4.8 * s} -${5.6 * s}" style="fill:none;stroke:var(--sign-ink);stroke-width:${2.2 * s};stroke-linecap:round;stroke-linejoin:round"/>`;

  function laneY(L, id) { return L.lanes.find((l) => l.id === id).y; }

  function drawRoads(L) {
    let defs = "", out = "";
    for (const lane of L.lanes) {
      defs += `<pattern id="hatch-${lane.id}" width="12" height="12" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><rect width="12" height="12" style="fill:var(--surface)"/><rect width="6" height="12" style="fill:${col(lane.id)}"/></pattern>`;
      out += `<path class="casing" data-hw="${lane.id}" d="${L.paths[lane.id]}" style="fill:none;stroke:var(--surface);stroke-width:16;stroke-linecap:round;stroke-linejoin:round"/>`;
    }
    L.pieces.forEach((p, i) => {
      defs += `<clipPath id="clip-${i}"><rect x="${p.x0}" y="0" width="${p.x1 - p.x0}" height="${L.height}"/></clipPath>`;
      const base = `d="${L.paths[p.hw]}" clip-path="url(#clip-${i})"`;
      const c = col(p.hw);
      const road = {
        built: `<path ${base} style="fill:none;stroke:${c};stroke-width:10"/><path ${base} style="fill:none;stroke:var(--surface);stroke-width:1.6;stroke-dasharray:10 12;opacity:.9"/>`,
        active: `<path ${base} style="fill:none;stroke:url(#hatch-${p.hw});stroke-width:10"/>`,
        todo: `<path ${base} style="fill:none;stroke:${c};stroke-width:8;stroke-dasharray:12 9;opacity:.35"/>`,
        idle: `<path ${base} style="fill:none;stroke:${c};stroke-width:3;stroke-dasharray:1 9;stroke-linecap:round;opacity:.35"/>`,
      }[p.style];
      out += `<g class="road ${p.style}" data-hw="${p.hw}">${road}</g>`;
    });
    for (const b of L.barriers) {
      out += `<g class="barrier" data-hw="${b.hw}" transform="translate(${b.x} ${b.y})"><rect x="-4" y="-17" width="8" height="34" rx="2" style="fill:var(--surface);stroke:var(--ink);stroke-width:1.5"/><path d="M-4 -11l8 -5M-4 -1l8 -5M-4 9l8 -5" style="stroke:var(--accent);stroke-width:3"/></g>`;
    }
    return { defs, out };
  }

  function stateStyle(status, fillReleased) {
    if (status === "released") return `fill:${fillReleased};stroke:var(--surface);stroke-width:3`;
    if (status === "next") return "fill:var(--surface);stroke:var(--accent);stroke-width:4";
    return "fill:var(--surface);stroke:var(--ghost);stroke-width:2.2;stroke-dasharray:4 3";
  }
  const halo = (shape) => `<g class="halo">${shape}</g>`;

  function drawStation(L, e) {
    const R = window.RM;
    const kindName = e.start ? "start of the 0.x line" : `${e.kind} release`;
    const title = e.start ? "Groundwork and preview" : e.release.title;
    const label = `Version ${e.start ? "0.x" : e.version}, ${kindName}, ${title}, ${e.start ? "started" : (R.RELEASE_STATUS[e.status] || e.status).toLowerCase()}`;
    let body = "", hit, tagsBelow = "";
    const hws = (e.lanes.length ? e.lanes : L.lanes.map((l) => l.id));
    const ys = hws.map((id) => (e.bundle && e.bundle[id] != null ? e.bundle[id] : laneY(L, id)));
    const top = Math.min(...ys), bottom = Math.max(...ys), x = e.x;

    if (e.kind === "major") {
      const w = 44, y0 = top - 18, h = bottom - top + 36;
      const cap = `<rect x="${x - w / 2}" y="${y0}" width="${w}" height="${h}" rx="${w / 2}"`;
      if (e.status === "next") body += halo(`${cap} style="fill:none;stroke:var(--accent);stroke-width:4"/>`);
      body += `${cap} style="${e.status === "released" ? "fill:var(--sign);stroke:var(--sign-ink);stroke-width:3" : e.status === "next" ? "fill:var(--surface);stroke:var(--accent);stroke-width:4" : "fill:var(--surface);stroke:var(--ink);stroke-width:3;stroke-dasharray:6 4"}"/>`;
      hws.forEach((id, k) => { body += `<circle cx="${x}" cy="${ys[k]}" r="4.5" style="fill:${col(id)};stroke:var(--surface);stroke-width:2"/>`; });
      // Shield in the gap above the bundle.
      const sy = top - 104;
      body += `<g transform="translate(${x - 28} ${sy})"><path d="M28 0 56 10v20c0 16-12 26-28 32C12 56 0 46 0 30V10Z" style="fill:var(--sign);stroke:var(--sign-ink);stroke-width:3"/><path d="M4 12h48" style="stroke:var(--sign-ink);stroke-width:2"/>${T(28, 44, e.start ? "0" : String(R.parseVer(e.version)[0]), "fill:var(--sign-ink);font:900 26px Overpass", 'text-anchor="middle"')}</g>`;
      // Overhead gantry sign.
      const line1 = e.start ? "START · 0.x" : `MAJOR · ${e.version.replace(/\.0$/, "")}`;
      const w2 = Math.max(width(title, F.gantry) + 44, 190), gy = L.G.rows.gantry;
      body += `<g class="gantry"><path d="M${x} ${gy + 52} V${sy}" style="stroke:var(--ink);stroke-width:2"/><rect x="${x - w2 / 2}" y="${gy}" width="${w2}" height="52" rx="9" style="fill:var(--sign)"/><rect x="${x - w2 / 2 + 5}" y="${gy + 5}" width="${w2 - 10}" height="42" rx="6" style="fill:none;stroke:var(--sign-ink);stroke-width:2"/>${T(x, gy + 21, line1, `fill:var(--sign-dim);font:${F.small};letter-spacing:.14em`, 'text-anchor="middle"')}${T(x, gy + 40, title, `fill:var(--sign-ink);font:${F.gantry}`, 'text-anchor="middle"')}</g>`;
      hit = [x - w / 2 - 8, y0 - 8, w + 16, h + 16];
    } else if (e.kind === "minor") {
      const w = 26, y0 = top - 20, h = bottom - top + 40;
      const cap = `<rect x="${x - w / 2}" y="${y0}" width="${w}" height="${h}" rx="${w / 2}"`;
      if (e.status === "next") body += halo(`${cap} style="fill:none;stroke:var(--accent);stroke-width:4"/>`);
      body += `${cap} style="${stateStyle(e.status, "var(--ink)")}"/>`;
      hws.forEach((id, k) => { body += `<circle cx="${x}" cy="${ys[k]}" r="5.5" style="fill:${col(id)};stroke:var(--surface);stroke-width:2"/>`; });
      // Exit sign with its tab and post.
      const minor = R.parseVer(e.version);
      const tab = `EXIT ${minor[0]}.${minor[1]}`;
      const sw = Math.max(width(e.release.title, F.sign) + 28, 96), sy = L.G.rows[e.row], tw = width(tab, F.tab) + 18;
      body += `<g class="exit"><path d="M${x} ${sy + 44} V${y0}" style="stroke:var(--ink);stroke-width:2"/><rect x="${x - sw / 2}" y="${sy - 16}" width="${tw}" height="20" rx="5" style="fill:var(--sign)"/>${T(x - sw / 2 + 9, sy - 2, tab, `fill:var(--sign-ink);font:${F.tab};letter-spacing:.12em`)}<rect x="${x - sw / 2}" y="${sy}" width="${sw}" height="44" rx="8" style="fill:var(--sign)"/><rect x="${x - sw / 2 + 4}" y="${sy + 4}" width="${sw - 8}" height="36" rx="5" style="fill:none;stroke:var(--sign-ink);stroke-width:1.8"/>${T(x, sy + 27, e.release.title, `fill:var(--sign-ink);font:${F.sign}`, 'text-anchor="middle"')}</g>`;
      if (e.status === "next") body += T(x, bottom + 38, "NEXT", "fill:var(--accent-ink);font:900 11px Overpass;letter-spacing:.14em", 'text-anchor="middle"');
      hit = [x - w / 2 - 8, y0 - 8, w + 16, h + 16];
    } else if (e.fix) {
      const y = ys[0], by = y + 46;
      const released = e.status === "released";
      body += `<path d="M${x} ${y} V${by - 11}" style="stroke:${col(hws[0])};stroke-width:5${released ? "" : ";stroke-dasharray:4 3;opacity:.6"}"/>`;
      body += `<circle cx="${x}" cy="${by}" r="11" style="${stateStyle(e.status, col(hws[0]))}"/>${released ? CHECK(x, by) : ""}`;
      tagsBelow = [by + 26, by + 40];
      hit = [x - 18, y - 6, 36, 66];
    } else {
      if (hws.length === 1) {
        const y = ys[0];
        if (e.status === "next") body += halo(`<circle cx="${x}" cy="${y}" r="10" style="fill:none;stroke:var(--accent);stroke-width:4"/>`);
        body += `<circle cx="${x}" cy="${y}" r="9" style="${stateStyle(e.status, col(hws[0]))}"/>${e.status === "released" ? CHECK(x, y) : ""}`;
      } else {
        const y0 = top - 14, h = bottom - top + 28;
        body += `<rect x="${x - 10}" y="${y0}" width="20" height="${h}" rx="10" style="${stateStyle(e.status, "var(--ink)")}"/>`;
        hws.forEach((id, k) => { body += `<circle cx="${x}" cy="${ys[k]}" r="4.5" style="fill:${col(id)};stroke:var(--surface);stroke-width:2"/>`; });
      }
      tagsBelow = [bottom + 30, bottom + 44];
      hit = [x - 16, top - 16, 32, bottom - top + 32];
    }

    if (tagsBelow) {
      const [vy, ty] = tagsBelow;
      body += T(x, vy, e.fix ? `${e.version} fix` : e.version, `fill:var(--ink);font:${F.ver}`, 'text-anchor="middle"');
      wrap(e.release.title, F.label, L.G.col.patch - 14).forEach((line, k) => { body += T(x, ty + k * 14, line, `fill:var(--muted);font:${F.label}`, 'text-anchor="middle"'); });
    }
    const [hx, hy, hw, hh] = hit;
    return `<g class="stn" data-id="${esc(e.id)}" data-hws="${hws.join(" ")}" role="button" tabindex="0" aria-label="${esc(label)}">${body}<rect class="hit" x="${hx}" y="${hy}" width="${hw}" height="${hh}" rx="10" style="fill:transparent;stroke:transparent"/></g>`;
  }

  function drawNow(L) {
    const n = L.now, x = n.x, lastY = L.lanes[L.lanes.length - 1].y;
    return `<g class="now"><path d="M${x} ${L.G.laneTop - 34} V${lastY + 34}" style="stroke:var(--accent);stroke-width:4;stroke-linecap:round"/>
      <g class="pin" transform="translate(${x} ${lastY + 62})"><circle class="pin-pulse" r="16" style="fill:var(--accent);opacity:.25"/><path d="M0 12C-8 2-12-4-12-10a12 12 0 0 1 24 0c0 6-4 12-12 22Z" transform="translate(0 -14)" style="fill:var(--accent);stroke:var(--surface);stroke-width:2"/><circle cy="-24" r="4.5" style="fill:var(--surface)"/></g>
      ${T(x, lastY + 96, "YOU ARE HERE", "fill:var(--accent-ink);font:900 11px Overpass;letter-spacing:.14em", 'text-anchor="middle"')}
      ${n.version ? T(x, lastY + 111, `${n.version} shipped`, "fill:var(--muted);font:600 11px 'Overpass Mono'", 'text-anchor="middle"') : ""}</g>`;
  }

  function drawLaneLabels(L) {
    return L.lanes.map((lane) => {
      const h = window.RM.hwById[lane.id];
      const blurb = wrap(h.opens ? `${h.blurb}. Opens at ${h.opens.replace(/\.0$/, "")}.` : h.blurb, "600 11px Overpass", 158, 2);
      return `<g class="lane-label" data-hw="${lane.id}"><rect x="16" y="${lane.y - 20}" width="158" height="40" rx="20" style="fill:${col(lane.id)}"/>${T(34, lane.y + 5, h.title.toUpperCase(), "fill:var(--sign-ink);font:italic 900 14px Overpass;letter-spacing:.04em")}${blurb.map((b, k) => T(18, lane.y + 36 + k * 13, b, "fill:var(--muted);font:600 11px Overpass")).join("")}</g>`;
    }).join("");
  }

  function drawEnd(L) {
    const x0 = L.roadEnd - 140;
    return `<defs><linearGradient id="fade" x1="0" x2="1"><stop offset="0" style="stop-color:var(--paper-2);stop-opacity:0"/><stop offset="1" style="stop-color:var(--paper-2);stop-opacity:1"/></linearGradient></defs>
      <rect x="${x0}" y="0" width="${L.width - x0}" height="${L.height}" style="fill:url(#fade)"/>
      ${T(L.roadEnd - 60, L.G.laneTop - 40, "more to come", "fill:var(--ghost);font:italic 400 15px Fraunces", 'text-anchor="middle"')}`;
  }

  function draw(svg, L) {
    const roads = drawRoads(L);
    const stations = L.events.filter((e) => e.kind === "major" || e.kind === "minor" || e.kind === "patch").map((e) => drawStation(L, e)).join("");
    svg.setAttribute("viewBox", `0 0 ${L.width} ${L.height}`);
    svg.setAttribute("width", L.width);
    svg.setAttribute("height", L.height);
    svg.innerHTML = `<defs>${roads.defs}</defs>${drawLaneLabels(L)}${roads.out}${drawNow(L)}${stations}${drawEnd(L)}`;
  }

  function mount(frame, layout, { onSelect } = {}) {
    let L = layout, selected = null, spot = null;
    const scroller = frame.querySelector(".map-scroll"), inner = frame.querySelector(".map-inner");
    const tags = frame.querySelector(".lane-tags"), svg = frame.querySelector("svg");
    const reduced = matchMedia("(prefers-reduced-motion: reduce)").matches;

    function paintTags() {
      tags.style.height = `${L.height}px`;
      tags.innerHTML = L.lanes.map((lane) => {
        const h = window.RM.hwById[lane.id];
        return `<span class="lane-tag" style="top:${lane.y - 17}px;--hw:${col(lane.id)}">${esc(h.title)}</span>`;
      }).join("");
    }
    function render() {
      inner.style.width = `${L.width}px`;
      inner.style.height = `${L.height}px`;
      draw(svg, L);
      paintTags();
      if (selected) api.select(selected);
      if (spot) api.spotlight(spot);
    }
    const api = {
      select(id) {
        selected = id;
        svg.querySelectorAll(".stn").forEach((g) => g.classList.toggle("on", g.dataset.id === id));
      },
      spotlight(hw) {
        spot = hw;
        svg.querySelectorAll("[data-hw]").forEach((el) => el.classList.toggle("dim", !!hw && el.dataset.hw !== hw));
        svg.querySelectorAll(".stn").forEach((el) => el.classList.toggle("dim", !!hw && !el.dataset.hws.split(" ").includes(hw)));
      },
      xOf(id) { const e = L.events.find((v) => v.id === id); return e ? e.x : 0; },
      scrollToX(x, smooth = true) { scroller.scrollTo({ left: Math.max(0, x - scroller.clientWidth * 0.4), behavior: smooth && !reduced ? "smooth" : "auto" }); },
      fit(on) { frame.classList.toggle("fit", on); },
      redraw(next) { L = next; render(); },
    };

    svg.addEventListener("click", (ev) => { const g = ev.target.closest(".stn"); if (g && !dragMoved) onSelect && onSelect(g.dataset.id); });
    svg.addEventListener("keydown", (ev) => {
      const g = ev.target.closest(".stn");
      if (g && (ev.key === "Enter" || ev.key === " ")) { ev.preventDefault(); onSelect && onSelect(g.dataset.id); }
    });
    scroller.addEventListener("scroll", () => tags.classList.toggle("show", scroller.scrollLeft > 40), { passive: true });
    scroller.addEventListener("keydown", (ev) => {
      if (ev.target !== scroller) return;
      if (ev.key === "ArrowRight") { ev.preventDefault(); scroller.scrollBy({ left: 240, behavior: reduced ? "auto" : "smooth" }); }
      if (ev.key === "ArrowLeft") { ev.preventDefault(); scroller.scrollBy({ left: -240, behavior: reduced ? "auto" : "smooth" }); }
    });
    // Drag to pan with a mouse.
    let drag = null, dragMoved = false;
    scroller.addEventListener("pointerdown", (ev) => {
      if (ev.pointerType !== "mouse" || ev.button !== 0 || ev.target.closest("a, button")) return;
      drag = { x: ev.clientX, left: scroller.scrollLeft }; dragMoved = false;
    });
    addEventListener("pointermove", (ev) => {
      if (!drag) return;
      const dx = ev.clientX - drag.x;
      if (!dragMoved && Math.abs(dx) > 4) { dragMoved = true; scroller.classList.add("dragging"); }
      if (dragMoved) scroller.scrollLeft = drag.left - dx;
    });
    addEventListener("pointerup", () => { drag = null; scroller.classList.remove("dragging"); setTimeout(() => { dragMoved = false; }, 0); });

    render();
    return api;
  }

  window.CompletionistMap = { mount };
})();
```

- [ ] **Step 3: Add the map CSS** to the page `<style>` in `index.html`:

```css
.map-sec { display: grid; gap: 1rem; padding-block: 1rem 0; }
.map-ctl { display: flex; flex-wrap: wrap; gap: .4rem; }
.ctl { font-weight: 800; font-size: var(--fs-xs); letter-spacing: .1em; text-transform: uppercase; padding: .5rem .85rem .4rem; border-radius: 999px; border: 2px solid var(--ink); background: var(--surface); cursor: pointer; }
.ctl:hover { background: var(--ink); color: var(--paper); }
.ctl[aria-pressed="true"] { background: var(--sign); color: var(--sign-ink); border-color: var(--sign); }
.ctl-now { border-color: var(--accent); color: var(--accent-ink); }
.hw-chips { display: flex; flex-wrap: wrap; gap: .4rem; }
.hw-chip { display: inline-flex; align-items: center; gap: .45rem; font-weight: 900; font-style: italic; font-size: var(--fs-s); text-transform: uppercase; letter-spacing: .04em; padding: .4rem .8rem .3rem; border-radius: 999px; border: 2px solid var(--hw); color: var(--ink); background: var(--surface); cursor: pointer; }
.hw-chip i { width: 14px; height: 14px; border-radius: 50%; background: var(--hw); }
.hw-chip[aria-pressed="true"] { background: var(--hw); color: var(--sign-ink); }
.map-frame { position: relative; margin-inline: 16px; border-radius: 18px; border: 2px solid var(--line-strong); overflow: hidden;
  background: linear-gradient(90deg, transparent 32.9%, rgb(0 0 0 / .045) 33.3%, transparent 33.8%, transparent 66.2%, rgb(0 0 0 / .045) 66.6%, transparent 67%), var(--paper-2);
  box-shadow: 0 30px 60px -40px var(--shadow), inset 0 0 0 6px var(--surface); }
@media (min-width: 760px) { .map-frame { margin-inline: 32px; } }
.map-scroll { overflow-x: auto; overflow-y: hidden; overscroll-behavior-x: contain; cursor: grab; }
.map-scroll.dragging { cursor: grabbing; user-select: none; }
.map-scroll:focus-visible { outline-offset: -4px; }
.map-inner { position: relative; }
.map-inner svg { position: absolute; inset: 0; overflow: visible; }
.lane-tags { position: sticky; left: 0; width: 150px; z-index: 2; pointer-events: none; opacity: 0; transition: opacity .2s; }
.lane-tags.show { opacity: 1; }
.lane-tag { position: absolute; left: 10px; background: var(--hw); color: var(--sign-ink); font-weight: 900; font-style: italic; font-size: var(--fs-xs); text-transform: uppercase; letter-spacing: .04em; padding: .45rem .75rem .35rem; border-radius: 999px; box-shadow: 0 8px 18px -8px var(--shadow), 0 0 0 3px var(--surface); white-space: nowrap; }
.fit .map-scroll { overflow: hidden; cursor: default; }
.fit .map-inner { width: 100% !important; height: auto !important; }
.fit .map-inner svg { position: static; width: 100%; height: auto; }
.fit .lane-tags { display: none; }
.stn { cursor: pointer; outline: none; }
.stn .hit { transition: stroke .15s; }
.stn:hover .hit, .stn:focus-visible .hit { stroke: var(--ink); stroke-width: 2; stroke-dasharray: 3 3; }
.stn.on .hit { stroke: var(--accent); stroke-width: 3; stroke-dasharray: none; }
.dim { opacity: .18; transition: opacity .2s; }
.halo { transform-box: fill-box; transform-origin: center; animation: halo 1.8s ease-out infinite; }
@keyframes halo { from { opacity: .9; transform: scale(1); } to { opacity: 0; transform: scale(1.6); } }
.pin-pulse { transform-box: fill-box; transform-origin: center; animation: halo 2.2s ease-out infinite; }
```

- [ ] **Step 4: Check in the browser.** Open `index.html` from the worktree path. Check:
  - three highways from the start interchange, with the Browser extension highway coming out of the 1.0 interchange
  - exit signs in two rows that don't overlap, the gantry signs, and the shields
  - built road with the white lane marking, hatched construction with barricades, dashed planned road, dotted idle road
  - "YOU ARE HERE" after 0.0.5
  - clicking a station outlines it in magenta; Tab and Enter work on stations
  - dragging pans the map, and the lane tags appear once you scroll
  - Night mode recolors everything without a reload
  - no console errors

  Take a screenshot at 1440 px and look for overlapping labels. If labels collide on the real data, adjust the column widths in `G.col` (and update the test numbers to match), not the drawing code.

- [ ] **Step 5: Log line, check, commit**
  - Log: `"Map drawing (map.js): highways with casing, built/construction/planned/idle road, barricades, stations, exit and gantry signs, shields, cul-de-sacs, you-are-here pin, lane labels."`

```bash
python scripts/check_roadmap.py
node --test scripts/map_layout.test.mjs
git add docs/roadmap/map.js docs/roadmap/index.html docs/roadmap/roadmap.js
git commit -m "Roadmap map: draw the highway map"
```

---

### Task 4: The rest of the map page (`page.js`) and the maximalist styling

**Files:**
- Create: `docs/roadmap/page.js`
- Modify: `docs/roadmap/index.html`, `docs/roadmap/roadmap.js` (log)

**Interfaces:**
- Consumes: `RM` (spec), `CompletionistMapLayout.layoutMap`, and `CompletionistMap.mount(...)` returning `{ select, spotlight, scrollToX, fit, redraw, xOf }`.
- Produces: `location.hash` values `#v<version>` (and `#v0.0.0`), plus task links to `tasks.html#<taskId>`.

- [ ] **Step 1: Add the page sections to `index.html`** in the order the spec gives under "Pages → index.html". Put the masthead, now sign and mile-marker posts before the map section. Put the travel guide, road work and itinerary after it, then the footer. Use these ids, which `page.js` fills in:

```html
<section class="wrap hero" aria-label="Where Completionist is">
  <span class="exit-tab">Autocomplete for Windows</span>
  <div class="sign hero-sign"><span class="hero-ghost" id="hero-ghost" aria-hidden="true"></span>
    <h1 class="hero-name">Completionist</h1><p class="hero-tag" id="hero-tag"></p><ul class="hero-arrows" id="hero-arrows"></ul></div>
  <div class="vms" role="status"><span class="vms-lbl">Now</span><p class="vms-text" id="vms-text"></p></div>
  <ul class="posts" id="posts" aria-label="Progress"></ul>
</section>
<!-- map section from Task 3 -->
<div class="wrap"><aside class="legend" id="legend" aria-label="Map key"></aside></div>
<section class="wrap guide" id="guide" aria-live="polite" aria-label="Travel guide"></section>
<section class="wrap work" aria-labelledby="work-h">
  <div class="sec-head"><div class="work-title"><div class="diamond" aria-hidden="true"><span><b>ROAD<br>WORK<br>AHEAD</b></span></div>
    <div class="sec-title"><span class="lbl">Doing, next and blocked</span><h2 id="work-h">Road <em>work</em></h2></div></div></div>
  <div class="lane-rule"></div><div class="work-cols" id="work-cols"></div>
</section>
<section class="wrap itinerary" aria-labelledby="itin-h">
  <div class="sec-head"><div class="sec-title"><span class="lbl">Every release, in full</span><h2 id="itin-h">The <em>itinerary</em></h2></div></div>
  <div class="lane-rule"></div><div id="itin"></div>
</section>
<footer class="wrap site">…links from the spec…</footer>
```

  Replace the inline script from Task 3 with `<script src="page.js"></script>`.

- [ ] **Step 2: Write `page.js`.** It has these functions, each rendering one section from `RM.data`:
  - `hero()`: sets `#hero-tag` from `tagline`, and `#hero-ghost` to the latest released version. `#hero-arrows` gets two `<li>` items. One is `↑ NEXT EXIT <version> · <title>`, from the `next` release, or the first unreleased one. The other is `→ <next major> · <title>`, the first unreleased major.
  - `vms()`: sets `#vms-text` to `data.now`.
  - `posts()`: the five posts from the spec. Each is `<li class="post"><b class="num">36<small>/49</small></b><span>tasks built</span></li>`. Count PRD stories as fully delivered when every live task that lists the story is done.
  - `chips(map)`: one `.hw-chip` per highway in `#hw-chips`, with `style="--hw:var(--hw-<id>)"` and `aria-pressed`. Clicking one calls `map.spotlight(id)`, and clicking the pressed one calls `map.spotlight(null)`.
  - `legend()`: a `.legend` cartouche with a compass rose (a small inline SVG with N, E, S and W and a four-point star in `--sign`). It has one entry for each row of the spec's "How to read the map" table, each with a 34×16 inline SVG sample drawn the same way `map.js` draws that thing. It ends with this sentence: "Road can be built past you-are-here: the work is done, it just hasn't shipped yet."
  - `guide(id)`: renders `#guide` for the selected station, following the spec's travel guide section. Here's how it decides which station type it's showing:

```js
function guide(id) {
  const R = RM, d = R.data;
  if (id === "0.0.0") return renderStart();                     // the 0.x line: its minors and patches
  const r = d.releases.find((x) => x.version === id);
  if (!r) return;
  const tier = R.tierOf(r.version);
  const m = r.milestone && R.milestoneById[r.milestone];
  const hws = R.highwaysOf(r);
  // header sign by tier, then title + pill (class "pill v-<status>") + date + highway chips,
  // then: patch -> <p class="prose big">r.text</p>
  //       minor -> <p class="prose drop">r.text</p> and, if m, a milestone box:
  //                goal, done_when, one meter per highway in hws (R.progress(R.tasksOn(m, h))),
  //                and the tasks grouped by highway: pill s-<status>, id, title, link tasks.html#<id>
  //       major -> r.essay paragraphs in <div class="essay drop">, plus the word count
  // A patch that has a milestone (0.0.3 / M0) also shows the milestone box.
}
```

  - `work()`: one `.work-col` per highway, with `style="--hw:..."`. Each shows the highway's tasks with status doing, next or blocked, sorted by `RM.STATUS[..].rank`. Each is a row with a pill, a mono id and a title, linking to `tasks.html#<id>`. With none, show "Clear road".
  - `itinerary(map)`: one `.itin-major` block per major line. The "0.x" block covers every version below 1.0.0, then one block per `X.0.0`. Each has a big shield (the same SVG as the brand mark, 72 px) and a heading. Every release inside gets its version, title, pill, date and full writing (patch `text`, minor `text`, major `essay`), plus a `Show on map` button that calls `select(version, true)`.
  - `select(id, scroll)`: calls `map.select(id)`, `guide(id)` and `history.replaceState(null, "", "#v" + id)` (inside try/catch). With `scroll`, it also calls `map.scrollToX(map.xOf(id))` and `document.getElementById("map-frame").scrollIntoView({ block: "nearest" })`.
  - **Start-up:**
    - If `!RM.data`, put the error box in `#guide` and stop. If `!RM.data.highways?.length`, do the same.
    - Call `RM.bindThemeButton(#theme)`.
    - Build `L = CompletionistMapLayout.layoutMap(RM.data)` and `map = CompletionistMap.mount(frame, L, { onSelect: (id) => select(id, false) })`.
    - Pick the first station: the hash if it names a release (`#v...`), otherwise the `next` release, otherwise the latest. Call `select(first)`, then `map.scrollToX(map.xOf("now"), false)`.
    - Wire the controls:
      - `#go-start` scrolls to 0.
      - `#go-now` scrolls to `xOf("now")`.
      - `#go-end` scrolls to `xOf("end")`.
      - `#fit` toggles `aria-pressed` and calls `map.fit`.
    - Handle `hashchange`.
    - Call `document.fonts.ready.then(() => map.redraw(CompletionistMapLayout.layoutMap(RM.data)))`.

- [ ] **Step 3: Style it maximalist.** Add these page styles. The spec lists the full inventory. These are the rules that set the look:

```css
.hero { display: grid; gap: 1.1rem; padding-block: .5rem 2.5rem; }
.hero .exit-tab { justify-self: start; margin-bottom: -1.1rem; margin-left: 1.5rem; position: relative; z-index: 1; }
.hero-sign { padding: clamp(1.75rem, 5vw, 3.5rem); overflow: hidden; display: grid; gap: .9rem; }
.hero-name { font-weight: 900; font-size: clamp(3.5rem, 13vw, 8rem); line-height: .85; letter-spacing: -.045em; position: relative; }
.hero-tag { font-family: var(--serif); font-style: italic; font-size: clamp(1.1rem, 2.6vw, 1.6rem); max-width: 36rem; position: relative; }
.hero-ghost { position: absolute; right: -1rem; bottom: -3.5rem; font-weight: 900; font-size: clamp(7rem, 26vw, 16rem); line-height: 1; letter-spacing: -.06em; color: transparent; -webkit-text-stroke: 2px var(--sign-dim); opacity: .28; pointer-events: none; }
.hero-arrows { list-style: none; margin: .5rem 0 0; padding: 1rem 0 0; border-top: 2px solid var(--sign-dim); display: grid; gap: .45rem; font-weight: 800; font-size: clamp(1rem, 2.4vw, 1.35rem); position: relative; }
.vms { display: grid; grid-template-columns: auto 1fr; gap: 1rem; align-items: center; background: var(--vms); color: var(--vms-ink); border-radius: 12px; padding: 1rem 1.25rem; border: 3px solid var(--sign-deep); position: relative; overflow: hidden; }
.vms::after { content: ""; position: absolute; inset: 0; pointer-events: none; background: radial-gradient(circle, transparent 55%, var(--vms) 60%) 0 0 / 4px 4px; opacity: .55; }
.vms-lbl { font-family: var(--mono); font-weight: 700; font-size: var(--fs-xs); letter-spacing: .2em; text-transform: uppercase; border: 2px solid currentColor; border-radius: 6px; padding: .25rem .5rem; }
.vms-text { font-family: var(--mono); font-weight: 600; font-size: clamp(.95rem, 2.2vw, 1.2rem); text-transform: uppercase; letter-spacing: .04em; text-shadow: 0 0 10px var(--accent); }
.posts { list-style: none; margin: 0; padding: 0; display: grid; grid-template-columns: repeat(auto-fit, minmax(9.5rem, 1fr)); gap: .75rem; }
.post { background: var(--sign); color: var(--sign-ink); border-radius: 10px 10px 4px 4px; padding: 1rem .9rem 1.4rem; display: grid; gap: .35rem; outline: 2px solid var(--sign-ink); outline-offset: -6px; box-shadow: 0 18px 28px -20px var(--shadow); position: relative; }
.post::after { content: ""; position: absolute; left: 50%; bottom: -14px; width: 10px; height: 14px; margin-left: -5px; background: var(--ink); border-radius: 0 0 3px 3px; }
.post b { font-weight: 900; font-size: 2.3rem; line-height: 1; letter-spacing: -.03em; }
.post b small { font-size: .45em; opacity: .8; }
.post span { font-weight: 800; font-size: var(--fs-xs); letter-spacing: .12em; text-transform: uppercase; color: var(--sign-dim); }
.legend { margin-top: 1rem; display: grid; grid-template-columns: auto 1fr; gap: 1rem 1.5rem; padding: 1.1rem 1.25rem; background: var(--surface); border: 2px solid var(--ink); border-radius: 4px; box-shadow: 6px 6px 0 var(--ink); }
.prose { font-family: var(--serif); font-size: 1.125rem; line-height: 1.7; }
.drop::first-letter { font-family: var(--serif); font-weight: 900; float: left; font-size: 4.2em; line-height: .8; padding: .08em .12em 0 0; color: var(--sign); }
.essay { columns: 2 22rem; column-gap: 2.5rem; column-rule: 2px dashed var(--line-strong); }
.essay p + p { margin-top: 1em; }
.work-title { display: flex; align-items: center; gap: 1rem; }
.work-cols { display: grid; grid-template-columns: repeat(auto-fit, minmax(15rem, 1fr)); gap: 1rem; }
.work-col { border-top: 8px solid var(--hw); background: var(--surface); border-radius: 0 0 var(--r) var(--r); padding: 1rem; display: grid; gap: .6rem; align-content: start; }
.itin-major { display: grid; gap: 1.25rem; padding-block: 2rem; border-bottom: 6px double var(--line-strong); }
```

  Also style `.guide` as a two-column grid on screens at least 900 px wide (the writing on the left, the milestone box on the right). Give it a big sign header, a mile-marker plate for patches, `.exit-tab` plus `.sign` for minors, and a 72 px shield for majors. Use `.sec-head`, `.sec-title` and `.lane-rule` from `theme.css` for every section heading.

- [ ] **Step 4: Check in the browser** at 1440 px and 375 px, in Day and Night:
  - The first view shows the map scrolled to "You are here", with 0.1.0 selected and its milestone box listing M1's tasks.
  - `#v1.0.0` shows the essay.
  - The spotlight chips dim the other highways.
  - "Whole map" fits the map to the width.
  - "Show on map" in the itinerary selects the station and scrolls to it.
  - There's no horizontal page scroll at 375 px.
  - The console has no errors.

- [ ] **Step 5: Log line, check, commit**
  - Log: `"Map page sections: masthead sign, now sign, mile-marker posts, highway spotlight, legend, travel guide, road work and itinerary."`

```bash
python scripts/check_roadmap.py
git add docs/roadmap/page.js docs/roadmap/index.html docs/roadmap/roadmap.js
git commit -m "Roadmap map: masthead, travel guide, road work and itinerary"
```

---

### Task 5: Retire `releases.html` and point the docs at the new pages

**Files:**
- Delete: `docs/roadmap/releases.html`
- Modify: `README.md`, `CLAUDE.md`, `scripts/check_roadmap.py` (docstring only), `docs/roadmap/roadmap.js` (log)

- [ ] **Step 1: Delete the page**

```bash
git rm docs/roadmap/releases.html
```

- [ ] **Step 2: Update `CLAUDE.md`**
  - In "The roadmap: keep it current", replace the two page bullets with:
    - `docs/roadmap/index.html`: the highway map. Every release on highways, one for each part of the app, with the release writing, the road work and the itinerary.
    - `docs/roadmap/tasks.html`: the task board by milestone, with filters, story coverage, decisions, risks and the log.
  - In the Layout table, change the `docs/roadmap/` row to: `Roadmap data (roadmap.js), the highway map (index.html, map-layout.js, map.js, page.js), the task board (tasks.html), shared theme.css and common.js`.
  - Add a row: `| CONTEXT.md | What the product is, its architecture, glossary, the official Evergreen color scheme and typography |`.
  - Add a line under Working agreements: `- **Colors and type:** use the Evergreen tokens and fonts in CONTEXT.md. Don't add colors without validating them as it describes.`

- [ ] **Step 3: Update `README.md`.** Change the Roadmap line to:

  `**Roadmap:** open [docs/roadmap/index.html](docs/roadmap/index.html) (the highway map) or [docs/roadmap/tasks.html](docs/roadmap/tasks.html) (the task board) in a browser. Both read docs/roadmap/roadmap.js, which agents keep current (see CLAUDE.md). Project context and the color scheme are in [CONTEXT.md](CONTEXT.md).`

  Also change the `docs/roadmap/` layout row to "Roadmap data, highway map and task board (open the HTML files from disk)".

- [ ] **Step 4: Check nothing still links the old page**

```bash
git grep -n "releases.html"
```

  Expected: no output. The spec and plan are allowed to mention it; if they're the only matches, that's fine.

- [ ] **Step 5: Log line, check, commit**
  - Log: `"Retired releases.html (its writing now lives in the map's travel guide and itinerary); README and CLAUDE.md point at the map and the task board."`

```bash
python scripts/check_roadmap.py
git add -A docs/roadmap README.md CLAUDE.md
git commit -m "Roadmap: retire releases.html; docs point at the map and task board"
```

---

### Task 6: Write `CONTEXT.md`

**Files:**
- Create: `CONTEXT.md` at the repo root
- Modify: `docs/roadmap/roadmap.js` (log)

- [ ] **Step 1: Write it.** It has the 13 sections listed in the spec's "What CONTEXT.md contains", in that order, with `##` headings. Take facts only from the spec, `README.md`, `engine/README.md`, `tip/README.md`, `CLAUDE.md` and `roadmap.js`. Don't invent anything.
  - The Evergreen section copies the spec's token table, rules, validation results and the re-validation command:

```bash
node <dataviz skill>/scripts/validate_palette.js "#0B6E4B,#5E3BA3,#0CA1A7,#787804" --mode light --surface "#FBFCF8"
node <dataviz skill>/scripts/validate_palette.js "#079461,#7553BD,#11A3A9,#6C6C01" --mode dark --surface "#141C18"
```

  - Start the file with one line: "Reference for anyone working on this project. CLAUDE.md is how agents work; this file is what the product is."

- [ ] **Step 2: Check it.** Every hex value in `CONTEXT.md` must also appear in `docs/roadmap/theme.css`:

```bash
python -c "import re,pathlib;c=set(re.findall(r'#[0-9A-Fa-f]{6}',pathlib.Path('CONTEXT.md').read_text(encoding='utf-8')));t=set(x.upper() for x in re.findall(r'#[0-9A-Fa-f]{6}',pathlib.Path('docs/roadmap/theme.css').read_text(encoding='utf-8')));print(sorted(x for x in c if x.upper() not in t) or 'all colors match')"
```

  Expected: `all colors match`.

- [ ] **Step 3: Log line, check, commit**
  - Log: `"CONTEXT.md: product, architecture, glossary, release and map rules, and the official Evergreen color scheme and typography."`

```bash
python scripts/check_roadmap.py
git add CONTEXT.md docs/roadmap/roadmap.js
git commit -m "CONTEXT.md: project reference and the Evergreen color scheme"
```

---

### Task 7: Final check and roadmap close-out

**Files:**
- Modify: `docs/roadmap/roadmap.js` (M1.15, log, `updated`)

- [ ] **Step 1: Run the checks**

```bash
python scripts/check_roadmap.py
node --test scripts/map_layout.test.mjs
git grep -n "releases.html" -- ":!docs/superpowers"
```

  Expected: `roadmap ok`, then all tests passing, then no output.

- [ ] **Step 2: Go through the spec's Acceptance list one item at a time** in the browser pane:
  - both pages
  - Day and Night
  - 1440 px and 375 px
  - keyboard only: Tab to a station, Enter, then check the guide updates
  - no console errors

  Write down anything that fails, and fix it before going on.

- [ ] **Step 3: Close M1.15**
  - Set its status to `done`.
  - Make its `notes` one factual sentence about what shipped.
  - Its `refs`: `["branch worktree-roadmap-map", "docs/roadmap/index.html", "docs/roadmap/map-layout.js", "docs/roadmap/map.js", "docs/roadmap/page.js", "docs/roadmap/tasks.html", "CONTEXT.md"]`.
  - Log: `"M1.15 done: the highway map, the Evergreen task board and CONTEXT.md."`
  - Commit:

```bash
python scripts/check_roadmap.py
git add docs/roadmap/roadmap.js
git commit -m "Roadmap: M1.15 done"
```

- [ ] **Step 4: Stop and ask the user about merging.** Don't merge or push without asking; CLAUDE.md requires asking before pushing to `main`. When the user says yes, the merge is a patch release (0.0.6 or the next free patch number). It needs:
  - `"highways": ["tooling"]`, status `released`, today's date, and one sentence, for example: "The roadmap became a highway map in the new Evergreen color scheme, with the task board restyled to match and a CONTEXT.md describing the project."
  - a log line
  - the `v0.0.6` tag, pushed

## Self-review notes

- **What covers each part of the spec:**
  - map semantics: Tasks 2 and 3
  - page sections: Task 4
  - task board: Task 1
  - removing `releases.html`: Task 5
  - `CONTEXT.md`: Task 6
  - colors and type: `theme.css` (done), used by Tasks 1, 3 and 4
  - accessibility: Tasks 3, 4 and 7
  - the every-commit rule: in each task's commit step
- **Names used across tasks:** `layoutMap`, `CompletionistMap.mount`, `select`, `spotlight`, `scrollToX`, `fit`, `redraw`, `xOf`, and the station id `"0.0.0"` for the start.
- **Layout constants:** `G.rows` (gantry 24, A 100, B 168) is defined in Task 2 and used in Task 3.
