# Fenix

A suite of five mix-bus colour processors by **fenrikos**, built with JUCE 8 as VST3 and LV2 plugins.
Each one is a separate plugin with its own live display, analogue-style VU metering and factory presets.
Use them individually or chain them in any order.

![Fenix Drive](docs/drive.png)

| Plugin | What it does | Display |
|---|---|---|
| **Fenix Drive** | Level-dependent harmonic saturation. A variable-knee sigmoid with DC bias for even harmonics, level-compensated, 4x oversampled. | Live transfer curve, with dots tracking the input peak |
| **Fenix Shelf EQ** | Two broad shelves with stepped frequencies for tonal tilts. Low-Q shelves, 2x oversampled so the top shelf holds its shape up to 20 kHz. | EQ response over a real-time spectrum |
| **Fenix HF Comp** | Compresses only the band above the crossover. LR4 split, stereo-linked peak detector, 4:1 soft knee, fast attack. | Spectrum with the compressed band lit, scrolling gain-reduction history, GR needle meter |
| **Fenix Width** | Mid/side width, plus "Space": a low-frequency side lift (shuffler-style) for depth without a phasey top end. | Goniometer with phosphor persistence, correlation bar |
| **Fenix Iron** | Output transformer colour. Only the low band saturates (flux-dependent), with a small LF resonance and gentle HF softening. | Output scope and the core's magnetisation curve |

Suggested chain order on a mix bus: Drive → Shelf EQ → HF Comp → Width → Iron.

<p>
  <img src="docs/shelf.png" width="49%" alt="Fenix Shelf EQ">
  <img src="docs/hfcomp.png" width="49%" alt="Fenix HF Comp">
  <img src="docs/width.png" width="49%" alt="Fenix Width">
  <img src="docs/iron.png" width="49%" alt="Fenix Iron">
</p>

## Features

- **Hardware-style faceplate.** Knurled knobs with lit tick rings, stepped switches for fixed frequencies, and backlit VU meters with needle ballistics (0 VU = −18 dBFS).
- **Presets.** Every plugin ships factory presets, which hosts also see as programs. Use **SAVE** to store your own; they go to `~/.config/fenrikos/Fenix/<plugin>/`.
- **Bypass.** The **IN** button crossfades to a latency-compensated dry path, so engaging or bypassing never clicks or shifts timing. It is also exposed as the host's bypass parameter.
- Double-click any knob to reset it.

## Building

You need CMake ≥ 3.22, a C++20 compiler, a JUCE 8 checkout, and JUCE's Linux dependencies
(ALSA, FreeType, X11, Xrandr, Xinerama, Xcursor).

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR=/path/to/JUCE
cmake --build build -j
```

The built plugins are copied to `~/.vst3` and `~/.lv2` automatically.

### Screenshots

`-DFENIX_SNAPSHOTS=ON` also builds a `<Plugin>_snapshot` tool for each plugin. It runs the editor against
a synthetic mix and saves the result as a PNG, optionally with a factory preset loaded:

```sh
./build/Drive_snapshot docs/drive.png 3
```

## Layout

```
common/    processor base (bypass, metering, presets), DSP helpers, look and feel, widgets, editor
plugins/   one source file per plugin: DSP, presets and its display
tools/     editor snapshot tool
```
