// Throwaway, in-memory fixtures. No native popup, engine, storage or provider access.
const $ = (id) => document.getElementById(id);
const fixtures = {
  word: { text: 'Let’s make something beau', words: ['beautiful', 'beauty', 'beautifully', 'beautify'], prefix: 'beau' },
  phrase: { text: 'Let’s make something beau', words: ['beautiful', 'beauty', 'beautifully', 'beautify'], prefix: 'beau', phrase: 'tiful, and make it feel effortless.' },
  correction: { text: 'It feels seper', words: ['separate', 'separation', 'separately'], prefix: 'seper', marks: [3], correction: true },
  next: { text: 'The smallest details make ', words: ['a difference', 'the experience', 'it feel natural', 'everything better'], prefix: '', unselected: true },
  long: { text: 'I think we should ', words: ['start', 'make', 'take', 'try'], prefix: '', phrase: 'start with the small details that make writing feel natural, then bring that same care to every part of the experience.' },
  empty: { text: 'A word we haven’t learned: zqx', words: [], prefix: 'zqx' },
  paused: { text: 'Just me and the page.', words: [], prefix: '', paused: true },
};
const variant = 'glass'; // Old variant URLs resolve to the selected liquid-glass direction.
let fixture, words = [], prefix = '', phrase = '', selected = 0, moved = false;
let visible = false, streaming = false, pending = false, armedAt = 0, timers = [], dismissed = false, dismissedWord = '';
let lastCaret = null;
let phase = 'idle', deadline = 0, startedAt = 0, ticker = null;
let minimized = false, autoMinimized = false;
const escape = (text) => text.replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const clearTimers = () => { timers.forEach(clearTimeout); timers = []; clearInterval(ticker); ticker = null; streaming = false; pending = false; };

