# NEURONiK Handoff

## Estado de la instrumentación del piloto

El host `NEURONiK_WebPilotHost` mide el arranque real y lo escribe en un log:

```bash
cd ABDNeural
"./build-reference/NEURONiK_WebPilotHost_artefacts/Release/NEURONiK Web Pilot.exe" --auto-quit
```

- `--auto-quit` cierra el host en cuanto el panel está listo, para poder medir sin interacción.
- Salida: `pilot-startup.log` junto al ejecutable (se añade un bloque por ejecución) y stdout.
- Qué mide: construcción del backend WebView2, `document.readyState` interactivo, panel en el DOM,
  `window.__pilotReady` (effect de React), recursos servidos con sus bytes y rutas no resueltas.
- El marcador `window.__pilotReady` lo publica `WebPilot/app/page.jsx`.

Resultados (Release, 2026-09-16):

```text
options built       535 - 622 ms
panel in DOM       1232 - 1386 ms
react ready        1366 - 1518 ms
recursos            8 peticiones, 476 KB, 1 fallo (favicon.ico)
```

El perfil WebView2 borrado no cambia el resultado de forma apreciable. El coste dominante previo a
nuestro código es la construcción del backend (~0,55 s).

## Contexto

NEURONiK es un sintetizador propio basado en JUCE/C++. Tiene un motor híbrido de síntesis aditiva/resonante, una segunda ruta Neurotik, parámetros APVTS, presets, MIDI mapping y una interfaz JUCE hardware-inspired.

El objetivo futuro es conservar Standalone/VST3/AU y añadir una interfaz web reutilizable, primero en navegador/WebView2 y posteriormente con un posible backend WASM para el mismo núcleo DSP.

Fecha de este handoff: **2026-09-16**.

## Repositorio

```text
ABDNeural/
```

Repositorio original:

```text
https://github.com/ajabadia/ABDNeural
```

No se ha modificado todavía `ABDMS2000` ni `ABDSharedCode` como parte de esta iniciativa.

## Línea base compilada

La copia de referencia está en:

```text
ABDNeural/build-reference/
```

Artefactos principales:

```text
ABDNeural/build-reference/NEURONiK_artefacts/Release/Standalone/NEURONiK.exe
ABDNeural/build-reference/NEURONiK_artefacts/Release/VST3/NEURONiK.vst3
```

El Standalone ha sido comprobado manualmente por el usuario y se considera la referencia funcional actual.

El build también generó el ejecutable de prueba:

```text
ABDNeural/build-reference/Release/NEURONiK_DSPReferenceTest.exe
```

## Prueba DSP actual

Archivo:

```text
Tests/DSPReferenceTest.cpp
```

Target CMake:

```text
NEURONiK_DSPReferenceTest
```

Registro CTest:

```cmake
enable_testing()
add_test(NAME NEURONiK_DSPReferenceTest COMMAND NEURONiK_DSPReferenceTest)
```

La prueba prepara `NeuronikEngine` a 48 kHz, envía MIDI note-on, renderiza un bloque de 512 muestras y verifica una señal no nula y una voz activa.

Último resultado conocido:

```text
peak = 0.0901112
rms  = 0.0360038
1/1 tests passed
```

Comando de validación (o simplemente `build.bat`, que hace todo y termina con pausa):

```bash
cmake --build build-reference --config Release --target NEURONiK_DSPReferenceTest
ctest --test-dir build-reference -C Release --output-on-failure
```

`build.bat` encadena configuración de CMake, regeneración del contrato, Standalone + VST3, host
del piloto, exportación de la WebUI y la suite completa. Acepta un directorio de build como
argumento (por defecto `build-reference`).

## Arquitectura actual relevante

### Procesador JUCE

```text
Source/Main/NEURONiKProcessor.cpp
Source/Main/NEURONiKProcessor.h
```

El procesador contiene:

- `juce::AudioProcessorValueTreeState apvts`;
- selección entre `NeuronikEngine` y `NeurotikEngine`;
- sincronización APVTS → motor mediante `synchronizeEngineParameters()`;
- inyección de MIDI desde la UI;
- persistencia de estado y presets;
- visualización de envolventes, LFO y espectro.

