// Pure geometry for the roadmap highway map: no DOM, no styling.
// Used by map.js in the page (window.CompletionistMapLayout) and by scripts/map_layout.test.mjs under node.
//
// History runs left to right as the old highway map did: one sign per major or minor release, and one stop per task
// (grouped under the release that shipped it, so a run of dots shows how much work a major took). Releases that carry
// no recorded task keep a release stop. Unshipped work follows in scope-sized regions; width never depends on task count.
(function (root, factory) {
  if (typeof module === "object" && module.exports) module.exports = factory();
  else root.CompletionistMapLayout = factory();
})(typeof self !== "undefined" ? self : this, function () {
  const G = {
    left: 190, laneTop: 250, bundleGap: 18, curve: 80, joint: 12, bottom: 140, tail: 40,
    col: { major: 220, minor: 170, patch: 104, task: 104 },
    rows: { gantry: 24, A: 100, B: 168 },
    // A busy highway gets a taller band: one slot row per `perSlot` single-lane stops (up to maxSlots), `pitch` apart.
    // Slots run on the road, then further below it (never above: the exit signs live there).
    perSlot: 10, maxSlots: 4, pitch: 64, minUp: 46, minDown: 100, slotOrder: [0, 1, 2, 3],
    // Unshipped work: region width follows scope; tasks fill a grid whose rows only add height.
    scope: { contained: 330, medium: 500, large: 700 }, destination: 205, gap: 30, taskPitch: 120, taskCol: 145, taskPad: 45,
  };
  const parse = (v) => String(v).split(".").map(Number);
  const cmp = (a, b) => { const x = parse(a), y = parse(b); return x[0] - y[0] || x[1] - y[1] || x[2] - y[2]; };
  const tier = (v) => { const [, minor, patch] = parse(v); return patch ? "patch" : minor ? "minor" : "major"; };
  const ACTIVE = new Set(["doing", "next", "blocked"]);

  function layoutMap(data) {
    const highways = data.highways;
    const owners = {};
    for (const h of highways) for (const a of h.areas) owners[a] = h.id;
    const isOpen = (h, v) => !h.opens || cmp(v, h.opens) >= 0;
    const live = (t) => t.status !== "dropped";
    const releases = data.releases.slice().sort((a, b) => cmp(a.version, b.version));
    const released = releases.filter((r) => r.status === "released");
    const future = releases.filter((r) => r.status !== "released");
    const shippedTasks = (v) => data.tasks.filter((t) => t.shippedIn === v && live(t));

    // Actual records name their lanes; otherwise lanes come from the release's own work.
    function lanesOf(r) {
      if (Array.isArray(r.highways) && r.highways.length) return highways.filter((h) => r.highways.includes(h.id)).map((h) => h.id);
      let own = data.tasks.filter((t) => (r.status === "released" ? t.shippedIn === r.version : t.targetRelease === r.version) && live(t));
      if (!own.length && tier(r.version) !== "patch") own = data.tasks.filter((t) => t.targetRelease === r.version && live(t));
      const used = new Set(own.map((t) => owners[t.area]));
      if (!used.size && r.status === "released" && tier(r.version) === "major") return highways.filter((h) => isOpen(h, r.version)).map((h) => h.id);
      return highways.filter((h) => used.has(h.id)).map((h) => h.id);
    }

    // 1. Shipped history, left to right: the start interchange, then every release or the tasks it shipped.
    const cols = [{ id: "0.0.0", kind: "major", version: "0.0.0", start: true, release: null, status: "released", fix: false,
      lanes: highways.filter((h) => isOpen(h, "0.0.0")).map((h) => h.id) }];
    const releaseAlias = [];
    for (const r of released) {
      const own = tier(r.version) === "patch" ? shippedTasks(r.version) : [];
      if (own.length) {
        own.forEach((t, i) => cols.push({ id: t.id, kind: "task", task: t, release: r, first: i === 0, count: own.length, shipped: true,
          lane: owners[t.area], lanes: [owners[t.area]] }));
        releaseAlias.push({ id: r.version, kind: "alias", release: r, version: r.version, status: r.status, lanes: lanesOf(r), taskId: own[0].id });
      } else {
        cols.push({ id: r.version, kind: tier(r.version), version: r.version, release: r, status: r.status, fix: r.kind === "fix", lanes: lanesOf(r) });
      }
    }

    // 2. Columns.
    let x = G.left;
    for (const c of cols) { c.width = G.col[c.kind]; c.x = x + c.width / 2; x += c.width; }
    const historyEnd = x;
    const xOfCol = Object.fromEntries(cols.map((c) => [c.id, c.x]));
    for (const a of releaseAlias) { a.x = xOfCol[a.taskId]; a.width = 0; }

    // 3. Unshipped work: one region per future release, scope wide, with its destination sign at the right-hand end.
    x += G.gap;
    const regions = [], futureStops = [], destinations = [];
    for (const r of future) {
      const plan = data.releasePlans[r.version] || {}, width = G.scope[plan.scope] || G.scope.medium;
      const own = data.tasks.filter((t) => t.targetRelease === r.version && !t.shippedIn);
      const work = own.filter(live);
      regions.push({ id: r.version, version: r.version, release: r, plan, x0: x, x1: x + width, width, tasks: own,
        progress: { done: work.filter((t) => t.status === "done").length, total: work.length } });
      destinations.push({ id: r.version, kind: tier(r.version), release: r, version: r.version, status: r.status, future: true,
        x: x + width - 65, width: G.destination, lanes: lanesOf(r) });
      const columns = Math.max(1, Math.floor((width - G.destination - 40) / G.taskCol));
      for (const h of highways) {
        own.filter((t) => owners[t.area] === h.id).forEach((t, i) => {
          futureStops.push({ id: t.id, kind: "task", task: t, target: r.version, lane: h.id, lanes: [h.id],
            x: x + G.taskPad + (i % columns) * G.taskCol, row: Math.floor(i / columns), branch: t.status === "dropped" });
        });
      }
      x += width;
    }

    // 4. Lanes: a highway's height follows its busiest history slots and its tallest unshipped stack.
    const solo = (c) => (c.kind === "task") || (c.kind === "patch" && !c.fix && c.lanes.length === 1);
    const slotsOf = Object.fromEntries(highways.map((h) => {
      const count = cols.filter((c) => solo(c) && c.lanes[0] === h.id).length;
      const n = Math.min(G.maxSlots, Math.max(1, Math.round(count / G.perSlot)));
      return [h.id, G.slotOrder.slice(0, n)];
    }));
    const dealt = {};
    for (const c of cols) {
      if (!solo(c)) continue;
      const id = c.lanes[0], slots = slotsOf[id];
      dealt[id] = dealt[id] || 0;
      c.slot = slots[dealt[id]++ % slots.length];
      c.dy = c.slot * G.pitch;
    }
    const laneY = {}, laneBand = {};
    let cursor = G.laneTop;
    highways.forEach((h, i) => {
      const rows = Math.max(1, ...futureStops.filter((s) => s.lane === h.id).map((s) => s.row + 1));
      const up = G.minUp, down = Math.max(G.minDown + Math.max(...slotsOf[h.id]) * G.pitch, (rows - 1) * G.taskPitch + G.minDown);
      if (i) cursor += up;
      laneY[h.id] = cursor;
      laneBand[h.id] = { top: cursor - up, bottom: cursor + down, rows };
      cursor += down;
    });
    for (const s of futureStops) s.y = laneY[s.lane] + s.row * G.taskPitch + (s.branch ? 22 : 0);

    const majors = cols.filter((c) => c.kind === "major");
    for (const e of majors) {
      const open = highways.filter((h) => isOpen(h, e.version));
      const mid = open.reduce((s, h) => s + laneY[h.id], 0) / open.length;
      e.bundle = Object.fromEntries(open.map((h, k) => [h.id, mid + (k - (open.length - 1) / 2) * G.bundleGap]));
    }

    const width = x + 80, roadEnd = width - G.tail;
    const lanes = highways.map((h) => {
      const construction = regions.find((r) => r.version === h.opens);
      const launch = h.opens ? [...cols, ...destinations].find((e) => e.version === h.opens) : null;
      const bundled = h.opens ? majors.find((e) => e.version === h.opens) : majors[0];
      const opensX = bundled ? bundled.x : (construction ? construction.x0 : (launch ? launch.x : roadEnd));
      return { id: h.id, y: laneY[h.id], band: laneBand[h.id], opensX, opensId: h.opens || "0.0.0", bundled: !!bundled, launchX: launch && launch.x };
    });

    // 5. One path per highway: out of its opening interchange, through every later interchange, to the end.
    const J = G.joint, C = G.curve;
    const paths = {};
    for (const lane of lanes) {
      const y = lane.y;
      if (!lane.bundled) { paths[lane.id] = `M${lane.opensX} ${y} H${roadEnd}`; continue; }
      const first = majors.find((e) => e.x === lane.opensX);
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

    // 6. Road pieces: history is built; each region's road is built, active or still to do in proportion to its work.
    const pieces = [], barriers = [];
    const histEnd = regions.length ? regions[0].x0 : roadEnd;
    for (const lane of lanes) {
      if (lane.opensX < histEnd) pieces.push({ hw: lane.id, x0: lane.opensX, x1: histEnd, style: "built" });
      for (const region of regions) {
        const x0 = Math.max(region.x0, lane.opensX), x1 = region.x1;
        if (x1 <= x0) continue;
        const own = region.tasks.filter((t) => live(t) && owners[t.area] === lane.id);
        if (!own.length) {
          const dest = destinations.find((d) => d.id === region.id);
          pieces.push({ hw: lane.id, x0, x1, style: dest.lanes.includes(lane.id) ? "todo" : "idle" });
          continue;
        }
        const span = x1 - x0;
        const xd = x0 + span * own.filter((t) => t.status === "done").length / own.length;
        const xa = xd + span * own.filter((t) => ACTIVE.has(t.status)).length / own.length;
        if (xd > x0) pieces.push({ hw: lane.id, x0, x1: xd, style: "built" });
        if (xa > xd) {
          pieces.push({ hw: lane.id, x0: xd, x1: xa, style: "active" });
          if (xd > x0 + 14) barriers.push({ hw: lane.id, x: xd, y: lane.y });
        }
        if (x1 > xa) pieces.push({ hw: lane.id, x0: xa, x1, style: "todo" });
      }
    }

    // 7. Sign rows: majors on the gantry row, minors alternate between exit rows A and B.
    let n = 0;
    for (const e of cols) {
      if (e.kind === "major") e.row = "gantry";
      else if (e.kind === "minor") e.row = n++ % 2 ? "B" : "A";
    }

    const taskStops = [...cols.filter((c) => c.kind === "task").map((c) => ({ ...c, y: laneY[c.lane] + c.dy, target: c.release.version })), ...futureStops];
    const active = regions.find((r) => r.version === data.activeRelease);
    const now = { id: "now", kind: "now", x: active ? active.x0 + 20 : histEnd, target: data.activeRelease, version: released.length ? released[released.length - 1].version : null };
    const events = [...cols.filter((c) => c.kind !== "task"), ...releaseAlias, ...destinations, now, { id: "end", kind: "end", x: width - 30 }];
    const height = laneBand[highways[highways.length - 1].id].bottom + G.bottom - G.minDown;
    return { G, width, height, roadEnd, historyEnd, events, regions, taskStops, lanes, paths, pieces, barriers, now };
  }

  return { layoutMap, G };
});
