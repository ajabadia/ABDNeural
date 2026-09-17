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

Arranque de la versión web (`start.bat`, mismo patrón que ABDMS2000, menú 1-3):

1. **Piloto en WebView2** — lanza "NEURONiK Web Pilot.exe" (bridge bidireccional con el
   plugin; es la web "de verdad", como el Vite 8384 en ABDMS2000).
2. **Solo WebUI en navegador** — sirve `WebPilot/out` en `http://localhost:8399` con
   `npx serve`; sin JUCE la página queda en LOCAL MODE (útil para depurar la página a solas).
3. **Selftest del bridge** — ejecuta el host con `--selftest` (E2E automático NATIVO->JS y
   JS->NATIVO, imprime resultado y cierra solo; exit 0 = OK).

Antes de lanzar la opción 1 o 3 debe existir el host (compilar con `build.bat`); el propio
`start.bat` comprueba artefactos y avisa si faltan. Termina con pausa para poder leer la
salida (regla de trabajo de este proyecto).

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

1. Lanzar `build.bat`: compila el host con el bridge y añade `NEURONiK_ParameterBridgeTest` a la
   suite (8 tests). Verificación interactiva que solo puede hacerse con la ventana delante:
   mover un slider de la tira nativa debe mover el control web y viceversa.
2. Añadir `setParameter` y estructuras de parámetros a la fachada.
3. Validar note-off y eventos expresivos mediante pruebas.
4. Crear una conversión explícita de APVTS a un modelo de parámetros común.
5. Mantener un adaptador JUCE que produzca exactamente la misma salida.
6. Añadir pruebas de:
   - note-on/note-off;
   - cambio de `masterLevel`;
   - cambio de `morphX/morphY`;
   - presets;
   - selección de `engineType`.
7. Crear un piloto aislado de Next.js con un solo panel y estado simulado.
8. Probar exportación estática y carga en WebView2 antes de migrar más UI.
9. Solo si el piloto supera el punto de decisión, iniciar el wrapper WASM y la primera pantalla web conectada.

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

## Bridge de parámetros JUCE <-> WebUI (2026-09-16)

El host del piloto ya no es solo un visor: refleja un APVTS real en ambos sentidos por el canal de
eventos de JUCE 8 y lleva una tira nativa de comparación.

```text
Source/WebUI/ParameterBridge.{h,cpp}   protocolo bidireccional (sin WebView2: testeable con una lambda)
Source/WebPilotHost.cpp                transporte + APVTS real (createLayoutApvts) + tira nativa
WebPilot/lib/bridge.js                 transporte JS (window.__JUCE__.backend; modo local sin JUCE)
WebPilot/app/page.jsx                  panel conectado, con fases de gesto y estado normalizado
Tests/ParameterBridgeTest.cpp          protocolo completo contra el APVTS del contrato
Tests/webviewBridgeDirectionTest.mjs   guard de dirección (compartido con ABDSharedCode)
```

Puntos clave de diseño (el detalle completo está en `WEB_PILOT.md`, sección del bridge):

- El APVTS reflejado es `State::createLayoutApvts()`: el layout exacto del plugin (70 parámetros).
- **Salida por sondeo** (`publishPendingChanges`, timer de 30 ms) en lugar de listeners de
  parámetros: elimina por construcción el modo de fallo "listener duplicado / evento perdido" y
  permite diferir por valor (un parámetro puesto al valor que ya tenía no viaja).
- **Sin eco**: lo que llega de JS actualiza `lastReported`, y el sondeo no se lo devuelve.
- **Gestos**: begin/change/end desde JS; si la página se recarga en mitad de un arrastre, el host
  cierra el gesto abierto (listener `pageLoaded` + `closeOpenGestures`).
- **Tolerancia**: mensajes malformados e IDs desconocidos se cuentan en `Stats`, nunca lanzan.
- `value` en el cable SIEMPRE normalizado 0..1; `real`/`text` viajan solo nativo -> JS.
- El host sirve `juce.js` sin interferir (integración nativa activada: sin ella no hay
  `window.__JUCE__` y ambas direcciones mueren).
- La tira nativa del host usa `SliderAttachment` reales sobre los mismos IDs: mover un lado debe
  mover el otro. Verificación interactiva pendiente del próximo `build.bat`.

Validado sin compilar el plugin: `next build` en verde, el smoke test del transporte JS (ids y
payloads en ambas direcciones, dispose sin fugas) y el guard de dirección sobre los 98 ficheros de
`Source/`. El test `NEURONiK_ParameterBridgeTest` compila y corre con `build.bat`.

### El protocolo, como contrato versionado (2026-09-16)

El formato del cable está fijado en `WebPilot/contracts/bridge-protocol.json` (versionado en git,
versión 1) con especificación completa en `WebPilot/BRIDGE_PROTOCOL.md`. Misma mecánica que el
contrato de parámetros: el fichero no se genera, se edita, y dos tests anti-drift lo comparan con
lo que cada lado ejecuta:

```text
NEURONiK_BridgeProtocolContractTest   C++: JSON vs literales compilados de ParameterBridge.h
NEURONiK_BridgeProtocolJs (node)      JS: JSON vs bridge.js + formas de mensaje reales (backend simulado)
```

Fijado por ellos: los tres event ids (`event`, `nativeEvent`, `pageLoaded`), las acciones
(`syncAllParams`, `parameterChanged`, `requestState`), las fases de gesto, la convención de
escala (`value` normalizado, `real`/`text` solo nativo->JS) y los comportamientos (sin eco,
salida por sondeo, gestos siempre cerrados, entrada tolerante, modo local). La política de
versiones está en `BRIDGE_PROTOCOL.md`: cambio aditivo opcional no la incrementa; renombrar un
literal o cambiar la escala, sí.

Ambos tests están en verde, y con ellos la suite sube a **10 tests** en el próximo `build.bat`
(8 C++ + 2 node).

**Validado por el usuario (build.bat completo, 2026-09-16 18:11): 10/10 tests, plugin recompilado
byte-idéntico (md5 f15de044…) y host del piloto con el bridge sirviendo la página (8 recursos,
478 KB, panel en DOM a 1645 ms, ready a 2384 ms; el 2,4 s vuelve a ser perfil WebView2 frío tras
recompilar — en caliente la serie documentada está en 886-1166 ms).**

### Verificación bidireccional automatizada: `--selftest` (2026-09-16 18:26)

El host del piloto incluye un modo que ejecuta el doble E2E del bridge sobre el canal real de
WebView2 (mismo recorrido que una prueba manual con el ratón):

```text
"NEURONiK Web Pilot.exe" --selftest
  [selftest] NATIVE -> JS: native masterLevel = 0.25, page slider = 0.25 -> OK
  [selftest] JS -> NATIVE: page slider set to 0.75, native masterLevel = 0.7500 -> OK
  [selftest] RESULT: OK        (exit code 0)
```

