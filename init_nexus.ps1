
$ErrorActionPreference = "Stop"

Write-Host "═══════════════════════════════════════════════════════════════════"
Write-Host "          NEXUS Synthesizer Plugin - Auto-Setup (PowerShell)"
Write-Host "═══════════════════════════════════════════════════════════════════"
Write-Host ""

# 1. Directory Structure
Write-Host "▶ Creating directory structure..."
$dirs = @(
    "LOGS/build", "LOGS/compilation", "LOGS/runtime", "LOGS/profiling", "LOGS/tests", "LOGS/blockers",
    "Source/Main", "Source/DSP/CoreModules", "Source/DSP/Synthesis", "Source/DSP/Filters",
    "Source/DSP/Effects", "Source/DSP/Modulation", "Source/DSP/ML", "Source/UI/Components",
    "Source/MIDI", "Source/State", "Source/Serialization", "Source/Platform",
    "Tests", "Models", "Resources", "Documentation", "build"
)
foreach ($d in $dirs) { New-Item -ItemType Directory -Force -Path $d | Out-Null }
Write-Host "✓ Directory structure created"

# 2. .gitignore
Write-Host "▶ Creating .gitignore..."
@'
# Build artifacts
build/
*.o
*.a
*.so
*.dylib
*.lib
*.exe
*.app
*.vst3
*.vst
*.component
*.dll

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
*.ninja

# LOGS
LOGS/

# Temp
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
'@ | Set-Content -Path ".gitignore" -Encoding UTF8
Write-Host "✓ .gitignore created"

# 3. CMakeLists.txt
Write-Host "▶ Creating CMakeLists.txt..."
@'
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
'@ | Set-Content -Path "CMakeLists.txt" -Encoding UTF8
Write-Host "✓ CMakeLists.txt created"

# 4. build.ps1
Write-Host "▶ Creating build.ps1..."
@'
param (
    [string]$BuildType = "Release"
)

$BuildDir = "build"
$LogDir = "LOGS/build"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$Timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"
$LogFile = "$LogDir/${Timestamp}_build.log"

if ($BuildType -eq "clean") {
    Write-Host "Cleaning build..."
    Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
    exit
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Set-Location $BuildDir

$CMakeConfig = if ($BuildType -eq "debug") { "Debug" } else { "Release" }

Write-Host "Configuring CMake ($CMakeConfig)..."
cmake .. -DCMAKE_BUILD_TYPE=$CMakeConfig | Tee-Object -FilePath "..\$LogFile" -Append
if ($LASTEXITCODE -ne 0) { Write-Error "CMake configuration failed"; exit 1 }

Write-Host "Building..."
cmake --build . --config $CMakeConfig | Tee-Object -FilePath "..\$LogFile" -Append
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

Write-Host "Build complete."
Set-Location ..
'@ | Set-Content -Path "build.ps1" -Encoding UTF8
Write-Host "✓ build.ps1 created"

# 5. README.md
Write-Host "▶ Creating README.md..."
@'
# NEXUS Synthesizer Plugin

Advanced neural/hybrid synthesizer.

## Quick Start (Windows)
1. `.\init_nexus.ps1`
2. `.\build.ps1`

See `Documentation/` for more details.
'@ | Set-Content -Path "README.md" -Encoding UTF8

# 6. Documentation Stubs
Write-Host "▶ Creating documentation stubs..."
"See NEXUS_Architecture_Specification.md" | Set-Content -Path "Documentation/QUICK_START.md" -Encoding UTF8
"See NEXUS_Architecture_Specification.md" | Set-Content -Path "Documentation/ARCHITECTURE_QUICK_REF.md" -Encoding UTF8
"Use this checklist everyday." | Set-Content -Path "Documentation/DAILY_CHECKLIST.md" -Encoding UTF8

# 7. Source Files
Write-Host "▶ Creating Source files..."

# PluginProcessor.h
@'
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

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
'@ | Set-Content -Path "Source/Main/PluginProcessor.h" -Encoding UTF8

# PluginProcessor.cpp
@'
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../State/ParameterDefinitions.h"

PluginProcessor::PluginProcessor()
    : apvts(*this, nullptr, "Parameters", Nexus::State::createParameterLayout())
{
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
}

void PluginProcessor::releaseResources()
{
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();
}

void PluginProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> floatBuffer(buffer.getNumChannels(), buffer.getNumSamples());
    processBlock(floatBuffer, midi);
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
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
'@ | Set-Content -Path "Source/Main/PluginProcessor.cpp" -Encoding UTF8

# PluginEditor.h
@'
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
'@ | Set-Content -Path "Source/Main/PluginEditor.h" -Encoding UTF8

# PluginEditor.cpp
@'
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
}
'@ | Set-Content -Path "Source/Main/PluginEditor.cpp" -Encoding UTF8

# ParameterDefinitions.h
@'
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>

namespace Nexus::State {

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "master_level", "Master Level", 
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f
    ));
    
    // Osc 1
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "osc1_pitch", "Osc1 Pitch",
        juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f
    ));

    return { params.begin(), params.end() };
}

} // namespace Nexus::State
'@ | Set-Content -Path "Source/State/ParameterDefinitions.h" -Encoding UTF8

# 8. Git Init
Write-Host "▶ Initializing Git..."
if (-not (Test-Path ".git")) {
    git init
    git add .
    git commit -m "[Phase1] [INIT] Initial project setup (via PowerShell)"
    Write-Host "✓ Git initialized and first commit created"
}
else {
    Write-Host "✓ Git already initialized"
}

Write-Host "═══════════════════════════════════════════════════════════════════"
Write-Host "✓ NEXUS SETUP COMPLETE (PowerShell)"
Write-Host "═══════════════════════════════════════════════════════════════════"
