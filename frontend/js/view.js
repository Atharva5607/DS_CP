'use strict';
// The network map: layout, pan/zoom, dragging, colour modes and drawing.
const cv = $('#map'), ctx = cv.getContext('2d');
let W = 0, H = 0;
const reduceMotion = matchMedia('(prefers-reduced-motion: reduce)').matches;

function resize() {
  const r = cv.getBoundingClientRect(), dpr = window.devicePixelRatio || 1;
  W = r.width; H = r.height;
  cv.width = Math.round(W * dpr); cv.height = Math.round(H * dpr);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
}
new ResizeObserver(() => { const first = W === 0; resize(); if (first && G.S) fitView(); }).observe(cv);

const sx = (i) => G.view.tx + G.pos[i].x * G.view.scale;
const sy = (i) => G.view.ty + G.pos[i].y * G.view.scale;
const alpha = (hex, a) => { const c = hexToRgb(hex); return `rgba(${c[0]},${c[1]},${c[2]},${a})`; };
const statusOf = (i) => (G.shown && G.shown[i] !== undefined ? G.shown[i] : G.S.nodes[i].status);
const isBlocked = (e) => (G.shownBlocked ? G.shownBlocked.has(keyOf(e.u, e.v)) : e.blocked);

/* ---------- layouts ---------- */
function layoutForce() {
  const n = G.S.nodes.length;
  if (!n) return;
  const rnd = (i, k) => { const v = Math.sin(i * k) * 43758.5453; return v - Math.floor(v); };
  const p = G.S.nodes.map((_, i) => ({
    x: 0.5 + 0.35 * Math.cos((2 * Math.PI * i) / n) + (rnd(i, 12.9898) - 0.5) * 0.06,
    y: 0.5 + 0.35 * Math.sin((2 * Math.PI * i) / n) + (rnd(i, 78.233) - 0.5) * 0.06,
  }));
  const k = 0.85 / Math.sqrt(n);
  for (let it = 0; it < 450; it++) {
    const temp = 0.08 * (1 - it / 450) + 0.004;
    const f = p.map(() => ({ x: 0, y: 0 }));
    for (let i = 0; i < n; i++) for (let j = i + 1; j < n; j++) {
      let dx = p[i].x - p[j].x, dy = p[i].y - p[j].y;
      const d = Math.max(0.001, Math.hypot(dx, dy)), rep = (k * k) / d;
      dx /= d; dy /= d;
      f[i].x += dx * rep; f[i].y += dy * rep; f[j].x -= dx * rep; f[j].y -= dy * rep;
    }
    for (const e of G.S.edges) {
      let dx = p[e.u].x - p[e.v].x, dy = p[e.u].y - p[e.v].y;
      const d = Math.max(0.001, Math.hypot(dx, dy)), att = (d * d) / k;
      dx /= d; dy /= d;
      f[e.u].x -= dx * att; f[e.u].y -= dy * att; f[e.v].x += dx * att; f[e.v].y += dy * att;
    }
    for (let i = 0; i < n; i++) {
      f[i].x += (0.5 - p[i].x) * 0.3; f[i].y += (0.5 - p[i].y) * 0.3;
      const m = Math.hypot(f[i].x, f[i].y) || 1, step = Math.min(m, temp);
      p[i].x += (f[i].x / m) * step; p[i].y += (f[i].y / m) * step;
    }
  }
  const xs = p.map((q) => q.x), ys = p.map((q) => q.y);
  const x0 = Math.min(...xs), x1 = Math.max(...xs), y0 = Math.min(...ys), y1 = Math.max(...ys);
  G.pos = p.map((q) => ({ x: ((q.x - x0) / (x1 - x0 || 1)) * 1000, y: ((q.y - y0) / (y1 - y0 || 1)) * 620 }));
}

function fitView() {
  if (!G.pos.length || !W) return;
  const xs = G.pos.map((q) => q.x), ys = G.pos.map((q) => q.y);
  const x0 = Math.min(...xs), x1 = Math.max(...xs), y0 = Math.min(...ys), y1 = Math.max(...ys);
  const bw = Math.max(1, x1 - x0), bh = Math.max(1, y1 - y0);
  const s = Math.min((W - 150) / bw, (H - 170) / bh, 2.4);
  G.view.scale = s;
  G.view.tx = (W - bw * s) / 2 - x0 * s;
  G.view.ty = (H - bh * s) / 2 - y0 * s + 8;
}
function zoomAt(x, y, f) {
  const v = G.view, ns = Math.max(0.3, Math.min(4, v.scale * f)), k = ns / v.scale;
  v.tx = x - (x - v.tx) * k; v.ty = y - (y - v.ty) * k; v.scale = ns;
}

