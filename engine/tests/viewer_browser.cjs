"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const { chromium } = require("playwright");

const root = path.resolve(__dirname, "..");
const html = fs.readFileSync(path.join(root, "src/completionist_engine/viewer.html"), "utf8");
const outputDir = path.resolve(root, "..", ".superpowers", "sdd", "2026-10-02-v2-desktop-overhaul", "viewer-artifacts");
const chromePath = "C:/Program Files/Google/Chrome/Application/chrome.exe";
const screenshotFixture = '<svg xmlns="http://www.w3.org/2000/svg" width="640" height="360" viewBox="0 0 640 360"><rect width="640" height="360" fill="#eef2ec"/><rect x="28" y="28" width="584" height="44" rx="8" fill="#0a5a3d"/><circle cx="54" cy="50" r="10" fill="#ffffff"/><rect x="28" y="96" width="584" height="220" rx="8" fill="#fbfcf8" stroke="#d5ddd6"/><text x="52" y="132" font-family="Segoe UI, sans-serif" font-size="20" fill="#16221c">Synthetic editor window</text><text x="52" y="174" font-family="Segoe UI, sans-serif" font-size="16" fill="#55635b">Fixture text shown in the request receipt.</text></svg>';

function deferred() {
  let resolve;
  const promise = new Promise((done) => { resolve = done; });
  return { promise, resolve };
}

function request(id, outcome, reply) {
  return { id, at: 1790960400, app: "editor.exe", model: "fixture/model", reply, ttft_ms: 80, total_ms: 160, outcome };
}

function detail(id, reply, hasImage = false) {
  return {
    id, at: 1790960400, app: "editor.exe", outcome: id === "r3" ? "cancelled" : "ok", model: "fixture/model", ttft_ms: 80, total_ms: 160,
    prompt: `synthetic prompt ${id}`, suffix: id === "r3" ? "" : "tail", reply, attempts: [{ model: "fixture/model", ok: true, ms: 160 }],
    events: [{ ms: 1, kind: "fixture", id }], settings: { api_key_set: true },
    screen: id === "r3" ? null : { app: "editor.exe", title: "Fixture", has_image: hasImage, text: "screen text read", sent: "screen text sent" },
  };
}

async function json(route, body) {
  await route.fulfill({ status: 200, contentType: "application/json", body: JSON.stringify(body) });
}

