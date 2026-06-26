# BigBubbleMuff

A native **Linux VST3** emulation of the Russian **"Bubble Font" Big Muff Pi**
fuzz/distortion (schematic versions V7C Green/Black Russian and V8 Small Box Black
Russian — circuit-identical). Built in C++20 with [JUCE](https://juce.com) and a
full **Wave Digital Filter** circuit model via
[chowdsp_wdf](https://github.com/Chowdhury-DSP/chowdsp_wdf).

The three controls — **Sustain**, **Tone**, **Volume** — behave like the real
pedal because the model is derived from the actual schematic netlist
([docs/netlist.md](docs/netlist.md)), not a generic distortion curve.

## Status
Phased build (see [CLAUDE.md](CLAUDE.md) and the project plan):
- **Phase A** — per-stage WDF model with WDF diode-pair clippers, input booster,
  passive tone stack and output recovery. **Complete & verified**: builds clean,
  tests pass, pluginval strictness 10 passes, ASan/UBSan clean, binary hardened
  (Full RELRO / NX / PIE / stack canaries).
- **Phase B** — joint BJT+diode R-type nonlinear roots for maximum fidelity
  (planned).

## Build

Requirements: CMake ≥ 3.25, a C++20 compiler (GCC 14 / Clang), Ninja, and Linux
audio/GUI dev packages (ALSA, X11, freetype, fontconfig).

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The VST3 is built to `build/BigBubbleMuff_artefacts/Release/VST3/BigBubbleMuff.vst3`
and copied to `~/.vst3`.

Offline / pinned-local JUCE:
`-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE` (JUCE 8.0.13 checkout).

## Dependencies (pinned by tag + SHA)
| Dependency  | Version | Commit                                     |
|-------------|---------|--------------------------------------------|
| JUCE        | 8.0.13  | `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2` |
| chowdsp_wdf | v1.0.0  | `36b5775555af21f0f417d2bc866ba7b4b2788614` |
| VST3 SDK    | 3.8     | local: `/home/human/third_party/vst3sdk`   |

## License
**GPLv3-or-later** — see [COPYING](COPYING). JUCE is used under its GPLv3 option;
chowdsp_wdf is BSD-3; the Steinberg VST3 SDK under its GPLv3-compatible terms.

## Credits / trademarks
Schematic traced by **Kit Rae** ([BigMuffPage](https://www.bigmuffpage.com)).
Circuit analysis reference: ElectroSmash "Big Muff Pi Analysis". "Big Muff" is a
trademark of Electro-Harmonix and "VST" of Steinberg Media Technologies GmbH; this
is an independent emulation with no affiliation or endorsement.
