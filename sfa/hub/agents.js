"use strict";
/* SFA Console: agents. Four reviewers (Prototype, Sound, Hardware, Workflow). Each has a built-in review that reads the
   workshop's own data and always works, and a chat that answers when the console was started with ANTHROPIC_API_KEY. */
const AG = { who: "prototype", info: null, threads: { prototype: [], sound: [], hardware: [], workflow: [] }, busy: false, design: null };
const AG_META = {
  prototype: { name: "Prototype", tag: "form, feel, identity", ask: ["What would you change first in this design?", "Is this layout distinct enough from an MPC and from the FM-1?", "Suggest materials for a recycled-junk build of this design."] },
  sound: { name: "Sound", tag: "engines, renders, presets", ask: ["Which renders should I listen to first, and why?", "How should ECB and Alien Stack be level-matched?", "What should the first engine port to 48 kHz watch out for?"] },
  hardware: { name: "Hardware", tag: "parts, power, cost", ask: ["Where is the cost going, and what would you cut?", "How many pins does this design need, and does a Daisy Seed have them?", "What should I build first on the dev kit?"] },
  workflow: { name: "Workflow", tag: "the daily loop", ask: ["What should I do next, in order?", "Is anything uncommitted or unflashed?", "Give me a checklist for flashing both units safely."] }
};
const F = (lv, t, go) => ({ lv, t, go });                               /* lv: 2 fix, 1 look, 0 fine */
function agReview(who) {
  const out = [], des = (P && Array.isArray(P.designs) && P.designs.length) ? P.designs : null, d = des ? (des.find(x => x.name === AG.design) || des[0]) : null;
  if (who === "prototype") {
    if (!d) return [F(1, "No design saved yet. Open the Designer, pick a concept and move one thing: that saves it.", "#designer")];
    const S = dzSpecs(d), B = d.body, fk = +pget(["layout", "fm1kp"], 0);
    out.push(F(0, "Reviewing " + d.name + ": " + B.w + " x " + B.d + " mm, " + S.cnt.keys + " keys, " + S.cnt.pads + " pads, " + S.cnt.encoders + " encoders, " + S.cnt.buttons + " buttons."));
    S.warn.forEach(w => out.push(F(2, w + ". Move or resize it in the Designer.", "#designer")));
    if (!S.cnt.keys) out.push(F(2, "No keybed. The brief says pads and keys, both."));
    else if (S.kp < 15) out.push(F(2, "Key pitch " + S.kp + " mm is in toy-keyboard territory. The whole point is keys that are comfortable: try 17 mm or more and see what it does to the width."));
    else if (S.kp < 17) out.push(F(1, "Key pitch " + S.kp + " mm is playable but tight. Print it at 1:1 and try a few chords before settling."));
    else out.push(F(0, "Key pitch " + S.kp + " mm (octave " + (S.kp * 7).toFixed(0) + " mm) should feel comfortable; a full-size key is about 23.5 mm."));
    if (!fk) out.push(F(1, "The FM-1 has not been measured. Without its key width there is no number for 'bigger than the FM-1'. Measure it and enter it in the Designer under Your FM-1.", "#designer"));
    if (!S.cnt.pads) out.push(F(2, "No pads. The brief says pads and keys, both."));
    else { if (S.cnt.pads !== 16) out.push(F(1, S.cnt.pads + " pads. The firmware thinks in sixteens (steps, slices, drum voices); another count needs a reason.")); if (S.padMin < 18) out.push(F(1, "Smallest pad is " + S.padMin + " mm. Under about 20 mm a pad works as a step button but not for finger drumming.")); }
    if (d.parts.some(p => p.type === "pads" && p.cols === 4 && p.rows === 4)) out.push(F(2, "A 4 x 4 pad block is the MPC arrangement the brief rules out. Split it into a row, two wings or two rails."));
    if (!S.cnt.screens) out.push(F(2, "No screen. A colour screen is in the brief.")); else if (+S.scr < 3.5) out.push(F(1, "A " + S.scr + " inch screen is below the 3.5 inch working choice; a 16-step grid with 8 tracks will not fit at 320 x 240."));
    if (S.cnt.encoders !== 4) out.push(F(1, S.cnt.encoders + " encoders. Every engine page has four parameters, so four encoders map one to one."));
    if (S.cnt.buttons < 8) out.push(F(1, "Only " + S.cnt.buttons + " buttons. Transport, shift, engine select and page need about twelve."));
    if (!d.parts.some(p => p.type === "knob")) out.push(F(1, "No volume knob. A real, always-reachable level control is worth its space."));
    if (B.w > 380) out.push(F(1, "At " + B.w + " mm wide it will not fit most backpacks' laptop sleeves (about 380 mm). Check the bag you actually use."));
    if (S.tilt < 2) out.push(F(1, "The body is flat (" + S.tilt.toFixed(1) + "°). A few degrees of rise towards the rear makes the screen readable from a seat; set a rear height above the front height."));
    const kb = d.parts.find(p => p.type === "keybed"); if (kb && kb.y + dzBox(kb).h < B.d - 20) out.push(F(1, "The keys are not at the front edge. Anything between the hands and the keys will be hit by wrists."));
    if (!(d.vibe || "").trim()) out.push(F(1, "No vibe note yet. One sentence on how it should feel settles a lot of material choices.", "#designer"));
    if (des.length < 2) out.push(F(1, "Only one design saved. Duplicate it and change one big thing (pad arrangement or key pitch) so there is something to compare."));
  } else if (who === "sound") {
    if (!S || !S.units) return [F(1, "The console server is not answering, so there are no renders to read.")];
    const songs = [];
    S.units.forEach(u => { const fs = u.render ? u.render.files : []; if (!fs.length) { out.push(F(1, u.name + ": no renders yet. Run Check all on the Units page.", "#units")); return; }
      const fl = fs.filter(f => f.verdict === "flag"), wa = fs.filter(f => f.verdict === "watch"), hot = fs.filter(f => f.tp > -1).sort((a, b) => b.tp - a.tp), lu = fs.filter(f => isFinite(f.lufs) && f.lufs > -60).sort((a, b) => a.lufs - b.lufs);
      fl.forEach(f => out.push(F(2, u.name + " " + f.file + " is flagged (true peak " + f1(f.tp) + ", " + f.clicks + " clicks). Open it in the Analyzer before anything is flashed.", "#analyzer")));
      wa.slice(0, 4).forEach(f => out.push(F(1, u.name + " " + f.file + ": watch (true peak " + f1(f.tp) + ", " + f.clicks + " clicks).", "#analyzer")));
      if (!fl.length && !wa.length) out.push(F(0, u.name + ": " + fs.length + " renders, all clean."));
      if (hot.length) out.push(F(1, u.name + ": " + hot[0].file + " peaks at " + f1(hot[0].tp) + " dBTP, under 1 dB of headroom."));
      if (lu.length > 3 && lu[lu.length - 1].lufs - lu[0].lufs > 12) out.push(F(1, u.name + ": levels spread " + (lu[lu.length - 1].lufs - lu[0].lufs).toFixed(1) + " LU between " + lu[0].file + " (" + f1(lu[0].lufs) + ") and " + lu[lu.length - 1].file + " (" + f1(lu[lu.length - 1].lufs) + "). Some of that is short hits against sustained notes; check that presets within one engine sit closer."));
      const sg = fs.find(f => f.file === "song.wav"); if (sg) songs.push([u.name, sg.lufs]); });
    if (songs.length === 2 && isFinite(songs[0][1]) && isFinite(songs[1][1])) { const dv = songs[0][1] - songs[1][1]; out.push(F(Math.abs(dv) > 1.5 ? 1 : 0, "Song loudness: " + songs[0][0] + " " + f1(songs[0][1]) + " LUFS, " + songs[1][0] + " " + f1(songs[1][1]) + " LUFS, " + Math.abs(dv).toFixed(1) + " LU apart." + (Math.abs(dv) > 1.5 ? " Side by side the " + (dv > 0 ? songs[1][0] : songs[0][0]) + " will sound weaker; A/B them level-matched in the Analyzer before changing either." : ""), "#analyzer")); }
    const tg = (P && P.presets) || {}, rw = Object.keys(tg).filter(k => (tg[k].tags || []).includes("rework")).length, any = Object.keys(tg).filter(k => (tg[k].tags || []).length).length;
    out.push(any ? F(rw ? 1 : 0, any + " presets tagged; " + rw + " marked rework.", "#presets") : F(1, "No presets are tagged yet. Tagging keepers and reworks is how the box's factory set gets chosen.", "#presets"));
    out.push(F(0, "Renders test the DSP and the sequencer, not the unit's converter or USB path. Record the flashed unit and drop the WAV next to its render to check that side."));
  } else if (who === "hardware") {
    const cap = +pget(["target", "bom_max"], 80), rmax = +pget(["target", "price_max"], 250), hrs = +pget(["target", "battery_h"], 6);
    out.push(F(2, "Cost: the parts search puts a keys + pads + screen + battery build at about $100 to $130 of parts (an estimate), against a BOM ceiling of $" + cap + ". At 2.5 to 3 times BOM that is $250 to $330 retail, at or over the $" + rmax + " top of the range. The ceiling or the range has to move; it is open in Decisions.", "#decisions"));
    out.push(F(1, "Keybed: no standalone 25-key mini keybed was found for sale as a part. Build the first prototype around a keybed taken from a cheap controller, and treat sourcing as the main supply risk.", "#direction"));
    out.push(F(1, "Pads: one bought force sensor per pad costs about $95 for sixteen. A printed sensor sheet under silicone pads is the direction; a 4-pad test strip on the dev kit answers feel and cost at once."));
    if (d) { const Sx = dzSpecs(d), mat = Math.ceil(Math.sqrt(Sx.scan)), pins = mat * 2 + 5 + 1 + 5 + 4 + 2; out.push(F(pins > 31 ? 2 : 1, "Pins for " + d.name + ": " + Sx.scan + " switch contacts (two per velocity key, buttons, encoders) is about a " + mat + " x " + mat + " matrix, so " + mat * 2 + " lines; add 5 for the pad mux and ADC, 5 for an SPI screen, 4 for the card, 2 for MIDI: roughly " + pins + ". The parts search notes the Daisy Seed has 31 GPIO, so " + (pins > 31 ? "this does not fit without shift registers or a scanner chip. Plan for one." : "it fits, tightly.") + " This is arithmetic, not a pinout.")); }
    out.push(F(1, "Screen speed: over SPI at 40 MHz a full 480 x 320 frame is roughly 16 a second. Meters and the play position want 30. Use partial updates, or a parallel bus on the product board; measure it on the dev kit before choosing."));
    out.push(F(1, "Battery: " + hrs + " hours at an assumed 1.5 W needs about " + Math.ceil(hrs * 1.5 / 0.85 / 3.7 * 1000 / 100) * 100 + " mAh at 3.7 V and 85 % conversion. The 1.5 W is a guess until the dev kit is measured with the screen and pads lit.", "#toolbox"));
    out.push(F(1, "Memory: a 480 x 320 frame buffer is 300 KB and sample voices need buffers. The Daisy has external RAM; the product board must have it too, and it is not in the parts list yet."));
    const dq = (P && P.decisions || []).filter(x => x.status === "open").length; if (dq) out.push(F(1, dq + " decisions are still open.", "#decisions"));
  } else {
    if (!S || !S.units) return [F(1, "The console server is not answering.")];
    S.units.forEach(u => { const fs = u.render ? u.render.files : [];
      if (!u.sim) out.push(F(1, u.name + ": the simulator is not built here. Run Check all.", "#units"));
      if (!fs.length) out.push(F(1, u.name + ": no renders. Nothing should be flashed before a render is measured.", "#units"));
      else if (fs.some(f => f.verdict === "flag")) out.push(F(2, u.name + ": a render is flagged. The flash will stop on it.", "#analyzer")); else out.push(F(0, u.name + ": simulated and measured, " + fs.length + " renders, none flagged."));
      out.push(F(0, u.name + ": latest logged build is " + (u.notes ? u.notes.build + " on " + u.notes.date : "none") + ". If you flashed since, log it from the Units page.")); });
    const g = AG.info && AG.info.git; if (g) out.push(g.dirty ? F(1, g.dirty + " files changed and not committed (" + g.files.slice(0, 4).join(", ") + (g.dirty > 4 ? ", ..." : "") + "). Commit so the plan and designs are safe: git add -A && git commit -m \"...\" && git push.") : F(0, "The repo is clean. Last commit: " + g.last + "."));
    if (usb && usb.found) out.push(usb.found.length > 1 ? F(2, usb.found.length + " FM-1s are on USB. Unplug all but one before flashing.") : F(0, usb.found.length ? "One FM-1 on USB." : "No FM-1 on USB right now."));
    out.push(F(0, "Order for a flash day: Check all → Analyzer → one unit on USB → Flash → play it → Log this flash → commit and push. Then the other unit."));
    if (S.online) out.push(F(1, "This is the online console: running, flashing and USB are off here. Do those on your own machine."));
  }
  return out;
}
WS.agents = async function () {
  $("#main").innerHTML = '<h1>Agents</h1><p class="lede">Four helpers that read the workshop: your designs, the plan, the parts findings, the latest renders and the repo. Each gives a review you can act on now, and answers questions when the console has an API key.</p><div id="agBox">Loading...</div>';
  await loadAll(); try { AG.info = await (await api("/api/agents")).json(); } catch (e) { AG.info = { llm: false }; }
  if (!usb) { try { await checkUsb(); } catch (e) {} }
  agDraw();
};
function agDraw() {
  const M = AG_META[AG.who], rv = agReview(AG.who), th = AG.threads[AG.who], llm = AG.info && AG.info.llm, des = (P && P.designs) || [];
  const n2 = rv.filter(r => r.lv === 2).length, n1 = rv.filter(r => r.lv === 1).length;
  $("#agBox").innerHTML = '<div class="agpick">' + Object.keys(AG_META).map(k => { const r = agReview(k), a = r.filter(x => x.lv === 2).length, b = r.filter(x => x.lv === 1).length; return '<button data-ag="' + k + '" class="agcard' + (k === AG.who ? " on" : "") + '"><b>' + AG_META[k].name + "</b><small>" + AG_META[k].tag + '</small><span>' + (a ? '<i class="pill flag">' + a + " to fix</i> " : "") + (b ? '<i class="pill watch">' + b + " to look at</i>" : (!a ? '<i class="pill clean">all good</i>' : "")) + "</span></button>"; }).join("") + "</div>" +
    '<div class="dz" style="margin-top:18px"><div><div class="card"><h2 style="font-size:18px">' + M.name + " review<small>" + (n2 + n1 ? n2 + " to fix · " + n1 + " to look at" : "nothing to raise") + "</small></h2>" + ((AG.who === "prototype" || AG.who === "hardware") && des.length > 1 ? '<label class="hint">Design <select id="agDes">' + des.map(x => '<option' + (x.name === (AG.design || des[0].name) ? " selected" : "") + ">" + esc(x.name) + "</option>").join("") + "</select></label>" : "") +
    '<div class="findings">' + rv.map(r => '<div class="finding l' + r.lv + '"><i></i><div>' + esc(r.t) + (r.go ? ' <a href="' + r.go + '">Open</a>' : "") + "</div></div>").join("") + '</div><div class="hint">This review is built into the console: fixed rules over your own data, no network. It runs again each time you open this page.</div></div></div>' +
    '<div><div class="card"><h2 style="font-size:18px">Ask ' + M.name + "<small>" + (llm ? esc(AG.info.model) : "chat is off") + '</small></h2><div class="thread" id="agThread">' + (th.length ? th.map(m => '<div class="msg ' + m.role + '">' + esc(m.content).replace(/\n/g, "<br>") + "</div>").join("") : '<div class="hint">' + (llm ? "Ask anything about the " + M.tag + ". The agent sees the same data as the review." : "No API key is set, so the agents cannot answer questions yet. The review on the left works without one.") + "</div>") + (AG.busy ? '<div class="msg assistant">thinking…</div>' : "") + "</div>" +
    (llm ? '<div class="chips">' + M.ask.map(q => '<button class="sm" data-askq="' + esc(q) + '">' + esc(q) + '</button>').join("") + '</div><div class="row" style="margin:0;align-items:flex-end"><textarea id="agIn" rows="2" placeholder="Ask ' + M.name + '…" style="flex:1;min-width:200px" ' + (AG.busy ? "disabled" : "") + '></textarea><button class="primary" id="agSend" ' + (AG.busy ? "disabled" : "") + '>Send</button></div><div class="row" style="margin:0"><button class="sm" id="agKeep" ' + (th.length ? "" : "disabled") + '>Save the last answer to notes</button><button class="sm" id="agClear" ' + (th.length ? "" : "disabled") + '>Clear</button><span class="hint" id="agMsg">Questions and the workshop data shown in the review are sent to the Anthropic API.</span></div>'
      : '<div class="note"><b>To turn chat on</b>, start the console with a key from your Anthropic account:<pre class="mono" style="margin:8px 0 0;white-space:pre-wrap">export ANTHROPIC_API_KEY=sk-ant-...\ntools/sfa console</pre>The key stays in that terminal; the page never sees it. API use is billed to your account.</div><div class="row" style="margin:0"><button class="tool" id="agCopy">Copy a brief for Claude</button><span class="hint" id="agMsg">Copies the same data as text, to paste into any Claude chat with your question.</span></div>') + "</div>" +
    (pget(["agentNotes", AG.who], "") ? '<div class="card ed" style="margin-top:14px"><b>Saved notes from ' + M.name + '</b><textarea id="agNotes" rows="6">' + esc(pget(["agentNotes", AG.who], "")) + "</textarea></div>" : "") + "</div></div>";
  document.querySelectorAll("[data-ag]").forEach(b => b.onclick = () => { AG.who = b.dataset.ag; agDraw(); });
  const on = (s, ev, fn) => { const e = $(s); if (e) e[ev] = fn; };
  on("#agDes", "onchange", e => { AG.design = e.target.value; agDraw(); });
  on("#agSend", "onclick", () => agAsk($("#agIn").value)); on("#agIn", "onkeydown", e => { if (e.key === "Enter" && (e.ctrlKey || e.metaKey)) agAsk($("#agIn").value); });
  document.querySelectorAll("[data-askq]").forEach(b => b.onclick = () => agAsk(b.dataset.askq));
  on("#agClear", "onclick", () => { AG.threads[AG.who] = []; agDraw(); });
  on("#agKeep", "onclick", () => { const last = th.slice().reverse().find(m => m.role === "assistant"); if (!last) return; pset(["agentNotes", AG.who], (pget(["agentNotes", AG.who], "") + "\n\n" + new Date().toISOString().slice(0, 10) + ": " + last.content).trim().slice(-8000)); agDraw(); });
  on("#agNotes", "onchange", e => pset(["agentNotes", AG.who], e.target.value.slice(0, 8000)));
  on("#agCopy", "onclick", async () => { try { const ctx = await (await api("/api/agentcontext" + (AG.design ? "?design=" + encodeURIComponent(AG.design) : ""))).text(), txt = "You are helping with Sin Fetish Audio (SFA): custom FM-1 firmware and a groovebox prototype. Act as the " + M.name + " reviewer (" + M.tag + "). Be practical and specific, and say when something is an estimate.\n\nBuilt-in review findings:\n" + rv.map(r => "- " + r.t).join("\n") + "\n\nWorkshop data:\n" + ctx + "\n\nMy question: "; await navigator.clipboard.writeText(txt); $("#agMsg").textContent = "Copied. Paste it into a Claude chat and add your question."; } catch (e) { $("#agMsg").textContent = "Could not copy: " + e.message; } });
  const t = $("#agThread"); if (t) t.scrollTop = t.scrollHeight;
}
async function agAsk(q) {
  q = (q || "").trim(); if (!q || AG.busy) return; const who = AG.who, th = AG.threads[who]; th.push({ role: "user", content: q }); AG.busy = true; agDraw();
  try { const r = await fetch("/api/agent", { method: "POST", headers: { "X-SFA": "1" }, body: JSON.stringify({ agent: who, design: AG.design || ((P.designs || [])[0] || {}).name, messages: th }) }); if (!r.ok) throw new Error(r.status === 503 ? "no API key is set on the console" : await r.text()); th.push({ role: "assistant", content: (await r.json()).reply || "(no answer)" }); }
  catch (e) { th.pop(); AG.busy = false; agDraw(); const m = $("#agMsg"); if (m) { m.textContent = "Not sent: " + e.message; m.style.color = "var(--bad)"; } const i = $("#agIn"); if (i) i.value = q; return; }
  AG.busy = false; if (page() === "agents") agDraw();
}
if (WS[page()]) route();