async function makePage(browser, viewport = { width: 1280, height: 900 }, colorScheme = "light", reducedMotion = "no-preference", options = {}) {
  const context = await browser.newContext({ viewport, colorScheme, reducedMotion, deviceScaleFactor: options.deviceScaleFactor || 1 });
  const page = await context.newPage();
  page.setDefaultTimeout(3000);
  if (options.beforeLoad) await page.addInitScript(options.beforeLoad);
  const runtimeState = { paused: false, private_mode: false };
  let preview = options.preview ? { id: "synthetic-preview", app: "editor.exe", title: "Fixture", prompt: "synthetic preview prompt", suffix: "next" } : null;
  let requestsCleared = false;
  const settings = {
    phrase: {
      api_key_set: true, instructions: "Finish the current sentence.", instructions_default: "Finish the current sentence.", instructions_custom: false, prompt_test_scenarios: [],
      enabled: true, models: ["fixture/model"], mode: "apps", provider_sort: "", completion_length: "default", spelling: "auto", context_source: "ocr",
      reasoning_effort: "none", provider_order: [], allow_fallbacks: true, provider_ignore: [], max_price_input: 0, max_price_output: 0, require_parameters: false, zdr: false,
      debounce: 0.3, min_chars: 2, trigger: "pause", dismiss_cooldown: 1, timeout: 10, failure_limit: 3, failure_pause: 30,
      writing_style: "match", preserve_casing: true, multiline: false, avoid_phrases: [], max_tokens: 60, temperature: 0.2,
      screen_context: true, context_apps: [], context_before: 100, fim: false, context_after: 0, preview_context: true, log_mode: "full", log_retention_minutes: 0,
    },
    words: { limit: 5, next: true, chunks: true, typo_correction: false, next_threshold: 0.5 },
    learning: { enabled: true, promote_after: 3 },
    popup: { font_size: 12, width_scale: 1 },
    privacy: { private_mode: false, pause_minutes: 5 },
    apps: { profiles: [], allow: [], block: [] },
    hotkeys: { partial_accept: "ctrl+right", dismiss: "escape", pause: "ctrl+alt+p" },
  };
  await page.route("http://viewer.test/**", async (route) => {
    const url = new URL(route.request().url());
    if (url.pathname === "/" || url.pathname === "/viewer") {
      await route.fulfill({ status: 200, contentType: "text/html", body: html });
      return;
    }
    if (url.pathname.startsWith("/api/requests/") && url.pathname.endsWith("/screenshot")) {
      const id = url.pathname.split("/").at(-2);
      const gate = globalThis.shotGates?.[id];
      if (gate) { gate.started.resolve(); await gate.promise; }
      await route.fulfill({ status: 200, contentType: "image/svg+xml", body: screenshotFixture });
      return;
    }
    if (url.pathname === "/api/requests/clear") {
      const gate = globalThis.clearGate;
      if (gate) { gate.started.resolve(); await gate.promise; }
      if (gate?.fail) { await route.fulfill({ status: 500, contentType: "application/json", body: JSON.stringify({ error: "synthetic clear failure" }) }); return; }
      requestsCleared = true; await json(route, { cleared: true }); return;
    }
    if (url.pathname.startsWith("/api/requests/")) {
      const id = url.pathname.split("/").at(-1);
      if (id === "r1") {
        const gate = globalThis.detailGates?.r1;
        if (gate) { gate.started.resolve(); await gate.promise; }
      }
      await json(route, detail(id, `synthetic receipt ${id}`, id === "r1" || id === "r2"));
      return;
    }
    if (url.pathname === "/api/requests") {
      const result = url.searchParams.get("result") || "";
      const gate = globalThis.listGates?.[result];
      if (gate) { gate.started.resolve(); await gate.promise; }
      const rows = requestsCleared ? [] : result === "failed" ? [request("old", "failed", "old filtered result")]
        : result === "ok" ? [request("new", "ok", "new filtered result")]
          : [request("r1", "ok", "first receipt"), request("r2", "ok", "second receipt"), request("r3", "cancelled", "")];
      await json(route, { available: true, total: rows.length, requests: rows });
      return;
    }
    if (url.pathname === "/api/runtime") {
      if (route.request().method() === "POST") {
        const action = JSON.parse(route.request().postData()).action;
        if (action === "pause") runtimeState.paused = true;
        if (action === "resume") runtimeState.paused = false;
        if (action === "private_on") runtimeState.private_mode = true;
        if (action === "private_off") runtimeState.private_mode = false;
      }
      await json(route, { ...runtimeState }); return;
    }
    if (url.pathname === "/api/context") {
      if (route.request().method() === "POST") preview = null;
      await json(route, { preview }); return;
    }
    if (url.pathname === "/api/settings") { await json(route, { ...settings, privacy: { ...settings.privacy, private_mode: runtimeState.private_mode } }); return; }
    if (url.pathname === "/api/words") { await json(route, { available: true, learning: true, total: 2, words: [{ word: "completion", count: 9 }, { word: "viewer", count: 4 }] }); return; }
    if (url.pathname === "/api/trigrams") { await json(route, { available: true, learning: true, total: 1, trigrams: [{ words: ["finish", "this", "sentence"], count: 3 }] }); return; }
    if (url.pathname === "/api/stats") { await json(route, { available: true, keystrokes_saved: 4, words: { rate: 0.5, accepted: 2, shown: 4 }, phrases: { accepted: 1, partial: 0, shown: 2 }, dismissed: 1, daily: [{ day: "2026-10-02", saved: 4, shown: 4, accepted: 2 }], apps: [], providers: [] }); return; }
    await route.fulfill({ status: 404, contentType: "application/json", body: JSON.stringify({ error: "fixture route missing" }) });
  });
  await page.goto("http://viewer.test/?t=synthetic#requests");
  await page.getByRole("heading", { name: "Phrase Requests" }).waitFor();
  return { context, page };
}

