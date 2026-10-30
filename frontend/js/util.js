'use strict';
// Shared helpers and the single mutable state object G used by every script.
const $ = (s, r = document) => r.querySelector(s);
const $$ = (s, r = document) => Array.from(r.querySelectorAll(s));
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const keyOf = (u, v) => (u < v ? u + '-' + v : v + '-' + u);
const STATUS = ['healthy', 'infected', 'isolated', 'recovered'];   // same order as the C++ enum
const SEG_COLORS = ['#2f6f9f', '#0e8f8a', '#7c5cbf', '#d98e12', '#4a7c3a', '#8a5a44', '#2a8bb6', '#b0507d'];

const G = {
  S: null, pos: [], selected: -1, target: -1, hover: -1,
  shown: null, shownBlocked: null, flashes: [], route: [], marked: new Set(), visited: new Set(),
  dist: {}, current: -1, mst: { kruskal: new Set(), prim: new Set() }, mstShow: 'none',
  colorBy: 'status', showNames: true, showCosts: false, sizeByImp: true,
  busy: false, lastAttack: null, base: null, view: { scale: 1, tx: 0, ty: 0 }, tab: 'overview',
};

let P = {};
function refreshPalette() {
  const cs = getComputedStyle(document.documentElement);
  const v = (n) => cs.getPropertyValue(n).trim();
  P = { paper: v('--paper'), panel: v('--panel'), card: v('--card'), ink: v('--ink'), muted: v('--muted'), line: v('--line'),
        healthy: v('--healthy'), infected: v('--infected'), recovered: v('--recovered'), isolated: v('--isolated'),
        amber: v('--amber'), teal: v('--teal'), violet: v('--violet'), grid: v('--grid') };
}

function hexToRgb(h) {
  const m = /^#?([0-9a-f]{6})$/i.exec(h.trim());
  const n = m ? parseInt(m[1], 16) : 0;
  return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}
function lerpColor(a, b, t) {
  const x = hexToRgb(a), y = hexToRgb(b);
  const c = x.map((v, i) => Math.round(v + (y[i] - v) * Math.max(0, Math.min(1, t))));
  return `rgb(${c[0]},${c[1]},${c[2]})`;
}

async function api(path, params = {}, method = 'POST', body = null) {
  const qs = new URLSearchParams(params).toString();
  const opt = { method, headers: {} };
  if (body !== null) { opt.body = body; opt.headers['Content-Type'] = 'text/plain'; }
  const res = await fetch('/api/' + path + (qs ? '?' + qs : ''), opt);
  if (!res.ok) throw new Error('HTTP ' + res.status);
  return res.json();
}

function toast(msg, kind) {
  const t = document.createElement('div');
  t.className = 'toast ' + (kind || '');
  t.textContent = msg;
  $('#toasts').appendChild(t);
  setTimeout(() => t.classList.add('out'), 3800);
  setTimeout(() => t.remove(), 4300);
}
function setBanner(t) { $('#banner').textContent = t; }
const nodeName = (i) => (G.S && G.S.nodes[i] ? G.S.nodes[i].name : '?');
