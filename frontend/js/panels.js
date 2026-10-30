'use strict';
// Right-hand panels: metrics, incidents, inspector, analytics, algorithm comparison, logs.
const fmtUs = (v) => (v >= 1000 ? +(v / 1000).toFixed(v >= 10000 ? 0 : 1) + ' ms' : (v >= 100 ? Math.round(v) : +v.toFixed(1)) + ' us');

function renderMetrics() {
  const m = G.S.metrics;
  const cells = [
    ['Machines affected', `${m.affected}<small> of ${m.total}</small>`, m.affected ? 'bad' : ''],
    ['Containment', `${m.saved}<small>%</small>`, 'good'],
    ['Infected now', m.infected, m.infected ? 'bad' : ''],
    ['Importance at risk', m.atRisk, m.atRisk ? 'bad' : ''],
    ['Isolated', m.isolated, ''],
    ['Recovered', m.recovered, ''],
    ['Segments', `${m.segments}<small> (largest ${m.largest})</small>`, m.segments > 1 ? 'warn' : ''],
    ['Cut links', `${m.cutLinks}<small> of ${m.links}</small>`, m.cutLinks ? 'warn' : ''],
    ['Recovery cost', m.recoveryCost, ''],
    ['Undo depth', m.undoDepth, ''],
  ];
  $('#metrics').innerHTML = cells.map(([k, v, c]) => `<div class="${c}"><dt>${k}</dt><dd>${v}</dd></div>`).join('');
}

function renderIncidents() {
  const inc = G.S.incidents, max = Math.max(1, ...inc.map((i) => i.sev));
  $('#incidents').innerHTML = inc.length
    ? inc.map((i) => `<li data-id="${i.node}"><span>${esc(nodeName(i.node))}</span><span class="bar" style="width:${(100 * i.sev) / max}%"></span><b>${i.sev}</b></li>`).join('')
    : '<li class="empty">No open incidents.</li>';
}

function renderInspector() {
  const el = $('#inspector'), i = G.selected;
  if (i < 0 || !G.S.nodes[i]) { el.innerHTML = '<p class="empty">Select a machine to see its details and links.</p>'; return; }
  const m = G.S.nodes[i];
  const links = G.S.edges.filter((e) => e.u === i || e.v === i).map((e) => ({ o: e.u === i ? e.v : e.u, e }));
  el.innerHTML =
    `<dl class="kv"><dt>Name</dt><dd><b>${esc(m.name)}</b> (${m.type.toLowerCase()})</dd>` +
    `<dt>Status</dt><dd><span class="pill" style="--c:${P[m.status]}">${m.status}</span></dd>` +
    `<dt>Importance</dt><dd>${m.imp} of 10</dd><dt>Risk</dt><dd>${m.risk} of 100</dd>` +
    `<dt>Links</dt><dd>${m.deg}</dd><dt>Segment</dt><dd>${m.seg < 0 ? 'none (isolated)' : m.seg + 1}</dd>` +
    `<dt>Critical</dt><dd>${m.cut ? 'Yes: losing it splits the network' : 'No'}</dd></dl>` +
    (links.length ? '<ul class="list">' + links.map(({ o, e }) =>
      `<li><span>${esc(nodeName(o))} <small style="color:var(--muted)">cost ${e.cost}, risk ${e.risk}${e.bridge ? ', bridge' : ''}</small></span>` +
      `<button class="small ghost needs-idle" data-u="${i}" data-v="${o}" data-act="${e.blocked ? 'restore' : 'cut'}" ${G.busy ? 'disabled' : ''}>${e.blocked ? 'Restore' : 'Cut'}</button></li>`).join('') + '</ul>'
      : '<p class="empty">No links yet. Shift+click another machine and press Add link.</p>');
}

function renderLog() {
  $('#log').innerHTML = G.S.log.slice().reverse().map((l) => `<li>${esc(l)}</li>`).join('');
}
function renderTrace() {
  const el = $('#trace');
  el.innerHTML = E.trace.slice(-150).map((l) => `<li>${esc(l)}</li>`).join('') || '<li>Run an attack or a route search to see each algorithm step here.</li>';
  el.scrollTop = el.scrollHeight;
}