async function testLatestRequestList(browser) {
  globalThis.listGates = { failed: { ...deferred(), started: deferred() } };
  const { context, page } = await makePage(browser);
  try {
    await page.getByRole("button", { name: /Open request/ }).first().waitFor();
    await page.locator("#requests-result").selectOption("failed");
    await globalThis.listGates.failed.started.promise;
    await page.locator("#requests-result").selectOption("ok");
    await page.getByText("new filtered result").waitFor();
    globalThis.listGates.failed.resolve();
    await page.waitForTimeout(100);
    assert.equal(await page.getByText("new filtered result").count(), 1, "the newest request filter remains visible after the older response resolves");
    assert.equal(await page.getByText("old filtered result").count(), 0, "an older request-list response is discarded");
  } finally {
    globalThis.listGates.failed.resolve();
    await context.close();
    delete globalThis.listGates;
  }
}

async function testDialogAndLateDetail(browser) {
  globalThis.detailGates = { r1: { ...deferred(), started: deferred() } };
  const { context, page } = await makePage(browser);
  try {
    const opener = page.getByRole("button", { name: /Open request/ }).first();
    await opener.waitFor();
    await opener.focus();
    await opener.press("Enter");
    await globalThis.detailGates.r1.started.promise;
    const dialog = page.getByRole("dialog", { name: /request/i });
    await dialog.waitFor();
    assert.equal(await page.locator("#main").evaluate((node) => node.inert), true, "the page behind the modal is inert");
    await page.getByRole("button", { name: "Close request" }).press("Escape");
    assert.equal(await dialog.count(), 0, "Escape closes the receipt");
    const secondOpener = page.getByRole("button", { name: /Open request/ }).nth(1);
    await secondOpener.focus();
    await secondOpener.press("Space");
    await page.getByText("synthetic receipt r2").waitFor();
    globalThis.detailGates.r1.resolve();
    await page.waitForTimeout(100);
    assert.equal(await page.getByRole("dialog").count(), 1, "the newer receipt remains open");
    assert.equal(await page.getByText("synthetic receipt r2").count(), 1, "an older detail cannot replace the newer receipt");
    assert.equal(await page.getByText("synthetic receipt r1").count(), 0, "a late closed receipt is ignored");
  } finally {
    globalThis.detailGates.r1.resolve();
    await context.close();
    delete globalThis.detailGates;
  }
}

async function testDialogFocusAndScreenshots(browser) {
  const { context, page } = await makePage(browser);
  const opener = page.getByRole("button", { name: /Open request/ }).nth(1);
  await opener.focus();
  await opener.press("Space");
  const dialog = page.getByRole("dialog", { name: /request/i });
  await dialog.waitFor();
  await page.getByText("synthetic receipt r2").waitFor();
  await dialog.getByText("screen text read").waitFor();
  await dialog.getByText("screen text sent").waitFor();
  const close = page.getByRole("button", { name: "Close request" });
  const screenshotLink = dialog.locator('a[title="Open full size"]');
  await screenshotLink.waitFor();
  await screenshotLink.focus();
  await page.keyboard.press("Tab");
  assert.equal(await close.evaluate((node) => node === document.activeElement), true, "Tab wraps from the last control to the first");
  await page.keyboard.press("Shift+Tab");
  assert.equal(await screenshotLink.evaluate((node) => node === document.activeElement), true, "Shift+Tab wraps from the first control to the last");
  await page.keyboard.press("Escape");
  assert.equal(await opener.evaluate((node) => node === document.activeElement), true, "Escape restores focus to the activating request button");
  await context.close();
}