/* ---------- colours ---------- */
function nodeFill(m, i) {
  const st = statusOf(i);
  if (G.colorBy === 'status') return P[st];
  if (st === 'infected') return P.infected;
  if (st === 'isolated') return P.isolated;
  if (G.colorBy === 'risk') return lerpColor(P.healthy, P.infected, m.risk / 100);
  if (G.colorBy === 'segment') return m.seg < 0 ? P.isolated : SEG_COLORS[m.seg % SEG_COLORS.length];
  if (G.colorBy === 'critical') return m.cut ? P.amber : P.healthy;
  return P.healthy;
}
function nodeRadius(m) {
  const z = Math.min(1.6, Math.max(0.75, Math.sqrt(G.view.scale)));
  const base = G.sizeByImp ? 5 + m.imp * 0.95 : (m.type === 'PC' ? 8 : 10);
  return base * z;
}
function shapePath(type, x, y, r) {
  ctx.beginPath();
  if (type === 'PC') ctx.arc(x, y, r, 0, Math.PI * 2);
  else if (type === 'SERVER') ctx.rect(x - r, y - r, 2 * r, 2 * r);
  else { ctx.moveTo(x, y - r * 1.3); ctx.lineTo(x + r * 1.3, y); ctx.lineTo(x, y + r * 1.3); ctx.lineTo(x - r * 1.3, y); ctx.closePath(); }
}