/* ---------- analytics ---------- */
function renderStatusChart() {
  const m = G.S.metrics;
  drawDonut($('#ch-status'), {
    parts: [{ label: 'Healthy', value: m.healthy, color: P.healthy }, { label: 'Infected', value: m.infected, color: P.infected },
            { label: 'Isolated', value: m.isolated, color: P.isolated }, { label: 'Recovered', value: m.recovered, color: P.recovered }],
    center: m.saved + '%', sub: 'never infected',
  });
}
function renderDegreeChart() { drawHistogram($('#ch-degree'), { values: G.S.nodes.map((n) => n.deg), color: P.healthy }); }
function renderSpreadChart() {
  const a = G.lastAttack, note = $('#spread-note');
  if (!a) { emptyChart($('#ch-spread'), 90, 'Launch an attack to see how it spreads'); note.textContent = ''; return; }
  const counts = {};
  let total = 0, maxD = 0;
  a.steps.forEach((s) => { if (s.type === 'infect') { counts[s.t] = (counts[s.t] || 0) + 1; total++; maxD = Math.max(maxD, s.t); } });
  let cum = 0;
  const data = [];
  for (let d = 0; d <= maxD; d++) { cum += counts[d] || 0; data.push({ x: d, y: cum }); }
  const label = a.mode === 'bfs' ? 'Wave (depth)' : a.mode === 'dfs' ? 'Depth' : a.mode === 'apt' ? 'Hop' : 'Rank';
  drawLines($('#ch-spread'), { series: [{ name: 'Machines compromised', color: P.infected, data }], xTitle: label, h: 160, xFmt: (v) => Math.round(v), yFmt: (v) => Math.round(v) });
  note.textContent = `${total} machine(s) compromised, deepest at ${maxD}.`;
}
async function renderTimelineChart() {
  const t = await api('timeline', {}, 'GET');
  const e = t.entries;
  if (!e.length) { emptyChart($('#ch-timeline'), 90, 'Actions you take will be charted here'); return; }
  const line = (k) => e.map((en, i) => ({ x: i + 1, y: en.snap[k] }));
  drawLines($('#ch-timeline'), { h: 170, xTitle: 'action', xFmt: (v) => Math.round(v), yFmt: (v) => Math.round(v), series: [
    { name: 'Infected', color: P.infected, data: line('infected') }, { name: 'Isolated', color: P.isolated, data: line('isolated') },
    { name: 'Recovered', color: P.recovered, data: line('recovered') } ] });
}
async function renderCritical() {
  const c = await api('critical', {}, 'GET'), el = $('#critical');
  const cuts = c.cut.map((x) => `<li class="click" data-node="${x.node}"><span>${esc(nodeName(x.node))}</span><span>splits into ${x.segments} segment(s), largest ${x.largest}</span></li>`).join('');
  el.innerHTML = (cuts ? `<ul class="list">${cuts}</ul>` : '<p class="empty">No single machine splits the network.</p>') +
    `<p class="note"><b>${c.cut.length}</b> critical machine(s), <b>${c.bridges.length}</b> bridge link(s). ` +
    `<button class="small ghost" id="btn-show-critical">Highlight on map</button></p>`;
  $('#btn-show-critical').onclick = () => { $('#colorby').value = 'critical'; setColorBy('critical'); };
}
function renderAnalytics() {
  if (G.tab !== 'analytics' || !G.S) return;
  renderStatusChart(); renderDegreeChart(); renderSpreadChart();
  renderTimelineChart().catch(() => {}); renderCritical().catch(() => {});
}

async function runRobust() {
  const r = await api('robustness', {}, 'GET');
  const pt = (arr) => arr.map((v, i) => ({ x: (i / r.total) * 100, y: v * 100 }));
  drawLines($('#ch-robust'), { h: 190, yMax: 100, xMax: 100, xMin: 0, xTitle: '% machines removed', xFmt: (v) => Math.round(v) + '%', yFmt: (v) => Math.round(v) + '%', series: [
    { name: 'Targeted (best-connected first)', color: P.infected, data: pt(r.targeted) }, { name: 'Random', color: P.healthy, data: pt(r.random) } ] });
  const f = (k) => (k < 0 ? 'never' : k + ' machine(s)');
  $('#robust-note').innerHTML = `Largest segment falls below half after <b>${f(r.halfTargeted)}</b> removed in a targeted attack, versus <b>${f(r.halfRandom)}</b> at random.`;
}

async function runMonte() {
  const r = await api('monte', { source: G.selected, target: G.target, aggr: $('#aggr').value, count: $('#atk-count').value, trials: 200 }, 'GET');
  const colors = { bfs: P.healthy, dfs: P.teal, apt: P.violet, hub: P.infected, random: P.isolated };
  drawBars($('#ch-monte'), { labelW: 130, items: r.rows.map((x) => ({ label: x.label, value: x.avg, min: x.min, max: x.max, color: colors[x.mode], text: `${x.avg.toFixed(1)}  (${x.min}-${x.max})` })) });
  $('#monte-note').innerHTML = `Average machines compromised over <b>${r.trials}</b> seeded runs; whiskers show the minimum and maximum.` +
    (G.selected < 0 ? ' Select an entry machine to include BFS, DFS and APT.' : '');
}