async function testLateScreenshot(browser) {
  globalThis.shotGates = { r1: { ...deferred(), started: deferred() } };
  const beforeLoad = () => {
    window.__screenshotFetches = []; window.__revokedViewerBlobs = [];
    const fetch = window.fetch.bind(window);
    window.fetch = (input, init = {}) => {
      if (String(input).endsWith("/screenshot")) window.__screenshotFetches.push({ cache: init.cache, token: init.headers?.["X-Completionist-Token"] });
      return fetch(input, init);
    };
    const revoke = URL.revokeObjectURL.bind(URL);
    URL.revokeObjectURL = (value) => { window.__revokedViewerBlobs.push(value); revoke(value); };
  };
  const { context, page } = await makePage(browser, undefined, undefined, undefined, { beforeLoad });
  try {
    const first = page.getByRole("button", { name: /Open request/ }).first();
    await first.focus(); await first.press("Enter");
    await globalThis.shotGates.r1.started.promise;
    await page.getByRole("button", { name: "Close request" }).press("Escape");
    const second = page.getByRole("button", { name: /Open request/ }).nth(1);
    await second.focus(); await second.press("Space");
    const dialog = page.getByRole("dialog");
    await dialog.getByText("synthetic receipt r2").waitFor();
    await dialog.locator('a[title="Open full size"]').waitFor();
    globalThis.shotGates.r1.resolve();
    await page.waitForTimeout(100);
    assert.equal(await dialog.locator('a[title="Open full size"]').count(), 1, "the older screenshot cannot replace or add to the latest receipt");
    assert.deepEqual(await page.evaluate(() => window.__screenshotFetches[0]), { cache: "no-store", token: "synthetic" }, "screenshots use the authenticated no-store fetch");
    await page.keyboard.press("Escape");
    assert.equal(await page.evaluate(() => window.__revokedViewerBlobs.length), 2, "late and current screenshot blobs are both revoked");
  } finally {
    globalThis.shotGates.r1.resolve();
    await context.close();
    delete globalThis.shotGates;
  }
}

async function testAbsentContextIsExplained(browser) {
  const { context, page } = await makePage(browser);
  try {
    const opener = page.getByRole("button", { name: /Open request/ }).nth(2);
    await opener.focus(); await opener.press("Enter");
    const dialog = page.getByRole("dialog");
    await dialog.getByText("synthetic receipt r3").waitFor();
    await dialog.getByText("No window context was read for this request").waitFor();
    await dialog.getByText("No window context was sent.").waitFor();
    await dialog.getByText("No text after the caret was sent.").waitFor();
    await dialog.getByRole("heading", { name: "Cancelled request" }).waitFor();
  } finally { await context.close(); }
}

async function testClearDropsReceiptAndLateScreenshot(browser) {
  globalThis.shotGates = { r1: { ...deferred(), started: deferred() } };
  const beforeLoad = () => {
    window.__revokedViewerBlobs = [];
    const revoke = URL.revokeObjectURL.bind(URL);
    URL.revokeObjectURL = (value) => { window.__revokedViewerBlobs.push(value); revoke(value); };
  };
  const { context, page } = await makePage(browser, undefined, undefined, undefined, { beforeLoad });
  try {
    const opener = page.getByRole("button", { name: /Open request/ }).first();
    await opener.focus(); await opener.press("Enter");
    await globalThis.shotGates.r1.started.promise;
    await page.keyboard.press("Escape");
    await page.getByRole("button", { name: "Clear Log" }).click();
    await page.getByRole("button", { name: "Confirm Clear" }).click();
    await page.getByText("No requests yet").waitFor();
    globalThis.shotGates.r1.resolve();
    await page.waitForTimeout(100);
    assert.equal(await page.getByRole("dialog").count(), 0, "clearing leaves no receipt open");
    assert.equal(await page.locator("#requests-body .log").count(), 0, "clearing removes the visible request list");
    assert.equal(await page.getByRole("link", { name: /window as it looked/ }).count(), 0, "a late screenshot is not rendered after clear");
    assert.equal(await page.evaluate(() => window.__revokedViewerBlobs.length), 1, "the late screenshot blob is revoked");
  } finally {
    globalThis.shotGates.r1.resolve();
    await context.close();
    delete globalThis.shotGates;
  }
}

async function testClearInvalidatesPendingRequestList(browser) {
  globalThis.listGates = { failed: { ...deferred(), started: deferred() } };
  globalThis.clearGate = { ...deferred(), started: deferred() };
  const { context, page } = await makePage(browser);
  try {
    await page.getByRole("button", { name: /Open request/ }).first().waitFor();
    await page.locator("#requests-result").selectOption("failed");
    await globalThis.listGates.failed.started.promise;
    await page.getByRole("button", { name: "Clear Log" }).click();
    await page.getByRole("button", { name: "Confirm Clear" }).click();
    await globalThis.clearGate.started.promise;

    globalThis.listGates.failed.resolve();
    await page.waitForTimeout(100);
    assert.equal(await page.getByText("old filtered result").count(), 0, "a request-list response that predates clear cannot render while clear is pending");

    globalThis.clearGate.resolve();
    await page.getByText("No requests match").waitFor();
    assert.equal(await page.getByText("old filtered result").count(), 0, "the successful clear reload remains empty");
  } finally {
    globalThis.listGates.failed.resolve();
    globalThis.clearGate.resolve();
    await context.close();
    delete globalThis.listGates;
    delete globalThis.clearGate;
  }
}

