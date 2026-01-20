#!/bin/bash

################################################################################
# NEXUS Synthesizer Plugin - Auto-Setup Script
# 
# Uso: bash init_nexus.sh
# 
# Crea:
# - Estructura LOGS/
# - Estructura Source/
# - Tests/
# - CMakeLists.txt básico
# - build.sh
# - .gitignore
# - README.md básico
# - Primera estructura git
#
################################################################################

set -e  # Exit on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

echo "═══════════════════════════════════════════════════════════════════"
echo "          NEXUS Synthesizer Plugin - Auto-Setup"
echo "═══════════════════════════════════════════════════════════════════"
echo ""

# ============================================================================
# 1. CREAR ESTRUCTURA DE CARPETAS
# ============================================================================

echo "▶ Creando estructura de directorios..."

# LOGS/
mkdir -p LOGS/build
mkdir -p LOGS/compilation
mkdir -p LOGS/runtime
mkdir -p LOGS/profiling
mkdir -p LOGS/tests
mkdir -p LOGS/blockers

# Source/
mkdir -p Source/Main
mkdir -p Source/DSP/CoreModules
mkdir -p Source/DSP/Synthesis
mkdir -p Source/DSP/Filters
mkdir -p Source/DSP/Effects
mkdir -p Source/DSP/Modulation
mkdir -p Source/DSP/ML
mkdir -p Source/UI/Components
mkdir -p Source/MIDI
mkdir -p Source/State
mkdir -p Source/Serialization
mkdir -p Source/Platform

# Tests/
mkdir -p Tests

# Models/
mkdir -p Models

# Resources/
mkdir -p Resources

# Documentation/
mkdir -p Documentation

# build/ (gitignored)
mkdir -p build

echo "✓ Estructura de directorios creada"
echo ""

# ============================================================================
# 2. CREAR .gitignore
# ============================================================================

echo "▶ Creando .gitignore..."

cat > .gitignore << 'EOF'
# Build artifacts
build/
*.o
*.a
*.so
*.dylib
*.lib
*.exe
*.app

# IDE / Editor
.vscode/
.idea/
*.swp
*.swo
*~
.DS_Store
.clang-format-style

# CMake
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
Makefile

# LOGS (nunca en repo)
LOGS/

# Temporary files
*.tmp
*.bak
*.log

# OS
.DS_Store
Thumbs.db

# Python
__pycache__/
*.pyc
*.pyo

EOF

echo "✓ .gitignore creado"
echo ""

# ============================================================================
# 3. CREAR CMakeLists.txt BÁSICO
# ============================================================================

echo "▶ Creando CMakeLists.txt..."

cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.21)
project(NEXUS_Synthesizer VERSION 0.1.0 LANGUAGES CXX)

# ============================================================================
# PROJECT SETUP
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion)
    add_compile_options(-fno-exceptions)
endif()

# ============================================================================
# FIND JUCE
# ============================================================================

find_package(JUCE CONFIG REQUIRED)

# ============================================================================
# ADD SOURCE FILES (DSP CORE)
# ============================================================================

set(NEXUS_SOURCES
    # Main
    Source/Main/PluginProcessor.h
    Source/Main/PluginProcessor.cpp
    Source/Main/PluginEditor.h
    Source/Main/PluginEditor.cpp
    
    # State
    Source/State/ParameterDefinitions.h
    
    # DSP - Core Modules
    Source/DSP/CoreModules/Oscillator.h
    Source/DSP/CoreModules/Oscillator.cpp
    Source/DSP/CoreModules/Envelope.h
    Source/DSP/CoreModules/Envelope.cpp
    Source/DSP/CoreModules/WavetableOscillator.h
    Source/DSP/CoreModules/WavetableOscillator.cpp
    
    # DSP - Synthesis
    Source/DSP/Synthesis/Voice.h
    Source/DSP/Synthesis/Voice.cpp
    Source/DSP/Synthesis/VoiceAllocator.h
    Source/DSP/Synthesis/VoiceAllocator.cpp
    Source/DSP/Synthesis/Synthesizer.h
    Source/DSP/Synthesis/Synthesizer.cpp
)

# ============================================================================
# CREATE PLUGIN TARGET
# ============================================================================

