# AUDITORÍA EXTENDIDA - Problemas Críticos Adicionales
## Fecha: 2026-01-30
## Prioridad: CRÍTICA para Fork NEUROTiK

---

## 🚨 NIVEL CRÍTICO - SEGURIDAD & ROBUSTEZ

### 1. ⚠️ CRÍTICO: Cast Peligroso en CustomUIComponents

**Severidad**: CRASH POTENCIAL / USE-AFTER-FREE

**Ubicación**: Código UI con properties de sliders

```cpp
// ❌ EXTREMADAMENTE PELIGROSO
auto* modAtom = reinterpret_cast<std::atomic<float>*>(static_cast<std::intptr_t>(
    (juce::int64)slider.getProperties()["modValue"]));
```

**Problemas**:
1. **Use-after-free**: Si UI se destruye mientras audio thread lee el puntero
2. **Strict aliasing violation**: Comportamiento indefinido en C++
3. **Race condition**: No hay sincronización entre creación/destrucción de UI y acceso desde audio

**Solución para NEUROTiK**:
```cpp
// ✅ SEGURO: Usar índices en array gestionado por Processor
class NEURONiKProcessor {
    std::array<std::atomic<float>, NUM_PARAMS> modulationValues;
public:
    std::atomic<float>& getModulationValue(int paramIndex) {
        jassert(paramIndex >= 0 && paramIndex < NUM_PARAMS);
        return modulationValues[paramIndex];
    }
};

// En UI:
slider.getProperties().set("modValueIndex", paramIndex); // Solo índice, no puntero
```

**Prioridad**: 🔴 MÁXIMA - Arreglar ANTES del fork

---

### 2. ⚠️ Validación de Nullptr Ausente

**Severidad**: CRASH EN PRODUCCIÓN

**Ubicación**: Múltiples sitios en código UI

```cpp
// ❌ CRASH si param es null
auto* param = vts.getParameter(id);
param->setValueNotifyingHost(val);
```

**Solución**:
```cpp
// ✅ Con validación
auto* param = vts.getParameter(id);
jassert(param != nullptr); // Debug
if (param != nullptr) {    // Release
    param->setValueNotifyingHost(val);
}
```

**Acción**: Añadir macro helper:
```cpp
#define SAFE_PARAM_SET(vts, id, value) \
    if (auto* p = vts.getParameter(id)) { \
        p->setValueNotifyingHost(value); \
    } else { \
        jassertfalse; \
        DBG("Missing parameter: " << id); \
    }
```

**Prioridad**: 🔴 ALTA

---

### 3. ⚠️ Inconsistencias de Namespace (Código Zombi)

**Severidad**: LINKER ERRORS / CÓDIGO MUERTO

**Ubicación**: `Resonator_v1.cpp` línea ~150

```cpp
} // namespace Nexus::DSP::Core  // ← Rompe linkage
```

**Acción Inmediata**:
```bash
grep -r "namespace Nexus" Source/
grep -r "Nexus::" Source/
```

**Prioridad**: 🟡 MEDIA - Limpiar antes del fork

---

## 🏗️ ARQUITECTURA & MANTENIBILIDAD

### 4. ⚠️ Duplicación de Código entre Motores (80%)

**Problema**: `NeuronikEngine.cpp` y `NeurotikEngine.cpp` comparten:
- FX chain (saturation, delay, chorus, reverb)
- MIDI handling
- LFO processing
- Modulation matrix

**Impacto**: Bug fixes requieren cambios en 2 lugares → riesgo de inconsistencias