async function testClearFailureKeepsRequestsUsable(browser) {
  globalThis.clearGate = { ...deferred(), started: deferred(), fail: true };
  const { context, page } = await makePage(browser);
  try {
    await page.locator("#requests-result").selectOption("failed");
    await page.getByText("old filtered result").waitFor();
    await page.getByRole("button", { name: "Clear Log" }).click();
    await page.getByRole("button", { name: "Confirm Clear" }).click();
    await globalThis.clearGate.started.promise;
    globalThis.clearGate.resolve();
    await page.getByText("synthetic clear failure").waitFor();
    await page.getByText("old filtered result").waitFor();
    assert.equal(await page.getByText("old filtered result").count(), 1, "a failed clear leaves the request log available and reloads its current filter");
  } finally {
    globalThis.clearGate.resolve();
    await context.close();
    delete globalThis.clearGate;
  }
}

async function testDirtySettingsNavigationGuard(browser) {
  const { context, page } = await makePage(browser);
  try {
    await page.getByRole("tab", { name: "Settings" }).click();
    const limit = page.locator('[name="words.limit"]');
    await limit.waitFor();
    await limit.fill("6");
    let decision = page.waitForEvent("dialog");
    page.once("dialog", (dialog) => dialog.dismiss());
    await page.getByRole("tab", { name: "Requests" }).click();
    await decision;
    assert.equal(await page.getByRole("tab", { name: "Settings" }).getAttribute("aria-selected"), "true", "cancel keeps the user in Settings");
    decision = page.waitForEvent("dialog");
    page.once("dialog", (dialog) => dialog.accept());
    await page.getByRole("tab", { name: "Requests" }).click();
    await decision;
    await page.getByRole("heading", { name: "Phrase Requests" }).waitFor();
    assert.equal(await page.getByRole("tab", { name: "Requests" }).getAttribute("aria-selected"), "true", "accept discards pending edits and navigates");
  } finally { await context.close(); }
}