### Parámetros

```text
Source/State/ParameterDefinitions.h
```

Este archivo es actualmente la definición central de IDs, rangos, defaults y elecciones de parámetros.

No se debe crear todavía una segunda lista independiente de parámetros para la web.

### Motor

```text
Source/DSP/ISynthesisEngine.h
Source/DSP/IVoice.h
Source/DSP/BaseEngine.h
Source/DSP/CoreModules/NeuronikEngine.*
Source/DSP/CoreModules/NeurotikEngine.*
Source/DSP/Synthesis/AdditiveVoice.*
Source/DSP/Synthesis/NeurotikVoice.*
```

Dependencias JUCE detectadas en el DSP:

- `juce::AudioBuffer<float>`;
- `juce::MidiBuffer`;
- `juce::MidiMessage`;
- `juce::LinearSmoothedValue`;
- `juce::Random`;
- `juce::AudioBuffer` en efectos;
- `juce::Reverb`;
- SIMD JUCE;
- utilidades `jlimit`, `jmin`, `MathConstants`.

## Decisión arquitectónica actual

Se ha elegido **extracción progresiva**, no desacoplamiento completo inmediato.

Primera frontera prevista:

```text
JUCE APVTS / MidiBuffer / AudioBuffer
              ↓
        adaptador JUCE
              ↓
     fachada DSP progresiva
              ↓
       futuro adaptador WASM
```

Durante la primera etapa se mantendrán temporalmente tipos JUCE donde eliminarlos implique riesgo. Se extraerán primero eventos, parámetros y la fachada de procesamiento; la sustitución completa de buffers y efectos queda para una fase posterior.

## Frontera DSP creada

Se ha añadido una primera fachada progresiva:

```text
Source/DSP/Runtime/DspEvent.h
Source/DSP/Runtime/DspEngineFacade.h
Source/DSP/Runtime/DspEngineFacade.cpp
```

La fachada expone eventos propios (`NoteOn`, `NoteOff`, pitch bend, presión y timbre) y adapta temporalmente a `juce::MidiMessage`/`juce::AudioBuffer` por dentro. Todavía no elimina dependencias JUCE; su objetivo es establecer el contrato que podrán usar el adaptador JUCE y el futuro adaptador WASM.

La prueba `NEURONiK_DSPReferenceTest` ya pasa a través de esta fachada.

## Próximo trabajo recomendado

1. Añadir `setParameter` y estructuras de parámetros a la fachada.
2. Validar note-off y eventos expresivos mediante pruebas.
3. Crear una conversión explícita de APVTS a un modelo de parámetros común.
4. Mantener un adaptador JUCE que produzca exactamente la misma salida.
5. Añadir pruebas de:
   - note-on/note-off;
   - cambio de `masterLevel`;
   - cambio de `morphX/morphY`;
   - presets;
   - selección de `engineType`.
6. Crear un piloto aislado de Next.js con un solo panel y estado simulado.
7. Probar exportación estática y carga en WebView2 antes de migrar más UI.
8. Solo si el piloto supera el punto de decisión, iniciar el wrapper WASM y la primera pantalla web conectada.

## Piloto Next.js creado

El experimento aislado está en:

```text
WebPilot/
```

Incluye un panel mínimo con estado simulado para:

```text
masterLevel
morphX
morphY
engineType
```

Configuración relevante:

```text
WebPilot/package.json
WebPilot/next.config.mjs
WebPilot/app/page.jsx
WebPilot/app/layout.jsx
WebPilot/app/globals.css
```

La configuración utiliza `output: 'export'` y `trailingSlash: true`, por lo que la salida estática se genera en:

```text
WebPilot/out/
```

El build validado ha sido:

```bash
cd ABDNeural/WebPilot
pnpm install --ignore-workspace
pnpm build
```

Resultado conocido: Next.js 16.3.5 genera la ruta estática `/` correctamente. `node_modules`, `.next` y `out` están excluidos mediante `WebPilot/.gitignore`.

El piloto todavía no conecta parámetros reales, WASM ni audio. Se ha añadido un host JUCE/WebView2 independiente:

```text
Source/WebPilotHost.cpp
```

Target CMake:

```text
NEURONiK_WebPilotHost
```

Ejecutable generado:

```text
build-reference/NEURONiK_WebPilotHost_artefacts/Release/NEURONiK Web Pilot.exe
```

El host sirve directamente `WebPilot/out/` mediante un `ResourceProvider`, habilita el backend WebView2 y no sustituye `NEURONiKEditor`.

## Verificación del piloto en WebView2 (2026-09-16)

Confirmado manualmente: la ventana `NEURONiK Web Pilot` carga el panel Next.js y los controles
responden (`Master Level`, `Morph X`, `Morph Y`, `Engine Type`, `STATIC EXPORT`, contador y JSON de estado).

Cadena validada sin hacks de plataforma:

```text
next build (output: 'export')
    → WebPilot/out/
    → ResourceProvider JUCE (backend WebView2)
    → https://juce.backend/
```

Detalle relevante: las rutas absolutas que emite Next (`/_next/static/chunks/*.js`) se resuelven
contra el origen `https://juce.backend/`, que es el que `WebBrowserComponent` intercepta, por lo que
no hace falta reescribir el `index.html` ni forzar `basePath`.

El proveedor de recursos se ha endurecido después de la verificación:

- la raíz del export se localiza por ruta de compilación (`NEURONiK_WEBPILOT_OUT_DIR`) y, si falla,
  buscando `WebPilot/out` hacia arriba desde el ejecutable y desde el directorio de trabajo;
- CMake falla en configure si falta `WebPilot/out/index.html`;
- si el documento no se encuentra, se sirve una página de diagnóstico legible en lugar de una ventana en blanco.

Nota operativa: el `.exe` del host queda bloqueado mientras la ventana está abierta, así que hay que
cerrarla antes de recompilar.

## Contrato de parámetros (generado)

El contrato que consume la WebUI se genera desde el propio APVTS:

```text
Source/State/ParameterDescriptors.h/.cpp         (descriptores)
Source/State/ParameterDescriptorExport.h/.cpp    (emisión determinista)
Tests/ParameterExportTool.cpp                    (CLI)
Tests/ParameterDescriptorTest.cpp                (regresión)
WebPilot/generated/parameters.generated.{json,js,d.ts}
WebPilot/lib/parameters.js                       (adaptador de la UI)
```

Puntos clave de diseño:

- Los descriptores se leen del layout real mediante un `AudioProcessor` mínimo de sondeo
  (`LayoutProbe`) y un APVTS temporal: los IDs, rangos, intervalos, `skew`, defaults y listas de
  opciones no pueden divergir del plugin.
- `ParameterLayout` no es iterable en JUCE 8 (sus parámetros son privados), de ahí el sondeo.
  El probe y el APVTS viven solo durante la llamada porque APVTS arranca un temporizador.
- Leer los descriptores requiere un `MessageManager` inicializado. En herramientas de consola se
  crea un `juce::ScopedJuceInitialiser_GUI`.
- La generación es determinista (sin fechas), por lo que el test puede comparar los ficheros
  versionados con una exportación nueva.

Comandos:

```bash
cd ABDNeural
cmake --build build-reference --config Release --target NEURONiK_ParameterExport
./build-reference/Release/NEURONiK_ParameterExport.exe WebPilot/generated
ctest --test-dir build-reference -C Release --output-on-failure
```

Resultado actual: 70 parámetros, 1 ID declarado pero no enrutado (`oscPitchCoarse`).

### `WebPilot/generated/` se versiona (decisión del 2026-09-16)

Los tres artefactos (~70 KB de texto: 36 KB JSON, 32 KB JS, 1,6 KB `.d.ts`) **sí entran en git**:

1. `NEURONiK_ParameterDescriptorTest` los compara contra una exportación nueva, así que tenerlos
   versionados convierte cualquier divergencia en un **diff revisable**. Si se ignoraran, en un
   clone limpio el test fallaría hasta ejecutar la exportación y la garantía anti-drift no valdría
   nada fuera de esta máquina.