- NATIVO -> JS: `setValueNotifyingHost(0.25)` en el APVTS y, 400 ms después, lectura del `value`
  del primer slider de la página (0.25 exacto).
- JS -> NATIVO: dispatch de un evento `input` real sobre el slider (lo que dispara un arrastre de
  usuario) con valor 0.75 y, tras el ciclo React -> bridge -> poller, lectura del parámetro nativo
  (0.7500 exacto).
- `--selftest` implica `--auto-quit`; el exit code (0/1) es el veredicto. En este modo el selftest
  es quien cierra la ventana, no el sondeo de arranque.
- **`build.bat` lo ejecuta como paso 8/8** (tras la suite de tests), solo si el host compiló y
  existe `WebPilot\out`. Un fallo del selftest marca el build como CON ERRORES. Se omite con
  `build.bat noselftest`. La ventana del piloto parpadea unos 3 segundos: es el selftest.
- **Verificación manual del usuario (misma sesión): confirmada.** Captura del host con la página
  en `BRIDGE LIVE` y la tira nativa mostrando exactamente los mismos valores (0.67/0.564/0.399 en
  ambos lados, 261 actualizaciones de parámetro). El punto de ROADMAP queda cerrado.

## Reglas de trabajo vigentes (desde la migración a Next.js, 2026-09-16)

1. **Sin monolitos**: ningún fichero nuevo por encima de ~300 líneas; si crece, se divide.
2. **DRY**: la matemática/interacción compartida vive en un módulo común (p. ej.
   `drag-core.js`), nunca duplicada entre controles o paneles.
3. **Tests al cerrar cada paso relevante**: JS con vitest, C++ con ctest; un paso sin su
   test no cuenta como terminado.
4. **Sin NTFS junctions en nada nuevo**: la reutilización va por paquetes npm/pnpm
   (workspace `@abdsynths/*` o `file:`); los junctions existentes son legacy.

## Familia de controles compartidos en ABDSharedAssets (2026-09-16)

NEURONiK arranca la migración de UI como **primer consumidor** de la familia de controles
compartidos del paquete `@abdsynths/shared` (`ABDSharedAssets/`). Documentación completa:
`ABDSharedAssets/COMPONENTS.md`. Resumen operativo:

- **Familia**: `Knob`, `Slider`, `Toggle` (+ `Wheel` preexistente) en
  `ABDSharedAssets/components/`, contrato común (constructor + `setValue/getValue/destroy`,
  `onChange` solo en ediciones de usuario), interacción DRY vía `drag-core.js`.
- **Skins**: el ASPECTO es intercambiable por synthe. Una skin es un **mapa de renderers por
  tipo** (`{knob, slider, toggle}`); `applySkin()` despacha por `CONTROL_KIND` y cae al renderer
  'vector' cuando falta un tipo. Incluidas: `vector` (SVG/CSS sin assets), `ms2000`
  (extraída de ABDMS2000), `junio` (sprites PNG extraídos de ABDJUNiO601, en
  `ABDSharedAssets/assets/junio/`). `registerSkin()` para skins de proyecto.
- **Demo única**: `ABDSharedAssets/demo/demo.html` **sección 8** (familia completa, 3 skins,
  colores del toggle junio, toggle momentary). `demo/proto/` fue **deprecada y eliminada**:
  su única aportación (cascada de tema de 3 niveles) está documentada en
  `docs/INTEGRATION_GUIDE.md` §5 bis y no mostraba ningún componente que la demo principal
  no tuviera. `npm run demo` sirve la raíz del paquete → abrir `/demo/demo.html`.
- **Tests**: `pnpm test` en ABDSharedAssets (vitest+jsdom) — 24/24 en verde: contrato,
  clamping, onChange/setValue, gestos, despacho de skins, fallback, destroy sin fugas.
- **Consumo sin junctions**: paquete pnpm (`workspace:*`) o `file:`; los controles no saben
  nada de JUCE/bridge/React — el envoltorio React es quien conecta con el contrato.

### Wrappers React en el WebPilot (2026-09-16)

Primera integración real de la familia en una pantalla Next.js del plugin:

- `WebPilot/lib/controls.jsx` — `useSharedControl(Clase, props)`: monta el control
  imperativo una vez por instancia (efecto ligado a la clase, no a las props), React ->
  `control.setValue()` (programático, **sin eco** de onChange), callbacks de usuario vía
  refs estables (`handlersRef`), `destroy()` en el cleanup. Wrappers: `ParamKnob`,
  `ParamSlider`, `ParamToggle` (bool como 0/1 en el cable) y `ParamChoice` (select nativo,
  índice N viaja como N/(count-1), el encoding discreto del APVTS).
- `WebPilot/lib/paramValue.js` — plomería de valores sobre el contrato: `realFromNormalized`
  / `normalizedFromReal` (misma matemática NormalisableRange que el host, con snap de
  intervalo), `displayText` (percent en rangos 0..1 sin unidad, unidades reales en el resto),
  `describeParam` (view-model del contrato; null para IDs desconocidos → error visible).
- `WebPilot/lib/useParameterControls.js` — glue de página: estado normalizado + snapshot
  inicial + push al bridge con fases de gesto (`begin`/`change`/`end`), modo local si no hay
  `window.__JUCE__`.
- `app/page.jsx` — reescrito sobre el hook y los wrappers. **`masterLevel` conserva
  `input[type=range]` nativo a propósito**: el `--selftest` del host lo conduce con
  `querySelector('input[type=range]')`; el resto (morphX, morphY, engineType) usa la familia
  compartida. Bug corregido de pasada: la página anterior convertía real→normalizado con
  `fromNormalized` (la inversa); fallaría con skew≠1 o rangos no 0..1.
- Consumo: `"@abdsynths/shared": "file:../../ABDSharedAssets"` en WebPilot/package.json,
  instalado con `pnpm install --ignore-workspace` (el workspace raíz capturaría el install).
  El paquete exportó además `"./components"` (barrel) y `tokens.css` dejó de `@import`ar
  Google Fonts (rompía `output: 'export'` sin red).
- Validado: **15/15 tests nuevos** (`pnpm test` en WebPilot: unitarios de paramValue + guard
  de contrato de página) y `pnpm build` en verde con el CSS de la familia embebido. E2E
  re-verificado con `--selftest` tras la corrección del drag (ver abajo).

### Corrección de interacción: drag 1:1 en la familia compartida (2026-09-16 21:45)

**Síntoma informado por el usuario**: los sliders morphX/morphY "iban a toda velocidad" al
arrastrar (masterLevel, el `input` nativo, iba bien). **Causa raíz** en `ABDSharedAssets/
components/drag-core.js` (afecta a TODA la familia: knob, slider y wheel): el delta se medía
**desde el origen del arrastre** pero los controles lo aplican como **incremento** sobre el
valor ya actualizado — el valor se componía y la velocidad crecía cuadráticamente con el
número de eventos de movimiento. Silencioso en tests porque los drags sintéticos iban en un
solo `pointermove`.

