# BLOQUEADORES CRÍTICOS PARA RELEASE COMERCIAL
## Auditoría Final - Problemas Absolutamente Críticos
## Fecha: 2026-01-30

---

## 🚨 NIVEL CRÍTICO - BLOQUEADORES DE RELEASE

### 21. ⚠️ std::function en Audio Thread (Heap Allocation)

**Severidad**: CRÍTICA - ALLOCACIÓN EN TIEMPO REAL  
**Estado**: ✅ VERIFICADO - NO ENCONTRADO

**Búsqueda Realizada**:
```bash
grep -r "std::function" Source/DSP/
# Resultado: No se encontraron ocurrencias
```

**Qué Buscar**:
```cpp
// ❌ PROHIBIDO en DSP:
std::function<void()> onEnvelopeEnd;  // Heap allocation en operator=
std::function<float(float)> modCallback;

// ❌ PROHIBIDO en lambdas capturadas:
auto callback = [func = std::function<void()>(...)]() { };
```

**Solución Si Se Encuentra**:
```cpp
// ✅ Opción 1: Puntero a función crudo
typedef void(*EnvelopeCallback)(void* userData);

class Envelope {
    EnvelopeCallback callback = nullptr;
    void* callbackUserData = nullptr;
    
    void setCallback(EnvelopeCallback cb, void* data) {
        callback = cb;
        callbackUserData = data;
    }
    
    void triggerCallback() {
        if (callback) callback(callbackUserData);
    }
};

// ✅ Opción 2: Observer pattern con interfaz pura
class IEnvelopeListener {
public:
    virtual ~IEnvelopeListener() = default;
    virtual void onEnvelopeEnd() = 0;
};

class Envelope {
    IEnvelopeListener* listener = nullptr;
    
    void setListener(IEnvelopeListener* l) { listener = l; }
    void triggerCallback() {
        if (listener) listener->onEnvelopeEnd();
    }
};
```

**Prioridad**: 🟢 VERIFICADO - No requiere acción

---

### 22. ⚠️ Inicialización de Statics Locales (Mutex Implícito)

**Severidad**: CRÍTICA - LOCK EN AUDIO THREAD  
**Estado**: ✅ VERIFICADO - NO ENCONTRADO

**Búsqueda Realizada**:
```bash
grep -r "static const" Source/DSP/*.cpp
grep -r "static auto" Source/DSP/*.cpp
# Resultado: No se encontraron ocurrencias problemáticas
```

**Problema**:
```cpp
// ❌ PROHIBIDO en processSample():
float processSample() {
    static const float PI = 3.14159f;  // OK (literal, no allocation)
    static auto table = computeExpensiveTable(); // ¡ALLOCACIÓN + MUTEX!
    // C++11 garantiza thread-safety con mutex implícito
}
```

**Solución**:
```cpp
// ✅ Mover a miembros de clase
class Resonator {
    std::array<float, 1024> expensiveTable;
    
    void prepare(double sampleRate, int samplesPerBlock) {
        // Inicializar tabla aquí (non-realtime)
        expensiveTable = computeExpensiveTable();
    }
    
    float processSample() noexcept {
        // Usar tabla pre-calculada (realtime-safe)
        return expensiveTable[index];
    }
};
```

**Prioridad**: 🟢 VERIFICADO - No requiere acción

---

### 23. ⚠️ CRÍTICO: Propagación de NaN/Infinity

**Severidad**: CRÍTICA - MUTE TOTAL DEL DAW  
**Estado**: ❌ NO VALIDADO - REQUIERE IMPLEMENTACIÓN

**Problema**: 
Si el Resonator recibe frecuencia negativa o cero por error de modulación:
- Genera NaN que infecta toda la cadena de audio
- Causa mute total del master channel del DAW
- Difícil de debuggear (silencio total sin error visible)

**Ubicaciones Críticas a Proteger**:

#### 1. Resonator::setFrequency
```cpp
// Source/DSP/CoreModules/Resonator.cpp
void Resonator::setFrequency(float freq) noexcept {
    // ❌ FALTA VALIDACIÓN
    fundamentalFreq = freq;
    updateCoefficients();
}

// ✅ DEBE SER:
void Resonator::setFrequency(float freq) noexcept {
    jassert(std::isfinite(freq) && freq > 0.0f);
    
    // Clamp a rango seguro
    if (!std::isfinite(freq) || freq <= 0.0f) {
        freq = 20.0f; // Fallback seguro
        DBG("WARNING: Invalid frequency clamped to 20Hz");
    }
    
    freq = juce::jlimit(20.0f, 20000.0f, freq);
    fundamentalFreq = freq;
    updateCoefficients();
}
```

#### 2. Filter::setCutoff
```cpp
// Source/DSP/CoreModules/Filter.cpp
void Filter::setCutoff(float cutoff) noexcept {
    jassert(std::isfinite(cutoff) && cutoff > 0.0f);
    
    if (!std::isfinite(cutoff) || cutoff <= 0.0f) {
        cutoff = 100.0f;
        DBG("WARNING: Invalid cutoff clamped to 100Hz");
    }
    
    cutoff = juce::jlimit(20.0f, 20000.0f, cutoff);
    cutoffFreq = cutoff;
}
```