2. Los importa `lib/parameters.js` y el build de Next, de modo que el repositorio queda
   autocontenido (salvo `node_modules`).
3. Son deterministas y pequeños: no generan ruido de diff.

Se regeneran en el paso 2/7 de `build.bat`. `WebPilot/.gitignore` lleva un comentario explícito
para que nadie añada una regla que los ignore. `git check-ignore` confirma que no están ignorados.

El caso "fichero ausente" del test falla con un mensaje que indica que hay que ejecutar
`NEURONiK_ParameterExport`, así que una pérdida accidental se detecta y se explica sola.

El adaptador `WebPilot/lib/parameters.js` añade lookup, recorte a rango, ajuste a intervalo,
conversión `0..1` con la misma matemática de `skew` que JUCE, formato de valor, estado por defecto
y validación del contrato. El panel del piloto ya no contiene rangos ni defaults escritos a mano.

## Conexión de parámetros divergentes: implementado (2026-09-16)

### Estado

```text
implemented 65 · uiOnly 4 · notRouted 1 · notInLayout 1
```

Conectado en esta pasada:

- `midiChannel` → filtrado de entrada (`Source/Main/MidiChannelFilter.h/.cpp`).
- `allNotesOff()` nuevo en `ISynthesisEngine` (una implementación en `BaseEngine`) y disparado
  con un flag atómico al cambiar de canal.
- `masterBPM` y `lfo1/2SyncMode`/`RhythmicDivision` → tempo sync real en ambos LFO.
- `fxDelaySync`/`fxDelayDivision` → tiempo del delay en duración de nota.
- `fxChorusRate`/`Depth` y `fxReverbSize`/`Damping`/`Width` → `setParameters()` completo.
- `velocityCurve` → `Source/Main/VelocityCurve.h/.cpp`, aplicado a los note-on entrantes.
- `midiThru` → `NEEDS_MIDI_OUTPUT TRUE` y limpieza del búfer de salida cuando está apagado.

Tests nuevos:

```text
NEURONiK_MidiChannelFilterTest   20 comprobaciones
NEURONiK_LfoSyncTest             19 comprobaciones
NEURONiK_VelocityCurveTest       36 comprobaciones
NEURONiK_PresetRoundTripTest     31 comprobaciones
```

Los tres registrados en CTest: la suite pasa 5/5 sin hardware.

`NEURONiK_DSPReferenceTest` cubre además el camino de pánico: tras `allNotesOff()` las voces
dejan de ser activas cuando termina el release (≈4,4 s con el release de 500 ms por defecto, ya que
la envolvente decae de forma multiplicativa hasta `1e-4`). Se expone como `DspEngineFacade::allNotesOff()`
para que la frontera web futura tenga el mismo pánico.

### Cambio de comportamiento a tener en cuenta

1. **Semántica del sync del LFO**: `getSyncedRateHz()` dividía mal (multiplicaba), así que "1/8"
   daba un ciclo cada dos negras. Ahora la división es longitud de nota. Nada llamaba a esa ruta,
   así que solo afecta a presets con `Tempo Sync` guardado, que antes no hacían nada.
2. **Aftertouch/pitch bend/CC74 ya no tratan el canal 1 como "todos"**. Es más correcto, pero si
   algún controlador dependía del comportamiento antiguo, se notará.
3. **`midiChannel` ahora silencia MIDI de otros canales**, incluido el del host. El teclado en
   pantalla sigue sonando siempre.
4. **Los efectos completos responden** (`chorus rate/depth`, `reverb size/damping/width`,
   `delay sync/division`). Con los defaults del APVTS el sonido no cambia, porque coinciden con
   los valores iniciales de los smoother del DSP; se pide de todos modos una validación de oído.

### Parámetros sin consumidor: decididos (2026-09-16)

| ID | Decisión | Por qué |
|---|---|---|
| `velocityCurve` | Conectado | Default `Linear` = identidad, así que ningún preset cambia de respuesta |
| `midiThru` | Conectado, opt-in | El control ahora manda: apagado (default) el plugin no emite MIDI al host |
| `unisonEnabled` | Control retirado de `ParameterPanel` | La puerta habría silenciado el unison de todos los presets (default off). El parámetro se conserva para que carguen |
| `harmMix` | **Retirado del layout** | Nadie lo leía y no había diseño de sonido detrás. Mantener un parámetro muerto solo compensa si alguien lo lee; retirarlo exige migrar los presets, y esa migración existe ahora |

