'use strict';
// Application flow: selection, actions against the C++ API, scenarios, tabs, shortcuts.

/* ---------- selection ---------- */
function selectPrimary(i) { G.selected = i; if (G.target === i) G.target = -1; afterSelect(); setBanner(`${nodeName(i)} selected.`); }
function selectSecond(i) {
  if (i === G.selected) { toast('Pick a different machine as the second one.'); return; }
  G.target = G.target === i ? -1 : i; afterSelect();
  if (G.target >= 0) setBanner(`Second machine: ${nodeName(i)}.`);
}
function clearSelection() { G.selected = -1; G.target = -1; afterSelect(); }
function afterSelect() { renderChips(); renderInspector(); syncButtons(); }

/* ---------- state and rendering ---------- */
function captureBase() {
  G.base = { status: G.S.nodes.map((n) => n.status), blocked: G.S.edges.filter((e) => e.blocked).map((e) => keyOf(e.u, e.v)) };
}
function clearOverlays() {
  G.route = []; G.marked = new Set(); G.visited = new Set(); G.dist = {}; G.mstShow = 'none';
}
function setState(S, o = {}) {
  G.S = S;
  if (o.layout) {
    clearOverlays(); G.selected = -1; G.target = -1; G.lastAttack = null; E.trace = [];
    layoutForce(); fitView();
  }
  if (o.session) captureBase();
  renderAll();
}
function placeNewNodes(prevN) {
  const cx = (W / 2 - G.view.tx) / G.view.scale, cy = (H / 2 - G.view.ty) / G.view.scale;
  for (let i = prevN; i < G.S.nodes.length; i++)
    G.pos[i] = { x: cx + (Math.random() - 0.5) * 220, y: cy + (Math.random() - 0.5) * 160 };
}
function renderAll() {
  renderMetrics(); renderIncidents(); renderInspector(); renderLog(); renderLegend(); renderChips(); syncButtons();
  if (G.tab === 'analytics') renderAnalytics();
  if (G.tab === 'log') renderTrace();
}
function setBusy(b) { G.busy = b; syncButtons(); }
function needsSource(mode) { return mode !== 'hub' && mode !== 'random'; }
function syncButtons() {
  const has1 = G.selected >= 0, has2 = has1 && G.target >= 0, mode = $('#atk-mode').value;
  $$('.needs-idle').forEach((b) => (b.disabled = G.busy));
  $$('.needs-two').forEach((b) => (b.disabled = G.busy || !has2));
  $('#btn-attack').disabled = G.busy || (needsSource(mode) && !has1) || (mode === 'apt' && !has2);
  $('#btn-isolate').disabled = G.busy || !has1;
  $('#btn-replay-last').disabled = G.busy || !G.lastAttack;
  $('#btn-advise').disabled = G.busy || !has1;
}

/* ---------- talking to the C++ server ---------- */
async function act(path, params = {}, opts = {}) {
  if (G.busy) return null;
  setBusy(true);
  clearOverlays();
  let r;
  try { r = await api(path, params); }
  catch (e) { setBanner('Could not reach the C++ server. Is ./netsim still running?'); toast('Server not reachable', 'err'); setBusy(false); return null; }
  const x = r.extra || {};
  if (r.steps && r.steps.length) { beginShown(); await runSteps(r.steps, opts.ds || null); }
  const prevN = G.S.nodes.length;
  G.S = r.state;
  if (G.S.nodes.length > prevN) placeNewNodes(prevN);
  endShown();
  if (x.path) G.route = x.path;
  if (x.mst) { G.mst[x.algo] = new Set(x.mst.map((e) => keyOf(e.u, e.v))); G.mstShow = x.algo; }
  if (x.newNode !== undefined) { G.selected = x.newNode; G.target = -1; }
  if (x.message) { setBanner(x.message); if (x.error) toast(x.message, 'err'); }
  if (opts.session) captureBase();
  setBusy(false);
  renderAll();
  return r;
}

async function doGenerate(topo, n, seed) {
  if (G.busy) return;
  const S = await api('generate', { topology: topo, n, seed });
  $('#topology').value = S.topology; $('#count').value = S.nodes.length; $('#seed').value = S.seed;
  setState(S, { layout: true, session: true });
  setBanner(S.log[S.log.length - 1] + '. Click a machine to choose the entry point.');
}

