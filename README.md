# NEURONiK - Advanced Hybrid Synthesizer

**NEURONiK** is a next-generation polyphonic synthesizer plugin built with JUCE. It combines a powerful 64-partial additive synthesis engine with concepts inspired by neural networks to deliver a unique and expressive sound design experience. 

While initially envisioned as a deep-learning-based instrument, NEURONiK has evolved into a highly optimized C++ DSP engine that captures the *spirit* of neural synthesis—sonic complexity, rich harmonic textures, and fluid morphing capabilities—without the overhead of heavy AI frameworks.

**Repository:** [https://github.com/ajabadia/ABDNeural](https://github.com/ajabadia/ABDNeural)

---

## Key Features

*   **Hybrid Additive Engine**: At its core, NEURONiK uses a 64-partial additive resonator that provides precise control over the harmonic content of the sound.
*   **Spectral Morphing**: Seamlessly blend between two different harmonic "models" (Model A and Model B) to create evolving textures and complex timbres.
*   **Sonic Matter Sculpting**: Go beyond traditional synthesis with unique "matter" parameters:
    *   **Inharmonicity**: Stretches the harmonic series to create metallic, bell-like, or dissonant sounds.
    *   **Roughness/Entropy**: Introduces controlled chaos and micro-variations into the sound for a more organic and less sterile character.
*   **Modern UI**: A clean, hardware-inspired user interface with tabbed sections for clear and intuitive control.
*   **Cross-Platform**: Built with JUCE and CMake for portability across Windows, macOS, and Linux.

## Technical Architecture

NEURONiK is built entirely in C++ using the **JUCE framework**. The architecture prioritizes performance and stability:

*   **Custom DSP Engine**: All audio processing is handled by a custom-built DSP engine designed for real-time performance with zero memory allocations on the audio thread.
*   **APVTS-driven State**: Parameter management is centralized using `juce::AudioProcessorValueTreeState` for robust DAW integration, preset handling, and automation.
*   **CMake Build System**: The project uses a modern CMake setup for straightforward compilation on any platform.

## Getting Started

### Prerequisites
*   A C++20 compatible compiler (MSVC, Clang, or GCC).
*   CMake 3.15 or higher.
*   The JUCE framework (the project is configured to find it in `C:\JUCE`).

### Building on Windows

A PowerShell script is provided to manage the build process. Open a PowerShell terminal and run:

```powershell
# Clean and build the plugin (Release configuration)
.\Scripts\manage.ps1 -Task Build -Config Release

# To simply clean the build directory
.\Scripts\manage.ps1 -Task Clean
```

The compiled plugin (`NEURONiK_Standalone.exe`, `NEURONiK.vst3`) will be located in the `build_neuronik/NEURONiK_artefacts/` directory.

## Project Roadmap

The project's future is guided by our [**Master Development Roadmap (ROADMAP.MD)**](DOCS/PLANS/ROADMAP.MD). It outlines the upcoming phases, including SIMD optimization for ARM (Raspberry Pi) and advanced UI features. We welcome contributions and ideas!
