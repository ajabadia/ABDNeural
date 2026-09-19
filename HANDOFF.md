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
- **`build.bat` lo ejecuta como paso 10/10** (tras la suite de tests; era 8/8 antes de que el WASM
  entrara como 3/10 y la WebUI del plugin como 4/10), solo si el host compiló y
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
(hoy es el 10/10: el WASM entró después como 3/10 y la WebUI del plugin como 4/10, ver "Procedimiento de build")
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
build.bat                    :: 10 pasos: contrato + WASM + WebUI del plugin + plugin + piloto + tests + selftest
build.bat build              :: lo mismo, en un directorio de build limpio
build.bat modelmaker         :: además compila ModelMaker (incrementa Source\ModelMaker\Version.h)
build.bat build modelmaker   :: build limpio incluyendo ModelMaker
build.bat tests              :: modo rápido: solo contrato + suite (ni plugin ni WebUI)
build.bat noselftest         :: omite el E2E del bridge (paso 10)
build.bat nowasm             :: omite el WASM del worklet (por defecto, si falla, aborta el build)
build.bat nextui             :: WebUI del piloto con Next en vez de Vite (referencia)
```

Pasos que ejecuta, en orden:

```text
1/10  cmake -S . -B <dir> -DCMAKE_BUILD_TYPE=Release
2/10  NEURONiK_ParameterExport + regeneracion de WebPilot\generated
3/10  build_wasm.bat: WASM + paridad + smoke + sync del worklet (aborta el build si falla)
4/10  WebUI: pnpm build              -> WebUI\dist  (la interfaz QUE EMBIBE EL PLUGIN)
5/10  NEURONiK_Standalone + NEURONiK_VST3   (embeben WebUI\dist en el enlace)
6/10  WebPilot: pnpm build            -> WebPilot\out (el piloto; se omite si no hay node_modules)
7/10  NEURONiK_WebPilotHost           (si falla, solo avisa; embebe WebPilot\out)
8/10  NEURONiK_ModelMaker             (solo con 'modelmaker', ver abajo)
9/10  compilacion de los 17 tests + ctest --output-on-failure
10/10 selftest del bridge del piloto  (se omite con 'noselftest'; ver arriba)
```

**El orden es la dependencia, no un gusto:** el `.wasm` lo produce 3/10, la WebUI lo copia a su
`dist` por el `publicDir` en 4/10, y el **plugin lo embebe en el enlace** en 5/10
(`juce_add_binary_data` sobre `WebUI\dist\*`). Con el plugin antes, se quedaba dentro el bundle y
el DSP de la pasada anterior (sintoma mudo: suena "el de antes"). Lo mismo vale para el host del
piloto, que embebe `WebPilot\out`.

Dos guards de staleness por la misma razon, y los dos abortan:

- 3/10/4/10: si `WebUI\dist\worklet\neuronik_dsp.wasm` falta o **no coincide** con
  `build-wasm\neuronik_dsp.wasm`, el plugin sonaria con un DSP viejo.
- 6/10/7/10: el mismo chequeo sobre `WebPilot\out\worklet` para la bancada del piloto.

Artefactos:

```text
<dir>\NEURONiK_artefacts\Release\Standalone\NEURONiK.exe      (con WebUI\dist embebida)
<dir>\NEURONiK_artefacts\Release\VST3\NEURONiK.vst3            (con WebUI\dist embebida)
<dir>\NEURONiK_WebPilotHost_artefacts\Release\NEURONiK Web Pilot.exe
<dir>\Release\NEURONiK_ModelMaker.exe                  (solo con 'modelmaker')
```

### La interfaz del plugin (`WebUI/`): la construye `build.bat` y la sirve el plugin

**Desde 8.1 (2026-09-19) la pagina ES la interfaz del plugin**: `NEURONiKEditor` monta
`Source/WebUI/NeuronikWebView.h`, que sirve `WebUI/dist` desde una copia **embebida** en el
binario. Por eso `build.bat` la construye en 4/10, antes de compilar el plugin, y por eso
`WebUI/dist` ya no es "una carpeta que no consume nadie".

El piloto React (`WebPilot/out`, 6/10 y 7/10) sigue vivo **solo mientras 8.1 no cierre el paso
2c**, porque su bancada es la unica implementacion del selftest de cuatro direcciones. Despues se
retira (ver `ROADMAP.md`, "Retirada del piloto").

```bash
cd ABDNeural/WebPilot && pnpm install   # WebUI es miembro del workspace anidado de WebPilot
cd ../WebUI && pnpm test                # 96 tests (vitest + jsdom)
cd ../WebUI && pnpm dev                 # navegador, modo local (sin host) — el bucle de iteracion
cd ../WebUI && pnpm build               # -> WebUI/dist  (lo mismo que hace el paso 4/10)
```

Los assets compartidos y el worklet no se duplican: el `publicDir` de Vite apunta a
`WebPilot/public`, asi que `dist\worklet\` sale con el procesador y el `.wasm` que acaba de
compilar el paso 3/10; y los estilos de `@abdsynths/shared` los **empaqueta Vite en el bundle**
(no se piden al disco en tiempo de ejecucion). Los selectores que el `--selftest` lee de la
pagina estan documentados y probados en `WebUI/README.md` (y en `WebUI/tests/`).

**Iterar la interfaz sin recompilar el plugin** (override de desarrollo, apagado por defecto):

```bat
set NEURONIK_WEBUI_DEV_DIR=D:\desarrollos\ABDSynths\ABDNeural\WebUI\dist
```

Con esa variable puesta, el plugin sirve la pagina desde disco: `pnpm build` (menos de un
segundo) y recargar, en vez de recompilar el plugin entero por cada retoque de CSS. Sin la
variable, el binario sirve su copia embebida y **no toca el disco** (cero rutas grabadas: el
VST3 funciona copiado a cualquier sitio).

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

**Pipeline:** `build.bat` reordenado — la WebUI ANTES del host (hoy 5/9 y 6/9; eran 4 y 5 cuando
el WASM todavía iba fuera del build), porque el
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

- `build.bat` (paso 4 entonces, **5/9 hoy**) ahora construye con **Vite** por defecto:
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

## 2026-09-18 — AudioWorklet del piloto (Fase 5, segundo hito)
- `WebPilot/public/worklet/neuronik-worklet.js`: AudioWorkletProcessor que
  instancia el módulo WASM real (glue ES6 importado; binario entregado por
  processorOptions como ArrayBuffer clonado — el scope del worklet no tiene
  fetch al mundo de la página) y renderiza via neuronikProcess. Eventos MIDI
  apilados a Runtime::Event (24 B), GlobalParams escritos por índice de campo
  (bpm f64 aparte), telemetría de voces/LFO cada ~21 ms.
- `WebPilot/lib/audioParams.js`: mapeo contrato -> índices de GlobalParams
  (21 campos reachables; bpm sin contrato aún). Conversion normalizada->real
  con la MISMA matemática del panel nativo (fromNormalized). Test de contrato
  (`tests/audioParams.test.js`): defaults C++ vs contrato, discretos, skew.
- `WebPilot/lib/audioWorkletEngine.js`: ciclo de vida del AudioContext en la
  página (botón SOUND ON = gesto de usuario), sync de parámetros por snapshot
  completo (cubre ediciones locales Y snapshots nativos del bridge), notas/
  wheels/panic en camino dual (bridge + worklet).
- Sincronización de artefactos: `sync_wasm.bat` / `pnpm --filter
  @abdsynths/web-pilot-vite sync:wasm` copia build-wasm/ -> public/worklet/
  (ejecutar tras cada build_wasm.bat; el .wasm se sirve desde la exportación).
- Validación: vitest 55/55, build Vite 4.1 s (306 KB JS), smoke Node del
  módulo servido (peak 0.53, drain OK), selftest del host exit 0 con la página
  nueva, build nativo + ctest 11/11.
- Pendiente de la Fase 5: wire-up de presets en la vía web.

## 2026-09-18 (b) — Paridad bit-exacta WASM<->nativo (Fase 5, tercer hito, CERRADO)
- `Tests/WasmParityTest.cpp` (target `NEURONiK_WasmParityTest` en CMake, junto a
  DSPReferenceTest): ejecuta 4 escenarios sobre la MISMA frontera
  (DspEngineFacade) que consume el puente WASM y vuelca el canal izquierdo a
  `build-wasm/parity-native.json`: A_neuronik_default (32 bloques),
  B_neurotik_default (32), C_fx_panico (48: panico a mitad y cola de
  reverb/delay), D_modmatrix (24: modMorphX con case 4).
- `Tests/neuronik_wasm_parity.mjs`: instancia el módulo WASM real y compara
  muestra a muestra con distancia en ulps (double fract32) + presupuesto por
  escenario. bpm se escribe partido en dos mitades de 32 bits (el modulo no
  exporta HEAPF64). `--strict` exige 0 ulps en TODO (para CI sin tolerancia).
- Resultado medido: **A/B/D bit-exactos a 0 ulps** (osciladores, envolventes,
  modMatrix — la libm coincide); C difiere en 2 de 6144 muestras a 16 ulps
  (~3e-8, -150 dBFS): divergencia MSVC vs musl de 1 ulp amplificada por el
  feedback del delay en la cola. Presupuesto de C documentado en 16 ulps;
  A/B/D exigen 0 (cualquier dif alli es regresion real).
- Integracion: `build_wasm.bat` pasa a 5 pasos (genera referencia nativa ->
  compila WASM -> paridad Node -> smoke). build.bat valida con exit 0.
- Falta el wire-up de presets en la via web (ultimo pendiente de la Fase 5).

## 2026-09-18 (c) — Wire-up de presets en la vía web (Fase 5, CIERRE)
- Un preset es estado APVTS **más** hasta 4 slots de SpectralModel (64
  parciales) entre los que morphea el Resonator; los modelos nunca viven en el
  APVTS, así que el snapshot de parámetros no los llevaba. Camino completo:
  - Puente WASM: `neuronikLoadModel(slot, engineType, ptr128floats, isValid)`
    (el modelo cuelga del engine concreto, no de la fachada).
  - Bridge nativo: interfaz `NativeModelController` + mensaje aditivo
    `modelsState` que viaja pegado a cada `syncAllParams` y tras cada
    `loadPreset`; el host adapta el processor con una vista file-backed
    (`modelPath<slot>`, la misma fuente que recarga un cambio de engine).
  - Página: `pushModelsToWorklet` + efecto que re-aplica los modelos tras cada
    cambio de engine del worklet (el switch reconstruye el engine y los slots
    vuelven a defaults hasta que la página los re-envía).
- Worklet: mensaje `neuronik:models` (buffer heap compartido de 128 floats por
  slot) + fix del `instantiateWasm` (method-shorthand + `.bind` nunca parseó:
  el worklet no podía cargar; ahora arrow function con this léxico).
- Paridad: nuevo escenario E (`E_modelo_espectral`, slot 0 con constantes
  exactas 2^-k y offsets 2^-7 para evitar doble redondeo f32/f64 entre MSVC y
  V8) — bit-exacto a 0 ulps. Presupuestos: A/B/D/E=0, C=16 (libm de la cola).
- Validación: build.bat RESULTADO OK (11/11 ctest, selftest bidireccional OK),
  vitest 56/56, contrato de protocolo en verde (gemelos C++ y JS),
  build_wasm.bat 5 pasos con paridad incluida.
- **FASE 5 CERRADA** — la vía web suena con el DSP real y consume presets
  completos (parámetros + timbre). Siguiente en el roadmap: quitar juce_* del
  motor interno y las decisiones abiertas (¿WASM con ambos motores?, formato
  de preset común).

## Fase 1 (de-JUCE del motor) — en curso

- Commit A (`5e24271`): `DspCore.h` con ports literales de jmin/jmax/jmap/jlimit,
  MathConstants, ignoreUnused, ScopedNoDenormals (máscara MXCSR 0x8040) y
  LinearSmoothedValue. Swap mecánico en Source/DSP. Validado bit-exacto.
- Commit B+C: port de **AudioBuffer** (HeapBlock, setSize/allocateData,
  setDataToReferTo, addFrom/copyFrom/clear, FloatVectorOperations escalares
  literales sin FMA) + vista zero-copy en la frontera (processor, test).
  `ISynthesisEngine::renderNextBlock` ya toma `dsp::AudioBuffer`.
- **Lección del arnés (bug latente del test desenterrado por el port):** el
  test limpiaba el buffer JUCE subyacente mientras el motor escribía vía la
  vista `dsp::AudioBuffer`: dos flags `isClear` desincronizados => clear() en
  no-op desde el bloque 2 => residuos entre bloques (21x el nivel). Era
  invisible en el baseline (heap fresco = páginas a cero). Diagnóstico con
  motores sobre memoria envenenada 0xAA/0x00: el motor es determinista. Fix:
  limpiar SIEMPRE a través del mismo objeto que recibe las escrituras.
- Fix de portabilidad: HeapBlock del port necesita move ctor/assign (clang/
  emscripten lo exige; MSVC era laxo). Paridad intacta.
- Validación: build.bat RESULTADO OK, paridad WASM A/B/D/E=0 ulps y C=16
  (presupuesto), smoke OK, vitest 56/56 tras sync_wasm.

## Fase 1 [4/6] — la frontera MIDI deja de ser JUCE (2026-09-18)

**Qué se hizo:** el motor ya no conoce el transporte MIDI de JUCE. Solo queda el
adaptador del lado host.

- `Source/DSP/DspMidiMessage.h` NUEVO: `dsp::MidiMessage`, port LITERAL del
  subconjunto que el motor interpreta (note on/off, pitch wheel, aftertouch de
  canal y polifónico, CC). Mismos cuerpos y defaults que JUCE, incluido el
  detalle que más importa: **`isNoteOff()` trata note-on con velocity 0 como
  note-off** (default de JUCE) y `getFloatVelocity()` sale del byte, no del
  float de entrada. Fuera del alcance a propósito: SysEx, meta y realtime.
- `Source/DSP/DspMidiBuffer.h` NUEVO: `dsp::MidiBuffer`, port de
  `juce::MidiBuffer` con el mismo empaquetado (`[int32 pos][uint16 size][bytes]`)
  y la misma semántica de inserción (ordenado por posición, port literal de
  `findEventAfter`; los empates quedan FIFO). `ensureSize()` + `clear()` que
  conserva la capacidad = cero asignaciones en el hilo de audio.
- `dsp::roundToInt` en `DspCore.h`: port literal del truco de doble precisión de
  JUCE (empates **al par**, no como `std::lround`). Es lo que cuantiza la
  velocity de nota en `floatValueToMidiByte`, así que sin él el port habría
  cambiado el sonido en los empates.
- `Source/DSP/Runtime/JuceMidiAdapter.h` NUEVO: frontera
  `juce::MidiBuffer` → `dsp::MidiBuffer` para hosts JUCE. Copia los bytes crudos
  (cero re-cuantización), conserva orden y posiciones, y descarta lo que el
  motor no interpreta (SysEx/meta/realtime y >3 bytes). `#error` explícito si
  se incluye bajo `__EMSCRIPTEN__`.