Nuevo cambio de comportamiento a tener en cuenta, además de los anteriores:

5. **`midiThru` apagado significa apagado**: antes el búfer de entrada se devolvía siempre, con
   lo que el toggle no servía de nada. Quien contara con ese eco accidental (por ejemplo, para
   encadenar otro instrumento) debe activar el parámetro. La ficha del plugin pasa a declarar
   salida MIDI, que es lo que el runtime ya anunciaba con `producesMidi()`.
6. **La curva de velocidad se aplica al teclado en pantalla**: es intencionado (la curva es
   respuesta del instrumento, no de un puerto), y con `Linear` no cambia nada.
7. **`harmMix` ya no existe como parámetro** (el total baja de 71 a 70). Los presets guardados
   que lo llevan cargan igual: los hijos desconocidos se ignoran y además se eliminan del estado
   al cargar, de modo que un re-guardado no vuelve a escribir el id muerto.
8. **Un parámetro ausente de un preset vuelve a su default**, no conserva el valor que tuviera
   cargado. Es comportamiento del APVTS, y es el deseable para presets antiguos: no heredan
   valores de otro preset. Está fijado con una prueba.

### Migración de presets

`Source/Serialization/PresetMigration.{h,cpp}`: antes de `replaceState()` se retiran los hijos
`<PARAM>` cuyo `id` no esté entre los parámetros actuales, y también el `METADATA` del fichero
(metadato del preset, no estado del plugin; si se quedaba dentro, se duplicaba al reescribir).
La función es pura sobre un `ValueTree`, así que se prueba sin tocar ficheros del usuario.

`Tests/PresetRoundTripTest.cpp` cubre: round trip de valores reales (incluido un rango con `skew`
y dos `choice`), `unisonEnabled` (retirado de la UI pero vivo) que sigue cargando, preset legado
con `harmMix` más un id futuro inexistente, re-guardado sin ids muertos, etiquetas que sobreviven,
fichero inexistente, XML malformado y preset sin parámetros.

**Neutralidad sonora, hecha prueba (2026-09-16).** "Un preset antiguo suena igual" es una
afirmación por parámetro, así que en lugar de dejarla a oído se comprueba: se guardan dos ficheros
idénticos salvo por un hijo `<PARAM id="harmMix">`, se cargan ambos y se comparan los **70 valores**
del estado resultante como texto canónico ordenado. Deben ser exactamente iguales:

```text
canonicalParameterState(load(limpio)) == canonicalParameterState(load(con harmMix))
```

