"use strict";
/* SFA Console: the Prototype Designer. Shape the whole instrument: drag keys, pads, screen, encoders and buttons on a
   to-scale top view, check the front and side, pick materials and a vibe, save designs and compare them.
   Designs live in hardware/prototype/plan.json under "designs". All sizes are millimetres. */
const DZ = { cur: 0, sel: null, view: "top", tab: "design", cmpA: 0, cmpB: 1, hist: [], redo: [], drag: null };
const DZ_SCREENS = { "2.8": { w: 57.6, h: 43.2, px: "320 x 240" }, "3.5": { w: 73.4, h: 49.0, px: "480 x 320" }, "4.0": { w: 83.5, h: 55.7, px: "480 x 320" }, "4.3": { w: 95.0, h: 53.9, px: "480 x 272" }, "5.0": { w: 108.0, h: 64.8, px: "800 x 480" } };
const DZ_WHITES = { 13: 8, 25: 15, 32: 19, 37: 22 };
const DZ_STYLES = {
  clean: { name: "Clean moulded", say: "One smooth shell, soft corners, hidden fasteners.", r: 9, body: "#ecebf5", pad: "#ff2e7e", accent: "#19b8d4", mat: { case: "Moulded ABS or a printed shell, matte", top: "Same piece as the case", keys: "Standard white and black keycaps", pads: "Silicone, single colour", knobs: "Soft-touch caps", finish: "Fine texture, one colour" } },
  industrial: { name: "Assembled / industrial", say: "Flat panels bolted together: bent steel or aluminium, visible screws, stencilled labels.", r: 2, body: "#b9bcc6", pad: "#ffb300", accent: "#00b8d9", mat: { case: "1.5 mm bent steel or aluminium tray", top: "Flat aluminium panel, screwed down", keys: "Standard keycaps, black only is an option", pads: "Silicone, amber", knobs: "Knurled metal", finish: "Raw brushed or powder coat, stencil legends" } },
  junk: { name: "Recycled junk", say: "Built from what is around: mismatched panels, reclaimed plastics, odd knobs, label tape. Every unit a bit different.", r: 4, body: "#c9c2a8", pad: "#ff5a36", accent: "#3fbf6f", mat: { case: "Reclaimed sheet plastic (HDPE board, old signage), cut panels", top: "Patchwork panels, different colours allowed", keys: "Salvaged or mixed keycaps", pads: "Silicone, mixed colours", knobs: "Whatever fits a 6 mm shaft", finish: "Label-tape legends, visible screws, no paint" } },
  wood: { name: "Wood ends", say: "A metal or plastic deck between two solid wooden cheeks.", r: 3, body: "#e8e4dc", pad: "#ff2e7e", accent: "#19b8d4", mat: { case: "Folded metal deck", top: "Same as the deck", keys: "Standard keycaps", pads: "Silicone", knobs: "Turned aluminium or wood", finish: "Oiled hardwood cheeks, painted deck" } }
};
const DZ_TYPES = {
  keybed: { name: "Keybed", def: { keys: 25, kp: 18, depth: 0 } }, pads: { name: "Pads", def: { cols: 8, rows: 2, size: 26, gap: 4 } }, screen: { name: "Screen", def: { diag: "3.5" } },
  encoders: { name: "Encoders", def: { n: 4, pitch: 26, vertical: 0 } }, buttons: { name: "Buttons", def: { cols: 6, rows: 1, bw: 12, bh: 10, gap: 4 } }, knob: { name: "Knob", def: { dia: 18 } },
  strip: { name: "Touch strip", def: { w: 12, h: 60 } }, speaker: { name: "Speaker", def: { dia: 34 } }
};
const dzId = () => "x" + Math.random().toString(36).slice(2, 8);
function dzBox(p) {
  if (p.type === "keybed") { const n = DZ_WHITES[p.keys] || 15; return { w: n * p.kp, h: +p.depth > 0 ? +p.depth : Math.round(Math.min(120, p.kp * 4.4)) }; }
  if (p.type === "pads") return { w: p.cols * p.size + (p.cols - 1) * p.gap, h: p.rows * p.size + (p.rows - 1) * p.gap };
  if (p.type === "screen") { const s = DZ_SCREENS[p.diag] || DZ_SCREENS["3.5"]; return { w: s.w + 10, h: s.h + 8 }; }
  if (p.type === "encoders") { const a = (p.n - 1) * p.pitch + 14; return p.vertical ? { w: 14, h: a } : { w: a, h: 14 }; }
  if (p.type === "buttons") return { w: p.cols * p.bw + (p.cols - 1) * p.gap, h: p.rows * p.bh + (p.rows - 1) * p.gap };
  if (p.type === "knob" || p.type === "speaker") return { w: p.dia, h: p.dia };
  return { w: p.w, h: p.h };
}
const dzHeight = { keybed: 9, pads: 3, screen: 2, encoders: 16, buttons: 3, knob: 18, strip: 1, speaker: 1 };   /* how far each rises above the deck */
function dzPart(type, x, y, over) { return Object.assign({ id: dzId(), type, x, y }, JSON.parse(JSON.stringify(DZ_TYPES[type].def)), over || {}); }
function dzTemplate(k, kp) {
  kp = kp || 18; const m = 12, parts = [], sc = dzBox({ type: "screen", diag: "3.5" }), keyW = 15 * kp, kd = Math.round(kp * 4.4), P = (t, x, y, o) => { const p = dzPart(t, x, y, o); parts.push(p); return p; };
  let W, ky;
  if (k === "butterfly") {
    const wing = 4 * 26 + 12, band = Math.max(sc.h, 56); W = Math.max(keyW, 2 * wing + sc.w + 20); const x0 = m + (W - (2 * wing + sc.w + 20)) / 2;
    P("pads", x0, m + (band - 56) / 2, { cols: 4, rows: 2 }); P("screen", x0 + wing + 10, m + (band - sc.h) / 2); P("pads", x0 + wing + sc.w + 20, m + (band - 56) / 2, { cols: 4, rows: 2 });
    P("encoders", m + W / 2 - 46, m + band + 8, { pitch: 26 }); P("buttons", m, m + band + 10); P("buttons", m + W - 92, m + band + 10); ky = m + band + 30;
  } else if (k === "terrace") {
    const block = 8 * 26 + 28; W = Math.max(keyW, block + 12 + sc.w);
    P("pads", m, m + 2); P("screen", m + W - sc.w, m); P("encoders", m + W - sc.w - 4, m + sc.h + 8, { pitch: 26 }); P("buttons", m, m + sc.h + 10, { cols: 12 }); ky = m + sc.h + 30;
  } else if (k === "rails") {
    W = Math.max(keyW + 2 * (26 + 10), sc.w + 2 * 36 + 80); const railH = 8 * 22 + 7 * 4;
    P("pads", m, m, { cols: 1, rows: 8, size: 22 }); P("pads", m + W - 22, m, { cols: 1, rows: 8, size: 22 });
    P("screen", m + (W - sc.w) / 2, m); P("encoders", m + (W - 92) / 2, m + sc.h + 8); P("buttons", m + (W - 188) / 2, m + sc.h + 32, { cols: 12 });
    ky = m + railH - kd; if (ky < m + sc.h + 52) ky = m + sc.h + 52; P("keybed", m + (W - keyW) / 2, ky, { kp }); return { parts, auto: true };
  } else {
    W = Math.max(keyW, sc.w + 4 * 26 + 4 * 16 + 24); const pw = Math.floor((W - 15 * 4) / 16 * 10) / 10;
    P("screen", m + (W - sc.w) / 2, m); P("encoders", m + (W - sc.w) / 2 - 48, m + sc.h / 2 - 7, { n: 2 }); P("encoders", m + (W + sc.w) / 2 + 8, m + sc.h / 2 - 7, { n: 2 });
    P("buttons", m, m + 5, { cols: 2, rows: 3, gap: 6 }); P("buttons", m + W - 30, m + 5, { cols: 2, rows: 3, gap: 6 });
    P("pads", m, m + sc.h + 8, { cols: 16, rows: 1, size: pw }); ky = m + sc.h + 8 + pw + 8;
  }
  P("keybed", m + (W - keyW) / 2, ky, { kp }); return { parts, auto: true };
}
const DZ_CONCEPTS = { steprow: "Step Row", butterfly: "Butterfly", terrace: "Terrace", rails: "Rails" };
function dzNew(concept, name) { const t = dzTemplate(concept), st = DZ_STYLES.clean; const d = { id: dzId(), name: name || DZ_CONCEPTS[concept], concept, style: "clean", body: { w: 0, d: 0, hf: 22, hr: 44, margin: 12, auto: true }, colors: { body: st.body, pad: st.pad, accent: st.accent, darkKeys: false }, mat: Object.assign({}, st.mat), vibe: "", notes: "", parts: t.parts }; dzFit(d); return d; }
function dzFit(d) { if (!d.body.auto) return; let mx = 0, my = 0, nx = 1e9, ny = 1e9; d.parts.forEach(p => { const b = dzBox(p); nx = Math.min(nx, p.x); ny = Math.min(ny, p.y); mx = Math.max(mx, p.x + b.w); my = Math.max(my, p.y + b.h); }); if (!d.parts.length) { d.body.w = 200; d.body.d = 120; return; } const m = +d.body.margin || 12, dx = m - nx, dy = m - ny; d.parts.forEach(p => { p.x = Math.round((p.x + dx) * 10) / 10; p.y = Math.round((p.y + dy) * 10) / 10; }); d.body.w = Math.round(mx - nx + 2 * m); d.body.d = Math.round(my - ny + 2 * m); }
const dzAll = () => { if (!Array.isArray(P.designs) || !P.designs.length) P.designs = [dzNew("steprow", "Step Row A")]; return P.designs; };
const dzCur = () => { const L = dzAll(); DZ.cur = Math.max(0, Math.min(DZ.cur, L.length - 1)); return L[DZ.cur]; };
function dzSpecs(d) {
  const cnt = { keys: 0, pads: 0, encoders: 0, buttons: 0, knobs: 0, screens: 0 }; let kp = null, padMin = null, scr = null;
  d.parts.forEach(p => { if (p.type === "keybed") { cnt.keys += +p.keys; kp = kp == null ? p.kp : Math.min(kp, p.kp); } if (p.type === "pads") { cnt.pads += p.cols * p.rows; padMin = padMin == null ? p.size : Math.min(padMin, p.size); } if (p.type === "encoders") cnt.encoders += +p.n; if (p.type === "buttons") cnt.buttons += p.cols * p.rows; if (p.type === "knob") cnt.knobs++; if (p.type === "screen") { cnt.screens++; scr = p.diag; } });
  const warn = [], bx = d.parts.map(p => ({ p, b: dzBox(p) }));
  bx.forEach((a, i) => { if (a.p.x < 0 || a.p.y < 0 || a.p.x + a.b.w > d.body.w || a.p.y + a.b.h > d.body.d) warn.push(DZ_TYPES[a.p.type].name + " hangs off the body"); for (let j = i + 1; j < bx.length; j++) { const c = bx[j]; if (a.p.x < c.p.x + c.b.w - 0.5 && c.p.x < a.p.x + a.b.w - 0.5 && a.p.y < c.p.y + c.b.h - 0.5 && c.p.y < a.p.y + a.b.h - 0.5) warn.push(DZ_TYPES[a.p.type].name + " overlaps " + DZ_TYPES[c.p.type].name.toLowerCase()); } });
  const tilt = Math.atan2((+d.body.hr || 0) - (+d.body.hf || 0), d.body.d) * 180 / Math.PI, scan = cnt.keys * 2 + cnt.buttons + cnt.encoders * 3;
  return { cnt, kp, padMin, scr, warn: [...new Set(warn)], tilt, scan, area: d.body.w * d.body.d / 100 };
}
/* ---------- drawing ---------- */
function dzTint(hex, k) { const n = parseInt(hex.slice(1), 16), c = [(n >> 16) & 255, (n >> 8) & 255, n & 255].map(v => Math.max(0, Math.min(255, Math.round(v + k)))); return "#" + c.map(v => v.toString(16).padStart(2, "0")).join(""); }
function dzPartSvg(p, d, sel) {
  const b = dzBox(p), st = d.style, C = d.colors, ink = "#0c0a1c"; let s = "";
  const rnd = i => { const v = Math.sin((i + 1) * 12.9898 + p.x * 0.37 + p.y * 0.11) * 43758.5453; return v - Math.floor(v); };
  if (p.type === "keybed") {
    const n = DZ_WHITES[p.keys] || 15, wk = C.darkKeys ? "#22203a" : "#ffffff", bk = C.darkKeys ? "#f3f1fb" : ink;
    for (let i = 0; i < n; i++) s += '<rect x="' + (p.x + i * p.kp).toFixed(1) + '" y="' + p.y + '" width="' + p.kp + '" height="' + b.h + '" rx="1.5" fill="' + (st === "junk" && rnd(i) > 0.86 ? dzTint(wk, -26) : wk) + '" stroke="' + ink + '" stroke-width=".5"/>';
    const start = p.keys == 32 ? 3 : 0; for (let i = 0; i < n - 1; i++) if ([0, 1, 3, 4, 5].includes((i + start) % 7)) s += '<rect x="' + (p.x + (i + 1) * p.kp - p.kp * 0.3).toFixed(1) + '" y="' + p.y + '" width="' + (p.kp * 0.6).toFixed(1) + '" height="' + (b.h * 0.62).toFixed(1) + '" rx="1" fill="' + bk + '"/>';
  } else if (p.type === "pads") {
    for (let r = 0; r < p.rows; r++) for (let c = 0; c < p.cols; c++) { const i = r * p.cols + c, col = st === "junk" ? [C.pad, "#ffb300", "#3fbf6f", "#00b8d9", "#b06cff"][Math.floor(rnd(i) * 5)] : C.pad; s += '<rect x="' + (p.x + c * (p.size + p.gap)).toFixed(1) + '" y="' + (p.y + r * (p.size + p.gap)).toFixed(1) + '" width="' + p.size + '" height="' + p.size + '" rx="' + (st === "industrial" ? 1 : 3) + '" fill="' + col + '"/>'; }
  } else if (p.type === "screen") {
    s += '<rect x="' + p.x + '" y="' + p.y + '" width="' + b.w + '" height="' + b.h + '" rx="3" fill="' + ink + '"/><rect x="' + (p.x + 5) + '" y="' + (p.y + 4) + '" width="' + (b.w - 10) + '" height="' + (b.h - 8) + '" rx="1" fill="' + C.accent + '"/>' + [0.25, 0.5, 0.75].map(f => '<rect x="' + (p.x + 9) + '" y="' + (p.y + 4 + (b.h - 8) * f).toFixed(1) + '" width="' + ((b.w - 18) * (1 - f * 0.4)).toFixed(1) + '" height="1.6" fill="#ffffff" opacity=".55"/>').join("");
  } else if (p.type === "encoders") {
    for (let i = 0; i < p.n; i++) { const cx = p.x + 7 + (p.vertical ? 0 : i * p.pitch), cy = p.y + 7 + (p.vertical ? i * p.pitch : 0); s += '<circle cx="' + cx + '" cy="' + cy + '" r="7" fill="' + (st === "industrial" ? "#8d909a" : st === "junk" ? ["#22203a", "#c33", "#eee", "#d9a400"][Math.floor(rnd(i) * 4)] : "#ffffff") + '" stroke="' + ink + '" stroke-width="1"/><line x1="' + cx + '" y1="' + cy + '" x2="' + cx + '" y2="' + (cy - 5.5) + '" stroke="' + ink + '" stroke-width="1.2"/>'; }
  } else if (p.type === "buttons") {
    for (let r = 0; r < p.rows; r++) for (let c = 0; c < p.cols; c++) s += '<rect x="' + (p.x + c * (p.bw + p.gap)) + '" y="' + (p.y + r * (p.bh + p.gap)) + '" width="' + p.bw + '" height="' + p.bh + '" rx="2" fill="' + (st === "junk" && rnd(r * 9 + c) > 0.7 ? "#22203a" : "#b9b3d6") + '" stroke="' + ink + '" stroke-width=".4"/>';
  } else if (p.type === "knob") { s += '<circle cx="' + (p.x + p.dia / 2) + '" cy="' + (p.y + p.dia / 2) + '" r="' + p.dia / 2 + '" fill="' + ink + '"/><line x1="' + (p.x + p.dia / 2) + '" y1="' + (p.y + p.dia / 2) + '" x2="' + (p.x + p.dia / 2) + '" y2="' + (p.y + 2) + '" stroke="#c8ff2e" stroke-width="1.6"/>';
  } else if (p.type === "strip") { s += '<rect x="' + p.x + '" y="' + p.y + '" width="' + p.w + '" height="' + p.h + '" rx="' + Math.min(p.w, p.h) / 2 + '" fill="' + ink + '"/><rect x="' + (p.x + p.w / 2 - 0.8) + '" y="' + (p.y + 5) + '" width="1.6" height="' + (p.h - 10) + '" fill="' + C.accent + '"/>';
  } else if (p.type === "speaker") { const cx = p.x + p.dia / 2, cy = p.y + p.dia / 2; s += '<circle cx="' + cx + '" cy="' + cy + '" r="' + p.dia / 2 + '" fill="' + dzTint(C.body, -22) + '" stroke="' + ink + '" stroke-width=".5"/>'; for (let a = -2; a <= 2; a++) for (let c = -2; c <= 2; c++) if (a * a + c * c <= 5) s += '<circle cx="' + (cx + a * p.dia / 7) + '" cy="' + (cy + c * p.dia / 7) + '" r="1.2" fill="' + ink + '"/>'; }
  return '<g data-pid="' + p.id + '" style="cursor:move">' + s + '<rect x="' + (p.x - 1.5) + '" y="' + (p.y - 1.5) + '" width="' + (b.w + 3) + '" height="' + (b.h + 3) + '" rx="3" fill="transparent" stroke="' + (sel ? "#6a3dff" : "none") + '" stroke-width="1.2" stroke-dasharray="3 2"/></g>';
}
function dzBodySvg(d) {
  const B = d.body, st = d.style, r = DZ_STYLES[st] ? DZ_STYLES[st].r : 8, C = d.colors, ink = "#0c0a1c"; let s = "";
  if (st === "wood") s += '<rect x="-9" y="-2" width="9" height="' + (B.d + 4) + '" rx="2" fill="#8a5a2b" stroke="' + ink + '" stroke-width=".6"/><rect x="' + B.w + '" y="-2" width="9" height="' + (B.d + 4) + '" rx="2" fill="#8a5a2b" stroke="' + ink + '" stroke-width=".6"/>';
  s += '<rect x="0" y="0" width="' + B.w + '" height="' + B.d + '" rx="' + r + '" fill="' + C.body + '" stroke="' + ink + '" stroke-width="1"/>';
  if (st === "junk") s += '<rect x="0" y="0" width="' + (B.w * 0.38).toFixed(0) + '" height="' + (B.d * 0.42).toFixed(0) + '" rx="' + r + '" fill="' + dzTint(C.body, -24) + '" stroke="' + ink + '" stroke-width=".5"/><rect x="' + (B.w * 0.7).toFixed(0) + '" y="' + (B.d * 0.3).toFixed(0) + '" width="' + (B.w * 0.3).toFixed(0) + '" height="' + (B.d * 0.7).toFixed(0) + '" rx="' + r + '" fill="' + dzTint(C.body, 18) + '" stroke="' + ink + '" stroke-width=".5"/><rect x="' + (B.w * 0.42).toFixed(0) + '" y="3" width="34" height="7" fill="#fff" stroke="' + ink + '" stroke-width=".3"/><text x="' + (B.w * 0.42 + 17).toFixed(0) + '" y="8.4" text-anchor="middle" font-size="4.6" font-family="monospace" fill="' + ink + '">SFA-01</text>';
  if (st === "industrial") s += '<line x1="0" y1="' + (B.d * 0.5).toFixed(0) + '" x2="' + B.w + '" y2="' + (B.d * 0.5).toFixed(0) + '" stroke="' + ink + '" stroke-width=".3" opacity=".5"/><path d="M4 ' + (B.d - 4) + ' h26 v-4 h-26 z" fill="#ffb300"/><path d="M8 ' + (B.d - 4) + ' l4 -4 M16 ' + (B.d - 4) + ' l4 -4 M24 ' + (B.d - 4) + ' l4 -4" stroke="' + ink + '" stroke-width="1.6"/>';
  if (st === "industrial" || st === "junk") [[5, 5], [B.w - 5, 5], [5, B.d - 5], [B.w - 5, B.d - 5], [B.w / 2, 5], [B.w / 2, B.d - 5]].forEach(([x, y], i) => { if (st === "junk" && i === 4) return; s += '<circle cx="' + x + '" cy="' + y + '" r="2" fill="#d9d9e2" stroke="' + ink + '" stroke-width=".4"/><line x1="' + (x - 1.3) + '" y1="' + (y - (i % 2 ? 1.3 : 0)) + '" x2="' + (x + 1.3) + '" y2="' + (y + (i % 2 ? 1.3 : 0)) + '" stroke="' + ink + '" stroke-width=".5"/>'; });
  return s;
}
function dzTop(d, opts) {
  opts = opts || {}; const B = d.body, pad = opts.pad == null ? 22 : opts.pad, mmAttr = opts.mm ? ' width="' + (B.w + 2 * pad) + 'mm" height="' + (B.d + 2 * pad) + 'mm" xmlns="http://www.w3.org/2000/svg"' : ' id="' + (opts.id || "") + '" role="img" aria-label="' + esc(d.name) + ', top view"';
  let s = '<svg viewBox="' + (-pad) + " " + (-pad) + " " + (B.w + 2 * pad) + " " + (B.d + 2 * pad) + '"' + mmAttr + ' style="width:100%;height:auto;display:block;touch-action:none">';
  if (opts.grid) { s += '<g stroke="#beb6dd" stroke-width=".15" opacity=".7">'; for (let x = 0; x <= B.w; x += 10) s += '<line x1="' + x + '" y1="0" x2="' + x + '" y2="' + B.d + '"/>'; for (let y = 0; y <= B.d; y += 10) s += '<line x1="0" y1="' + y + '" x2="' + B.w + '" y2="' + y + '"/>'; s += "</g>"; }
  s += dzBodySvg(d) + d.parts.map(p => dzPartSvg(p, d, opts.sel === p.id)).join("");
  if (opts.dims !== false) s += '<g fill="none" stroke="#5d5880" stroke-width=".4"><path d="M0 ' + (B.d + 9) + " H" + B.w + " M0 " + (B.d + 6) + " v6 M" + B.w + " " + (B.d + 6) + ' v6"/><path d="M' + (B.w + 12) + " 0 V" + B.d + " M" + (B.w + 9) + " 0 h6 M" + (B.w + 9) + " " + B.d + ' h6"/></g><text x="' + B.w / 2 + '" y="' + (B.d + 17) + '" text-anchor="middle" font-size="6" font-family="monospace" fill="#5d5880">' + B.w + ' mm</text><text x="' + (B.w + 18) + '" y="' + B.d / 2 + '" text-anchor="middle" font-size="6" font-family="monospace" fill="#5d5880" transform="rotate(90 ' + (B.w + 18) + " " + B.d / 2 + ')">' + B.d + " mm</text>";
  return s + "</svg>";
}
function dzElev(d, side) {            /* front: looking from the player; side: profile, front edge on the left */
  const B = d.body, hf = +B.hf || 20, hr = +B.hr || hf, L = side ? B.d : B.w, top = Math.max(hf, hr) + 26, ink = "#0c0a1c", C = d.colors; let s = '<svg viewBox="-14 ' + (-top) + " " + (L + 28) + " " + (top + 18) + '" role="img" aria-label="' + esc(d.name) + (side ? ", side view" : ", front view") + '" style="width:100%;height:auto;display:block">';
  const deck = t => side ? hf + (hr - hf) * (1 - t / B.d) : hf;                   /* deck height at depth y (side) */
  if (side) s += '<path d="M0 0 V' + (-hf) + " L" + L + " " + (-hr) + ' V0 Z" fill="' + C.body + '" stroke="' + ink + '" stroke-width="1"/>';
  else s += '<rect x="0" y="' + (-hr) + '" width="' + L + '" height="' + hr + '" fill="' + dzTint(C.body, -14) + '" stroke="' + ink + '" stroke-width=".6"/><rect x="0" y="' + (-hf) + '" width="' + L + '" height="' + hf + '" rx="2" fill="' + C.body + '" stroke="' + ink + '" stroke-width="1"/>';
  d.parts.slice().sort((a, b) => a.y - b.y).forEach(p => { const b = dzBox(p), h = dzHeight[p.type] || 2, col = p.type === "pads" ? C.pad : p.type === "screen" ? ink : p.type === "keybed" ? (C.darkKeys ? "#22203a" : "#fff") : p.type === "encoders" || p.type === "knob" ? "#8d909a" : "#b9b3d6";
    if (side) { const x0 = B.d - p.y - b.h, y0 = deck(p.y + b.h), y1 = deck(p.y); s += '<path d="M' + x0 + " " + (-y0) + " L" + (x0 + b.w * 0 + b.h) + " " + (-y1) + " v" + (-h) + " L" + x0 + " " + (-y0 - h) + ' Z" fill="' + col + '" stroke="' + ink + '" stroke-width=".4"/>'; }
    else { const base = side ? 0 : (p.y < B.d * 0.5 ? hr : hf); if (p.type === "encoders") for (let i = 0; i < p.n; i++) { const x = p.x + (p.vertical ? 0 : i * p.pitch); s += '<rect x="' + (x + 1) + '" y="' + (-base - h) + '" width="12" height="' + h + '" rx="1.5" fill="' + col + '" stroke="' + ink + '" stroke-width=".4"/>'; } else s += '<rect x="' + p.x + '" y="' + (-base - h) + '" width="' + b.w + '" height="' + h + '" rx="1" fill="' + col + '" stroke="' + ink + '" stroke-width=".4"/>'; } });
  if (d.style === "wood" && !side) s += '<rect x="-9" y="' + (-hr - 2) + '" width="9" height="' + (hr + 2) + '" fill="#8a5a2b" stroke="' + ink + '" stroke-width=".5"/><rect x="' + L + '" y="' + (-hr - 2) + '" width="9" height="' + (hr + 2) + '" fill="#8a5a2b" stroke="' + ink + '" stroke-width=".5"/>';
  s += '<line x1="-14" y1="0" x2="' + (L + 14) + '" y2="0" stroke="#5d5880" stroke-width=".5"/><text x="' + L / 2 + '" y="11" text-anchor="middle" font-size="6" font-family="monospace" fill="#5d5880">' + (side ? "depth " + B.d + " mm · front " + hf + " mm, rear " + hr + " mm" : "width " + B.w + " mm · " + (hr > hf ? "rear rises to " + hr + " mm" : "height " + hf + " mm")) + "</text>" + (side ? '<text x="2" y="' + (-hf - 4) + '" font-size="5" font-family="monospace" fill="#5d5880">front</text><text x="' + (L - 2) + '" y="' + (-hr - 20) + '" text-anchor="end" font-size="5" font-family="monospace" fill="#5d5880">rear</text>' : "");
  return s + "</svg>";
}
/* ---------- page ---------- */
WS.designer = WS.layouts = async function () {
  $("#main").innerHTML = '<div class="row"><h1 style="margin:0 12px 0 0">Prototype Designer</h1>' + saveBar + '</div><p class="lede">Shape the whole instrument. Drag the keys, pads, screen, encoders and buttons; the size and the numbers follow. Everything is to scale in millimetres. Save designs and compare them.</p><div id="dzBox">Loading...</div>';
  await loadAll(); dzAll(); DZ.hist = []; DZ.redo = []; dzDraw();
};
function dzCommit(noHist) { const d = dzCur(); dzFit(d); if (!noHist) { DZ.hist.push(JSON.stringify(d)); if (DZ.hist.length > 60) DZ.hist.shift(); DZ.redo = []; } psave(); dzDraw(); }
function dzSnap() { DZ.hist.push(JSON.stringify(dzCur())); if (DZ.hist.length > 60) DZ.hist.shift(); DZ.redo = []; }
function dzUndo(dir) { const L = dzAll(), from = dir < 0 ? DZ.hist : DZ.redo, to = dir < 0 ? DZ.redo : DZ.hist; if (!from.length) return; to.push(JSON.stringify(L[DZ.cur])); L[DZ.cur] = JSON.parse(from.pop()); DZ.sel = null; psave(); dzDraw(); }
function dzDraw() {
  const L = dzAll(), d = dzCur(), S = dzSpecs(d), fw = +pget(["layout", "fm1w"], 0), fd = +pget(["layout", "fm1d"], 0), fk = +pget(["layout", "fm1kp"], 0), sel = d.parts.find(p => p.id === DZ.sel);
  let h = '<div class="row"><select id="dzSel" aria-label="Design">' + L.map((x, i) => '<option value="' + i + '"' + (i === DZ.cur ? " selected" : "") + ">" + esc(x.name) + "</option>").join("") + '</select><select id="dzNewSel" aria-label="New design from a concept"><option value="">New from concept…</option>' + Object.keys(DZ_CONCEPTS).map(k => '<option value="' + k + '">' + DZ_CONCEPTS[k] + "</option>").join("") + '</select><button id="dzDup">Duplicate</button><button id="dzDel" class="danger">Delete</button><span class="sp"></span>' + tabsBar("data-dzt", [["design", "Design"], ["look", "Materials and vibe"], ["compare", "Compare"]], DZ.tab).replace('class="sub"', 'class="sub" style="margin:0"') + "</div>";
  const kp = (l, v, c) => kpi(l, v, c);
  const specs = '<div class="kpis" style="margin:16px 0 0">' + kp("Body", d.body.w + " x " + d.body.d + " mm") + kp("Height", (+d.body.hf) + " → " + (+d.body.hr) + " mm · " + S.tilt.toFixed(1) + "°") + kp("Footprint", S.area.toFixed(0) + " cm²" + (fw && fd ? " · " + (d.body.w * d.body.d / (fw * fd)).toFixed(1) + "x FM-1" : "")) + kp("Key pitch", S.kp ? S.kp + " mm" + (fk ? " · " + (S.kp / fk * 100 - 100 >= 0 ? "+" : "") + (S.kp / fk * 100 - 100).toFixed(0) + "%" : "") : "no keys", S.kp >= 17 ? "q" : "est") + kp("Smallest pad", S.padMin ? S.padMin + " mm" : "no pads", S.padMin >= 22 ? "q" : "est") + kp("Controls", S.cnt.keys + " keys · " + S.cnt.pads + " pads · " + S.cnt.encoders + " enc · " + S.cnt.buttons + " btn") + "</div>" + (S.warn.length ? '<div class="note bad" style="margin-top:12px"><b>Check:</b> ' + S.warn.map(esc).join("; ") + ".</div>" : "");
  if (DZ.tab === "design") {
    const insp = () => { if (!sel) return '<div class="hint">Click a part to edit it. Drag to move (1 mm steps; hold Shift for 5 mm). Arrow keys nudge the selected part. Ctrl+Z undoes.</div>'; const T = DZ_TYPES[sel.type], b = dzBox(sel), num = (k, l, step) => '<label class="fld"><span>' + l + '</span><input type="number" data-pp="' + k + '" value="' + sel[k] + '" step="' + (step || 1) + '"></label>';
      let f = num("x", "Left (mm)") + num("y", "From rear (mm)");
      if (sel.type === "keybed") f += '<label class="fld"><span>Keys</span><select data-pp="keys">' + Object.keys(DZ_WHITES).map(k => "<option" + (k == sel.keys ? " selected" : "") + ">" + k + "</option>").join("") + "</select></label>" + num("kp", "Key pitch (mm)", 0.5) + num("depth", "Key length (0 = auto)");
      if (sel.type === "pads") f += num("cols", "Columns") + num("rows", "Rows") + num("size", "Pad size (mm)", 0.5) + num("gap", "Gap (mm)", 0.5);
      if (sel.type === "screen") f += '<label class="fld"><span>Diagonal (inch)</span><select data-pp="diag">' + Object.keys(DZ_SCREENS).map(k => "<option" + (k === sel.diag ? " selected" : "") + ">" + k + "</option>").join("") + "</select></label>";
      if (sel.type === "encoders") f += num("n", "Count") + num("pitch", "Spacing (mm)") + '<label class="fld"><span>Direction</span><select data-pp="vertical"><option value="0"' + (+sel.vertical ? "" : " selected") + '>row</option><option value="1"' + (+sel.vertical ? " selected" : "") + ">column</option></select></label>";
      if (sel.type === "buttons") f += num("cols", "Columns") + num("rows", "Rows") + num("bw", "Width (mm)") + num("bh", "Depth (mm)") + num("gap", "Gap (mm)");
      if (sel.type === "knob" || sel.type === "speaker") f += num("dia", "Diameter (mm)");
      if (sel.type === "strip") f += num("w", "Width (mm)") + num("h", "Length (mm)");
      return '<div class="row" style="margin:0 0 8px"><b style="font-size:15px">' + T.name + '</b><span class="hint mono">' + b.w.toFixed(0) + " x " + b.h.toFixed(0) + ' mm</span><span class="sp"></span><button class="sm" id="dzCx">Centre</button><button class="sm" id="dzCopy">Copy</button><button class="sm danger" id="dzRm">Remove</button></div><div class="grid2" style="grid-template-columns:repeat(auto-fit,minmax(120px,1fr));margin:0">' + f + "</div>"; };
    h += '<div class="dz"><div><div class="row" style="margin:14px 0 8px">' + tabsBar("data-dzv", [["top", "Top"], ["front", "Front"], ["side", "Side"]], DZ.view).replace('class="sub"', 'class="sub" style="margin:0"') + '<span class="sp"></span><button class="sm" id="dzU" ' + (DZ.hist.length ? "" : "disabled") + '>Undo</button><button class="sm" id="dzR" ' + (DZ.redo.length ? "" : "disabled") + ">Redo</button></div>" +
      '<div class="card dzstage">' + (DZ.view === "top" ? dzTop(d, { sel: DZ.sel, grid: true, id: "dzSvg" }) : dzElev(d, DZ.view === "side")) + "</div>" + (DZ.view !== "top" ? '<div class="hint" style="margin-top:8px">A rough ' + DZ.view + " view from part heights and the body's front and rear height. Switch to Top to move things.</div>" : "") + specs + "</div>" +
      '<div><div class="card ed" style="margin-top:14px"><label class="fld"><span>Design name</span><input id="dzName" value="' + esc(d.name) + '" maxlength="40"></label><div class="grid2" style="grid-template-columns:repeat(auto-fit,minmax(110px,1fr));margin:0">' + [["hf", "Front height"], ["hr", "Rear height"], ["margin", "Edge margin"]].map(x => '<label class="fld"><span>' + x[1] + ' (mm)</span><input type="number" data-bb="' + x[0] + '" value="' + d.body[x[0]] + '"></label>').join("") + '</div><label style="display:flex;gap:8px;align-items:center"><input type="checkbox" id="dzAuto"' + (d.body.auto ? " checked" : "") + "> Body follows the parts</label>" + (d.body.auto ? "" : '<div class="grid2" style="grid-template-columns:1fr 1fr;margin:0"><label class="fld"><span>Width (mm)</span><input type="number" data-bb="w" value="' + d.body.w + '"></label><label class="fld"><span>Depth (mm)</span><input type="number" data-bb="d" value="' + d.body.d + '"></label></div>') + "</div>" +
      '<div class="card ed" style="margin-top:14px">' + insp() + "</div>" +
      '<div class="card" style="margin-top:14px"><b>Add a part</b><div class="chips">' + Object.keys(DZ_TYPES).map(t => '<button class="sm" data-add="' + t + '">' + DZ_TYPES[t].name + "</button>").join("") + "</div></div>" +
      '<div class="card ed" style="margin-top:14px"><b>Your FM-1, for comparison</b><div class="grid2" style="grid-template-columns:repeat(3,1fr);margin:0">' + [["fm1w", "Width"], ["fm1d", "Depth"], ["fm1kp", "Key width"]].map(x => '<label class="fld"><span>' + x[1] + ' (mm)</span><input type="number" data-fm="' + x[0] + '" value="' + esc(pget(["layout", x[0]], "")) + '" step="0.5"></label>').join("") + '</div><div class="hint">Measure it with a ruler. Blank until you do: there are no FM-1 dimensions on file.</div></div>' +
      '<div class="card" style="margin-top:14px"><b>Take it further</b><div class="actions"><button class="tool" id="dzPrint">Print at 1:1</button><button id="dzSvgDl">SVG</button><button id="dzJson">JSON</button><button id="dzPlan">Use in Planner</button><button id="dzDec">Propose in Decisions</button><a href="#agents"><button>Ask the agents</button></a></div><span class="hint" id="dzMsg"></span></div></div></div>';
  } else if (DZ.tab === "look") {
    const ST = DZ_STYLES[d.style] || DZ_STYLES.clean;
    h += '<div class="dz"><div><div class="card dzstage" style="margin-top:14px">' + dzTop(d, { dims: false, pad: 14 }) + '</div><div class="card dzstage" style="margin-top:14px">' + dzElev(d, false) + "</div></div><div>" +
      '<div class="card" style="margin-top:14px"><b>Style</b>' + Object.keys(DZ_STYLES).map(k => '<label class="stylepick' + (k === d.style ? " on" : "") + '"><input type="radio" name="dzStyle" value="' + k + '"' + (k === d.style ? " checked" : "") + "><span><b>" + DZ_STYLES[k].name + "</b><small>" + DZ_STYLES[k].say + "</small></span></label>").join("") + '<div class="hint">Picking a style sets its colours and suggested materials. Change anything afterwards.</div></div>' +
      '<div class="card ed" style="margin-top:14px"><b>Colours</b><div class="row" style="margin:0">' + [["body", "Body"], ["pad", "Pads"], ["accent", "Screen / accent"]].map(x => '<label class="hint" style="display:flex;gap:6px;align-items:center"><input type="color" data-cc="' + x[0] + '" value="' + d.colors[x[0]] + '" style="width:38px;height:30px;padding:2px">' + x[1] + "</label>").join("") + '<label class="hint" style="display:flex;gap:6px;align-items:center"><input type="checkbox" id="dzDark"' + (d.colors.darkKeys ? " checked" : "") + "> dark keys</label></div></div>" +
      '<div class="card ed" style="margin-top:14px"><b>Materials</b><div class="grid2" style="grid-template-columns:1fr;margin:0">' + [["case", "Case"], ["top", "Top panel"], ["keys", "Keys"], ["pads", "Pads"], ["knobs", "Knobs"], ["finish", "Finish and legends"]].map(x => '<label class="fld"><span>' + x[1] + '</span><input data-mm="' + x[0] + '" value="' + esc((d.mat || {})[x[0]] || "") + '" placeholder="' + esc(ST.mat[x[0]]) + '"></label>').join("") + '</div><label class="fld"><span>Vibe: what should it feel like in the hands and on a table?</span><textarea id="dzVibe" rows="3" placeholder="e.g. looks like it was built in a workshop, not styled in a studio; survives a backpack">' + esc(d.vibe || "") + '</textarea></label><label class="fld"><span>Notes</span><textarea id="dzNotes" rows="3">' + esc(d.notes || "") + "</textarea></label></div></div></div>" + specs;
  } else {
    if (L.length < 2) h += '<div class="note" style="margin-top:14px">Save a second design to compare: use Duplicate or New from concept.</div>';
    else { DZ.cmpA = Math.min(DZ.cmpA, L.length - 1); DZ.cmpB = Math.min(DZ.cmpB, L.length - 1); const A = L[DZ.cmpA], Bd = L[DZ.cmpB], sa = dzSpecs(A), sb = dzSpecs(Bd), pick = (id, v) => '<select id="' + id + '">' + L.map((x, i) => '<option value="' + i + '"' + (i === v ? " selected" : "") + ">" + esc(x.name) + "</option>").join("") + "</select>";
      const rows = [["Width", A.body.w, Bd.body.w, "mm"], ["Depth", A.body.d, Bd.body.d, "mm"], ["Footprint", sa.area, sb.area, "cm²"], ["Rear height", +A.body.hr, +Bd.body.hr, "mm"], ["Tilt", sa.tilt, sb.tilt, "°"], ["Key pitch", sa.kp || 0, sb.kp || 0, "mm"], ["Keys", sa.cnt.keys, sb.cnt.keys, ""], ["Pads", sa.cnt.pads, sb.cnt.pads, ""], ["Smallest pad", sa.padMin || 0, sb.padMin || 0, "mm"], ["Encoders", sa.cnt.encoders, sb.cnt.encoders, ""], ["Buttons", sa.cnt.buttons, sb.cnt.buttons, ""], ["Switch contacts to scan", sa.scan, sb.scan, ""], ["Layout warnings", sa.warn.length, sb.warn.length, ""]];
      h += '<div class="cards" style="margin-top:14px"><div class="card"><div class="row" style="margin:0">' + pick("dzA", DZ.cmpA) + '<span class="hint">' + esc((DZ_STYLES[A.style] || {}).name || "") + "</span></div>" + dzTop(A, { pad: 22 }) + '</div><div class="card"><div class="row" style="margin:0">' + pick("dzB", DZ.cmpB) + '<span class="hint">' + esc((DZ_STYLES[Bd.style] || {}).name || "") + "</span></div>" + dzTop(Bd, { pad: 22 }) + "</div></div>" +
        '<div class="card tw" style="padding:0;margin-top:16px"><table><tr><th></th><th>' + esc(A.name) + "</th><th>" + esc(Bd.name) + "</th><th>Difference</th></tr>" + rows.map(r => { const dv = r[2] - r[1], f = v => (Math.round(v * 10) / 10) + (r[3] ? " " + r[3] : ""); return "<tr><td>" + r[0] + "</td><td>" + f(r[1]) + "</td><td>" + f(r[2]) + '</td><td style="color:var(--' + (dv === 0 ? "dim" : "fg") + ')">' + (dv === 0 ? "same" : (dv > 0 ? "+" : "") + f(dv)) + "</td></tr>"; }).join("") + '<tr><td>Materials</td><td style="white-space:normal;text-align:right">' + esc((A.mat || {}).case || "") + '</td><td style="white-space:normal;text-align:right">' + esc((Bd.mat || {}).case || "") + "</td><td></td></tr></table></div>"; }
  }
  const box = $("#dzBox"); box.innerHTML = h; dzWire(box, d, sel);
}
function dzWire(box, d, sel) {
  const L = dzAll(), on = (s, ev, fn) => { const e = $(s); if (e) e[ev] = fn; };
  on("#dzSel", "onchange", e => { DZ.cur = +e.target.value; DZ.sel = null; DZ.hist = []; DZ.redo = []; dzDraw(); });
  on("#dzNewSel", "onchange", e => { if (!e.target.value) return; const n = dzNew(e.target.value, DZ_CONCEPTS[e.target.value] + " " + String.fromCharCode(65 + L.filter(x => x.concept === e.target.value).length)); L.push(n); DZ.cur = L.length - 1; DZ.sel = null; DZ.hist = []; DZ.redo = []; psave(); dzDraw(); });
  on("#dzDup", "onclick", () => { const c = JSON.parse(JSON.stringify(d)); c.id = dzId(); c.name = (d.name + " copy").slice(0, 40); c.parts.forEach(p => p.id = dzId()); L.push(c); DZ.cur = L.length - 1; DZ.sel = null; psave(); dzDraw(); });
  on("#dzDel", "onclick", () => { if (L.length < 2) { alert("Keep at least one design."); return; } if (!confirm("Delete " + d.name + "?")) return; L.splice(DZ.cur, 1); DZ.cur = 0; DZ.sel = null; psave(); dzDraw(); });
  box.querySelectorAll("[data-dzt]").forEach(b => b.onclick = () => { DZ.tab = b.dataset.dzt; dzDraw(); });
  box.querySelectorAll("[data-dzv]").forEach(b => b.onclick = () => { DZ.view = b.dataset.dzv; dzDraw(); });
  on("#dzU", "onclick", () => dzUndo(-1)); on("#dzR", "onclick", () => dzUndo(1));
  on("#dzName", "onchange", e => { d.name = e.target.value.slice(0, 40) || "Design"; psave(); dzDraw(); });
  on("#dzAuto", "onchange", e => { dzSnap(); d.body.auto = e.target.checked; dzCommit(true); });
  box.querySelectorAll("[data-bb]").forEach(el => el.onchange = () => { dzSnap(); d.body[el.dataset.bb] = Math.max(0, Math.min(900, +el.value || 0)); dzCommit(true); });
  box.querySelectorAll("[data-pp]").forEach(el => el.onchange = () => { if (!sel) return; dzSnap(); const k = el.dataset.pp, lim = { kp: [10, 24], size: [8, 60], cols: [1, 16], rows: [1, 8], n: [1, 8], pitch: [16, 60], gap: [0, 20], bw: [6, 30], bh: [6, 30], dia: [8, 80], w: [6, 200], h: [6, 200], depth: [0, 160], x: [-50, 900], y: [-50, 600] }[k]; sel[k] = k === "diag" ? el.value : lim ? Math.max(lim[0], Math.min(lim[1], +el.value || 0)) : +el.value; dzCommit(true); });
  on("#dzCx", "onclick", () => { dzSnap(); sel.x = Math.round((d.body.w - dzBox(sel).w) / 2); dzCommit(true); });
  on("#dzCopy", "onclick", () => { dzSnap(); const c = JSON.parse(JSON.stringify(sel)); c.id = dzId(); c.x += 8; c.y += 8; d.parts.push(c); DZ.sel = c.id; dzCommit(true); });
  on("#dzRm", "onclick", () => { dzSnap(); d.parts = d.parts.filter(p => p.id !== sel.id); DZ.sel = null; dzCommit(true); });
  box.querySelectorAll("[data-add]").forEach(b => b.onclick = () => { dzSnap(); const p = dzPart(b.dataset.add, 14, 14); d.parts.push(p); DZ.sel = p.id; DZ.view = "top"; dzCommit(true); });
  box.querySelectorAll("[name=dzStyle]").forEach(r => r.onchange = () => { const st = DZ_STYLES[r.value]; d.style = r.value; d.colors.body = st.body; d.colors.pad = st.pad; d.colors.accent = st.accent; d.mat = Object.assign({}, st.mat); psave(); dzDraw(); });
  box.querySelectorAll("[data-cc]").forEach(el => el.onchange = () => { d.colors[el.dataset.cc] = el.value; psave(); dzDraw(); });
  on("#dzDark", "onchange", e => { d.colors.darkKeys = e.target.checked; psave(); dzDraw(); });
  box.querySelectorAll("[data-mm]").forEach(el => el.onchange = () => { d.mat = d.mat || {}; d.mat[el.dataset.mm] = el.value.slice(0, 160); psave(); });
  on("#dzVibe", "onchange", e => { d.vibe = e.target.value.slice(0, 1200); psave(); }); on("#dzNotes", "onchange", e => { d.notes = e.target.value.slice(0, 2000); psave(); });
  box.querySelectorAll("[data-fm]").forEach(el => el.onchange = () => { pset(["layout", el.dataset.fm], el.value === "" ? "" : Math.max(0, +el.value || 0)); dzDraw(); });
  on("#dzA", "onchange", e => { DZ.cmpA = +e.target.value; dzDraw(); }); on("#dzB", "onchange", e => { DZ.cmpB = +e.target.value; dzDraw(); });
  const msg = t => { const m = $("#dzMsg"); if (m) m.textContent = t; };
  on("#dzPrint", "onclick", () => { const w = window.open("", "_blank"); if (!w) { msg("Allow pop-ups to print."); return; } w.document.write("<!doctype html><title>" + esc(d.name) + " 1:1</title><style>@page{margin:6mm}body{margin:0;font:10pt sans-serif}svg{width:auto!important}</style><p>" + esc(d.name) + " · " + d.body.w + " x " + d.body.d + " mm · print at 100% (no fit to page). Check with a ruler.</p>" + dzTop(d, { mm: true })); w.document.close(); w.focus(); setTimeout(() => w.print(), 300); });
  on("#dzSvgDl", "onclick", () => dl(d.name.replace(/[^\w-]+/g, "-") + ".svg", dzTop(d, { mm: true }), "image/svg+xml"));
  on("#dzJson", "onclick", () => dl(d.name.replace(/[^\w-]+/g, "-") + ".sfa-design.json", JSON.stringify(Object.assign({ schema: "sfa-design/1" }, d), null, 1)));
  on("#dzPlan", "onclick", () => { const S = dzSpecs(d); pset(["target", "w"], d.body.w); pset(["target", "d"], d.body.d); pset(["target", "h"], +d.body.hr); pset(["target", "controls"], S.cnt.keys + " keys at " + S.kp + " mm, " + S.cnt.pads + " pads, " + S.cnt.encoders + " encoders, " + S.cnt.buttons + " buttons (" + d.name + ")"); if (S.scr) pset(["target", "screen"], S.scr + " in colour IPS, " + DZ_SCREENS[S.scr].px.replace(/ /g, "")); msg("Planner target updated from " + d.name + "."); });
  on("#dzDec", "onclick", () => { const D = P.decisions = P.decisions || [], S = dzSpecs(d); let x = D.find(q => q.id === "f6"); if (!x) { x = { id: "f6", title: "", status: "proposed", why: "", date: "" }; D.push(x); } x.title = "Layout: " + d.name; x.status = "proposed"; x.why = d.body.w + " x " + d.body.d + " mm, " + S.cnt.keys + " keys at " + S.kp + " mm, " + S.cnt.pads + " pads, " + ((DZ_STYLES[d.style] || {}).name || "") + ". From the Designer; print at 1:1 and try it before locking."; psave(); msg("Recorded in Decisions as a proposal."); });
  /* dragging on the top view */
  const svg = $("#dzSvg"); if (!svg) return;
  const pt = e => { const m = svg.getScreenCTM().inverse(), q = new DOMPoint(e.clientX, e.clientY).matrixTransform(m); return [q.x, q.y]; };
  svg.onpointerdown = e => { const g = e.target.closest("[data-pid]"); if (!g) { if (DZ.sel) { DZ.sel = null; dzDraw(); } return; } const p = d.parts.find(x => x.id === g.dataset.pid), [x, y] = pt(e); DZ.drag = { p, dx: x - p.x, dy: y - p.y, moved: false, snap: JSON.stringify(d) }; DZ.sel = p.id; svg.setPointerCapture(e.pointerId); };
  svg.onpointermove = e => { const D = DZ.drag; if (!D) return; const [x, y] = pt(e), st = e.shiftKey ? 5 : 1, nx = Math.round((x - D.dx) / st) * st, ny = Math.round((y - D.dy) / st) * st; if (nx === D.p.x && ny === D.p.y) return; D.p.x = nx; D.p.y = ny; D.moved = true; const g = svg.querySelector('[data-pid="' + D.p.id + '"]'); if (g) g.outerHTML = dzPartSvg(D.p, d, true); };
  svg.onpointerup = svg.onpointercancel = () => { const D = DZ.drag; DZ.drag = null; if (!D) return; if (D.moved) { DZ.hist.push(D.snap); DZ.redo = []; dzFit(d); psave(); } dzDraw(); };
}
document.addEventListener("keydown", e => {
  if (page() !== "designer" && page() !== "layouts") return; if (/INPUT|SELECT|TEXTAREA/.test(e.target.tagName)) return;
  if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "z") { e.preventDefault(); dzUndo(e.shiftKey ? 1 : -1); return; }
  const d = dzCur(), p = d.parts.find(x => x.id === DZ.sel); if (!p || DZ.tab !== "design") return; const st = e.shiftKey ? 5 : 1, mv = { ArrowLeft: [-st, 0], ArrowRight: [st, 0], ArrowUp: [0, -st], ArrowDown: [0, st] }[e.key];
  if (mv) { e.preventDefault(); dzSnap(); p.x += mv[0]; p.y += mv[1]; dzCommit(true); } else if (e.key === "Delete" || e.key === "Backspace") { e.preventDefault(); dzSnap(); d.parts = d.parts.filter(x => x.id !== p.id); DZ.sel = null; dzCommit(true); }
});
if (WS[page()]) route();