**Fix**: el delta es ahora **relativo al movimiento anterior** (`lastX/lastY` actualizados en
cada `pointermove`); el valor sigue al puntero 1:1 (N eventos de d px = N*d px de recorrido).
Test de regresión nuevo en `tests/controls.test.js` (drag en 4 movimientos → valor exacto);
suite en **25/25**. Requiere re-servir/re-exportar cualquier WebUI que embebiera `drag-core`.


## Paso 1 ejecutado: ParameterPanel real + assets embebidos (2026-09-16, por validar)

Escrito SIN compilar (acuerdo de turno: compila el usuario con `build.bat`). Estado en disco:

- **Host con el plugin de verdad.** `PilotComponent` instancia `NEURONiKProcessor` (miembro
  propio, declarado DESPUÉS de `browser` y ANTES de `bridge` — el orden de destrucción importa:
  el panel y el bridge mueren antes que el procesador). Fuera `State::createLayoutApvts()`: el
  bridge puentea `processor.getAPVTS()`. El selftest consulta `processor.getAPVTS()` igual que
  antes consultaba el APVTS de juguete — protocolo y página no cambian.
- **Panel real en vez de tira.** `NativeStrip` eliminado; abajo del navegador vive ahora el
  `NEURONiK::UI::ParameterPanel` real (pestaña GENERAL: envolvente, unison, freeze, RANDOM,
  selector de motor, MidiLearners). Altura 240 px (la tira eran 120). El `timerCallback` ya no
  refresca etiquetas (las pintaba la tira); el panel se repinta con sus propios timers.
- **CMake del host.** Compila las mismas fuentes que el plugin (`${NEURONIK_SOURCES}`, sin el
  `.rc` para no duplicar VERSIONINFO), patrón igual que el Standalone. Enlaza además
  `juce_audio_basics`, `juce_data_structures`, `juce_dsp` y `NEURONiK_Common`. Include dirs de
  Main/UI/Panels/Browser/DSP/State/Serialization añadidos.
- **Assets WebUI embebidos (apuntado por el usuario y hecho).** `juce_add_binary_data
  (NEURONiK_WebPilotAssets)` con el GLOB de `WebPilot/out/**` (CONFIGURE_DEPENDS, excluyendo
  `_not-found` para no duplicar identificadores). `loadPilotResource` sirve DISCO primero y
  cae a `BinaryData` si el exe está solo (`loadEmbeddedResource`, match por sufijo/basename);
  define `NEURONIK_HAS_PILOT_ASSETS`. El informe del log imprime `[embedded fallback: N]`.
  Esta es la vía de servicio del VST3 final.

**Correcciones tras la primera compilación (2026-09-16):**

- `C2065 JucePlugin_Name` (NEURONiKProcessor.cpp:418): el target gui-app no genera macros de
  plugin → `JucePlugin_Name="NEURONiK"` clavado en `target_compile_definitions` del host
  (mismo valor que el vcxproj del plugin).
- `C3861 loadEmbeddedResource`: estaba definida DESPUÉS de `loadPilotResource` → declaración
  adelantada dentro de la clase (el orden de definición ya no importa).
- `C2039 getNumResources` / `C2664 getNamedResourceOriginalFilename(i)`: esas APIs NO existen
  en el `BinaryData` generado. La real: `BinaryData::namedResourceListSize` +
  `namedResourceList[]` (nombres mangled), y el payload se pide por NOMBRE de recurso
  (`getNamedResource(resourceName, size)`), nunca por ruta; el nombre original se saca con
  `getNamedResourceOriginalFilename(resourceName)` para el match por sufijo/basename.
- `C2039 fromLastCharacterOfDelimiter`: no existe en `juce::String` → `fromLastOccurrenceOf
  ("/", false, true)`.

**Bug preexistente destapado por el piloto (corregido 2026-09-16):** TODA la telemetría de UI
se sincronizaba SOLO dentro de `processBlock` (`uiAttack…uiFRelease`, `uiMorphX/Y`). En el
plugin da igual (siempre hay audio), pero el host del piloto NO tiene callback de audio →
XYPad y EnvelopeVisualizer nativos se quedaban congelados. Fix: extraído a
`NEURONiKProcessor::refreshUiTelemetryFromApvts()` (público, thread-safe: atomics + loads del
APVTS); `processBlock` lo sigue llamando cada bloque y el host lo consulta en su timer.
La telemetría derivada del MOTOR (espectral, LFO, envolventes de salida) sigue donde estaba:
solo existe tras render.

Observaciones auditadas SIN cambiar (por si parecen bugs y no lo son):
- `morphX/morphY` son 0..1 con default 0 — "esquina modelo A", no centro. El DSP hace
  `lerp(modelA, modelB, morphX)` con `jlimit(0,1)`: legal por diseño. El fallback local 0.5
  del XYPad es inofensivo (lo sobrescribe el source al instante). El centro real del pad
  depende de qué modelos cargue el preset.
- El XYPad nativo invierte Y de forma consistente (pintado, ratón y thumb con el mismo
  `1 - y`): correcto.

**3ª compilación (2026-09-16 23:03) — HOST ENLAZA Y PASA:** juce_audio_utils resolvió los
LNK2019, selftest OK con host recompilado, 10/10. NOTA: los fixes de telemetría (23:05) son
posteriores a los .obj (22:57) → necesitan UNA pasada más (rápida: 2 ficheros). El lado WebUI
(hook normalizado) SÍ quedó dentro (paso 5 regeneró out/).

**4ª compilación (2026-09-16 23:43, build.bat completo): 10/10 tests + selftest OK, telemetría
dentro (exe 23:43 > fixes 23:05) y el exit code del selftest YA llega al proceso — verificado
en ambos caminos: `--selftest` → exit 0; `--selftest-force-fail` (drill temporal) →
`RESULT: FAIL`, `reason=selftest-forced-fail`, exit 1.** La pasada aun así imprimió
RESULTADO: OK pese a un fallo real en el paso 5 (incidencia y endurecimiento abajo).

**Incidencia del paso 5 (RESUELTA): snapshot viejo de `@abdsynths/shared` en node_modules.**
El barrel instalado ya exportaba XYPad pero le faltaba `components/xypad.js` (copia a medias
de la sesión cortada) → `next build` murió con `Module not found: Can't resolve './xypad.js'`.
El XYPad SÍ está completo en ABDSharedAssets: `xypad.js` (23:11), 12 tests propios, suite
**37/37** en verde, exportado en el barrel y usado en la demo (sección 8). Resolución:
`cd WebPilot && pnpm install --ignore-workspace` refresca los deps `file:` desde el paquete
fuente. **Lección:** tocar `ABDSharedAssets` exige re-install en cada consumidor `file:`, no
basta re-exportar; el snapshot de node_modules puede quedarse a medias si la sesión muere.

**Endurecido build.bat (el paso 5 ya no es blando):** un fallo de `pnpm build` marca
`WEBUI_BUILD_FAILED`, `EXIT_CODE=1` y el paso 8 OMITE el selftest. Antes: aviso y el selftest
corría contra `WebPilot\out` ANTERIOR — en esta pasada celebró un OK con la WebUI vieja.

