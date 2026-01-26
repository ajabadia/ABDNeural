cmake_minimum_required(VERSION 3.21)
project(NEURONiK_Synthesizer VERSION 0.1.0 LANGUAGES CXX)

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
# FIND OR INCLUDE JUCE
# ============================================================================

# Allow passing JUCE_PATH from command line/script
if(NOT JUCE_PATH AND EXISTS "C:/JUCE")
    set(JUCE_PATH "C:/JUCE")
endif()

if(JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(STATUS "Including JUCE from local path: ${JUCE_PATH}")
    add_subdirectory("${JUCE_PATH}" JUCE)
else()
    message(STATUS "Searching for JUCE as a system package...")
    find_package(JUCE CONFIG REQUIRED)
endif()

# ============================================================================
# ADD SOURCE FILES (DSP CORE)
# ============================================================================

set(NEURONIK_SOURCES
    # Main
    Source/Main/NEURONiKProcessor.h
    Source/Main/NEURONiKProcessor.cpp
    Source/Main/NEURONiKEditor.h
    Source/Main/NEURONiKEditor.cpp
    
    # UI
    Source/UI/ParameterPanel.h
    Source/UI/ParameterPanel.cpp
    Source/UI/SpectralVisualizer.h
    Source/UI/SpectralVisualizer.cpp
    Source/UI/XYPad.h
    Source/UI/XYPad.cpp
    Source/UI/MidiLearner.cpp
    Source/UI/Panels/OscillatorPanel.h
    Source/UI/Panels/OscillatorPanel.cpp
    Source/UI/Panels/FilterEnvPanel.h
    Source/UI/Panels/FilterEnvPanel.cpp
    Source/UI/Panels/FXPanel.h
    Source/UI/Panels/FXPanel.cpp
    Source/UI/Panels/ModulationPanel.cpp
    Source/UI/Panels/PresetPanel.h
    Source/UI/Panels/PresetPanel.cpp
    
    # UI - Browser
    Source/UI/Browser/PresetBrowser.h
    Source/UI/Browser/PresetBrowser.cpp

    # State
    Source/State/ParameterDefinitions.h
    Source/Serialization/PresetManager.h
    Source/Serialization/PresetManager.cpp
    
    # DSP - CoreModules
    Source/DSP/CoreModules/LFO.cpp
    Source/DSP/CoreModules/Oscillator.h
    Source/DSP/CoreModules/Oscillator.cpp
    Source/DSP/CoreModules/Resonator.h
    Source/DSP/CoreModules/Resonator.cpp
    Source/DSP/CoreModules/Envelope.h
    Source/DSP/CoreModules/Envelope.cpp
    Source/DSP/CoreModules/FilterBank.h
    Source/DSP/CoreModules/FilterBank.cpp
    
    # DSP - Synthesis
    Source/DSP/Synthesis/ResonatorSound.h
    Source/DSP/Synthesis/ResonatorVoice.h
    Source/DSP/Synthesis/ResonatorVoice.cpp
)
message(STATUS "NEURONIK_SOURCES: ${NEURONIK_SOURCES}")

# ============================================================================
# CREATE PLUGIN TARGET
# ============================================================================

juce_add_plugin(NEURONiK
    PRODUCT_NAME "NEURONiK"
    DESCRIPTION "NEURONiK Synthesizer Plugin"
    PLUGIN_MANUFACTURER_CODE Nrnk
    PLUGIN_CODE Nrnk
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    SOURCES ${NEURONIK_SOURCES}
)


# Reverted to standard to avoid linker conflicts with custom options button
# target_compile_definitions(NEURONiK PUBLIC JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)



if(TARGET NEURONiK_Standalone)
    target_sources(NEURONiK_Standalone PRIVATE ${NEURONIK_SOURCES})
endif()

target_link_libraries(NEURONiK PRIVATE
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
)
