# PLAN DE IMPLEMENTACIÓN - Correcciones Críticas Pre-Fork
## Objetivo: Resolver problemas de seguridad críticos antes del fork a NEUROTiK

---

## 📋 TAREAS PRIORITARIAS

### ✅ TAREA 1: Eliminar ScopedLock en Audio Thread
**Estado**: COMPLETADO ✅  
**Build**: #34  
**Descripción**: Reemplazado con sistema lock-free de double buffering para MIDI

---

### 🔴 TAREA 2: Eliminar reinterpret_cast Peligroso
**Prioridad**: CRÍTICA  
**Riesgo**: Use-after-free, crash en producción  
**Tiempo estimado**: 2-3 horas

#### Paso 1: Buscar Todas las Ocurrencias
```bash
grep -rn "reinterpret_cast.*Properties" Source/
grep -rn "getProperties().*modValue" Source/
```

#### Paso 2: Añadir Array de Modulación al Processor
```cpp
// En NEURONiKProcessor.h
class NEURONiKProcessor {
public:
    // Índices de parámetros para modulación
    enum ModulationTarget {
        MOD_MORPH_X = 0,
        MOD_MORPH_Y,
        MOD_CUTOFF,
        MOD_RESONANCE,
        // ... otros destinos
        MOD_TARGET_COUNT
    };
    
    // Array thread-safe para valores de modulación
    std::array<std::atomic<float>, MOD_TARGET_COUNT> modulationValues;
    
    // Getter seguro
    std::atomic<float>& getModulationValue(ModulationTarget target) {
        jassert(target >= 0 && target < MOD_TARGET_COUNT);
        return modulationValues[target];
    }
};
```

#### Paso 3: Refactorizar UI Components
```cpp
// ANTES (❌ PELIGROSO):
auto* modAtom = reinterpret_cast<std::atomic<float>*>(
    static_cast<std::intptr_t>((juce::int64)slider.getProperties()["modValue"]));

// DESPUÉS (✅ SEGURO):
int modIndex = slider.getProperties()["modValueIndex"];
auto& modValue = processor.getModulationValue(
    static_cast<NEURONiKProcessor::ModulationTarget>(modIndex));
```

#### Paso 4: Actualizar setupRotaryControl
```cpp
// En UIUtils.cpp
void setupRotaryControl(
    Component& parent,
    RotaryControl& control,
    const juce::String& paramID,
    const juce::String& label,
    juce::AudioProcessorValueTreeState& vts,
    NEURONiKProcessor& processor,
    juce::LookAndFeel& lnf,
    NEURONiKProcessor::ModulationTarget modTarget = NEURONiKProcessor::MOD_TARGET_COUNT)
{
    // ... setup existente ...
    
    if (modTarget != NEURONiKProcessor::MOD_TARGET_COUNT) {
        control.slider.getProperties().set("modValueIndex", (int)modTarget);
        control.slider.getProperties().set("hasModulation", true);
    }
}
```

#### Archivos a Modificar:
- `Source/Main/NEURONiKProcessor.h`
- `Source/UI/UIUtils.h`
- `Source/UI/UIUtils.cpp`
- `Source/UI/RotaryControl.h` (si usa modulation)
- Todos los paneles que usan `setupRotaryControl`

---

### 🔴 TAREA 3: Añadir Validación de Nullptr
**Prioridad**: ALTA  
**Riesgo**: Crash si parámetro no existe  
**Tiempo estimado**: 1 hora

#### Paso 1: Crear Macro Helper
```cpp
// En NEURONiKProcessor.h o archivo de utilidades
#define SAFE_PARAM_GET(vts, id) \
    [&]() -> juce::RangedAudioParameter* { \
        auto* p = vts.getParameter(id); \
        jassert(p != nullptr && "Parameter not found: " #id); \
        return p; \
    }()

#define SAFE_PARAM_SET(vts, id, value) \
    do { \
        if (auto* p = vts.getParameter(id)) { \
            p->setValueNotifyingHost(value); \
        } else { \
            jassertfalse; \
            DBG("Missing parameter: " << id); \
        } \
    } while(0)
```

#### Paso 2: Buscar Todos los Usos
```bash
grep -rn "getParameter(" Source/ | grep -v "jassert" | grep -v "if ("
```

#### Paso 3: Añadir Validaciones
```cpp
// ANTES:
auto* param = vts.getParameter(IDs::morphX);
param->setValueNotifyingHost(value);

// DESPUÉS:
SAFE_PARAM_SET(vts, IDs::morphX, value);
```

#### Archivos Críticos:
- `Source/Main/NEURONiKEditor.cpp`
- `Source/UI/Panels/*.cpp`
- `Source/Main/MidiMappingManager.cpp`

---

### 🔴 TAREA 4: Añadir Fallback ARM64
**Prioridad**: ALTA  
**Riesgo**: No funciona en Apple Silicon  
**Tiempo estimado**: 2 horas

#### Paso 1: Crear Header de Detección de Plataforma
```cpp
// Source/DSP/PlatformDetect.h
#pragma once

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define PLATFORM_X86 1
    #define PLATFORM_ARM 0
    #include <immintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define PLATFORM_X86 0
    #define PLATFORM_ARM 1
    #include <arm_neon.h>
#else
    #define PLATFORM_X86 0
    #define PLATFORM_ARM 0
#endif

// Feature detection
#if PLATFORM_X86
    #define HAS_SSE 1
    #define HAS_NEON 0
#elif PLATFORM_ARM
    #define HAS_SSE 0
    #define HAS_NEON 1
#else
    #define HAS_SSE 0
    #define HAS_NEON 0
#endif
```