#### 3. LFO::setRate
```cpp
// Source/DSP/CoreModules/LFO.cpp
void LFO::setRate(float newRateHz) noexcept {
    jassert(std::isfinite(newRateHz) && newRateHz >= 0.0f);
    
    if (!std::isfinite(newRateHz) || newRateHz < 0.0f) {
        newRateHz = 1.0f;
        DBG("WARNING: Invalid LFO rate clamped to 1Hz");
    }
    
    newRateHz = juce::jlimit(0.01f, 100.0f, newRateHz);
    rateHz_.store(newRateHz, std::memory_order_relaxed);
    // ...
}
```

#### 4. AdditiveVoice::renderNextBlock
```cpp
// Source/DSP/Synthesis/AdditiveVoice.cpp
bool AdditiveVoice::renderNextBlock(...) {
    // Al final del procesamiento, validar output
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* data = buffer.getWritePointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i) {
            // Detectar y eliminar NaN/Inf
            if (!std::isfinite(data[i])) {
                jassertfalse; // Debug alert
                data[i] = 0.0f; // Silencio seguro
            }
        }
    }
    return isActive();
}
```

**Macro Helper**:
```cpp
// En archivo común (e.g., DSPUtils.h)
#define VALIDATE_AUDIO_PARAM(param, min, max, fallback, name) \
    do { \
        jassert(std::isfinite(param) && param >= min && param <= max); \
        if (!std::isfinite(param) || param < min || param > max) { \
            DBG("WARNING: Invalid " name " (" << param << ") clamped to " << fallback); \
            param = fallback; \
        } \
    } while(0)

// Uso:
VALIDATE_AUDIO_PARAM(freq, 20.0f, 20000.0f, 440.0f, "frequency");
```

**Archivos a Modificar**:
- `Source/DSP/CoreModules/Resonator.cpp`
- `Source/DSP/CoreModules/Filter.cpp`
- `Source/DSP/CoreModules/LFO.cpp`
- `Source/DSP/Synthesis/AdditiveVoice.cpp`
- `Source/DSP/Synthesis/NeurotikVoice.cpp`

**Prioridad**: 🔴 CRÍTICA - Implementar INMEDIATAMENTE

---

### 24. ⚠️ CRÍTICO: IDs de Plugin para Fork NEUROTiK

**Severidad**: CRÍTICA - CONFLICTO DE PLUGINS  
**Estado**: ⚠️ REQUIERE ACCIÓN EN FORK

**Problema**: 
DAWs (Logic, Ableton Live, FL Studio) usan `PLUGIN_MANUFACTURER_CODE` y `PLUGIN_CODE` como identificadores únicos globales. Si NEUROTiK usa los mismos códigos que AXIONiK:
- El DAW sobrescribirá un plugin con el otro al escanear
- Pérdida de presets
- Sesiones corruptas

**Estado Actual** (CMakeLists.txt línea 141-142):
```cmake
juce_add_plugin(NEURONiK
    PRODUCT_NAME "NEURONiK"
    PLUGIN_MANUFACTURER_CODE Nrnk  # ← ACTUAL
    PLUGIN_CODE Nrnk                # ← ACTUAL
    # ...
)
```

**ACCIÓN OBLIGATORIA para Fork NEUROTiK**:
```cmake
# En NEUROTiK/CMakeLists.txt
juce_add_plugin(NEUROTIK
    PRODUCT_NAME "NEUROTiK"
    PLUGIN_MANUFACTURER_CODE Nrtk  # ← CAMBIAR (4 chars, unique)
    PLUGIN_CODE Ntk1                # ← CAMBIAR (4 chars, unique)
    FORMATS VST3 AU Standalone
    IS_SYNTH TRUE
    # ...
)
```