**Pendiente del checklist del paso 1:** (a) visual: RANDOM nativo mueve morphX/Y en página y
slider de página mueve VOLUME nativo; (c) XYPad nativo sigue a la página. (b) fallback
embebido **VALIDADO (2026-09-17, ver sección propia más abajo)**. Drill
`--selftest-force-fail`: RETIRADO (2026-09-17) tras validar; el flag ya no fuerza nada y se
degraja a un `--selftest` normal.

**Fix de revisión (2026-09-16, por compilar): el exit code del selftest nunca salía del exe.**
`selftestPassed` moría en el `PilotComponent`: ni `finish()` ni `systemRequestedQuit()` lo
publicaban, así que el proceso salía SIEMPRE con 0 y la puerta del paso 8/8 de `build.bat`
(`if !ERRORLEVEL! neq 0`) era decorativa — un selftest FAIL se habría celebrado como
`[OK] Bridge verificado`. Arreglado en `Source/WebPilotHost.cpp`: `g_selftestExitCode`
(atómico, -1 = sin veredicto) lo escribe `selftestFinish()` y `PilotApplication::
systemRequestedQuit()` lo aplica con `setApplicationReturnValue(code)`. Sin selftest
(`--auto-quit` o cierre manual) el default 0 se conserva. Validación:

```bat
"build-reference\NEURONiK_WebPilotHost_artefacts\Release\NEURONiK Web Pilot.exe" --selftest
echo %ERRORLEVEL%   :: debe ser 0

"build-reference\NEURONiK_WebPilotHost_artefacts\Release\NEURONiK Web Pilot.exe" --selftest-force-fail
echo %ERRORLEVEL%   :: debe ser 1 (simulacro temporal, imprime selftest-forced-fail)
```

El simulacro `--selftest-force-fail` (RETIRADO 2026-09-17 tras validar) saltaba el E2E y
publicaba el veredicto FAIL en el primer tick: comprobó que el exit 1 llegaba al proceso sin
depender de que el bridge fallara de verdad. En `--auto-quit`/cierre manual el exit sigue
siendo 0.
**VALIDADO (2026-09-16 23:5x): ambos caminos, exit 0 y exit 1.** Revalidado 2026-09-17 con el
host corregido del fallback (ambos en verde).

## Ejercicio del fallback embebido — VALIDADO (2026-09-17), con 2 bugs arreglados

Ejercicio: renombrar `WebPilot/out`, lanzar `--selftest`, restaurar. Resultado final:
**`[embedded fallback: 8]` (497 KB), E2E del bridge OK en ambos sentidos sobre la WebUI
embebida, exit 0**; con disco 8/496 KB; y `--selftest-force-fail` sigue publicando exit 1.

- **Bug 1 — fuga de exit code en timeout:** un selftest que moría por `timeout` sin veredicto
  salía con 0. `finish()` ahora publica FAIL si el selftest acaba sin veredicto
  (`selftestFinish()` graba el código ANTES de llamar a `finish`, así que nunca lo pisa).
- **Bug 2 — el snapshot embebido servía la página 404 como `index.html` (causa raíz del
  timeout):** el glob de CMake excluía `_not-found` pero NO la ruta `404/`, y
  `404/index.html` ganó el identificador `index.html` (juce_add_binary_data usa UN identificador
  por basename, orden alfabético). Síntoma perverso: `resources served: 7 (460 KB)` con **cero
  misses** — pedía 7 y le servían 7, pero el documento era el not-found (sin `.panel`).
  Arreglo en `CMakeLists.txt`: excluir del snapshot `_not-found`, `404.html` y `404/…`; el
  matcher (`loadEmbeddedResource`) conserva el guard para que una ruta de error jamás
  responda a una petición normal. Lección: tras tocar el snapshot, verificar que
  `originalFilenames` no tiene duplicados (`grep '"index.html"' BinaryData1.cpp` → 1).

Checklist restante del paso 1: (a) visual: RANDOM nativo mueve morphX/Y en página, slider de
página mueve VOLUME nativo; (c) con telemetría dentro: XYPad nativo sigue a la página.
(b) fallback embebido: HECHO.

## Presets por el bridge (2026-09-17): protocolo v1 aditivo

La página ya lista, carga y guarda presets. Decisiones de diseño:

- **Wire (aditivo a v1, no hay bump de versión):** JS->nativo `listPresets`, `loadPreset{name}`,
  `savePreset{name}`; nativo->JS `presetList{presets,current}` y `presetError{operation,detail}`.
  Un load correcto cierra los gestos abiertos de la página y resincroniza TODO el estado
  (snapshot completo + presetList); un save correcto responde presetList.
- **Nombres no fiables:** `isUnsafePresetName` rechaza vacío, >100 chars, separadores, `..`,
  punto inicial y bordes con espacio → `presetError`, nunca un file write.
- **`PresetController` (interfaz en ParameterBridge.h):** el bridge posee el wire; el host
  inyecta un adaptador sobre el `PresetManager` del plugin (puntero NO owned: el procesador
  le sobrevive). Así `ParameterBridgeTest` prueba el protocolo con un backend falso, sin
  tocar `Documents/NEURONiK/Presets`.
- **El adaptador comprueba la existencia ANTES de cargar:** `loadPreset()` del PresetManager
  hace no-op silencioso si el fichero no existe; sin ese check un preset inexistente
  parecería cargar bien. `savePreset` verifica el fichero tras escribir.
- **Stats nuevos:** `presetsLoaded`, `presetsSaved`, `presetErrors`.
- **UI:** barra en `page.jsx` (select de carga + input/SAVE); deshabilitada en modo local.
  El hook expone `presetState/presetError/listPresets/loadPreset/savePreset` y pide la lista
  en el mount (tras `requestState`).
- **Tests:** sección 8 de `ParameterBridgeTest` (list/load/traversal/save/fail/no-backend),
  literales nuevos en el contrato anti-drift C++ y mjs, suite vitest del hook (42/42).
  Trampa encontrada: la sección 7 del test desmonta el sender con `setSender({})`; la sección
  8 debe reinstalar `recorder.sender()` o la respuesta no llega y el test segfaulta al indexar.
- **Pendiente de oído/vista:** cargar/guardar de verdad contra `Documents/NEURONiK/Presets`
  desde la ventana abierta (la barra está: seleccionar preset o escribir nombre y SAVE).

## Pantalla GENERAL en Next.js (2026-09-17, Fase 7)

Pestañas **BRIDGE/GENERAL** dentro de UNA página (`page.jsx`): el snapshot embebido sirve por
basename y una segunda ruta colisionaría con `index.html` (bug del 404 ya sufrido). La GENERAL
refleja la pestaña GENERAL nativa (IDs de `ParameterPanel.cpp`): motor, ADSR (gráfico SVG puro,
proporciones por tiempo real con piso mínimo, sustain a ancho fijo), unison, RANDOM y los tres
freeze. Puntos finos:

- **Un solo hook para ambas pestañas** (`SCREEN_PARAMETER_IDS = PILOT + GENERAL`): el estado es
  el mismo normalizado 0..1, así que un preset o un gesto nativo se ve en la pestaña que esté
  abierta. `ParamKnob`/`ParamToggle` pintan SU propia etiqueta (ParamControlShell): en GENERAL no
  se añade heading propio, solo un readout en unidades reales (`.control-value`).
- **Footer**: la validación es la del hook (`contractErrors`, validador NORMALIZADO). El anterior
  `validateState(controls.map(...), parameters)` comparaba estado normalizado contra rangos en
  unidades reales → errores falsos en cuanto entraron floats con min≠0 (envAttack 0.001).
- **pageContract.test.js** fija: `masterLevel` sigue siendo el `input[type=range]` nativo que el
  selftest conduce, `ParamSlider`/`ParamChoice` en la BRIDGE, `value={normalized}` y
  `handleChange(control.id…)` — todos los checks siguen en verde (vitest 42/42).
- **Selftest con 3er chequeo E2E (GENERAL)**: tras las dos direcciones de `masterLevel`, el
  selftest empuja `envAttack` (normalizado 0.5) desde nativo y lee el estado de la página desde
  el JSON del footer: los 11 ids de la GENERAL deben estar presentes y numéricos, y `envAttack`
  debe valer 0.5. El veredicto del selftest exige ahora las TRES direcciones
  (`selftestNativeToJsOk && selftestJsToNativeOk && selftestGeneralOk`).
- **CSS**: tabs/grupos/ADSR con tokens del tema (`--accent`, `--line`, `--muted`), sin frameworks
  (regla del skill JUCE_hybrid: vanilla CSS dentro del WebView).

**2ª compilación — enlace (previsto en el punto 1 del checklist):** 4x `LNK2019` sobre
`juce::MidiKeyboardComponent`/`KeyboardComponentBase` (los usa NEURONiKEditor, no PresetBrowser
como sospechábamos): `MidiKeyboardComponent` vive en `juce_audio_utils` → añadida al target del
host. Lección: la lib que falta no la indica el símbolo, hay que saber en qué módulo JUCE vive
cada componente.

**Trampa de pipeline corregida (build.bat):** el paso 4 era "blando" (aviso y continúa) y el
selftest del paso 8 corría igual con el exe VIEJO del host — un host sin recompilar daba
`RESULTADO: OK` engañoso porque el selftest solo prueba el bridge y la WebUI se carga en
caliente desde disco. Ahora el build recuerda `HOST_BUILD_FAILED` y, si el host no compiló,
**omite el selftest y sale con error**. Si un día quieres compilar solo el plugin, usa
`set WITH_SELFTEST=0` o compila los targets a mano.

## Teclado MIDI compartido en la página (2026-09-17, Fase 4)

La tab **KEYS** monta el teclado compartido `@abdsynths/midi-keyb` (el MISMO paquete que
consume ABDMS2000, evolucionado a v0.2.0 — nada de forks locales) + las ruedas de pitch/mod.
Puntos de diseño:

- **Wire (aditivo a v1):** JS->nativo `midiNoteOn{note,velocity}`, `midiNoteOff{note}`,
  `midiPitchBend{value -1..+1}`, `midiModWheel{value 0..1}`, `midiPanic` (sin campos);
  nativo->JS `midiNoteState{held[], pitchBend, modWheel}` (~6 Hz desde el timer del host).
  Campos fuera de rango → `stats.midiRejected`, jamás llegan al motor. Sin backend MIDI los
  actions se aceptan y se descartan en silencio.
- **`MidiController` (interfaz en ParameterBridge.h, patrón PresetController):** el bridge
  posee el wire; el host inyecta `MidiInjectionAdapter` sobre los `injectNoteOn/Off/`
  `injectPitchBend/injectController/requestAllNotesOff` del procesador — el MISMO camino
  lock-free (FIFO) que usa el teclado del editor nativo.
- **¡El piloto SUENA!:** `juce::AudioProcessorPlayer` + `AudioDeviceManager` con el dispositivo
  por defecto. Sin esto `processBlock` nunca corría en el host: las notas morían en el FIFO sin
  oírse y la telemetría del motor quedaba congelada. Orden de miembros: processor ANTES de
  deviceManager/player; el callback se desmonta en el destructor antes de que muera el
  procesador.
- **Notas mantenidas reales:** máscara de 128 bits (4×`atomic<uint32>`) plegada en
  `processBlock` desde el MIDI ya filtrado por canal (hardware + inyectado, ambas fuentes).
  Acumulativa entre bloques (el FIFO se vacía cada bloque; sin la máscara `held` parpadearía).
  `allNotesOff` del motor la pone a cero (el pánico no deja resaltes fantasma). Lectura por
  `getHeldNotes()` con bucle de desplazamiento (sin `<bit>`: el host compila C++17).
- **Feedback sin eco (API v0.2 del paquete):** `setPitchBend/setModWheel` mueven la rueda con
  `Wheel.setValue(n, false)` tras un gate `suppressWheelCallbacks` — visual sin re-disparar
  `onPitchBend/onModWheel` como si fuera input del usuario. `notesOffVisual` existe pero NO se
  usa en el loop de telemetría a propósito: el teclado mantiene su propio estado de pulsación y
  un repaint desde telemetría pelearía con el dedo del usuario. Nota fina: el slider del wheel
  es `min=-8192 max=8191` (¡centro 0, no 8192!) — el mapeo del feedback es
  `v>0 ? v*8191 : v*8192`.
- **Página (`KeysTab` en page.jsx):** contenedores por id (`#piano-keyboard`, ruedas) escritos
  con `innerHTML` y teclado creado UNA vez por mount de la tab (callbacks via `refs` para no
  recrearlo). PANIC de la página llama a `keyboard.panic()` (client-side, suelta sus teclas) +
  `sendMidiPanic()` (nativo, para hardware/DAW). `window.__pilotSendMidi` expone el camino de
  envío al host para el selftest.
- **Selftest 4ª dirección (MIDI):** la página envía noteOn/noteOff de la 60 por SU propio
  camino (`__pilotSendMidi`) y el host verifica la máscara de notas; en paralelo empuja
  CC1=64 nativo y lee el slider del wheel (`#mod-wheel-container .kbd-wheel-slider`, escala
  0..127 → normalizada) tras cambiar a la tab KEYS. 100% asíncrono (evaluateJavascript +
  Timers; el valor del wheel viaja en un `atomic<float>` miembro — las lambdas hermanas no
  pueden capturarse entre sí). Veredicto: las CUATRO direcciones.