#### Paso 2: Refactorizar Resonator con Paths Condicionales
```cpp
// Source/DSP/CoreModules/Resonator.cpp
#include "PlatformDetect.h"

float Resonator::processSample() noexcept {
    #if HAS_SSE
        return processSample_SSE();
    #elif HAS_NEON
        return processSample_NEON();
    #else
        return processSample_Scalar();
    #endif
}

#if HAS_SSE
float Resonator::processSample_SSE() noexcept {
    // Código SSE existente
    __m128 v0 = _mm_load_ps(&b0_v[0]);
    // ...
}
#endif

#if HAS_NEON
float Resonator::processSample_NEON() noexcept {
    // Equivalente NEON
    float32x4_t v0 = vld1q_f32(&b0_v[0]);
    // ...
}
#endif

// Siempre compilar fallback scalar
float Resonator::processSample_Scalar() noexcept {
    float output = 0.0f;
    for (int i = 0; i < 64; ++i) {
        // Versión sin SIMD
        output += b0_v[i] * amplitudes[i];
    }
    return output;
}
```

#### Paso 3: Actualizar CMakeLists.txt
```cmake
# Detectar arquitectura y añadir flags apropiados
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(NEURONiK PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/arch:AVX>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-msse4.1>
    )
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    # NEON está habilitado por defecto en ARM64
    message(STATUS "Building for ARM64 with NEON support")
endif()
```

#### Archivos a Modificar:
- `Source/DSP/PlatformDetect.h` (nuevo)
- `Source/DSP/CoreModules/Resonator.h`
- `Source/DSP/CoreModules/Resonator.cpp`
- `CMakeLists.txt`

---

## 🔍 VERIFICACIÓN Y TESTING

### Test 1: Validación de Nullptr
```cpp
// Test unitario
TEST_CASE("Parameter validation") {
    NEURONiKProcessor processor;
    auto& vts = processor.getAPVTS();
    
    // Debe fallar gracefully con parámetro inválido
    SAFE_PARAM_SET(vts, "invalid_param_id", 0.5f);
    // No debe crashear
}
```

### Test 2: Modulación Thread-Safe
```cpp
// Test de stress
TEST_CASE("Modulation thread safety") {
    NEURONiKProcessor processor;
    
    // Thread 1: UI escribe
    std::thread ui([&]() {
        for (int i = 0; i < 10000; ++i) {
            processor.getModulationValue(MOD_MORPH_X).store(
                (float)i / 10000.0f, std::memory_order_release);
        }
    });
    
    // Thread 2: Audio lee
    std::thread audio([&]() {
        for (int i = 0; i < 10000; ++i) {
            float val = processor.getModulationValue(MOD_MORPH_X).load(
                std::memory_order_acquire);
            // Usar valor
        }
    });
    
    ui.join();
    audio.join();
    // No debe crashear ni tener data races
}
```

### Test 3: ARM64 Compilation
```bash
# En Mac con Apple Silicon
cmake -B build -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Debug

# Verificar que compila sin errores
```

---

## 📊 CRONOGRAMA

| Tarea | Tiempo | Dependencias | Responsable |
|-------|--------|--------------|-------------|
| ✅ Tarea 1: Lock-free MIDI | - | - | Completado |
| 🔴 Tarea 2: Eliminar reinterpret_cast | 2-3h | - | Pendiente |
| 🔴 Tarea 3: Validación nullptr | 1h | - | Pendiente |
| 🔴 Tarea 4: Fallback ARM64 | 2h | - | Pendiente |
| Testing & Verificación | 1h | 2,3,4 | Pendiente |
| **TOTAL** | **6-7h** | | |

---

## ✅ CRITERIOS DE ACEPTACIÓN

### Tarea 2 (reinterpret_cast):
- [ ] Cero ocurrencias de `reinterpret_cast` en código UI
- [ ] Array de modulación implementado en Processor
- [ ] Todos los controles usan índices, no punteros
- [ ] Compila sin warnings
- [ ] Test de thread-safety pasa

### Tarea 3 (nullptr):
- [ ] Macro `SAFE_PARAM_GET/SET` implementada
- [ ] Todos los `getParameter()` validados
- [ ] Mensajes de debug para parámetros faltantes
- [ ] No crashes con parámetros inválidos

### Tarea 4 (ARM64):
- [ ] `PlatformDetect.h` creado
- [ ] Paths SSE/NEON/Scalar implementados
- [ ] Compila en x86_64 (Windows/Mac Intel)
- [ ] Compila en arm64 (Mac Apple Silicon)
- [ ] Tests de audio pasan en ambas plataformas

---

## 🚀 PRÓXIMOS PASOS POST-CORRECCIÓN

1. **Commit & Tag**: `git tag v1.0.0-pre-fork-stable`
2. **Branch Fork**: `git checkout -b neurotik-fork`
3. **Extraer BaseEngine**: Implementar clase base común
4. **Limpiar Namespace**: Eliminar referencias a `Nexus`
5. **Documentación**: Actualizar README con arquitectura

---

**Plan creado**: 2026-01-30 12:50:00  
**Revisión**: Antes de iniciar implementación  
**Aprobación requerida**: Usuario
