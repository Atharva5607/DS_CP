'use strict';
// Tiny canvas chart helpers (no external libraries, work offline, follow the theme).
function prep(c, h) {
  const dpr = window.devicePixelRatio || 1;
  const w = Math.max(200, c.clientWidth || c.parentElement.clientWidth || 300);
  c.style.height = h + 'px';
  c.width = Math.round(w * dpr); c.height = Math.round(h * dpr);
  const ctx = c.getContext('2d');
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, w, h);
  ctx.font = '11px "Segoe UI", system-ui, sans-serif';
  return { ctx, w, h };
}
function emptyChart(c, h, text) {
  const { ctx, w } = prep(c, h);
  ctx.fillStyle = P.muted; ctx.textAlign = 'center'; ctx.font = '12.5px "Segoe UI", system-ui, sans-serif';
  ctx.fillText(text, w / 2, h / 2 + 4);
}
function legend(ctx, items, x, y) {
  ctx.textAlign = 'left';
  items.forEach((it) => {
    ctx.fillStyle = it.color; ctx.fillRect(x, y - 8, 10, 10);
    ctx.fillStyle = P.ink; ctx.fillText(it.name, x + 14, y + 1);
    x += 18 + ctx.measureText(it.name).width;
  });
}

// series: [{name, color, data:[{x,y}], dashed}]
function drawLines(c, o) {
  const h = o.h || 190;
  const all = [];
  o.series.forEach((s) => s.data.forEach((p) => all.push(p)));
  if (!all.length) return emptyChart(c, 90, o.empty || 'No data yet');
  const { ctx, w } = prep(c, h);
  const m = { l: 46, r: 24, t: 26, b: 28 }, iw = w - m.l - m.r, ih = h - m.t - m.b;
  const xs = all.map((p) => p.x), ys = all.map((p) => p.y);
  const xMin = o.xMin !== undefined ? o.xMin : Math.min(...xs), xMax = o.xMax !== undefined ? o.xMax : Math.max(...xs);
  let yMin = o.logY ? Math.max(0.1, Math.min(...ys) * 0.7) : (o.yMin !== undefined ? o.yMin : 0);
  let yMax = o.yMax !== undefined ? o.yMax : Math.max(...ys) * (o.logY ? 1.4 : 1.1);
  if (!(yMax > yMin)) yMax = yMin + 1;
  const X = (x) => m.l + (xMax === xMin ? 0.5 : (x - xMin) / (xMax - xMin)) * iw;
  const lo = Math.log10(yMin), hi = Math.log10(yMax);
  const Y = (y) => o.logY ? m.t + ih - ((Math.log10(Math.max(y, yMin)) - lo) / (hi - lo || 1)) * ih
                          : m.t + ih - ((y - yMin) / (yMax - yMin)) * ih;
  ctx.strokeStyle = P.line; ctx.fillStyle = P.muted; ctx.lineWidth = 1; ctx.textAlign = 'right';
  const ticks = [];
  if (o.logY) { for (let d = Math.ceil(lo); d <= Math.floor(hi); d++) ticks.push(Math.pow(10, d)); }
  else { for (let i = 0; i <= 4; i++) ticks.push(yMin + ((yMax - yMin) * i) / 4); }
  ticks.forEach((t) => {
    const y = Y(t);
    ctx.beginPath(); ctx.moveTo(m.l, y); ctx.lineTo(w - m.r, y); ctx.stroke();
    ctx.fillText((o.yFmt || ((v) => (v >= 100 ? Math.round(v) : +v.toFixed(1))))(t), m.l - 6, y + 3);
  });
  ctx.textAlign = 'center';
  const nx = Math.min(5, Math.max(2, Math.floor(iw / 70)));
  for (let i = 0; i < nx; i++) {
    const xv = xMin + ((xMax - xMin) * i) / (nx - 1);
    ctx.fillText((o.xFmt || ((v) => Math.round(v)))(xv), X(xv), h - 10);
  }
  if (o.xTitle) { ctx.textAlign = 'right'; ctx.fillText(o.xTitle, w - m.r, h - 1); }
  o.series.forEach((s) => {
    if (!s.data.length) return;
    ctx.strokeStyle = s.color; ctx.lineWidth = 2; ctx.setLineDash(s.dashed ? [5, 4] : []);
    ctx.beginPath();
    s.data.forEach((p, i) => (i ? ctx.lineTo(X(p.x), Y(p.y)) : ctx.moveTo(X(p.x), Y(p.y))));
    ctx.stroke(); ctx.setLineDash([]);
    if (s.data.length <= 12) { ctx.fillStyle = s.color; s.data.forEach((p) => { ctx.beginPath(); ctx.arc(X(p.x), Y(p.y), 2.6, 0, 7); ctx.fill(); }); }
  });
  legend(ctx, o.series.map((s) => ({ name: s.name, color: s.color })), 8, 12);
}

