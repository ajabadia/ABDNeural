# AUDITORÍA DE CÓDIGO - AXIONiK Synthesizer
## Fecha: 2026-01-30
## Build: #34

---

## 🚨 PROBLEMAS CRÍTICOS CORREGIDOS

### 1. ✅ Bloqueo del Audio Thread (RESUELTO)

**Problema**: Uso de `juce::ScopedLock` (CriticalSection) en `processBlock()` causaba priority inversion y posibles dropouts.

**Ubicación**: `Source/Main/NEURONiKProcessor.cpp`, líneas 280-286

**Solución Implementada**: Sistema lock-free de double buffering para MIDI

```cpp
// ANTES (❌ PELIGROSO):
{
    const juce::ScopedLock midiLock(midiBufferLock);
    if (!uiMidiBuffer.isEmpty()) {
        midiMessages.addEvents(uiMidiBuffer, 0, -1, 0);
        uiMidiBuffer.clear();
    }
}

// AHORA (✅ LOCK-FREE):
if (midiBufferReady.load(std::memory_order_acquire)) {
    audioMidiBuffer.swapWith(uiMidiBuffer);
    midiBufferReady.store(false, std::memory_order_release);
    if (!audioMidiBuffer.isEmpty()) {
        midiMessages.addEvents(audioMidiBuffer, 0, -1, 0);
        audioMidiBuffer.clear();
    }
}
```

**Impacto**: Eliminado riesgo de glitches/dropouts en audio thread. Thread-safety garantizada con atomics.

---

### 2. ✅ LFO Processing Error (RESUELTO)

**Problema**: LFOs usaban `processSample()` en lugar de `processBlock()`, causando:
- Velocidad 512x más lenta de lo esperado
- "Petardeo" audible
- Modulación errática

**Ubicación**: `Source/DSP/CoreModules/NeuronikEngine.cpp` y `NeurotikEngine.cpp`

**Solución Implementada**:
```cpp
// ANTES (❌ INCORRECTO):
lfo1Value.store(lfo1.processSample(), ...);

// AHORA (✅ CORRECTO):
const int numSamples = buffer.getNumSamples();
lfo1Value.store(lfo1.processBlock(numSamples), ...);
```

**Impacto**: 
- LFOs funcionan a velocidad correcta
- Modulación suave y continua
- No más glitches audibles

---

### 3. ✅ Orden de Ejecución de Modulación (RESUELTO)

**Problema**: LFOs se procesaban DESPUÉS de aplicar modulación, causando desfase de 1 bloque.

**Solución**: Reordenado `renderNextBlock()` para procesar LFOs PRIMERO:

```cpp
void NeuronikEngine::renderNextBlock(...) {
    // 1. Process LFOs FIRST
    lfo1Value.store(lfo1.processBlock(numSamples), ...);
    
    // 2. Update Parameters (includes applyModulation)
    updateParameters();
    
    // 3. Render Voices (uses modulated values)
    for (auto& v : voices)
        v->renderNextBlock(buffer, 0, numSamples);
}
```

**Impacto**: Matriz de modulación funciona correctamente sin latencia.

---

## ⚠️ PROBLEMAS IDENTIFICADOS (PENDIENTES)

### 4. Recálculo Costoso en Hot Path

**Ubicación**: `Source/DSP/CoreModules/Resonator.cpp`

**Problema**: `updateHarmonicsFromModels()` recalcula 64 armónicos con `std::pow` por bloque.
- Con 32 voces @ 64 samples/block: ~1.4 millones de `pow()` por segundo

**Recomendación**:
- Implementar dirty flags para cachear resultados cuando morphX/Y no cambien
- Pre-calcular curvas de pow en tablas (LUT)
- Usar SIMD intrinsics para paralelizar cálculo de 64 parciales

**Prioridad**: MEDIA (afecta CPU usage pero no estabilidad)

---

### 5. Branching por Sample en Resonator

**Ubicación**: `Source/DSP/CoreModules/Resonator.cpp`

**Problema**:
```cpp
float Resonator::processSample() noexcept {
    if (entropyAmount > 0.001f) { // ❌ Branch per sample
        float r1 = fastFloatRand(randomSeed);
        // ...
    }
}
```

**Recomendación**: Calcular jitter de phase en bloque, no verificar condicionalmente por sample.

**Prioridad**: BAJA (optimización de rendimiento)

---

### 6. Gestión de Memoria en Voices

**Ubicación**: `Source/DSP/CoreModules/NeuronikEngine.cpp`

**Problema**: `std::vector<std::unique_ptr<>>` causa indirecciones de cache.

**Recomendación**: Usar `std::array<AdditiveVoice, 32>` para mejor cache locality.

**Prioridad**: BAJA (requiere refactoring de AdditiveVoice)

---

### 7. Anti-Aliasing Insuficiente

