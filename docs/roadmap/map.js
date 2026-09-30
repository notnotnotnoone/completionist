// Draws the roadmap highway map from TyperMapLayout.layoutMap(data) and handles selection, spotlight, panning and fit.
// Exposes window.TyperMap.mount(frameEl, layout, { onSelect }).
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
      const fs = Math.min(14, (112 / width(h.title.toUpperCase(), "italic 900 14px Overpass")) * 14).toFixed(1);
      const blurb = wrap(h.opens ? `${h.blurb}. Opens at ${h.opens.replace(/\.0$/, "")}.` : h.blurb, "600 11px Overpass", 158, 2);
      return `<g class="lane-label" data-hw="${lane.id}"><rect x="16" y="${lane.y - 20}" width="158" height="40" rx="20" style="fill:${col(lane.id)}"/>${T(34, lane.y + 5, h.title.toUpperCase(), `fill:var(--sign-ink);font:italic 900 ${fs}px Overpass;letter-spacing:.04em`)}${blurb.map((b, k) => T(18, lane.y + 36 + k * 13, b, "fill:var(--muted);font:600 11px Overpass")).join("")}</g>`;
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

  window.TyperMap = { mount };
})();