async function launchAttack(over = {}) {
  const mode = over.mode || $('#atk-mode').value;
  if (over.mode) { $('#atk-mode').value = over.mode; onModeChange(); }
  if (over.aggr !== undefined) { $('#aggr').value = over.aggr; $('#aggr-out').textContent = (+over.aggr).toFixed(1); }
  if (over.count !== undefined) { $('#atk-count').value = over.count; $('#count-out').textContent = over.count; }
  const before = G.S.nodes.map((n) => n.status), blocked = G.S.edges.filter((e) => e.blocked).map((e) => keyOf(e.u, e.v));
  const r = await act('attack', { source: G.selected, mode, aggr: $('#aggr').value, target: G.target, count: $('#atk-count').value },
                      { ds: mode === 'bfs' ? 'queue' : mode === 'dfs' ? 'stack' : null });
  if (r && r.steps.length) { G.lastAttack = { before, blocked, steps: r.steps, mode }; syncButtons(); }
  if (G.tab === 'analytics') renderAnalytics();
}

async function replayLast() {
  const a = G.lastAttack;
  if (!a || G.busy) return;
  setBusy(true); clearOverlays();
  beginShown({ status: a.before, blocked: a.blocked });
  await runSteps(a.steps, a.mode === 'bfs' ? 'queue' : a.mode === 'dfs' ? 'stack' : null);
  endShown(); setBusy(false); renderAll();
}
async function replaySession() {
  if (G.busy) return;
  const t = await api('timeline', {}, 'GET');
  if (!t.entries.length) { toast('Nothing to replay yet: run an attack or a defence action first.'); return; }
  setBusy(true); clearOverlays();
  await runSession(t.entries, G.base);
  setBusy(false); renderAll(); setBanner('Session replay finished.');
}

function setColorBy(v) { G.colorBy = v; localStorage.setItem('colorBy', v); renderLegend(); }
function showTab(name) {
  G.tab = name;
  $$('.tab').forEach((t) => t.setAttribute('aria-selected', String(t.dataset.tab === name)));
  $$('.panel').forEach((p) => (p.hidden = p.id !== 'tab-' + name));
  if (name === 'analytics') renderAnalytics();
  if (name === 'log') renderTrace();
}
function onModeChange() {
  const m = $('#atk-mode').value;
  const hints = {
    bfs: 'Select the entry machine. The worm crosses links in waves, using a queue.',
    dfs: 'Select the entry machine. The attacker digs deep with a stack and backtracks when stuck.',
    apt: 'Select the entry machine, then Shift+click the target. The attacker follows the easiest Dijkstra route and retries each hop.',
    hub: 'No entry point needed. Takes out the best-connected machines, found with a max-heap.',
    random: 'No entry point needed. Takes out machines picked at random, for comparison with hub failure.',
  };
  $('#atk-hint').textContent = hints[m];
  $('#row-count').hidden = !(m === 'hub' || m === 'random');
  syncButtons();
}

/* ---------- scenarios ---------- */
const pickBy = (filter, score) => {
  let best = -1, bs = -1e9;
  G.S.nodes.forEach((n, i) => { if (filter(n) && score(n) > bs) { bs = score(n); best = i; } });
  return best;
};
const SCENARIOS = [
  { text: 'A worm enters through a busy workstation and spreads in BFS waves.',
    run: async () => { await doGenerate('random', 26, 7); G.selected = pickBy((n) => n.type === 'PC', (n) => n.deg); afterSelect(); await launchAttack({ mode: 'bfs', aggr: 1.1 }); } },
  { text: 'An insider moves laterally across a grid, going deep before backtracking (DFS).',
    run: async () => { await doGenerate('mesh', 25, 3); G.selected = 0; afterSelect(); await launchAttack({ mode: 'dfs', aggr: 1.0 }); } },
  { text: 'An APT follows the easiest route from a remote PC to the most valuable server.',
    run: async () => {
      await doGenerate('random', 28, 11);
      G.selected = pickBy((n) => n.type === 'PC', (n) => -n.deg);
      G.target = pickBy((n) => n.type === 'SERVER', (n) => n.imp * 100 + n.id); afterSelect();
      await launchAttack({ mode: 'apt', aggr: 1.3 });
    } },
  { text: 'The two best-connected machines of a tree fail, splitting the network.',
    run: async () => { await doGenerate('tree', 31, 5); clearSelection(); await launchAttack({ mode: 'hub', count: 2 }); } },
  { text: 'Defence demo: ask the advisor, isolate its best pick, attack, then respond to incidents.',
    run: async () => {
      await doGenerate('random', 24, 21);
      G.selected = pickBy((n) => n.type === 'PC', (n) => n.deg); afterSelect();
      setBanner('Asking the defence advisor which machine to isolate first...');
      const adv = await api('advise', { source: G.selected, aggr: 0.9 }, 'GET');
      if (adv.ok && adv.best.length) {
        G.marked = new Set(adv.best.map((b) => b.node)); await sleep(1400);
        await act('isolate', { node: adv.best[0].node }); await sleep(500);
      }
      await launchAttack({ mode: 'bfs', aggr: 0.9 }); await sleep(600);
      await act('respond'); await act('respond');
    } },
];

