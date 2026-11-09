# SFA prototype: small groovebox, $150-250

Planning docs for Sin Fetish Audio's own hardware. `spec.json` is the source of truth; this README is generated from the same data (2026-11-09).

## Goal

A small groovebox, $150-250 retail. Prototype on a Daisy Seed (64 MB SDRAM, stereo codec, SD, 480 MHz M7) running core/ SFA ONE; product path is a custom STM32H750 board reusing the simulator and engine.

Retail about 3x BOM; includes enclosure, assembly, FCC/CE amortisation, shipping and margin. FM-1 street price is about $70, so this is 2-3x the FM-1. Market comparison: Chompi $599, EP-133 about GBP 275.

Honest note: the FM-1 sells for about $70 street. A $150-250 box has to be clearly better, not a different box at the same job.

## Recommended path

1. **Prototype on Daisy Seed** (about $21): lots of RAM for samples, onboard stereo codec, SD, 480 MHz M7, runs `core/` SFA ONE.
2. **Teensy 4.1** ($32) is the fallback if USB audio class does not work on Daisy.
3. **Product board**: custom STM32H750 + TLV320AIC3204 + QSPI flash + SDRAM, reusing the simulator and `core/`.

Simulator first, as always: nothing is flashed that has not been rendered and measured.

## Platforms

| Platform | SoC | Price | Verdict | Main cons |
|---|---|---|---|---|
| Daisy Seed | STM32H750, 480 MHz M7 | $21 | choose | USB audio class 2 unverified; header-pin module, not a finished product; H750 internal flash is 128 KB, firmware runs from QSPI |
| Teensy 4.1 | i.MX RT1062, 600 MHz M7 | $32 | maybe | Needs a separate audio codec board; Audio shield retired at SparkFun |
| Pimoroni Pico Plus 2 | RP2350, dual M33 150 MHz | not verified | maybe | Weaker DSP than M7; Stock was out at one reseller; Price not verified |
| ESP32-S3 board | Xtensa LX7 dual 240 MHz | not verified | no | Weak for 8+ voices; Stock was out at one reseller; Price not verified |
| Raspberry Pi CM5 / Zero 2 W | Linux, Cortex-A | not verified | no | Boot time; Latency tuning; Over budget on CM5; Price not verified |
| Custom STM32H750 board | STM32H750 + TLV320AIC3204 + QSPI + SDRAM | not verified | choose (product stage) | Design and FCC cost; H750 Digi-Key listing showed a 52-week lead time |

## Spec

### Screen

Small colour LCD, reusing the FM-1 UI work.

| Item | Value | Note |
|---|---|---|
| Panel | 1.54in 240x240 IPS, ST7789, SPI | Same controller and resolution as the FM-1, so the UI code ports as is |
| Refresh | SPI at 40-60 MHz with DMA, dirty-rect redraw | Keeps audio interrupts clear |
| Upgrade | 2.0in 320x240 later | Only if the UI needs it |

### Controls

Pads, four encoders, a few buttons.

| Item | Value | Note |
|---|---|---|
| Pads | 16 velocity-sensing pads (FSR matrix or silicone over FSR) | FM-1 has no velocity pads; an FSR matrix is a known DIY pattern (PJRC Teensy project) |
| Encoders | 4 push encoders (the four SFA macros TONE BODY COLOR MOTION) | Matches core/ SFA ONE macros |
| Buttons | 12 tactile: play, rec, shift, page, track, undo... | Estimate |
| Alternative | MPR121 touch pads (no velocity) as in Rheslip Pico-2 Groovebox | Cheaper, worse feel |

### Audio I/O

Stereo out, stereo line in, headphones.

| Item | Value | Note |
|---|---|---|
| Codec | Daisy Seed onboard stereo 24-bit up to 96 kHz (prototype); TLV320AIC3204 on the product board | TLV320: 100 dB DAC SNR, 93 dB ADC SNR (LCSC listing) |
| Out | 2x 3.5mm (or 6.35mm) line out + headphone | Estimate |
| In | stereo line in, mic via preamp later | Sampling from line is the main use |
| USB | USB-C: power, MIDI, mass storage; USB audio class unverified on Daisy | Teensy 4.1 is the USB-audio fallback |

### MIDI

TRS-A in and out plus USB MIDI.

| Item | Value | Note |
|---|---|---|
| TRS MIDI in | Type A, 6N138 optocoupler | FM-1 is MIDI in only on TRS (base commit message) |
| TRS MIDI out/thru | Switchable out or thru | Improvement over FM-1 |
| USB MIDI | Device mode; host mode optional |  |

