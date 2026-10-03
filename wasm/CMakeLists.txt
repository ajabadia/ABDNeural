cmake_minimum_required(VERSION 3.22)
# LANGUAGES C y no solo CXX: JUCE exige un compilador C para los targets que
# dependan de el, y aborta el configure si no esta declarado. Medido el 2026-10-02
# con JUCE 8.0.12 (la version que espera este proyecto): "A C compiler is required
# to build targets that depend on JUCE. Add 'C' to your project's LANGUAGES", en el
# CMakeLists de JUCE, no en este. El proyecto solo tiene C++, asi que esto no anade
# ningun fichero .c: solo le dice a CMake que active el chequeo del toolchain de C.
project(NEURONiK_WASM LANGUAGES C CXX)

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
# gnu++17 (no c++17 estricto). MEDIDO: esto NO es lo que arregla la tabla de
# prioridades, aunque durante un tiempo lo pareciese. Con la tabla vacia el
# compilador dice `no matching function for call to 'size'` TAMBIEN en modo GNU:
# lo que falla es que `std::size` no tiene candidato para `Entry[0]`, y el error
# es de sobrecarga, no de longitud de array. Lo que de verdad rellena la tabla
# esta mas abajo, en el parche de `juce_ThreadPriorities_native.h`. La extension
# GNU se queda porque no hace dano y hay TUs que la usan.
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

# ============================================================================
# JUCE NO PARCHEADO: EL ARREGLO ESTA AQUI, EN EL CODIGO DEL PROYECTO
# ============================================================================
#
# MEDIDO. Compilando `neuronik_dsp` con em++ sale, en `juce_core.cpp`:
#
#   juce_ThreadPriorities_native.h:94: error: no matching function for call
#   to 'size'   (static_assert (std::size (table) == 5, ...))
#
# Y la nota del compilador dice `_Cont = Entry[0]`, o sea que la tabla `table`
# de `juce::ThreadPriorities` sale VACIA. La causa: `juce_TargetPlatform.h`
# declara `JUCE_WASM` para `__wasm__`, pero el `table` de esa cabecera solo
# tiene entradas para ANDROID, LINUX/BSD, MAC/IOS y WINDOWS. Con Emscripten no
# se cumple ninguna, la tabla se queda con cero elementos y el `static_assert`
# de "exactamente cinco" revienta.
#
# MEDIDO TAMBIEN que esto no lo arregla subir de version: se comprobo en
# 8.0.12, 8.0.13, 8.0.14, 8.0.15, 9.0.0, 9.0.2, 9.0.3 y en `master` del
# mirror. NINGUNA tiene la rama `JUCE_WASM`. El unico sitio donde estaba
# resuelto era un JUCE parcheado a mano en la maquina, y por eso el fallo
# salia solo en el CI, que clona el mirror.
#
# Que se parezca JUCE NO se toca: ni fork, ni rama, ni commit. Lo que se hace es
# anadir CINCO entradas al `table` del checkout que ya esta en disco, desde este
# fichero, que es codigo del proyecto. WebAssembly no tiene prioridades de hilo
# nativas --Emscripten corre de un solo hilo salvo que se linkee con pthreads, y
# este proyecto no lo hace-- asi que las cinco van en cero, igual que hace la
# rama de JUCE_LINUX con las suyas.
#
# IDEMPOTENTE: el `string(FIND)` mira si la rama ya esta y no hace nada. Cuando
# JUCE meta una entrada para WASM, esto deja de tocar el fichero, y lo que
# sobre es este bloque, que se puede borrar en un commit.
set(_juce_tp "${JUCE_PATH}/modules/juce_core/native/juce_ThreadPriorities_native.h")
if(EXISTS "${_juce_tp}")
    file(READ "${_juce_tp}" _juce_tp_txt)
    string(FIND "${_juce_tp_txt}" "#if JUCE_WASM" _juce_tp_ya)
    if(_juce_tp_ya EQUAL -1)
        set(_juce_tp_ancla "THREAD_PRIORITY_IDLE },\n       #endif\n    };")
        string(FIND "${_juce_tp_txt}" "${_juce_tp_ancla}" _juce_tp_col)
        if(_juce_tp_col GREATER -1)
            string(REPLACE
                "${_juce_tp_ancla}"
                "THREAD_PRIORITY_IDLE },\n       #endif\n\n       #if JUCE_WASM\n        { Thread::Priority::highest,    0 },\n        { Thread::Priority::high,       0 },\n        { Thread::Priority::normal,     0 },\n        { Thread::Priority::low,        0 },\n        { Thread::Priority::background, 0 },\n       #endif\n    };"
                _juce_tp_txt2
                "${_juce_tp_txt}")
            file(WRITE "${_juce_tp}" "${_juce_tp_txt2}")
            message(STATUS "NEURONiK WASM: anadidas las cinco entradas de JUCE_WASM")
        else()
            message(WARNING "juce_ThreadPriorities_native.h no tiene el cierre esperado del table; no se toca")
        endif()
    else()
        message(STATUS "NEURONiK WASM: JUCE ya trae la rama JUCE_WASM; no se toca")
    endif()
else()
    message(WARNING "no se encuentra ${_juce_tp}; el build wasm puede no compilar")
endif()

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

# Sin contraccion FMA: clang puede fusionar a*b+c en fmaf (libcall correctamente
# redondeada) mientras MSVC /O2 no contrae. La matematica determinista del
# sustrato (DspCore/DspMath.h) exige que las dos plataformas evaluen el mismo
# polinomio con el mismo redondeo; sin esta bandera no lo garantiza.
target_compile_options(neuronik_dsp PRIVATE "-ffp-contract=off")

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
