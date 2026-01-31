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

# Use JUCE_PATH from environment if not set via CLI
if(NOT JUCE_PATH AND DEFINED ENV{JUCE_PATH})
    set(JUCE_PATH "$ENV{JUCE_PATH}")
endif()

# Fallback to C:/JUCE on Windows if still not found
if(NOT JUCE_PATH AND WIN32 AND EXISTS "C:/JUCE")
    set(JUCE_PATH "C:/JUCE")
endif()

# Handle JUCE as a subdirectory if path is provided and valid
if(JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(STATUS "Including JUCE from local path: ${JUCE_PATH}")
    add_subdirectory("${JUCE_PATH}" JUCE)
else()
    # Otherwise, search for JUCE using find_package (pre-installed or via CMAKE_PREFIX_PATH)
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
    Source/Main/MidiMappingManager.h
    Source/Main/MidiMappingManager.cpp
    
    # UI
    Source/UI/ParameterPanel.h
    Source/UI/ParameterPanel.cpp
    Source/UI/SpectralVisualizer.h
    Source/UI/SpectralVisualizer.cpp
    Source/UI/XYPad.h
    Source/UI/XYPad.cpp
    Source/UI/EnvelopeVisualizer.h
    Source/UI/CustomUIComponents.h
    Source/UI/CustomUIComponents.cpp
    Source/UI/ThemeManager.h
    Source/UI/ThemeManager.cpp
    Source/UI/MidiLearner.h
    Source/UI/MidiLearner.cpp
    Source/UI/Panels/OscillatorPanel.h
    Source/UI/Panels/OscillatorPanel.cpp
    Source/UI/Panels/FilterEnvPanel.h
    Source/UI/Panels/FilterEnvPanel.cpp
    Source/UI/Panels/FXPanel.h
    Source/UI/Panels/FXPanel.cpp
    Source/UI/Panels/ModulationPanel.h
    Source/UI/Panels/ModulationPanel.cpp
    Source/UI/Panels/PresetPanel.h
    Source/UI/Panels/PresetPanel.cpp
    Source/UI/LcdDisplay.h
    Source/UI/LcdDisplay.cpp
    
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
    Source/DSP/CoreModules/ResonatorBank.h
    Source/DSP/CoreModules/ResonatorBank.cpp
    Source/DSP/CoreModules/NeuronikEngine.h
    Source/DSP/CoreModules/NeuronikEngine.cpp
    Source/DSP/CoreModules/NeurotikEngine.h
    Source/DSP/CoreModules/NeurotikEngine.cpp
    Source/DSP/BaseEngine.h
    Source/DSP/BaseEngine.cpp
    
    # DSP - Synthesis
    Source/DSP/IVoice.h
    Source/DSP/ISynthesisEngine.h
    Source/DSP/Synthesis/AdditiveVoice.h
    Source/DSP/Synthesis/AdditiveVoice.cpp
    Source/DSP/Synthesis/NeurotikVoice.h
    Source/DSP/Synthesis/NeurotikVoice.cpp
)

if(MSVC)
    list(APPEND NEURONIK_SOURCES Resources/NEURONiK.rc)
endif()

message(STATUS "NEURONIK_SOURCES: ${NEURONIK_SOURCES}")

# ============================================================================
# SHARED LIBRARY (COMMON)
# ============================================================================

add_library(NEURONiK_Common STATIC
    Source/Common/SpectralModel.h
)

target_link_libraries(NEURONiK_Common PRIVATE
    juce::juce_core
)

target_include_directories(NEURONiK_Common PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Common"
)

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

# WORKAROUND: JUCE is not propagating SOURCES to plugin targets
# Explicitly add sources to all plugin targets
target_sources(NEURONiK PRIVATE ${NEURONIK_SOURCES})

if(TARGET NEURONiK_Standalone)
    target_sources(NEURONiK_Standalone PRIVATE ${NEURONIK_SOURCES})
    
    # Also link libraries if they aren't propagated (safeguard)
    target_link_libraries(NEURONiK_Standalone PRIVATE
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
        NEURONiK_Common
    )
    
    # And include dirs
    target_include_directories(NEURONiK_Standalone PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/Main"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/State"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/Serialization"
    )
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
    NEURONiK_Common
)

# Add include directories for source files
target_include_directories(NEURONiK PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Main"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/State"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Serialization"
)

# ============================================================================
# MODEL MAKER APP (STANDALONE)
# ============================================================================

# Increment Version (Windows)
add_custom_command(
    OUTPUT force_version_update
    COMMAND powershell -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/update_version.ps1"
    COMMENT "Auto-incrementing Model Maker version..."
    VERBATIM
)

# Dummy target to force header update
add_custom_target(UpdateVersion DEPENDS force_version_update)

add_executable(NEURONiK_ModelMaker WIN32
    Source/ModelMaker/Main.cpp
    Source/ModelMaker/MainComponent.h
    Source/ModelMaker/MainComponent.cpp
    Source/ModelMaker/Analysis/SpectralAnalyzer.h
    Source/ModelMaker/Analysis/SpectralAnalyzer.cpp
    Source/DSP/CoreModules/Oscillator.cpp
    Source/DSP/CoreModules/Resonator.cpp
    $<$<PLATFORM_ID:Windows>:Resources/ModelMaker.rc>
)

add_dependencies(NEURONiK_ModelMaker UpdateVersion)

target_link_libraries(NEURONiK_ModelMaker PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    NEURONiK_Common
)