Es una garantía más fuerte que escuchar un preset, porque cubre los 70 parámetros y no una
impresión. La parte que sigue siendo auditiva es la comparación de la pareja de EXEs (ver
`Versiones compiladas\`); en esta máquina no hay presets guardados
(`Documents\NEURONiK\Presets` no existe), así que hay que crear uno con el build antiguo primero.

## Detalle histórico de la propuesta (referencia)

### A. `midiChannel` — filtrado de entrada

El canal se lee en `handleMidiEvent` solo para etiquetar la voz, nunca para descartar. Cambio
propuesto en `NEURONiKProcessor::processBlock()`, justo después de inyectar la FIFO de la UI y
antes de `renderNextBlock()`:

```cpp
// miembro reutilizado: juce::MidiBuffer channelFilteredMidi;
const int channelSetting = juce::roundToInt (apvts.getRawParameterValue (IDs::midiChannel)->load());
if (channelSetting > 0)
{
    channelFilteredMidi.clear();                       // conserva capacidad entre bloques
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        const bool isSystemMessage = message.getChannel() == 0;   // sysex, clock, ...
        if (isSystemMessage || message.getChannel() == channelSetting)
            channelFilteredMidi.addEvent (message, metadata.samplePosition);
    }
    midiMessages.swapWith (channelFilteredMidi);
}
```

Puntos a resolver en el mismo cambio:

1. **Notas colgadas al cambiar de canal**: añadir `virtual void allNotesOff() = 0` a
   `ISynthesisEngine` e implementarlo por motor (cada uno tiene sus voces). Llamarlo desde
   `parameterChanged` cuando cambie `midiChannel`. Como extra, habilita un botón de pánico.
2. **Inconsistencia existente**: el aftertouch y el CC74 usan
   `(v->getChannel() == channel || channel == 1)`, es decir, el canal 1 actúa como "todos". Con
   filtrado real eso sobra: una instancia en canal 3 no debería responder a aftertouch de canal 1.
3. **MIDI learn**: cuando se implemente, debe ignorar el filtro (o avisar), o no se podrá mapear
   un controlador que emita en otro canal.

Riesgo: bajo. Coste estimado: 1–2 h + pruebas.

### B. `masterBPM` y sync de LFO — 4 cambios pequeños

El LFO ya soporta tempo sync; solo falta el cableado.

1. `GlobalParams`: añadir `double bpm = 120.0;` (hoy no existe ningún campo de tempo).
2. `BaseEngine::updateParameters()`: aplicar la configuración completa del LFO, no solo
   waveform/rate/depth:

```cpp
const double bpm = currentGlobalParams.bpm;
auto applyLfo = [bpm] (Core::LFO& lfo, const GlobalParams::LFOParams& p)
{
    lfo.setWaveform (static_cast<Core::LFO::Waveform> (p.waveform));
    lfo.setRate (p.rateHz);
    lfo.setDepth (p.depth);
    lfo.setSyncMode (p.syncMode == 0 ? Core::LFO::SyncMode::Free
                                     : Core::LFO::SyncMode::TempoSync);
    lfo.setTempoBPM (bpm);
    lfo.setRhythmicDivision (quarterNotesForDivisionIndex (p.rhythmicDivision));
};
applyLfo (lfo1, currentGlobalParams.lfo1);
applyLfo (lfo2, currentGlobalParams.lfo2);
```

3. Tabla de divisiones, en un único sitio, reutilizable después por el delay:

```cpp
// mismo orden que {"1/1","1/2","1/4","1/8","1/16","1/32","1/4t","1/8t","1/16t"}
static constexpr float kDivisionInQuarterNotes[] = {
    4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 2.0f / 3.0f, 1.0f / 3.0f, 1.0f / 6.0f
};
```

4. `synchronizeEngineParameters()`: rellenar `bpm`, `lfo1.syncMode`, `lfo1.rhythmicDivision` y
   los equivalentes de `lfo2`. Los dos bloques del motor duplican ~20 líneas: conviene extraer un
   `applyGlobalParams()` antes de añadir una tercera copia.

Detalles importantes:

- **Rango a alinear**: el parámetro admite 20–400 BPM pero `LFO::setTempoBPM` valida 10–300. Si no
  se toca, a 350 BPM el LFO usará 300 en silencio. Recomendado: ampliar el LFO a 400.
- **Seguridad de presets**: el default de `lfo1/2SyncMode` es `Free` (índice 0), así que conectar
  el sync **no cambia el sonido** de ningún preset existente salvo que ya tuviera `Tempo Sync`
  guardado, que hasta ahora no hacía nada.
- **Efecto colateral deseado**: `fxDelaySync`/`fxDelayDivision` pueden conectarse después con la
  misma tabla; hoy el delay siempre interpreta su tiempo en segundos.

Prueba propuesta (sin hardware): en `Tests/DSPReferenceTest.cpp`, con `syncMode = TempoSync`,
`division = 1/4` y `bpm = 120`, el periodo del LFO debe ser ≈0.5 s; y una prueba de regresión en
modo `Free` que confirme que `rateHz` sigue mandando.

Riesgo: bajo (plomería aditiva). Coste estimado: 2–4 h con pruebas. Validación final auditiva.

## Documentación de parámetros

El inventario actual de parámetros DSP está en:

```text
DSP_PARAMETERS.md
```

Incluye rangos, defaults, conversiones de tiempo, parámetros por motor y divergencias conocidas entre la definición APVTS y la sincronización actual. Debe actualizarse antes de convertir ese contrato en TypeScript.

## Criterio para no desperdiciar trabajo web

El primer objetivo web no es migrar ABDMS2000 ni toda la interfaz de NEURONiK. Es validar este circuito mínimo:

```text
Next.js export estático
        ↓