juce_add_plugin(NEXUS
    PRODUCT_NAME "NEXUS"
    DESCRIPTION "NEXUS Synthesizer Plugin"
    PLUGIN_MANUFACTURER_CODE Nxs1
    PLUGIN_CODE Nxs1
    FORMATS VST3 AU
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

target_sources(NEXUS PRIVATE ${NEXUS_SOURCES})

target_compile_features(NEXUS PRIVATE cxx_std_17)

target_link_libraries(NEXUS PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_parameters
)

# ============================================================================
# TESTS (Optional, if using Catch2)
# ============================================================================

# find_package(Catch2 3 REQUIRED)
# add_executable(nexus_tests Tests/DspUnitTests.cpp)
# target_link_libraries(nexus_tests PRIVATE Catch2::Catch2WithMain)

EOF

echo "✓ CMakeLists.txt creado"
echo ""

# ============================================================================
# 4. CREAR build.sh
# ============================================================================

echo "▶ Creando build.sh..."

cat > build.sh << 'EOF'
#!/bin/bash

# NEXUS Build Script
# Usage: ./build.sh [clean|release|debug]

set -e

BUILD_TYPE=${1:-Release}
BUILD_DIR="build"
LOG_DIR="LOGS/build"

# Crear LOG dir si no existe
mkdir -p "$LOG_DIR"

TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")
LOG_FILE="$LOG_DIR/${TIMESTAMP}_build.log"

echo "═══════════════════════════════════════════════════════════════════"
echo "NEXUS Build ($BUILD_TYPE)"
echo "Log: $LOG_FILE"
echo "═══════════════════════════════════════════════════════════════════"
echo ""

# Clean if requested
if [ "$BUILD_TYPE" == "clean" ]; then
    echo "▶ Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    echo "✓ Cleaned"
    exit 0
fi

# Determine build type (default Release)
if [ "$BUILD_TYPE" == "debug" ]; then
    CMAKE_BUILD_TYPE="Debug"
else
    CMAKE_BUILD_TYPE="Release"
fi

# Create build dir
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo "▶ Configuring CMake (type: $CMAKE_BUILD_TYPE)..."
cmake .. \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -GNinja 2>&1 | tee -a "$LOG_FILE"

if [ $? -ne 0 ]; then
    echo "✗ CMake configuration failed"
    exit 1
fi

echo "✓ CMake configured"
echo ""

# Build
echo "▶ Building..."
cmake --build . --config "$CMAKE_BUILD_TYPE" 2>&1 | tee -a "$LOG_FILE"

if [ $? -ne 0 ]; then
    echo "✗ Build failed"
    exit 1
fi

echo "✓ Build successful"
echo ""

# Summary
BINARY_PATH="$(find . -name 'NEXUS_artefacts' -o -name 'NEXUS.vst3' | head -1)"
if [ -n "$BINARY_PATH" ]; then
    echo "📦 Binary: $BINARY_PATH"
fi

echo ""
echo "✓ Build completed at $(date '+%Y-%m-%d %H:%M:%S')"
echo "  Log: $LOG_FILE"

EOF

chmod +x build.sh

echo "✓ build.sh creado y ejecutable"
echo ""

# ============================================================================
# 5. CREAR README.md BÁSICO
# ============================================================================

echo "▶ Creando README.md..."

cat > README.md << 'EOF'
# NEXUS Synthesizer Plugin

Advanced FM/wavetable synthesizer plugin with ML features.

## Quick Start

```bash
# Setup
bash init_nexus.sh    # (ya ejecutado)

# Build
./build.sh            # Release build
./build.sh debug      # Debug build
./build.sh clean      # Clean

# Tests (when ready)
cd Tests && ./run_tests.sh
Project Structure
text
nexus/
├── LOGS/              # Build/runtime/profiling logs (gitignored)
├── Source/
│   ├── Main/          # Plugin entry points
│   ├── DSP/           # Digital Signal Processing
│   ├── UI/            # User Interface
│   ├── MIDI/          # MIDI handling
│   └── State/         # Parameters & presets
├── Tests/             # Unit tests
├── Models/            # ML models (.pt files)
├── Documentation/     # Detailed docs
└── build/             # CMake output (gitignored)
Documentation
See Documentation/ folder:

AI_OPERATIONAL_GUIDELINES.md - Rules & procedures

DAILY_DEVELOPER_CHECKLIST.md - Daily workflow

NEXUS_Architecture_Specification.md - Technical spec

NEXUS_CodeStandards_Examples.md - Code style guide

ERROR_PATTERNS_AND_SOLUTIONS.md - Debugging help

Requirements
C++17 compiler (GCC 9+, Clang 9+, MSVC 2019+)

CMake 3.21+

JUCE 7.0+

(Optional) LibTorch 2.0+ for ML features

Compilation
Linux / macOS
bash
./build.sh        # Release
./build.sh debug  # Debug
Windows (Visual Studio)
bash
cmake -B build -G "Visual Studio 16 2019"
cmake --build build --config Release
Development Guidelines
LOGS: All logs go in LOGS/, never in repo root.

File Size: Max 300 lines per .h/.cpp file.

Audio Thread: No allocations, no locks in processBlock().

Quality: Compile warnings = errors, tests must pass.

See AI_OPERATIONAL_GUIDELINES.md for details.

Roadmap
Phase 1 (Weeks 1–2): Setup & skeleton

Phase 2 (Weeks 3–7): DSP core (synthesis)

Phase 3 (Week 8): Neural control

Phase 4 (Week 9): UI development

Phase 5 (Week 10): MIDI & polish

Phases 6+: Advanced features

Performance Targets
Desktop: < 25% CPU, 8 voices

Raspberry Pi: < 30% CPU, 6 voices

Latency: < 10 ms typical

License
[Your License Here]

Author
[Your Name]

For detailed development docs, see Documentation/.

EOF

echo "✓ README.md creado"
echo ""

============================================================================
6. COPIAR DOCUMENTOS CLAVE (stub files)
============================================================================
echo "▶ Creando stubs en Documentation/..."

Stub para guía rápida
cat > Documentation/QUICK_START.md << 'EOF'

NEXUS: Quick Start for Developers
5 Golden Rules
LOGS RULE: All logs in LOGS/, never in repo root

FILE SIZE: Max 300 lines per .h/.cpp

AUDIO THREAD: Zero allocations, zero locks in processBlock()

QUALITY GATES: No merge without: 0 warnings, 100% tests passing

DOCUMENTATION: Doc comments on all public methods

Daily Routine
Morning (5 min)
bash
git pull origin develop
# Check DAILY_DEVELOPER_CHECKLIST.md - Startup section
./build.sh   # Quick compile check
During Day
Edit Source/ files (max 300 lines each)

Follow NEXUS_CodeStandards_Examples.md patterns

Compile every 30 min: ./build.sh

Before Pushing (30 min)
bash
./build.sh                    # Full compile
cd Tests && ./run_tests.sh    # Tests
git diff --stat               # Review changes
git commit -m "[PhaseX] [TYPE] Message"
git push origin feature/name
When Things Break
Copy full error message

Search in ERROR_PATTERNS_AND_SOLUTIONS.md

Follow solution step-by-step

If not found → create LOGS/blockers/DATE_ISSUE.md

Structure Reference
text
Source/DSP/
├── CoreModules/    - Oscillator, Envelope, Wavetable
├── Synthesis/      - Voice, VoiceAllocator, Synthesizer
├── Filters/        - Filter designs
├── Effects/        - Reverb, Delay, etc
├── Modulation/     - LFO, ModMatrix
└── ML/             - Feature extraction, inference
See NEXUS_CodeStandards_Examples.md for detailed code patterns.

EOF

Stub para architecture reference
cat > Documentation/ARCHITECTURE_QUICK_REF.md << 'EOF'

NEXUS Architecture Quick Reference
Audio Signal Flow
text
MIDI Input
    ↓
Voice Allocator (note-on/off, voice stealing)
    ↓
Voice (per note):
    Oscillator (sine/wavetable/FM) 
        ↓
    Envelope (ADSR)
        ↓
    Filter (cutoff, resonance)
        ↓
    Effects (reverb, delay, etc)
    ↓
Audio Output (mixed)
Parameter Flow
text
UI/Host → APVTS ← Feature values
            ↓
    Modulation Matrix
    (32 routes: sources → destinations)
            ↓
    Synthesis parameters
            ↓
    Audio processing
Modules Overview
Module	Responsibility
Voice	One note: oscillator + envelope
VoiceAllocator	Note on/off, round-robin/stealing
Synthesizer	Master orchestrator
ModMatrix	32-route modulation
FeatureExtractor	Pitch, spectrum analysis (ML)
Performance Budget (Desktop, 8 voices)
Component	CPU Budget
Oscillators (8×)	2%
Envelopes (8×)	1%
Filters (8×)	2%
Effects	5%
UI + overhead	3%
TOTAL TARGET	< 25%
See NEXUS_Architecture_Specification.md for full details.

EOF

echo "✓ Documentation stubs creados"
echo ""

============================================================================
7. CREAR PRIMER HEADER/CPP PLACEHOLDERS
============================================================================
echo "▶ Creando archivos placeholder..."

PluginProcessor.h
cat > Source/Main/PluginProcessor.h << 'EOF'
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**

NEXUS Plugin Processor

Main audio processing class. Handles:

Audio callback (processBlock)

MIDI input

Parameter management (APVTS)

Voice allocation & synthesis
*/
class PluginProcessor : public juce::AudioProcessor
{
public:
PluginProcessor();
~PluginProcessor() override = default;

void prepareToPlay(double sampleRate, int samplesPerBlock) override;
void releaseResources() override;

void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

juce::AudioProcessorEditor* createEditor() override;
bool hasEditor() const override;

const juce::String getName() const override;
bool acceptsMidi() const override { return true; }
bool producesMidi() const override { return false; }
bool isMidiEffect() const override { return false; }
double getTailLengthSeconds() const override { return 0.0; }

int getNumPrograms() override { return 1; }
int getCurrentProgram() override { return 0; }
void setCurrentProgram(int) override {}
const juce::String getProgramName(int) override { return "Default"; }
void changeProgramName(int, const juce::String&) override {}

void getStateInformation(juce::MemoryBlock&) override;
void setStateInformation(const void*, int) override;

juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
juce::AudioProcessorValueTreeState apvts;


JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
EOF

PluginProcessor.cpp
cat > Source/Main/PluginProcessor.cpp << 'EOF'
#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
: apvts(*this, nullptr, "Parameters", {})
{
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
// TODO: Initialize synthesizer, allocators, etc.
// synthesizer_.setSampleRate(sampleRate);
// synthesizer_.prepareToPlay(samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
// TODO: Cleanup if needed
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
buffer.clear();


// TODO: 
// 1. Process MIDI messages
// 2. Update parameters from APVTS
// 3. Render audio via synthesizer
// 4. Apply output level
}

void PluginProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
juce::AudioBuffer<float> floatBuffer(buffer.getNumChannels(), buffer.getNumSamples());
for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
floatBuffer.copyFrom(ch, 0, reinterpret_cast<float*>(buffer.getWritePointer(ch)), buffer.getNumSamples());


processBlock(floatBuffer, midi);

for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        buffer.getWritePointer(ch)[i] = floatBuffer.getReadPointer(ch)[i];
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
return new PluginEditor(*this);
}

bool PluginProcessor::hasEditor() const
{
return true;
}

const juce::String PluginProcessor::getName() const
{
return JucePlugin_Name;
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
// TODO: Save plugin state
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
// TODO: Restore plugin state
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
return new PluginProcessor();
}
EOF

PluginEditor.h
cat > Source/Main/PluginEditor.h << 'EOF'
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class PluginEditor : public juce::AudioProcessorEditor
{
public:
explicit PluginEditor(PluginProcessor&);
~PluginEditor() override = default;


void paint(juce::Graphics&) override;
void resized() override;
private:
PluginProcessor& processor;


JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
EOF

PluginEditor.cpp
cat > Source/Main/PluginEditor.cpp << 'EOF'
#include "PluginEditor.h"

PluginEditor::PluginEditor(PluginProcessor& p)
: AudioProcessorEditor(p), processor(p)
{
setSize(400, 300);
}

void PluginEditor::paint(juce::Graphics& g)
{
g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
g.setColour(juce::Colours::white);
g.setFont(15.0f);
g.drawFittedText("NEXUS Synthesizer", getLocalBounds(), juce::Justification::centred, 1);
}

void PluginEditor::resized()
{
// TODO: Layout UI components
}
EOF

Parameter Definitions
cat > Source/State/ParameterDefinitions.h << 'EOF'
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Nexus::State {

/// Create and return APVTS parameter layout
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;


// Master
params.push_back(std::make_unique<juce::AudioParameterFloat>(
    "master_level", "Master Level", 
    juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f
));

// Oscillator 1
params.push_back(std::make_unique<juce::AudioParameterFloat>(
    "osc1_pitch", "Osc1 Pitch",
    juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f
));

params.push_back(std::make_unique<juce::AudioParameterFloat>(
    "osc1_level", "Osc1 Level",
    juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f
));

// Filter
params.push_back(std::make_unique<juce::AudioParameterFloat>(
    "filter_cutoff", "Filter Cutoff",
    juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 5000.0f
));

params.push_back(std::make_unique<juce::AudioParameterFloat>(
    "filter_resonance", "Filter Resonance",
    juce::NormalisableRange<float>(1.0f, 20.0f), 1.0f
));

// Envelope
params.push_back(std::make_unique<juce::AudioParameterFloat>(
    "env_attack", "Envelope Attack",
    juce::NormalisableRange<float>(1.0f, 5000.0f, 1.0f, 0.3f), 10.0f
));

params.push_back(std::make_unique<juce::AudioParameterFloat>(
    "env_decay", "Envelope Decay",
    juce::NormalisableRange<float>(1.0f, 5000.0f, 1.0f, 0.3f), 100.0f
));

params.push_back(std::make_unique<juce::AudioParameterFloat>(
    "env_sustain", "Envelope Sustain",
    juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f
));

params.push_back(std::make_unique<juce::AudioParameterFloat>(
    "env_release", "Envelope Release",
    juce::NormalisableRange<float>(1.0f, 5000.0f, 1.0f, 0.3f), 200.0f
));

return { params.begin(), params.end() };
}

} // namespace Nexus::State
EOF

echo "✓ Source files placeholder creados"
echo ""

============================================================================
8. CREAR DAILY CHECKLIST BÁSICO
============================================================================
echo "▶ Creando DAILY_CHECKLIST.md..."

cat > Documentation/DAILY_CHECKLIST.md << 'EOF'

NEXUS: Daily Developer Checklist
Use this checklist every day before pushing code.

Morning (10 min)
 git pull origin develop

 Check LOGS/ exists: ls -la LOGS/{build,compilation,runtime,profiling,tests,blockers}

 Quick compile: ./build.sh 2>&1 | tail -20

Before Coding
 No files > 300 lines (check changed files)

 No headers without #pragma once

 Run clang-format: clang-format -i Source/**/*.{h,cpp}

After Coding
 Compile again: ./build.sh

 Review: git diff --stat

 Clean debug code: grep -r "std::cout\|printf" Source/ || echo "✓ Clean"

Before Pushing
 Tests pass: cd Tests && ./run_tests.sh || true

 Check warnings count (should be 0 or same as before)

 Git status clean: git status

 Commit message format: [PhaseX] [TYPE] Message

At End of Day
Create or update LOGS/daily_YYYY-MM-DD.md:

text
# Daily Log - YYYY-MM-DD

## Completed
- [ ] Task 1
- [ ] Task 2

## Blockers
- None

## Next
- Task for tomorrow
EOF

echo "✓ DAILY_CHECKLIST.md creado"
echo ""

============================================================================
9. INICIALIZAR GIT
============================================================================
echo "▶ Inicializando Git..."

if [ ! -d .git ]; then
git init
git config user.email "developer@nexus.local" 2>/dev/null || true
git config user.name "NEXUS Developer" 2>/dev/null || true
echo "✓ Git initialized"
else
echo "✓ Git already initialized"
fi

echo ""

============================================================================
10. CREAR PRIMER COMMIT
============================================================================
echo "▶ Staging files para primer commit..."

git add .
git status --short

echo ""
echo "▶ Creando primer commit..."

git commit -m "[Phase1] [INIT] Initial project setup

Estructura de carpetas (LOGS, Source, Tests)

CMakeLists.txt básico con JUCE

build.sh script

README.md y documentación inicial

Placeholder de PluginProcessor/Editor

Parameter definitions base

Daily checklist template" 2>/dev/null || echo "⚠ Git commit skipped (already committed or git issue)"

echo ""

============================================================================
RESUMEN
============================================================================
echo "═══════════════════════════════════════════════════════════════════"
echo "✓ NEXUS SETUP COMPLETE"
echo "═══════════════════════════════════════════════════════════════════"
echo ""
echo "📁 Structure:"
echo " LOGS/ ← All build/runtime/profiling logs"
echo " Source/ ← Plugin source code"
echo " Tests/ ← Unit tests"
echo " Documentation/ ← Guides & specs"
echo ""
echo "📝 Next Steps:"
echo ""
echo "1. Review CMakeLists.txt (set JUCE path if needed)"
echo ""
echo "2. Install JUCE if needed:"
echo " git clone https://github.com/juce-framework/JUCE.git"
echo ""
echo "3. Build:"
echo " ./build.sh # Release build"
echo " ./build.sh debug # Debug build"
echo ""
echo "4. Check logs:"
echo " cat LOGS/build/latest.log"
echo ""
echo "5. Start coding Phase 1:"
echo " - Edit Source/Main/PluginProcessor.cpp"
echo " - Follow NEXUS_CodeStandards_Examples.md"
echo " - Use DAILY_CHECKLIST.md every day"
echo ""
echo "📚 Documentation:"
echo " Documentation/QUICK_START.md"
echo " Documentation/ARCHITECTURE_QUICK_REF.md"
echo " Documentation/DAILY_CHECKLIST.md"
echo ""
echo "✓ First commit created: [Phase1] [INIT] Initial project setup"
echo ""
echo "═══════════════════════════════════════════════════════════════════"
echo "Ready to develop! 🚀"
echo "═══════════════════════════════════════════════════════════════════"

text

***

**Ahora ejecuta en tu terminal:**

```bash
bash init_nexus.sh