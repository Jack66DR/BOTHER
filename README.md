# BOTHER

**manners! DSP** tremolo with a temper. AU and VST3, macOS universal (Apple Silicon and Intel).

Seven shapes, a knob that rounds them off or sharpens them into a chop, and a second LFO that leans on the rate so the wobble speeds up, slows down and never quite settles. Then dirt: drive before the wobble, grit after it.

## Build

GitHub Actions builds it on every push to `main` (Actions tab, "Build BOTHER"). It also runs the offline engine test and Apple's `auval` check.
Download the `BOTHER-macOS` artifact from the finished run. Inside are `BOTHER-AU.zip` and `BOTHER-VST3.zip`.

## Install

Unzip `BOTHER-AU.zip`, then in Terminal:

```sh
sudo cp -R ~/Downloads/BOTHER.component /Library/Audio/Plug-Ins/Components/
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/BOTHER.component
```

Then in Logic: Plug-in Manager, find manners! DSP > BOTHER, and Reset & Rescan Selection if it doesn't appear.
The build is ad hoc signed, not notarised, which is why the `xattr` line is needed.

## Signal flow

```
input ─► IN ─► DRIVE ─► TREMOLO ─► GRIT ─► TONE ─► MIX ─► OUT
  │                       ▲                         ▲
  │       SHAPE ─► EDGE ──┤                         │
  │       DRIFT ─► RATE ──┘                         │
  └──────────────────────── dry ────────────────────┘
```

DRIVE sits before the wobble, so the chop cuts into the dirt rather than the dirt smoothing over the chop.
GRIT sits after it, so the quiet troughs sputter: fewer bits matter most when the signal is small.

## Controls

| Control | What it does |
|---|---|
| **TREMOLO** | |
| SHAPE | SINE, TRIANGLE, SQUARE, RAMP UP, RAMP DOWN, STEPS (a new random level each cycle) and WANDER (a smooth random curve). |
| EDGE | Centre is the pure shape. Right sharpens: a sine becomes a hard on/off chop. Left rounds: corners melt, ramps soften and STEPS glides between its levels. Rounding keeps the depth; it doesn't just make things shallower. |
| RATE | 0.05 to 80 Hz. Past about 20 Hz it stops being a wobble and turns into a ring-mod-ish growl. |
| BEAT | Takes RATE's place when SYNC is lit: 4 bars down to 1/32, with dotted and triplet values. |
| DEPTH | How far down each dip goes. 100 % goes all the way to silence. |
| SPREAD | The right channel's offset, 0 to 180°. At 180° it's an auto-pan. |
| SYNC | Follows the host tempo. While the song plays and DRIFT AMOUNT is at zero, the wobble is locked to the grid. With drift on, it starts on the beat and then wanders off it, which is the point. |
| HARMONIC | The brownface trick: the signal splits at 700 Hz and the lows and highs pulse in opposite directions, so it shimmers instead of chopping. |
| **DRIFT** | A second, slower LFO that bends the tremolo's rate. |
| SPEED | 0.02 to 8 Hz. |
| AMOUNT | How far it bends RATE, up to three octaves either way. |
| CURVE | The drift's own shape, from the same seven. SQUARE jumps between two speeds; STEPS picks a new speed every cycle. |
| SWAY | Lets the drift pull DEPTH too: when it slows the wobble down, it also makes it shallower. |
| **DIRT** | |
| GRIT | Rasp, fewer bits and a lower sample rate on one knob. |
| TONE | Tilt around 900 Hz, 8 dB either way at the ends. |
| DRIVE | Up to 27 dB into a slightly lopsided tanh, with most of the level handed back, so it adds bite rather than volume. Anti-aliased. At zero it's a clean wire. |
| **OUTPUT** | |
| IN / OUT | Input and output level. |
| MIX | Dry/wet. A tremolo usually lives at 100 %; lower it to keep some of the dry signal under the dips. |

## The face

The manners! house face, worn card and all. BOTHER prints in sunburnt coral, with the live light in seasick mint.

The MODULATION display is a window cut into the card:

- **Left**: two cycles of the shape exactly as EDGE has it, printed.
- **Right**: the gain you are actually hearing over the last 2.5 seconds, lit. Drift, sway and sync all show up here as they happen.
- **Top right**: FREE, or the BEAT and whether it's LOCKED to the grid, and the rate in Hz right now.
- **Bottom**: the drift rail. The needle leans toward SLOWER or FASTER as the drift bends the rate.

Turn a knob and the name strip shows its value. The preset is in the bottom right; click its arrows to step through.

## The menu (three bars, top right)

The same menu as every manners! plugin.

- **Load preset**: ten factory presets (MILD BOTHER, SURF SHACK, BROWNFACE, CHOPPER, SEASICK, PANIC ROOM, RING THE BELL, WASP, SLOW BREATHING, BROKEN AMP), and yours.
- **Save preset**: yours go in `~/Music/manners! DSP/BOTHER/Presets` as `.manners` files.
- **Previous / Next preset**, **Undo / Redo**, **Copy / Paste settings** (between instances), **Init**, **Randomise** (shoves a handful of knobs somewhere unexpected; IN, OUT, MIX, RATE's sync and BEAT are left alone).
- **Window size**: 75, 100, 125 or 150 %. The window also drags to any size.

Factory presets leave IN and OUT where you set them. Logic's own Settings menu still saves and recalls everything as usual.

## Notes

- No latency, no tail, about 1 % of one core.
- EDGE's rounding is a smoother, so a heavily rounded wobble runs a touch behind the beat in SYNC. At the fully rounded end that is about a third of a cycle.
- Font: Barlow Semi Condensed, SIL Open Font License (see `Resources/BarlowSemiCondensed-OFL.txt`).
- The shared face lives in `Source/manners/`, copied from CREPUSCULAR unchanged.

## Local test build (optional)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBOTHER_BUILD_TESTS=ON
cmake --build build --target BOTHER_Test BOTHER_Tool
./build/BOTHER_Test_artefacts/Release/BOTHER_Test
./build/BOTHER_Tool_artefacts/Release/BOTHER_Tool render demo.wav broken_amp
./build/BOTHER_Tool_artefacts/Release/BOTHER_Tool snapshot face.png chopper
./build/BOTHER_Tool_artefacts/Release/BOTHER_Tool presets
```