/* ---------- drawing ---------- */
function draw(now) {
  ctx.clearRect(0, 0, W, H);
  if (!G.S || !G.pos.length || G.pos.length !== G.S.nodes.length) { requestAnimationFrame(draw); return; }
  G.flashes = G.flashes.filter((f) => now - f.start < f.dur);
  const routeEdges = new Set();
  for (let i = 1; i < G.route.length; i++) routeEdges.add(keyOf(G.route[i - 1], G.route[i]));
  const showK = G.mstShow === 'kruskal' || G.mstShow === 'both', showP = G.mstShow === 'prim' || G.mstShow === 'both';
  const z = Math.min(1.5, Math.max(0.8, Math.sqrt(G.view.scale)));

  for (const e of G.S.edges) {
    const k = keyOf(e.u, e.v), blocked = isBlocked(e);
    const dead = G.S.nodes[e.u].status === 'isolated' || G.S.nodes[e.v].status === 'isolated';
    const x1 = sx(e.u), y1 = sy(e.u), x2 = sx(e.v), y2 = sy(e.v);
    let color = e.risk >= 65 ? alpha(P.infected, 0.42) : alpha(P.ink, 0.26), w = (1 + e.imp / 5) * z, dash = [];
    if (dead) { color = alpha(P.ink, 0.12); dash = [4, 5]; }
    if (G.colorBy === 'critical' && e.bridge && !blocked) { color = P.amber; w = 4 * z; }
    if (blocked) { color = alpha(P.infected, 0.8); dash = [6, 5]; w = 2 * z; }
    ctx.setLineDash(dash); ctx.strokeStyle = color; ctx.lineWidth = w;
    ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke(); ctx.setLineDash([]);
    if (showK && G.mst.kruskal.has(k)) { ctx.strokeStyle = P.teal; ctx.lineWidth = 4.5 * z; ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke(); }
    if (showP && G.mst.prim.has(k)) { ctx.strokeStyle = P.violet; ctx.lineWidth = showK ? 2 * z : 4.5 * z; ctx.setLineDash(showK ? [3, 5] : []); ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke(); ctx.setLineDash([]); }
    if (routeEdges.has(k)) { ctx.strokeStyle = P.amber; ctx.lineWidth = 5.5 * z; ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke(); }
    const mx = (x1 + x2) / 2, my = (y1 + y2) / 2;
    if (blocked) {   // a small X marks a cut link
      ctx.strokeStyle = P.infected; ctx.lineWidth = 2.2;
      ctx.beginPath(); ctx.moveTo(mx - 5, my - 5); ctx.lineTo(mx + 5, my + 5); ctx.moveTo(mx + 5, my - 5); ctx.lineTo(mx - 5, my + 5); ctx.stroke();
    } else if (G.showCosts) {
      ctx.font = '10.5px "Segoe UI", system-ui, sans-serif'; ctx.textAlign = 'center';
      const t = String(e.cost), tw = ctx.measureText(t).width + 8;
      ctx.fillStyle = alpha(P.paper, 0.9); ctx.fillRect(mx - tw / 2, my - 7, tw, 14);
      ctx.fillStyle = P.muted; ctx.fillText(t, mx, my + 3.5);
    }
  }

  // moving packets and fading highlights on links used by the algorithm
  for (const f of G.flashes) {
    const p = (now - f.start) / f.dur;
    ctx.globalAlpha = Math.max(0, 1 - p);
    ctx.strokeStyle = f.color; ctx.lineWidth = 4.5 * z;
    ctx.beginPath(); ctx.moveTo(sx(f.a), sy(f.a)); ctx.lineTo(sx(f.b), sy(f.b)); ctx.stroke();
    ctx.globalAlpha = 1;
    if (f.packet) {
      const q = Math.min(1, p * 1.6);
      ctx.fillStyle = f.color; ctx.beginPath();
      ctx.arc(sx(f.a) + (sx(f.b) - sx(f.a)) * q, sy(f.a) + (sy(f.b) - sy(f.a)) * q, 5 * z, 0, 7); ctx.fill();
    }
  }

  const many = G.S.nodes.length > 45 && G.view.scale < 0.9;
  G.S.nodes.forEach((m, i) => {
    const st = statusOf(i), x = sx(i), y = sy(i), r = nodeRadius(m);
    if (st === 'infected' && !reduceMotion) {
      const p = (now / 900 + i * 0.13) % 1;
      ctx.strokeStyle = alpha(P.infected, 0.5 * (1 - p)); ctx.lineWidth = 2;
      ctx.beginPath(); ctx.arc(x, y, r + 4 + p * 14, 0, Math.PI * 2); ctx.stroke();
    }
    if (G.marked.has(i)) { shapePath(m.type, x, y, r + 8); ctx.fillStyle = alpha(P.amber, 0.28); ctx.fill(); ctx.strokeStyle = P.amber; ctx.lineWidth = 2; ctx.stroke(); }
    shapePath(m.type, x, y, r);
    ctx.fillStyle = nodeFill(m, i); ctx.fill();
    ctx.lineWidth = 2; ctx.strokeStyle = st === 'isolated' ? P.muted : P.paper;
    if (st === 'isolated') ctx.setLineDash([3, 3]);
    ctx.stroke(); ctx.setLineDash([]);
    if (st === 'recovered' && G.colorBy !== 'status') { shapePath(m.type, x, y, r + 3); ctx.strokeStyle = P.recovered; ctx.lineWidth = 2.5; ctx.stroke(); }
    if (G.visited.has(i)) { shapePath(m.type, x, y, r + 5); ctx.strokeStyle = P.amber; ctx.lineWidth = 3; ctx.stroke(); }
    if (i === G.current) { shapePath(m.type, x, y, r + 6); ctx.strokeStyle = P.teal; ctx.lineWidth = 3; ctx.stroke(); }
    if (i === G.selected) { shapePath(m.type, x, y, r + 7); ctx.strokeStyle = P.ink; ctx.lineWidth = 2.5; ctx.stroke(); }
    if (i === G.target) { shapePath(m.type, x, y, r + 7); ctx.strokeStyle = P.teal; ctx.lineWidth = 2.5; ctx.setLineDash([4, 3]); ctx.stroke(); ctx.setLineDash([]); }
    if (G.dist[i] !== undefined) {
      const t = String(G.dist[i]);
      ctx.font = 'bold 10.5px "Segoe UI", system-ui, sans-serif'; ctx.textAlign = 'center';
      const tw = ctx.measureText(t).width + 8;
      ctx.fillStyle = P.amber; ctx.fillRect(x - tw / 2, y - r - 19, tw, 14);
      ctx.fillStyle = '#111'; ctx.fillText(t, x, y - r - 8);
    }
    if ((G.showNames && !many) || i === G.selected || i === G.target || i === G.hover) {
      ctx.fillStyle = P.ink; ctx.font = '11px "Segoe UI", system-ui, sans-serif'; ctx.textAlign = 'center';
      ctx.fillText(m.name, x, y + r + 14);
    }
  });
  requestAnimationFrame(draw);
}

function addFlash(a, b, color, ms, packet) { G.flashes.push({ a, b, color, dur: ms, start: performance.now(), packet: !!packet }); }