- `DspEngineFacade` deja de incluir `juce_audio_basics`: construye
  `dsp::MidiMessage` con las mismas factorías y **reutiliza un `dsp::MidiBuffer`
  miembro** (antes creaba un buffer local por bloque → asignaba en el hilo de
  audio). La frontera `Runtime/*` es ya 100 % libre de JUCE.
- `Source/Main/NEURONiKProcessor`: traduce a `engineMidiBuffer` cada bloque
  (miembro reutilizado). El resto del procesador (filtro de canal, curva de
  velocidad, máscara de notas, MIDI thru) sigue con su `juce::MidiBuffer`: es el
  lado host y ahí JUCE es lo correcto.
- Limpiezas de paso: `IVoice.h` incluye `DspCore.h` (antes obtenía
  `dsp::AudioBuffer` por rebote de `juce_audio_basics`); eliminado
  `Source/DSP/Synthesis/ResonatorSound.h`, que era **código muerto** (cero
  referencias) y el único `juce_audio_processors` del árbol DSP.
- `Tests/MidiPortTest.cpp` NUEVO (target `NEURONiK_MidiPortTest`, en ctest y en
  el paso 7 de `build.bat`): anti-drift contra los originales — barrido de 2.849
  patrones de bytes comparando TODOS los predicados/getters, 1.824 casos de las
  seis factorías comparando bytes crudos, 2.541 valores de velocity (incluye el
  empate 63.5), `roundToInt` en semienteros, `getMidiNoteInHertz` en las 128
  notas, semántica de `dsp::MidiBuffer` (orden, posiciones, empates, `clear()`) y
  del adaptador (qué se copia, qué se descarta, reutilización del destino).

**Verificación ejecutada (2026-09-18, todo en esta máquina):**

```text
build_wasm.bat (5 pasos)                    EXIT 0 → paridad A/B/D/E = 0 ulps,
                                            C = 16 (presupuesto), smoke peak=0.53199
cmake --build ... NEURONiK_Standalone       EXIT 0 (procesador + editor + plugin)
ctest --test-dir build-reference -C Release 12/12 (11 previos + MidiPortTest)
pnpm --filter @abdsynths/web-pilot-vite build  EXIT 0 (bundle de la WebUI)
NEURONiK_WebPilotHost + --selftest          EXIT 0 → 4/4 direcciones:
                                            NATIVO->JS, JS->NATIVO, GENERAL y MIDI
```

La paridad no se movió por el port: los picos nativos de los cinco escenarios
son idénticos a los de antes (A 0.531995, B 0.006339, C 0.528966, D 0.553536,
E 0.434755) y A/B/D/E siguen bit-exactos. El selftest es el que valida de punta
a punta el lado host del paso: la página manda note on/off de la 60 y el host
lee la máscara de notas (su línea `MIDI: ... -> OK` pasa por
`NEURONiKProcessor` → `copyToDspMidiBuffer` → `dsp::MidiBuffer` → motor).
Falta la pasada completa de `build.bat`, que repite todo lo anterior en un solo
comando (y solo añade el bundle embebido del host y el informe de pasos).

**Lo que aún queda de JUCE en el motor** (pasos 5/6 y 6/6):

| Dependencia | Dónde |
|---|---|
| `juce::Reverb` | `Source/DSP/Effects/Reverb.h` |
| `juce::dsp::SIMDRegister` | `Source/DSP/Utils/SIMDWrapper.h` (rama nativa; WASM ya usa el fallback escalar) |
| `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` | 10 clases del DSP (incluidos `BaseEngine.h`, `LFO.h`, los efectos y las voces) |
| `JUCE_DEBUG`/`DBG`/`jassertfalse` | `Source/DSP/DSPUtils.h`, `AdditiveVoice.cpp`, `NeurotikVoice.cpp` |
| includes `juce_core`/`juce_audio_basics` residuales | `Envelope.h`, `FilterBank.h`, `Oscillator.h`, `Resonator.h`, `ResonatorBank.h`, `RhythmicDivision.h`, `Saturation.h`, `Chorus.h`, `Delay.h`, `NeurotikVoice.h` (varios son vestigiales: ya no usan ningún símbolo `juce::`) |

## Arreglo de tiempo real: el jitter de entropía y el placeholder de `Resonator` (2026-09-18)

Dos cosas que marcó la auditoría del motor, ya cerradas:

1. **`Resonator::prepareEntropy` ya no asigna en el hilo de audio.** Hacía
   `ampJitterBuffer.resize(...)` dentro del callback cuando la entropía estaba
   activa (guardado por `entropyAmount < 0.001f`, y la entropía es 0 por defecto,
   pero es exactamente la regla ZERO ALLOCATIONS). Ahora los buffers se reservan
   UNA vez con `Resonator::prepareJitterBuffers(maxBlockSize)`, que llama
   `AdditiveVoice::prepare()`; `prepareEntropy` solo rellena y deja
   `jitterLength = min(numSamples, capacidad)`. Si un host entrega un bloque mayor
   que el preparado, el jitter se recicla por módulo (determinista) en vez de
   reservar: con el bloque dentro de lo reservado `sampleIdx % jitterLength ==
   sampleIdx`, así que el audio es idéntico (paridad A/B/D/E = 0 ulps y picos
   nativos sin cambios).
2. **Fuera el placeholder `Resonator::processSample()` sin argumento.** Devolvía
   `processSample(0)` («This won't work as is») y ModelMaker lo llamaba de verdad:
   con entropía activa y sin jitter preparado leía fuera del vector. Ahora hay una
   sola sobrecarga indexada, ModelMaker pasa su `i`, y la ruta de entropía exige
   `jitterLength > 0` (si nadie preparó el jitter queda inerte en vez de tocar
   memoria inválida).

## Fase 1 [6/6] — el motor deja de incluir JUCE (2026-09-18)

- `DspDebug.h` NUEVO: `dspDbg(...)`, port de `DBG` con la misma puerta
  (`! defined (NDEBUG)` = `JUCE_DEBUG`): en Release no compila ni evalúa la
  expresión. Escribe en stderr (el motor no tiene Logger).
- `DspLeakedObjectDetector.h` NUEVO: `dspLeakDetector` /
  `dspDeclareNonCopyableWithLeakDetector(Class)`, port de
  `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` + `juce::LeakedObjectDetector`.
  **Como en JUCE, el detector solo existe en Debug** (en Release el macro deja
  únicamente el borrado de copia), así que el binario Release no engorda.
  `DspCore.h` lo incluye al final para que el macro siga estando disponible donde
  antes lo ponía juce_core.
- Macro de JUCE sustituido en las 9 clases del DSP que lo usaban (`BaseEngine`,
  `LFO`, los dos motores, `Chorus`, `Delay`, `Saturation`, `Reverb`,
  `NeurotikVoice`).
- `DSPUtils.h`: `JUCE_DEBUG`/`DBG`/`jassertfalse` → `dspDbg`/`dspAssert`, sin la
  puerta explícita (el macro ya es un no-op en Release). `dsp::ignoreUnused
  (paramName)` mata además los **8 avisos C4100** que rompían la puerta de «0
  avisos»: el plugin y el host del piloto compilan ahora **limpios**.
- Fuera los includes `juce_core`/`juce_audio_basics` **vestigiales** (ninguno de
  esos ficheros usaba ya un símbolo `juce::`): `Envelope.h`, `FilterBank.h`,
  `Oscillator.h`, `Resonator.h`, `ResonatorBank.h`, `RhythmicDivision.h`, `LFO.h`,
  `Saturation.h`, `Chorus.h`, `Delay.h`, `NeurotikVoice.h`, `DSPUtils.h`,
  `BaseEngine.h`, `NeuronikEngine.h`, `NeurotikEngine.h`. `Resonator.h` recupera
  `<cstdint>`/`<vector>`, que obtenía por rebote.
- `Tests/LfoSyncTest.cpp` incluye `juce_core` explícitamente: usaba `juce::String`
  en sus ayudantes apoyándose en el include del DSP.

**Lo único que queda de JUCE en `Source/DSP`** (a propósito) — `juce::Reverb`
se portó en el paso 5/6, ver la sección siguiente:

| Dependencia | Dónde | Por qué |
|---|---|---|
| `juce::dsp::SIMDRegister` (rama nativa) | `Utils/SIMDWrapper.h` | es la implementación nativa real, no un vestigio; WASM ya usa el fallback escalar |
| `juce::MidiBuffer` → `dsp::MidiBuffer` | `Runtime/JuceMidiAdapter.h` | es la frontera del host JUCE, por diseño |

**Verificación (2026-09-18):** `build_wasm.bat` EXIT 0 (paridad A/B/D/E = 0 ulps,
C = 16, smoke peak 0.53199), `ctest` 12/12, Standalone y host del piloto EXIT 0 con
**0 avisos**, `--selftest` 4/4 direcciones, y `NEURONiK_ModelMaker` EXIT 0
(comprueba el cambio de `processSample`; su `Version.h` se restauró).

## Fase 1 [5/6] — el motor deja de depender de juce::Reverb (2026-09-18)

- `Effects/DspReverb.h` NUEVO: port libre de JUCE de `juce::Reverb` (Freeverb,
  JUCE 8.0.12, `juce_audio_basics/utilities/juce_Reverb.h`): 8 comb filters en
  paralelo + 4 all-pass en serie por canal, con los tunings y el stereo spread
  idénticos. `Effects/Reverb.h` queda como envoltorio de producto (mapeo
  mix→wet/dry y suavizado), de modo que el efecto puro es reutilizable tal cual.
  **Ya no queda ninguna dependencia de `juce_audio_basics` en `Source/DSP/Effects`.**
- `dspUndenormalise` (en `DspCore.h`) NUEVO: port de `JUCE_UNDENORMALISE` con una
  decisión de política documentada. El macro de JUCE está definido **solo en x86**
  y **no es un no-op**: `(x + 0.1f) - 0.1f` redondea dos veces, aplasta los
  denormales (su propósito) y **perturba los valores normales en ~2 ulps de 1.0**.
  `juce::Reverb` lo aplica a `last` y a `temp` en cada muestra de cada comb
  filter, así que el mismo `juce::Reverb` **calculaba distinto en nativo y en
  WASM**. El port es no-op uniforme: la paridad bit a bit nativo↔WASM es la
  invariante del motor, y los denormales en x86 ya los cubre
  `dsp::ScopedNoDenormals`, que es determinista porque cambia el modo de la FPU y
  no las muestras.
- **La puerta de paridad se endurece: los cinco escenarios a 0 ulps.** El
  escenario `C_fx_panico` (delay + chorus + reverb a mix no nulo + pánico)
  medía 16 ulps y estaban atribuidos a libm (`sin`/`exp` en el feedback del delay)
  desde la Fase 5. **No era libm: era `JUCE_UNDENORMALISE`.** Con el port, C mide
  0 ulps y su presupuesto vuelve a 0 en `Tests/neuronik_wasm_parity.mjs` (la
  historia queda escrita en la cabecera del test, para que nadie vuelva a
  atribuirlo a libm).
- **Bug latente del port de `dsp::HeapBlock` corregido**: `clear` y `allocate`
  tomaban **bytes** y JUCE toma **elementos** (`sizeof(ElementType) * numElements`).
  Era invisible porque sus únicos consumidores eran `AudioBuffer` (que no usa
  `HeapBlock`) y `DspMidiBuffer` (`HeapBlock<uint8_t>`: elementos == bytes).
  `juce::Reverb` sí lo expone: `buffer.clear((size_t) bufferSize)` sobre un
  `HeapBlock<float>` con la semántica de bytes dejaba 3/4 del buffer de cada comb
  filter sin inicializar y realimentaba basura —la reverb divergía a ~1e35 en
  pocos bloques, y por eso el primer `<reverb>.reset()` no bastaba para que dos
  renders fueran idénticos—. Corregido en `DspCore.h` con la nota de la
  discrepancia; `DspMidiBuffer` no cambia de comportamiento (uint8).
- `build_wasm.bat`: nuevo paso **[6/6]** que ejecuta `sync-wasm.mjs`. El drift de
  artefactos (worklet servido con el DSP de la pasada anterior) ya había ocurrido
  dos veces y no se detecta en el código porque los `.js`/`.wasm` se versionan.

**Tests nuevos** (el mismo fuente se compila dos veces, con y sin el define):

- `NEURONiK_DspReverbParityTest`: la política enviada (no-op uniforme) contra
  `juce::Reverb` con presupuesto documentado, más determinismo (dos renders bit
  idénticos, con `reset()` de por medio) y una cola larga de silencio sin NaN ni
  infinitos.
- `NEURONiK_DspReverbJucePolicyTest`: el mismo fuente con
  `-DDSP_UNDENORMALISE_JUCE_POLICY=1` (réplica de `JUCE_UNDENORMALISE`) exigiendo
  **0 ulps**. Esto es lo que prueba que el port es literal: la única diferencia
  con `juce::Reverb` es la política, y queda cuantificada.

**Medido** (MSVC x64 Release, 48 kHz, 24 bloques de 512, estéreo y mono): modo
JUCE **0 ulps** y 0.000e+00 en las 36.864 muestras comparadas; modo enviado
maxDiff **2.384e-07** (exactamente 2 ulps de 1.0) y ≤ 5.632 ulps en cola de
magnitud pequeña. La condición de arquitectura es propia (`DSP_HOST_IS_X86`),
**no `JUCE_INTEL`**: un header JUCE-free no puede depender de un macro que solo
existe si antes se incluyó JUCE —el primer intento hacía que la política de JUCE
nunca se activara y el test del port literal pasaba en falso—.

**Verificación (2026-09-18):** `build_wasm.bat` (6 pasos) EXIT 0 → paridad
**A/B/C/D/E = 0 ulps** (C incluido) y smoke peak 0.53199; `ctest` **14/14**;
host del piloto EXIT 0 con `--selftest` **4/4** direcciones (NATIVO↔JS, GENERAL,
MIDI); `NEURONiK_WebPilotHost` compila con 0 avisos nuevos. `juce::Reverb` ya
solo aparece en el test que lo usa como referencia.

## Migración de los efectos a DspEffects — chorus, delay y saturación (2026-09-19)

Continuación de la Fase 1 [5/6]: la reverb abrió el módulo compartido
`ABDSharedCode::DspEffects` (`DspEffects/DspReverb.h`) y ahora se mueven los tres
efectos que todavía vivían solo en el synth. La partición es la misma en los tres:
**motor puro en el módulo compartido, política de producto en el envoltorio.**

- `DspEffects/DspChorus.h` NUEVO: chorus estéreo de línea retardada modulada (LFO
  de fase, 5ms..30ms, lectura interpolada lineal, mezcla wet/dry). API por
  muestra: `processSample (canal, x, depth, mix)` + `advance (rateHz)`.
- `DspEffects/DspDelay.h` NUEVO: retardo estéreo realimentado (buffer circular de
  2 canales, lectura interpolada, escritura de `input + delayed * feedback`). API
  por muestra: `processSample (canal, x, delayInSamples, feedback)` +
  `advanceWritePosition()`.
- `DspEffects/DspSaturation.h` NUEVO: la forma del soft-clipping
  (`atan (x * drive) * 0.63661977236f`) como utilidad estática sin estado (su
  "estado" en el producto era el smoother del drive, que es política).
- `Source/DSP/Effects/{Chorus,Delay,Saturation}.h` pasan a ser envoltorios de
  producto: suavizado de parámetros (20ms/50ms), mapeo (segundos -> muestras,
  amount -> drive, recorte del feedback a 0.95), puerta de denormales y mezcla.
  **La API pública no cambia**, así que `BaseEngine` no toca una línea.

**Por qué por muestra y no por bloque (la decisión que sostiene la paridad).** Los
smoothers de los tres efectos se leían *dentro* del bucle de muestras, así que un
`processBlock` con parámetros por bloque habría movido rate/depth/mix/tiempo/
feedback en cada muestra: otra salida. Por eso el motor compartido expone la
muestra y el consumidor aporta la política, y no al revés. La contrapartida es un
contrato de llamada explícito (recorrer los canales y luego avanzar una vez por
muestra), documentado en la cabecera de cada motor junto al `channel % 2`
heredado del original.

**Nota (denormales).** El `ScopedNoDenormals` del delay lo sigue abriendo el
consumidor: la API compartida es por muestra y no tiene nivel de bloque. El
original lo abría en su `processBlock`, así que está exactamente en el mismo
sitio.

**Desviación documentada.** `dsp::Chorus::prepare()` pone el puntero de
escritura a 0. El original no lo hacía: con un `prepare()` en caliente (cambio de
sample rate) el puntero podía quedar por encima del buffer nuevo y la primera
escritura se salía del rango. En el uso normal (un `prepare` antes de procesar) el
puntero ya valía 0, de modo que no cambia ningún resultado definido. `reset()`, en
cambio, sigue sin tocarlo (solo vacía el buffer), como el original.

**Test nuevo:** `Tests/DspEffectsParityTest.cpp` -> `NEURONiK_DspEffectsParityTest`
(registrado en `ctest` y en la lista de targets de `build.bat`). Aquí no hay
implementación ajena contra la que comparar (esto no es un port de JUCE), así que
el test lleva una **referencia congelada**: la copia literal de los tres efectos
tal y como estaban antes de moverse. Mismo guion de parámetros por bloque, misma
entrada determinista (LCG, sin reloj ni `rand`), y se exigen **0 ulps** muestra a
muestra. Cubre: chorus estéreo/mono/3 canales (el `channel % 2` del motor), delay
estéreo/mono, saturación por bloque (incluido el primer bloque con drive
exactamente 1.0, que pasa por la puerta de bypass del producto) y por muestra,
determinismo entre dos objetos nuevos, y una cola de 200 bloques de silencio con
fb 0.95 sin NaN ni infinitos.

**Medido** (48 kHz, 24 bloques de 512): **0 ulps en las 7 comparaciones** (147.456
muestras) con MSVC x64 Release (`/O2 /W4`) y con MinGW g++ 10.3
(`-O2 -DNDEBUG -Wall -Wextra`), **0 avisos** con los dos compiladores.

**Verificación:** pendiente de la pasada completa de `build.bat` (los 17 targets
de ctest, que ahora incluyen estos dos tests).

## Sustrato: dsp::AudioBuffer contra juce::AudioBuffer (2026-09-19)

`Tests/AudioBufferParityTest.cpp` -> `NEURONiK_AudioBufferParityTest` (ctest y
lista de targets de `build.bat`). Cierra el hueco que dejó la corrección del
helper: el port de `AudioBuffer` era el único consumidor del sustrato sin test
contra su original (los otros dos ports, `MidiMessage`/`MidiBuffer`, ya estaban
pinados en `MidiPortTest`, y la reverb tiene el suyo). La comparación vive aquí
porque el módulo compartido es JUCE-free por contrato.

- **Paridad de datos.** El mismo guion sobre los dos tipos (setSize con sus
  banderas, clear por canal, setSample/addSample, applyGain, applyGainRamp,
  addFrom/copyFrom y sus variantes con rampa, reverse, makeCopyOf, getMagnitude,
  getRMSLevel) deja las mismas muestras bit a bit, la misma magnitud/RMS y la
  misma bandera `hasBeenCleared()`.
- **Estructura de punteros (el guard de la rama que estuvo muerta).** Barrido de 1
  a 40 canales preguntando dónde vive el array de canales: dentro del objeto hasta
  **31** y en el heap desde 32, igual que JUCE (`numChannels < 32`). Con el helper
  devolviendo 1 la frontera medida era **0** en el port y 31 en JUCE: ese
  desacuerdo es exactamente lo que delata el test.
- **Memoria externa, movimiento y copia.** Un buffer que referencia canales del
  que llama: tras moverse (constructor y asignación) sigue escribiendo en esos
  canales y su array de punteros vive en el DESTINO; el constructor de copia
  COMPARTE la memoria externa (documentado en JUCE) y `makeCopyOf()` en cambio se
  queda con memoria propia. Los cuatro comportamientos se comparan con JUCE.

**Medido** (MSVC x64 Release, JUCE 8.0.12 real, 2 canales x 512 muestras): 0 ulps
en muestras, magnitud y RMS; frontera de `preallocatedChannelSpace` dsp=31
juce=31; 0 avisos de compilación.

**Control negativo del guard:** el `static_assert` de `DspCoreTests` con el
helper **anterior** (parámetro por valor) no compila — comprobado a propósito en
un fichero aparte —, así que la regresión no puede volver en silencio.

## Renombrado: DSPUtils.h -> DspSafety.h, y las homonimias de la familia (2026-09-19)

Al evaluar la homonimia entre el `DSPUtils.h` de este repo y el de
`ABDSharedCode/SynthCore` (fruto del refactor DRY transversal), la conclusión fue
**renombrar sí, unificar todavía no**. El motivo completo, y las tres condiciones
para unificar, están escritos en la cabecera del fichero renombrado.

- `Source/DSP/DSPUtils.h` -> `Source/DSP/DspSafety.h`: mismo contenido, mismo
  namespace (`NEURONiK::DSP`), mismas firmas. El nombre nuevo describe lo que es
  (validación de parámetros + saneo de NaN/Inf), encaja con la convención `Dsp*`
  de los módulos compartidos (si algún día se muda, no vuelve a renombrarse) y
  elimina el nombre que compartía con `SynthCore/DSPUtils.h`.
- **8 ficheros** actualizan el include (todos los `.cpp` de `Source/DSP` que la
  usaban): `CoreModules/{LFO,FilterBank,Resonator,ResonatorBank,NeuronikEngine,NeurotikEngine}.cpp`
  y `Synthesis/{AdditiveVoice,NeurotikVoice}.cpp`.
- **No se unifica** con SynthCore: no hay código duplicado (aquel no tiene
  validación de parámetros ni saneo de buffers), y estos helpers se apoyan en el
  sustrato (`AudioBuffer`, `dspDbg`, `dspAssert`, `jlimit`), así que mudarlos
  obliga a meter utilidades de producto en DspCore o a que SynthCore dependa de
  DspCore — dos módulos que hoy son independientes y que consume gente distinta.
- `sanitizeAudioBuffer` tenía **0 llamadas** y se **borró**: su política es la que
  el motor no usa (ver el bloque siguiente).

**Verificado:** los 8 `.cpp` sintaxis-limpios con MSVC `/W4` (0 errores, 0 avisos;
los que tiran de JUCE con los includes de módulo reales) y los dos que son
JUCE-free también con GCC `-Wall -Wextra` (0 avisos). Cero referencias de código o
build al nombre antiguo (`grep` en `*.h/*.cpp/*.txt/*.cmake/*.bat`).

**Inventario de homonimias que queda.** Comparando nombres de fichero entre
`ABDNeural/Source/DSP` y los módulos compartidos salen 7 coincidencias, y solo dos
son problemas reales:

| Coincidencia | ¿Problema? |
|---|---|
| `DspCore.h`, `DspDebug.h`, `DspLeakedObjectDetector.h`, `DspMidiBuffer.h`, `DspMidiMessage.h` | **No**: son los shims de este repo. Misma entidad con dos caminos, deliberado y documentado. |
| `DSPUtils.h` | Sí, y queda **resuelto** con este renombrado. |
| `LFO.h` | Sí, **abierto**: `CoreModules/LFO.h` (NEURONiK, `NEURONiK::DSP::Core`, ondas Sine/Triangle/SawUp/SawDown/Square/S&H con Free/TempoSync) y `SynthCore/LFO.h` (el de MS2000/ABDEep, `abd::synth`, ondas MS2000 con `syncNoteIdx` de la spec SysEx) son dos LFO distintos con el mismo nombre de fichero, y en ABDMS2000 hay un tercer `LFO.h` que es shim al de SynthCore. Misma clase de riesgo que el de DSPUtils; el renombrado de uno de los dos (o la convergencia real en un LFO parametrizable) es decisión aparte, porque las dos APIs no son un superconjunto la una de la otra. |

**Y `sanitizeAudioBuffer` se borra, no se conecta (2026-09-19).** Estaba sin usar,
pero el motivo de fondo es otro: **su política es la contraria a la que el motor ya
aplica**. Donde el motor se topa con un NaN de verdad (los lazos de realimentación
de las voces) la respuesta es *detectar y reiniciar la voz*:

```cpp
// AdditiveVoice.cpp / NeurotikVoice.cpp
if (! isfinite (...)) { dspDbg ("NaN ... voice reset"); reset(); return false; }
```

Eso arregla el **estado** que diverge. `sanitizeAudioBuffer` solo escribía 0 en el
buffer de salida: el lazo seguiría roto y produciendo NaN en las muestras
siguientes (silencio sostenido y CPU gastada), además de tapar el síntoma. Ponerlo
en `BaseEngine::applyGlobalFX` sería eso mismo pagando un barrido `isfinite` por
muestra sobre el buffer maestro **en el hilo de audio**. El otro caso real, los
denormales, ya lo cubre `dsp::ScopedNoDenormals` en los lazos. El razonamiento
queda escrito en la cabecera de `DspSafety.h`, junto al de por qué el fichero no se
unifica todavía con SynthCore. Se va con él el `#include <limits>`, que solo él
usaba.

**Verificado:** los 8 `.cpp` que incluyen `DspSafety.h` pasan `cl` `/W4 /O2`
sintaxis-limpios (**0 errores, 0 avisos**), y la cabecera suelta más los dos TUs
JUCE-free (`LFO.cpp`, `FilterBank.cpp`) también con GCC `-Wall -Wextra -Wpedantic`
(**0 avisos**). `grep` de `sanitizeAudioBuffer` en el repo: solo la mención
histórica de este documento.

**De paso, un bug latente del sustrato compartido.** El aviso de GCC
`-Wsizeof-pointer-div` que apareció al escribir el test venía de
`dsp::numElementsInArray` (`DspCore.h`): el port tomaba el array POR VALOR, así que
el array decaía a puntero en la llamada y la función devolvía
`sizeof(Type*) / sizeof(Type)` — 1 en sus tres consumidores, que le pasan
`preallocatedChannelSpace` (un `Type* [32]`). La comprobación `numChannels < 32` se
evaluaba como `numChannels < 1`, de modo que la rama de la memoria PREASIGNADA de
`AudioBuffer` era inalcanzable: `allocateChannels` hacía un `malloc` por buffer
sobre memoria externa (exactamente el que JUCE evita ahí: "blow up things like
Pro-Tools") y el constructor/asignación de movimiento iban siempre por la rama de
aliasar en vez de copiar al hueco propio. Corregido a la forma de JUCE
(`Type (&)[N]`, devuelve N), con ruta testigo en `DspCoreTests`
(`testNumElementsInArray`, con un `static_assert` que no compila si el array vuelve
a decaer). Verificado con MSVC `/W4` y GCC `-Wall -Wextra`: 55 comprobaciones OK y
0 avisos en los dos (el aviso de GCC desaparece).

**Efecto en ABDMS2000:** ninguno. Su `CMakeLists.txt` ya hace `add_subdirectory`
de ABDSharedCode y `DspEffects` es INTERFACE (solo ruta de include), así que esto
es aditivo; y el synth no usa hoy ningún efecto del módulo (ni reverb tiene).
Cuando quiera reutilizarlos, `ABDShared::DspEffects` ya está en el grafo.

---

## Matriz de paridad WASM por sample rate y tamaño de bloque, y el hallazgo que destapa (2026-09-19)

Cierra el último punto sin verificar de la Fase 5. Antes la paridad se medía en **una
sola** pareja (48 kHz / 128), que no dice nada sobre los otros sample rates ni sobre
el buffer que use el host.

**Qué se hizo**

- `Tests/WasmParityTest.cpp` pasa de un caso a una matriz de **9**: 44.1/48/96 kHz ×
  64/128/512 muestras de bloque. La clave es que la duración de cada escenario es la
  MISMA en los nueve — el número de bloques se reescala sobre una referencia de 128
  (32 → 64 bloques con bloque 64, → 8 con bloque 512) y el pánico se reescala igual —
  así que la comparación es de contenido musical y no de número de llamadas.
- El JSON pasa a `{"referenceBlockSize":128, "cases":[{sampleRate, blockSize,
  scenarios:[...]}], "blockSizeInvariance":[...], "blockSizeDependentScenarios":[...]}`.
- `Tests/neuronik_wasm_parity.mjs` recorre la matriz entera: reinicializa el módulo con
  cada pareja (`_neuronikInit(sampleRate, blockSize)`) y compara cada caso contra su
  propia referencia. El calendario (`blocks`, `panicAtBlock`) **se lee de la
  referencia**, no se duplica: era la forma más fácil de comparar dos cosas distintas
  sin darse cuenta. 9 casos × 5 escenarios = 45 comparaciones / 184.320 muestras.

**El hallazgo: la salida depende del tamaño de bloque en dos rutas**

El test mide también, en nativo, si los tres tamaños de bloque dan la misma señal
(misma duración ⇒ deberían ser bit-exactos si el DSP es por muestra):

| Escenario | 44.1 kHz | 48 kHz | 96 kHz |
|---|---|---|---|
| A_neuronik_default | **bit-exacto** (0/8192) | **bit-exacto** | **bit-exacto** |
| B_neurotik_default | **bit-exacto** (0/8192) | **bit-exacto** | **bit-exacto** |
| E_modelo_espectral | **bit-exacto** (0/6144) | **bit-exacto** | **bit-exacto** |
| C_fx_panico | depende (maxAbs 2.7e-1) | depende (3.0e-1) | depende (2.6e-1) |
| D_modmatrix | depende (maxAbs 8.7e-3) | depende (8.7e-3) | depende (3.1e-3) |

Las dos causas, localizadas:

1. **Modulación por bloque** — `BaseEngine::applyGlobalFX` llama a
   `lfo1.processBlock(numSamples)` / `lfo2.processBlock(numSamples)`, y
   `LFO::processBlock` devuelve **un** valor por llamada (avanza la fase `increment *
   (numSamples - 1)` y lo mantiene). La matriz de modulación es entonces una escalera
   cuyo paso es el tamaño de bloque. Coherente con lo medido: en D la primera
   diferencia cae EXACTAMENTE en el primer límite de bloque (índice 128 comparando
   bloque 64 contra 128; índice 256 comparando 512 contra 128).
2. **Smoothers de la reverb por bloque** — `Source/DSP/Effects/Reverb.h::processBlock`
   hace `sizeSmoother.getNextValue()` **una vez por llamada**, fuera del bucle de
   muestras, y con ese valor llama a `reverb.setParameters()`. Su rampa de 20 ms dura
   20 ms *por bloque*, no 20 ms de audio. Coherente con lo medido: en C nada diverge
   hasta ~448 muestras (44.1/48 kHz) o ~960 (96 kHz), o sea hasta que termina la
   primera rampa. La rampa de la reverb se comporta así desde antes de la migración a
   `DspEffects` (el comentario del fichero lo decía: "update parameters once per block")
   — el test de paridad de la migración no podía verlo porque comparaba una sola
   pareja de bloques.

El **núcleo** (osciladores, resonador, envolventes, filtros, voz aditiva y Neurotik)
es bit-exacto en los tres tamaños: eso es lo que hacía falta saber para la web, y sale
bien.

**Por qué importa para la web.** El `AudioWorklet` renderiza siempre en cuantos de
128 muestras y un host nativo suele ir a 256/512/1024. Para C y D, web y nativo no dan
la misma señal. Arreglarlo es pequeño (LFO por muestra manteniendo el valor del último
bloque no sirve: hay que avanzar y devolver por muestra, y el wrapper de la reverb
tiene que mover cada smoother dentro del bucle), pero **cambia la salida** de los
presets con FX y de los que usan la matriz de modulación — es una decisión de producto,
no un refactor. Queda como decisión abierta en `ROADMAP.md` (Fase 5) con esta misma
evidencia. Nota para el que lo coja: `dsp::LinearSmoothedValue::setTargetValue` **sí**
hace early-return con el mismo target (port fiel de JUCE), así que el problema NO es
rearmar la rampa, es llamar a `getNextValue()` fuera del bucle de muestras.

**Decisión sobre las dos rutas (2026-09-19): se arreglan las dos, y el motivo principal del
primero no es el bloque.**

- **Reverb → es un defecto.** Sus cuatro smoothers avanzan *una vez por bloque*, y la rampa
  está declarada como 20 ms (`reset(sampleRate, 0.02)` = 882 pasos). Con una llamada por
  bloque la rampa dura 882 **bloques**: ~2,5 s con bloque 128 y **~10 s con bloque 512** a
  44,1 kHz. O sea que al cargar un preset la reverb no llega a su valor en 20 ms, se arrastra
  segundos, y cuánto dura depende del buffer del host. Revisados los cuatro envoltorios:
  chorus, delay y saturación avanzan por muestra; la reverb es **la única** con este patrón.
  Arreglo: consumir `numSamples` pasos por bloque y rearmar `setParameters()` solo cuando el
  valor suavizado cambie (durante la rampa), no en cada bloque con el mismo valor.
- **Modulación → no es un defecto, es una tasa de control acoplada al host.** El LFO se lee
  una vez por bloque (`applyGlobalFX` → `lfo.processBlock(numSamples)` → `applyModulation()`),
  así que la matriz modula por bloque y su granularidad es la del host (128 en la web, 512 en
  un host de 512). Arreglo: tasa de control **fija** (bloques internos de 64/128 muestras con
  el resto encadenado), de modo que la modulación la defina el tiempo y no el troceado. Eso
  cambia la modulación en hosts de bloque grande (menos escalón) y obliga a **re-basar** la
  referencia de paridad.

Los dos van en pasos separados, con la matriz de 9 casos como verificación: al final,
`blockSizeDependentScenarios` debe quedar vacía o con una justificación escrita de lo que
quede.

**Reproducción** (no necesita emsdk, es el lado nativo):

```
cl /nologo /std:c++17 /O2 /W4 /EHsc /DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 \
   /I Source /I Source/DSP /I Source/Common /I ../ABDSharedCode /I C:/JUCE/modules \
   Tests/WasmParityTest.cpp <las 12 fuentes de DspSources.cmake> \
   C:/JUCE/modules/juce_core/juce_core.cpp C:/JUCE/modules/juce_core/juce_core_CompilationTime.cpp \
   shell32.lib ole32.lib oleaut32.lib shlwapi.lib user32.lib advapi32.lib
neuronik_parity.exe parity-native.json      # imprime la matriz y el veredicto por rate
```

(El target CMake `NEURONiK_WasmParityTest` sigue siendo el camino oficial; esto es
solo la vía corta para mirarlo sin compilar el plugin.) El generador **no falla** por
la dependencia de bloque: la publica en el JSON y avisa por consola, porque es una
propiedad del motor y no del puente WASM. El árbitro duro sigue siendo el 0 ulps por
caso.

**De paso (mismo día):**

- `Tests/ParameterBridgeTest.cpp`: los literales `0.6` / `0.75` / `1.0` pasan a `f`.
  El aviso `C4305` de MSVC solo saltaba con `0.6` (es el único de los tres que no es
  exactamente representable en float, y MSVC calla cuando lo es), pero los tres son el
  mismo caso y el fichero ya usaba `0.8f`/`0.5f` doscientas líneas más arriba.
- `WasmParityTest.cpp` decía "4 escenarios" en su cabecera y hay cinco desde
  `E_modelo_espectral`; corregido (y el comentario de CMake del target, igual).
- **`oscPitchCoarse` retirado del namespace de IDs** (no implementado). Estaba
  declarado desde el primer día pero nunca entró en el layout: no lo leía el motor, ni
  el panel, ni ningún preset. Era una promesa del draft viejo `DOC/3`
  (`oscPitchFine`, `oscPitchOctave`, `oscHarmonicCount`, ninguno adoptado tampoco).
  Misma regla que `harmMix` en 2026-09-16. Lo que sí existe es el pitch por voz (MPE:
  `EventType::PitchBend` → `IVoice::notePitchBend(semitonos)` → `pow(2, semis/12)`); un
  coarse tune global sería otra cosa y, si se quiere, es una feature con su tarea (se
  hace en el host transponiendo las notas, junto a `velocityCurve`/`midiChannel`, sin
  tocar motor ni ABI WASM). Consecuencia: `getUnroutedParameterIds()` queda vacío,
  `notInLayout` pasa a 0 en el contrato generado, y el test de regresión ahora exige
  que la lista esté vacía (si alguien declara un ID fuera del layout, salta). El
  mecanismo se queda: es lo que hace visible una divergencia nueva en vez de dejarla
  invisible. Detalle en `DSP_PARAMETERS.md`.

## Arreglo de la rampa de la reverb: duraba 960 BLOQUES, no 20 ms (2026-09-19)

Primer paso de la decisión "las dos rutas por bloque se arreglan". El envoltorio
`Source/DSP/Effects/Reverb.h` hacía `getNextValue()` **una vez por llamada**, fuera del
bucle de muestras. Su rampa está declarada como 20 ms (`reset(sampleRate, 0.02)`, 960
pasos a 48 kHz), así que en la práctica la subida no duraba 20 ms: duraba 960 **bloques**
— ~2,5 s con bloque 128 y ~10 s con 512 — y el tiempo lo imponía el buffer del host. Al
cargar un preset con reverb, el efecto se arrastraba durante segundos.

**Qué se cambió** (un fichero, `Effects/Reverb.h::processBlock`):

- El parámetro se lee y se aplica **por muestra**: bucle de una muestra contra
  `dsp::Reverb` (`processStereo(left+i, right+i, 1)`), porque `dsp::Reverb` solo expone API
  por bloque. Eso es lo que convierte la rampa en una función del tiempo y no del troceado.
- **Camino rápido** (el habitual): si ninguno de los cuatro smoothers está rampeando
  (`isSmoothing()` falso), se aplica el valor actual una vez y la reverb procesa el bloque
  entero de un golpe. Es el mismo recorrido que el bucle (el port ya itera por muestra por
  dentro), con una sola llamada: el modo por muestra se paga solo los ~20 ms siguientes a
  un cambio de parámetro.
- `applyParameters()` rearma `setParameters()` **solo si el valor cambió** (cuatro
  comparaciones): `setParameters` llama a `updateDamping()` y no queremos eso por muestra.
- Reverb apagada (target y valor actual ≤ 0,002): se sigue saltando el bloque —es
  equivalente, con wet 0 la señal no se toca— pero los cuatro smoothers **avanzan**
  (`skip(numSamples)`), así que encenderla más tarde arranca la rampa donde le toca por
  tiempo, no donde se quedó el bloque anterior. Con objetivo > 0 nunca se salta: la reverb
  procesa desde la primera muestra.

**Verificación** (target `NEURONiK_WasmParityTest` en `build-reference`, `/W4`, 0 avisos):

| Escenario | 64 vs 128 vs 512, a 44,1 / 48 / 96 kHz |
|---|---|
| A, B, E (núcleo) | bit-exacto (0/8192, 0/6144) — sin cambios |
| **C_fx_panico** | **bit-exacto en las 9 celdas (0/12288, maxAbs 0,0)** |
| D_modmatrix | sigue dependiendo (maxAbs 8,7e-3 a 44,1/48 kHz, 3,1e-3 a 96 kHz) |

`blockSizeDependentScenarios` pasa de `["C_fx_panico", "D_modmatrix"]` a
`["D_modmatrix"]`. Y contra el volcado del código anterior (mismo 48 kHz/128): A, B, D y E
**bit-idénticos** — el camino de reverb apagada no se tocó — mientras C difiere en todas
las muestras con maxAbs 8,9e-2 (peak 0,528966 → 0,519034). Diferir es el objetivo: al
acabar el render anterior su wet seguía al ~5% del objetivo (48 de los 960 pasos de la
rampa) y ahora llega a su valor en 20 ms, o sea que el preset con reverb suena como debe
sonar desde el primer bloque.

**De paso:** `WasmParityTest.cpp` usa `fopen`/`fprintf` (deliberado: volcados de MB con
formato `%.9g` que tiene que coincidir con el parser del `.mjs`) y MSVC avisaba `C4996`.
Se define `_CRT_SECURE_NO_WARNINGS` al principio del fichero con el motivo escrito: es un
generador host-only, no entra en el módulo WASM.

**Sigue abierto:** la modulación (`lfo.processBlock` una vez por bloque, escenario D). El
arreglo acordado es la tasa de control fija de 64/128 muestras, y ese sí obliga a
**re-basar** la referencia de paridad.

**Nota de contexto, no tocada:** el mapeo del envoltorio (`wetLevel = mix*0.5`,
`dryLevel = 1 - mix*0.2`) va sobre la escala interna del port, que es la de JUCE
(`dryScaleFactor = 2.0`): ese `dryLevel` da una ganancia seca de **1,6 a 2,0**, o sea un
boost de +4 a +6 dB de la señal seca cuando la reverb está activa (en esa escala, la
unidad está en `dryLevel = 0,5`). Es anterior a este arreglo (lo que cambia es cuándo se
aplica, no la fórmula) y decidir si se re-mapea es producto, no un bug del arreglo.

## Tasa de control fija del motor: 64 muestras (2026-09-19)

Segundo paso de la decisión sobre las dos rutas por bloque. El LFO se leía **una vez por
bloque del host** y la matriz de modulación se aplicaba con ese único valor, así que la
modulación era una escalera de paso = `blockSize`: 128 en el `AudioWorklet`, 512 en un
host de 512. Ahora la rejilla la fija el tiempo, no el buffer.

**Qué se cambió**

- `BaseEngine::kControlBlockSize = 64` + `controlCarry`: el motor renderiza en tramos de 64
  muestras DE AUDIO, con el sobrante **encadenado entre bloques del host** (un host de 96
  da 64+32, y el bloque siguiente empieza cerrando los 32 que faltaban). Así la rejilla no
  se desplaza según el troceado.
- `BaseEngine::renderVoicesWithControlRate(buffer)`: por tramo → avanza los LFOs, aplica la
  matriz (`applyModulation()`, ahora virtual pura en `BaseEngine`) y solo entonces renderiza
  las voces de ese tramo. Los dos motores (`NeuronikEngine`, `NeurotikEngine`) lo llaman en
  lugar de su bucle de voces, y `applyGlobalFX()` deja de leer los LFOs (sigue con FX y
  master por bloque, que ya eran por muestra por dentro).
- `LFO::processBlock` avanza la fase **por muestra** en vez de multiplicarla de golpe
  (`phase_ += increment * (numSamples - 1)`): con troceado, multiplicar redondea distinto y
  la fase del LFO dependería del troceado. De paso desaparece la segunda implementación del
  Sample & Hold que vivía en `processBlock`: avanzaba la interpolación **dos veces por
  muestra** (una en su propio bucle y otra dentro de `generateRandomSampleAndHold`), y el
  doble avance dependía del número de llamadas, o sea que el ruido del S&H también cambiaba
  con el buffer del host. Queda un solo camino, por muestra, y el S&H avanza una vez.
- Por qué 64 y no 128: el LFO llega a 20 Hz (`lfo1RateHz`/`lfo2RateHz`) y a 48 kHz una
  rejilla de 64 muestras son ~37 escalones por ciclo frente a ~19 con 128. Además 64 es
  múltiplo del `kSubBlockSize = 32` de las voces, así que el troceado no crea fronteras
  nuevas dentro de sus sub-bloques. Coste: la voz se llama 2 veces por cuanto en la web y 8
  en un host de 512 (cada llamada reconfigura su resonador desde el modelo espectral, que
  es el trabajo extra), y solo eso.

**Verificación** (target `NEURONiK_WasmParityTest`, `/W4`, 0 avisos)

| Escenario | 44,1 / 48 / 96 kHz × 64/128/512 |
|---|---|
| A, B, C, D, E | **bit-exacto en las 15 celdas** (0 diferencias) |

`blockSizeDependentScenarios` pasa de `["D_modmatrix"]` a `[]`. Y para aislar el cambio,
contra el volcado del paso anterior (reverb ya arreglada): A, B, C y E son **bit-idénticos**
(el camino sin modulación activa no se toca) y D cambia desde la muestra **64** exacta
(maxAbs 2,1e-3 a 48 kHz/128; peak 0,553536 → 0,555390), que es justo la primera frontera de
la rejilla nueva.

**Artefacto pendiente de regenerar:** el volcado `build-wasm/parity-native.json` lo regenera
`build_wasm.bat` y no está en git, pero el `.wasm`/`.js` de `WebPilot/public/worklet/` **sí
está trackeado** y es anterior a los dos arreglos (reverb y tasa de control). Hay que
ejecutar `build_wasm.bat` para regenerarlo, o el test Node comparará un módulo viejo contra
una referencia nativa nueva. No se ha hecho aquí porque necesita emsdk.

## Matematica determinista en el sustrato: sin/atan sin libm (2026-09-19)

Al regenerar el `.wasm` pendiente, la matriz de paridad destapó una diferencia de **10 ulp
en UNA muestra** del escenario C a 44,1 kHz (1/6144; `maxDiff` 2,98e-7, por debajo del guard
absoluto de 1e-6), igual en los tres tamaños de bloque. 48 y 96 kHz seguían bit-exactos, y
nativo y WASM eran cada uno invariante al tamaño de bloque: la diferencia era de toolchain,
no del motor.

**Causa, medida (no supuesta).** Mismo binario compilado con MSVC y con em++ sobre un barrido
de 400.000 argumentos: `sinf` difiere en 611/400.000 valores (1 ulp) y `atanf` en
24.694/400.000. En el lazo del chorus/delay ese 1 ulp se amplifica por encima del presupuesto
de 0 ulps. Se descartó la contracción FMA: `-ffp-contract=off` a solas no cambiaba el
resultado.

**Decisión (canónica, la misma que se tomó con `JUCE_UNDENORMALISE`):** uniformar la
aritmética en vez de relajar el gate. Nuevo `ABDSharedCode/DspCore/DspMath.h` (módulo
DspCore): `abd::dsp::sin/cos/atan` calculados SOLO con operaciones IEEE básicas (+ - * /),
idénticos en los dos toolchains. `DspChorus` y `DspSaturation` los usan. El target WASM
compila con `-ffp-contract=off` (clang podría fusionar `a*b+c` en `fmaf`; MSVC `/O2` no
contrae): es parte del contrato de determinismo del módulo, no un extra.

- Precisión: ~6 ulp de `sin` y ~3 de `atan` frente a la libm (5,2e-7 / 1,8e-7 absolutos),
  inaudible para un LFO y una saturación.
- Determinismo verificado en aislamiento: el mismo barrido compilado con MSVC y con em++ da
  salidas **idénticas bit a bit** (800.001 valores).
- **Aviso:** los valores NO son los de la libm, así que es un cambio de sonido deliberado (el
  motor es pre-1.0). `DspEffectsParityTest` actualiza sus `Reference*` a la misma matemática;
  ese test sigue probando el envoltorio de producto y el determinismo, no el cambio de libm
  (sería circular).
- Tests: `DspCoreTests` gana un bloque de DspMath (valores conocidos + precisión vs libm).

Verificación de esta pasada: `build_wasm.bat` (paridad 15/15 esperada) y `build.bat` (suite).

**Integración en el build general (2026-09-19).** El WASM vivía fuera de `build.bat`, así que el
worklet de `WebPilot/public/worklet/` podía quedarse en una pasada anterior (drift invisible:
los `.js`/`.wasm` se versionan y solo se nota como audio viejo). Ahora `build.bat` lo compila
como **paso [4/9]**, ANTES de exportar la WebUI (que copia `public/` -> `out/`) y de compilar el
host (que embebe `out/`): `call build_wasm.bat --internal-log nopause` (sin anidar su tee ni
quedarse en su pausa). Un fallo de compilación/paridad WASM **aborta el build**; se omite a
propósito con `build.bat nowasm` (y en el modo rápido `build.bat tests`).

`build_wasm.bat` gana además lo que ya tenía `build.bat`: **log espejo `wasm-last-run.log`** y
**pausa final** (saltable con `build_wasm.bat nopause`), para poder leer el resultado sin
prisa y para que el log quede en disco.

También `build.bat` comprueba por tamaño que `WebPilot\out\worklet\neuronik_dsp.wasm` coincide
con `build-wasm\neuronik_dsp.wasm` antes de compilar el host: si Vite no copió `public/` a
`out/`, el worklet embebido sería un DSP viejo y sonaría a la pasada anterior (síntoma mudo),
así que el build aborta. (La copia `build-wasm` -> `public/worklet` la hace `sync-wasm.mjs`,
paso 6/6 de `build_wasm.bat`; es la misma pieza que en ABDMS2000 copia el `.wasm` a las
carpetas de la versión web.)

---

## Andamiaje vainilla de la WebUI (Fase 8, 2026-09-19)

Se ejecuta la decisión de stack que quedó escrita en `ROADMAP.md` (Fase 8): la interfaz de
NEURONiK es **web sobre WebView2 con JS vainilla**, no React. Nace `ABDNeural/WebUI/`.

**Carpeta propia.** `ABDMS2000/WebUI` se usa solo como referencia de arquitectura (un módulo
de puente, uno de contrato, uno de UI); no se comparte código con ese proyecto.

**Portado del piloto, sin cambios de comportamiento** (es JS sin framework; lo que muere con
el piloto es su armazón React `app/page.jsx` + `lib/controls.jsx`):

| WebUI | Origen |
|---|---|
| `src/bridge/bridgeCore.js` | `WebPilot/lib/bridge.js` — transporte del bridge WebView2 |
| `src/contracts/parameters.js` | `WebPilot/lib/parameters.js` — adaptador del contrato generado |
| `src/contracts/paramValue.js` | `WebPilot/lib/paramValue.js` — normalizado ↔ unidades reales |
| `src/wasm/audioParams.js` | `WebPilot/lib/audioParams.js` — contrato → `GlobalParams` del worklet |
| `src/contracts/paramStore.js` | `WebPilot/lib/useParameterControls.js` — el pegamento del hook, ahora store vainilla |

`paramStore.js` es el único port con traducción: el hook guardaba el estado en `useState` y
devolvía handlers memoizados; el store expone un objeto de estado inmutable, `getState()`,
`subscribe()` (llama al oyente de inmediato y en cada cambio, y devuelve un `unsubscribe`) y los
mismos handlers (`pushParameter`, `handleChange`, `handleGesture`, presets, MIDI, modelos).
Mantiene a propósito los handles que el host ya usa: `window.__pilotReady` y
`window.__pilotSendMidi`.

**El contrato no se copia.** `src/contracts/parameters.js` importa
`WebPilot/generated/parameters.generated.js`, que sigue siendo la única copia (la escribe
`NEURONiK_ParameterExport`, paso 2/9 de `build.bat`). Cuando el piloto se retire (8.4) ese
directorio se muda a `WebUI/` y el import es de una línea. El `vite.config.js` abre su
`server.fs.allow` para poder leerlo (igual que el piloto hace con `ABDSharedAssets/tests`).

**Suite propia: 61 tests en 6 ficheros** (`cd WebUI && pnpm test`): los cuatro del piloto
portados (`bridgeCore`, `paramValue`, `parametersState`, `audioParams`), los de integración del
ex-hook (`paramStore`, incluidos gestos, snapshot, presets, MIDI y `subscribe`) y
`appContract.test.js`, que vigila contra el código fuente que el control base siga siendo un
`<input type="range">` de `masterLevel` (lo que conduce el `--selftest` del host) y que no haya
React en la entrada. El piloto sigue en **56/56**.

**Bundle, para el objetivo de 8.5:** `WebUI/dist` sale en **32,4 KB de JS (6,3 KB gzip)** frente
a los 306 KB (89 KB gzip) del piloto React, con las mismas dependencias compartidas. Es la
medición que sostiene "un bundle, un motor de UI".

**Workspace:** `WebUI` es miembro del workspace pnpm anidado de `WebPilot`
(`ABDNeural/WebPilot/pnpm-workspace.yaml`), como ya lo era `WebPilotVite` — así
`@abdsynths/shared` se resuelve zero-copy y un solo `pnpm install` cubre las tres piezas.

**Lo que NO se toca (a propósito):** `build.bat` sigue exportando `WebPilotVite` a
`WebPilot/out`, que es lo que embebe el host del piloto y lo que sirve `start.bat`. `WebUI`
compila a `WebUI/dist` y queda **fuera de ese circuito**: cambiar el motor de UI es un paso
deliberado de 8.2 (paridad de control), no un efecto colateral de crear la carpeta. Tampoco se
migran todavía las pestañas nativas, el LCD/D-pad, el navegador de presets con tags, el MIDI
Learn ni los visualizadores: eso es 8.2 y 8.3, y la lista está en el inventario 8.0 del ROADMAP.

**Dentro del plugin el audio es nativo** (8.1): la página habla por el bridge (APVTS) y el
motor WASM del worklet (`src/wasm/`, ya cubierto por `neuronik_wasm_parity.mjs`) es para la
página **fuera** del plugin. Dos motores sonando no es un caso soportado.

---

## Shell vainilla de la UI (base de 8.2, 2026-09-19) — hecha antes de 8.1 a propósito

Sobre el andamiaje anterior se montó la primera UI de verdad, **sin cablear a nada**: el
host, `build.bat`, `start.bat` y CMake siguen sirviendo el piloto React (`WebPilot/out`).
Esta shell compila a `WebUI/dist`, que hoy no consume nadie.

**Por qué en este orden (y qué me faltó a mí).** Empecé cableando la bancada del piloto
(`NEURONiK Web Pilot.exe`) a la UI nueva y el usuario lo paró con razón: **8.1 es "el
*editor del plugin* hospeda la página"**, y su DoD son Standalone y VST3 con el selftest de
4 direcciones — nada de eso se toca moviendo el *host del piloto* de carpeta. Y la pantalla
que estaba montando es 8.2 (paridad de control), que estaba ajustando a los selectores del
selftest en vez de al panel nativo. Se cerró la shell primero porque es **agnóstica de quién
la hospeda** (hospedar la página es ResourceProvider + adaptadores, los mismos con cualquier
página), así que no se tira: **8.1 es el siguiente paso**.

**Qué hay ahora en `WebUI/src/`:**

| Fichero | Qué es |
|---|---|
| `contracts/screens.js` | Los ids de cada pantalla, como datos (BRIDGE, GENERAL, KEYS), todos del contrato generado. |
| `ui/panel.js` | Shell: pestañas, el **control base** (`masterLevel`) y GENERAL con los 11 ids y su valor real. **Sin widgets** (los de la familia compartida son 8.2); la propia UI lo dice. |
| `ui/keyboard.js` | El teclado compartido (`@abdsynths/midi-keyb`) con la API de feedback del host (vía silenciosa: `setModWheel`/`setPitchBend` no re-disparan callbacks de usuario). |
| `app.js` | Arranque. El orden importa: el panel se monta **antes** de `store.start()` (el host mide "panel in DOM" y "page ready" por separado). |

**El contrato con el host, pinchado en tests** (esto es lo que evita que el E2E falle solo
dentro de WebView2, minutos después):

| Selector / handle | Lo lee | Test |
|---|---|---|
| el PRIMER `input[type=range]` = `#masterLevel` | NATIVE→JS y JS→NATIVE | `panel.test.js`, `keyboard.test.js` |
| `footer.panel-footer code` (JSON normalizado) | GENERAL (11 ids) | `panel.test.js` |
| `[data-tab="keys"]`, `#mod-wheel-container .kbd-wheel-slider` | MIDI | `keyboard.test.js` |
| `window.__pilotReady`, `window.__pilotSendMidi` | métricas y MIDI | `paramStore.test.js` |

El frágil es el primero: **el teclado también monta inputs `type=range`** (las ruedas), así
que el orden de las pantallas (BRIDGE antes que KEYS) es lo que mantiene el slider del
control base en cabeza. Reordenar las pestañas rompe el selftest, y hay test.

**Suite: 81 tests en 9 ficheros** (96 hoy, con la política de audio de 8.1) (`cd WebUI && pnpm test`), incluidos los que montan el
teclado compartido en jsdom de verdad (no una maqueta) y comprueban que `setMidiState`
mueve la rueda a 64 sin devolver el eco como input de usuario. Bundle: **62,4 KB de JS
(15,6 KB gzip)**.

**Lo que NO se hizo, para que no se confunda con hecho:** la paridad de 8.2 (widgets de la
familia compartida, reparto por las seis pestañas del panel nativo, envolvente dibujada,
matriz de modulación, `dspStatus` visible) y por supuesto 8.1. La lista completa sigue en el
inventario 8.0.

---

## Politica de audio fijada en codigo + motor del worklet portado (8.1, 2026-09-19)

Primer bullet de 8.1 cerrado; los otros dos (el editor hospeda la pagina, tamano/zoom) siguen
abiertos, asi que **8.1 no esta terminada**.

**La regla.** NEURONiK embarca el mismo DSP dos veces: el motor nativo del plugin y el modulo
WASM que corre en un AudioWorklet. Los dos sonando a la vez no es un caso soportado (voces
dobles, FX con fase rara) y nada en el protocolo lo impedia. Ahora:

- **Una sola senal**: `window.__JUCE__`, la MISMA que usa el puente. `bridgeCore.js` exporta
  `nativeBackend()` y `WebUI/src/audio/policy.js` lo consume; dos detecciones distintas habrian
  sido dos formas de equivocarse. La politica no puede discrepar del estado del bridge.
- **Una sola puerta**: `startAudioEngine()` (el motor portado) consulta la guarda ANTES de
  nada. Dentro de un host devuelve estado `blocked` con el motivo y **no llega a construir un
  `AudioContext`**; hay test que lo vigila espiando el constructor, que es la unica forma de
  estar seguro de que no hay segundo motor.
- **La pagina que el host sirve HOY tambien la cumple.** El piloto React es el que esta al otro
  lado del WebView2 mientras 8.1 no este, asi que su `audioControl()` recibe `bridgeAvailable`:
  dentro del host pinta `AUDIO: NATIVO` y no ofrece SOUND ON. Esa guarda desaparece con el
  piloto (8.4); el espejo definitivo es `WebUI/src/audio/policy.js`.

**El motor portado** (`WebUI/src/audio/audioWorkletEngine.js`, port de
`WebPilot/lib/audioWorkletEngine.js`) es el mismo ciclo de vida — AudioContext, worklet node,
handshake `neuronik:ready`, mensajes `params`/`engine`/`models`/`midi`/`panic`, teardown —
con dos cambios: la guarda de politica y un estado `blocked` propio (no es un error: es una
respuesta). El `.wasm` cruza por `processorOptions` (el unico canal que funciona desde un
export estatico) y los artefactos ya viajan solos: `publicDir` de Vite apunta a
`WebPilot/public`, asi que `dist/worklet/` sale con el procesador y el `.wasm`.

En la shell, la linea de audio es **letrero o control, nunca las dos**: dentro del host no hay
boton que pulsar, y en el navegador `SOUND ON` arranca el motor y el estado se empuja al
worklet por UN camino (`syncEngine`), el mismo para ediciones de la pagina y snapshots nativos.
Los mensajes MIDI del teclado van por las dos vias (bridge y worklet) porque solo una puede
estar viva segun la politica.

**Tests:** WebUI **96 en 11 ficheros** (nuevos: `policy.test.js`, `audioEngine.test.js`, y el
control de audio en `panel.test.js`), piloto **57 en 6** (con el nuevo guardian de fuente en
`pageContract.test.js`). Bundle: 68,0 KB de JS (17,7 KB gzip).

**Lo que queda abierto de este bullet:** la comprobacion **E2E**. El `--selftest` corre contra
el *host del piloto*, no contra el plugin, asi que la politica esta probada en unitario y en
la pagina, pero no dentro del WebView2 del plugin. Se cierra en 8.1 cuando el editor hospede
la pagina (la via barata entonces: que el selftest del editor lea un handle de la pagina, como
ya hace con `__pilotReady` y `__pilotSendMidi`).

---

## 8.1, paso 1: los adaptadores del bridge dejan de ser del banco de pruebas (2026-09-19)

Los tres adaptadores (`PresetManagerAdapter`, `MidiInjectionAdapter`, `EngineModelsAdapter`)
vivian dentro de `Source/WebPilotHost.cpp` — y **ese era el motivo real de que solo la bancada
del piloto pudiera hablar con la pagina**: no habia forma de reutilizarlos sin copiarlos.

Ahora estan en `Source/WebUI/BridgeAdapters.h`, en `namespace NEURONiK::WebUI`:

- **Header-only a proposito**: no llevan mas estado que una referencia al procesador (que
  sobrevive al bridge en cualquier host), asi que no necesitan `.cpp` ni fuente nueva en ningun
  target. Se incluyen y ya.
- El host del piloto pasa a `#include "WebUI/BridgeAdapters.h"` + tres `using`, y **ningun uso
  cambia** (los 9 sitios: `make_unique`, miembros y declaraciones se quedan igual).
- Comprobado a mano contra `WebUI/ParameterBridge.h`: las tres interfaces se implementan
  literalmente (`PresetController`: list/load/save/current; `MidiController`: noteOn/noteOff/
  pitchBend/modWheel/allNotesOff; `NativeModelController`: getNumModelSlots/getCurrentModel).
- El movimiento es **verbatim**: unico cambio, quitar la calificacion `NEURONiK::WebUI::` de
  los nombres base, que ahora son locales al namespace.

**Sin cambio de comportamiento esperado**: la bancada sigue sirviendo la pagina y el selftest
igual que antes; lo unico que cambia es donde vive el codigo. **Pendiente de compilar** (este
paso no lo he podido verificar yo).

Paso 2 de 8.1, que es el que falta: `ResourceProvider` compartido (disco en dev + snapshot
embebido), el `WebBrowserComponent` dentro de `NEURONiKEditor` (tamano/zoom sin romper
`resized()`), y llevar el selftest de cuatro direcciones a **Standalone y VST3** — hoy ese
arnes solo existe en `WebPilotHost.cpp`.