async function runAdvisor() {
  if (G.selected < 0) { toast('Select the machine the attack will start from first.', 'err'); return; }
  const r = await api('advise', { source: G.selected, aggr: $('#aggr').value }, 'GET');
  showTab('analytics');
  const el = $('#advisor');
  if (!r.ok) { el.innerHTML = `<p class="empty">${esc(r.message)}</p>`; return; }
  G.marked = new Set(r.best.map((b) => b.node));
  el.innerHTML = `<p class="note" style="margin:0 0 6px">Entry point <b>${esc(nodeName(r.source))}</b>. Expected spread without defence: <b>${r.baseline.toFixed(1)}</b> machines.</p><ul class="list">` +
    r.best.map((b) => `<li class="click" data-node="${b.node}"><span>${esc(nodeName(b.node))} <small style="color:var(--muted)">expected ${b.avg.toFixed(1)} (${r.baseline > 0 ? Math.round((100 * (r.baseline - b.avg)) / r.baseline) : 0}% less)</small></span>` +
      `<button class="small" data-isolate="${b.node}">Isolate</button></li>`).join('') + '</ul>';
  setBanner('Defence advisor: the highlighted machines reduce the expected spread the most if isolated now.');
}

/* ---------- algorithms tab ---------- */
async function runMstCompare() {
  const r = await api('mstcompare', {}, 'GET');
  G.mst.kruskal = new Set(r.kruskal.mst.map((e) => keyOf(e.u, e.v)));
  G.mst.prim = new Set(r.prim.mst.map((e) => keyOf(e.u, e.v)));
  const k = r.kruskal, p = r.prim, fast = k.micros <= p.micros ? 'k' : 'p';
  const save = r.full > 0 ? Math.round((100 * (r.full - k.cost)) / r.full) : 0;
  $('#mst-compare').innerHTML =
    `<table class="data"><tr><th></th><th>Kruskal</th><th>Prim</th></tr>` +
    `<tr><td>Backbone cost</td><td>${k.cost}</td><td>${p.cost}</td></tr>` +
    `<tr><td>Links used</td><td>${k.edges}</td><td>${p.edges}</td></tr>` +
    `<tr><td>Heap operations</td><td>${k.heapOps}</td><td>${p.heapOps}</td></tr>` +
    `<tr><td>Union-find operations</td><td>${k.dsuOps}</td><td>-</td></tr>` +
    `<tr class="best"><td>Average time</td><td>${fast === 'k' ? '<b>' : ''}${fmtUs(k.micros)}${fast === 'k' ? '</b>' : ''}</td><td>${fast === 'p' ? '<b>' : ''}${fmtUs(p.micros)}${fast === 'p' ? '</b>' : ''}</td></tr></table>` +
    `<p class="note">Both give the same cost: <b>${r.sameCost ? 'yes' : 'NO'}</b>. The backbone costs <b>${k.cost}</b> against <b>${r.full}</b> for all links (${save}% saved).</p>` +
    `<div class="btns"><button class="small" data-show="kruskal">Show Kruskal</button><button class="small" data-show="prim">Show Prim</button>` +
    `<button class="small" data-show="both">Show both</button><button class="small ghost" data-show="none">Hide</button></div>`;
  G.mstShow = 'both';
}

async function runBenchmark() {
  const btn = $('#btn-bench'); btn.disabled = true; btn.textContent = 'Running...';
  try {
    const r = await api('benchmark', { topology: $('#topology').value }, 'GET');
    const cols = [['bfs', 'BFS', P.healthy], ['dfs', 'DFS', P.teal], ['dijkstra', 'Dijkstra', P.amber], ['kruskal', 'Kruskal', P.infected], ['prim', 'Prim', P.violet], ['critical', 'Cut pts', P.muted]];
    drawLines($('#ch-bench'), { h: 220, logY: true, xTitle: 'machines', xFmt: (v) => Math.round(v), yFmt: fmtUs,
      series: cols.map(([k, name, color]) => ({ name, color, data: r.scaling.map((row) => ({ x: row.n, y: Math.max(0.1, row[k]) })) })) });
    drawGrouped($('#ch-bench2'), { h: 190, names: ['Kruskal', 'Prim'], colors: [P.infected, P.violet],
      groups: r.compare.map((row) => ({ label: row.label, values: [row.kruskal, row.prim] })) });
    $('#bench-table').innerHTML = `<p class="note">All algorithms at 800 machines, average microseconds per run.</p><table class="data"><tr><th>Topology</th>` +
      cols.map((c) => `<th>${c[1]}</th>`).join('') + '</tr>' +
      r.compare.map((row) => `<tr><td>${row.label}</td>` + cols.map(([k]) => `<td>${row[k] >= 100 ? Math.round(row[k]) : row[k]}</td>`).join('') + '</tr>').join('') + '</table>';
  } finally { btn.disabled = false; btn.textContent = 'Run'; }
}
