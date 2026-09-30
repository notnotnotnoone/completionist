// The highway-map page: masthead, now sign, mile-marker posts, spotlight chips, legend, travel guide,
// road work and itinerary, plus the wiring to the map (CompletionistMap) and to location.hash (#v0.1.0).
(() => {
  const $ = (s) => document.querySelector(s);
  const R = window.RM;
  const data = R && R.data;

  function fail(msg) {
    const box = document.getElementById("guide");
    box.innerHTML = `<div class="error"><strong>${msg}</strong><p>Run <code>python scripts/check_roadmap.py</code> to find the problem. <code>roadmap.js</code> must sit next to this page.</p></div>`;
    for (const id of ["hero-tag", "vms-text"]) { const el = document.getElementById(id); if (el) el.textContent = "Roadmap data unavailable"; }
  }
  if (!data) { R && R.bindThemeButton($("#theme")); return fail("Couldn't load roadmap.js."); }
  if (!R.highways.length) { R.bindThemeButton($("#theme")); return fail("roadmap.js has no highways."); }

  const { esc, rich } = R;
  const releases = data.releases.slice().sort((a, b) => R.cmpVer(a.version, b.version));
  const released = releases.filter((r) => r.status === "released");
  const latest = released.length ? released[released.length - 1] : null;
  const shortVer = (v) => v.replace(/\.0$/, "");
  const pill = (cls, text) => `<span class="pill ${cls}">${esc(text)}</span>`;
  const relPill = (r) => pill(`v-${r.status}`, R.RELEASE_STATUS[r.status] || r.status);
  const hwChip = (id) => { const h = R.hwById[id]; return `<span class="hw-tag" style="--hw:${R.hwColor(id)}"><i></i>${esc(h.title)}</span>`; };
  const shield = (label, size) => `<svg class="shield" width="${size}" height="${Math.round(size * 1.1)}" viewBox="0 0 40 44" aria-hidden="true"><path d="M20 1 38 7v14c0 10-8 17-18 21C10 38 2 31 2 21V7Z" style="fill:var(--sign);stroke:var(--sign-ink);stroke-width:2"/><path d="M6 10h28" style="stroke:var(--sign-ink);stroke-width:1.2"/><text x="20" y="31" text-anchor="middle" style="fill:var(--sign-ink);font:900 20px var(--display)">${esc(label)}</text></svg>`;

  // ---- masthead, now sign, posts
  function hero() {
    $("#hero-tag").textContent = data.tagline || "";
    $("#hero-ghost").textContent = latest ? latest.version : "";
    const upcoming = releases.filter((r) => r.status !== "released");
    const next = releases.find((r) => r.status === "next") || upcoming[0];
    const major = upcoming.find((r) => R.tierOf(r.version) === "major");
    const rows = [];
    if (next) rows.push(`<li>↑ Next exit ${esc(shortVer(next.version))} · ${esc(next.title)}</li>`);
    if (major) rows.push(`<li>→ ${esc(shortVer(major.version))} · ${esc(major.title)}</li>`);
    $("#hero-arrows").innerHTML = rows.join("");
    $("#updated").textContent = data.updated;
    $("#updated").setAttribute("datetime", data.updated);
  }
  function vms() { $("#vms-text").textContent = data.now || ""; }

  function posts() {
    const t = R.progress(R.tasks);
    const open = latest ? R.highways.filter((h) => R.hwOpenAt(h, latest.version)).length : R.highways.filter((h) => !h.opens).length;
    const active = data.milestones.find((m) => m.status === "active");
    const ap = active ? R.progress(active.tasks) : null;
    const nums = Object.keys(data.stories);
    const delivered = nums.filter((n) => {
      const live = R.live(R.tasks.filter((x) => x.stories.includes(Number(n))));
      return live.length && live.every((x) => x.status === "done");
    }).length;
    const post = (n, of, label) => `<li class="post"><b class="num">${n}<small>/${of}</small></b><span>${esc(label)}</span></li>`;
    $("#posts").innerHTML = post(t.done, t.total, "tasks built")
      + post(released.length, releases.length, "releases shipped")
      + post(open, R.highways.length, "highways open")
      + (ap ? post(ap.done, ap.total, `${active.id} progress`) : "")
      + post(delivered, nums.length, "PRD stories delivered");
  }

  // ---- spotlight chips
  function chips(map) {
    const box = $("#hw-chips");
    box.innerHTML = R.highways.map((h) => `<button type="button" class="hw-chip" data-hw="${esc(h.id)}" style="--hw:${R.hwColor(h.id)}" aria-pressed="false"><i></i>${esc(h.title)}</button>`).join("");
    box.addEventListener("click", (e) => {
      const b = e.target.closest(".hw-chip");
      if (!b) return;
      const on = b.getAttribute("aria-pressed") !== "true";
      box.querySelectorAll(".hw-chip").forEach((x) => x.setAttribute("aria-pressed", String(x === b && on)));
      map.spotlight(on ? b.dataset.hw : null);
    });
  }

  // ---- legend
  function legend() {
    const sv = (inner) => `<svg viewBox="0 0 34 16" width="34" height="16" aria-hidden="true">${inner}</svg>`;
    const C = "var(--hw-engine)";
    const road = (extra) => `<path d="M1 8H33" style="fill:none;stroke:${C};stroke-width:8;${extra}"/>`;
    const dot = (cx, fill) => `<circle cx="${cx}" cy="8" r="3" style="fill:${fill};stroke:var(--surface);stroke-width:1.5"/>`;
    const rows = [
      ["Highway", "a part of the app", sv(road(""))],
      ["Interchange", "a major release: every highway meets", sv(`<rect x="11" y="1" width="12" height="14" rx="6" style="fill:var(--sign);stroke:var(--sign-ink);stroke-width:1.5"/>${dot(17, "var(--hw-engine)")}`)],
      ["Station", "a minor release, across its highways", sv(`<rect x="12" y="1" width="10" height="14" rx="5" style="fill:var(--ink);stroke:var(--surface);stroke-width:1.5"/>${dot(17, "var(--hw-tsf)")}`)],
      ["Stop", "a patch release", sv(`${road("opacity:.35")}<circle cx="17" cy="8" r="6" style="fill:${C};stroke:var(--surface);stroke-width:1.5"/><path d="M14.3 8.2l2 2.2 3.6-4.2" style="fill:none;stroke:var(--sign-ink);stroke-width:1.6;stroke-linecap:round;stroke-linejoin:round"/>`)],
      ["Cul-de-sac", "a bug-fix patch", sv(`${road("opacity:.35")}<path d="M17 8V12" style="stroke:${C};stroke-width:3"/><circle cx="17" cy="12.5" r="3" style="fill:${C}"/>`)],
      ["Released", "solid, with a check", sv(`<circle cx="17" cy="8" r="6" style="fill:${C};stroke:var(--surface);stroke-width:1.5"/><path d="M14.3 8.2l2 2.2 3.6-4.2" style="fill:none;stroke:var(--sign-ink);stroke-width:1.6;stroke-linecap:round"/>`)],
      ["Up next", "magenta ring, pulsing", sv(`<circle cx="17" cy="8" r="6" style="fill:var(--surface);stroke:var(--accent);stroke-width:2.5"/>`)],
      ["Planned", "dashed outline", sv(`<circle cx="17" cy="8" r="6" style="fill:var(--surface);stroke:var(--ghost);stroke-width:1.6;stroke-dasharray:3 2.5"/>`)],
      ["Road built", "the work is done", sv(`${road("")}<path d="M2 8H32" style="stroke:var(--surface);stroke-width:1.2;stroke-dasharray:5 6"/>`)],
      ["Under construction", "doing, next or blocked", sv(`<defs><pattern id="lg-h" width="8" height="8" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><rect width="8" height="8" style="fill:var(--surface)"/><rect width="4" height="8" style="fill:${C}"/></pattern></defs><path d="M1 8H33" style="fill:none;stroke:url(#lg-h);stroke-width:8"/><rect x="3" y="1" width="4" height="14" rx="1" style="fill:var(--surface);stroke:var(--ink);stroke-width:1"/><path d="M3 5l4-2M3 10l4-2" style="stroke:var(--accent);stroke-width:1.8"/>`)],
      ["Not built", "to do", sv(road("stroke-dasharray:6 4;opacity:.35"))],
      ["No work planned", "the road is open", sv(`<path d="M1 8H33" style="fill:none;stroke:${C};stroke-width:3;stroke-dasharray:1 5;stroke-linecap:round;opacity:.35"/>`)],
      ["You are here", "the latest release", sv(`<path d="M17 1V15" style="stroke:var(--accent);stroke-width:2.5;stroke-linecap:round"/>`)],
    ];
    $("#legend").innerHTML = `<svg class="compass" viewBox="0 0 80 80" width="84" height="84" role="img" aria-label="Compass: time runs east, from left to right"><circle cx="40" cy="40" r="36" style="fill:none;stroke:var(--ink);stroke-width:2"/><circle cx="40" cy="40" r="30" style="fill:none;stroke:var(--line-strong);stroke-width:1;stroke-dasharray:2 3"/><path d="M40 8l7 32-7 5-7-5Z" style="fill:var(--sign)"/><path d="M40 72l-7-32 7-5 7 5Z" style="fill:var(--surface);stroke:var(--ink);stroke-width:1.5"/><path d="M8 40l32-7 5 7-5 7Z" style="fill:var(--surface);stroke:var(--ink);stroke-width:1.5"/><path d="M72 40l-32-7-5 7 5 7Z" style="fill:var(--sign)"/><g style="fill:var(--ink);font:900 8px var(--display)" text-anchor="middle"><text x="40" y="6">N</text><text x="40" y="79">S</text><text x="4" y="43">W</text><text x="76" y="43">E</text></g></svg>
      <div><p class="lbl">Map key · time runs east</p><ul class="key">${rows.map(([n, d, s]) => `<li>${s}<span><b>${esc(n)}</b> ${esc(d)}</span></li>`).join("")}</ul>
      <p class="key-note">Road can be built past you-are-here: the work is done, it just hasn't shipped yet.</p></div>`;
  }

  // ---- travel guide
  const taskLink = (t) => `<li class="g-task"><span class="pill s-${esc(t.status)}">${esc(R.STATUS[t.status].label)}</span><a class="mono" href="tasks.html#${esc(t.id)}">${esc(t.id)}</a><span>${rich(t.title)}</span></li>`;

  function milestoneBox(m, hws) {
    const meters = hws.map((id) => {
      const p = R.progress(R.tasksOn(m, id));
      return p.total ? `<div class="g-meter" style="--hw:${R.hwColor(id)}"><span>${esc(R.hwById[id].title)}</span><span class="meter"><i style="width:${(p.done / p.total) * 100}%;background:var(--hw)"></i></span><b class="num">${p.done}/${p.total}</b></div>` : "";
    }).join("");
    const groups = hws.map((id) => {
      const list = R.tasksOn(m, id).slice().sort((a, b) => R.STATUS[a.status].rank - R.STATUS[b.status].rank);
      return list.length ? `<div class="g-group" style="--hw:${R.hwColor(id)}"><h4>${esc(R.hwById[id].title)}</h4><ul>${list.map(taskLink).join("")}</ul></div>` : "";
    }).join("");
    return `<aside class="g-box"><p class="lbl">${esc(m.id)} · milestone</p><h4 class="g-goal">${esc(m.title)}</h4><p>${rich(m.goal)}</p>
      <p class="g-done"><span class="lbl">Done when</span> ${rich(m.done_when)}</p>${meters}${groups}</aside>`;
  }

  function guideHead(r) {
    const tier = R.tierOf(r.version);
    if (tier === "patch") return `<span class="plate" aria-hidden="true">${esc(r.version)}</span>`;
    if (tier === "minor") { const [a, b] = R.parseVer(r.version); return `<span class="exit-stack"><span class="exit-tab">Exit ${a}.${b}</span><span class="sign exit-sign">${esc(r.version)}</span></span>`; }
    return shield(String(R.parseVer(r.version)[0]), 72);
  }

  function guide(id) {
    const box = $("#guide");
    if (id === "0.0.0") {
      const zero = releases.filter((r) => R.cmpVer(r.version, "1.0.0") < 0);
      box.innerHTML = `<article class="gd"><header class="gd-head">${shield("0", 72)}<div><h3>Groundwork and preview</h3><div class="gd-meta">${pill("v-released", "Started")}${R.highways.filter((h) => !h.opens).map((h) => hwChip(h.id)).join("")}</div></div></header>
        <div class="gd-body"><div class="gd-text"><p class="prose drop">The 0.x line is the groundwork and the preview: the toolchain, the first engine and text service, and the daily-driver polish that leads up to 1.0. Every highway that exists at the start begins at this interchange.</p>
        <ul class="g-list">${zero.map((r) => `<li><button type="button" class="linkish" data-go="${esc(r.version)}">${esc(r.version)}</button> ${esc(r.title)} ${relPill(r)}</li>`).join("")}</ul></div></div></article>`;
      return;
    }
    const r = releases.find((x) => x.version === id);
    if (!r) return;
    const tier = R.tierOf(r.version);
    const m = r.milestone && R.milestoneById[r.milestone];
    const hws = R.highwaysOf(r);
    let text;
    if (tier === "major") {
      const paras = r.essay || [];
      text = `<div class="essay drop">${paras.map((p) => `<p>${rich(p)}</p>`).join("")}</div><p class="gd-count">${R.words(paras.join(" "))} words</p>`;
    } else if (tier === "minor") text = `<p class="prose drop">${rich(r.text)}</p>`;
    else text = `<p class="prose big">${rich(r.text)}</p>`;
    box.innerHTML = `<article class="gd gd-${tier}"><header class="gd-head">${guideHead(r)}<div><h3>${esc(r.title)}</h3><div class="gd-meta">${relPill(r)}${r.date ? `<time datetime="${esc(r.date)}">${esc(R.fmtDate(r.date))}</time>` : ""}${hws.map(hwChip).join("")}</div></div></header>
      <div class="gd-body${m ? " has-box" : ""}"><div class="gd-text">${text}</div>${m ? milestoneBox(m, hws) : ""}</div></article>`;
  }

  // ---- road work ahead
  function work() {
    const order = ["doing", "next", "blocked"];
    $("#work-cols").innerHTML = R.highways.map((h) => {
      const list = R.tasks.filter((t) => t.hw && t.hw.id === h.id && order.includes(t.status)).sort((a, b) => R.STATUS[a.status].rank - R.STATUS[b.status].rank);
      return `<section class="work-col" style="--hw:${R.hwColor(h.id)}"><h3>${esc(h.title)}</h3>${list.length ? `<ul>${list.map(taskLink).join("")}</ul>` : `<p class="clear">Clear road</p>`}</section>`;
    }).join("");
  }

  // ---- itinerary
  function itinerary() {
    const groups = [{ label: "0.x", title: "Groundwork and preview", list: [] }];
    for (const r of releases) {
      if (R.cmpVer(r.version, "1.0.0") < 0) groups[0].list.push(r);
      else if (R.tierOf(r.version) === "major") groups.push({ label: shortVer(r.version), title: r.title, list: [r] });
      else groups[groups.length - 1].list.push(r);
    }
    $("#itin").innerHTML = groups.map((g) => `<section class="itin-major"><header class="itin-head">${shield(g.label, 72)}<h3>${esc(g.label)} · ${esc(g.title)}</h3></header>
      ${g.list.map((r) => {
        const tier = R.tierOf(r.version);
        const body = tier === "major" ? (r.essay || []).map((p) => `<p>${rich(p)}</p>`).join("") : `<p>${rich(r.text)}</p>`;
        return `<article class="itin-rel itin-${tier}" id="i-${esc(r.version)}"><header><span class="mono ver">${esc(r.version)}</span><h4>${esc(r.title)}</h4>${relPill(r)}${r.date ? `<time datetime="${esc(r.date)}">${esc(R.fmtDate(r.date))}</time>` : ""}</header>
          <div class="itin-text">${body}</div><button type="button" class="ctl" data-go="${esc(r.version)}">Show on map</button></article>`;
      }).join("")}</section>`).join("");
  }

  // ---- wiring
  const L = CompletionistMapLayout.layoutMap(data);
  const map = CompletionistMap.mount($("#map-frame"), L, { onSelect: (id) => select(id, false) });

  function select(id, scroll) {
    map.select(id);
    guide(id);
    try { history.replaceState(null, "", "#v" + id); } catch {}
    if (scroll) {
      map.scrollToX(map.xOf(id));
      $("#map-frame").scrollIntoView({ block: "nearest", behavior: matchMedia("(prefers-reduced-motion: reduce)").matches ? "auto" : "smooth" });
    }
  }
  const valid = (id) => id === "0.0.0" || releases.some((r) => r.version === id);
  const fromHash = () => { const h = decodeURIComponent(location.hash.slice(1)); return h.startsWith("v") && valid(h.slice(1)) ? h.slice(1) : null; };

  R.bindThemeButton($("#theme"));
  hero(); vms(); posts(); chips(map); legend(); work(); itinerary();
  const first = fromHash() || (releases.find((r) => r.status === "next") || latest || releases[0] || { version: "0.0.0" }).version;
  select(first, false);
  map.scrollToX(map.xOf("now"), false);

  $("#go-start").addEventListener("click", () => map.scrollToX(0));
  $("#go-now").addEventListener("click", () => map.scrollToX(map.xOf("now")));
  $("#go-end").addEventListener("click", () => map.scrollToX(map.xOf("end")));
  $("#fit").addEventListener("click", (e) => { const on = e.currentTarget.getAttribute("aria-pressed") !== "true"; e.currentTarget.setAttribute("aria-pressed", String(on)); map.fit(on); });
  document.addEventListener("click", (e) => { const b = e.target.closest("[data-go]"); if (b) select(b.dataset.go, true); });
  addEventListener("hashchange", () => { const id = fromHash(); if (id) select(id, true); });
  document.fonts.ready.then(() => map.redraw(CompletionistMapLayout.layoutMap(data)));
})();
