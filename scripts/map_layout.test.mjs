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

// A busy highway: 24 single-highway patches on "e", one on "t".
const busy = () => {
  const d = fixture();
  d.releases = [{ version: "0.0.1", status: "released", highways: ["t"] }, { version: "1.0.0", status: "planned" }];
  for (let i = 2; i <= 25; i++) d.releases.push({ version: `0.0.${i}`, status: "released", highways: ["e"] });
  return d;
};

test("lanes: a busy highway gets a taller band, the others keep the standard 116", () => {
  const L = layoutMap(busy());
  const y = Object.fromEntries(L.lanes.map((l) => [l.id, l.y]));
  assert.equal(y.e, 250);
  assert.equal(y.t - y.e, 180);
  assert.equal(L.lanes[0].band.bottom - L.lanes[0].band.top, 180);
  assert.equal(y.b - y.t, 116);
  assert.equal(L.height, 686);
});

test("slots: a busy highway's patches cycle through slot rows, quiet ones stay on the road", () => {
  const L = layoutMap(busy());
  const dys = L.events.filter((e) => e.kind === "patch" && e.lanes[0] === "e").map((e) => e.dy);
  assert.deepEqual(dys.slice(0, 4), [0, 64, 0, 64]);
  assert.ok(L.events.filter((e) => e.kind === "patch" && e.lanes[0] === "t").every((e) => !e.dy));
});
