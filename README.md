# NEURONiK - Advanced Hybrid Synthesizer

**NEURONiK** is a next-generation polyphonic synthesizer plugin built with JUCE. It combines a powerful 64-partial additive synthesis engine with concepts inspired by neural networks to deliver a unique and expressive sound design experience. 

While initially envisioned as a deep-learning-based instrument, NEURONiK has evolved into a highly optimized C++ DSP engine that captures the *spirit* of neural synthesis—sonic complexity, rich harmonic textures, and fluid morphing capabilities—without the overhead of heavy AI frameworks.

The project is mid-migration (Phase 8 of the roadmap): the DSP core has been de-coupled from
JUCE so the exact same engine runs natively and as WebAssembly (bit-exact parity), and the
plugin interface is moving from native JUCE panels to a web UI (vanilla JS + Vite) served
inside WebView2 through a versioned parameter bridge.

**Repository:** [https://github.com/ajabadia/ABDNeural](https://github.com/ajabadia/ABDNeural)

---

## Key Features

*   **Hybrid Additive Engine**: At its core, NEURONiK uses a 64-partial additive resonator that provides precise control over the harmonic content of the sound.
*   **Spectral Morphing**: Seamlessly blend between two different harmonic "models" (Model A and Model B) to create evolving textures and complex timbres.
*   **Sonic Matter Sculpting**: Go beyond traditional synthesis with unique "matter" parameters:
    *   **Inharmonicity**: Stretches the harmonic series to create metallic, bell-like, or dissonant sounds.
    *   **Roughness/Entropy**: Introduces controlled chaos and micro-variations into the sound for a more organic and less sterile character.
*   **Expressive Control**: Support for **Velocity Curves** (Linear, Soft, Hard) and **Aftertouch** modulation for dynamic performances.
*   **Flexible MIDI Mapping**: A robust CC mapping system with persistence, auto-conflict resolution, and a dedicated "MIDI CONTROL" LCD menu for hardware-style configuration.
*   **Modern UI**: A clean, hardware-inspired user interface — currently a single web canvas with all 70 parameters, model slots A–D and a shared MIDI keyboard strip, rendered with the control family from `@abdsynths/shared` (with a Dark/Light theme switch in the header; the light palette is the suite's measured-contrast `data-theme="light"`). The canvas closes with the GLOBAL & MASTER card at the bottom-left: only the master fader (the host's native range input) plus RANDOM and an EDIT button that opens the right-side drawer with tempo, MIDI and freezes. Short choice lists (engine, LFO sync) render as shared `Segmented` selectors; the modulation ring and the 64-bar spectral visualizer are live via the telemetry channel.
*   **Cross-Platform DSP**: The DSP core is plain C++ built with CMake; the plugin host layer is JUCE. (The definitive web UI layer is Windows-only for now — WebView2.)

## Technical Architecture

*   **JUCE host layer**: `AudioProcessorValueTreeState` for parameters, presets, MIDI mapping and persistence. APVTS remains the single source of truth for parameters.
*   **De-JUCE'd DSP core** (`Source/DSP/**`): a progressive facade (`DspEngineFacade`) over engines with no JUCE includes and deterministic math (no libm), running at a fixed 64-sample control rate. The identical core compiles to WASM for the AudioWorklet.
*   **Web UI** (`WebUI/`): vanilla JS (no framework) built with Vite; shared knobs/sliders/toggles come from `@abdsynths/shared`; the page sounds through an AudioWorklet running the WASM engine.
*   **Parameter bridge**: generated parameter contract (from APVTS into `WebUI/generated/`, never hand-duplicated) plus a versioned wire protocol (`WebUI/contracts/bridge-protocol.json`) enforced by tests on both the C++ and JS sides.
*   **CMake Build System**: Modern CMake setup for straightforward compilation.

## Getting Started

### Prerequisites
*   MSVC (C++20) and CMake 3.15+.
*   The JUCE framework: found via the `JUCE_PATH` environment variable, falling back to `C:\JUCE`.
*   Node.js + pnpm: WebUI build/tests and the browser mode of `start.bat`.
*   emsdk (auto-detected in `C:\emsdk`, or set `EMSDK`): only needed to (re)build the WASM worklet (`build_wasm.bat`).

### Building on Windows

`build.bat` does everything: CMake configure, parameter-contract regeneration, Standalone + VST3 + WebView2 host build, WebUI export, WASM worklet, the full CTest and web test suites, and the bridge E2E selftest. It always ends with a pause and mirrors its console output to `build-last-run.log`.

```bat
build.bat              :: full build + tests + selftest (default build dir: build-reference)
build.bat tests        :: fast mode: contract + test suites only
build.bat noselftest   :: skip the bridge E2E step
build.bat nowasm       :: skip the WASM worklet build
build.bat modelmaker   :: also build ModelMaker (bumps Source\ModelMaker\Version.h)
build.bat <dir>        :: use a different build directory
```

The compiled plugin is located in `build-reference\NEURONiK_artefacts\Release\`:

*   `Standalone\NEURONiK.exe`
*   `VST3\NEURONiK.vst3`

> The legacy `Scripts\manage.ps1` is superseded by `build.bat`.

## ModelMaker (opt-in tool)

`NEURONiK_ModelMaker` is the factory for the resonator's harmonic models — the standalone
companion to the plugin's model slots A–D.

**Workflow:** LOAD AUDIO (any sample) → spectral analysis extracting 64 partials with pitch
detection → A/B by ear (PLAY ORIGINAL vs PLAY MODEL — the model is synthesized with the same
DSP the plugin uses: it shares `Oscillator`/`Resonator` with the engine) → EXPORT MODEL
writes a `*.neuronikmodel` file, plain JSON (`{amplitudes[64], frequencyOffsets[64], name,
description}`). Those files are what the A–D slots in the plugin load — and what the XY pad
morphs between.

**Why it is opt-in:** it is a separate deliverable, kept out of the default build on
purpose. Its version (`Source\ModelMaker\Version.h`, a versioned file) only moves on
RELEASE builds: a plain verification compile never touches it. Build it with:

```bat
build.bat modelmaker            :: builds the exe; Version.h stays untouched
build.bat modelmaker release    :: release build: increments Version.h (commit it)
```

Run it after touching the shared DSP core (it compiles the same resonator code). Two house
notes:

*   The plugin reads both the JSON dialect the ModelMaker writes and a legacy XML one
    (`Tests/ModelSlotTest.cpp`); the bridge selftest writes its four test models in the
    ModelMaker's JSON dialect on purpose — if the plugin ever stopped reading that format,
    the E2E suite would fail loudly.
*   A future web migration is sketched as **Phase 9** in `ROADMAP.md`.

## Running the Web Version

`start.bat` (compile first with `build.bat` if artifacts are missing):

1.  **WebView2 bench (recommended)** — launches the `NEURONiK Web Pilot.exe` host: it serves `WebUI\dist` with the live JUCE<->WebUI bridge, exactly the same page the plugin embeds. (The "Web Pilot" name is documented cosmetic debt from the retired pilot; see `DOCS/PILOT_RETIRED.md`.)
2.  **Browser only** — serves `WebUI\dist` at `http://localhost:8399`; without the bridge the page runs in LOCAL MODE (handy for debugging the UI on its own).
3.  **Bridge selftest** — automated E2E over the real WebView2 channel in six directions (mod matrix, native→JS, JS→native, GENERAL, MIDI, models A–D); exit code 0 = OK.

## WebUI Development

*   `WebUI/` is vanilla JS built with Vite. From `WebUI/`: `pnpm install`, `pnpm test` (vitest + jsdom), `pnpm build`.
*   Parameter names/ranges/defaults are never duplicated by hand: the contract is generated from the APVTS into `WebUI/generated/`.
*   The bridge wire format has its own versioned contract (`WebUI/contracts/bridge-protocol.json`) checked by contract tests in C++ and JS.
*   The worklet engine comes from `build_wasm.bat` (or `pnpm sync:wasm` / `WebUI/scripts/sync-wasm.mjs` to refresh `WebUI/public/worklet/`). Bit-exact parity with the native engine is asserted by `Tests/neuronik_wasm_parity.mjs` across sample rates and block sizes.

## Testing

*   **C++**: `ctest --test-dir build-reference -C Release` — includes `NEURONiK_DSPReferenceTest` (RMS/peak reference render), the bridge protocol contract test and the WebView2 host selftest.
*   **Web**: `pnpm test` inside `WebUI/`.
*   House rule: a step without its test doesn't count as done.

## Documentation

| Doc | What it is |
|---|---|
| `HANDOFF.md` | Running, append-only log of every decision and step (newest entries at the end) |
| `ROADMAP.md` | Live development plan — Phases 0–8; Phase 8 is the definitive WebView2 interface |
| `DOCS/BRIDGE_PROTOCOL.md` | The JUCE<->WebUI bridge protocol |
| `DOCS/PILOT_RETIRED.md` | Why the Next.js pilot was retired, and what replaced it |
| `DSP_PARAMETERS.md` | The DSP parameter contract |
| `WebUI/README.md` | WebUI architecture and status |

## Project Roadmap

The project's future is guided by our [**Master Development Roadmap (ROADMAP.md)**](ROADMAP.md). Phase 8 (current): the definitive web interface in WebView2 — 8.3 adds everything that is not "a parameter" (preset browser with tags, LCD + MIDI CC menu, spectral visualizer, drawn XYPad, MIDI learn), 8.4 retires the native JUCE panels, and 8.5 validates the VST3 with an embedded host. Windows-only for now (WebView2); a future macOS entry point would be `juce::WebBrowserComponent`.

We welcome contributions and ideas!