**Reglas para Códigos**:
- **Manufacturer Code**: 4 caracteres, único por desarrollador
  - Registrar en [JUCE Forum Plugin Codes](https://forum.juce.com/t/plugin-manufacturer-codes/12345)
  - Ejemplos: `Nrtk`, `AbdN`, `AjAb`
- **Plugin Code**: 4 caracteres, único por plugin
  - Ejemplos: `Ntk1`, `Axnk`, `Nrnk`
- **Formato**: Solo letras y números, case-sensitive

**Checklist Pre-Fork**:
- [ ] Decidir códigos para NEUROTiK
- [ ] Verificar que no colisionan con otros plugins
- [ ] Actualizar CMakeLists.txt INMEDIATAMENTE después de `git clone`
- [ ] Documentar códigos en README

**Prioridad**: 🔴 CRÍTICA - Primera acción en fork

---

## 📊 RESUMEN ACTUALIZADO DE PROBLEMAS CRÍTICOS

| # | Problema | Severidad | Estado | Acción |
|---|----------|-----------|--------|--------|
| 1 | ScopedLock en audio thread | CRÍTICA | ✅ RESUELTO | Build #34 |
| 2 | reinterpret_cast peligroso | CRÍTICA | ⚠️ PENDIENTE | Implementar |
| 3 | Validación nullptr | ALTA | ⚠️ PENDIENTE | Implementar |
| 4 | Fallback ARM64 | ALTA | ⚠️ PENDIENTE | Implementar |
| 21 | std::function en audio | CRÍTICA | ✅ VERIFICADO | No encontrado |
| 22 | Static locals con mutex | CRÍTICA | ✅ VERIFICADO | No encontrado |
| 23 | Propagación NaN/Infinity | CRÍTICA | ❌ PENDIENTE | Implementar YA |
| 24 | Plugin IDs para fork | CRÍTICA | ⚠️ FORK | Cambiar en fork |

---

## ✅ CHECKLIST ACTUALIZADO PRE-RELEASE

### Bloqueadores Absolutos (NO RELEASE SIN ESTO)
- [ ] **#1**: ✅ Lock-free MIDI (COMPLETADO)
- [ ] **#2**: Eliminar reinterpret_cast
- [ ] **#3**: Validación nullptr
- [ ] **#23**: Validación NaN/Infinity en todos los parámetros críticos
- [ ] **#4**: Fallback ARM64

### Bloqueadores Pre-Fork NEUROTiK
- [ ] **#24**: Cambiar Plugin IDs en CMakeLists.txt
- [ ] Verificar que códigos no colisionan
- [ ] Documentar códigos en README

### Verificación Final
- [ ] Test de stress: 1 hora de reproducción continua sin NaN
- [ ] Test de modulación extrema: LFO @ max depth en todos los destinos
- [ ] Test de parámetros inválidos: Enviar valores fuera de rango
- [ ] Test de Apple Silicon: Compilar y ejecutar en ARM64
- [ ] Test de DAW: Cargar ambos plugins (AXIONiK + NEUROTiK) sin conflicto

---

## 🎯 PLAN DE ACCIÓN ACTUALIZADO

### Prioridad INMEDIATA (Hoy)
1. **Implementar validación NaN/Infinity** (2 horas)
   - Crear macro `VALIDATE_AUDIO_PARAM`
   - Añadir validaciones en Resonator, Filter, LFO
   - Añadir sanitización en output de voces

2. **Eliminar reinterpret_cast** (2-3 horas)
   - Implementar array de modulación en Processor
   - Refactorizar UI para usar índices

3. **Validación nullptr** (1 hora)
   - Crear macros `SAFE_PARAM_GET/SET`
   - Aplicar en todo el código UI

### Prioridad ALTA (Esta semana)
4. **Fallback ARM64** (2 horas)
   - Crear `PlatformDetect.h`
   - Implementar paths SSE/NEON/Scalar

### Prioridad CRÍTICA (Antes de Fork)
5. **Preparar Plugin IDs** (30 min)
   - Decidir códigos para NEUROTiK
   - Documentar en checklist de fork

---

## 🔍 SCRIPT DE VERIFICACIÓN

```bash
#!/bin/bash
# verify_critical_issues.sh

echo "=== Verificando Problemas Críticos ==="

# 1. Buscar std::function en DSP
echo "1. Buscando std::function en audio thread..."
grep -r "std::function" Source/DSP/ && echo "❌ ENCONTRADO" || echo "✅ OK"

# 2. Buscar static locals problemáticos
echo "2. Buscando static locals en processSample..."
grep -r "static.*=" Source/DSP/*.cpp | grep -v "static const.*=" && echo "⚠️ REVISAR" || echo "✅ OK"

# 3. Verificar validación de NaN
echo "3. Verificando validación isfinite..."
grep -r "isfinite" Source/DSP/ && echo "✅ IMPLEMENTADO" || echo "❌ FALTA"

# 4. Verificar Plugin IDs
echo "4. Verificando Plugin IDs..."
grep "PLUGIN_CODE" CMakeLists.txt

echo "=== Verificación Completa ==="
```

---

## 📝 NOTAS TÉCNICAS

### NaN Propagation Chain

**Cómo se Propaga un NaN**:
```
1. Modulación envía valor inválido (e.g., -100.0 Hz)
2. Resonator::setFrequency(-100.0) → updateCoefficients()
3. Coeficientes de filtro = NaN (división por cero o log negativo)
4. processSample() → output = NaN
5. Voice output = NaN
6. Engine output = NaN
7. Processor output = NaN
8. DAW master channel = NaN → MUTE TOTAL
```

**Detección**:
```cpp
// En Debug builds, añadir al final de processBlock:
#if JUCE_DEBUG
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* data = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            jassert(std::isfinite(data[i]));
        }
    }
#endif
```

### Plugin ID Collision Examples

**Caso Real**: 
- Plugin A: `PLUGIN_CODE "Abcd"`
- Plugin B: `PLUGIN_CODE "Abcd"` (mismo código)
- Resultado en Logic Pro X:
  - Solo uno aparece en la lista
  - El otro es ignorado silenciosamente
  - Sesiones con el plugin ignorado no cargan correctamente

**Solución**: Códigos únicos registrados

---

**Documento actualizado**: 2026-01-30 13:00:00  
**Próxima acción**: Implementar validación NaN/Infinity  
**Aprobación requerida**: Usuario
