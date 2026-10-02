import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import vm from 'node:vm';

const html = readFileSync(new URL('../engine/src/completionist_engine/viewer.html', import.meta.url), 'utf8');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
new vm.Script(script); // Parse the complete shipped script, including event handlers.
const settings = script.slice(script.indexOf('const OPTIONS ='), script.indexOf('form.addEventListener("input"')) +
  script.slice(script.indexOf('function revealAncestors('), script.indexOf('addEventListener("beforeunload"'));
class Node {
  constructor(tag, props = {}, ...children) {
    this.tagName = tag.toUpperCase(); this.dataset = {}; this.children = []; this.events = {}; this.value = ''; this.isConnected = true;
    this.classList = { toggle() {} }; Object.assign(this, props); this.append(...children);
  }
  append(...children) { for (const n of children.filter(x => x instanceof Node)) { n.parentElement = this; this.children.push(n); } }
  prepend(...children) { const old = this.children; this.children = []; this.append(...children, ...old); }
  replaceChildren(...children) { this.children = []; this.append(...children); }
  setAttribute(key, value) { this[key] = value; }
  addEventListener(key, fn) { this.events[key] = fn; }
  remove() { this.parentElement.children = this.parentElement.children.filter(n => n !== this); }
  focus() { this.focused = true; }
  matches(selector) {
    if (selector === '.profile') return this.className === 'profile';
    if (selector === '[data-profile-key]') return 'profileKey' in this.dataset;
    if (selector === '[data-key]') return 'key' in this.dataset;
    if (selector === 'details[data-fold]') return this.tagName === 'DETAILS' && 'fold' in this.dataset;
    const name = selector.match(/^\[name="(.+)"\]$/); return name ? this.name === name[1] : false;
  }
  querySelectorAll(selector) { return this.children.flatMap(n => [...(n.matches(selector) ? [n] : []), ...n.querySelectorAll(selector)]); }
  querySelector(selector) { return this.querySelectorAll(selector)[0] || null; }
}
function setup() {
  const form = new Node('form'), extra = new Map(['savebar', 'savenote'].map(id => [id, new Node('div')]));
  const findId = (node, id) => node.id === id ? node : node.children.map(n => findId(n, id)).find(Boolean);
  const calls = [], ctx = vm.createContext({ console, form, current: 'settings', document: { body: new Node('body') },
    $: id => id === 'form' ? form : findId(form, id) || extra.get(id), el: (tag, props, ...kids) => new Node(tag, props, ...kids),
    api: async (path, body) => { calls.push({ path, body }); return path === '/api/runtime' ? { paused: false, private_mode: false } : { available: true, preview: null }; },
    toast() {}, banner() {}, armable() {}, Event: class {},
  });
  vm.runInContext(settings.replace('const form = $("form");', ''), ctx);
  vm.runInContext(`loaded = { words: {}, learning: {}, phrase: {api_key_set: false, instructions: '', instructions_default: ''}, popup: {}, privacy: {}, apps: {profiles: []}, hotkeys: {} };
    for (const [, , , rows] of GROUPS) for (const [path, kind, , , opt = {}] of rows) {
      const [sec, key] = path.split('.');
      if (key !== 'profiles') loaded[sec][key] = kind === 'bool' ? false : kind === 'number' ? opt.min : kind === 'list' ? [] : kind === 'select' ? opt.options[0][0] : '';
    }
    renderSettings();`, ctx);
  return { ctx, form, calls, run: code => vm.runInContext(code, ctx) };
}

test('all approved settings appear once, enums round-trip and unchanged form saves nothing', () => {
  const { run, form } = setup();
  assert.equal(JSON.stringify(run('readForm()')), '{}');
  const paths = form.querySelectorAll('[data-key]').map(n => n.name);
  assert.equal(new Set(paths).size, paths.length);
  for (const path of ['words.typo_correction', 'phrase.zdr', 'privacy.pause_minutes', 'hotkeys.partial_accept', 'phrase.preview_context']) assert.ok(paths.includes(path));
  form.querySelector('[name="phrase.spelling"]').value = 'canadian';
  assert.equal(run('readForm().phrase.spelling'), 'canadian');
});

test('profile editor round-trips inherited values and boolean false and removes rows', () => {
  const { run, form } = setup();
  run(`loaded.apps.profiles = [{app: 'notepad.exe', learning: false, word_limit: 4, spelling: 'british'}]; renderSettings();`);
  assert.equal(JSON.stringify(run('readProfiles()')), '[{"app":"notepad.exe","spelling":"british","learning":false,"word_limit":4}]');
  // Profile object key order must not turn an unchanged row into an unsaved edit.
  assert.equal(JSON.stringify(run('readForm()')), '{}');
  const row = form.querySelector('.profile'); row.children[1].events.click();
  assert.equal(JSON.stringify(run('readForm().apps.profiles')), '[]');
});

test('open Advanced tiles survive rerender and validation reveals every ancestor', () => {
  const { run, form } = setup();
  const nodes = form.querySelectorAll('details[data-fold]');
  nodes.find(n => n.dataset.fold === 'advanced').open = true;
  nodes.find(n => n.dataset.fold === 'routing').open = true;
  run('renderSettings()');
  assert.ok(form.querySelectorAll('details[data-fold]').find(n => n.dataset.fold === 'routing').open);
  run(`revealAncestors(form.querySelector('[name="phrase.log_mode"]'))`);
  assert.ok(form.querySelectorAll('details[data-fold]').find(n => n.dataset.fold === 'advanced').open);
  assert.ok(form.querySelectorAll('details[data-fold]').find(n => n.dataset.fold === 'context').open);
});

test('preview is displayed as exact text and Send/Cancel post its identity', async () => {
  const { run, form, calls } = setup();
  run(`runtimeState = {paused:false, private_mode:false}; contextState = {preview:{id:42,app:'notepad.exe',title:'Draft',prompt:'<exact>\\ntext'}}; renderRuntime();`);
  const box = form.children[0];
  const prompt = box.children.find(n => n.tagName === 'PRE'); assert.equal(prompt.textContent, '<exact>\ntext');
  const buttons = box.children.at(-1).children;
  await buttons[0].events.click();
  assert.equal(JSON.stringify(calls.find(c => c.path === '/api/context' && c.body)?.body), '{"action":"send","id":42}');
  calls.length = 0;
  await run('contextAction("cancel", 42)');
  assert.equal(JSON.stringify(calls.find(c => c.path === '/api/context' && c.body)?.body), '{"action":"cancel","id":42}');
});