- **Empaquetado pnpm (lección):** `pnpm install` desde WebPilot se colaba en el workspace raíz
  de la suite (`pnpm-workspace.yaml` de ABDSynths NO lista ABDNeural/WebPilot). Solución:
  WebPilot es ahora workspace anidado propio que incluye `ABDSharedAssets` y
  `ABDSharedCode/MidiKeyboard` como miembros (el `@abdsynths/shared@workspace:*` interno del
  paquete de teclado obliga a ello). Turbopack necesita `turbopack.root = raíz de la suite` en
  `next.config.mjs` (los symlinks resuelven fuera de WebPilot) y vitest necesita
  `server.fs.allow` para el setup compartido. El sprite `assets/bender.png` de la rueda se
  copió a `WebPilot/public/assets/` (el snapshot embebido lo sirve).
- **Tests:** sección 9 de `ParameterBridgeTest` (routing/rangos/panic/sin-backend), literales
  MIDI en el contrato anti-drift C++ y mjs, vitest del bridge (formas exactas + midiNoteState)
  y del hook (`__pilotSendMidi` + `midiState`, limpieza en unmount). 46/46 en verde.
- **VALIDADO por el usuario (2026-09-17):** el teclado suena y se comporta. El primer vistazo
  mostró el keybed colapsado a una tira negra — culpable: el CSS del CONTENEDOR (`.keys-strip`)
  no existía en la página (el keybed estira sus teclas a 100% de la altura del contenedor);
  el componente compartido estaba bien. Fix en `117ac33` y re-validado.

**Qué validar cuando compile** (en orden):

1. Que el host enlaza (primera vez con el plugin entero dentro; si falta un símbolo, añadir la
   lib JUCE que pida — candidata: `juce_audio_utils` si PresetBrowser toca file choosers).
2. `--selftest` en verde (el E2E no cambió: `masterLevel` sigue siendo el `input` nativo).
3. A la vista: mover un knob del panel nativo REAL (p. ej. ATTACK) y ver que la página no
   cambia (no es de las 4 que muestra) pero RANDOM sí debe mover morphX/morphY en la página;
   y mover un slider de la página debe verse en el knob VOLUME del panel.
4. Con la carpeta `WebPilot/out` renombrada, el host debe seguir cargando la UI (fallback
   embebido) y el log debe contar recursos con `[embedded fallback: N]`.

## Migrar la primera pantalla real a Next.js: qué falta (2026-09-16)

El piloto ya valida la cadena completa (Next.js estático + WebView2 + bridge bidireccional +
contrato versionado). Lo que sigue es migrar UI real del plugin. Candidato natural: la pestaña
**GENERAL** (`ParameterPanel`, `Source/UI/ParameterPanel.{h,cpp}`, 274 líneas), porque es la que
usa el contrato de parámetros tal cual (sliders/choices sobre IDs) sin piezas nativas especiales.

En orden, lo que falta:

1. **Doble host temporal (lo primero y más barato).** Dentro del `WebPilotHost`, sustituir la tira
   nativa de comparación por un `ParameterPanel` real del plugin (necesita `NEURONiKProcessor&`, no
   solo el APVTS: los botones de acción del panel leen el procesador). Verificación: mover un
   slider nativo del panel real mueve la página, y viceversa. Si esto pasa, la migración es
   mecánica.
2. **Transporte de acciones de panel.** El contrato de parámetros cubre sliders/choices, pero
   `ParameterPanel` también dispara acciones (randomize, preset save/load). Añadir al protocolo
   (versión 2, aditiva) un mensaje `panelAction { name, args? }` JS->nativo y registrar las
   acciones soportadas en el contrato JSON.
3. **Reutilizar el tema.** `ThemeManager` expone colores (surface, text, accent…). Exportarlos al
   bridge (un `themeChanged` nativo->JS o variables CSS inyectadas) para que la pantalla web y la
   nativa no diverjan visualmente.
4. **Componentes web equivalentes.** Mapear los controles custom JUCE (XYPad, LcdDisplay,
   EnvelopeVisualizer, SpectralVisualizer) a React. Para la pestaña GENERAL no hace falta ninguno;
   son la fase 2 de la migración (y probablemente canvas, no DOM).
5. **Estado del editor vs estado del plugin.** La página web hoy refleja el APVTS. Faltan:
   restaurar `window.__pilotReady` tras recarga (ya funciona) y decidir qué pasa con parámetros
   uiOnly (randomStrength) — hoy viajan y no tienen efecto DSP: documentar en el contrato que son
   "efectivos solo vía acciones".
6. **Decisión formal Next.js** (punto de decisión del ROADMAP) con los números reales de esta
   sesión: bundle 476 KB / 8 peticiones, arranque 886-1166 ms en caliente, y el coste del runtime
   (~572 KB crudo / 172 KB gzip) como suelo conocido.

No-bloqueantes pero a tener en cuenta: el host mide `document interactive` ~3.2 s en la primera
ejecución tras recompilar (perfil de WebView2 frío; en caliente vuelve a <1.2 s); si la migración
huele lenta, repetir la medición con el perfil caliente antes de culpar al framework.

## Cobertura de tests del lado web (auditoría 2026-09-16)

- **Hueco encontrado:** `WebPilot/lib/bridge.js` (la contrapartida JS del protocolo versionado)
  no tenía NINGÚN test: su filtrado de mensajes, modo local y dispose solo se ejercitaban en el
  selftest E2E. `parameters.js` además sin cubrir `defaultState`/`validateState`/
  `describeControl`/`divergentParameters`.
- **Relleno:** `tests/bridge.test.js` (9 tests contra un backend falso de `window.__JUCE__`:
  formato de cable exacto JS->nativo, `pageLoaded` en su propio event id, routing
  nativo->JS, mensajes malformados ignorados sin lanzar, dispose sin fugas, backend que
  lanza no tira la página) y `tests/parametersState.test.js` (11: siembra de defaults por
  tipo, validación de estado, divergencias, resumen de pantalla).
- **Suite WebPilot: 15 -> 35 tests.** Suite completa de la sesión: 10 C++ (ctest) + 35 web
  + 25 ABDSharedAssets.

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
NEURONiK_PresetRoundTripTest     31 comprobaciones  (verificado con build.bat 2026-09-16)
```

Los tres registrados en CTest: la suite pasa 6/6 sin hardware (build.bat, 2026-09-16, pasos 1-7 con
ModelMaker omitido por diseño).

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
build.bat                    :: plugin + contrato + WebUI + tests + selftest (usa build-reference)
build.bat build              :: lo mismo, en un directorio de build limpio
build.bat modelmaker         :: además compila la herramienta ModelMaker
build.bat build modelmaker   :: build limpio incluyendo ModelMaker
build.bat noselftest         :: omite el E2E del bridge (paso 8)
```

Pasos que ejecuta, en orden:

```text
1/8  cmake -S . -B <dir> -DCMAKE_BUILD_TYPE=Release
2/8  NEURONiK_ParameterExport + regeneracion de WebPilot\generated
3/8  NEURONiK_Standalone + NEURONiK_VST3
4/8  NEURONiK_WebPilotHost            (si falla, solo avisa)
5/8  WebPilot: pnpm build            (se omite si no hay node_modules)
6/8  NEURONiK_ModelMaker             (solo con 'modelmaker', ver abajo)
7/8  compilacion de los 8 tests + ctest --output-on-failure
8/8  selftest del bridge del piloto  (se omite con 'noselftest'; ver arriba)
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

Comprobado también después del último `build.bat` (2026-09-16 15:4x): el md5 del Standalone
**sigue coincidiendo** con esta copia, así que es una referencia válida y no un binario de una
revisión anterior.

Sirven como A/B de oído: entre las dos solo cambian la retirada de `harmMix`, la curva de
velocidad, el MIDI thru opt-in y las sincronizaciones ya documentadas. Con los defaults, la
diferencia audible esperada en presets existentes es **ninguna**; en el A/B, el default de
`velocityCurve` es `Linear` (identidad) y el de `midiThru` es off (antes el búfer se devolvía
siempre, lo que solo se nota si el host leía el MIDI del plugin).

### Build post-wrappers (2026-09-16 21:30)

Primera build con la familia de controles compartidos en la WebUI del piloto (wrappers React
`ParamSlider`/`ParamChoice` + `useParameterControls`; ver sección "Wrappers React en el
WebPilot"). Validada con `build.bat` completo: 10/10 tests + selftest del bridge OK
(NATIVO->JS y JS->NATIVO).

```text
NEURONiK_2026-09-16_2130_post-wrappers.exe                 5.383.168 bytes (Standalone)
NEURONiK_WebPilotHost_2026-09-16_2130_post-wrappers.exe    3.453.440 bytes (host del piloto)
```

Nota: el Standalone quedó **byte-idéntico** a la copia de las 15:17 (los wrappers viven en la
WebUI, no en el plugin); el binario que cambia es el host del piloto, y la WebUI nueva es la
que `build.bat` exporta a `WebPilot\out` (paso 5/8) — el host la carga desde ahí en caliente.

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

## Persistencia de estado (Fase 4 cerrada, 2026-09-17)

`NEURONiK_StatePersistenceTest` (procesador REAL, mismas fuentes que el host): roundtrip
de sesión DAW completo — editar parámetros + mapping MIDI en A, getStateInformation,
setStateInformation en B (instancia fresca), y comparar: 0 diferencias en el APVTS
completo, mapping restaurado, y contratos de robustez (null/basura/estado extranjero
ignorados sin corromper el estado vivo; estado de build antigua con ids extra aplica lo
conocido y conserva defaults).

**Bug real destapado por el test:** `MidiMappingManager::saveToValueTree` terminaba con
`v.getOrCreateChildWithName("MIDIMAPPINGS") = midiNode;` — la asignación de ValueTree en
JUCE **no copia contenido, re-referencia objetos** (semántica de puntero compartido). El
nodo quedaba vacío y TODO mapping de MIDI Learn se perdía silenciosamente al guardar la
sesión del DAW (los presets no estaban afectados: `saveToValueTree` solo lo llama
`getStateInformation`). Fix: `removeChild` + `appendChild`, con comentario explicando la
trampa para que nadie "simplifique" de vuelta.

**Pipeline:** `build.bat` reordenado — la WebUI (paso 4) ANTES del host (paso 5), porque el
host embebe `WebPilot/out` en el enlace (`juce_add_binary_data`); compilar el host antes
dejaraba dentro el bundle de la pasada anterior. El test nuevo (`NEURONiK_StatePersistenceTest`)
entró en el paso 7.

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

## 2026-09-17 (b): Siembra de juce::Random + bomba de paréntesis en build.bat

**Siembra Random (regla 7A del skill JUCE hybrid — preparación WASM):**
- `LFO` ahora toma semilla explícita por constructor (antes: `getMillisecondCounter`, no determinista; y el miembro pasaba por el ctor por defecto, que busca entropía del sistema).
- `BaseEngine` instancia `lfo1`/`lfo2` con semillas distintas (S&H decorrelacionado).
- `NeurotikVoice(int voiceIndex)` siembra determinista y única por voz (con semilla idéntica, el ruido de excitación sería idéntico entre voces en unison: artefacto audible).
- `ParameterPanel::randomizeParameters` usa instancia local sembrada con reloj en vez de `getSystemRandom()` (UI, no RT; pero mismo principio).

**Bug del pipeline (explicaba builds que morían en silencio):** en la rama de error del paso 4 había un `(staleness)` sin escapar dentro de un bloque `if (...)`. cmd parsea el bloque entero aunque la condición sea falsa: el `)` cerraba el bloque prematuramente y el script moría con "No se esperaba . en este momento" justo al terminar el paso 4 — sin llegar nunca al `pause` final. Escapado como las demás líneas. Cada pasada ahora deja además `build-last-run.log` (envoltorio PowerShell Tee-Object, UTF-8, exit code propagado).

## 2026-09-17 (c): Fase 6 — spike Vite contra-piloto (A/B cerrado)

**Qué es `WebPilotVite/`**: contra-piloto que compila la MISMA página que Next
(`WebPilot/app/page.jsx` + `lib/`, importados 1:1 — cero copias) con Vite en vez de
Next. Solo añade: `index.html`, `src/main.jsx` (entry que importa los CSS globales que
en Next llevaba el layout), `src/main.css` (reset mínimo) y `vite.config.js`.
Es miembro del workspace anidado de WebPilot (`pnpm-workspace.yaml`), así que los
paquetes compartidos (@abdsynths/shared, @abdsynths/midi-keyb) resuelven igual.

**Veredicto técnico: Vite gana en todo lo medible.**

| Métrica | Next (next build) | Vite (vite build) |
|---|---|---|
| Bundle en disco | 854 KB / 10 recursos | **471 KB / 4 recursos** (-45%) |
| Tiempo de build | 15-25 s | **2-6 s** |
| Selftest del host | RESULT: OK, exit 0 | **RESULT: OK, exit 0** (mismo árbitro) |
| Fallback embebido | 0 | 0 (con el fix de publicDir) |
| Config extra | turbopack.root a la suite (obligatorio) | root local (resuelve symlinks solo) |

**A/B hecho con swap de directorios** (`out` ↔ `out-vite`) sobre el MISMO exe del host,
modo disco y modo embebido (rebuild del target del host). El arranque (~7.2-7.9 s react
ready) está dominado por el arranque frío de WebView2 y resultó empardado: el ahorro
real del bundle Vite se verá en el parseo JS y en memoria, no en el primer paint del
host de prueba.

**Trampas del camino:**
1. `vite.config.js` con `--config` relativo + `pnpm --filter` falla (duplicación de
   ruta). Sin `--config`: Vite encuentra vite.config.js en su cwd.
2. Con raíz elevada a la suite (como hicimos en Turbopack) Rollup exige `input`
   explícito y los `/src/...` absolutos del HTML se resuelven contra la raíz. Con raíz
   local todo funciona sin trucos — NO es necesaria la raíz-suite en Vite.
3. `publicDir` por defecto es `<root>/public`: el sprite `bender.png` del wheel vive en
   `WebPilot/public/` y había que apuntarlo a mano (si no, el fallback embebido se
   come 1 recurso y el A/B del snapshot queda cojo).

**Cómo construirlo**: `cd WebPilot && pnpm --filter @abdsynths/web-pilot-vite build`
(salida: `WebPilot/out-vite/`). Para probarlo en el host: swap `out` ↔ `out-vite` y
relanzar; para embeberlo, rebuild del target `NEURONiK_WebPilotHost` con el swap hecho.

**Pendiente de decisión**: el switch definitivo (migrar `build.bat` paso 4 a Vite y
dejar Next solo como referencia, o mantener ambos). El piloto quedó funcional en ambos.

## 2026-09-17 (d): Switch del paso 4 a Vite — Next queda como referencia

- `build.bat` (paso 4) ahora construye con **Vite** por defecto:
  `pnpm --filter @abdsynths/web-pilot-vite build`, salida DIRECTA a `WebPilot/out`
  (la ruta que consumen el snapshot embebido y el selftest — nada cambia de sitio).
  `emptyOutDir` deja `out/` solo con ficheros del motor activo: no se mezclan restos.
- **`build.bat nextui`** conserva la ruta Next intacta (misma página, otro empaquetador)
  para comparaciones futuras: `build.bat nextui [modelmaker|noselftest|tests|<dir>]`.
- `vite.config.js`: outDir a `../WebPilot/out` (antes `out-vite`, que ya no existe).
- Validado: pasada completa por defecto (Vite, 4 recursos/445 KB, selftest OK, 11/11
  ctest), pasada `nextui noselftest` (Next OK) y restauración canónica por defecto.

## 2026-09-17 (e): Fase 1 — frontera DSP separada (paridad bit-exacta)

**Qué se hizo:**
- `DspTypes.h` NUEVO: `GlobalParams` extraído del header de `ISynthesisEngine` a un
  header POD **sin JUCE** (única dependencia: el propio fichero). `ISynthesisEngine.h`
  lo incluye hacia atrás — nadie más cambió.
- `DspEngineFacade.h` ya no incluye juce_*.h (solo `DspTypes.h` + `DspEvent.h`, con
  forward-declaration del engine). El `#include <juce_audio_basics>` vive solo en el
  `.cpp`, donde también vive el adaptador `Event`→`MidiMessage`: la separación de
  tipos de evento del transporte JUCE es real (un host WASM nunca ve juce_*).