**Ubicación**: `Source/DSP/CoreModules/Oscillator.cpp`

**Problema**: Osciladores Saw/Square no implementan PolyBLEP o oversampling.

**Nota**: Menos crítico en síntesis aditiva pura (controlamos parciales), pero módulo standalone podría aliasar.

**Prioridad**: BAJA (no afecta motor principal)

---

## ✅ ASPECTOS POSITIVOS (MANTENER)

1. **Separación Editor/Processor**: Uso correcto de JUCE APVTS
2. **Thread-safety básica**: Uso de `std::atomic<float>` para parámetros compartidos
3. **Modulation Matrix**: Arquitectura flexible con 4 slots
4. **Sistema de Modelos Espectrales**: Concepto de 4 esquinas intuitivo
5. **UI Glass Design**: LookAndFeel coherente sin allocaciones en `paint()`
6. **MIDI Learn**: Sistema completo con persistencia

---

## 🎯 NOMENCLATURA TÉCNICA

### Aclaración: ¿Es "Neural"?

**Respuesta**: NO técnicamente.

**AXIONiK Actual**:
- Motor: 64 osciladores sinusoidales (síntesis aditiva)
- Representación: Arrays de amplitudes estáticas
- Morphing: LERP bilineal 2D entre 4 espectros
- Análisis: FFT + detección de armónicos (HPS)

**Sintetizador Neural Real (ej. Hartmann Neuron)**:
- Motor: Redes neuronales (SOM/MLP) + Resíntesis
- Representación: Pesos sinápticos (espacio latente)
- Morphing: Interpolación no-lineal en espacio neuronal
- Análisis: Entrenamiento de redes sobre audio

**Recomendación de Branding**: 
- "Spectral Morphing Synthesizer"
- "Hybrid Spectral Synthesizer" ✅ (ACTUAL)
- Evitar "Neural" para evitar confusiones técnicas

---

## 🚀 ROADMAP PARA NEUROTIK (FORK)

### Fase 0: Emergencias (✅ COMPLETADO)
- [x] Eliminar ScopedLock de processBlock
- [x] Corregir procesamiento de LFOs
- [x] Optimizar orden de ejecución de modulación

### Fase 1: Modularización (PRÓXIMO)
- [ ] Crear módulos JUCE independientes:
  - `CommonUI/` (GlassBox, Knobs, MidiLearner)
  - `Persistence/` (PresetManager, Serialization)
  - `DSPUtils/` (Envelope, LFO genéricos)

### Fase 2: Arquitectura Hexagonal
- [ ] Dominio (Core): Interfaz `IVoice`, `SpectralModel` como POD
- [ ] Adapters: `AdditiveVoice` (legacy), `NeuralVoice` (futuro, ONNX Runtime)
- [ ] UI: Desacoplado del DSP específico

### Fase 3: Motor Dual
```cpp
enum class EngineMode { ClassicAdditive, NeuralExperimental };
std::unique_ptr<IVoice> voice = (mode == Neural) 
    ? std::make_unique<NeuralVoice>(onnxSession)
    : std::make_unique<AdditiveVoice>();
```

---

## 📊 ESTADO ACTUAL

**Build**: #34  
**Compilación**: ✅ Exitosa  
**Estabilidad**: ✅ Thread-safe (audio thread lock-free)  
**Funcionalidad**: ✅ Matriz de modulación operativa  
**Rendimiento**: ⚠️ Optimizable (ver puntos 4-7)  

**Veredicto**: **LISTO PARA TESTING INTERNO**

Prioridad inmediata para release pública:
1. ✅ Thread-safety (COMPLETADO)
2. ⚠️ Optimización DSP (PENDIENTE - no bloqueante)
3. ⚠️ Anti-aliasing (PENDIENTE - no crítico)

---

## 📝 NOTAS TÉCNICAS

### Lock-Free MIDI Injection

El sistema implementado usa:
- **Double Buffering**: Dos buffers MIDI pre-alocados
- **Atomic Flag**: `std::atomic<bool> midiBufferReady`
- **Memory Ordering**: `acquire`/`release` para sincronización

**Garantías**:
- Zero allocations en audio thread
- Zero locks en audio thread
- Wait-free para UI thread (solo atomic store)
- Lock-free para audio thread (atomic load + swap)

### LFO Processing

**Corrección Crítica**:
- Antes: 1 sample de avance por bloque → velocidad incorrecta
- Ahora: N samples de avance por bloque → velocidad correcta

**Ejemplo** (LFO @ 1Hz, 48kHz, 512 samples/block):
- Antes: 94 samples/segundo → 0.00196 Hz (512x más lento)
- Ahora: 48000 samples/segundo → 1.0 Hz ✅

---

**Documento generado automáticamente por auditoría de código**  
**Última actualización**: 2026-01-30 12:40:00
