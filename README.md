# Fenix

A suite of five mix-bus colour processors, built with JUCE 8 as VST3 and LV2 plugins.
Each one is a separate plugin, so you can use them individually or chain them in any order.

| Plugin | What it does | Controls |
|---|---|---|
| **Fenix Drive** | Level-dependent harmonic saturation. A variable-knee sigmoid with DC bias for even harmonics, level-compensated, 4x oversampled. | Drive, Density, Mix, Output |
| **Fenix Shelf EQ** | Two broad shelves with stepped frequencies for tonal tilts. Low-Q shelves, 2x oversampled so the top shelf holds its shape up to 20 kHz. | Low Freq, Low, High Freq, High, Output |
| **Fenix HF Comp** | Compresses only the band above the crossover. LR4 split, stereo-linked peak detector, 4:1 soft knee, fast attack. | Threshold, Crossover, Mix, Output + HF reduction meter |
| **Fenix Width** | Mid/side width, plus "Space": a low-frequency side lift (shuffler-style) for depth without a phasey top end. | Width, Space, Output + correlation meter |
| **Fenix Iron** | Output transformer colour. Only the low band saturates (flux-dependent), with a small LF resonance and gentle HF softening. | Amount, Output |

Suggested chain order on a mix bus: Drive → Shelf EQ → HF Comp → Width → Iron.

## Building

You need CMake ≥ 3.22, a C++20 compiler, a JUCE 8 checkout, and JUCE's Linux dependencies
(ALSA, FreeType, X11, Xrandr, Xinerama, Xcursor).

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR=/path/to/JUCE
cmake --build build -j
```

The built plugins are copied to `~/.vst3` and `~/.lv2` automatically.

## Layout

```
common/    shared processor base, parameter helpers, biquads, generic editor
plugins/   one source file per plugin
```