**Solución para NEUROTiK**:
```cpp
// BaseEngine.h
class BaseEngine : public ISynthesisEngine {
protected:
    // Común a todos los motores
    Core::LFO lfo1, lfo2;
    Effects::Saturation saturation;
    Effects::Delay delay;
    Effects::Chorus chorus;
    Effects::Reverb reverb;
    
    virtual void renderVoices(AudioBuffer<float>& buffer) = 0;
    
public:
    void renderNextBlock(AudioBuffer<float>& buffer, MidiBuffer& midi) final {
        processLFOs(buffer.getNumSamples());
        updateParameters();
        applyModulation();
        processMidi(midi);
        
        renderVoices(buffer); // ← Único método virtual
        
        applyFXChain(buffer);
        applyMasterGain(buffer);
    }
};

// NeuronikEngine.h
class NeuronikEngine : public BaseEngine {
protected:
    void renderVoices(AudioBuffer<float>& buffer) override {
        // Solo lógica específica de síntesis aditiva
    }
};
```

**Prioridad**: 🟡 MEDIA - Implementar en fork

---

### 5. ⚠️ Paths de Include Caóticos

**Problema**: Rutas relativas frágiles
```cpp
#include "../../../Source/Common/SpectralModel.h"
```

**Solución NEUROTiK**: Módulos JUCE
```cpp
// En juce_modules/neurotik_common/
#include <neurotik_common/SpectralModel.h>
```

**Prioridad**: 🟢 BAJA - Refactor gradual

---

### 6. ⚠️ Gestión de Memoria en Model Maker

**Problema**: FFT analysis crea `std::vector<float> fftData` en stack
- Archivo de 10 minutos @ 48kHz = ~28.8M samples
- Stack overflow potencial

**Solución**:
```cpp
class ModelMaker {
    std::vector<float> fftBuffer; // Miembro de clase (heap)
    
    void analyzeAudio(const AudioBuffer<float>& audio) {
        fftBuffer.resize(audio.getNumSamples());
        // Usar fftBuffer en lugar de variable local
    }
};
```

**Prioridad**: 🟡 MEDIA

---

## 🔧 BUGS SILENCIOSOS DE C++/JUCE

### 7. ⚠️ juce::String en Audio Thread

**Regla**: NUNCA pasar `juce::String` a través de interfaz `IVoice`
- `juce::String` hace heap allocation
- No es real-time safe

**Verificación**: Auditar todas las firmas de métodos en `IVoice.h`

**Prioridad**: 🟢 BAJA - Parece correcto actualmente

---

### 8. ⚠️ Constructor de Copia de SpectralModel

**Problema**: Copiar 512 bytes (128 floats) por cada nota
```cpp
struct SpectralModel {
    std::array<float, 64> amplitudes;      // 256 bytes
    std::array<float, 64> frequencyOffsets; // 256 bytes
};
```

**Optimización**:
```cpp
// Opción 1: Shared ownership
using SpectralModelPtr = std::shared_ptr<const SpectralModel>;

// Opción 2: Unique ownership si cada voz muta
using SpectralModelPtr = std::unique_ptr<SpectralModel>;
```

**Prioridad**: 🟢 BAJA - Optimización, no bug

---

### 9. ⚠️ Inicialización en ResonatorBank

**Problema**:
```cpp
alignas(16) float b0_v[128] = {0}; // Solo primer elemento garantizado
```

**Fix**:
```cpp
alignas(16) float b0_v[128]{}; // Value initialization (todos a 0)
// O en constructor:
std::fill(std::begin(b0_v), std::end(b0_v), 0.0f);
```

**Prioridad**: 🟡 MEDIA - Comportamiento indefinido potencial

---

## 🎛️ DSP/AUDIO ESPECÍFICO

### 10. ⚠️ Falta de Manejo de Denormals en SIMD

**Problema**: Operaciones SIMD sin flush denormals
- En CPUs Intel antiguas: 100x degradación de performance

**Solución**:
```cpp
void ResonatorBank::prepare(double sampleRate, int samplesPerBlock) {
    #ifdef __SSE__
    _mm_setcsr(_mm_getcsr() | 0x8040); // FTZ + DAZ
    #endif
    // O confiar en juce::ScopedNoDenormals en Processor
}
```

