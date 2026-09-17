cmake_minimum_required(VERSION 3.21)
project(NEURONiK_Synthesizer VERSION 0.1.0 LANGUAGES CXX)

# ============================================================================
# PROJECT SETUP
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable warnings
if(MSVC)
    # /MP: compilacion en paralelo a nivel de fichero dentro de cada proyecto
    # (sin ella cl.exe compila los .cpp en secuencial dentro de cada vcxproj).
    add_compile_options(/W4 /MP)
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

# Lista DSP compartida con el build WASM (single-source, anti-drift):
include("${CMAKE_CURRENT_SOURCE_DIR}/DspSources.cmake")

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

    # MIDI
    Source/Main/MidiChannelFilter.h
    Source/Main/MidiChannelFilter.cpp
    Source/Main/VelocityCurve.h
    Source/Main/VelocityCurve.cpp
    Source/Serialization/PresetManager.h
    Source/Serialization/PresetManager.cpp
    Source/Serialization/PresetMigration.h
    Source/Serialization/PresetMigration.cpp
    
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
    
    # DSP - Musical time
    Source/DSP/CoreModules/RhythmicDivision.h

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
    # midiThru turns the engine's MIDI output on and off, so the capability has
    # to be declared even though the default is off.
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    SOURCES ${NEURONIK_SOURCES}
)


# Reverted to standard to avoid linker conflicts with custom options button
# target_compile_definitions(NEURONiK PUBLIC JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)

# WORKAROUND: JUCE is not propagating SOURCES to plugin targets
# Explicitly add sources to all plugin targets
target_sources(NEURONiK PRIVATE ${NEURONIK_SOURCES})

target_compile_definitions(NEURONiK PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)

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
# DSP REFERENCE TESTS
# ============================================================================

enable_testing()

add_executable(NEURONiK_DSPReferenceTest
    Tests/DSPReferenceTest.cpp
    ${NEURONIK_DSP_SOURCES}
)

target_include_directories(NEURONiK_DSPReferenceTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
)

target_link_libraries(NEURONiK_DSPReferenceTest PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    NEURONiK_Common
)

add_test(NAME NEURONiK_DSPReferenceTest COMMAND NEURONiK_DSPReferenceTest)

# ============================================================================
# PARAMETER DESCRIPTOR CONTRACT
# ============================================================================

# Single source of truth: descriptors are read back from createParameterLayout()
# so the WebUI can never disagree with the plugin about an ID, range or default.
add_library(NEURONiK_ParameterContract STATIC
    Source/State/ParameterDescriptors.cpp
    Source/State/ParameterDescriptorExport.cpp
)

target_include_directories(NEURONiK_ParameterContract PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/State"
)

target_link_libraries(NEURONiK_ParameterContract PUBLIC
    juce::juce_core
    juce::juce_data_structures
    juce::juce_audio_processors
)

# Regenerates WebPilot/generated/{parameters.generated.json,.js,.d.ts}
add_executable(NEURONiK_ParameterExport Tests/ParameterExportTool.cpp)
target_link_libraries(NEURONiK_ParameterExport PRIVATE NEURONiK_ParameterContract)

# MIDI channel filter: pure MIDI logic, no plugin dependencies needed.
add_executable(NEURONiK_MidiChannelFilterTest
    Tests/MidiChannelFilterTest.cpp
    Source/Main/MidiChannelFilter.cpp
)

target_include_directories(NEURONiK_MidiChannelFilterTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Main"
)

target_link_libraries(NEURONiK_MidiChannelFilterTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_events
)

add_test(NAME NEURONiK_MidiChannelFilterTest COMMAND NEURONiK_MidiChannelFilterTest)

# Velocity curve: pure MIDI shaping, no plugin dependencies needed.
add_executable(NEURONiK_VelocityCurveTest
    Tests/VelocityCurveTest.cpp
    Source/Main/VelocityCurve.cpp
)

target_include_directories(NEURONiK_VelocityCurveTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Main"
)

target_link_libraries(NEURONiK_VelocityCurveTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_events
)

add_test(NAME NEURONiK_VelocityCurveTest COMMAND NEURONiK_VelocityCurveTest)

# Tempo sync: the LFO and the shared rhythmic division table.
add_executable(NEURONiK_LfoSyncTest
    Tests/LfoSyncTest.cpp
    Source/DSP/CoreModules/LFO.cpp
)

target_include_directories(NEURONiK_LfoSyncTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
)

target_link_libraries(NEURONiK_LfoSyncTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_dsp
)

add_test(NAME NEURONiK_LfoSyncTest COMMAND NEURONiK_LfoSyncTest)

add_executable(NEURONiK_ParameterDescriptorTest Tests/ParameterDescriptorTest.cpp)
target_link_libraries(NEURONiK_ParameterDescriptorTest PRIVATE NEURONiK_ParameterContract)
target_compile_definitions(NEURONiK_ParameterDescriptorTest PRIVATE
    NEURONIK_PARAMETER_ARTIFACTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/WebPilot/generated"
)

