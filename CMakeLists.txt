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