**Nota**: Ya usas `ScopedNoDenormals` en Processor ✅

**Prioridad**: 🟢 BAJA - Ya mitigado

---

### 11. ⚠️ Desbordamiento de Fase en LFO

**Problema**: Acumulación de error de punto flotante
```cpp
phase_ += phaseIncrement_;
if (phase_ >= 1.0f) phase_ -= 1.0f;
```

**Solución**:
```cpp
// Opción 1: fmod
phase_ = std::fmod(phase_ + phaseIncrement_, 1.0f);

// Opción 2: Double precision para acumulador
double phaseAccumulator_ = 0.0;
phaseAccumulator_ += phaseIncrement_;
phase_ = static_cast<float>(std::fmod(phaseAccumulator_, 1.0));
```

**Prioridad**: 🟢 BAJA - Solo tras horas de ejecución

---

### 12. ⚠️ Latencia en Analysis (Model Maker)

**Problema**: FFT de 8192 samples = 170ms @ 48kHz
- No es tiempo real

**Acción**: Documentar que análisis es offline-only

**Prioridad**: 🟢 BAJA - Documentación

---

## 🚀 CONSIDERACIONES PARA FORK NEUROTiK

### 13. ⚠️ Compatibilidad de Formatos de Modelo

**Problema**: `.neuronikmodel` actual = arrays raw de 64 floats
- NEUROTiK con redes neuronales = tensores de 8-16 latents

**Estrategia de Migración**:
```json
// Versión 1.0 (AXIONiK)
{
  "version": "1.0",
  "type": "spectral",
  "partials": [0.5, 0.3, ...]
}

// Versión 2.0 (NEUROTiK)
{
  "version": "2.0",
  "type": "neural",
  "latents": [0.1, -0.5, ...],
  "weights": "base64_encoded_onnx"
}
```

**Prioridad**: 🟡 MEDIA - Planificar ahora

---

### 14. ⚠️ ONNX Runtime y Distribución

**Consideraciones**:
- **Licencia**: MIT (OK para comercial) + atribución requerida
- **Tamaño**: +5-10MB al plugin
- **CPU/GPU**: Manejar fallbacks si modelo requiere CUDA

**Checklist**:
- [ ] Incluir LICENSE de ONNX Runtime
- [ ] Detectar GPU disponible en runtime
- [ ] Fallback a CPU si GPU no disponible
- [ ] Documentar requisitos de sistema

**Prioridad**: 🟡 MEDIA - Para fase de integración ONNX

---

### 15. ⚠️ Threading en Training (Model Maker)

**Problema**: No usar `juce::Thread` para training pesado
- Bloquea message thread

**Solución**:
```cpp
class ModelTrainer : public juce::ThreadPoolJob {
    Result runJob() override {
        // Training loop aquí
        return jobHasFinished;
    }
};

// En Model Maker:
juce::ThreadPool pool(1);
pool.addJob(new ModelTrainer(...), true);
```

**Prioridad**: 🟢 BAJA - Para futuro training

---

## ⚠️ CODE SMELLS MENORES (PERO IMPORTANTES)

### 16. Números Mágicos

**Problema**: `64` hardcodeado en 200+ lugares

**Solución**:
```cpp
namespace Constants {
    constexpr size_t NumPartials = 64;
    constexpr size_t MaxVoices = 32;
    constexpr size_t ModSlots = 4;
}
```

**Prioridad**: 🟢 BAJA - Refactor gradual

---

### 17. Deprecated JUCE APIs

**Problema**: `juce::FontOptions::withStyle("Bold")` puede deprecarse

**Solución**:
```cpp
// Usar enums
juce::Font(14.0f, juce::Font::bold);
```

**Prioridad**: 🟢 BAJA

---

### 18. ⚠️ CRÍTICO: Inclusión de `<immintrin.h>`

**Problema**: Header específico de x86/x64
- No corre en ARM Macs (Apple Silicon)

