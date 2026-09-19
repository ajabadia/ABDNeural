cmake_minimum_required(VERSION 3.22)
project(NEURONiK_WASM LANGUAGES CXX)

# ============================================================================
# NEURONiK DSP -> WebAssembly (Fase 5)
#
# Compiles the REAL DSP (no port, no duplicated source list) through the
# Phase-1 boundary:
#   JS -> NeuronikWasmBridge (extern "C") -> Runtime::DspEngineFacade -> engine
#
# Build:  build_wasm.bat   (or:  emcmake cmake -G Ninja -S wasm -B build-wasm)
#
# ANTI-DRIFT (lección ABDMS2000): su build WASM duplicaba la lista de fuentes
# y se quedó 4 ficheros atrás (incluido SynthEngine.cpp). Aquí la lista vive
# en el CMakeLists raíz como NEURONIK_DSP_SOURCES y este proyecto la incluye.
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# gnu++17 (no c++17 estricto): juce_ThreadPriorities_native.h sin rama WASM deja
# la tabla vacia (Entry[0], extension GNU de array de longitud cero) y el modo
# estricto la rechaza. Con extensiones GNU compila y el comportamiento en
# runtime es inocuo (sin prioridades de hilo en WASM).
set(CMAKE_CXX_EXTENSIONS ON)

get_filename_component(NEURONIK_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# JUCE 8.0.12 tiene soporte WASM nativo (#elif JUCE_WASM en juce_core.cpp):
# se compilan los modulos estandar, sin stubs ni parches.
if(NOT JUCE_PATH AND DEFINED ENV{JUCE_PATH})
    set(JUCE_PATH "$ENV{JUCE_PATH}")
endif()
if(NOT JUCE_PATH AND EXISTS "C:/JUCE/CMakeLists.txt")
    set(JUCE_PATH "C:/JUCE")
endif()
if(NOT JUCE_PATH)
    message(FATAL_ERROR "JUCE_PATH no encontrado (define JUCE_PATH o instala en C:/JUCE)")
endif()
message(STATUS "NEURONiK WASM: JUCE en ${JUCE_PATH}")

# El proyecto raiz (wasm/CMakeLists) se consume con emcmake -S wasm: la
# source dir ES este fichero; JUCE entra con nombre de binario dedicado.
add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")

# ============================================================================
# ABDSHAREDCODE - substrato DSP compartido (Fase 1 [5/6])
# ============================================================================
# El sustrato del motor (dsp::AudioBuffer, HeapBlock, SmoothedValue, puertos
# MIDI...) vive en ABDSharedCode::DspCore; los ficheros de Source/DSP/ son shims
# que lo re-exportan. Aqui solo hace falta la RUTA DE INCLUDE: DspCore y
# DspEffects son INTERFACE (todo inline/template), no hay nada que enlazar.
# A proposito NO se hace add_subdirectory(ABDSharedCode): seria cross-compilar
# con em++ targets de este repo que no tienen nada que ver con el motor
# (AutoUpdater, HardwareMidiDetect + sus assets embebidos) a cambio de nada.
set(ABDSHARED_CODE_DIR "${NEURONIK_ROOT}/../ABDSharedCode")
if(NOT EXISTS "${ABDSHARED_CODE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "ABDSharedCode no encontrado en ${ABDSHARED_CODE_DIR}: el motor toma de ahi "
        "el sustrato DSP (DspCore). Clona ABDSharedCode como hermano de ABDNeural.")
endif()
message(STATUS "NEURONiK WASM: ABDSharedCode en ${ABDSHARED_CODE_DIR}")

# Lista DSP compartida con el raiz (single-source, anti-drift). El puente
# wasm es el unico fichero exclusivo de este build.
include("${NEURONIK_ROOT}/DspSources.cmake")

set(WASM_DSP_SOURCES
    ${NEURONIK_ROOT}/Source/Wasm/NeuronikWasmBridge.cpp
    ${NEURONIK_DSP_SOURCES}
)

# NEURONIK_DSP_SOURCES viene con rutas relativas al raiz (Source/DSP/...):
# reconstruir absolutas para que CMake las resuelva desde este proyecto.
set(WASM_DSP_SOURCES_ABS "")
foreach(rel_src ${WASM_DSP_SOURCES})
    if(IS_ABSOLUTE "${rel_src}")
        list(APPEND WASM_DSP_SOURCES_ABS "${rel_src}")
    else()
        get_filename_component(abs_src "${NEURONIK_ROOT}/${rel_src}" ABSOLUTE)
        list(APPEND WASM_DSP_SOURCES_ABS "${abs_src}")
    endif()
endforeach()

add_executable(neuronik_dsp ${WASM_DSP_SOURCES_ABS})

target_include_directories(neuronik_dsp PRIVATE
    "${NEURONIK_ROOT}/Source"
    "${NEURONIK_ROOT}/Source/DSP"
    "${NEURONIK_ROOT}/Source/Common"
    "${ABDSHARED_CODE_DIR}"
)

# juce_SystemStats_wasm.cpp de JUCE 8.0.12 usa emscripten_get_now() sin
# incluir <emscripten.h> (bug de la version). Include forzado global: inocuo
# para el resto de TUs.
target_compile_options(neuronik_dsp PRIVATE "-includeemscripten.h")

target_link_libraries(neuronik_dsp PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_events
    juce::juce_dsp
)

# ============================================================================
# Enlazado: ES6 + MODULARIZE (AudioWorklet-friendly, convencion DEViLBOX),
# exports planas del bridge (convencion ABDMS2000).
# ============================================================================
set_target_properties(neuronik_dsp PROPERTIES
    OUTPUT_NAME "neuronik_dsp"
    SUFFIX ".js"
    LINK_FLAGS "-s MODULARIZE=1 -s EXPORT_ES6=1 -s ALLOW_MEMORY_GROWTH=1 \
-s MALLOC=emmalloc -s ENVIRONMENT=web,worker \
-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue','HEAPF32','HEAP32'] \
-s EXPORTED_FUNCTIONS=['_neuronikInit','_neuronikSetEngine','_neuronikProcess','_neuronikSetGlobalParams','_neuronikGlobalParamsSize','_neuronikGlobalParamsLayout','_neuronikModMatrixLayout','_neuronikAllNotesOff','_neuronikNumActiveVoices','_neuronikGetLfo','_malloc','_free'] \
-O2"
)