function caretToEnd() {
  $('editor').focus({ preventScroll: true });
  const range = document.createRange();
  range.selectNodeContents($('editor')); range.collapse(false);
  const selection = getSelection(); selection.removeAllRanges(); selection.addRange(range);
  rememberCaret();
}
function rememberCaret() {
  const selection = getSelection();
  if (!selection.rangeCount || !$('editor').contains(selection.anchorNode)) return;
  const range = selection.getRangeAt(0).cloneRange(); range.collapse(true);
  let rect = range.getBoundingClientRect();
  if (!rect.height && range.startContainer.nodeType === Node.TEXT_NODE && range.startOffset > 0) {
    range.setStart(range.startContainer, range.startOffset - 1);
    const previous = range.getBoundingClientRect();
    rect = { left: previous.right, top: previous.top, bottom: previous.bottom, height: previous.height };
  }
  if (rect.height) lastCaret = rect;
}
function positionPopup() {
  const popup = $('popup');
  if (!visible) return;
  const stage = $('stage').getBoundingClientRect();
  const caret = lastCaret || $('editor').getBoundingClientRect();
  const scale = Number($('scale').value);
  // Scale demo is deliberately constrained to this simulated window, not a real monitor.
  const desiredWidth = 330;
  const usableScale = Math.min(scale, (stage.width - 20) / 190);
  popup.style.width = `${Math.min(desiredWidth, (stage.width - 20) / usableScale)}px`;
  popup.style.transform = `scale(${usableScale})`;
  const width = popup.offsetWidth * usableScale, height = popup.offsetHeight * usableScale;
  let left = Math.min(Math.max(10, caret.left - stage.left), stage.width - width - 10);
  let top = caret.bottom - stage.top + 6;
  const above = top + height > stage.height - 10;
  if (above) top = caret.top - stage.top - height - 6;
  popup.style.left = `${Math.max(10, left)}px`;
  popup.style.top = `${Math.max(10, top)}px`;
  popup.dataset.position = above ? 'above' : 'below';
  adaptInformation();
}
function completionMarkup(word) {
  if (fixture.correction) return [...word].map((c, i) => `<span class="${fixture.marks.includes(i) ? 'correction' : i < prefix.length ? 'typed' : ''}">${escape(c)}</span>`).join('');
  return `<span class="typed">${escape(word.slice(0, prefix.length))}</span>${escape(word.slice(prefix.length))}`;
}
function row(word, index) {
  const active = selected === index;
  const origin = index === 1 && !fixture.correction ? 'Learned' : 'Local';
  return `<div id="candidate-${index}" role="option" aria-selected="${active}" class="candidate ${active ? 'selected' : ''}"><span class="completion">${completionMarkup(word)}</span><span class="origin">${origin}</span>${active ? '<kbd>Tab</kbd>' : ''}</div>`;
}
function phraseRow() {
  if (!phrase && (pending || fixture.phrase && !fixture.paused && !dismissed)) return '<div class="candidate phrase pending" aria-hidden="true"><div class="phrase-label"><span class="origin">AI</span><span class="ai-status"></span><span class="ai-light" aria-hidden="true"></span></div><span class="completion ai-wait"></span><div class="idle-track" aria-hidden="true"><div class="idle-progress"></div></div></div>';
  if (!phrase) return '';
  const active = selected === -1;
  const next = phrase.match(/^\s*\S+\s*/)?.[0] || phrase;
  const preview = `<span class="typed">${escape(prefix)}</span><span class="accept-next">${escape(next)}</span>${escape(phrase.slice(next.length))}`;
  return `<div id="candidate-phrase" role="option" aria-selected="${active}" class="candidate phrase ${active ? 'selected' : ''}"><div class="phrase-label"><span class="origin">AI</span><span class="ai-status"></span><span class="ai-light" aria-hidden="true"></span>${active ? '<kbd>Tab</kbd>' : ''}</div><span class="completion ${streaming ? 'streaming' : ''}">${preview}</span></div>`;
}
function render() {
  visible = !dismissed && (words.length > 0 || phrase.length > 0) && !fixture.paused;
  const popup = $('popup'); popup.hidden = !visible;
  popup.className = 'popup glass';
  const footer = `<div class="menu-footer"><span>↑ ↓ choose</span><span>${phrase ? '<kbd>Ctrl</kbd> + <kbd>→</kbd> next word' : 'Esc dismiss'}</span></div>`;
  const comparison = fixture.correction ? `<div class="correction-comparison"><span>${escape(prefix)}</span><span aria-hidden="true">→</span><strong>${escape(words[Math.max(0, selected)] || words[0] || '')}</strong><small>Correction</small></div>` : '';
  popup.innerHTML = `${phraseRow()}<div class="words">${words.map(row).join('')}</div>${comparison}${footer}`;
  if (visible && selected !== -2) $('editor').setAttribute('aria-activedescendant', selected === -1 ? 'candidate-phrase' : `candidate-${selected}`);
  else $('editor').removeAttribute('aria-activedescendant');
  $('editor').setAttribute('aria-expanded', String(visible));
  positionPopup();
  const selectionText = selected === -2 ? 'none · Tab passes through' : selected === -1 ? 'phrase · Tab accepts' : `word ${selected + 1} · Tab accepts`;
  $('state').textContent = `${variant} · ${$('scenario').selectedOptions[0].text} · ${$('scale').selectedOptions[0].text.split(' · ')[0]}\n${visible ? `${words.length} words · ${streaming ? 'streaming' : phrase ? 'phrase ready' : 'no phrase'} · ${popup.dataset.position}\nSelection: ${selectionText}` : fixture.paused ? 'Paused · menu hidden · keys pass through' : 'Menu hidden · keys pass through'}`;
  updateInfo();
}
function loadScenario() {
  clearTimers(); fixture = { ...fixtures[$('scenario').value] };
  $('editor').textContent = fixture.text;
  words = [...fixture.words]; prefix = fixture.prefix; phrase = ''; selected = fixture.unselected ? -2 : words.length ? 0 : -2;
  moved = false; dismissed = false; dismissedWord = ''; armedAt = 0; lastCaret = null;
  caretToEnd(); render();
  scheduleAI();
}
function scheduleAI(force = false) {
  clearTimers();
  if (fixture.paused) phase = 'paused';
  else if ($('connection').value === 'offline') phase = 'offline';
  else if (!fixture.phrase) phase = 'local';
  else if ($('trigger').value === 'manual' && !force) phase = 'manual';
  else {
    phase = 'countdown'; pending = true;
    deadline = performance.now() + (force ? 0 : Number($('delay').value));
    ticker = setInterval(updateInfo, 50);
    timers.push(setTimeout(() => {
      phase = 'thinking'; startedAt = performance.now(); updateInfo();
      timers.push(setTimeout(startStream, 950));
    }, Math.max(0, deadline - performance.now())));
  }
  render();
}
function startStream() {
    const target = fixture.phrase;
    let count = 0;
    const stream = () => {
      if (dismissed) return;
      if (!count) {
        armedAt = performance.now() + 150;
        // The native model highlights a phrase-only popup immediately.
        if (!words.length) selected = -1;
        timers.push(setTimeout(() => { if (!moved && words.length) selected = -1; render(); }, 150));
      }
      phase = 'streaming'; pending = false; count = Math.min(count + 5, target.length); phrase = target.slice(0, count);
      streaming = count < target.length; render();
      if (streaming) timers.push(setTimeout(stream, 55));
      else { phase = 'ready'; clearInterval(ticker); ticker = null; updateInfo(); }
    };
    stream();
}
function updateInfo() {
  const remaining = Math.max(0, deadline - performance.now()) / 1000;
  const elapsed = ((performance.now() - startedAt) / 1000).toFixed(1);
  const titles = { countdown: 'Idle', thinking: 'Working', streaming: 'Receiving', ready: 'Ready', manual: 'Manual', offline: 'Offline' };
  document.querySelectorAll('.ai-status').forEach(el => { el.textContent = titles[phase] || ''; });
  document.querySelectorAll('.ai-wait').forEach(el => { el.textContent = phase === 'countdown' ? `${remaining.toFixed(1)}s until AI` : phase === 'thinking' ? `${elapsed}s` : phase === 'manual' ? 'Ctrl + Space' : phase === 'offline' ? 'Local words available' : ''; });
  $('tense-value').textContent = $('tense').value;
  const connection = $('connection-light');
  const online = $('connection').value === 'online';
  connection.classList.toggle('offline', !online);
  connection.setAttribute('aria-label', online ? 'Connected' : 'Offline');
  connection.title = online ? 'Connected' : 'Offline';
  const progress = phase === 'countdown' ? 1 - Math.max(0, deadline - performance.now()) / Number($('delay').value) : ['thinking','streaming','ready'].includes(phase) ? 1 : 0;
  document.querySelectorAll('.idle-progress').forEach(el => { el.style.transform = `scaleX(${Math.max(0, Math.min(1, progress))})`; });
  document.querySelectorAll('.ai-light').forEach(el => { el.classList.toggle('working', phase === 'thinking' || phase === 'streaming'); });
  $('info-panel').dataset.phase = phase;
}
function setMinimized(value, automatic = false) {
  minimized = value;
  $('info-panel').classList.toggle('minimized', value);
  $('info-panel').dataset.automatic = String(automatic);
  $('info-body').inert = value;
  $('minimize').setAttribute('aria-expanded', String(!value));
  $('minimize').setAttribute('aria-label', value ? 'Expand information panel' : 'Minimize information panel');
  $('minimize').textContent = value ? '⌃' : '⌄';
}
$('minimize').addEventListener('mousedown', e => e.preventDefault());
$('minimize').addEventListener('click', () => { autoMinimized = false; setMinimized(!minimized); });
function adaptInformation() {
  const dock = $('info-dock') || $('info-panel').querySelector('.info-dock');
  const dockRect = dock.getBoundingClientRect();
  const panelRect = { left: dockRect.left, right: dockRect.right, bottom: dockRect.top - 10, top: dockRect.top - 10 - $('info-body').offsetHeight };
  const menu = visible ? $('popup').getBoundingClientRect() : null;
  const intersects = menu && menu.left < panelRect.right && menu.right > panelRect.left && menu.top < panelRect.bottom && menu.bottom > panelRect.top;
  const constrained = window.innerHeight < 320 || intersects;
  if (constrained && !minimized) { autoMinimized = true; setMinimized(true, true); }
  else if (!constrained && autoMinimized) { autoMinimized = false; setMinimized(false, true); }
}
$('tense').addEventListener('change', updateInfo);
for (const id of ['trigger', 'delay', 'connection']) $(id).addEventListener('change', () => {
  phrase = ''; moved = false; selected = fixture.unselected ? -2 : words.length ? 0 : -2; dismissed = false; scheduleAI();
});
window.addEventListener('resize', adaptInformation);
adaptInformation();
function insertCompletion(partial = false) {
  const selection = getSelection();
  if (!selection.rangeCount || !$('editor').contains(selection.anchorNode)) return;
  const range = selection.getRangeAt(0);
  let insertion;
  if (partial) {
    insertion = phrase.match(/^\s*\S+\s*/)?.[0] || phrase;
  } else if (selected === -1) insertion = phrase;
  else {
    const word = words[selected];
    if (fixture.correction) {
      const before = range.cloneRange(); before.selectNodeContents($('editor')); before.setEnd(range.startContainer, range.startOffset);
      let offset = Math.max(0, before.toString().length - prefix.length);
      const walker = document.createTreeWalker($('editor'), NodeFilter.SHOW_TEXT);
      let node;
      while ((node = walker.nextNode())) {
        if (offset <= node.length) { range.setStart(node, offset); break; }
        offset -= node.length;
      }
    }
    insertion = fixture.correction ? word : word.slice(prefix.length);
  }
  range.deleteContents();
  const text = document.createTextNode(insertion); range.insertNode(text); range.setStartAfter(text); range.collapse(true);
  selection.removeAllRanges(); selection.addRange(range); $('editor').normalize();
  rememberCaret(); clearTimers();
  phase = partial ? 'ready' : 'accepted';
  if (partial) { phrase = phrase.slice(insertion.length); prefix = ''; words = []; selected = -1; if (!phrase) dismissed = true; }
  else dismissed = true;
  render();
  $('state').textContent += partial ? '\nAccepted next phrase word' : '\nAccepted suggestion';
}
$('editor').addEventListener('keydown', (event) => {
  if (event.isComposing) return;
  if (event.ctrlKey && !event.altKey && !event.shiftKey && event.key === ' ') { event.preventDefault(); dismissed = false; scheduleAI(true); return; }
  if (event.key === 'Enter') return;
  if (event.ctrlKey && !event.altKey && !event.shiftKey && event.key === 'ArrowRight' && visible && phrase) { event.preventDefault(); insertCompletion(true); return; }
  if (event.ctrlKey || event.altKey || event.metaKey || event.shiftKey || !visible) return;
  if (event.key === 'Escape') { event.preventDefault(); dismissed = true; dismissedWord = $('editor').textContent.trim().split(/\s/).pop(); clearTimers(); phase = 'dismissed'; render(); }
  else if (event.key === 'Tab' && selected !== -2) { event.preventDefault(); insertCompletion(); }
  else if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
    event.preventDefault(); moved = true;
    const order = [...(phrase ? [-1] : []), ...words.map((_, i) => i)];
    const delta = event.key === 'ArrowDown' ? 1 : -1;
    const index = selected === -2 ? delta > 0 ? -1 : 0 : order.indexOf(selected);
    selected = order[(index + delta + order.length) % order.length]; render();
  }
});
$('editor').addEventListener('input', () => {
  clearTimers(); phrase = ''; moved = false;
  const text = $('editor').textContent;
  const fragment = text.match(/[\p{L}]+$/u)?.[0] || '';
  if (dismissedWord && fragment.startsWith(dismissedWord) && !/\s$/.test(text)) { dismissed = true; render(); return; }
  dismissedWord = ''; dismissed = false;
  const vocabulary = ['beautiful', 'beauty', 'beautifully', 'beautify', 'better', 'because', 'begin', 'bring', 'care', 'careful', 'complete', 'completion', 'consider', 'design', 'detail', 'details', 'effortless', 'experience', 'feel', 'feels', 'flow', 'make', 'natural', 'polished', 'prototype', 'start', 'something', 'thought', 'write', 'writing'];
  fixture = { text, prefix: fragment, words: [], paused: $('scenario').value === 'paused' };
  prefix = fragment;
  words = fragment ? vocabulary.filter(w => w.startsWith(fragment.toLowerCase()) && w.length > fragment.length).slice(0, 5).map(w => fragment + w.slice(fragment.length)) : /\s$/.test(text) ? [...fixtures.next.words] : [];
  selected = fragment && words.length ? 0 : -2;
  if ($('scenario').value === 'phrase' || $('scenario').value === 'long') {
    fixture.phrase = fragment ? words.length ? words[0].slice(fragment.length) + ', with care in every detail.' : null : 'make the next step feel effortless.';
  }
  rememberCaret(); render();
  scheduleAI();
});
document.addEventListener('selectionchange', () => { rememberCaret(); if (fixture) positionPopup(); });
$('scenario').addEventListener('change', loadScenario);
$('placement').addEventListener('change', () => { $('stage').dataset.placement = $('placement').value; caretToEnd(); render(); });
$('scale').addEventListener('change', () => {
  $('stage').style.height = `${480 * Number($('scale').value)}px`;
  rememberCaret(); render();
});
$('replay').addEventListener('click', loadScenario);
$('theme').addEventListener('click', () => {
  document.documentElement.setAttribute('data-theme-switching', '');
  const dark = document.documentElement.dataset.theme !== 'dark';
  document.documentElement.dataset.theme = dark ? 'dark' : 'light';
  $('theme').textContent = dark ? 'Light appearance ◑' : 'Dark appearance ◐';
  requestAnimationFrame(() => requestAnimationFrame(() => document.documentElement.removeAttribute('data-theme-switching')));
});
document.addEventListener('keydown', event => {
  document.documentElement.dataset.input = 'keyboard';
});
document.addEventListener('pointerdown', () => { document.documentElement.dataset.input = 'pointer'; });
window.addEventListener('resize', () => { lastCaret = null; rememberCaret(); positionPopup(); });
document.fonts.ready.then(() => { rememberCaret(); positionPopup(); });
$('scenario').value = 'phrase';
loadScenario();