**Solución**:
```cpp
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <immintrin.h>
    #define USE_SSE 1
#else
    #define USE_SSE 0
#endif

// En código:
#if USE_SSE
    // Path SIMD
#else
    // Path scalar fallback
#endif
```

**Prioridad**: 🔴 ALTA - Apple Silicon es mainstream

---

### 19. Gestión de Errores de Archivo

**Problema**: `File::replaceWithText` puede fallar por permisos

**Solución**:
```cpp
juce::Result result = file.replaceWithText(content);
if (!result.wasOk()) {
    DBG("Failed to save: " << result.getErrorMessage());
    // Mostrar error al usuario
}
```

**Prioridad**: 🟡 MEDIA

---

### 20. Memory Alignment en Resonator

**Problema**: `alignas(16)` correcto para SSE, pero verificar alignment en clase

**Solución**:
```cpp
class ResonatorBank {
    alignas(16) std::array<float, 128> b0_v;
    // Verificar que clase también esté alineada si es miembro
};
```

**Prioridad**: 🟢 BAJA - Verificar en profiling

---

## ✅ CHECKLIST PRE-FORK NEUROTiK

### Prioridad CRÍTICA (Antes de fork)
- [ ] **#1**: Eliminar `reinterpret_cast` de UI properties → usar índices
- [ ] **#2**: Añadir validación nullptr en todos los `getParameter()`
- [ ] **#18**: Añadir fallback ARM64 para código SSE

### Prioridad ALTA (Primera semana del fork)
- [ ] **#3**: Eliminar namespace `Nexus` (código zombi)
- [ ] **#4**: Extraer `BaseEngine` común
- [ ] **#9**: Corregir inicialización de arrays en `ResonatorBank`

### Prioridad MEDIA (Primer mes del fork)
- [ ] **#5**: Migrar a includes modulares
- [ ] **#6**: Mover `fftData` a heap en Model Maker
- [ ] **#13**: Diseñar formato de archivo versionado
- [ ] **#19**: Añadir manejo de errores de archivo

### Prioridad BAJA (Optimizaciones futuras)
- [ ] **#8**: Evaluar `shared_ptr` para `SpectralModel`
- [ ] **#11**: Mejorar precisión de fase en LFO
- [ ] **#16**: Extraer constantes mágicas
- [ ] **#17**: Actualizar APIs deprecated de JUCE

---

## 📊 RESUMEN DE IMPACTO

| Categoría | Crítico | Alto | Medio | Bajo | Total |
|-----------|---------|------|-------|------|-------|
| Seguridad & Robustez | 2 | 1 | 1 | 0 | 4 |
| Arquitectura | 0 | 0 | 3 | 1 | 4 |
| Bugs C++/JUCE | 0 | 0 | 1 | 3 | 4 |
| DSP/Audio | 0 | 0 | 0 | 4 | 4 |
| Fork NEUROTiK | 0 | 0 | 2 | 1 | 3 |
| Code Smells | 0 | 1 | 1 | 3 | 5 |
| **TOTAL** | **2** | **2** | **8** | **12** | **24** |

---

## 🎯 PLAN DE ACCIÓN INMEDIATO

### Esta Semana (Pre-Fork)
1. ✅ Eliminar `ScopedLock` en audio thread (COMPLETADO)
2. 🔴 Eliminar `reinterpret_cast` en UI properties
3. 🔴 Añadir validación nullptr
4. 🔴 Añadir fallback ARM64

### Próxima Semana (Inicio Fork)
1. Crear `BaseEngine` común
2. Limpiar namespace `Nexus`
3. Corregir inicialización de arrays

### Primer Mes (Consolidación Fork)
1. Migrar a includes modulares
2. Diseñar formato de archivo versionado
3. Documentar análisis offline-only

---

**Documento generado**: 2026-01-30 12:45:00  
**Próxima revisión**: Antes de crear branch `neurotik-fork`