/* ---------- wiring ---------- */
function bind() {
  $('#btn-generate').onclick = () => doGenerate($('#topology').value, $('#count').value, $('#seed').value);
  $('#btn-dice').onclick = () => { $('#seed').value = Math.floor(Math.random() * 9000) + 1; };
  $('#btn-layout').onclick = () => { layoutForce(); fitView(); };
  $('#btn-addnode').onclick = () => act('addnode', { type: $('#new-type').value });
  $('#btn-addlink').onclick = () => act('addlink', { u: G.selected, v: G.target, cost: $('#link-cost').value, risk: $('#link-risk').value });
  $('#btn-cut').onclick = () => act('cut', { u: G.selected, v: G.target });
  $('#btn-restore').onclick = () => act('restore', { u: G.selected, v: G.target });
  $('#btn-attack').onclick = () => launchAttack();
  $('#btn-isolate').onclick = () => act('isolate', { node: G.selected });
  $('#btn-respond').onclick = () => act('respond');
  $('#btn-undo').onclick = () => act('undo');
  $('#btn-reset').onclick = () => act('reset', {}, { session: true });
  $('#btn-backbone').onclick = () => act('backbone', { algo: $('#mst-algo').value });
  $('#btn-route').onclick = () => act('path', { from: G.selected, to: G.target, mode: $('#route-mode').value });
  $('#btn-advise').onclick = () => runAdvisor();
  $('#btn-replay-last').onclick = replayLast;
  $('#btn-replay-all').onclick = replaySession;
  $('#btn-robust').onclick = () => runRobust();
  $('#btn-monte').onclick = () => runMonte();
  $('#btn-mstcmp').onclick = () => runMstCompare();
  $('#btn-bench').onclick = () => runBenchmark();
  $('#btn-trace-clear').onclick = () => { E.trace = []; renderTrace(); };
  $('#btn-scenario').onclick = async () => {
    if (G.busy) return;
    const s = SCENARIOS[+$('#scenario').value];
    setBanner(s.text);
    try { await s.run(); } catch (e) { toast('Scenario failed: ' + e.message, 'err'); }
  };
  $('#scenario').onchange = () => setBanner(SCENARIOS[+$('#scenario').value].text);
  $('#atk-mode').onchange = onModeChange;
  $('#aggr').oninput = (e) => ($('#aggr-out').textContent = (+e.target.value).toFixed(1));
  $('#atk-count').oninput = (e) => ($('#count-out').textContent = e.target.value);
  $('#speed').oninput = (e) => ($('#speed-out').textContent = e.target.value + ' ms');
  $('#colorby').onchange = (e) => setColorBy(e.target.value);
  $('#opt-names').onchange = (e) => (G.showNames = e.target.checked);
  $('#opt-costs').onchange = (e) => (G.showCosts = e.target.checked);
  $('#opt-size').onchange = (e) => (G.sizeByImp = e.target.checked);
  $('#z-in').onclick = () => zoomAt(W / 2, H / 2, 1.25);
  $('#z-out').onclick = () => zoomAt(W / 2, H / 2, 0.8);
  $('#z-fit').onclick = fitView;
  $('#btn-png').onclick = exportPng;
  $('#btn-help').onclick = () => $('#help').showModal();
  $('#help-close').onclick = () => $('#help').close();
  $('#btn-theme').onclick = () => setTheme(document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark');
  $$('.tab').forEach((t) => (t.onclick = () => showTab(t.dataset.tab)));

  $('#btn-save').onclick = () => { const a = document.createElement('a'); a.href = '/api/export'; a.download = 'network.netsim'; a.click(); };
  $('#file-load').onchange = async (e) => {
    const f = e.target.files[0]; e.target.value = '';
    if (!f || G.busy) return;
    const r = await api('import', {}, 'POST', await f.text());
    if (r.extra && r.extra.error) { toast(r.extra.message, 'err'); return; }
    setState(r.state, { layout: true, session: true });
    toast('Network loaded: ' + r.state.nodes.length + ' machines');
  };

  $('#btn-find').onclick = async () => {
    const name = $('#q-name').value.trim().toUpperCase();
    const r = await api('machine', { name }, 'GET');
    if (r.found) { selectPrimary(r.id); G.marked = new Set([r.id]); $('#q-result').textContent = `Found ${name} through the hash table.`; }
    else $('#q-result').textContent = `No machine named "${name}". Names look like PC-3, SRV-4, RTR-7.`;
  };
  $('#btn-risky').onclick = async () => {
    const r = await api('risky', { min: $('#q-min').value, max: $('#q-max').value }, 'GET');
    G.marked = new Set(r.ids);
    $('#q-result').textContent = r.ids.length ? `${r.ids.length} machine(s) in range, highlighted on the map (AVL height ${r.height}).` : 'No machines in that risk range.';
  };

  // event delegation for dynamic content
  $('#incidents').onclick = (ev) => { const li = ev.target.closest('li[data-id]'); if (li) selectPrimary(+li.dataset.id); };
  $('#inspector').onclick = (ev) => { const b = ev.target.closest('button[data-act]'); if (b) act(b.dataset.act, { u: b.dataset.u, v: b.dataset.v }); };
  $('#critical').onclick = (ev) => { const li = ev.target.closest('li[data-node]'); if (li) selectPrimary(+li.dataset.node); };
  $('#advisor').onclick = (ev) => {
    const b = ev.target.closest('button[data-isolate]');
    if (b) { act('isolate', { node: b.dataset.isolate }); return; }
    const li = ev.target.closest('li[data-node]'); if (li) selectPrimary(+li.dataset.node);
  };
  $('#mst-compare').onclick = (ev) => { const b = ev.target.closest('button[data-show]'); if (b) G.mstShow = b.dataset.show; };

  document.addEventListener('keydown', (ev) => {
    if (/INPUT|SELECT|TEXTAREA/.test(ev.target.tagName) || ev.ctrlKey || ev.metaKey || ev.altKey || $('#help').open) return;
    const k = ev.key.toLowerCase();
    if (k === ' ') { ev.preventDefault(); $('#btn-play').click(); }
    else if (k === 'n') $('#btn-step').click();
    else if (k === 's') $('#btn-skip').click();
    else if (k === 'a' && !$('#btn-attack').disabled) launchAttack();
    else if (k === 'i' && !$('#btn-isolate').disabled) $('#btn-isolate').click();
    else if (k === 'r') $('#btn-respond').click();
    else if (k === 'u') $('#btn-undo').click();
    else if (k === 'f') fitView();
    else if (k === 'escape') clearSelection();
    else if (k === '?') $('#help').showModal();
  });
  window.addEventListener('resize', () => { if (G.S) renderAnalytics(); });
}

function setTheme(t) {
  document.documentElement.dataset.theme = t;
  localStorage.setItem('theme', t);
  refreshPalette(); renderLegend();
  if (G.S) { renderAnalytics(); }
}

(async function init() {
  const saved = localStorage.getItem('theme');
  document.documentElement.dataset.theme = saved || (matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light');
  refreshPalette();
  G.colorBy = localStorage.getItem('colorBy') || 'status'; $('#colorby').value = G.colorBy;
  bind(); onModeChange(); showTab('overview'); renderTrace();
  resize();
  try {
    const S = await api('state', {}, 'GET');
    $('#topology').value = S.topology; $('#count').value = S.nodes.length; $('#seed').value = S.seed;
    setState(S, { layout: true, session: true });
    setBanner('Click a machine on the map to select it, then launch an attack. Try a scenario from the top bar.');
  } catch (e) { setBanner('Could not reach the C++ server. Start ./netsim and reload.'); }
  requestAnimationFrame(draw);
})();