// items: [{label, value, color, min, max}] horizontal bars with optional min-max whiskers
function drawBars(c, o) {
  if (!o.items.length) return emptyChart(c, 90, o.empty || 'No data yet');
  const rowH = 28, h = o.items.length * rowH + 12;
  const { ctx, w } = prep(c, h);
  const lw = o.labelW || 118, max = Math.max(...o.items.map((i) => Math.max(i.value, i.max || 0)), o.minMax || 1) * 1.08;
  const iw = w - lw - 96;
  ctx.textAlign = 'right'; ctx.textBaseline = 'middle';
  o.items.forEach((it, k) => {
    const y = 6 + k * rowH + rowH / 2;
    ctx.fillStyle = P.ink; ctx.fillText(it.label, lw - 8, y);
    ctx.fillStyle = it.color; ctx.fillRect(lw, y - 8, Math.max(2, (it.value / max) * iw), 16);
    if (it.min !== undefined) {
      ctx.strokeStyle = P.ink; ctx.lineWidth = 1.5;
      const a = lw + (it.min / max) * iw, b = lw + (it.max / max) * iw;
      ctx.beginPath(); ctx.moveTo(a, y); ctx.lineTo(b, y); ctx.moveTo(a, y - 5); ctx.lineTo(a, y + 5); ctx.moveTo(b, y - 5); ctx.lineTo(b, y + 5); ctx.stroke();
    }
    ctx.fillStyle = P.ink; ctx.textAlign = 'left';
    ctx.fillText(it.text !== undefined ? it.text : (+it.value.toFixed(1)).toString(), lw + (Math.max(it.value, it.max || 0) / max) * iw + 6, y);
    ctx.textAlign = 'right';
  });
  ctx.textBaseline = 'alphabetic';
}

// groups: [{label, values:[...]}], names:[...], colors:[...]
function drawGrouped(c, o) {
  const h = o.h || 190;
  const { ctx, w } = prep(c, h);
  const m = { l: 44, r: 8, t: 26, b: 28 }, iw = w - m.l - m.r, ih = h - m.t - m.b;
  const max = Math.max(...o.groups.flatMap((g) => g.values), 1) * 1.1;
  ctx.strokeStyle = P.line; ctx.fillStyle = P.muted; ctx.textAlign = 'right';
  for (let i = 0; i <= 4; i++) {
    const v = (max * i) / 4, y = m.t + ih - (v / max) * ih;
    ctx.beginPath(); ctx.moveTo(m.l, y); ctx.lineTo(w - m.r, y); ctx.stroke();
    ctx.fillText(v >= 100 ? Math.round(v) : +v.toFixed(1), m.l - 6, y + 3);
  }
  const gw = iw / o.groups.length, bw = Math.min(26, (gw - 8) / o.names.length);
  o.groups.forEach((g, gi) => {
    const x0 = m.l + gi * gw + (gw - bw * o.names.length) / 2;
    g.values.forEach((v, k) => {
      ctx.fillStyle = o.colors[k]; const bh = (v / max) * ih;
      ctx.fillRect(x0 + k * bw, m.t + ih - bh, bw - 2, bh);
    });
    ctx.fillStyle = P.muted; ctx.textAlign = 'center'; ctx.fillText(g.label, m.l + gi * gw + gw / 2, h - 10);
  });
  legend(ctx, o.names.map((n, i) => ({ name: n, color: o.colors[i] })), 8, 12);
}

// parts: [{label, value, color}]
function drawDonut(c, o) {
  const { ctx, w } = prep(c, 150);
  const total = o.parts.reduce((a, p) => a + p.value, 0) || 1;
  const cx = 78, cy = 75, R = 62, r = 38;
  let a0 = -Math.PI / 2;
  o.parts.forEach((p) => {
    if (!p.value) return;
    const a1 = a0 + (p.value / total) * Math.PI * 2;
    ctx.beginPath(); ctx.arc(cx, cy, R, a0, a1); ctx.arc(cx, cy, r, a1, a0, true); ctx.closePath();
    ctx.fillStyle = p.color; ctx.fill(); a0 = a1;
  });
  ctx.fillStyle = P.ink; ctx.textAlign = 'center'; ctx.font = 'bold 20px Palatino, Georgia, serif';
  ctx.fillText(o.center, cx, cy + 4); ctx.font = '11px "Segoe UI", system-ui, sans-serif'; ctx.fillStyle = P.muted;
  ctx.fillText(o.sub || '', cx, cy + 19);
  ctx.textAlign = 'left';
  o.parts.forEach((p, i) => {
    const y = 34 + i * 24;
    ctx.fillStyle = p.color; ctx.fillRect(168, y - 9, 11, 11);
    ctx.fillStyle = P.ink; ctx.font = '12.5px "Segoe UI", system-ui, sans-serif';
    ctx.fillText(`${p.label}  ${p.value}`, 186, y);
  });
}

function drawHistogram(c, o) {   // o.values: integers, o.color
  if (!o.values.length) return emptyChart(c, 90, 'No machines');
  const counts = {};
  o.values.forEach((v) => { counts[v] = (counts[v] || 0) + 1; });
  const keys = Object.keys(counts).map(Number).sort((a, b) => a - b);
  const lo = keys[0], hi = keys[keys.length - 1], groups = [];
  for (let d = lo; d <= hi; d++) groups.push({ label: String(d), values: [counts[d] || 0] });
  drawGrouped(c, { groups, names: ['Machines with this many links'], colors: [o.color], h: 150 });
}