add_test(NAME NEURONiK_ParameterDescriptorTest COMMAND NEURONiK_ParameterDescriptorTest)

# Preset round trip: reuses the contract library, which already materialises the
# real layout in an APVTS without building the whole processor.
add_executable(NEURONiK_PresetRoundTripTest
    Tests/PresetRoundTripTest.cpp
    Source/Serialization/PresetManager.cpp
    Source/Serialization/PresetMigration.cpp
)

target_include_directories(NEURONiK_PresetRoundTripTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/State"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Serialization"
)

target_link_libraries(NEURONiK_PresetRoundTripTest PRIVATE
    NEURONiK_ParameterContract
)

add_test(NAME NEURONiK_PresetRoundTripTest COMMAND NEURONiK_PresetRoundTripTest)

# State persistence: the DAW-session round trip (getStateInformation /
# setStateInformation) needs the REAL processor, so it compiles the same source
# set as the WebPilot host (plugin sources minus the .rc) plus the bridge.
set(NEURONIK_STATE_TEST_SOURCES ${NEURONIK_SOURCES})
if(MSVC)
    list(REMOVE_ITEM NEURONIK_STATE_TEST_SOURCES Resources/NEURONiK.rc)
endif()

add_executable(NEURONiK_StatePersistenceTest
    Tests/StatePersistenceTest.cpp
    Source/WebUI/ParameterBridge.cpp
    ${NEURONIK_STATE_TEST_SOURCES}
)

target_include_directories(NEURONiK_StatePersistenceTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Main"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Panels"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/State"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Serialization"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/WebUI"
)

target_compile_definitions(NEURONiK_StatePersistenceTest PRIVATE
    JucePlugin_Name="NEURONiK"
)

target_link_libraries(NEURONiK_StatePersistenceTest PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    NEURONiK_Common
)

add_test(NAME NEURONiK_StatePersistenceTest COMMAND NEURONiK_StatePersistenceTest)

# Parameter bridge: same real layout APVTS as the contract, no WebView2 — the
# transport is a recording lambda, so the protocol is testable without a window.
add_executable(NEURONiK_ParameterBridgeTest
    Tests/ParameterBridgeTest.cpp
    Source/WebUI/ParameterBridge.cpp
)

target_include_directories(NEURONiK_ParameterBridgeTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/State"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/WebUI"
)

target_link_libraries(NEURONiK_ParameterBridgeTest PRIVATE
    NEURONiK_ParameterContract
)

add_test(NAME NEURONiK_ParameterBridgeTest COMMAND NEURONiK_ParameterBridgeTest)

# Bridge protocol contract: pins the versioned WebPilot/contracts/bridge-protocol.json
# against the wire constants compiled into ParameterBridge.h. Same anti-drift mechanism
# as the parameter contract above (NEURONIK_PARAMETER_ARTIFACTS_DIR).
add_executable(NEURONiK_BridgeProtocolContractTest
    Tests/BridgeProtocolContractTest.cpp
)

target_include_directories(NEURONiK_BridgeProtocolContractTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/WebUI"
)

target_link_libraries(NEURONiK_BridgeProtocolContractTest PRIVATE
    # ParameterContract pulls juce_data_structures + juce_audio_processors, which
    # satisfies the Colour/Colours symbols ParameterBridge.h drags in via
    # juce_audio_processors -> juce_gui_basics headers.
    NEURONiK_ParameterContract
)

target_compile_definitions(NEURONiK_BridgeProtocolContractTest PRIVATE
    NEURONIK_BRIDGE_PROTOCOL_JSON="${CMAKE_CURRENT_SOURCE_DIR}/WebPilot/contracts/bridge-protocol.json"
)

add_test(NAME NEURONiK_BridgeProtocolContractTest COMMAND NEURONiK_BridgeProtocolContractTest)

# JS twin of the same anti-drift check: bridge.js literals + real message shapes
# against the same versioned JSON. Needs node (located below, before first use), so it
# is skipped with a notice when there is none — same policy as the direction guard.
find_program(NEURONIK_NODE_EXECUTABLE node)

