'use strict';
// Playback engine: replays the step lists produced by the C++ algorithms.
// Supports pause, single-step, skip, a live queue/stack strip and an algorithm trace.
const E = { paused: false, once: false, skip: false, running: false, i: 0, n: 0, queue: [], stack: [], ds: null, trace: [], prefix: '' };

const dsFor = (label) => (label.startsWith('BFS') ? 'queue' : label.startsWith('DFS') ? 'stack' : null);
const delayFor = (s) => {
  const d = +$('#speed').value;
  return s.type === 'fail' ? d * 0.25 : s.type === 'dequeue' || s.type === 'backtrack' ? d * 0.5 : d;
};

function tr(msg) {
  E.trace.push(msg);
  if (E.trace.length > 400) E.trace.shift();
  $('#now').textContent = msg;
  if (G.tab === 'log' && typeof renderTrace === 'function') renderTrace();
}

function updatePlayLabel() { $('#btn-play').textContent = E.paused ? 'Resume' : 'Pause'; }

function renderDs() {
  const el = $('#ds-strip');
  if (!E.ds) { el.innerHTML = ''; return; }
  const items = E.ds === 'queue' ? E.queue : E.stack;
  const cap = 14, cut = items.length > cap;
  const list = E.ds === 'queue' ? items.slice(0, cap) : items.slice(-cap);
  const headIdx = E.ds === 'queue' ? 0 : list.length - 1;
  const name = E.ds === 'queue' ? 'Queue, front to back' : 'Stack, bottom to top';
  el.innerHTML = `<span class="ds-name">${name}:</span>` +
    (items.length ? list.map((v, k) => `<span class="cell${k === headIdx ? ' head' : ''}">${esc(nodeName(v))}</span>`).join('') : '<span class="ds-name">empty</span>') +
    (cut ? `<span class="ds-name">+${items.length - cap} more</span>` : '');
}

function beginShown(init) {
  const n = G.S.nodes.length;
  G.shown = G.S.nodes.map((m, i) => (init ? (init.status[i] || 'healthy') : m.status));
  G.shownBlocked = new Set(init ? init.blocked : G.S.edges.filter((e) => e.blocked).map((e) => keyOf(e.u, e.v)));
  G.visited = new Set(); G.dist = {}; G.current = -1;
  if (G.shown.length !== n) G.shown.length = n;
}
function endShown() {
  G.shown = null; G.shownBlocked = null; G.current = -1;
  E.queue = []; E.stack = []; E.ds = null; E.running = false; E.prefix = '';
  $('#prog').textContent = ''; renderDs();
}

function applyStep(s) {
  const speed = +$('#speed').value;
  switch (s.type) {
    case 'infect':
      G.shown[s.b] = 'infected';
      if (s.a >= 0) addFlash(s.a, s.b, P.infected, Math.max(350, speed * 3), true);
      if (E.ds === 'queue') E.queue.push(s.b);
      if (E.ds === 'stack') { E.stack.push(s.b); G.current = s.b; }
      tr(s.a < 0 ? `infect ${nodeName(s.b)} (entry point)` : `${nodeName(s.a)} infects ${nodeName(s.b)} at depth ${s.t}`);
      break;
    case 'dequeue':
      E.queue.shift(); G.current = s.a;
      tr(`dequeue ${nodeName(s.a)}, try its neighbours`);
      break;
    case 'fail':
      addFlash(s.a, s.b, P.isolated, Math.max(250, speed * 2), false);
      tr(`${nodeName(s.a)} fails to cross to ${nodeName(s.b)}`);
      break;
    case 'backtrack':
      E.stack.pop(); G.current = E.stack.length ? E.stack[E.stack.length - 1] : -1;
      tr(`backtrack from ${nodeName(s.a)}`);
      break;
    case 'visit':
      G.visited.add(s.a); G.dist[s.a] = s.t; G.current = s.a;
      tr(`settle ${nodeName(s.a)} at distance ${s.t}`);
      break;
    case 'relax':
      addFlash(s.a, s.b, P.amber, Math.max(300, speed * 3), false); G.dist[s.b] = s.t;
      tr(`relax ${nodeName(s.a)} to ${nodeName(s.b)}: tentative ${s.t}`);
      break;
    case 'set':
      G.shown[s.a] = STATUS[s.b];
      tr(`${nodeName(s.a)} is now ${STATUS[s.b]}`);
      break;
    case 'cut':
      G.shownBlocked.add(keyOf(s.a, s.b)); tr(`link ${nodeName(s.a)} - ${nodeName(s.b)} cut`);
      break;
    case 'uncut':
      G.shownBlocked.delete(keyOf(s.a, s.b)); tr(`link ${nodeName(s.a)} - ${nodeName(s.b)} restored`);
      break;
    case 'stuck':
      tr('no route to the target: the attacker is stuck');
      break;
    default: break;
  }
  renderDs();
}

async function runSteps(steps, ds, keepSkip) {
  E.ds = ds || null; E.queue = []; E.stack = []; E.n = steps.length; E.i = 0; E.running = true;
  if (!keepSkip) E.skip = false;
  renderDs();
  while (E.i < steps.length) {
    while (E.paused && !E.skip && !E.once) await sleep(40);
    E.once = false;
    applyStep(steps[E.i++]);
    $('#prog').textContent = `${E.prefix}step ${E.i}/${E.n}`;
    if (!E.skip) await sleep(delayFor(steps[E.i - 1]));
  }
}

// Replays every recorded action of the session from the starting snapshot.
async function runSession(entries, base) {
  beginShown(base); E.skip = false;
  for (let k = 0; k < entries.length; k++) {
    const en = entries[k];
    setBanner(`Replay ${k + 1}/${entries.length}: ${en.label}`);
    E.prefix = `action ${k + 1}/${entries.length}, `;
    await runSteps(en.steps, dsFor(en.label), true);
    if (!E.skip) await sleep(+$('#speed').value * 3);
  }
  endShown();
}

$('#btn-play').onclick = () => { E.paused = !E.paused; E.once = false; updatePlayLabel(); };
$('#btn-step').onclick = () => { E.paused = true; E.once = true; updatePlayLabel(); };
$('#btn-skip').onclick = () => { E.skip = true; E.paused = false; updatePlayLabel(); };
