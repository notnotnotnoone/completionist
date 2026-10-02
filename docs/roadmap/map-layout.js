// Pure geometry for the roadmap highway map: no DOM, no styling.
// Used by map.js in the page (window.CompletionistMapLayout) and by scripts/map_layout.test.mjs under node.
(function (root, factory) {
  if (typeof module === "object" && module.exports) module.exports = factory();
  else root.CompletionistMapLayout = factory();
})(typeof self !== "undefined" ? self : this, function () {
  const G = {
    left: 190, laneTop: 250, bundleGap: 18, curve: 80, joint: 12, bottom: 140, tail: 40,
    col: { major: 220, minor: 170, patch: 104, now: 120, end: 160 },
    rows: { gantry: 24, A: 100, B: 168 },
    // A busy highway gets a taller band: one slot row per `perSlot` single-highway patches (up to maxSlots),
    // `pitch` apart. Slots run on the road, then further below it (never above: the exit signs live there).
    perSlot: 10, maxSlots: 4, pitch: 64, minUp: 46, minDown: 70, slotOrder: [0, 1, 2, 3],
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

    // 3. Lanes: a highway's height follows how many patch stops it carries; its stops are dealt into slot rows.
    const solo = (e) => e.kind === "patch" && !e.fix && e.lanes.length === 1;
    const slotsOf = Object.fromEntries(highways.map((h) => {
      const count = events.filter((e) => solo(e) && e.lanes[0] === h.id).length;
      const n = Math.min(G.maxSlots, Math.max(1, Math.round(count / G.perSlot)));
      return [h.id, G.slotOrder.slice(0, n)];
    }));
    const dealt = {};
    for (const e of events) {
      if (!solo(e)) continue;
      const id = e.lanes[0], slots = slotsOf[id];
      dealt[id] = dealt[id] || 0;
      e.slot = slots[dealt[id]++ % slots.length];
    }
    const laneY = {}, laneBand = {};
    let cursor = G.laneTop;
    highways.forEach((h, i) => {
      const slots = slotsOf[h.id];
      const up = Math.max(G.minUp, -Math.min(0, ...slots) * G.pitch + 20), down = Math.max(G.minDown, Math.max(0, ...slots) * G.pitch + G.minDown);
      if (i) cursor += up;
      laneY[h.id] = cursor;
      laneBand[h.id] = { top: cursor - up, bottom: cursor + down };
      cursor += down;
    });
    for (const e of events) if (e.slot !== undefined) e.dy = e.slot * G.pitch;
    const majors = events.filter((e) => e.kind === "major");
    for (const e of majors) {
      const open = highways.filter((h) => isOpen(h, e.version));
      const mid = open.reduce((s, h) => s + laneY[h.id], 0) / open.length;
      e.bundle = Object.fromEntries(open.map((h, k) => [h.id, mid + (k - (open.length - 1) / 2) * G.bundleGap]));
    }
    const lanes = highways.map((h) => {
      const at = h.opens ? majors.find((e) => e.version === h.opens) : majors[0];
      return { id: h.id, y: laneY[h.id], band: laneBand[h.id], opensX: at.x, opensId: at.id };
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

    const height = laneBand[highways[highways.length - 1].id].bottom + G.bottom - G.minDown;
    return { G, width, height, roadEnd, events, lanes, paths, pieces, barriers, now: events.find((e) => e.kind === "now") };
  }

  return { layoutMap, G };
});