if(NEURONIK_NODE_EXECUTABLE)
    add_test(NAME NEURONiK_BridgeProtocolJs
        COMMAND ${NEURONIK_NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/Tests/bridgeProtocolContractTest.mjs"
    )
else()
    message(STATUS "node not found: NEURONiK_BridgeProtocolJs test skipped")
endif()

# WebView2 bridge direction guard, shared with the rest of the ABDSynths suite: the
# WebUI only hears messages sent with emitEventIfBrowserIsVisible(). Needs node, so
# it is skipped with a notice rather than failing the configure when there is none.
if(NEURONIK_NODE_EXECUTABLE)
    add_test(NAME NEURONiK_WebViewBridgeDirection
        COMMAND ${NEURONIK_NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/Tests/webviewBridgeDirectionTest.mjs"
    )
else()
    message(STATUS "node not found: NEURONiK_WebViewBridgeDirection test skipped")
endif()


# ============================================================================
# NEXT.JS WEBVIEW2 PILOT HOST
# ============================================================================

# Fail fast: the host serves the Next.js static export straight from disk.
set(NEURONIK_WEBPILOT_OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/WebPilot/out")
if(NOT EXISTS "${NEURONIK_WEBPILOT_OUT_DIR}/index.html")
    message(FATAL_ERROR
        "WebPilot static export missing. Run 'pnpm install --ignore-workspace && pnpm build' "
        "inside ${CMAKE_CURRENT_SOURCE_DIR}/WebPilot before configuring this host.")
endif()

# Embedded snapshot of the WebUI export: the host serves from disk first (fresh
# `pnpm build` output) and falls back to this copy inside the exe when the folder
# is not next to it. This is also how the final VST3 will carry its WebUI.
file(GLOB_RECURSE NEURONIK_WEBPILOT_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/WebPilot/out/*")

# The error routes (`404.html`, `404/index.html`, `_not-found/…`) share the root
# document's basename (`index.html`): juce_add_binary_data keeps ONE identifier per
# basename (alphabetically the 404 copy won it), so the embedded snapshot used to
# serve the not-found page as index.html and the pilot never reached react-ready.
# They are error pages the fallback never needs to serve -> exclude all of them so
# the embedded identifiers are unique AND honest. loadEmbeddedResource keeps a
# matching guard so an error-route request can never shadow a normal one.
list(FILTER NEURONIK_WEBPILOT_ASSETS EXCLUDE REGEX "_not-found")
list(FILTER NEURONIK_WEBPILOT_ASSETS EXCLUDE REGEX "[/\\\\]404([/\\\\]|\\.html$)")

if(NEURONIK_WEBPILOT_ASSETS)
    juce_add_binary_data(NEURONiK_WebPilotAssets SOURCES ${NEURONIK_WEBPILOT_ASSETS})
else()
    message(WARNING "WebPilot/out is empty: the pilot host will require the folder next to the exe (no embedded WebUI).")
endif()

juce_add_gui_app(NEURONiK_WebPilotHost
    PRODUCT_NAME "NEURONiK Web Pilot"
    VERSION "0.1.0"
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    SOURCES Source/WebPilotHost.cpp
)

# The REAL plugin, not a stand-in: the host instantiates NEURONiKProcessor, and
# the native panel below the page is the tab the editor ships (ParameterPanel).
# Same copy-the-sources pattern the Standalone target already uses.
set(NEURONIK_WEBPILOT_HOST_SOURCES ${NEURONIK_SOURCES})
if(MSVC)
    # The plugin .rc (VERSIONINFO/icon) would collide with the gui app's own.
    list(REMOVE_ITEM NEURONIK_WEBPILOT_HOST_SOURCES Resources/NEURONiK.rc)
endif()

target_sources(NEURONiK_WebPilotHost PRIVATE
    Source/WebPilotHost.cpp
    Source/WebUI/ParameterBridge.cpp
    ${NEURONIK_WEBPILOT_HOST_SOURCES}
)

target_compile_definitions(NEURONiK_WebPilotHost PRIVATE
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    NEURONiK_WEBPILOT_OUT_DIR="${NEURONIK_WEBPILOT_OUT_DIR}"
    # Plugin sources (copied below) reference JucePlugin_Name; the gui app target
    # does not generate plugin macros, so pin the same value the plugin target uses.
    JucePlugin_Name="NEURONiK"
)

if(TARGET NEURONiK_WebPilotAssets)
    target_link_libraries(NEURONiK_WebPilotHost PRIVATE NEURONiK_WebPilotAssets)
    target_compile_definitions(NEURONiK_WebPilotHost PRIVATE NEURONIK_HAS_PILOT_ASSETS=1)
endif()

# The panel's UI dependencies (RotaryControl, GlassBox, MidiLearner, themes...)
# and the processor's DSP chain: the same set the Standalone links, minus the
# audio-device stack (the pilot has no audio callback; parameter traffic only).
target_link_libraries(NEURONiK_WebPilotHost PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices   # AudioProcessorPlayer: the pilot renders audio so page MIDI notes sound
    juce::juce_audio_utils
    juce::juce_data_structures
    juce::juce_dsp
    NEURONiK_Common
)

target_include_directories(NEURONiK_WebPilotHost PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Main"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Panels"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Browser"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/State"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Serialization"
)

target_include_directories(NEURONiK_WebPilotHost PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/WebUI"
)

target_link_libraries(NEURONiK_WebPilotHost PRIVATE
    # The contract library brings the real APVTS layout (createLayoutApvts), so the host
    # bridges exactly the parameters the plugin ships, not a hand written copy.
    NEURONiK_ParameterContract
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    # SliderAttachment / ComboBoxAttachment for the native comparison strip.
    juce::juce_audio_processors
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