panel pequeño
        ↓
modelo de parámetros simulado
        ↓
WebView2
```

La decisión de continuar con Next.js se tomará después de comprobar empaquetado, recursos, ciclo de vida y complejidad. Si falla, la alternativa será React/Vite y la arquitectura de parámetros deberá seguir siendo reutilizable.

## Procedimiento de build

El proyecto usa CMake y busca JUCE en `C:/JUCE` o mediante `JUCE_PATH`.

### `build.bat` (recomendado)

Un solo comando hace el ciclo completo y **termina siempre con pausa**, tanto si sale bien como si
falla, para poder copiar la salida:

```bat
build.bat                    :: plugin + contrato + WebUI + tests (usa build-reference)
build.bat build              :: lo mismo, en un directorio de build limpio
build.bat modelmaker         :: además compila la herramienta ModelMaker
build.bat build modelmaker   :: build limpio incluyendo ModelMaker
```

Pasos que ejecuta, en orden:

```text
1/7  cmake -S . -B <dir> -DCMAKE_BUILD_TYPE=Release
2/7  NEURONiK_ParameterExport + regeneracion de WebPilot\generated
3/7  NEURONiK_Standalone + NEURONiK_VST3
4/7  NEURONiK_WebPilotHost            (si falla, solo avisa)
5/7  WebPilot: pnpm build            (se omite si no hay node_modules)
6/7  NEURONiK_ModelMaker             (solo con 'modelmaker', ver abajo)
7/7  compilacion de los 6 tests + ctest --output-on-failure
```

Artefactos:

```text
<dir>\NEURONiK_artefacts\Release\Standalone\NEURONiK.exe
<dir>\NEURONiK_artefacts\Release\VST3\NEURONiK.vst3
<dir>\NEURONiK_WebPilotHost_artefacts\Release\NEURONiK Web Pilot.exe
<dir>\Release\NEURONiK_ModelMaker.exe                  (solo con 'modelmaker')
```

El paso lento es el plugin: varios minutos la primera vez (compila JUCE y las dos variantes,
Standalone y VST3) y
bastante menos después, porque solo recompila lo que cambie. Si solo hay que tocar pruebas, es más
rápido pedir el target concreto:

```bash
cmake --build build-reference --config Release --target NEURONiK_PresetRoundTripTest
ctest --test-dir build-reference -C Release --output-on-failure
```

### ModelMaker: fuera del build por defecto (decisión)

`NEURONiK_ModelMaker` **no** se compila salvo que se pida con `modelmaker`. Motivos:

1. **Modifica un fichero versionado.** El target arrastra `add_dependencies(... UpdateVersion)`, que
   ejecuta `Scripts/update_version.ps1` e incrementa `NEURONIK_MODELMAKER_VERSION_SUB` en
   `Source/ModelMaker/Version.h` (fichero trackeado por git, ahora en 0.1.28). Si estuviera en el
   camino por defecto, cada build diario ensuciaría el árbol y forzaría recompilaciones.
2. **Es otro entregable.** Aplicación WIN32 independiente; no forma parte del plugin ni del
   contrato, así que mezclarla con el build del sintetizador hace más lento el ciclo normal.
3. **Históricamente se compilaba en otro directorio** (`build_neuronik`, ver más abajo), así que
   tampoco es que la caché de `build-reference` lo espere.

Pero tampoco conviene olvidarla: comparte con el plugin `Oscillator.cpp`, `Resonator.cpp` y
`NEURONiK_Common`, de modo que un cambio en el núcleo DSP puede romperla sin que nadie se entere.
De ahí que exista el modo opt-in: úsalo tras tocar esos ficheros.

### Builds guardadas

Las copias de entregables se guardan en:

```text
Versiones compiladas\
```

Convención de nombre: `<Producto>_<AAAA-MM-DD>_<HHMM>_<motivo>.exe`.

### Pareja de referencia (2026-09-16): antes y después de retirar `harmMix`

```text
NEURONiK_2026-09-16_1444_pre-harmMix-retiro.exe     5.376.512 bytes
md5 e6d6c239f9bc8d16de1bc1cfc79d53a2     Standalone 2026-09-16 14:44