async function testLayouts(browser) {
  fs.mkdirSync(outputDir, { recursive: true });
  const beforeLoad = () => { window.confirm = () => true; };
  for (const width of [390, 560, 1280]) {
    for (const colorScheme of ["light", "dark"]) {
      const { context, page } = await makePage(browser, { width, height: 900 }, colorScheme, "reduce", { beforeLoad });
      assert.equal(await page.evaluate(() => matchMedia("(prefers-reduced-motion: reduce)").matches), true, "the reduced-motion preference is active in the browser fixture");
      for (const [name, slug] of [["Words", "words"], ["Phrases", "phrases"], ["Requests", "requests"], ["Stats", "stats"], ["Settings", "settings"]]) {
        await page.getByRole("tab", { name }).click();
        await page.locator(`#panel-${slug}`).waitFor({ state: "visible" });
        const contentSelector = { words: "#words-body .rows", phrases: "#phrases-body .rows", requests: "#requests-body .log", stats: "#stats-body .cards", settings: "#runtime h3" }[slug];
        await page.locator(contentSelector).waitFor({ state: "visible" });
        const overflow = await page.evaluate(() => document.documentElement.scrollWidth > document.documentElement.clientWidth);
        assert.equal(overflow, false, `no page overflow on ${name} at ${width}px in ${colorScheme} theme`);
        await page.screenshot({ path: path.join(outputDir, `${slug}-${width}-${colorScheme}.png`), fullPage: true });
        if (slug === "requests" && width <= 640) assert.equal(await page.locator("#requests-body .scroll-hint").isVisible(), true, "narrow request tables explain horizontal scrolling");
        if (slug === "requests") {
          const opener = page.getByRole("button", { name: /Open request/ }).first();
          await opener.focus(); await opener.press("Enter");
          const dialog = page.getByRole("dialog");
          await dialog.getByText("synthetic receipt r1").waitFor();
          await dialog.locator('a[title="Open full size"]').waitFor();
          await page.screenshot({ path: path.join(outputDir, `request-receipt-${width}-${colorScheme}.png`), fullPage: true });
          await page.keyboard.press("Escape");
        }
      }
      if (width === 390) {
        await page.getByRole("tab", { name: "Words" }).focus();
        await page.keyboard.press("Home");
        assert.equal(await page.getByRole("tab", { name: "Words" }).getAttribute("aria-selected"), "true", "Home moves the selected tab to Words");
        await page.getByRole("tab", { name: "Settings" }).focus();
        await page.keyboard.press("End");
        assert.equal(await page.getByRole("tab", { name: "Settings" }).getAttribute("aria-selected"), "true", "End moves the selected tab to Settings");
      }
      await context.close();
    }
  }
  const { context, page } = await makePage(browser, { width: 195, height: 800 }, "light", "reduce", { beforeLoad, deviceScaleFactor: 2 });
  for (const [name, slug] of [["Words", "words"], ["Phrases", "phrases"], ["Requests", "requests"], ["Stats", "stats"], ["Settings", "settings"]]) {
    await page.getByRole("tab", { name }).click();
    await page.locator(`#panel-${slug}`).waitFor({ state: "visible" });
    const contentSelector = { words: "#words-body .rows", phrases: "#phrases-body .rows", requests: "#requests-body .log", stats: "#stats-body .cards", settings: "#runtime h3" }[slug];
    await page.locator(contentSelector).waitFor({ state: "visible" });
    const dimensions = await page.evaluate(() => {
      const target = document.querySelector("#panel-settings .set .t");
      const ancestors = [];
      for (let node = target; node; node = node.parentElement) {
        const rect = node.getBoundingClientRect(), style = getComputedStyle(node);
        ancestors.push({ tag: node.tagName, className: typeof node.className === "string" ? node.className : "", width: Math.round(rect.width), left: Math.round(rect.left), right: Math.round(rect.right), minWidth: style.minWidth, display: style.display });
        if (node.id === "form") break;
      }
      const offenders = [...document.querySelectorAll("body *")].map((node) => { const rect = node.getBoundingClientRect(); return { tag: node.tagName, className: typeof node.className === "string" ? node.className : "", text: (node.innerText || "").slice(0, 32), left: Math.round(rect.left), right: Math.round(rect.right), width: Math.round(rect.width) }; }).filter((node) => node.right > document.documentElement.clientWidth + 1).slice(0, 8);
      return { viewport: document.documentElement.clientWidth, document: document.documentElement.scrollWidth, ancestors, offenders };
    });
    assert.equal(dimensions.document > dimensions.viewport, false, `no page overflow at 200%-equivalent ${slug} width: ${JSON.stringify(dimensions)}`);
    if (slug === "requests" || slug === "settings") await page.screenshot({ path: path.join(outputDir, `${slug}-200-percent.png`), fullPage: true });
  }
  await context.close();
}

