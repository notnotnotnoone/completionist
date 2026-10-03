// Shared helpers for the roadmap pages (index.html = highway map, tasks.html = task board).
// Reads window.COMPLETIONIST_ROADMAP from roadmap.js and exposes window.RM.
(() => {
  const data = window.COMPLETIONIST_ROADMAP;

  // Day/night: an explicit choice is kept per viewer; otherwise the system setting wins.
  const THEME_KEY = "completionist-roadmap:theme";
  try { const t = localStorage.getItem(THEME_KEY); if (t === "light" || t === "dark") document.documentElement.dataset.theme = t; } catch {}
  function isDark() {
    const t = document.documentElement.dataset.theme;
    return t ? t === "dark" : matchMedia("(prefers-color-scheme: dark)").matches;
  }
  function bindThemeButton(btn) {
    if (!btn) return;
    const paint = () => {
      const dark = isDark();
      btn.setAttribute("aria-pressed", String(dark));
      btn.innerHTML = dark
        ? `<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M13.5 10.2A6 6 0 0 1 5.8 2.5a6 6 0 1 0 7.7 7.7Z" fill="currentColor"/></svg>Night`
        : `<svg viewBox="0 0 16 16" aria-hidden="true"><circle cx="8" cy="8" r="3.2" fill="currentColor"/><path d="M8 1v2M8 13v2M1 8h2M13 8h2M3 3l1.4 1.4M11.6 11.6 13 13M3 13l1.4-1.4M11.6 4.4 13 3" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/></svg>Day`;
    };
    btn.addEventListener("click", () => {
      const next = isDark() ? "light" : "dark";
      document.documentElement.dataset.theme = next;
      try { localStorage.setItem(THEME_KEY, next); } catch {}
      paint();
      document.dispatchEvent(new CustomEvent("rm:theme"));
    });
    matchMedia("(prefers-color-scheme: dark)").addEventListener("change", () => { paint(); document.dispatchEvent(new CustomEvent("rm:theme")); });
    paint();
  }

  const esc = (s) => String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[c]);
  const rich = (s) => esc(s).replace(/`([^`]+)`/g, "<code>$1</code>");
  const words = (s) => String(s || "").split(/\s+/).filter(Boolean).length;
  const parseVer = (v) => String(v).split(".").map(Number);
  const cmpVer = (a, b) => { const x = parseVer(a), y = parseVer(b); return x[0] - y[0] || x[1] - y[1] || x[2] - y[2]; };
  const tierOf = (v) => { const [, minor, patch] = parseVer(v); return patch ? "patch" : minor ? "minor" : "major"; };
  const fmtDate = (iso) => { if (!iso) return ""; const [y, m, d] = iso.split("-").map(Number); return new Date(y, m - 1, d).toLocaleDateString("en-US", { year: "numeric", month: "short", day: "numeric" }); };

  const STATUS = {
    doing: { label: "Doing", rank: 0 }, next: { label: "Next", rank: 1 }, blocked: { label: "Blocked", rank: 2 },
    todo: { label: "To do", rank: 3 }, done: { label: "Done", rank: 4 }, dropped: { label: "Dropped", rank: 5 },
  };
  const RELEASE_STATUS = { released: "Released", next: "Up next", planned: "Planned" };
  const MS_STATUS = { active: "Active", done: "Done", planned: "Planned" };
  const AREA = { engine: "Engine", dll: "DLL", install: "Install", test: "Manual test", bench: "Benchmark", data: "Data",
    docs: "Docs", repo: "Repo", toolchain: "Toolchain", extension: "Extension", viewer: "Viewer" };

  if (!data) { window.RM = { data: null, bindThemeButton, esc, rich }; return; }

  const highways = data.highways || [];
  const hwById = Object.fromEntries(highways.map((h) => [h.id, h]));
  const hwOfArea = {};
  for (const h of highways) for (const a of h.areas || []) hwOfArea[a] = h;
  const hwColor = (id) => `var(--hw-${id})`;
  const hwOpenAt = (h, version) => !h.opens || cmpVer(version, h.opens) >= 0;

  const tasks = (data.tasks || []).map((t) => ({ ...t, stories: t.stories || [], hw: hwOfArea[t.area] || null }));
  const releaseByVersion = Object.fromEntries(data.releases.map((r) => [r.version, r]));
  const groups = data.releases.filter((r) => data.releasePlans[r.version]).map((r) => {
    const plan = data.releasePlans[r.version];
    return { ...plan, id: r.version, version: r.version, release: r,
      status: r.status === "released" ? "done" : r.version === data.activeRelease ? "active" : "planned",
      tasks: tasks.filter((t) => t.targetRelease === r.version) };
  });
  const groupByVersion = Object.fromEntries(groups.map((g) => [g.version, g]));
  for (const t of tasks) t.ms = groupByVersion[t.targetRelease] || { id: t.targetRelease, title: releaseByVersion[t.targetRelease]?.title || t.targetRelease };
  const releaseForMilestone = releaseByVersion;
  const milestoneById = groupByVersion;

  const live = (list) => list.filter((t) => t.status !== "dropped");
  const progress = (list) => {
    const l = live(list);
    return {
      done: l.filter((t) => t.status === "done").length,
      active: l.filter((t) => t.status === "doing" || t.status === "next" || t.status === "blocked").length,
      total: l.length,
    };
  };
  const tasksOn = (m, hwId) => m.tasks.filter((t) => t.hw && t.hw.id === hwId);

  // Actual records name their lanes; future destinations derive participating lanes from their work.
  function highwaysOf(r) {
    const tier = tierOf(r.version);
    if (Array.isArray(r.highways) && r.highways.length) return r.highways.filter((id) => hwById[id]);
    let own = r.status === "released" ? tasks.filter((t) => t.shippedIn === r.version) : tasks.filter((t) => t.targetRelease === r.version);
    if (!own.length && tier !== "patch") own = tasks.filter((t) => t.targetRelease === r.version);
    const used = new Set(live(own).map((t) => t.hw && t.hw.id).filter(Boolean));
    if (!used.size && r.status === "released" && tier === "major") return highways.filter((h) => hwOpenAt(h, r.version)).map((h) => h.id);
    return highways.filter((h) => used.has(h.id)).map((h) => h.id);
  }

  window.RM = {
    data, highways, hwById, hwOfArea, hwColor, hwOpenAt, tasks, groups, groupByVersion, releaseByVersion, milestoneById, releaseForMilestone,
    progress, tasksOn, highwaysOf, live,
    esc, rich, words, parseVer, cmpVer, tierOf, fmtDate,
    STATUS, RELEASE_STATUS, MS_STATUS, AREA, areaName: (a) => AREA[a] || a,
    bindThemeButton, isDark,
  };
})();
