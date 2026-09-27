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
# ABDSHAREDCODE — substrato DSP compartido del motor (Fase 1 [5/6])
# ============================================================================
# DRY transversal: el sustrato que hasta ahora vivia en Source/DSP (AudioBuffer,
# HeapBlock, SmoothedValue, FloatVectorOperations, ScopedNoDenormals, Random,
# Maths, los puertos MIDI, dspDbg y el leak detector) vive en
# ABDSharedCode::DspCore (namespace abd::dsp). Los ficheros de Source/DSP/ son
# shims que lo re-exportan con el nombre antiguo (`namespace dsp = abd::dsp;`),
# de modo que el motor no cambia una sola linea.
#
# DEBE ir despues de JUCE: los modulos de ABDSharedCode que NEURONiK no usa
# (AutoUpdater, HardwareDrivers) enlazan juce_core/juce_audio_basics.
set(ABDSHARED_CODE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ABDSharedCode")

# Se apagan los modulos y los tests que no se consumen. Los tests de DspCore SI
# se construyen: son 2 s (header-only, sin JUCE) y son el guard de regresion del
# sustrato compartido (incluido el bug de HeapBlock que descubrio el port de la
# reverb); se registran en ctest unas lineas mas abajo.
set(ABDSHAREDCODE_BUILD_SYNTHCORE_TESTS OFF)
set(ABDSHAREDCODE_BUILD_AUTOUPDATER OFF)
set(ABDSHAREDCODE_BUILD_HARDWAREDRIVERS OFF)
set(ABDSHAREDCODE_BUILD_HARDWAREMIDIDETECT OFF)

if(EXISTS "${ABDSHARED_CODE_DIR}/CMakeLists.txt")
    message(STATUS "ABDSharedCode: usando el local en ${ABDSHARED_CODE_DIR}")
    add_subdirectory("${ABDSHARED_CODE_DIR}" "${CMAKE_BINARY_DIR}/ABDSharedCode")
else()
    include(FetchContent)
    FetchContent_Declare(
      ABDSharedCode
      GIT_REPOSITORY https://github.com/ajabadia/ABDSharedCode.git
      GIT_TAG        master
    )
    FetchContent_MakeAvailable(ABDSharedCode)
endif()

# DspCore y DspEffects son INTERFACE (todo inline/template): no hay nada que
# enlazar, solo la ruta de include. Se pone a nivel de directorio a proposito:
# los shims de Source/DSP la necesitan desde CUALQUIER consumidor —el plugin, el
# host del piloto, ModelMaker y cada test— y asi un target nuevo no puede
# olvidarse de propagarla. (Diferencia con ABDEep, que si enlaza
# ABDShared::SynthCore target a target: aquel modulo tiene .cpp que enlazar.)
include_directories("${ABDSHARED_CODE_DIR}")

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
    # Lo que queda es lo que la bancada WebView2 MONTA de verdad (ParameterPanel +
    # XYPad, en WebPilotHost) y lo que estos usan. El resto del arbol nativo (paneles
    # de parametros, LCD, browser de presets, visualizadores sin montar) se retiro el
    # 2026-09-19: se compilaba sin que nadie lo instanciara y ahi vivian el
    # temporizador que reescribia el APVTS y las etiquetas duplicadas a mano
    # (ver DOCS/WEBUI_VS_NATIVE_PARITY.md).
    Source/UI/ParameterPanel.h
    Source/UI/ParameterPanel.cpp
    Source/UI/XYPad.h
    Source/UI/XYPad.cpp
    Source/UI/CustomUIComponents.h
    Source/UI/CustomUIComponents.cpp
    Source/UI/ThemeManager.h
    Source/UI/ThemeManager.cpp
    Source/UI/MidiLearner.h
    Source/UI/MidiLearner.cpp

    # State
    Source/State/ParameterDefinitions.h
    Source/State/ParameterRandomizer.h
    Source/State/ParameterRandomizer.cpp

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
    Source/Common/SpectralModelWriter.h
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
    # La interfaz del plugin es una pagina servida en WebView2 (Fase 8, ticket
    # 8.1): NEEDS_WEB_BROWSER trae WebBrowserComponent y NEEDS_WEBVIEW2 el loader
    # del runtime de Edge, para TODOS los formatos (Standalone y VST3). La bancada
    # del piloto ya lo pedia; el plugin no lo pedia porque no hospedaba pagina.
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
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
    # Substrato y efectos compartidos (INTERFACE: solo aportan includes). La
    # ruta ya llega por el include_directories del bloque de ABDSharedCode; el
    # enlace explicito deja la dependencia visible en el grafo del producto.
    ABDShared::DspCore
    ABDShared::DspEffects
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
# WEBUI (interfaz definitiva del plugin, Fase 8)
# ============================================================================

# El plugin sirve `WebUI/dist` desde una copia EMBEBIDA en el binario: asi el
# VST3 funciona copiado a cualquier sitio, sin depender del cwd del build ni de
# ninguna ruta absoluta (DoD de 8.1). Fallar aqui en alto es mejor que compilar
# un plugin sin interfaz, que es un binario que abre en blanco.
set(NEURONIK_WEBUI_DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/WebUI/dist")

file(GLOB_RECURSE NEURONIK_WEBUI_ASSETS CONFIGURE_DEPENDS
    "${NEURONIK_WEBUI_DIST_DIR}/*")

if(NEURONIK_WEBUI_ASSETS)
    # El proveedor compartido existe ya en ABDSharedCode y es el que usan
    # HardwareMidiDetect y ABDScope: el plugin lo adopta en vez de escribir otro.
    if(NOT TARGET ABDShared::WebView2Bridge)
        message(FATAL_ERROR
            "ABDShared::WebView2Bridge is missing: this plugin serves its WebUI through "
            "the shared provider (abd::webview2), it does not ship its own copy.")
    endif()

    # HEADER_NAME + NAMESPACE explicitos: el generico `BinaryData` es un nombre
    # comun en un arbol con varias librerias de recursos, y la homonimia es la clase
    # de deuda que el inventario de ABDSharedCode marca. Cada recurso, con su nombre.
    juce_add_binary_data(NEURONiK_WebUIAssets
        HEADER_NAME "NeuronikWebUIAssets.h"
        NAMESPACE "NeuronikWebUIAssets"
        SOURCES ${NEURONIK_WEBUI_ASSETS}
    )

    # Los dos formatos compilan el editor, asi que los dos montan la pagina. El
    # target de tests que COPIA NEURONIK_SOURCES (StatePersistenceTest) no hereda
    # nada de esto: compila el editor sin interfaz, que es lo que su guard mira.
    foreach(_neuronik_ui_target NEURONiK NEURONiK_Standalone)
        if(TARGET ${_neuronik_ui_target})
            # ParameterBridge.cpp va AQUI, no en NEURONIK_SOURCES: quien lo necesita
            # es la pagina (el editor la monta a traves de NeuronikWebView), no el
            # motor. Los targets que no montan pagina lo anaden por su cuenta
            # (StatePersistenceTest y la bancada WebView2, que lo prueban).
            # Sin esta linea el plugin compila y NO enlaza: la lib del wrapper
            # queda con NEURONiKEditor.obj pidiendo los 10 simbolos del puente.
            target_sources(${_neuronik_ui_target} PRIVATE
                Source/WebUI/NeuronikWebView.h
                Source/WebUI/ParameterBridge.cpp
            )

            target_include_directories(${_neuronik_ui_target} PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/Source/WebUI"
            )

            target_link_libraries(${_neuronik_ui_target} PRIVATE
                NEURONiK_WebUIAssets
                NEURONiK_SelftestAssets
                NEURONiK_FactoryModels
                ABDShared::WebView2Bridge
            )

            target_compile_definitions(${_neuronik_ui_target} PRIVATE
                JUCE_USE_WIN_WEBVIEW2=1
                JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
                # Montar la pagina. Sin esto NEURONiKEditor compila la variante sin
                # interfaz (un aviso en pantalla), que es lo honesto cuando no hay
                # pagina que servir.
                NEURONIK_HAS_WEBUI_VIEW=1
            )
        endif()
    endforeach()
else()
    # No es FATAL_ERROR a proposito: la configuracion tiene que poder hacerse en
    # un arbol recien clonado, y quien construye `WebUI/dist` es build.bat (o el
    # desarrollador con `pnpm build`). El plugin compila sin interfaz y AVISA; el
    # guard de build.bat impide que ese binario se de por bueno.
    message(WARNING
        "WebUI/dist is empty: the plugin will build WITHOUT the web interface. "
        "Run 'pnpm install && pnpm build' inside ${CMAKE_CURRENT_SOURCE_DIR}/WebUI "
        "(build.bat does it automatically).")
endif()

# ============================================================================
# DSP REFERENCE TESTS
# ============================================================================

enable_testing()

# Guard de regresion del sustrato compartido (ABDSharedCode::DspCore): corre el
# test del modulo dentro de esta suite porque es aqui donde se consume. El target
# solo existe si ABDSHAREDCODE_BUILD_DSPCORE_TESTS esta ON (lo esta unas lineas
# mas arriba); con OFF la suite no lo exige.
if(TARGET ABDShared_DspCore_Tests)
    add_test(NAME ABDShared_DspCore_Tests COMMAND ABDShared_DspCore_Tests)
endif()

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
# WASM PARITY REFERENCE (Fase 5)
# ============================================================================
# Genera build-wasm/parity-native.json: la referencia nativa de los 5
# escenarios x 9 casos (44.1/48/96 kHz x 64/128/512 muestras de bloque) que
# Tests/neuronik_wasm_parity.mjs re-ejecuta sobre el módulo WASM real y compara
# muestra a muestra. La duración de cada escenario es la misma en todos los
# casos (los bloques se reescalan sobre 128): eso es lo que convierte la matriz
# en una validación de sample rate y block size, y de paso mide en nativo si la
# salida depende del tamaño de bloque. No entra en ctest: es un generador, el
# árbitro final es el test Node (paso 4 de build_wasm.bat).
add_executable(NEURONiK_WasmParityTest
    Tests/WasmParityTest.cpp
    ${NEURONIK_DSP_SOURCES}
)

target_include_directories(NEURONiK_WasmParityTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
)

target_link_libraries(NEURONiK_WasmParityTest PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    NEURONiK_Common
)

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

# Regenerates WebUI/generated/{parameters.generated.json,.js,.d.ts}
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

# MIDI ports of the DSP core (Fase 1, paso 4/6): dsp::MidiMessage/MidiBuffer are
# ports of juce::MidiMessage/MidiBuffer, so this test pins them against their
# originals (byte-space sweep, factories, velocity quantisation) and pins the
# JUCE -> dsp adapter (order, positions, dropped system messages).
add_executable(NEURONiK_MidiPortTest
    Tests/MidiPortTest.cpp
)

target_include_directories(NEURONiK_MidiPortTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
)

target_link_libraries(NEURONiK_MidiPortTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
)

add_test(NAME NEURONiK_MidiPortTest COMMAND NEURONiK_MidiPortTest)

# Reverb del motor (Fase 1, paso 5/6): dsp::Reverb es el port libre de JUCE de
# juce::Reverb. El mismo fuente se compila DOS veces porque el port tiene una
# politica de denormales conmutable (ver DspCore.h -> dspUndenormalise):
#   - por defecto: no-op uniforme (es lo que se envia; paridad nativo <-> WASM)
#   - DSP_UNDENORMALISE_JUCE_POLICY=1: replica de JUCE_UNDENORMALISE, que no es
#     un no-op en x86 y es lo que permite exigir 0 ulps contra juce::Reverb.
add_executable(NEURONiK_DspReverbParityTest
    Tests/DspReverbParityTest.cpp
)

target_include_directories(NEURONiK_DspReverbParityTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
)

target_link_libraries(NEURONiK_DspReverbParityTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
)

add_test(NAME NEURONiK_DspReverbParityTest COMMAND NEURONiK_DspReverbParityTest)

# Mismo fuente con la politica de JUCE: prueba que el port es literal (0 ulps).
add_executable(NEURONiK_DspReverbJucePolicyTest
    Tests/DspReverbParityTest.cpp
)

target_include_directories(NEURONiK_DspReverbJucePolicyTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
)

target_compile_definitions(NEURONiK_DspReverbJucePolicyTest PRIVATE DSP_UNDENORMALISE_JUCE_POLICY=1)

target_link_libraries(NEURONiK_DspReverbJucePolicyTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
)

add_test(NAME NEURONiK_DspReverbJucePolicyTest COMMAND NEURONiK_DspReverbJucePolicyTest)

# Efectos migrados a ABDSharedCode::DspEffects (continuacion de la Fase 1
# [5/6], que movio la reverb): chorus, delay y saturacion son ahora motores
# compartidos y Source/DSP/Effects/ son envoltorios de producto. Este test fija
# que la migracion no cambio la salida: compara el envoltorio contra una
# REFERENCIA CONGELADA del efecto de antes, 0 ulps, muestra a muestra.
#
# No enlaza JUCE: el modulo compartido (DspCore + DspEffects) es JUCE-free, que
# es justo el punto de la migracion. Solo necesita la ruta de include de
# ABDSharedCode, que ya llega a nivel de directorio.
add_executable(NEURONiK_DspEffectsParityTest
    Tests/DspEffectsParityTest.cpp
)

target_include_directories(NEURONiK_DspEffectsParityTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
)

add_test(NAME NEURONiK_DspEffectsParityTest COMMAND NEURONiK_DspEffectsParityTest)

# Sustrato compartido (ABDSharedCode::DspCore) contra su original: el port de
# dsp::AudioBuffer se compara con juce::AudioBuffer (misma secuencia de
# operaciones = mismas muestras bit a bit) y, sobre todo, la ESTRUCTURA de
# punteros: donde vive el array de canales (la rama de preallocatedChannelSpace,
# que estuvo muerta mientras numElementsInArray devolvia 1) y que pasa al mover o
# al copiar un buffer que referencia memoria externa. La comparacion tiene que
# estar en un consumidor con JUCE: el modulo compartido es JUCE-free por
# contrato.
add_executable(NEURONiK_AudioBufferParityTest
    Tests/AudioBufferParityTest.cpp
)

target_include_directories(NEURONiK_AudioBufferParityTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
)

target_link_libraries(NEURONiK_AudioBufferParityTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
)

add_test(NAME NEURONiK_AudioBufferParityTest COMMAND NEURONiK_AudioBufferParityTest)

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
    NEURONIK_PARAMETER_ARTIFACTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/WebUI/generated"
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

# Factory presets CZ101: la herramienta OFFLINE que escribe
# Assets/Presets/*.neuronikpreset (el APVTS real por defecto + modelPath0 con
# el marcador {{FACTORY_MODELS}}). Es el material que despues embebe
# NEURONiK_FactoryModels y que installFactoryPresets() instala en Documents.
# Sin add_test: regenerar pisa ficheros versionados (es una herramienta, como
# la sonda RealWav, no una prueba de ctest).
add_executable(NEURONiK_FactoryPresetGenerator
    Tests/FactoryPresetGenerator.cpp
    Source/Serialization/PresetManager.cpp
    Source/Serialization/PresetMigration.cpp
)

target_include_directories(NEURONiK_FactoryPresetGenerator PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/State"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Serialization"
)

target_link_libraries(NEURONiK_FactoryPresetGenerator PRIVATE
    NEURONiK_ParameterContract
)

# State persistence: the DAW-session round trip (getStateInformation /
# setStateInformation) needs the REAL processor, so it compiles the same source
# set as the WebView2 bench (plugin sources minus the .rc) plus the bridge.
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
    NEURONiK_FactoryModels
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

# Model slots A-D (ticket 8.2): el formato de un .neuronikmodel (el que escribe el
# ModelMaker contra el que leia el plugin), la carga por ranura, el preset que las
# referencia, el modelsState con el adaptador REAL y el audio. Necesita el
# procesador de verdad —los modelos solo llegan al motor por su FIFO de comandos y
# el hilo de audio—, asi que compila el mismo juego de fuentes que el de estado.
add_executable(NEURONiK_ModelSlotTest
    Tests/ModelSlotTest.cpp
    Source/WebUI/ParameterBridge.cpp
    ${NEURONIK_STATE_TEST_SOURCES}
)

target_include_directories(NEURONiK_ModelSlotTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Main"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Panels"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/State"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Serialization"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/WebUI"
)

target_compile_definitions(NEURONiK_ModelSlotTest PRIVATE
    JucePlugin_Name="NEURONiK"
)

target_link_libraries(NEURONiK_ModelSlotTest PRIVATE
    NEURONiK_FactoryModels
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

add_test(NAME NEURONiK_ModelSlotTest COMMAND NEURONiK_ModelSlotTest)

# FASE 11.1: la HUELLA del modelo de 25 KB. El struct paso de ~8 KB a ~25 KB y
# cada voz lleva OCHO copias (4 slots x models+frameCache), asi que el
# presupuesto por voz lo manda el modelo, no la polifonia: los motores
# pre-asignan 32 voces. Esta prueba mide heap y pila del procesador REAL
# (contador propio sobre operator new/delete) y FIJA el presupuesto por slot,
# por voz, por motor y por procesador. Compila el mismo juego de fuentes que el
# de ranuras: necesita el procesador de verdad.
add_executable(NEURONiK_MemoryBudgetTest
    Tests/MemoryBudgetTest.cpp
    Source/WebUI/ParameterBridge.cpp
    ${NEURONIK_STATE_TEST_SOURCES}
)

target_include_directories(NEURONiK_MemoryBudgetTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Main"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Panels"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/State"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Serialization"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/WebUI"
)

target_compile_definitions(NEURONiK_MemoryBudgetTest PRIVATE
    JucePlugin_Name="NEURONiK"
)

target_link_libraries(NEURONiK_MemoryBudgetTest PRIVATE
    NEURONiK_FactoryModels
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

add_test(NAME NEURONiK_MemoryBudgetTest COMMAND NEURONiK_MemoryBudgetTest)

# Round-trip del ModelMaker con analisis REAL (2026-09-20): el flujo completo de la
# herramienta (audio -> detectPitch/analyze -> export JSON -> loadModel en A-D) con
# el MISMO codigo que la GUI conduce. Es la reproduccion automatica del flujo a mano.
add_executable(NEURONiK_ModelMakerRoundTripTest
    Tests/ModelMakerRoundTripTest.cpp
    Source/ModelMaker/Analysis/SpectralAnalyzer.cpp
    Source/WebUI/ParameterBridge.cpp
    ${NEURONIK_STATE_TEST_SOURCES}
)

target_include_directories(NEURONiK_ModelMakerRoundTripTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Main"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Panels"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/State"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Serialization"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/WebUI"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ModelMaker/Analysis"
)

target_compile_definitions(NEURONiK_ModelMakerRoundTripTest PRIVATE
    JucePlugin_Name="NEURONiK"
)

target_link_libraries(NEURONiK_ModelMakerRoundTripTest PRIVATE
    NEURONiK_FactoryModels
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

add_test(NAME NEURONiK_ModelMakerRoundTripTest COMMAND NEURONiK_ModelMakerRoundTripTest)

# Sonda CLI de produccion del ModelMaker (2026-09-23): la GUI es WIN32 sin
# CLI, asi que esta sonda enlaza los MISMOS ficheros de produccion
# (SpectralAnalyzer + PresetManager + set del plugin) y conduce el flujo real
# WAV -> detectPitch -> analyze -> export v2 -> loadModelFromFile ->
# sampleFrame contra WAVs reales. Sin add_test: es sonda, no test de ctest.
add_executable(NEURONiK_ModelMakerRealWavProbe
    Tests/ModelMakerRealWavProbe.cpp
    Source/ModelMaker/Analysis/SpectralAnalyzer.cpp
    Source/WebUI/ParameterBridge.cpp
    ${NEURONIK_STATE_TEST_SOURCES}
)

target_include_directories(NEURONiK_ModelMakerRealWavProbe PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Main"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Panels"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ModelMaker/Analysis"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Serialization"
)

target_compile_definitions(NEURONiK_ModelMakerRealWavProbe PRIVATE
    JucePlugin_Name="NEURONiK"
)

target_link_libraries(NEURONiK_ModelMakerRealWavProbe PRIVATE
    NEURONiK_FactoryModels
    juce::juce_audio_basics
    juce::juce_audio_formats
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

# Factory presets CZ101 - VERIFICACION ACUSTICA (ctest): un preset de fabrica
# suena a la FUENTE. Instalacion en Documents, carga real del preset embebido
# en el processor, render a 48 kHz y dos cheats de cents con el harness DFT de
# la sonda (Hann 8192, top-8, mediana <= 35): render vs modelo (contrato
# siempre) y modelo vs WAV del banco (contrato si CMake inyecta
# NEURONiK_CZ101_WAV_DIR porque ../ABDCZ101/DOCS/patches existe).
add_executable(NEURONiK_FactoryPresetAudioTest
    Tests/FactoryPresetAudioTest.cpp
    Source/ModelMaker/Analysis/SpectralAnalyzer.cpp
    Source/WebUI/ParameterBridge.cpp
    ${NEURONIK_STATE_TEST_SOURCES}
)

target_include_directories(NEURONiK_FactoryPresetAudioTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Main"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Panels"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/State"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Serialization"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/WebUI"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ModelMaker/Analysis"
)

target_compile_definitions(NEURONiK_FactoryPresetAudioTest PRIVATE
    JucePlugin_Name="NEURONiK"
)

set(NEURONIK_CZ101_WAV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ABDCZ101/DOCS/patches")
if(EXISTS "${NEURONIK_CZ101_WAV_DIR}/CZ-BASS1.wav")
    target_compile_definitions(NEURONiK_FactoryPresetAudioTest PRIVATE
        NEURONiK_CZ101_WAV_DIR="${NEURONIK_CZ101_WAV_DIR}")
endif()

target_link_libraries(NEURONiK_FactoryPresetAudioTest PRIVATE
    NEURONiK_FactoryModels
    juce::juce_audio_basics
    juce::juce_audio_formats
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

add_test(NAME NEURONiK_FactoryPresetAudioTest COMMAND NEURONiK_FactoryPresetAudioTest)

# El analizador del ModelMaker en aislado: multiframe, peak-picking +-2 bins
# e interpolacion sub-bin (frequencyOffsets ya no es un TODO). El A/B musical
# del analizador: que la tabla y los offsets respondan a la FUENTE, no al azar
# de una sola ventana.
add_executable(NEURONiK_SpectralAnalyzerTest
    Tests/SpectralAnalyzerTest.cpp
    Source/ModelMaker/Analysis/SpectralAnalyzer.cpp
)

target_include_directories(NEURONiK_SpectralAnalyzerTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ModelMaker/Analysis"
)

target_link_libraries(NEURONiK_SpectralAnalyzerTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_dsp
    NEURONiK_Common
)

add_test(NAME NEURONiK_SpectralAnalyzerTest COMMAND NEURONiK_SpectralAnalyzerTest)

# El ajuste de rejilla por MINIMOS CUADRADOS de detectPitch con material
# sintetico cuyo f0 se DESPLAZA entre ventanas: misma cabecera (220 Hz,
# bitwise identica) en control y deriva, pero el LS lee la MEDIA de las
# ventanas (230) y el residuo delata el desplazamiento. Ademas la via
# temporal por frame, la guardia de material pobre (el ajuste se abstiene)
# y el indicador n/d sin material.
add_executable(NEURONiK_LeastSquaresGridTest
    Tests/LeastSquaresGridTest.cpp
    Source/ModelMaker/Analysis/SpectralAnalyzer.cpp
)

target_include_directories(NEURONiK_LeastSquaresGridTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ModelMaker/Analysis"
)

target_link_libraries(NEURONiK_LeastSquaresGridTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_dsp
    NEURONiK_Common
)

add_test(NAME NEURONiK_LeastSquaresGridTest COMMAND NEURONiK_LeastSquaresGridTest)
# 2026-09-27: BATERIA DE FAMILIAS DE OCTAVA (proto HPS) -- decision por RESIDUO
# GLOBAL sobre TODAS las ventanas. Las tres familias {seed/2, seed, seed*2}
# sinteticas (tono limpio, fund.debil que muere vs continua, dos alturas, semilla
# desplazada y barrido) mas los WAV reales si el banco existe. Sin banco, salta.
add_executable(NEURONiK_OctaveFamilyTest
    Tests/OctaveFamilyTest.cpp
    Source/ModelMaker/Analysis/SpectralAnalyzer.cpp
)

target_include_directories(NEURONiK_OctaveFamilyTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ModelMaker/Analysis"
)

target_link_libraries(NEURONiK_OctaveFamilyTest PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_dsp
    NEURONiK_Common
)

add_test(NAME NEURONiK_OctaveFamilyTest COMMAND NEURONiK_OctaveFamilyTest)


# 2026-09-25: LOS RANGOS DEL RESIDUO con los CINCO WAV reales del banco CZ101
# (../ABDCZ101/DOCS/patches, la misma ruta que inyecta el test acustico de
# presets). El residuo de rejilla es el numero que la UI enseña en el indicador
# y junto al aviso de pitch inestable: aqui se clava por material y por BANDA —
# los tres tonales en verde (BASS1 6.2, HAMOG 4.5, PAD1 7.1 cents), SWEP1 en
# amarillo (25.2: una rejilla con sub-armonico) y RRISE en naranja (243.9: un
# barrido no es UNA rejilla) con la guardia disparada (548 cents). Las bandas
# salen de SpectralAnalyzer::residualGreenCents/AmberCents, las MISMAS que usa
# el pincel del indicador. Sin el banco el test saltea y pasa.
add_executable(NEURONiK_Cz101ResidualRangesTest
    Tests/Cz101ResidualRangesTest.cpp
    Source/ModelMaker/Analysis/SpectralAnalyzer.cpp
)

target_include_directories(NEURONiK_Cz101ResidualRangesTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ModelMaker/Analysis"
)

if(EXISTS "${NEURONIK_CZ101_WAV_DIR}/CZ-BASS1.wav")
    target_compile_definitions(NEURONiK_Cz101ResidualRangesTest PRIVATE
        NEURONiK_CZ101_WAV_DIR="${NEURONIK_CZ101_WAV_DIR}")
endif()

target_link_libraries(NEURONiK_Cz101ResidualRangesTest PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_dsp
    NEURONiK_Common
)

add_test(NAME NEURONiK_Cz101ResidualRangesTest COMMAND NEURONiK_Cz101ResidualRangesTest)

# FASE 11.2: clustering de parciales por FORMA DE ENVOLVENTE. El modulo puro
# (LayerClustering.h) se prueba con trazas sinteticas — sin audio, deterministico
# — y el analizador de punta a punta: drone + voz migrando deben dar un modelo
# con 2 capas cuya RAIZ sea el drone, y un barrido de pitch fuerte NO debe
# partirse (la puerta de rejilla).
add_executable(NEURONiK_LayerClusteringTest
    Tests/LayerClusteringTest.cpp
    Source/ModelMaker/Analysis/SpectralAnalyzer.cpp
)

target_include_directories(NEURONiK_LayerClusteringTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ModelMaker/Analysis"
)

target_link_libraries(NEURONiK_LayerClusteringTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_dsp
    NEURONiK_Common
)

add_test(NAME NEURONiK_LayerClusteringTest COMMAND NEURONiK_LayerClusteringTest)

# FASE 11.5: los DATOS DE VISTA de las capas que pinta la pestana "CAPAS" del
# ModelMaker (indice -> capa, envolvente por frame, leyenda). Modulo PURO: se
# prueba con modelos sinteticos, sin abrir una ventana.
add_executable(NEURONiK_LayerViewTest
    Tests/LayerViewTest.cpp
    Source/ModelMaker/Analysis/SpectralAnalyzer.cpp
)

target_include_directories(NEURONiK_LayerViewTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ModelMaker/Analysis"
)

target_link_libraries(NEURONiK_LayerViewTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_dsp
    NEURONiK_Common
)

add_test(NAME NEURONiK_LayerViewTest COMMAND NEURONiK_LayerViewTest)

# FASE 11.5: el MODELO PURO del indicador de rejilla del ModelMaker (la fila
# "Rejilla: f0 ... | residuo ... | N picos", su color por bandas y lo que el
# clic carga en el editor de pitch). Sin JUCE: la GUI solo pinta lo que el
# modelo devuelve, y este test lo verifica sin abrir una ventana.
add_executable(NEURONiK_GridIndicatorTest
    Tests/GridIndicatorTest.cpp
)

target_include_directories(NEURONiK_GridIndicatorTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ModelMaker/Analysis"
)

add_test(NAME NEURONiK_GridIndicatorTest COMMAND NEURONiK_GridIndicatorTest)
# FASE 10.4: el emparejador del analisis temporal — un seno con vibrato
# (offset sub-bin que evoluciona) debe producir frames que SIGUEN el vibrato.
# El flujo completo temporal: analyzeTemporal -> export v2 (GUI) -> lector.
add_executable(NEURONiK_TemporalAnalysisTest
    Tests/TemporalAnalysisTest.cpp
    Source/ModelMaker/Analysis/SpectralAnalyzer.cpp
    Source/WebUI/ParameterBridge.cpp
    ${NEURONIK_STATE_TEST_SOURCES}
)

target_include_directories(NEURONiK_TemporalAnalysisTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Main"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/UI/Panels"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/ModelMaker/Analysis"
)

target_compile_definitions(NEURONiK_TemporalAnalysisTest PRIVATE
    JucePlugin_Name="NEURONiK"
)

target_link_libraries(NEURONiK_TemporalAnalysisTest PRIVATE
    NEURONiK_FactoryModels
    juce::juce_audio_basics
    juce::juce_audio_formats
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

add_test(NAME NEURONiK_TemporalAnalysisTest COMMAND NEURONiK_TemporalAnalysisTest)

# FASE 10: modelos temporales — sampleFrame + morphZ en el motor real
# (z=0 frame canonico bit-exacto; z=1 frame temporal; el knob es inertel
# con modelos estaticos, que es la garantia de compatibilidad de la fase).
add_executable(NEURONiK_FrameSamplerTest
    Tests/FrameSamplerTest.cpp
    ${NEURONIK_DSP_SOURCES}
)

target_include_directories(NEURONiK_FrameSamplerTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
)

target_link_libraries(NEURONiK_FrameSamplerTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_dsp
    NEURONiK_Common
)

add_test(NAME NEURONiK_FrameSamplerTest COMMAND NEURONiK_FrameSamplerTest)

# FASE 11.3: el motor SUMA las capas (sampler por capa + morphZ2/morphZ3 +
# pesos). Barrido por capas sintetico tipo RRISE (capa 0 = drone con dos
# frames, capa 1 = parcial que barre): morphZ1 mueve el drone y no toca al
# barrido, morphZ2 al reves. Con UNA capa el camino es bit-exacto al legado.
add_executable(NEURONiK_LayerEngineTest
    Tests/LayerEngineTest.cpp
    Source/Wasm/NeuronikWasmBridge.cpp
    ${NEURONIK_DSP_SOURCES}
)

target_include_directories(NEURONiK_LayerEngineTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
)

target_link_libraries(NEURONiK_LayerEngineTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_dsp
    NEURONiK_Common
)

add_test(NAME NEURONiK_LayerEngineTest COMMAND NEURONiK_LayerEngineTest)

# 2026-09-25: OFFSETS TRANSPONIBLES (modo delta-n/n). El motor escala el
# offset por base/f0 con el modo declarado, de modo que la inharmonicidad
# se transpone con el teclado. Sin modo el factor es 1.0 EXACTO (legado bit
# a bit, comprobado sobre el audio).
add_executable(NEURONiK_TransposableOffsetsTest
    Tests/TransposableOffsetsTest.cpp
    ${NEURONIK_DSP_SOURCES}
)

target_include_directories(NEURONiK_TransposableOffsetsTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
)

target_link_libraries(NEURONiK_TransposableOffsetsTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_dsp
    NEURONiK_Common
)

add_test(NAME NEURONiK_TransposableOffsetsTest COMMAND NEURONiK_TransposableOffsetsTest)
# El modo bow del motor modal: sostenido con arco (ganancia plena) frente al
# legacy impulso+ruido, con el motor real. Pin de compatibilidad incluido
# (bow=0 conserva el nivel historico del sostenido).
add_executable(NEURONiK_NeurotikBowTest
    Tests/NeurotikBowTest.cpp
    ${NEURONIK_DSP_SOURCES}
)

target_include_directories(NEURONiK_NeurotikBowTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP"
)

target_link_libraries(NEURONiK_NeurotikBowTest PRIVATE
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_dsp
    NEURONiK_Common
)

add_test(NAME NEURONiK_NeurotikBowTest COMMAND NEURONiK_NeurotikBowTest)

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

# El RANDOMIZE salio del panel nativo (ticket 8.2) a State/ParameterRandomizer,
# donde puede probarse sin procesador ni UI. Este test es la regresion del bug que
# traia: mezclaba unidades reales con normalizadas y clavaba el parametro en su
# maximo. Necesita el MISMO layout APVTS que el contrato.
add_executable(NEURONiK_ParameterRandomizerTest
    Tests/ParameterRandomizerTest.cpp
    Source/State/ParameterRandomizer.cpp
)

target_include_directories(NEURONiK_ParameterRandomizerTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/State"
)

target_link_libraries(NEURONiK_ParameterRandomizerTest PRIVATE
    NEURONiK_ParameterContract
)

add_test(NAME NEURONiK_ParameterRandomizerTest COMMAND NEURONiK_ParameterRandomizerTest)

# Bridge protocol contract: pins the versioned WebUI/contracts/bridge-protocol.json
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
    NEURONIK_BRIDGE_PROTOCOL_JSON="${CMAKE_CURRENT_SOURCE_DIR}/WebUI/contracts/bridge-protocol.json"
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

# Selftest contract, C++ <-> pagina. El selftest de siete direcciones
# (WebUI/BridgeSelftest.h) consulta unos anclajes de la pagina (el primer
# input[type=range], el <code> del pie, la pestana KEYS, la rueda de modulacion,
# el cajon lateral de la matriz y la celda de un control, la ficha MODELOS A-D,
# __pilotSendMidi) y comprueba los 11 ids de GENERAL: si un
# lado se mueve sin el otro, el selftest deja de comprobar lo que dice comprobar y
# puede dar OK. Tambien fija que las siete direcciones son OBLIGATORIAS: la maquinaria de
# "direccion no aplicable" se fue con el piloto retirado (ticket 8.4), y este test impide
# que vuelva por la puerta de atras. Lo fija por los dos lados, como el contrato del
# protocolo. Necesita node.
if(NEURONIK_NODE_EXECUTABLE)
    add_test(NAME NEURONiK_WebUiSelftestContract
        COMMAND ${NEURONIK_NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/Tests/webuiSelftestContractTest.mjs"
    )

    # Paridad lienzo web <-> superficies nativas. El test NO exige que las dos
    # superficies se vean iguales (no lo son, y a proposito: ver
    # DOCS/WEBUI_VS_NATIVE_PARITY.md); exige lo unico que si es contractual: que
    # ningun id del lienzo ni del C++ se haya quedado fuera del contrato, y que el
    # lienzo no pinte el mismo parametro dos veces. El informe completo (que
    # parametros ve cada superficie con el mismo preset cargado) lo imprime el
    # script, con `--quiet` para el resumen. Necesita node.
    add_test(NAME NEURONiK_NativePanelParity
        COMMAND ${NEURONIK_NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/Tests/nativePanelParityReport.mjs" --quiet
    )
else()
    message(STATUS "node not found: NEURONiK_WebUiSelftestContract test skipped")
endif()

# Worklet WASM sync guard: por HASH (SHA256) build-wasm <-> public/worklet <-> dist/worklet.
# Hace fallar ctest y el build si el DSP que sirve la WebUI (y que embebe el plugin) quedo viejo.
# La comparacion vive en Tests/workletSyncTest.mjs y la comparten build.bat y build_wasm.bat.
if(NEURONIK_NODE_EXECUTABLE)
    add_test(NAME NEURONiK_WorkletSync
        COMMAND ${NEURONIK_NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/Tests/workletSyncTest.mjs"
    )
else()
    message(STATUS "node not found: NEURONiK_WorkletSync test skipped")
endif()

# Ficheros REFERENCIADOS: todo lo que el build y el E2E nombran tiene que estar
# versionado. Sin esto, un fichero que se queda sin `git add` funciona en la
# maquina donde se escribio y rompe en un clone limpio con un 404 o con un
# target que no compila. Ya ocurrio con la pagina de prueba de la aguja
# (`/needle-probe/`): el `playwright.config.js` levantaba un servidor para ella
# y ni el spec ni su fixture estaban versionados -los 22 tests de E2E en verde
# aqui, timeout en el clone-. El guard nombra el fichero que falta y quien lo
# nombro. Sin git (tarball, zip) avisa y pasa.
if(NEURONIK_NODE_EXECUTABLE)
    add_test(NAME NEURONiK_ReferencedFiles
        COMMAND ${NEURONIK_NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/Tests/referencedFilesExist.mjs"
    )
else()
    message(STATUS "node not found: NEURONiK_ReferencedFiles test skipped")
endif()

# Ruta local del pad: en MODO LOCAL (navegador, sin host) la pagina siembra una ruta
# de la MATRIZ a Morph Z (LFO 2 -> Morph Z) para que el anillo del pad baile con el LFO.
# Este test la lleva al MOTOR REAL (build-wasm) por el MISMO camino que la pagina -el store
# y el mapeo contrato -> GlobalParams del worklet- y mide _neuronikGetMod(28): barrido de los
# dos signos a ~1 s de periodo, con control negativo en Off. Sin build-wasm avisa y pasa
# (clone limpio sin WASM compilado: mismo skip que NEURONiK_WorkletSync).
if(NEURONIK_NODE_EXECUTABLE)
    add_test(NAME NEURONiK_LocalMorphZRoute
        COMMAND ${NEURONIK_NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/Tests/localMorphZRouteTest.mjs"
    )
else()
    message(STATUS "node not found: NEURONiK_LocalMorphZRoute test skipped")
endif()

# Memoria local de las ranuras: en MODO LOCAL (navegador, sin host) no hay preset que
# devuelva los modelos a sus cuatro ranuras -el plugin los recupera por `modelPath<slot>`-,
# asi que la pagina RECUERDA en localStorage el TEXTO crudo de lo que se cargo. Este test
# recorre el camino completo (el store real, con un localStorage de mentira porque Node no
# lo tiene) y comprueba que la ranura recuperada vuelve bit a bit por el MISMO parser y que
# en el MOTOR REAL la misma nota suena distinto que con la ranura vacia. Sin build-wasm
# avisa y pasa (mismo skip que NEURONiK_WorkletSync).
if(NEURONIK_NODE_EXECUTABLE)
    add_test(NAME NEURONiK_LocalModelCache
        COMMAND ${NEURONIK_NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/Tests/localModelCacheTest.mjs"
    )
else()
    message(STATUS "node not found: NEURONiK_LocalModelCache test skipped")
endif()

# E2E de navegador REAL (Playwright + Chromium): el smoke del MODO LOCAL que hasta hoy se
# verificaba a mano -SOUND ON arranca el motor WASM, la ficha RANURAS carga un modelo, una
# tecla enciende la voz y el pad mueve morphX/morphY con su anillo bailando-. Sirve `dist`
# (el artefacto que embebe el plugin; el comando del servidor construye antes) y asierta
# sobre el DOM y sobre los mensajes que la pagina manda al worklet. Es el UNICO test que ve
# esa frontera de verdad: los de node leen el WASM a mano y el vitest corre en jsdom.
# Sin @playwright/test instalado avisa y pasa (mismo skip que los guards de WASM); el
# navegador se instala con `npx playwright install chromium`.
if(NEURONIK_NODE_EXECUTABLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/WebUI/node_modules/@playwright/test/cli.js")
    # Con SU spec: la regresion visual (que compara contra binarios versionados)
    # es otro test, y asi este no depende de que las referencias esten al dia.
    add_test(NAME NEURONiK_WebUiLocalModeE2e
        COMMAND ${NEURONIK_NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/WebUI/node_modules/@playwright/test/cli.js" test e2e/localMode.spec.js
    )
    set_tests_properties(NEURONiK_WebUiLocalModeE2e PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/WebUI"
        TIMEOUT 600
        # Las dos suites de navegador CONSTRUYEN el mismo `dist` antes de servirlo
        # (el comando del webServer). Dos `vite build` a la vez sobre el mismo
        # directorio de salida se pisan y uno falla al arrancar —medido, no
        # supuesto—. Este candado impide que se solapen y NO serializa el resto de
        # ctest: los otros 39 siguen en paralelo.
        RESOURCE_LOCK "neuronik_webui_dist"
    )
else()
    message(STATUS "playwright not installed: NEURONiK_WebUiLocalModeE2e test skipped")
endif()

# ---------------------------------------------------------------------------
# REGRESION VISUAL del lienzo (Playwright + Chromium), una referencia por ficha.
# ---------------------------------------------------------------------------
# El E2E de arriba afirma que el lienzo EXISTE y que sus textos dicen lo que deben;
# este afirma que se PINTA bien: cada ficha tiene su foto de referencia en
# WebUI/e2e/snapshots/ y se compara con el umbral de playwright.config.js
# (20 pixeles y 0.2 por pixel, medidos: el ruido entre corridas es 0 y la
# regresion mas pequena construida movia 77 pixeles).
#
# Es un test APARTE, y no un caso mas del E2E, por dos motivos que se notan al
# mirar los numeros: (1) regenerar las referencias con `--update-snapshots` debe
# poder hacerse sin tocar las del smoke, y (2) comparar contra un binario es mas
# lento y mas propenso a fallar por el entomo que el resto, asi que se puede
# dejar fuera mientras se investiga. Por eso el E2E de arriba invoca SU spec y no
# el `test` a pelo. `pnpm test:e2e` (sin argumentos) sigue corriendo las dos.
# Mismo Chromium, mismo puerto y mismo build de dist: las dos suites sirven el
# mismo artefacto.
if(NEURONIK_NODE_EXECUTABLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/WebUI/node_modules/@playwright/test/cli.js")
    add_test(NAME NEURONiK_WebUiVisualRegression
        COMMAND ${NEURONIK_NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/WebUI/node_modules/@playwright/test/cli.js" test e2e/visual.spec.js
    )
    set_tests_properties(NEURONiK_WebUiVisualRegression PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/WebUI"
        TIMEOUT 600
        # Mismo candado de `dist` que el E2E de arriba, y un puerto PROPIO: con
        # `--strictPort` compartirlo haria que el segundo servidor no arrancara
        # (EADDRINUSE). Ver la nota de E2E_PORT en WebUI/playwright.config.js.
        RESOURCE_LOCK "neuronik_webui_dist"
        ENVIRONMENT "NEURONIK_E2E_PORT=5239"
    )
else()
    message(STATUS "playwright not installed: NEURONiK_WebUiVisualRegression test skipped")
endif()



# ============================================================================
# SELFTEST ASSETS (los SEIS modelos del banco CZ101 para el E2E del bridge)
# ============================================================================

# Los SEIS modelos del banco CZ101 (Assets/Models), generados por la sonda CLI
# del ModelMaker a partir de los WAV reales de ../ABDCZ101/DOCS/patches. Viajan
# EMBEBIDOS para que el selftest del bridge (BridgeSelftest.h, ocho direcciones)
# sea AUTOCONTENIDO: la direccion MODELOS escribe los seis en su directorio
# temporal, los REVALIDA con el lector de produccion —los dos dialectos del v2:
# denso de 1 frame y con f0 por frame de 4 frames— y carga los cuatro del banco
# en las ranuras A-D, que es lo que la pagina ensena. Antes aqui viajaba UN solo
# modelo (el temporal de CZ-BASS1) y las otras tres ranuras eran JSON sinteticos
# de un parcial: el E2E medía el formato, no el material.
# NOTA: los assets van en el directorio del plugin (no en Tests/) porque el
# generador de JUCE nombra los simbolos por contenido; el include lo consume
# BridgeSelftest.h via NeuronikSelftestAssets.h.
juce_add_binary_data(NEURONiK_SelftestAssets
    HEADER_NAME "NeuronikSelftestAssets.h"
    NAMESPACE "NeuronikSelftestAssets"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Models/CZ-BASS1.neuronikmodel"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Models/CZ-HAMOG.neuronikmodel"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Models/CZ-PAD1.neuronikmodel"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Models/CZ-SWEP1.neuronikmodel"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Models/CZ-RRISE-temporal.neuronikmodel"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Models/CZ-BASS1-temporal.neuronikmodel"
)

# MODELOS DE FABRICA (banco CZ101 real): los seis .neuronikmodel generados por
# la sonda del ModelMaker (ModelMakerRealWavProbe) desde los WAV del banco
# (ABDCZ101/DOCS/patches). Viajan EMBEBIDOS y el processor los INSTALLA en
# Documents/NEURONiK/Models/ en el arranque (installFactoryModels): los
# presets de fabrica referencian esos ficheros por modelPath<slot>, el mismo
# mecanismo de siempre, y un usuario que borre Documents los recupera solo.
# Los SIETE presets (Assets/Presets, generados offline por
# Tests/FactoryPresetGenerator.cpp) viajan en el MISMO binario — uno por
# modelo mas CZ101-BANK, el BANCO en un preset (los cuatro patches tonales
# en las cuatro esquinas del pad XY):
# installFactoryPresets() los escribe a Documents/NEURONiK/Presets/ (flat,
# getAllPresets no es recursivo) sustituyendo {{FACTORY_MODELS}} por ese
# directorio de modelos.
juce_add_binary_data(NEURONiK_FactoryModels
    HEADER_NAME "NeuronikFactoryModels.h"
    NAMESPACE "NeuronikFactoryModels"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Models/CZ-BASS1.neuronikmodel"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Models/CZ-HAMOG.neuronikmodel"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Models/CZ-PAD1.neuronikmodel"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Models/CZ-SWEP1.neuronikmodel"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Models/CZ-RRISE-temporal.neuronikmodel"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Models/CZ-BASS1-temporal.neuronikmodel"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Presets/CZ-BASS1.neuronikpreset"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Presets/CZ-HAMOG.neuronikpreset"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Presets/CZ-PAD1.neuronikpreset"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Presets/CZ-SWEP1.neuronikpreset"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Presets/CZ-RRISE-temporal.neuronikpreset"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Presets/CZ-BASS1-temporal.neuronikpreset"
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Presets/CZ101-BANK.neuronikpreset"
)

# WEBVIEW2 BENCH (bancada de la WebUI + panel nativo)
# ============================================================================

# Ya retirado el piloto (ticket 8.4), esta bancada sirve la UNICA pagina que hay
# (`WebUI/dist`, la que embebe el plugin) desde disco, sin snapshot embebido: el que
# lleva su WebUI dentro es el PLUGIN, y contestar una peticion con otra pagina no es un
# fallback. Por eso aqui no hay FATAL_ERROR que exigir ni juce_add_binary_data que
# embeber — esa fue la otra mitad de la retirada (el exe llevaba el export del piloto).
#
# Sigue existiendo porque es la UNICA superficie que monta el panel nativo
# (ParameterPanel) y el XYPad de morph, y la unica que mide el arranque de la pagina
# (`--auto-quit` -> pilot-startup.log).

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
    # La ruta de la pagina va grabada (misma pagina que el plugin, `WebUI/dist`) y
    # `NEURONIK_WEBUI_DEV_DIR` (mismo override de desarrollo que el plugin) la puede
    # apuntar a otra exportacion.
    NEURONiK_WEBUI_DIR="${NEURONIK_WEBUI_DIST_DIR}"
    # Plugin sources (copied below) reference JucePlugin_Name; the gui app target
    # does not generate plugin macros, so pin the same value the plugin target uses.
    JucePlugin_Name="NEURONiK"
)

# The panel's UI dependencies (RotaryControl, GlassBox, MidiLearner, themes...)
# and the processor's DSP chain: the same set the Standalone links.
target_link_libraries(NEURONiK_WebPilotHost PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices   # AudioProcessorPlayer: renders audio so page MIDI notes sound
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
    # BridgeSelftest.h (address MODELS) loads the embedded temporal model.
    NEURONiK_SelftestAssets
    # installFactoryModels() (NEURONiKProcessor) writes the CZ101 bank models.
    NEURONiK_FactoryModels
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

# Version bump (Windows) - RELEASE-GATED (2026-09-20).
# Antes: un custom command con OUTPUT ficticio (siempre "caducado") corria el ps1 en
# CADA compilacion y quemaba un numero de Version.h (fichero versionado en git) por
# build. Ahora el ps1 SOLO incrementa si la build viene marcada como release
# (NEURONIK_MM_RELEASE=1; build.bat modelmaker release lo pone); como PRE_BUILD del
# target corre siempre, pero sin la marca es un no-op barato. La version oficial del
# plugin (juce plugin version) vive en CMakeLists y NO depende de esto.
set(NEURONIK_MODELMAKER_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/update_version.ps1")

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

# El bump corre como PRE_BUILD (siempre se ejecuta, es barato) pero el ps1 SOLO
# incrementa si la build viene marcada release (NEURONIK_MM_RELEASE=1, ver el ps1).
add_custom_command(
    TARGET NEURONiK_ModelMaker PRE_BUILD
    COMMAND powershell -ExecutionPolicy Bypass -File "${NEURONIK_MODELMAKER_VERSION_SCRIPT}"
    COMMENT "ModelMaker version bump (solo incrementa en builds marcadas release)..."
    VERBATIM
)

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