### Engines

SFA ONE first, then ECB and Alien Stack voices ported.

| Item | Value | Note |
|---|---|---|
| Engine | core/ SFA ONE: exciter, resonator, folder, ladder, 4 voices | Plain C99 float, no allocation: ports directly |
| CPU | Four voices cost about twice ECB's whole render on host | Sizes the processor; 480 MHz M7 with FPU has headroom over the FM-1's pi32v2 at 360 MHz |
| Target | 8-16 voices at 48 kHz, no xruns | Must be measured on hardware |

### Sampler

Play, chop, resample; samples live in RAM and on SD.

| Item | Value | Note |
|---|---|---|
| RAM | 64 MB SDRAM on Daisy | FM-1 store is 184 KB, about 4.2 s of 22.05 kHz mono; 64 MB is minutes at 48 kHz stereo |
| Format | 48 kHz 16/24-bit WAV from SD | Sample Lab exports for it |
| Features | auto-chop, per-pad pitch, reverse, loop, resample the mix |  |

### Sequencer

Step sequencer with 16 steps per pad, parameter locks and song chain.

| Item | Value | Note |
|---|---|---|
| Resolution | 96 PPQN internal, MIDI clock in/out | Estimate |
| Patterns | 64 patterns, 16 tracks | Estimate |
| Reuse | ECB step entry and GEN/BEAT generators | From the existing firmware |

### Storage

microSD for samples, projects and firmware images.

| Item | Value | Note |
|---|---|---|
| Card | microSD via SDMMC (Daisy has an SD interface) | Rheslip Pico-2 Groovebox also uses microSD |
| Internal | QSPI flash for firmware; H750 has only 128 KB internal flash | Product board needs external QSPI |
| Backup | Projects as plain files, drag from computer |  |

### Power

USB-C or one LiPo cell; charge and play.

| Item | Value | Note |
|---|---|---|
| Battery | single-cell LiPo about 2000 mAh | Estimate; runtime must be measured |
| Charger | USB-C charge IC (bq25185 class) | Stock varied at vendors |
| Shipping | UN 38.3 test summary and labels needed to ship cells by air | Cost not stated in source |

### Enclosure

3D-printed first, then injection or CNC.

| Item | Value | Note |
|---|---|---|
| Prototype | FDM or resin print, laser-cut top plate | Estimate $6 |
| Size | about Chompi size, roughly 180x120x30 mm | Chompi page gave price, not size |
| Product | aluminium extrusion or injection ABS; tooling is a large fixed cost | Open question |

### Software

One portable core, many targets, simulator first.

| Item | Value | Note |
|---|---|---|
| Layering | core/ engines + plat.h-style HAL (68 functions on FM-1) + host simulator | Reuse fm1-base, MPL-2.0 |
| Loop | tools/sfa sim -> render -> report before any flash | Standing rule |
| Licence | SFA code own; fm1-base MPL-2.0; check Daisy hardware reuse licence | Unverified |

## Bill of materials

Confidence: `quote` = price seen at a seller; `estimate` = assumed, to be replaced with a quote.

### Prototype

| Ref | Part | Qty | Unit $ | Line $ | Confidence |
|---|---|---|---|---|---|
| U1 | Daisy Seed | 1 | 21 | 21 | quote |
| DS1 | 1.54in 240x240 ST7789 IPS module | 1 | 6 | 6 | estimate |
| P1-16 | FSR + silicone pad, 16 velocity pads | 16 | 1.5 | 24.0 | estimate |
| E1-4 | Push encoder | 4 | 1.2 | 4.8 | estimate |
| SW1-12 | Tactile button | 12 | 0.15 | 1.8 | estimate |
| J1 | TRS MIDI in jack 3.5mm | 1 | 0.8 | 0.8 | estimate |
| J2 | TRS MIDI out jack 3.5mm | 1 | 0.8 | 0.8 | estimate |
| U2 | 6N138 optocoupler | 1 | 0.6 | 0.6 | estimate |
| J3-5 | Audio jacks (out L/R, in, headphone) | 4 | 0.8 | 3.2 | estimate |
| SD1 | microSD breakout | 1 | 3 | 3 | estimate |
| BT1 | LiPo 2000 mAh | 1 | 8 | 8 | estimate |
| CH1 | USB-C LiPo charger module | 1 | 3 | 3 | estimate |
| J6 | USB-C breakout | 1 | 1.5 | 1.5 | estimate |
| EN1 | 3D-printed enclosure + hardware | 1 | 6 | 6 | estimate |
| M1 | Perfboard, wire, headers, misc | 1 | 15 | 15 | estimate |

