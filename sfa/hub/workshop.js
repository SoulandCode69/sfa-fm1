"use strict";
/* SFA Console: workshop pages added on 2026-11-10. Direction, Layouts, Patterns, Toolbox.
   Uses the console's globals: $, api, esc, kpi, H2, ctl, bindPlan, pget, pset, psave, P, loadAll, saveBar, money. */
window.WS = {};
const tabsBar = (attr, tabs, cur) => '<div class="sub">' + tabs.map(t => "<button " + attr + '="' + t[0] + '" class="' + (t[0] === cur ? "on" : "") + '">' + t[1] + "</button>").join("") + "</div>";
const getJSON = async u => (await api(u)).json();
const dl = (name, text, type) => { const a = document.createElement("a"); a.href = URL.createObjectURL(new Blob([text], { type: type || "application/json" })); a.download = name; a.click(); setTimeout(() => URL.revokeObjectURL(a.href), 2000); };

/* =====================================================================================================
   DIRECTION: the brief for our own box (hardware/prototype/direction.json + parts-research.json)
   ===================================================================================================== */
let DIR = null, PARTS = null, dirTab = "brief";
WS.direction = async function () {
  $("#main").innerHTML = '<div class="row"><h1 style="margin:0 12px 0 0">Prototype Direction</h1>' + saveBar + '</div><p class="lede" id="dirLede">Loading...</p><div id="dirBox"></div>';
  await loadAll();
  try { DIR = DIR || await getJSON("/api/direction"); } catch (e) { $("#dirLede").textContent = "hardware/prototype/direction.json is missing."; return; }
  if (!PARTS) { try { PARTS = await getJSON("/api/parts"); } catch (e) { PARTS = null; } }
  $("#dirLede").textContent = DIR.note; drawDir();
};
function drawDir() {
  const D = DIR, tabs = [["brief", "Brief"], ["controls", "Controls"], ["screen", "Screen"], ["sound", "Engines, sampler, sequencer"], ["io", "I/O and power"], ["parts", "Parts direction"], ["pipeline", "Console → FM-1 → box"], ["open", "Open questions"]];
  let h = tabsBar("data-dt", tabs, dirTab);
  if (dirTab === "brief") {
    h += '<div class="cards" style="grid-template-columns:repeat(auto-fit,minmax(300px,1fr))">' + D.brief.map((b, i) => '<div class="card"><span class="eyebrow">' + String(i + 1).padStart(2, "0") + " · " + esc(b.status) + '</span><h2 style="font-size:17px">' + esc(b.h) + '</h2><div style="color:var(--dim)">' + esc(b.p) + "</div></div>").join("") + "</div>" +
      H2("Improvements over the FM-1") + '<div class="card tw" style="padding:0"><table><tr><th style="text-align:left">FM-1 today</th><th style="text-align:left">Our box</th></tr>' + D.improvements.map(i => '<tr><td style="text-align:left;white-space:normal;color:var(--dim)">' + esc(i.fm1) + '</td><td style="text-align:left;white-space:normal"><b>' + esc(i.box) + "</b></td></tr>").join("") + "</table></div>" +
      '<p class="hint" style="margin-top:14px">Next: pick a layout on the <a href="#layouts">Layouts</a> page, then lock it in <a href="#decisions">Decisions</a>.</p>';
  } else if (dirTab === "controls") {
    h += '<div class="card tw" style="padding:0"><table><tr><th style="text-align:left">Control</th><th style="text-align:left">Count</th><th style="text-align:left">Why</th><th style="text-align:left">Still open</th></tr>' + D.controls.map(c => '<tr><td style="text-align:left"><b>' + esc(c.what) + '</b></td><td style="text-align:left;white-space:normal">' + esc(c.count) + '</td><td style="text-align:left;white-space:normal;min-width:240px;color:var(--dim)">' + esc(c.why) + '</td><td style="text-align:left;white-space:normal;min-width:180px">' + esc(c.open) + "</td></tr>").join("") + "</table></div>" +
      '<div class="note" style="margin-top:14px">Scanning: 25 velocity keys are 50 contacts (an 8 x 7 matrix), 16 force-sensing pads go through one 16-channel analogue mux into one ADC pin, 4 encoders with push are 12 lines, about 12 buttons join the key matrix. That is why the pin count of the brain matters; see Open questions.</div>';
  } else if (dirTab === "screen") {
    h += '<div class="note" style="margin-bottom:16px">' + esc(D.screen.summary) + "</div>" + H2("What it must show") + '<div class="card tw" style="padding:0"><table><tr><th style="text-align:left">View</th><th style="text-align:left">Needs</th><th style="text-align:left">Smallest that works</th></tr>' + D.screen.must_show.map(v => '<tr><td style="text-align:left"><b>' + esc(v.view) + '</b></td><td style="text-align:left;white-space:normal;min-width:260px">' + esc(v.needs) + '</td><td style="text-align:left;white-space:normal">' + esc(v.min) + "</td></tr>").join("") + "</table></div>" +
      H2("Requirements") + '<div class="card"><ul class="clean">' + D.screen.requirements.map(r => "<li>" + esc(r) + "</li>").join("") + "</ul></div>" + H2("Mock: the engine page at 480 x 320") + '<div class="card" style="max-width:520px">' + screenMock() + '<div class="hint">A sketch to check that the content fits, not a design.</div></div>';
  } else if (dirTab === "sound") {
    const col = (t, a) => '<div class="card"><h2 style="font-size:17px">' + t + '</h2><ul class="clean">' + a.map(x => "<li>" + esc(x) + "</li>").join("") + "</ul></div>";
    h += '<div class="cards">' + col("Engines", D.sound.engines) + col("Sampler", D.sound.sampler) + col("Sequencer", D.sound.sequencer) + '</div><p class="hint" style="margin-top:14px">Which engines go in, and what still has to be built, is tracked on the <a href="#engines">Engine map</a>.</p>';
  } else if (dirTab === "io") {
    h += '<div class="card"><div class="items">' + D.io.map(i => "<b>" + esc(i.k) + "</b><div>" + esc(i.v) + (i.note ? "<small>" + esc(i.note) + "</small>" : "") + "</div>").join("") + "</div></div>";
  } else if (dirTab === "parts") {
    h += '<div class="card tw" style="padding:0"><table><tr><th style="text-align:left">Part</th><th style="text-align:left">Direction</th><th>State</th></tr>' + D.parts_direction.map(p => '<tr><td style="text-align:left"><b>' + esc(p.part) + '</b></td><td style="text-align:left;white-space:normal;min-width:280px">' + esc(p.dir) + '</td><td><span class="pill ' + (p.state === "open" ? "watch" : "info") + '">' + esc(p.state) + "</span></td></tr>").join("") + "</table></div>";
    if (PARTS && PARTS.categories) {
      h += H2("What the parts search found", "updated " + esc(PARTS.updated)) + '<div class="note warn" style="margin-bottom:14px"><b>Price read.</b> ' + esc(PARTS.price_read) + "</div>" +
        PARTS.categories.map(c => '<div class="card" style="margin-bottom:14px"><h2 style="font-size:17px">' + esc(c.title) + '</h2><div class="hint">' + esc(c.note) + '</div><div class="tw"><table><tr><th style="text-align:left">Option</th><th>Price</th><th>Qty</th><th></th><th style="text-align:left">Specs and fit</th></tr>' + c.options.map(o => '<tr><td style="text-align:left;white-space:normal;min-width:170px">' + (/^https?:\/\//.test(o.url || "") ? '<a href="' + esc(o.url) + '" target="_blank" rel="noopener">' + esc(o.name) + "</a>" : esc(o.name)) + '<br><span class="hint">' + esc(o.vendor || "") + "</span></td><td>" + (o.price_usd == null ? "–" : money(o.price_usd)) + "</td><td>" + esc(o.qty || "") + '</td><td><span class="pill ' + (o.confidence === "seen" ? "clean" : "watch") + '">' + esc(o.confidence) + '</span></td><td style="text-align:left;white-space:normal;min-width:260px">' + esc(o.specs) + '<br><span class="hint">' + esc(o.fit) + "</span></td></tr>").join("") + "</table></div></div>").join("") +
        '<p class="hint">"seen" means the price was read on the vendor page on the date above; "estimate" is not from a page. Pages opened are listed in hardware/prototype/PARTS-SOURCES.md.</p>';
    }
  } else if (dirTab === "pipeline") {
    h += '<div class="flow">' + D.pipeline.map(s => "<div><b>" + esc(s.h) + "</b><span>" + esc(s.p) + '</span><div style="margin-top:8px"><span class="pill info">' + esc(s.where) + "</span></div></div>").join("") + "</div>" +
      H2("What moves, and how") + '<div class="card tw" style="padding:0"><table><tr><th style="text-align:left">Thing</th><th style="text-align:left">Console</th><th style="text-align:left">FM-1 today</th><th style="text-align:left">Our box</th></tr>' +
      [["Engine code", "C source in app/, visitors/, core/", "built in, flashed", "same source, built at 48 kHz"], ["Presets", "tagged on the Presets page", "compiled tables (rebuild to change)", "files in a pack on the card"], ["Samples", "chopped in the Sample Lab", "samples.bin, about 4.2 s, flashed after the firmware", "WAV files in a pack, minutes"], ["Kits", "exported from the Sample Lab", "the bank's 16 slots", "kit files in a pack"], ["Patterns", "sketched on the Patterns page", "saved on the unit; not portable yet", "pattern files in a pack"], ["Checks", "tools/sfa sim scripts", "the same scripts, then ears", "the same scripts on host and board"]].map(r => "<tr>" + r.map((c, i) => '<td style="text-align:left;white-space:normal' + (i ? "" : ";font-weight:600") + '">' + esc(c) + "</td>").join("") + "</tr>").join("") + "</table></div>" +
      '<p class="hint" style="margin-top:14px">The pack format is in Docs → FORMATS.md; the engine contract and the order of work are in <a href="#arch">Architecture</a>.</p>';
  } else {
    const st = q => pget(["questions", q.id, "status"], "open");
    h += '<div class="kpis">' + kpi("Open", D.open_questions.filter(q => st(q) !== "answered" && st(q) !== "parked").length) + kpi("Answered", D.open_questions.filter(q => st(q) === "answered").length + " of " + D.open_questions.length, "q") + '</div><div data-re>' +
      D.open_questions.map(q => '<div class="card ed" style="margin-bottom:12px"><div class="row" style="margin:0"><b style="flex:1;min-width:200px;font-size:15px">' + esc(q.q) + "</b>" + ctl(["questions", q.id, "status"], "open", "sel", ["open", "researching", "answered", "parked"]) + '</div><div style="color:var(--dim)">' + esc(q.why) + '</div><div><span class="eyebrow" style="display:inline;margin-right:6px">How to answer</span>' + esc(q.how) + "</div>" + ctl(["questions", q.id, "answer"], "", "area") + "</div>").join("") + "</div>";
  }
  const box = $("#dirBox"); box.innerHTML = h; bindPlan(box, drawDir);
}
function screenMock() {
  const bar = (x, n, v, t) => '<text x="' + (x + 50) + '" y="118" text-anchor="middle" font-size="13" fill="#9fb0d8" font-family="monospace">' + n + '</text><rect x="' + (x + 10) + '" y="130" width="80" height="90" rx="6" fill="#1b2440"/><rect x="' + (x + 10) + '" y="' + (220 - v * 0.9) + '" width="80" height="' + v * 0.9 + '" rx="6" fill="#e11d55"/><text x="' + (x + 50) + '" y="244" text-anchor="middle" font-size="16" fill="#fff" font-family="monospace">' + t + "</text>";
  return '<svg viewBox="0 0 480 320" role="img" aria-label="Mock of the engine page" style="width:100%;border-radius:8px;background:#0c1224"><rect width="480" height="26" fill="#141c36"/><text x="10" y="18" font-size="13" fill="#9fb0d8" font-family="monospace">120.0 ▶ PAT 03</text><text x="470" y="18" text-anchor="end" font-size="13" fill="#9fb0d8" font-family="monospace">SD ▪ 82%</text><text x="14" y="62" font-size="26" font-weight="700" fill="#fff" font-family="sans-serif">OTTER</text><text x="14" y="88" font-size="16" fill="#7da2ff" font-family="monospace">GLASS KEYS</text><text x="466" y="62" text-anchor="end" font-size="13" fill="#9fb0d8" font-family="monospace">PAGE 1/5</text>' + bar(20, "CUTOFF", 70, "89") + bar(130, "RESO", 35, "45") + bar(240, "ENV", 80, "102") + bar(350, "DRIVE", 12, "15") + '<g fill="#1b2440">' + Array.from({ length: 16 }, (_, i) => '<rect x="' + (14 + i * 28.5) + '" y="270" width="24" height="34" rx="4"' + ([0, 4, 8, 12, 14].includes(i) ? ' fill="#2456e6"' : "") + "/>").join("") + "</g></svg>";
}
document.addEventListener("click", e => { const t = e.target.closest("[data-dt]"); if (t) { dirTab = t.dataset.dt; drawDir(); } });

/* =====================================================================================================
   LAYOUTS: three ways to place keys, pads, screen and encoders; sizes follow the key pitch
   ===================================================================================================== */
const LAY_DEF = { concept: "steprow", keys: 25, kp: 18, pad: 26, screen: "3.5", h: 38, fm1w: "", fm1d: "", fm1kp: "" };
const SCREENS = { "2.8": { w: 57.6, h: 43.2, px: "320 x 240" }, "3.5": { w: 73.4, h: 49.0, px: "480 x 320" }, "4.0": { w: 83.5, h: 55.7, px: "480 x 320" }, "4.3": { w: 95.0, h: 53.9, px: "480 x 272" } };
const CONCEPTS = {
  steprow: { name: "Step Row", say: "Sixteen pads in one row, right above the keys: step 1 to 16 reads left to right like the sequencer itself. Screen in the middle with two encoders on each side. The most compact of the three.", plus: "smallest body; pads double as an obvious step row; both hands reach pads from the keys", minus: "pads are narrow (finger drumming is tighter); one long row can feel far at the ends" },
  butterfly: { name: "Butterfly", say: "The screen sits in the centre with eight pads on each side, like wings. Left hand drums on the left wing, right hand on the right, eyes stay in the middle. Encoders in a row under the screen.", plus: "full-size pads; symmetrical and unlike anything on the shelf; wide body allows near full-size keys", minus: "widest body; a step row split in two halves" },
  terrace: { name: "Terrace", say: "A 2 x 8 pad block on the left, screen on the right with the encoders beneath it. Pads for the left hand, screen and editing for the right.", plus: "full-size pads in a familiar two-row strip; all editing in one place", minus: "asymmetric: left-handed drummers lose out; screen is off-centre" }
};
function layGeom(o) {
  const m = 12, g = 4, sc = SCREENS[o.screen] || SCREENS["3.5"], sw = sc.w + 10, sh = sc.h + 8, nW = { 13: 8, 25: 15, 37: 22 }[o.keys] || 15, kd = Math.round(Math.min(110, o.kp * 4.4)), it = [], enc = 14, p = o.pad;
  const keysW = nW * o.kp; let Wi, D, ky;
  const R = (t, x, y, w, h, r) => it.push({ t, x, y, w, h, r: r || 0 });
  if (o.concept === "butterfly") {
    const wing = 4 * p + 3 * g, band = Math.max(sh, 2 * p + g); Wi = Math.max(keysW, 2 * wing + sw + 20);
    const x0 = m + (Wi - (2 * wing + sw + 20)) / 2;
    for (let r = 0; r < 2; r++) for (let c = 0; c < 4; c++) { R("pad", x0 + c * (p + g), m + (band - (2 * p + g)) / 2 + r * (p + g), p, p, 3); R("pad", x0 + wing + 10 + sw + 10 + c * (p + g), m + (band - (2 * p + g)) / 2 + r * (p + g), p, p, 3); }
    R("screen", x0 + wing + 10, m + (band - sh) / 2, sw, sh, 3);
    const sy = m + band + 6; for (let i = 0; i < 4; i++) R("enc", m + Wi / 2 - 54 + i * 36 - enc / 2 + 0, sy + 3, enc, enc, enc / 2);
    for (let i = 0; i < 6; i++) { R("btn", m + i * 16, sy + 5, 12, 10, 2); R("btn", m + Wi - 12 - i * 16, sy + 5, 12, 10, 2); }
    ky = sy + 20 + 6; D = ky + kd + m;
  } else if (o.concept === "terrace") {
    const block = 8 * p + 7 * g, band = Math.max(sh, 2 * p + g); Wi = Math.max(keysW, block + 12 + sw);
    for (let r = 0; r < 2; r++) for (let c = 0; c < 8; c++) R("pad", m + c * (p + g), m + (band - (2 * p + g)) / 2 + r * (p + g), p, p, 3);
    R("screen", m + Wi - sw, m + (band - sh) / 2, sw, sh, 3);
    const sy = m + band + 6; for (let i = 0; i < 4; i++) R("enc", m + Wi - sw + (sw - 4 * enc) / 5 * (i + 1) + i * enc, sy + 3, enc, enc, enc / 2);
    for (let i = 0; i < 12; i++) R("btn", m + i * 16, sy + 5, 12, 10, 2);
    ky = sy + 20 + 6; D = ky + kd + m;
  } else {
    Wi = Math.max(keysW, sw + 4 * 26 + 4 * 16 + 24); const pw = (Wi - 15 * g) / 16, ph = Math.min(p, 24);
    const sx = m + (Wi - sw) / 2; R("screen", sx, m, sw, sh, 3);
    [-2, -1, 1, 2].forEach(k => R("enc", (k < 0 ? sx + k * 26 + 4 : sx + sw + (k - 1) * 26 + 8), m + sh / 2 - enc / 2, enc, enc, enc / 2));
    for (let i = 0; i < 6; i++) { R("btn", m + (i % 2) * 16, m + 5 + Math.floor(i / 2) * 16, 12, 10, 2); R("btn", m + Wi - 12 - (i % 2) * 16, m + 5 + Math.floor(i / 2) * 16, 12, 10, 2); }
    const py = m + sh + 8; for (let i = 0; i < 16; i++) R("pad", m + i * (pw + g), py, pw, ph, 2);
    ky = py + ph + 8; D = ky + kd + m; o._pw = pw;
  }
  const kx = m + (Wi - keysW) / 2;
  for (let i = 0; i < nW; i++) R("white", kx + i * o.kp, ky, o.kp, kd, 1.5);
  for (let i = 0; i < nW - 1; i++) if ([0, 1, 3, 4, 5].includes(i % 7)) R("black", kx + (i + 1) * o.kp - o.kp * 0.3, ky, o.kp * 0.6, kd * 0.62, 1);
  return { W: Wi + 2 * m, D, it, kd, nW, sc, buttons: 12, padW: o.concept === "steprow" ? o._pw : p };
}
function laySvg(o, big, inlineStyle) {
  const G = layGeom(o), pad = big ? 16 : 4, cls = { screen: "dv-screen", pad: "dv-pad", white: "dv-white", black: "dv-black", enc: "dv-enc", btn: "dv-btn" };
  const st = inlineStyle ? "<style>.dv-body{fill:#eef1f7;stroke:#0e1220;stroke-width:.4}.dv-screen{fill:#0e1220}.dv-pad{fill:#e11d55}.dv-white{fill:#fff;stroke:#0e1220;stroke-width:.3}.dv-black{fill:#0e1220}.dv-enc{fill:#fff;stroke:#0e1220;stroke-width:.5}.dv-btn{fill:#c5ccdb}.dv-dim{stroke:#586078;stroke-width:.3;fill:none}.dv-txt{font:5px monospace;fill:#586078}</style>" : "";
  let s = '<svg viewBox="' + (-pad) + " " + (-pad) + " " + (G.W + 2 * pad) + " " + (G.D + 2 * pad) + '" ' + (inlineStyle ? 'width="' + (G.W + 2 * pad) + 'mm" height="' + (G.D + 2 * pad) + 'mm" xmlns="http://www.w3.org/2000/svg"' : 'role="img" aria-label="' + esc(CONCEPTS[o.concept].name) + ' layout, top view"') + ">" + st + '<rect class="dv-body" x="0" y="0" width="' + G.W + '" height="' + G.D + '" rx="8"/>';
  G.it.forEach(i => { s += '<rect class="' + cls[i.t] + '" x="' + i.x.toFixed(1) + '" y="' + i.y.toFixed(1) + '" width="' + i.w.toFixed(1) + '" height="' + i.h.toFixed(1) + '" rx="' + i.r + '"/>'; if (i.t === "screen") s += '<rect class="dv-screen-in" fill="#2456e6" x="' + (i.x + 5).toFixed(1) + '" y="' + (i.y + 4).toFixed(1) + '" width="' + (i.w - 10).toFixed(1) + '" height="' + (i.h - 8).toFixed(1) + '" rx="1"/>'; });
  if (big) s += '<path class="dv-dim" d="M0 ' + (G.D + 8) + " H" + G.W + " M0 " + (G.D + 5) + " V" + (G.D + 11) + " M" + G.W + " " + (G.D + 5) + " V" + (G.D + 11) + " M" + (G.W + 8) + " 0 V" + G.D + " M" + (G.W + 5) + " 0 H" + (G.W + 11) + " M" + (G.W + 5) + " " + G.D + " H" + (G.W + 11) + '"/><text class="dv-txt" x="' + G.W / 2 + '" y="' + (G.D + 14.5) + '" text-anchor="middle">' + G.W.toFixed(0) + ' mm</text><text class="dv-txt" x="' + (G.W + 13) + '" y="' + G.D / 2 + '" text-anchor="middle" transform="rotate(90 ' + (G.W + 13) + " " + G.D / 2 + ')">' + G.D.toFixed(0) + " mm</text>";
  return s + "</svg>";
}
const layOpt = () => { const o = {}; Object.keys(LAY_DEF).forEach(k => o[k] = pget(["layout", k], LAY_DEF[k])); o.kp = +o.kp || 18; o.pad = +o.pad || 26; o.keys = +o.keys || 25; return o; };
WS.layouts = async function () {
  $("#main").innerHTML = '<div class="row"><h1 style="margin:0 12px 0 0">Layout Concepts</h1>' + saveBar + '</div><p class="lede">Three ways to place the keys, pads, screen and encoders. None is a 4 x 4 pad block under a screen. The size follows the key pitch: change it and every drawing and number follows. Drawn to scale, top view.</p><div id="layBox">Loading...</div>';
  await loadAll(); drawLay();
};
function drawLay() {
  const o = layOpt(), G = layGeom(o), C = CONCEPTS[o.concept], fw = +o.fm1w, fd = +o.fm1d, fk = +o.fm1kp;
  const f = (k, label, kind, unit, opts) => '<label class="fld"><span>' + label + (unit ? " <small>" + unit + "</small>" : "") + "</span>" + ctl(["layout", k], LAY_DEF[k], kind, opts) + "</label>";
  const oct = o.kp * 7, rel = fk ? (o.kp / fk * 100 - 100) : null;
  $("#layBox").innerHTML =
    '<div class="concepts">' + Object.keys(CONCEPTS).map(k => { const oo = Object.assign({}, o, { concept: k }), g = layGeom(oo); return '<div class="card concept' + (k === o.concept ? " on" : "") + '" data-lc="' + k + '" tabindex="0" role="button" aria-pressed="' + (k === o.concept) + '"><h2 style="font-size:17px">' + CONCEPTS[k].name + "<small>" + g.W.toFixed(0) + " x " + g.D.toFixed(0) + " mm</small></h2>" + laySvg(oo, false) + "</div>"; }).join("") + "</div>" +
    H2(C.name, "selected") + '<div class="card bigplan">' + laySvg(o, true) + '<div style="color:var(--dim)">' + C.say + '</div><div class="items"><b>For</b><div>' + C.plus + "</div><b>Against</b><div>" + C.minus + '</div></div><div class="actions"><button class="tool" id="layPrint">Print at 1:1</button><button id="layUse">Use this size in the Planner</button><button id="layDec">Record as the layout proposal</button><span class="hint" id="layMsg"></span></div></div>' +
    '<div class="kpis" style="margin-top:20px">' + kpi("Body", G.W.toFixed(0) + " x " + G.D.toFixed(0) + " x " + (+o.h || 38) + " mm") + kpi("Footprint", (G.W * G.D / 100).toFixed(0) + " cm²" + (fw && fd ? " · " + (G.W * G.D / (fw * fd)).toFixed(1) + "x FM-1" : "")) + kpi("Key pitch", o.kp + " mm" + (rel != null ? " · " + (rel >= 0 ? "+" : "") + rel.toFixed(0) + "%" : ""), o.kp >= 17 ? "q" : "est") + kpi("Octave", oct.toFixed(0) + " mm") + kpi("Pad", G.padW.toFixed(0) + " mm wide", G.padW >= 22 ? "q" : "est") + kpi("Controls", o.keys + " keys · 16 pads · 4 enc · " + G.buttons + " btn") + "</div>" +
    H2("Sizes") + '<div class="card ed" data-re><div class="grid2">' + f("keys", "Keys", "sel", "", ["13", "25", "37"]) + f("kp", "Key pitch (white key width)", "num", "mm") + f("pad", "Pad size", "num", "mm") + f("screen", "Screen diagonal", "sel", "inch", Object.keys(SCREENS)) + f("h", "Body height", "num", "mm") + "</div>" +
    '<div class="hint">Reference: a full-size piano key is about 23.5 mm wide (an octave of about 164 mm). Slim and mini keys are smaller and vary by maker, so measure the ones you like. Screen: ' + G.sc.px + ", active area " + G.sc.w + " x " + G.sc.h + ' mm.</div></div>' +
    H2("Your FM-1, for comparison", "measure with a ruler") + '<div class="card ed" data-re><div class="grid2">' + f("fm1w", "FM-1 width", "num", "mm") + f("fm1d", "FM-1 depth", "num", "mm") + f("fm1kp", "FM-1 key pitch", "num", "mm") + '</div><div class="hint">These are blank on purpose: we have no measured FM-1 dimensions on file. Fill them in and the numbers above show how much bigger the box and its keys are.</div></div>';
  const box = $("#layBox"); bindPlan(box, drawLay);
  box.querySelectorAll("[data-lc]").forEach(c => { const pick = () => { pset(["layout", "concept"], c.dataset.lc); drawLay(); }; c.onclick = pick; c.onkeydown = e => { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); pick(); } }; });
  $("#layPrint").onclick = () => { const w = window.open("", "_blank"); if (!w) { $("#layMsg").textContent = "Allow pop-ups to print."; return; } w.document.write("<!doctype html><title>SFA " + C.name + " 1:1</title><style>@page{margin:8mm}body{margin:0;font:10pt sans-serif}</style><p>SFA " + C.name + " · " + G.W.toFixed(0) + " x " + G.D.toFixed(0) + " mm · key pitch " + o.kp + " mm · print at 100% (no fit to page), landscape. Check with a ruler: the body should measure " + G.W.toFixed(0) + " mm.</p>" + laySvg(o, true, true)); w.document.close(); w.focus(); setTimeout(() => w.print(), 300); };
  $("#layUse").onclick = () => { pset(["target", "w"], Math.round(G.W)); pset(["target", "d"], Math.round(G.D)); pset(["target", "h"], +o.h || 38); pset(["target", "controls"], o.keys + " keys at " + o.kp + " mm, 16 pads, 4 encoders, about " + G.buttons + " buttons (" + C.name + ")"); pset(["target", "screen"], o.screen + " in colour IPS, " + G.sc.px.replace(/ /g, "")); $("#layMsg").textContent = "Planner target updated."; };
  $("#layDec").onclick = () => { const D = P.decisions = P.decisions || []; let d = D.find(x => x.id === "f6"); if (!d) { d = { id: "f6", title: "", status: "proposed", why: "", date: "" }; D.push(d); } d.title = "Layout concept: " + C.name; d.status = "proposed"; d.why = G.W.toFixed(0) + " x " + G.D.toFixed(0) + " mm, " + o.keys + " keys at " + o.kp + " mm pitch, " + o.screen + " in screen. Proposed from the Layouts page; print it at 1:1 and try it before locking."; psave(); $("#layMsg").textContent = "Recorded in Decisions as a proposal."; };
}

/* =====================================================================================================
   PATTERNS: a sketchbook for beats and arrangements; exports sfa-pattern/1
   ===================================================================================================== */
let patI = 0, patPlay = null, actx = null;
const PAT_TRACKS = [["KICK", "cat"], ["SNARE", "cat"], ["HAT", "cat"], ["PERC", "cat"], ["BASS", "fox"], ["LEAD", "otter"]];
const newPat = n => ({ id: "p" + Date.now().toString(36), name: n || "NEW PATTERN", bpm: 120, steps: 16, swing: 0, notes: "", unit: "ecb", tracks: PAT_TRACKS.map(t => ({ name: t[0], engine: t[1], cells: Array(64).fill(0) })) });
const pats = () => { if (!Array.isArray(P.patterns) || !P.patterns.length) P.patterns = [newPat("FIRST SKETCH")]; return P.patterns; };
WS.patterns = async function () {
  $("#main").innerHTML = '<div class="row"><h1 style="margin:0 12px 0 0">Patterns</h1>' + saveBar + '</div><p class="lede">A sketchbook for beats and arrangements. Click a step to set it, click again for harder, softer, off. Play gives a rough click-track preview, not the engines. Sketches are saved with the plan and export in the pack format.</p><div id="patBox">Loading...</div>';
  await loadAll(); patStop(); drawPat();
};
function drawPat() {
  const L = pats(); patI = Math.min(patI, L.length - 1); const p = L[patI], n = +p.steps || 16, arr = P.arrangement = Array.isArray(P.arrangement) ? P.arrangement : [];
  const stepMs = 60000 / (+p.bpm || 120) / 4, bars = arr.reduce((t, s) => t + (+s.rep || 1) * ((L.find(x => x.id === s.pat) || p).steps / 16), 0), secs = arr.reduce((t, s) => { const q = L.find(x => x.id === s.pat) || p; return t + (+s.rep || 1) * q.steps * 60 / (+q.bpm || 120) / 4; }, 0);
  let h = '<div class="row"><select id="patSel">' + L.map((x, i) => '<option value="' + i + '"' + (i === patI ? " selected" : "") + ">" + esc(x.name) + "</option>").join("") + '</select><button id="patNew">New</button><button id="patDup">Duplicate</button><button id="patDel" class="danger">Delete</button><span class="sp"></span><button class="tool" id="patGo">' + (patPlay ? "Stop" : "Play") + '</button><button id="patExp">Export</button><button id="patImp">Import</button><input type="file" id="patFile" accept=".json" hidden></div>' +
    '<div class="card ed"><div class="grid2"><label class="fld"><span>Name</span><input data-pf="name" value="' + esc(p.name) + '" maxlength="24"></label><label class="fld"><span>Tempo <small>BPM</small></span><input type="number" data-pf="bpm" value="' + esc(p.bpm) + '" min="40" max="240"></label><label class="fld"><span>Steps</span><select data-pf="steps">' + [16, 32, 48, 64].map(v => "<option" + (v === n ? " selected" : "") + ">" + v + '</option>').join("") + '</select></label><label class="fld"><span>Swing <small>%</small></span><input type="number" data-pf="swing" value="' + esc(p.swing) + '" min="0" max="75"></label><label class="fld"><span>For</span><select data-pf="unit">' + [["ecb", "Earth Creature Beats"], ["as", "Alien Stack"], ["box", "The prototype"]].map(v => '<option value="' + v[0] + '"' + (v[0] === p.unit ? " selected" : "") + ">" + v[1] + "</option>").join("") + "</select></label></div>";
  for (let b = 0; b < n; b += 16) {
    h += '<div class="tw" style="padding:2px"><div class="grid16" style="grid-template-columns:86px repeat(16,minmax(22px,1fr));min-width:520px;margin-bottom:10px">' + '<div class="hint mono">' + (n > 16 ? "bar " + (b / 16 + 1) : "") + "</div>" + Array.from({ length: 16 }, (_, i) => '<div class="hint mono" style="text-align:center">' + (i % 4 === 0 ? b + i + 1 : "·") + "</div>").join("") +
      p.tracks.map((t, ti) => '<input class="tn" data-tn="' + ti + '" value="' + esc(t.name) + '" maxlength="9" style="padding:3px 6px;font:500 12px var(--mono)" aria-label="Track name">' + Array.from({ length: 16 }, (_, i) => { const v = t.cells[b + i] || 0; return '<button class="cell' + (v ? " v" + v : (Math.floor(i / 4) % 2 ? " b" : "")) + '" data-cell="' + ti + "," + (b + i) + '" aria-label="' + esc(t.name) + " step " + (b + i + 1) + (v ? " on" : " off") + '"></button>'; }).join("")).join("") + "</div></div>";
  }
  h += '<div class="row" style="margin:0"><button class="sm" id="patAddT">Add track</button><button class="sm" id="patClr">Clear steps</button><span class="hint">One step is ' + stepMs.toFixed(0) + " ms; a bar is " + (stepMs * 16 / 1000).toFixed(2) + ' s.</span></div><label class="fld" style="margin-top:6px"><span>Notes: sound choices, what works, what to try on the unit</span><textarea data-pf="notes" rows="3">' + esc(p.notes) + "</textarea></label></div>" +
    H2("Arrangement", arr.length ? bars.toFixed(0) + " bars · " + Math.floor(secs / 60) + ":" + String(Math.round(secs % 60)).padStart(2, "0") : "empty") + '<div class="arr">' + arr.map((s, i) => '<div class="slot"><b>SLOT ' + (i + 1) + '</b><select data-as="' + i + '" data-af="pat" style="width:100%;margin:6px 0">' + L.map(x => '<option value="' + x.id + '"' + (x.id === s.pat ? " selected" : "") + ">" + esc(x.name) + '</option>').join("") + '</select><div class="row" style="margin:0"><span class="hint">x</span><input type="number" data-as="' + i + '" data-af="rep" value="' + (+s.rep || 1) + '" min="1" max="64" style="width:64px"><button class="sm" data-ax="' + i + '">remove</button></div><input data-as="' + i + '" data-af="note" value="' + esc(s.note || "") + '" placeholder="scene / note" style="width:100%;margin-top:6px"></div>').join("") + '<button id="arrAdd" style="align-self:center">Add slot</button></div><p class="hint" style="margin-top:10px">ECB and Alien Stack songs hold four slots with repeats and a scene per slot; more slots here are for planning the box.</p>';
  const box = $("#patBox"); box.innerHTML = h;
  const save = re => { psave(); if (re) drawPat(); };
  $("#patSel").onchange = e => { patI = +e.target.value; patStop(); drawPat(); };
  $("#patNew").onclick = () => { L.push(newPat("PATTERN " + (L.length + 1))); patI = L.length - 1; save(1); };
  $("#patDup").onclick = () => { const c = JSON.parse(JSON.stringify(p)); c.id = "p" + Date.now().toString(36); c.name = (p.name + " 2").slice(0, 24); L.push(c); patI = L.length - 1; save(1); };
  $("#patDel").onclick = () => { if (L.length < 2 || !confirm("Delete " + p.name + "?")) return; L.splice(patI, 1); P.arrangement = arr.filter(s => s.pat !== p.id); patI = 0; save(1); };
  box.querySelectorAll("[data-pf]").forEach(el => el.onchange = () => { const k = el.dataset.pf; p[k] = el.type === "number" ? Math.max(+el.min || 0, Math.min(+el.max || 1e9, +el.value || 0)) : k === "steps" ? +el.value : el.value; save(k !== "notes"); });
  box.querySelectorAll("[data-tn]").forEach(el => el.onchange = () => { p.tracks[+el.dataset.tn].name = el.value.toUpperCase().slice(0, 9) || "TRACK"; save(1); });
  box.querySelectorAll("[data-cell]").forEach(el => el.onclick = () => { const [ti, si] = el.dataset.cell.split(",").map(Number), c = p.tracks[ti].cells; c[si] = { 0: 2, 2: 3, 3: 1, 1: 0 }[c[si] || 0]; if (c[si]) blip(p.tracks[ti].name, c[si]); save(1); });
  $("#patAddT").onclick = () => { if (p.tracks.length < 12) { p.tracks.push({ name: "TRACK " + (p.tracks.length + 1), engine: "cat", cells: Array(64).fill(0) }); save(1); } };
  $("#patClr").onclick = () => { if (confirm("Clear every step in " + p.name + "?")) { p.tracks.forEach(t => t.cells.fill(0)); save(1); } };
  $("#arrAdd").onclick = () => { arr.push({ pat: p.id, rep: 4, note: "" }); save(1); };
  box.querySelectorAll("[data-as]").forEach(el => el.onchange = () => { const s = arr[+el.dataset.as]; s[el.dataset.af] = el.dataset.af === "rep" ? Math.max(1, Math.min(64, +el.value || 1)) : el.value; save(el.dataset.af !== "note"); });
  box.querySelectorAll("[data-ax]").forEach(el => el.onclick = () => { arr.splice(+el.dataset.ax, 1); save(1); });
  $("#patGo").onclick = () => { patPlay ? patStop() : patStart(p); drawPat(); };
  $("#patExp").onclick = () => dl(p.name.toLowerCase().replace(/[^a-z0-9]+/g, "-") + ".sfa-pattern.json", JSON.stringify(patExport(p), null, 1));
  $("#patImp").onclick = () => $("#patFile").click();
  $("#patFile").onchange = async e => { try { const d = JSON.parse(await e.target.files[0].text()); if (d.schema !== "sfa-pattern/1" || !Array.isArray(d.tracks)) throw new Error("not an sfa-pattern/1 file"); const q = newPat(String(d.name || "IMPORTED").slice(0, 24)); q.bpm = Math.max(40, Math.min(240, +d.bpm || 120)); q.steps = [16, 32, 48, 64].includes(+d.steps) ? +d.steps : 16; q.swing = Math.max(0, Math.min(75, +d.swing || 0)); q.tracks = d.tracks.slice(0, 12).map(t => { const c = Array(64).fill(0); (t.steps || []).forEach(s => { if (s.i >= 0 && s.i < 64) c[s.i | 0] = s.vel > 105 ? 3 : s.vel > 70 ? 2 : 1; }); return { name: String(t.name || t.engine || "TRACK").toUpperCase().slice(0, 9), engine: String(t.engine || "cat").slice(0, 12), cells: c }; }); L.push(q); patI = L.length - 1; save(1); } catch (x) { alert("Import failed: " + x.message); } };
}
const patExport = p => ({ schema: "sfa-pattern/1", name: p.name, steps: +p.steps, swing: +p.swing, bpm: +p.bpm, unit: p.unit, notes: p.notes, tracks: p.tracks.map((t, i) => ({ name: t.name, engine: t.engine, pad: i, steps: t.cells.slice(0, p.steps).map((v, k) => v ? { i: k, vel: [0, 60, 100, 122][v] } : null).filter(Boolean), locks: [] })) });
function blip(name, v, when) {
  try { actx = actx || new (window.AudioContext || window.webkitAudioContext)(); } catch (e) { return; }
  const t = when || actx.currentTime, g = actx.createGain(), lvl = [0, 0.25, 0.5, 0.8][v] * 0.5; g.connect(actx.destination);
  if (/HAT|PERC|SNARE|CLAP|SHAK|RIDE|CYM/.test(name)) {
    const len = /SNARE|CLAP/.test(name) ? 0.14 : 0.05, b = actx.createBuffer(1, Math.ceil(actx.sampleRate * len), actx.sampleRate), d = b.getChannelData(0); for (let i = 0; i < d.length; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / d.length, 2);
    const s = actx.createBufferSource(), f = actx.createBiquadFilter(); f.type = "highpass"; f.frequency.value = /SNARE|CLAP/.test(name) ? 1200 : 6000; s.buffer = b; s.connect(f); f.connect(g); g.gain.setValueAtTime(lvl, t); s.start(t);
  } else {
    const o = actx.createOscillator(), kick = /KICK|BD|808/.test(name), f0 = kick ? 150 : /BASS|SUB/.test(name) ? 65 : 330; o.type = kick ? "sine" : /BASS|SUB/.test(name) ? "triangle" : "square";
    o.frequency.setValueAtTime(f0, t); if (kick) o.frequency.exponentialRampToValueAtTime(45, t + 0.12);
    g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(lvl * (o.type === "square" ? 0.4 : 1), t + 0.004); g.gain.exponentialRampToValueAtTime(0.0001, t + (kick ? 0.25 : 0.16)); o.connect(g); o.start(t); o.stop(t + 0.3);
  }
}
function patStart(p) {
  try { actx = actx || new (window.AudioContext || window.webkitAudioContext)(); actx.resume(); } catch (e) { return; }
  let step = 0, next = actx.currentTime + 0.06;
  const tick = () => { const sd = 60 / (+p.bpm || 120) / 4; while (next < actx.currentTime + 0.12) { const sw = step % 2 ? sd * (+p.swing || 0) / 100 * 0.66 : 0; p.tracks.forEach(t => { const v = t.cells[step]; if (v) blip(t.name, v, next + sw); }); const sNow = step; setTimeout(() => { document.querySelectorAll(".cell.play").forEach(c => c.classList.remove("play")); document.querySelectorAll('[data-cell$=",' + sNow + '"]').forEach(c => c.classList.add("play")); }, Math.max(0, (next - actx.currentTime) * 1000)); next += sd; step = (step + 1) % (+p.steps || 16); } };
  patPlay = setInterval(tick, 25); tick();
}
function patStop() { if (patPlay) clearInterval(patPlay); patPlay = null; }
window.addEventListener("hashchange", patStop);

/* =====================================================================================================
   TOOLBOX: small calculators for daily FM-1 work and for sizing the box
   ===================================================================================================== */
let taps = [];
const TB = { bpm: 120, note: 60, n: 16, la: -16.3, lb: -18.3, mah: 3000, watts: 1.5, eff: 85, gb: 32, db: -6 };
WS.toolbox = async function () {
  $("#main").innerHTML = '<h1>Toolbox</h1><p class="lede">Small calculators for the numbers that keep coming up: tempo and delay times, notes, the FM-1 sample bank, level matching, card space and battery life. Nothing here is saved.</p><div class="cards" id="tbBox" style="grid-template-columns:repeat(auto-fit,minmax(330px,1fr))"></div>';
  drawTb();
};
function drawTb() {
  const n = (k, label, unit, step) => '<label class="fld"><span>' + label + (unit ? " <small>" + unit + "</small>" : "") + '</span><input type="number" data-tb="' + k + '" value="' + TB[k] + '" step="' + (step || "any") + '"></label>';
  const q = 60000 / TB.bpm, rows = [["1/4", q], ["1/8", q / 2], ["1/8 dotted", q * 0.75], ["1/8 triplet", q / 3], ["1/16", q / 4], ["1/32", q / 8], ["1 bar", q * 4]];
  const NN = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"], hz = 440 * Math.pow(2, (TB.note - 69) / 12);
  const REGION = 46 * 4096 - 64, cnt = Math.max(1, Math.min(16, Math.round(TB.n))), smp = Math.floor((REGION - 8 - 20 * cnt) / 2), secs = smp / 22050;
  const wh = TB.mah / 1000 * 3.7, hrs = wh * TB.eff / 100 / TB.watts, mins = TB.gb * 1e9 / (48000 * 2 * 2) / 60, gain = TB.la - TB.lb;
  const tbl = (hd, rs) => '<div class="tw"><table><tr>' + hd.map(x => "<th>" + x + "</th>").join("") + "</tr>" + rs.map(r => "<tr>" + r.map(c => "<td>" + c + "</td>").join("") + "</tr>").join("") + "</table></div>";
  const card = (t, sub, body) => '<div class="card ed"><h2 style="font-size:17px">' + t + '</h2><div class="hint" style="margin-top:-6px">' + sub + "</div>" + body + "</div>";
  $("#tbBox").innerHTML =
    card("Tempo", "Delay times and step lengths.", '<div class="row" style="margin:0;align-items:flex-end">' + n("bpm", "Tempo", "BPM", 0.1) + '<button class="tool" id="tapBtn">Tap</button></div>' + tbl(["note", "ms", "Hz", "samples @44.1k", "@48k"], rows.map(r => [r[0], r[1].toFixed(1), (1000 / r[1]).toFixed(2), Math.round(r[1] * 44.1), Math.round(r[1] * 48)]))) +
    card("Note", "MIDI note to name, pitch and period.", n("note", "MIDI note", "0 to 127", 1) + '<div class="kpis" style="margin:0;grid-template-columns:repeat(3,1fr)">' + kpi("Name", NN[((TB.note % 12) + 12) % 12] + (Math.floor(TB.note / 12) - 1)) + kpi("Hz", hz.toFixed(2)) + kpi("Period", (1000 / hz).toFixed(2) + " ms") + '</div><div class="hint">' + (44100 / hz).toFixed(1) + " samples at 44.1 kHz, " + (48000 / hz).toFixed(1) + " at 48 kHz. A4 = 440 Hz.</div>") +
    card("FM-1 sample bank", "How much audio fits in the user bank (22.05 kHz, mono, 16-bit).", n("n", "Samples in the kit", "1 to 16", 1) + '<div class="kpis" style="margin:0;grid-template-columns:repeat(2,1fr)">' + kpi("Total", secs.toFixed(2) + " s") + kpi("Each, if equal", (secs / cnt * 1000).toFixed(0) + " ms") + '</div><div class="hint">The region is ' + REGION.toLocaleString() + " bytes: 8 for the header, 20 per sample, the rest audio. One sample can be at most 4.17 s.</div>") +
    card("Level match", "How much to turn one render down to match another.", '<div class="grid2" style="margin:0">' + n("la", "Louder", "LUFS", 0.1) + n("lb", "Quieter", "LUFS", 0.1) + '</div><div class="kpis" style="margin:0;grid-template-columns:repeat(2,1fr)">' + kpi("Turn the louder down", gain.toFixed(1) + " dB") + kpi("As a gain", Math.pow(10, -gain / 20).toFixed(3)) + '</div>' + n("db", "Any dB value", "dB", 0.1) + '<div class="hint">' + TB.db + " dB is a gain of " + Math.pow(10, TB.db / 20).toFixed(4) + " (" + (Math.pow(10, TB.db / 20) * 100).toFixed(1) + "%).</div>") +
    card("Card space", "For the prototype: 48 kHz, 16-bit, stereo.", n("gb", "Card size", "GB", 1) + '<div class="kpis" style="margin:0;grid-template-columns:repeat(2,1fr)">' + kpi("Stereo audio", (mins / 60).toFixed(1) + " h") + kpi("Per minute", (48000 * 4 * 60 / 1e6).toFixed(1) + " MB") + '</div><div class="hint">The FM-1 bank holds about 4.2 seconds. One gigabyte holds ' + (1e9 / (48000 * 4) / 60).toFixed(0) + " minutes.</div>") +
    card("Battery", "Runtime from capacity and draw. Measure the draw; do not guess it.", '<div class="grid2" style="margin:0">' + n("mah", "Cell", "mAh at 3.7 V", 100) + n("watts", "Average draw", "W", 0.1) + n("eff", "Converter efficiency", "%", 1) + '</div><div class="kpis" style="margin:0;grid-template-columns:repeat(2,1fr)">' + kpi("Energy", wh.toFixed(1) + " Wh") + kpi("Runtime", hrs.toFixed(1) + " h", hrs >= 6 ? "q" : "est") + '</div><div class="hint">Target is 6 hours. ' + TB.mah + " mAh x 3.7 V = " + wh.toFixed(1) + " Wh; x " + TB.eff + "% / " + TB.watts + " W.</div>");
  document.querySelectorAll("[data-tb]").forEach(el => el.onchange = () => { const v = +el.value; if (isFinite(v) && (el.dataset.tb !== "bpm" || v > 0) && (el.dataset.tb !== "watts" || v > 0)) TB[el.dataset.tb] = v; drawTb(); });
  $("#tapBtn").onclick = () => { const t = performance.now(); taps = taps.filter(x => t - x < 2500); taps.push(t); if (taps.length > 2) { const d = (taps[taps.length - 1] - taps[0]) / (taps.length - 1); TB.bpm = Math.round(600000 / d) / 10; drawTb(); $("#tapBtn").focus(); } };
}
if (WS[page()]) route();
