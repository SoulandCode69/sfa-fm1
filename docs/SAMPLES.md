# Sample sources — Earth Creature Beats

All factory samples are **original, rendered by `tools/render_pack.py`** (numpy synthesis at 44.1 kHz, lowpassed and
written as 22.05 kHz mono WAVs to `assets/pack/`). Sin Fetish Audio releases them as **CC0 1.0 (public domain)**.
No third-party sample packs are included: nothing reachable from the build environment had licensing we could verify
to a production standard. Drop any licensed pack into `assets/pack/<KIT>/` (16 WAVs per kit, file order = pad order;
`assets/pack/808/` for the bass bank, root note in the file name as `_rNN`) and run `python3 tools/pack_samples.py`.

| Bank | Count | Rate | What |
|---|---|---|---|
| CLASSIC | 16 | 22.05 kHz | 808-style: kick, boom, snare, rim, clap, hats, cymbal, toms, cowbell, clave, maracas, conga, sub drop |
| PUNCH | 16 | 22.05 kHz | 909-style: tight kicks, snappy snares, clap, bright hats, ride, toms, rim, bell, shaker, zap, swell |
| LOFI | 16 | 22.05 kHz | crushed kicks, snap, congas, bongo, cowbell, woodblock, tabla, tick |
| 808 | 16 | 11.025 kHz | 808 long/short/dist/boom/punch/soft, sub sine/short/tri, bass plucks, saw bass, FM basses, reese (roots C1 / C2) |

Packed as IMA ADPCM 4-bit into the firmware image (`app/samples_data.c`, ~350 KB), decoded as they play; every
sample normalised to -1 dBFS, trailing silence trimmed. Hopper's bank (user samples) lives in the flash data area
instead, written by SAMPLE + SAVE or `tools/sfa_sample.py`.

## Build 8
Two more kits, TECHNO and HOUSE, rendered by tools/render_pack.py like the others (original synthesis, CC0). The
built kits TRAP, DNB and HIPHOP use no new audio: tools/vkits.py assembles them from the pool by name. The demo user
bank (assets/user: a one-bar break built from the HOUSE kit, a chord stab, a vowel sweep) is tools/demo_bank.py, CC0.

## Alien Stack
REPTILE (16 chromatic one-shots) and ABDUCT (16 processed drum hits) are synthesised by tools/render_visitors.py on
render_pack.py's primitives, original and CC0; the Greys' former default bank (drone, transmission, sweep, choir) is
kept as assets/vuser for the user-bank tool.