**Prototype total: $99.50**

### Product estimate (per unit, at about 1000 units)

| Ref | Part | Qty | Unit $ | Line $ | Confidence |
|---|---|---|---|---|---|
| PU1 | STM32H750 (product board) | 1 | 5.2328 | 5.23 | quote |
| PU2 | TLV320AIC3204IRHBR codec | 1 | 0.9696 | 0.97 | quote |
| PU3 | QSPI flash 16 MB + SDRAM 32 MB | 1 | 5 | 5 | estimate |
| PU4 | Assembled 4-layer PCB | 1 | 25 | 25 | estimate |
| PU5 | Display + pads + encoders + buttons | 1 | 30 | 30 | estimate |
| PU6 | Jacks, USB-C, SD socket, power parts | 1 | 12 | 12 | estimate |
| PU7 | Battery + charger IC | 1 | 9 | 9 | estimate |
| PU8 | Enclosure, injection amortised | 1 | 8 | 8 | estimate |
| PU9 | FCC/CE amortised (3-5k USD over 1000 units) | 1 | 4 | 4 | estimate |
| PU10 | Packaging and manual | 1 | 3 | 3 | estimate |

**Product BOM estimate: $102.20** against a BOM target of $80 (retail about 3x). Mostly estimates; only the two chip prices are quotes.

## Improvements over FM-1

| FM-1 limit | Prototype | Evidence |
|---|---|---|
| FM-1 sample store is 184 KB, about 4.2 s at 22.05 kHz mono | 64 MB SDRAM + microSD; 48 kHz stereo samples | base/README.md memory budget; Daisy Seed spec |
| TRS is MIDI in only | MIDI in, out/thru on TRS-A | base commit message |
| 581 KB app slot, 464 KB packed budget | Firmware runs from QSPI, megabytes | base/bsp docs |
| pi32v2 at 360 MHz, ECB at xrun limits with four engines | 480 MHz M7 with FPU, float engines | docs/notes.md Build 4; core/README.md |
| No velocity pads | 16 velocity FSR pads | PJRC FSR matrix project |
| Closed USB audio-class device | Open firmware, SD drag-and-drop | FM-1 base notes |

What FM-1 does better: it exists, it is about $70, and it is small and finished.

## Enclosure ideas

- Print first (FDM/resin) with a laser-cut or milled top plate.
- Chompi-size footprint, about 180x120x30 mm (assumed; the Chompi page gave price only).
- Product: aluminium extrusion or injection ABS; tooling is a large fixed cost.

## Risks

- H750 supply: Digi-Key showed a 52-week lead time.
- Compliance: FCC testing roughly $3,000-5,000; UN 38.3 test summary and labels needed to ship cells by air.
- The $150-250 target against a roughly $70 incumbent.
- Most BOM lines are estimates.

## Open questions

| Question | Why | How to answer |
|---|---|---|
| Does Daisy Seed do USB audio class 2? | Computer recording without a cable matters | Check Electro-Smith libDaisy docs and test a unit |
| Daisy hardware reuse licence? | Needed before a product board copies the Seed design | Read the licence from Electro-Smith; the forum page was not reachable |
| Can the $150-250 retail hit with this BOM? | Retail is roughly 3x BOM; FM-1 street is about $70 | Get real quotes for PCB, assembly and pads at 100 and 1000 units |
| H750 supply risk (52-week lead time at Digi-Key) | A product with no chips is no product | Ask LCSC/Mouser for stock; consider H743 or H723 |
| Velocity pad feel and unit cost | Pads decide the instrument's character | Build 16 FSR pads, test with players |
| FCC and CE route, UN 38.3 for the battery | Gates selling and shipping | Get a test-house quote |
| Battery runtime | Sets the cell size | Measure draw on the Daisy prototype |

## Next steps

1. Buy one Daisy Seed and the parts in the prototype BOM.
2. Port core/ SFA ONE to Daisy behind plat.h and run the renders through the SFA Analyzer.
3. Measure CPU for 8 and 16 voices at 48 kHz.
4. Test USB audio class on Daisy; keep Teensy 4.1 as fallback.
5. Build and play the 16 FSR pad matrix.
6. Get real BOM quotes for the custom H750 board.
7. Decide go/no-go on the product board after the prototype plays.

## Sources

See [SOURCES.md](SOURCES.md).