NEURONiK_2026-09-16_1517_post-harmMix-retiro.exe    5.383.168 bytes
md5 f15de044138afd3d1a61d5e8256cbd05     Standalone 2026-09-16 15:17
```

La segunda es el build validado con `build.bat`:

```text
100% tests passed out of 6  (DSPReference, MidiChannelFilter, VelocityCurve,
                             LfoSync, ParameterDescriptor, PresetRoundTrip)
contrato: 70 parametros · 65 wired · 4 UI only · 1 not routed · 1 not in the layout
```

Sirven como A/B de oído: entre las dos solo cambian la retirada de `harmMix`, la curva de
velocidad, el MIDI thru opt-in y las sincronizaciones ya documentadas. Con los defaults, la
diferencia audible esperada en presets existentes es **ninguna**; en el A/B, el default de
`velocityCurve` es `Linear` (identidad) y el de `midiThru` es off (antes el búfer se devolvía
siempre, lo que solo se nota si el host leía el MIDI del plugin).

Detalles:

- Los `.exe` están cubiertos por `.gitignore`, así que estas copias no entran en el repositorio.
- Convención sugerida: guardar una copia **antes** de un cambio que pueda alterar el sonido y otra
  después, con el motivo en el nombre.
- `Versiones compiladas\NEURONiK_ModelMaker\` contiene un **acceso directo** (no el binario). Se
  rehizo el 2026-09-16: apunta a `build-reference\Release` (donde `build.bat modelmaker` deja
  `NEURONiK_ModelMaker.exe`) y su descripción recuerda cómo generarlo. El anterior apuntaba a
  `D:\desarrollos\ABDNeural\build_neuronik\Release`, ruta de cuando el proyecto vivía en otra
  carpeta.

### `build.ps1` (eliminado el 2026-09-16)

Estaba desfasado: construía **todos** los targets en `build/` (por tanto incluía ModelMaker y
tocaba `Version.h`), no ejecutaba pruebas y no pausaba. Se comprobó que ningún script lo
referenciaba y se eliminó; `build.bat` lo sustituye por completo. La eliminación queda como cambio
pendiente de commitear (el fichero estaba trackeado).

### Nota sobre `UpdateVersion`

El sistema de build tiene un target `UpdateVersion` que modifica automáticamente
`Source/ModelMaker/Version.h`. Revisar `git status` después de compilar y descartar cualquier
incremento accidental que no forme parte de la tarea.

## Reglas de comparación

Antes de cambiar el DSP:

- ejecutar `NEURONiK_DSPReferenceTest`;
- conservar `build-reference`;
- compilar el nuevo Standalone en otra carpeta si es necesario;
- comparar primero compilación y RMS/peak;
- comprobar manualmente el mismo preset en el EXE de referencia y en el nuevo.

No borrar ni sobrescribir `build-reference` hasta generar una nueva referencia validada.

## Decisiones pendientes

- Si la interfaz web piloto se hará inicialmente con React/Vite o Next.js estático.
- Qué panel será el primero en migrar.
- Si el primer WASM incluirá ambos motores (`Neuronik` y `Neurotik`) o solo uno.
- Qué formato de preset común se utilizará entre APVTS, web y WASM.
- Qué componentes de `ABDSharedCode` se extraerán sin acoplarlos a Next.js.

## No hacer todavía

- No reescribir todo `NEURONiKProcessor`.
- No eliminar APVTS.
- No duplicar los IDs de parámetros en JavaScript.
- No migrar toda la interfaz JUCE antes de validar un panel piloto.
- No añadir Next.js al proyecto principal sin probar antes exportación estática y WebView2.
- No comparar solo por oído: mantener pruebas automatizadas de audio.