/* ---------- interaction ---------- */
function mouse(ev) { const r = cv.getBoundingClientRect(); return { x: ev.clientX - r.left, y: ev.clientY - r.top }; }
function nodeAt(x, y) {
  let best = -1, bd = 1e9;
  G.S.nodes.forEach((m, i) => { const d = Math.hypot(sx(i) - x, sy(i) - y); if (d < nodeRadius(m) + 6 && d < bd) { bd = d; best = i; } });
  return best;
}
let drag = null;
cv.addEventListener('pointerdown', (ev) => {
  if (ev.button === 2 || !G.S) return;
  cv.setPointerCapture(ev.pointerId);
  const { x, y } = mouse(ev), hit = nodeAt(x, y);
  drag = hit >= 0 ? { type: 'node', id: hit, x0: x, y0: y, moved: false } : { type: 'pan', x0: x, y0: y, tx0: G.view.tx, ty0: G.view.ty, moved: false };
});
cv.addEventListener('pointermove', (ev) => {
  if (!G.S) return;
  const { x, y } = mouse(ev), tip = $('#tip');
  if (drag) {
    const dx = x - drag.x0, dy = y - drag.y0;
    if (Math.hypot(dx, dy) > 3) { drag.moved = true; cv.classList.add('dragging'); }
    if (drag.moved) {
      if (drag.type === 'node') { G.pos[drag.id].x = (x - G.view.tx) / G.view.scale; G.pos[drag.id].y = (y - G.view.ty) / G.view.scale; }
      else { G.view.tx = drag.tx0 + dx; G.view.ty = drag.ty0 + dy; }
    }
    tip.hidden = true; return;
  }
  G.hover = nodeAt(x, y);
  if (G.hover < 0) { tip.hidden = true; return; }
  const m = G.S.nodes[G.hover];
  tip.innerHTML = `<b>${esc(m.name)}</b> (${m.type.toLowerCase()})<br>importance ${m.imp}, risk ${m.risk}, links ${m.deg}<br>${statusOf(G.hover)}${m.cut ? ', critical' : ''}`;
  tip.hidden = false;
  tip.style.left = Math.min(x + 14, W - 190) + 'px'; tip.style.top = Math.min(y + 14, H - 70) + 'px';
});
cv.addEventListener('pointerup', (ev) => {
  if (drag && !drag.moved) {
    if (drag.type === 'node') { if (ev.shiftKey) selectSecond(drag.id); else selectPrimary(drag.id); }
    else clearSelection();
  }
  drag = null; cv.classList.remove('dragging');
});
cv.addEventListener('contextmenu', (ev) => {
  ev.preventDefault();
  if (!G.S) return;
  const { x, y } = mouse(ev), hit = nodeAt(x, y);
  if (hit >= 0) selectSecond(hit);
});
cv.addEventListener('mouseleave', () => { G.hover = -1; $('#tip').hidden = true; });
cv.addEventListener('wheel', (ev) => { ev.preventDefault(); const { x, y } = mouse(ev); zoomAt(x, y, Math.exp(-ev.deltaY * 0.0016)); }, { passive: false });

/* ---------- legend, chips, image export ---------- */
function renderLegend() {
  const sw = (c, t) => `<li><i class="dot" style="--c:${c}"></i>${t}</li>`;
  let html = '';
  if (G.colorBy === 'status') html = sw(P.healthy, 'Healthy') + sw(P.infected, 'Infected') + sw(P.isolated, 'Isolated') + sw(P.recovered, 'Recovered');
  else if (G.colorBy === 'risk') html = '<li>Low<i class="ramp"></i>High risk</li>' + sw(P.infected, 'Infected') + sw(P.isolated, 'Isolated');
  else if (G.colorBy === 'segment') html = '<li>Colour = connected segment</li>' + sw(P.infected, 'Infected') + sw(P.isolated, 'Isolated');
  else html = sw(P.amber, 'Critical machine (cut vertex)') + sw(P.healthy, 'Other') + '<li>Amber links are bridges</li>';
  html += '<li><i class="sh circ"></i>PC</li><li><i class="sh"></i>Server</li><li><i class="sh dia"></i>Router</li>';
  $('#legend').innerHTML = html;
}
function renderChips() {
  const chip = (label, i) => (i >= 0 && G.S && G.S.nodes[i] ? `<span class="chip"><b>${label}</b> ${esc(G.S.nodes[i].name)}</span>` : '');
  $('#chips').innerHTML = chip('First', G.selected) + chip('Second', G.target);
}
function exportPng() {
  const out = document.createElement('canvas');
  out.width = cv.width; out.height = cv.height;
  const c = out.getContext('2d');
  c.fillStyle = P.paper; c.fillRect(0, 0, out.width, out.height);
  c.drawImage(cv, 0, 0);
  out.toBlob((b) => {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(b); a.download = 'network-map.png'; a.click();
    setTimeout(() => URL.revokeObjectURL(a.href), 2000);
  });
}
