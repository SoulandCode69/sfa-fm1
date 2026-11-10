# The SFA prototype: direction

Set on 2026-11-10. The live version of this page is the console's **Direction** and **Designer** pages; the data is in
`hardware/prototype/direction.json`, and our choices are in `hardware/prototype/plan.json`. Nothing here is built or measured yet.

## The brief

1. **A bit bigger than the FM-1.** Size follows the keys.
2. **Comfortable keys.** The FM-1 keys are too small. Velocity sensitive, two octaves (25 keys).
3. **Pads and keys, both.** Sixteen velocity pads and a keybed.
4. **A colour screen** big enough for lists, a 16-step grid and a waveform. Working choice: 3.5 inch IPS, 480 x 320.
5. **Its own look.** Not a 4 x 4 pad block under a screen.
6. **Portable, but playability first.**

## Layout concepts

Starting points in the console's Designer, where every part can be dragged, resized, styled and saved as a design (a fourth concept, Rails, puts the pads in two columns at the edges). Printable at 1:1. Sizes below are for 25 keys at an 18 mm key pitch with a 3.5 inch screen.

| Concept | Idea | Size | For | Against |
|---|---|---|---|---|
| **Step Row** | 16 pads in one row above the keys, screen in the middle, two encoders each side | about 299 x 200 mm | smallest; pads read as steps 1 to 16 | narrow pads |
| **Butterfly** | screen in the centre, 8 pads on each side, encoders under the screen | about 359 x 192 mm | full-size pads, symmetrical, unlike anything else | widest |
| **Terrace** | 2 x 8 pads on the left, screen and encoders on the right | about 355 x 192 mm | full-size pads, editing in one place | asymmetric |

Key pitch is the number that sets everything. A full-size piano key is about 23.5 mm; slim and mini keys are smaller and vary by
maker. We have no measured FM-1 dimensions on file: measure the unit and enter them in the console.

## Controls

25 keys, 16 pads, 4 endless encoders with push (the engines have four parameters per page), about 12 buttons, a volume knob,
and possibly one touch strip. Scanning: 50 key contacts in a matrix, pads through one 16-channel analogue mux, 12 encoder lines.

## Screen

Must show: the engine page (name, preset, four parameters), a preset and pack browser, a 16-step grid for up to 8 tracks, a
waveform with slice markers, the mixer, the song, and a status line. IPS, at least 30 frames a second, no touch required.

## Sound

The same engine sources as the FM-1 firmwares, at 48 kHz. Samples on microSD (minutes, not 4.2 seconds), 16 sample voices,
chop on the box. Sequencer: 16 to 64 steps, per-step velocity and parameter locks, song mode, real-time record, MIDI clock.

## I/O and power

Stereo line out and headphones, stereo line in, TRS-A MIDI in and out, USB-C (power, MIDI, the card as a drive), microSD
reachable from outside, one Li-ion cell, target 6 hours.

## What the parts search found (2026-11-10)

* **Keybed**: no standalone 25-key velocity mini keybed was found for sale as a part with a price. The first prototype should
  reuse the keybed from a cheap controller (Nektar SE25 at $49.99, listed out of stock; Roland K-25m at about $93 to $99).
* **Pads**: a bought force sensor per pad is too dear (Adafruit square FSR $5.95 each, about $95 for 16). A printed FSR
  matrix or silicone pads over a PCB is the direction; no priced ready-made set was found.
* **Screen**: 3.5 inch IPS ILI9488 at $8.34 per 100 (BuyDisplay), 4 inch ST7796S SPI at $13.50 for one (Elecrow). Over SPI at
  40 MHz a full 480 x 320 frame is roughly 16 a second, so partial updates or a parallel bus are needed for 30.
* **Power**: a bq25185 charger plus 5 V boost board at $7.16 per 100 (Adafruit). 3000 mAh x 3.7 V = 11.1 Wh, about 6.3 hours at
  1.5 W and 85 % efficiency. The 1.5 W is an assumption until measured.
* **Price**: an $80 BOM looks too tight with keys, pads, screen and battery. The estimate is $100 to $130, which at 2.5 to 3
  times BOM is about $250 to $330 retail; $250 is reachable only with a cost-down design and a thin margin. This is an open decision.

Every price above was read on a vendor page on that date except where marked as an estimate; the pages are listed in
`hardware/prototype/PARTS-SOURCES.md`.

## How things move: console, FM-1, box

1. **Design in the console**: engine code, samples (Sample Lab), patterns (Patterns), preset tags (Presets).
2. **Prove it in the simulator**: `tools/sfa sim`, render, measure in the Analyzer.
3. **Flash the FM-1**: `tools/sfa up`. Samples go as the bank file; presets are compiled in today.
4. **Pack it**: presets, kits, samples and patterns as a `.sfapack` (plain files, no code). See FORMATS.md.
5. **Run it on the box**: the same engine source at 48 kHz; packs on the card; the same test script on host and board.

## Open questions

Key pitch; which layout; where the keybed comes from; FSR or switch pads; whether the screen reaches 30 frames a second on the
dev kit's free pins; whether USB can do MIDI, card-as-drive and charging together; whether the price range holds; what it draws.
Each has a "how to answer" and a status in the console.