async function testHiddenPolling(browser) {
  const beforeLoad = () => {
    const realSet = window.setInterval.bind(window), realClear = window.clearInterval.bind(window);
    let id = -1;
    window.__viewerPolls = new Map(); window.__requestListCalls = 0;
    window.setInterval = (fn, delay, ...args) => {
      if (delay === 3000) { const next = id--; window.__viewerPolls.set(next, { fn, args }); return next; }
      return realSet(fn, delay, ...args);
    };
    window.clearInterval = (timer) => { if (window.__viewerPolls.delete(timer)) return; realClear(timer); };
    const fetch = window.fetch.bind(window);
    window.fetch = (...args) => { if (String(args[0]).includes("/api/requests?")) window.__requestListCalls++; return fetch(...args); };
    Object.defineProperty(document, "hidden", { configurable: true, get: () => window.__testHidden || false });
    window.__setTestHidden = (hidden) => { window.__testHidden = hidden; document.dispatchEvent(new Event("visibilitychange")); };
  };
  const { context, page } = await makePage(browser, { width: 1280, height: 900 }, "light", "reduce", { beforeLoad });
  const opener = page.getByRole("button", { name: /Open request/ }).first();
  await opener.waitFor();
  await opener.focus();
  assert.equal(await page.evaluate(() => window.__viewerPolls.size), 1, "Requests starts one visible polling timer");
  const beforePoll = await page.evaluate(() => window.__requestListCalls);
  await page.evaluate(() => window.__viewerPolls.values().next().value.fn());
  await page.waitForFunction((count) => window.__requestListCalls === count + 1, beforePoll);
  assert.equal(await opener.evaluate((node) => node.isConnected && node === document.activeElement), true, "unchanged polling keeps the focused request row stable");
  const beforeHide = await page.evaluate(() => window.__requestListCalls);
  await page.evaluate(() => window.__setTestHidden(true));
  assert.equal(await page.evaluate(() => window.__viewerPolls.size), 0, "backgrounding clears the polling timer");
  assert.equal(await page.evaluate(() => window.__requestListCalls), beforeHide, "backgrounding does not issue an extra list fetch");
  await page.evaluate(() => window.__setTestHidden(false));
  await page.waitForFunction((count) => window.__requestListCalls === count + 1, beforeHide);
  assert.equal(await page.evaluate(() => window.__viewerPolls.size), 1, "returning to the visible Requests page restarts polling");
  await context.close();
}

async function testRuntimeActions(browser) {
  const { context, page } = await makePage(browser, { width: 1280, height: 900 }, "light", "reduce", { preview: true });
  try {
    await page.getByRole("tab", { name: "Settings" }).click();
    await page.getByRole("heading", { name: "Session actions · apply immediately" }).waitFor();
    const send = page.getByRole("button", { name: "Send", exact: true });
    await send.waitFor();
    assert.equal(await send.isEnabled(), true, "preview sending is available in the active public session");
    await page.getByRole("button", { name: "Pause Now" }).click();
    await page.getByRole("button", { name: "Resume Now" }).waitFor();
    assert.equal(await send.isDisabled(), true, "pause disables preview sending");
    await page.getByRole("button", { name: "Resume Now" }).click();
    await page.getByRole("button", { name: "Pause Now" }).waitFor();
    await page.getByRole("button", { name: "Turn Private Mode On" }).click();
    await page.getByRole("button", { name: "Turn Private Mode Off" }).waitFor();
    assert.equal(await send.isDisabled(), true, "private mode disables preview sending");
    await page.getByRole("button", { name: "Turn Private Mode Off" }).click();
    await page.getByRole("button", { name: "Send", exact: true }).click();
    await page.getByText("Context sent.").waitFor();
    assert.equal(await page.getByRole("button", { name: "Send", exact: true }).count(), 0, "sending clears the synthetic preview");
  } finally { await context.close(); }
}

(async () => {
  fs.mkdirSync(outputDir, { recursive: true });
  const browser = await chromium.launch({ headless: true, executablePath: chromePath });
  const failures = [];
  for (const [name, test] of [
    ["latest request filter wins", testLatestRequestList],
    ["modal ignores a late detail and restores focus", testDialogAndLateDetail],
    ["modal traps keyboard focus and supports button activation", testDialogFocusAndScreenshots],
    ["modal ignores a late screenshot", testLateScreenshot],
    ["receipts explain absent context and suffix", testAbsentContextIsExplained],
    ["clear closes receipt work and removes screenshots", testClearDropsReceiptAndLateScreenshot],
    ["clear invalidates request-list fetches immediately", testClearInvalidatesPendingRequestList],
    ["clear failure leaves request list usable", testClearFailureKeepsRequestsUsable],
    ["dirty settings navigation remains guarded", testDirtySettingsNavigationGuard],
    ["polling stops while Requests is backgrounded", testHiddenPolling],
    ["runtime actions and preview privacy guards work", testRuntimeActions],
    ["responsive themes have no page overflow", testLayouts],
  ]) {
    try { await test(browser); console.log(`PASS ${name}`); }
    catch (error) { failures.push({ name, error }); console.error(`FAIL ${name}: ${error.message}`); }
  }
  await browser.close();
  if (failures.length) process.exitCode = 1;
  else console.log(`Screenshots: ${outputDir}`);
})().catch((error) => { console.error(error); process.exitCode = 1; });