- API de parámetros en la fachada: `setGlobalParams` / `setPolyphony` (delegan en el
  engine, que hace el handoff RT-safe).
- Contrato de `ISynthesisEngine` documentado EN la cabecera: ciclo de vida,
  restricciones RT de `renderNextBlock`, hilos de los getters, y qué tipos cruzan
  la frontera.
- `DSPReferenceTest` ampliado con **paridad de frontera**: mismo motor, misma
  secuencia de notas (on → 8 bloques → off → 12 de cola) por la ruta JUCE directa
  (`renderNextBlock` + `MidiBuffer`) y por la ruta fachada (punteros crudos +
  `Runtime::Event`); comparación **muestra a muestra sin tolerancia**. Resultado:
  bit-exacta en 2×512 muestras (peak 0.216). Viable porque el DSP por defecto es
  determinista: semillas constantes (siembra del 2026-09-17) y `entropyAmount=0`
  (el jitter del resonador usa `getMillisecondCounter` pero está cortado a <0.001 —
  si algún día se activa por defecto, ese path necesitará sembrado determinista).

**Estado de la frontera:** `DspTypes.h` + `Runtime/*` compilan sin JUCE; el motor
interno (`BaseEngine`, voces, efectos) sigue usando juce_* por decisión de diseño
transicional (casilla abierta a propósito en el roadmap, se ataca con la Fase 5/WASM).

## 2026-09-17 (d) — Fase 5: DSP real compilado a WASM (smoke test Node en verde)

- `wasm/CMakeLists.txt` + `build_wasm.bat`: compilan el DSP REAL (sin port) sobre la
  frontera de la Fase 1 (DspEngineFacade). JUCE 8.0.12 con em++ requiere: (a) entorno
  de Visual Studio ANTES que emsdk en el PATH (bootstrap de juceaide; el bat lo auto-arma
  via vswhere), (b) generador Ninja (el generador VS no soporta el toolchain em++),
  (c) `-includeemscripten.h` global (bug de juce_SystemStats_wasm.cpp en 8.0.12: usa
  emscripten_get_now() sin incluir <emscripten.h>), (d) parche minimo en
  C:/JUCE/modules/juce_core/native/juce_ThreadPriorities_native.h (rama JUCE_WASM con
  tabla a 0, igual que LINUX; auditoria del hash original en /tmp/juce_patch_audit.txt —
  replicar el parche si se actualiza JUCE).
- `DspSources.cmake` (nuevo): lista DSP single-source compartida nativo<->WASM.
  Leccion ABDMS2000 aplicada (su WASM duplicaba la lista a mano y sufrio drift: le
  faltaban 4 fuentes del nativo, incluido SynthEngine.cpp).
- `SIMDWrapper.h` dual: nativo sigue con juce::dsp::SIMDRegister (intacto); WASM usa
  fallback escalar de 4 lanes con API identica (JUCE no define SIMDRegister bajo
  Emscripten: JUCE_USE_SIMD=0 y forzarlo choca con #error interno). `Resonator.cpp`
  ya no usa SIMDRegister directamente (pasa por simdGreaterThanOrEqual del wrapper).
  Paridad nativa intacta: DSPReferenceTest bit-exacta en verde (11/11).
- `Source/Wasm/NeuronikWasmBridge.cpp`: ABI C plana (convencion ABDMS2000) hablando
  con DspEngineFacade; layouts de GlobalParams/modMatrix por offsetof (JS nunca
  hardcodea offsets); static_assert de Event = 24 bytes standard-layout.
- Smoke test Node (`Tests/neuronik_wasm_smoke.mjs`): binario entregado via hook
  instantiateWasm (el glue ES6 con ENVIRONMENT=web,worker usa fetch sobre
  import.meta.url y no sabe leer file://), HEAPF32/HEAP32 en EXPORTED_RUNTIME_METHODS.
  Verifica: audio finito no nulo (peak 0.53), voz activa tras noteOn, drain de release
  tras allNotesOff (contrato: dispara releases RT-safe; la cola exponencial de 200 ms
  cruza el umbral de Idle ~1.4-1.9 s), exit 0.
- Artefactos: build-wasm/neuronik_dsp.js (13 KB) + .wasm (89 KB), ES6+MODULARIZE,
  listos para AudioWorklet. Quedan en la Fase 5: worklet JS, paridad WASM<->nativo
  (el DSP es determinista: bit-exacta alcanzable) y wire-up de presets/params en UI.
