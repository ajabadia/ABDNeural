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

set(NEXUS_SOURCES
    # Main
    Source/Main/NEXUSProcessor.h
    Source/Main/NEXUSProcessor.cpp
    Source/Main/NEXUSEditor.h
    Source/Main/NEXUSEditor.cpp
    
    # UI
    Source/UI/ParameterPanel.h
    Source/UI/ParameterPanel.cpp
    Source/UI/SpectralVisualizer.h
    Source/UI/SpectralVisualizer.cpp
    
    # State
    Source/State/ParameterDefinitions.h
    
    # DSP - CoreModules
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
message(STATUS "NEXUS_SOURCES: ${NEXUS_SOURCES}")

# ============================================================================
# CREATE PLUGIN TARGET
# ============================================================================

juce_add_plugin(NEXUS
    PRODUCT_NAME "NEXUS"
    DESCRIPTION "NEXUS Synthesizer Plugin"
    PLUGIN_MANUFACTURER_CODE Nxs1
    PLUGIN_CODE Nxs1
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    SOURCES ${NEXUS_SOURCES}
)

# Custom Standalone App Support - Force all sources into Standalone target
# to resolve persistent linkage issues with createPluginFilter and dependencies.
if(TARGET NEXUS_Standalone)
    target_sources(NEXUS_Standalone PRIVATE ${NEXUS_SOURCES})
endif()

# Reverted to standard to avoid linker conflicts with custom options button
# target_compile_definitions(NEXUS PUBLIC JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)

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
)
