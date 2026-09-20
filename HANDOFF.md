# NEURONiK Handoff

> **Este documento es un registro corrido (append-only):** las entradas nuevas se añaden al final,
> fechadas. Las secciones temáticas de la primera mitad reflejan el arranque de la migración
> (**2026-09-16**) y varias quedaron superadas por decisiones posteriores —las marcadas como
> «histórico» y las que ya tienen entrada propia más abajo—. Estado vigente: la última entrada de
> este documento y la **Fase 8** de `ROADMAP.md`.

## Estado de la instrumentación del arranque (bancada WebView2)

La bancada `NEURONiK_WebPilotHost` sirve `WebUI/dist` —la misma página que embebe el plugin— y
mide su arranque real en un log. Es lo único que queda con nombre del piloto: el target, su exe y
su `.cpp` conservan el nombre porque renombrarlos toca CMake, `build.bat`, `start.bat` y dos
tests, y no era parte de este encargo (ver la sección de la retirada, al final).

```bash
cd ABDNeural
"./build-reference/NEURONiK_WebPilotHost_artefacts/Release/NEURONiK Web Pilot.exe" --auto-quit
```

- `--auto-quit` cierra el host en cuanto el panel está listo, para poder medir sin interacción.
- Salida: `pilot-startup.log` junto al ejecutable (se añade un bloque por ejecución) y stdout.
- Qué mide: construcción del backend WebView2, `document.readyState` interactivo, panel en el DOM,
  `window.__pilotReady` (lo publica el store de la página al terminar de aplicarse el snapshot),
  recursos servidos con sus bytes y rutas no resueltas.
- El marcador `window.__pilotReady` lo publica `WebUI/src/contracts/paramStore.js` (lo pone
  `WebUI/src/app.js` en marcha). Los nombres "panel in DOM" y "react ready" del log son del piloto:
  la métrica es la misma y cambió el dueño del marcador.

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

Fecha de apertura de este handoff: **2026-09-16** (crece por entradas fechadas; la última está al final).

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

1. **Bancada WebView2** — lanza "NEURONiK Web Pilot.exe" (bridge bidireccional con el
   plugin; es la web "de verdad", como el Vite 8384 en ABDMS2000). Ése es el nombre del exe
   porque el target conserva el del piloto: retirado el piloto, esto es la bancada de la WebUI.
2. **Solo WebUI en navegador** — sirve `WebUI/dist` en `http://localhost:8399` con
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

## Próximo trabajo recomendado (histórico, 2026-09-16)

> Superada por las entradas fechadas posteriores: el bridge existe y está verificado en las dos
> direcciones, el piloto se creó y luego se retiró, Vite ganó el A/B y el plan vigente es la
> **Fase 8** de `ROADMAP.md`.

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

- la raíz de la página se localiza por ruta de compilación (`NEURONiK_WEBUI_DIR`) y, si falla,
  buscando `WebUI/dist` hacia arriba desde el ejecutable y desde el directorio de trabajo;
  `NEURONIK_WEBUI_DEV_DIR` la puede apuntar a cualquier exportación (mismo override que el plugin);
- CMake **ya no falla** en configure por esta carpeta: hasta la retirada del piloto exigía
  `WebPilot/out/index.html` (y llevaba su export embebido, `NEURONiK_WebPilotAssets`); hoy la
  bancada sirve `WebUI/dist` desde disco y no embebe página;
- si el documento no se encuentra, se sirve una página de diagnóstico legible en lugar de una ventana en blanco.

Nota operativa: el `.exe` del host queda bloqueado mientras la ventana está abierta, así que hay que
cerrarla antes de recompilar.

## Bridge de parámetros JUCE <-> WebUI (2026-09-16)

El host del piloto ya no es solo un visor: refleja un APVTS real en ambos sentidos por el canal de
eventos de JUCE 8 y lleva una tira nativa de comparación.

```text
Source/WebUI/ParameterBridge.{h,cpp}   protocolo bidireccional (sin WebView2: testeable con una lambda)
Source/WebPilotHost.cpp                transporte + APVTS real (createLayoutApvts) + tira nativa (bancada)
WebUI/src/bridge/bridgeCore.js         transporte JS (window.__JUCE__.backend; modo local sin JUCE)
WebUI/src/app.js                       panel conectado, con fases de gesto y estado normalizado
Tests/ParameterBridgeTest.cpp          protocolo completo contra el APVTS del contrato
Tests/webviewBridgeDirectionTest.mjs   guard de dirección (compartido con ABDSharedCode)
```

(Las dos rutas JS de esta tabla decían `WebPilot/lib/bridge.js` y `WebPilot/app/page.jsx` hasta la
retirada del piloto: son los ficheros que se portaron a `WebUI/src/`, y desde 8.4 viven ahí.)

Puntos clave de diseño (el detalle completo está en `DOCS/PILOT_RETIRED.md`, sección del bridge —
ese documento es el registro del piloto y se conserva por esto mismo):

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

El formato del cable está fijado en `WebUI/contracts/bridge-protocol.json` (versionado en git,
versión 1) con especificación completa en `DOCS/BRIDGE_PROTOCOL.md`. Misma mecánica que el
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

## Decisiones pendientes (histórico, 2026-09-16)

> Resueltas desde entonces: el stack es **JS vanilla + Vite** (Next.js retirado el 2026-09-19),
> el primer panel migrado fue GENERAL y el lienzo 8.2 ya cubre los 70 parámetros, y los presets
> viajan por el **protocolo v1 del bridge** (2026-09-17). Lo que queda por decidir vive en la
> **Fase 8** de `ROADMAP.md`.

- Si la interfaz web piloto se hará inicialmente con React/Vite o Next.js estático.
- Qué panel será el primero en migrar.
- Si el primer WASM incluirá ambos motores (`Neuronik` y `Neurotik`) o solo uno.
- Qué formato de preset común se utilizará entre APVTS, web y WASM.
- Qué componentes de `ABDSharedCode` se extraerán sin acoplarlos a Next.js.

## No hacer todavía (histórico, 2026-09-16)

> Frenos del arranque de la migración. Los vigentes los fija la **Fase 8** de `ROADMAP.md`
> (p. ej. solo este proyecto —ABDMS2000 no se toca— y no revivir las homonimias retiradas, ver
> 8.5). Dos se siguen cumpliendo por diseño: el APVTS sigue siendo la SSOT de parámetros y los
> IDs no se duplican a mano (viajan en el contrato generado).

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

---

## 8.1, paso 2c: el selftest de cuatro direcciones pasa al editor, y la bancada usa el MISMO (2026-09-19)

Ultimo paso de 8.1 antes de retirar el piloto. El arnes deja de ser de la bancada y pasa a
`Source/WebUI/BridgeSelftest.h`; quien lo corre **en el plugin** es el editor, porque es la
unica superficie que hospeda la pagina en los dos formatos.

**Que se movio**

| Pieza | De | A |
|---|---|---|
| La maquina de estados del selftest | `Source/WebPilotHost.cpp` (~300 lineas dentro de `PilotComponent`) | `Source/WebUI/BridgeSelftest.h` (`NEURONiK::WebUI::BridgeSelftest`) |
| Selectores de la pagina | repartidos por el C++ de la bancada | `SelftestPage` (un solo sitio, documentado como contrato) |
| Quien lo dispara | solo la bancada (`--selftest`) | el editor del plugin (argv en Standalone, `NEURONIK_SELFTEST=1` en cualquier formato) |

La bancada **ya no tiene copia**: la usa igual, enchufandole su navegador y su veredicto. El
port es de comportamiento: los cinco scripts JS, los tiempos (400/600 ms) y el texto de las
lineas del log son los mismos, asi que un log del plugin y uno de la bancada se leen igual.

**El disparo, y por que son dos vias**

```text
NEURONiK.exe --selftest                    -> Standalone (proceso): codigo de salida 0/1
NEURONIK_SELFTEST=1 NEURONiK.exe           -> cualquier formato, y el UNICO del VST3
NEURONIK_SELFTEST_LOG=<ruta>               -> donde queda el transcript (opcional)
```

El VST3 lo lanza el DAW y no recibe argv, asi que la variable de entorno no es un atajo: es su
unica via. Con ella puesta el arnes corre dentro de pluginval o del DAW que abra el editor. El
log por defecto va a datos de usuario del sistema (`.../NEURONiK/neuronik-selftest.log`)
porque el VST3 no puede escribir junto a su propio binario (esa carpeta es del host).

**Dos precauciones que la bancada no necesitaba**, porque alli moria el proceso entero:

- **timeout de 30 s**: un hop perdido termina en FAIL, nunca deja el editor de un DAW colgado;
- **guarda de vida en TODOS los callbacks** (evaluaciones del navegador y `callAfterDelay`): el
  editor se puede cerrar con hops en vuelo, y el arnes se declara despues de `webView` en el
  editor para morir **antes** que el navegador. Sin esto, cerrar el editor a mitad del selftest
  es un `use-after-free` con la firma de un crash aleatorio del plugin.

**El contrato con la pagina, fijado por los dos lados.** Los anclajes que el arnes consulta (el
primer `input[type=range]`, el `<code>` del pie, la pestana KEYS, la rueda de modulacion,
`__pilotSendMidi`) y los 11 ids de GENERAL se comprueban en
`Tests/webuiSelftestContractTest.mjs` (ctest, `NEURONiK_WebUiSelftestContract`, test 18): que el
C++ declare exactamente esos anclajes, que cada uno siga en el fichero de la pagina que lo posee
**y** pinchado en la suite de la propia pagina, y que las dos listas de ids de GENERAL coincidan.
Un anclaje que se mueve en un solo lado deja el selftest comprobando lo que no cree: eso es un
falso OK, y es lo que este test impide. Verificado que falla de verdad (mutacion temporal del
anclaje → exit 1).

**Verificacion (Release, todo en verde)**

```text
build completo (cmake --build build-reference --config Release)  0 errores, 0 avisos propios
ctest -C Release                                                 18/18 (17 + el nuevo)
NEURONiK.exe --selftest                                          las 4 direcciones OK, exit 0
NEURONiK.exe con NEURONIK_SELFTEST=1 (sin argv)                  OK, exit 0
NEURONiK Web Pilot.exe --selftest (arnes compartido)             OK, exit 0 (sin regresion)
```

**Dos regresiones del paso 2, arregladas aqui** (el `build.bat` se paro en 5/10 y el log solo
ensenaba la primera, porque la compilacion aborta):

- **103 errores dentro de `juce_StandaloneFilterWindow.h`** al compilar `NEURONiKEditor.cpp`. El
  paso 2 quito `#include <juce_audio_utils/...>` del editor, y ese header **no incluye sus
  dependencias** (`juce_audio_devices` para `AudioDeviceManager`/`AudioIODeviceCallback`/
  `MidiInput` y `AudioProcessorPlayer` de `juce_audio_utils`): confia en el `JuceHeader.h` del
  wrapper. Los tres includes van ahora explicitos en el guard de `JucePlugin_Build_Standalone`,
  con el motivo escrito al lado.
- **10 `LNK2019` de `ParameterBridge`** al enlazar el VST3: `ParameterBridge.cpp` se quedo fuera
  de los targets del plugin (solo lo anadian la bancada y los tests), asi que
  `NEURONiKEditor.obj` pedia el puente y nadie lo aportaba. Va en el `foreach` de los targets
  que montan pagina y **no** en `NEURONIK_SOURCES`: quien lo necesita es la pagina, no el motor.

**`build.bat` 10/10** ejecuta ahora PRIMERO el selftest del **plugin** (la superficie que se
envia; su Standalone hospeda la pagina de `WebUI/dist`) y despues el de la bancada, que sigue
contando hasta el commit de retirada. El transcript del plugin queda en
`build-reference\neuronik-selftest.log`, para leer el detalle sin depender del stdout.

**Lo que queda de 8.1 / 8:** el **commit de retirada del piloto** (mudar las 3 SSOT —
`WebPilot/generated`, `bridge-protocol.json`, `public/` — y repuntar sus 4 consumidores, mas
`bridgeProtocolContractTest.mjs` y `webviewBridgeDirectionTest.mjs`), y despues 8.2 (paridad de
control). El **VST3 real** con un host dentro (DAW o pluginval) es 8.5: su arnes ya esta puesto
y documentado, pero no se apunta como verificado.

## 8.2 — el lienzo unico: los 70 controles en una pantalla (2026-09-19)

**Peticion:** lienzo mas grande (referencia ABDMS2000) e intentar todos los controles en una
sola pantalla; si no cabe, el sistema de los hermanos (fichas por secciones con lo principal +
panel deslizante lateral, que es EXACTAMENTE lo que hace ABDMS2000 con `slideDrawer`; ABDEep
mide 1200x768 y el suyo tambien va por ahi).

**Lo primero fue medir, no opinar.** Hechos que cambian la decision:

- ABDMS2000 tiene editor de **1080x680** (mas pequeno que el que tenia NEURONiK, 1100x720) y
  reparte casi todo en cajones; ABDEep 1200x768. O sea: "tamano MS2000" NO es mas grande.
- NEURONiK tiene 70 parametros, no los ~40 de un MS2000. Para que quepan de una vez hacen
  falta ~1440x900 de superficie, mas ancho que cualquier hermano: eso es "mas grande" aqui.
- Reparto por el contrato (sin tablas a mano): 46 `float`, 5 `bool`, 19 `choice`.

**Decision: un solo lienzo de 1440x900, sin pestanas de parametros.** Siete fichas en tres
bandas de 12 carriles, todas a dos filas, siguiendo la agrupacion del panel nativo:
OSCILADOR (12) + RESONADOR (4) + GLOBAL & MASTER (9) / FILTRO & ENVOLVENTE (11) + EFECTOS (12)
/ LFO 1 & 2 (10) + MATRIZ DE MODULACION (12) = **70**. La franja de teclado es fija abajo
(como en el nativo) y plegable con el boton TECLADO.

**El encaje es un numero, no una impresion.** `src/contracts/sections.js` es la SSOT del
reparto y la geometria, y `tests/sections.test.js` comprueba con esos mismos numeros: alto
calculado <= 1440x900, fichas a dos filas, bandas que llenan el ancho, y que la CSS declara
las MISMAS medidas (`--abd-cell-h: 80px`, `--abd-knob-size: 48px`...).

**Y se midio en un motor de verdad, porque jsdom no calcula layout.** Arnar temporal en
iframe + Chrome headless a 1440x900 (borrado despues): `scrollHeight == clientHeight == 900`,
`scrollWidth == clientWidth == 1440` -> **cero desborde, cero barras**; 70 celdas (45 knob /
5 toggle / 19 select), 36 teclas y la rueda de modulacion montadas, y el primer
`input[type=range]` del documento sigue siendo `masterLevel`. Esta medicion pago sola: mi
primera cuenta dejaba fuera bordes, huecos de fila y relleno del armazon, y el lienzo
desbordaba **33 px** (scroll activo) sin que ningun test unitario lo notara.

- **Tipo por descriptor, no por tabla:** `float -> Knob`, `bool -> Toggle`, `choice ->
  desplegable`. `masterLevel` es el UNICO control fuera de la familia compartida: su `range`
  nativo es la mitad del contrato del selftest (8.1 paso 2c) y va en la ficha GLOBAL, por
  delante de las ruedas del teclado.
- **Hueco encontrado en `@abdsynths/shared`:** la familia no tiene control de LISTA
  (Knob/Slider/Toggle/Wheel/XYPad). Los 19 `choice` van a un `<select>` nativo estilizado; un
  control `Select` compartido es candidato claro, sobre todo para los 28 destinos de la matriz
  de modulacion.
- **Sin pestanas, pero con los anclajes:** `[data-tab="keys"]` sigue publicado (ahora es el
  boton que pliega la franja de teclado) porque el selftest del host lo pulsa; el anti-drift
  (`Tests/webuiSelftestContractTest.mjs`) lo exige en `contracts/screens.js` y en la suite.
- **Verificacion:** vitest **127/127** (13 ficheros; +3 ficheros nuevos), `pnpm build` OK
  (80,2 KB JS / 20,8 KB gzip), build Release del plugin 0 errores, **18/18 ctest** y el
  `--selftest` del Standalone contra la pagina nueva: 4 direcciones OK, exit 0.
- **Editor nativo:** `NEURONiKEditor` pasa a 1440x925 (`canvasWidth`/`canvasHeight` + la barra
  de menu), para que el WebView reciba exactamente el lienzo. El ancho del lienzo vive en tres
  sitios que tienen que decir lo mismo (constante C++, `CANVAS` de sections.js,
  `--abd-canvas-w` de la CSS); los dos del lado de la pagina se comprueban entre si.

**A/B contra el nativo con el mismo preset (hecho 2026-09-19).** `Tests/nativePanelParityReport.mjs`
extrae los dos inventarios de sus fuentes —los patrones de `Source/UI/**` y el contrato +
`sections.js`— y los cruza id a id con el preset cargado (el INIT del contrato, o un
`.neuronikpreset` real con `--preset`). El informe completo esta anotado en
**`DOCS/WEBUI_VS_NATIVE_PARITY.md`**; el script corre en ctest (`NEURONiK_NativePanelParity`, test
19) solo por sus dos invariantes (ningun id fuera del contrato, ninguna celda repetida) y esta
verificado que **falla de verdad** (mutacion temporal del lienzo -> exit 1).

Lo que sale con el INIT: la pagina ve **70/70**, el nativo **66/70** con algun control y **1/70**
montado en lo que se envia (los paneles estan compilados y sin instanciar: se retiran en 8.4),
**4** parametros sin control nativo en ningun sitio (`oscLevel`, `midiThru`, `velocityCurve`,
`unisonEnabled` — este ultimo `notRouted`, y la pagina es la unica que lo dice), **41/66**
etiquetas distintas (el nativo abrevia: `VOLUME` vs Master Level, `ROOM` vs Reverb Size) y
**4/66** tipos (`mod*Amount`: fader horizontal vs knob).

El unico hueco que es **funcion** y no presentacion: el nativo **filtra los destinos de la
matriz por motor** (desactivando indices 21-27 en un timer de 100 ms) y la pagina entrega
`engines` en el view-model sin que **nadie lo consuma**; ademas el gating nativo es fragil por
indice. Lo demas es cosmetica deliberada de un panel que se retira (incluido que el nativo lee los
valores con `juce::String(v, 2)`, sin unidades). La lectura literal del DoD ("se ve identico")
queda descartada con evidencia: la paridad que se exige es **mismo valor real** y **misma
cobertura**, y eso se cumple.

**RANDOMIZE y primera retirada del arbol nativo (hecho 2026-09-19, despues del A/B).**

- **RANDOMIZE al estado, no al panel.** El sorteo vivia en `ParameterPanel::randomizeParameters()`
  y se iba a perder con el panel. Ahora es **`State/ParameterRandomizer`** (tabla de intencion +
  congelados, probado sin procesador ni UI) y el puente gana la accion **`randomize`** (sin
  campos: la fuerza sale de `randomStrength`, los congelados del APVTS). La pagina tiene el boton
  en la cabecera de GLOBAL & MASTER y sin host queda deshabilitado (`store.randomize()` devuelve
  false en modo local).
- **Dos defectos corregidos, con test** (`NEURONiK_ParameterRandomizerTest`, nuevo, test 20):
  1. la mezcla promediaba el valor actual en **REAL** con el sorteo en **NORMALIZADO**
     (`jmap(strength, currentValue, random0to1)`): todo parametro cuyo rango real no fuera 0..1
     acababa clavado en su maximo (un cutoff saltaba a 20 kHz). Ahora la mezcla es lineal en
     unidades reales y se convierte una vez;
  2. la ventana de `resonatorRes` (0.3..0.95) **no cabia** en su parametro (0.5..1): el test exige
     que cada ventana quepa en su rango, que a fuerza 0 no cambie nada, que lo congelado no se
     mueva y que la mezcla sea el punto medio en Hz.
- **Retirada del arbol sin instanciar.** Fuera del arbol y de `CMakeLists.txt`: los **cuatro
  paneles de parametros**, `PresetPanel`, `PresetBrowser` (y `PresetListModels.h`), `LcdDisplay`,
  `LcdMenuManager`, `SpectralVisualizer` y `EnvelopeVisualizer`. Con `ModulationPanel` se fue su
  `timerCallback`, que **reescribia el APVTS cada 100 ms** (puesto el destino de modulacion en Off)
  y era el bug mas serio que quedaba en el arbol. Se quedan `ParameterPanel` y `XYPad` porque la
  bancada los monta, y el overload de `VerticalSliderControl` porque **no era muerto** (es el
  master vertical: la afirmacion contraria del informe se corrigio).
- **El A/B sigue al arbol**: `Tests/nativePanelParityReport.mjs` pasa de 66 a **15** ids nativos y
  ahora **falla** si una superficie declarada desaparece (antes un borrado lo habria reventado con
  un stacktrace). El informe esta en `DOCS/WEBUI_VS_NATIVE_PARITY.md`.

**Verificacion:** build Release 0 errores · **20/20 ctest** · vitest **132/132** ·
`--selftest` del Standalone contra la pagina nueva (con el boton RANDOM): 4 direcciones OK, exit 0.
No commiteado; `Source/ModelMaker/Version.h` revertido (lo autobumpea compilar ese target).

**Curva ADSR (hecho 2026-09-19).** `src/ui/envelopeCurve.js` dibuja la envolvente de amplitud a
partir de los cuatro `env*` del lienzo, en la **celda libre** de la ficha FILTRO & ENVOLVENTE (11
controles en una rejilla de 6x2), asi que la geometria no cambia ni un pixel: la celda mide los 80
px fijos de `.cell` y el SVG se estira dentro. Decisiones que no son obvias: los tiempos se
comprimen con raiz cuadrada (el rango va de 1 ms a 5 s y el `skew` de esos parametros ya es
logaritmico; en lineal un ataque de 1 ms seria medio pixel) y el tramo de sostenido tiene ancho
propio (no es un tiempo: sin el, un sostenido sin rampas parecia una meseta de ancho cero).
No es una celda de parametro: lleva `.card__visual`, no `.cell`, para que "70 celdas" siga
significando lo mismo en los tests y en el informe de paridad. Vistas y acciones de ficha se
declaran como DATO (`SECTION_VISUALS`/`SECTION_ACTIONS` en `sections.js`) y las resuelve `app.js`;
el panel solo las pinta.

**`Select` compartido, gating por contrato y matriz al cajon (hecho 2026-09-19).** Los tres salen
del mismo hilo: el lienzo tenia 70 controles a la vez y el unico hueco que era FUNCION (no
presentacion) en el A/B era el filtrado de destinos por motor.

- **`Select` en la familia compartida** (`ABDSharedAssets/components/select.js` + CSS + skin
  `vector`/`ms2000` + 14 tests + demo): las 19 listas del lienzo ya no construyen ningun `<select>`
  a mano. Modelo de valor por **INDICE** (el hermano discreto del boolean de `Toggle`): normalizar
  a 0..1 queda al llamador porque en un `choice` ese mapeo lleva el skew del parametro, y meterlo
  en la capa compartida arrastraria matematica del APVTS. Dos cosas que un control nuevo necesita y
  que ahora estan resueltas: `applySkin` **falla con error explicito** cuando no hay renderer para
  un `CONTROL_KIND` (antes salia como "fn is not a function", que parece un error del llamador), y
  el nombre `.abd-select` choca con el de `controls.css` (la libreria CSS previa, que estiliza un
  `<select>` crudo) → el layout del bloque es **opt-in** (`.abd-select--labelled`) para que un
  `<select>` suelto siga viendose igual, con test. Es el unico de la familia sin `drag-core`
  (arrastrar por 28 opciones elige por accidente): el desplegable y las flechas cubren la edicion.
- **Gating de destinos por motor, en el CONTRATO.** `optionEngines` (un motor por opcion) +
  `engineParameter` (el id del selector) salen del generador; en C++ los deriva
  `applyEngineGating()` de la **tabla unica** de destinos (`State/ParameterDefinitions.h`:
  etiqueta + parametro que mueve, EN ORDEN porque el indice es estado de preset) cruzada con
  `engineCoverageFor()`, mas la cobertura de cada opcion del propio `engineType`. El reparto sale
  identico al del panel retirado (neuronik `2 3 10-16 20 21 22`, neurotik `23 24 25 26`, 12 de los
  dos) y el test de contrato lo pincha, junto con las etiquetas POR INDICE y que
  `getModDestinations()` liste la tabla (antes la lista estaba duplicada a mano dentro del layout).
  En la pagina, el `Select` deshabilita lo que el motor activo no consume, pone el motivo en la
  opcion (`title`) y **nunca reescribe el valor**: lo marca (`[data-divergent]`). Si el snapshot no
  trae el motor, no se aplica gating (no se inventa el activo).
- **Matriz de modulacion al cajon lateral.** Una ficha puede declarar `drawer` en el reparto: sus
  celdas se montan en `src/ui/drawer.js` (patron ABDMS2000/ABDEep/ABDCZ101) agrupadas por ruta, y en
  el lienzo quedan el resumen de las 4 rutas (`src/ui/modSummary.js`) y el boton. **Diferencias
  deliberadas con el `slideDrawer` de los hermanos**: el contenido NO se reconstruye al abrir (las
  70 celdas estan siempre en el documento: el selftest y la suite cuentan celdas, y reconstruir
  perderia el gesto en curso) y abrir/cerrar es una clase CSS. Los ids siguen en el reparto, asi que
  store, recuento y cobertura no cambian. Medido en Chrome a 1440x900: **0 px de desborde** con el
  cajon fuera de pantalla, 12 celdas en el cajon y 0 en la rejilla, 4 rutas en el resumen, teclado
  montado y `masterLevel` como primer `range`.
- **Flecos que la pasada dejo de paso**: la fabrica de vistas (`src/ui/visuals.js`) sale de `app.js`
  porque el harness del test la duplicaba (montaba una curva ADSR para CUALQUIER vista declarada, y
  al anadir el resumen contaba dos); el resumen **no** lleva `data-parameter-id` (ese atributo marca
  celdas de control en esta pagina, y marcarlo duplicaba el recuento de los 70); y un cajon
  destruido ya no muta estado.

**Fleco abierto, con el dato localizado: el ANILLO del valor modulado.** El procesador **si**
publica la modulacion viva (`getModulationValueForUI()` sobre `modulationValues[]`, que llenaba el
`ModulatedSlider` nativo), pero **no viaja en el cable**: el protocolo del puente
(`Source/WebUI/ParameterBridge.h`, versionado en `WebPilot/contracts/bridge-protocol.json`) no tiene
canal de modulacion. Hacerlo bien es un cambio de frontera: controlador en el puente + mensaje
aditivo (tipo `midiNoteState`) + poll del host + estado en el store + el anillo como opcion del
`Knob` compartido. Se deja para la proxima pasada **a proposito**: dibujar el anillo con la CANTIDAD
del slot no seria el valor modulado, seria otra cosa con el mismo nombre.

**Lo que queda de 8.2:** el anillo (arriba) y, si una seccion crece, mas cajon (el reparto es
dato: cambiarlo es una linea y el test de encaje avisa). Los slots de modelo A-D se cierran en la
pasada siguiente (abajo).

**Verificacion de esta pasada:** vitest **166/166** en la WebUI (+14 en `ABDSharedAssets`, 53/53) ·
`pnpm build` OK · build Release 0 errores · **20/20 ctest** · `--selftest` del Standalone: **4
direcciones OK, exit 0** · contrato regenerado (`parameters.generated.*`, con `optionEngines`) y
`ModelMaker/Version.h` revertido. No commiteado.

## 8.2 (b) — los slots de modelo A–D: la carga la hace el host y contesta TARDE (2026-09-19)

**Que era en el nativo.** El bloque MODEL de `OscillatorPanel` eran cuatro botones
`loadA..loadD` sobre el XYPad (`buttonClicked`): abrian un `juce::FileChooser` de
`*.neuronikmodel`, cargaban con `processor.loadModel(file, slot)` (slot 0 = A) y **rotulaban la
ranura con el nombre del fichero elegido**. Ese `loadModel` no devolvia nada y salia en silencio
si el fichero no era un modelo valido, asi que un fichero inservible dejaba el nombre puesto
hasta que el timer de 10 Hz lo corregia leyendo `getModelNames()`. Un tick diciendo una cosa y el
motor teniendo otra.

**Que hay ahora.** Ficha **MODELOS A–D** en el lienzo (`span: 2`, en la banda del LFO: la matriz
baja de 8 a 6 carriles porque en el lienzo solo alberga el resumen — 6 le sobran) con cuatro
filas (letra, nombre cargado, **CARGAR**) y una linea de estado (`n/4 cargados`, o el motivo del
ultimo fallo). Las ranuras son del MOTOR, no del APVTS (un preset lleva `modelPath<slot>`, no una
copia de los parciales), asi que la ficha **no tiene celdas**: el encaje de los 70 no se mueve y
un test lo fija, junto al reparto de esa banda.

**Decision de sitio, consultada.** Cualquier fila nueva son 84 px que el lienzo no tiene (18 px de
holgura), asi que el cajon lateral era la salida natural; se pregunto y se eligio **ficha propia
siempre visible** (los cuatro botones del nativo tambien lo estaban, sobre el pad) antes que un
cajon para cuatro botones.

**La carga, en el contrato (aditivo a v1).**

- `loadModel { slot: 0..3 }` (JS→nativo) es la **unica accion cuya respuesta llega DESPUES**: la
  pagina no tiene sistema de ficheros ni puede nombrar rutas (el cable no es de fiar), asi que el
  HOST abre el dialogo (`EngineModelsAdapter`, con el `juce::FileChooser` que tenia el panel) y
  contesta mas tarde. `NativeModelController` gana `loadModel(slot)` y `getModelName(slot)`.
- `modelsState` viaja con **`name`** por ranura ("EMPTY" cuando no hay nada): el nombre solo lo
  sabe el motor y las dos superficies tienen que ensenar la MISMA lista.
- `modelError { slot, detail }`: cancelar, elegir algo que no es un modelo, ranura fuera de rango
  o **fraccionaria** (se rechaza, no se trunca) o no haber backend. Una carga nunca falla en
  silencio, que era justo el defecto de arriba. `stats.modelLoads` / `stats.modelErrors` lo hacen
  observable.
- `NEURONiKProcessor::loadModel` ahora devuelve `bool` (antes no habia forma de saber si la carga
  ocurrio) y solo renombra la ranura cuando el modelo es valido.
- El contrato versionado documenta las tres formas y la regla (`behaviour.modelMessages`), y los
  dos tests de contrato pinchan los literales nuevos.

**Vida de un dialogo abierto.** El adaptador es dueño del `FileChooser` y `juce::FileChooser`
suelta su callback pendiente al destruirse (`~FileChooser` limpia `asyncCallback` y el pimpl se
apaga con `safeThis.lock()` fallando), asi que un host cerrado a media eleccion de fichero no
contesta a un puente muerto; en el editor, ademas, los adaptadores se declaran DESPUES del puente
(mueren antes) y el destructor corta el transporte primero. Se comprobo en las fuentes de JUCE
(`juce_FileChooser_windows.cpp`) antes de apoyarse en ello.

**En la pagina.** `loadModel(slot)` en el store (devuelve false sin host: no hay a quien pedirselo,
igual que `randomize`), `modelError` en el estado (un intento nuevo limpia el mensaje viejo, que es
de otra carga), y la vista `src/ui/modelSlots.js` como DATO de ficha (`SECTION_VISUALS`,
`parameterIds: []`): el panel le da sitio y **estado entero** (`paint(parameters, state)`, porque
`models`/`modelError` viven fuera del APVTS) y el handler de carga le llega por `options.onLoad`
desde `app.js`, para que el panel siga sin saber que dibuja cada vista. Un nombre con
`isValid: false` se **marca** en ambar (mismo criterio que el gating del `Select`), no se oculta.

**Verificacion de esta pasada:** build Release **0 errores** (VST3 + Standalone + bancada) ·
**20/20 ctest** · vitest WebUI **185/185** (+19: 12 en `tests/modelSlots.test.js` y el resto en
`sections`/`panel`/`bridgeCore`/`paramStore`/`appContract`) · `pnpm build` OK (95.2 KB JS) ·
medicion en Chrome a 1440x900 con la ficha nueva: **0 px de desborde**, 8 fichas, **70 celdas**,
19 select, 4 filas de ranura (228x206 px de ficha, 164 px de cuerpo para 86 px de vista) y
`masterLevel` como primer `range` · `--selftest` del Standalone: **4 direcciones OK, exit 0**. No
commiteado.

## 8.2 (c) — «cargar un modelo en A–D se ve y suena», comprobado, y destapa un fallo viejo (2026-09-19)

**El encargo, partido en dos mitades porque una sola superficie no puede hacer las dos.** Un proceso
con ventana no puede medir su propia salida de audio sin pelearse con el hilo que la está tirando
(con un dispositivo de audio abierto lo hace el `AudioProcessorPlayer`), así que la prueba se reparte:

- **SE VE** — la **quinta dirección** del selftest (`Source/WebUI/BridgeSelftest.h`), corriendo en el
  **plugin real**: escribe cuatro `.neuronikmodel` en el directorio temporal, los carga por
  `NEURONiKProcessor::loadModel` en A–D y lee los cuatro nombres (`.model-slots__name`) de la ficha de
  la página de verdad, en su WebView2. Se escriben en el **JSON del ModelMaker** a propósito: si el
  plugin dejase de entender ESE formato, las ranuras se quedan mudas y la dirección lo dice en voz
  alta en vez de dar OK con cuatro ranuras vacías.
- **SUENA** — `Tests/ModelSlotTest.cpp`, con el procesador real (**26 comprobaciones**).

**Lo que destapó la mitad «suena», que es la razón de que exista.**

1. **El formato del ModelMaker nunca se leía.** `PresetManager::loadModelFromFile` solo entendía el
   dialecto XML (`<NEURONIK_MODEL amplitudes=".." offsets=".."/>`) y la herramienta escribe **JSON**
   (`{amplitudes[64], frequencyOffsets[64], name, description}`), así que **los ficheros de la propia
   herramienta no cargaban**: la ranura se quedaba con nombre y sin sonido — exactamente el síntoma
   del panel nativo, pero por otra causa. Ahora se leen los dos dialectos y lo que no es un modelo se
   rechaza sin tocar la ranura.
2. **Mi primer arreglo tenía un `use-after-free`.** `juce::JSON::parse(texto).getDynamicObject()` sobre
   un `var` **temporal** devuelve un puntero que muere al final de la sentencia: el objeto parseaba
   «bien» y salía **vacío**. Lo cazó el propio test (un literal parseaba y el fichero no), y el patrón
   correcto es el que ya usaba `BridgeSelftest.h`: guardar el `var` y leerlo después. Anotado porque es
   un fallo con la firma de un crash aleatorio en producción, no de un error de compilación.
3. **Un crash de división por cero que NO era del test.** El test reventaba con
   `STATUS_INTEGER_DIVIDE_BY_ZERO` (0xC0000094) en el primer bloque **con nota**. La causa: cambiar de
   motor construye un motor nuevo y lo prepara con `getSampleRate()`/`getBlockSize()`, que valen 0
   hasta que el host fija la tasa. Dos caminos lo disparaban:
   - el **constructor del procesador** llamaba `LOAD_PARAM(engineType)`, así que construía un SEGUNDO
     motor (32 voces, delay, reverb) para prepararlo con 0 y tirar el primero — en Debug eso es un
     `jassert` en el smoothed value de la saturación. El motor de arriba ya se creó con ESE mismo
     valor; el que queda lo deja a punto `prepareToPlay`. `engineType` sale de esa lista.
   - `parameterChanged` preparaba el motor nuevo con la tasa del host **aunque el host no la hubiera
     dado**: ahora, sin tasa, el motor se prepara en `prepareToPlay` (un host siempre llama a prepare
     antes de que haya audio). Es la misma clase de bug que un preset restaurado antes de que el
     reproductor arranque, así que se cierra con guarda **y con test** (sección 6).
   El test ahora se prepara como lo hace un host (`setRateAndBufferSizeDetails`) en vez de confiar en
   `prepareToPlay`, que es una llamada virtual: **fue el arnés el que mentía**, no el motor.

**Una tercera cosa, ya en el arnés: ni un OK engañoso ni un FAIL ajeno.** La bancada del piloto sirve
todavía su exportación retirada (`WebPilot/out`), anterior a la ficha MODELOS A–D, así que su quinta
dirección fallaba por una página que ya no se mantiene — y un FAIL ahí no habla del puente. El arnés
no adivina: **el dueño declara** `BridgeSelftest::PageCapabilities`, cuyo defecto es la capacidad
**COMPLETA** (callar no libra de ninguna dirección), y la bancada declara `retiredPilotPage()`. Su
transcript dice `MODELOS: OMITIDO (no aplicable): la pagina de esta superficie no publica la ficha…` y
el veredicto lo repite **en su misma línea** (`RESULT: OK  (MODELOS no aplicable en esta pagina;
obligatoria en el plugin)`), para que un OK de cuatro no pueda parecer un OK de cinco. El editor del
plugin no declara nada y `Tests/webuiSelftestContractTest.mjs` lo fija por los dos lados: capacidad
por defecto completa, parámetro opcional, y **el único que se acoge al omitido es la bancada**.

**Verificación de esta pasada:** build Release **0 errores** (VST3 + Standalone + bancada) ·
**21/21 ctest** (incluido el nuevo contrato del arnés) · vitest WebUI **185/185** · `--selftest` del
**Standalone**: `MODELOS`, `NATIVO -> JS`, `JS -> NATIVO`, `GENERAL` y `MIDI` en verde, `RESULT: OK`,
**exit 0** · `--selftest` de la **bancada**: las cuatro que su página soporta en verde, `MODELOS
OMITIDO` declarado, **exit 0**. No commiteado.

## La bancada sirve la página del PLUGIN (y `--pilot-page` para la retirada) (2026-09-19)

**El problema, en una frase.** La bancada era, desde el principio, la única superficie con página; el
plugin la hospeda desde el paso 2c, y entonces la bancada se quedó comprobando **otra** página (la
exportación retirada, sin la ficha MODELOS A–D) que ya nadie mantiene: un veredicto verde ahí no decía
nada de lo que se envía, y su quinta dirección tenía que declararse no aplicable.

**Qué cambia.** `Source/WebPilotHost.cpp` gana `PageSource` (`pluginWebUI` por defecto, `pilotExport`
con `--pilot-page`):

- **Por defecto sirve `WebUI/dist`** (la página que embebe el plugin) desde disco, con la ruta
  grabada en el binario (`NEURONiK_WEBUI_DIR`) y el mismo override de desarrollo que el plugin
  (`NEURONIK_WEBUI_DEV_DIR`, apuntar ahí y recargar). Su `--selftest` corre las **cinco**
  direcciones, igual que el Standalone, y por eso `build.bat` ya no puede dar un verde de cuatro.
- **El fallback embebido no cambia de página.** El snapshot del exe es la exportación del piloto
  (`NEURONiK_WebPilotAssets`), así que solo se usa cuando la página servida ES esa (`--pilot-page`).
  Con la del plugin, un fallo de disco da la página de diagnóstico visible — que dice qué página se
  estaba sirviendo, dónde la buscó y cómo arreglarlo — en vez de responder a `WebUI/dist` con la
  página del piloto: un fallback que cambia de página no es un fallback.
- **La capacidad del arnés viaja con la bandera**, no con el ejecutable: `--pilot-page` declara
  `PageCapabilities::retiredPilotPage()` (y su transcript lo dice en voz alta); el defecto no declara
  nada, o sea las cinco direcciones obligatorias. Es el camino débil **pedido a propósito**.
- **El reporte dice qué página se sirvió** (`page`, `page root`) además de las métricas de arriba,
  así que un log viejo no se puede confundir con el de otra página.
- **`build.bat` simplificado de paso:** la guarda del selftest de la bancada ya no es
  `WebPilot\out\index.html` ni el resultado de la exportación del piloto (`WEBUI_BUILD_FAILED`
  desaparece), sino `WebUI\dist\index.html`: lo que se comprueba es la página que se envía.

**Verificación (las dos rutas, sobre el mismo binario):**

```
"NEURONiK Web Pilot.exe" --selftest              -> sirviendo WebUI/dist (la pagina del plugin)
   MODELOS OK · NATIVO->JS OK · JS->NATIVO OK · GENERAL OK · MIDI OK · RESULT: OK  (exit 0)
"NEURONiK Web Pilot.exe" --selftest --pilot-page -> sirviendo WebPilot/out (la exportacion retirada)
   MODELOS: OMITIDO (no aplicable) · las otras cuatro OK · RESULT: OK (MODELOS no aplicable)  (exit 0)
```

**Fleco abierto:** el snapshot embebido de la bancada sigue siendo el del piloto (y su target
**exige** `WebPilot/out` para configurarse). Cuando llegue el commit de retirada, eso se va con él:
la bancada pasará a servir solo `WebUI/dist` y su `CMakeLists` perderá el `FATAL_ERROR` y
`NEURONiK_WebPilotAssets`.

## La dirección MATRIZ: el selftest corre con el cajón abierto y la matriz en uso (2026-09-19)

**El encargo.** Verificar que el cajón lateral de la matriz no rompe el selftest del plugin **con la
matriz en uso** — no que el cajón pinte bien (eso lo cubre `WebUI/tests/drawer.test.js`), sino que el
resto del arnés siga midiendo lo mismo cuando la UI está en ese estado.

**Cómo se comprueba, y por qué así.** El arnés gana una dirección **0 (MATRIZ)** que corre la PRIMERA y
deja el cajón **abierto a propósito**, para que las otras cinco pasen con la matriz en uso y el lienzo
tapado (un anclaje que deja de ser el primero del documento, un cajón que roba el foco, un poll que
deja de empujar: es ahí donde se rompe una UI):

1. pone la matriz **en uso** por el APVTS, que es por donde la pondría un preset: ruta 1 = LFO 1 →
   Filter Cutoff, cantidad +0.5. Los índices que se esperan en la página NO están escritos en el
   arnés: el destino sale de `getModDestinationTable()` (append-only, y su orden ES estado de preset)
   y la fuente de `getModSources()`, así que mover un destino no invalida la dirección;
2. **pulsa el disparador** `[data-drawer-trigger]` — el mismo clic que daría un usuario, no una clase
   puesta a mano — y lee, **dentro del cajón abierto**: sus 4 rutas, sus 12 celdas (3 por ruta), la
   fuente y el destino como `selectedIndex` del `<select>` nativo y la cantidad en el `aria-valuenow`
   del dial del `Knob` compartido. Que el cajón abierto y su velo compartan id se exige aparte: si no
   coinciden, lo que se abrió no es el diálogo que ese disparador gobierna.

Leer los controles **dentro** del cajón no es un detalle de estilo: si la celda estuviera en el
lienzo, la aserción pasaría igual y el cajón podría estar vacío.

**El arnés se cazó a sí mismo (y por eso la dirección vale).** La primera versión comparaba
`drawer.dataset.drawer` con `trigger.dataset.drawerTrigger` para saber que el cajón abierto era el del
disparador: son **ids distintos** (`drawer-modMatrix` — el id del DOM — frente a `modMatrix` — el de la
sección), así que la dirección dijo **FAIL** en su primera corrida con todo lo demás ya en verde. El
fallo no era del cajón: era una suposición mía sobre el DOM. Se sustituyó por la pareja cajón/velo
(comprobación más fuerte y verdadera) y por leer los controles dentro del cajón.

**Dónde SÍ y dónde NO.** La capacidad `matrixDrawer` (`PageCapabilities`) nace **completa**: la página
de la bancada del piloto retirado no lleva cajón, y con `--pilot-page` la dirección se declara NO
APLICABLE en voz alta, igual que MODELOS. El `RESULT` lista **las dos** en su misma línea
(`RESULT: OK  (MATRIZ, MODELOS no aplicable(s) en esta pagina; obligatorias en el plugin)`), y el
contract-test fija que el único que se acoge al omitido sea la bancada.

**Verificación de esta pasada:** build Release **0 errores** · **21/21 ctest** (contrato del arnés
incluido, con los 5 anclajes nuevos pinchados por los dos lados) · vitest WebUI **185/185** (la página
no cambia: esta dirección no pide una línea de JS nueva) · `--selftest` del **Standalone**: MATRIZ +
MODELOS + NATIVO→JS + JS→NATIVO + GENERAL + MIDI en verde, `RESULT: OK`, **exit 0** · `--selftest` de
la **bancada** sobre la página del plugin: **las seis**, exit 0 · `--selftest --pilot-page`: cuatro
verdes y **dos omitidos declarados**, exit 0. No commiteado.

**Lo que esta dirección NO comprueba (a propósito).** Que la modulación llegue al AUDIO: eso vive en
el motor y el arnés no puede exigirlo sin garantizar que el host esté corriendo `processBlock` (en el
VST3 con un DAW dentro, el editor puede estar abierto con el audio parado). Lo que sí queda medido es
que la matriz está configurada y que **la página la enseña donde tiene que enseñarla**.

## Retirada del piloto: las tres SSOT se mudan y la bancada pierde su snapshot (2026-09-19)

**El encargo.** Retirar el piloto: mudar sus tres SSOT (el contrato de parámetros, el contrato
versionado del protocolo y `public/`) y quitar el snapshot que la bancada llevaba embebido, con
los consumidores repuntados en el mismo commit.

**Cuatro cosas que se decidieron mirando el árbol, no el plan:**

1. **La bancada SE QUEDA** — y no es un matiz: el plan decía "se borra tras la mudanza de las
   SSOT", pero al ir a borrarla aparece que `Source/WebPilotHost.cpp` es la **única** superficie que
   monta `ParameterPanel` + `XYPad` (el comentario del procesador ya lo decía: *"its only consumer
   since the EnvelopeVisualizer was retired"*) y la única que mide el arranque (`--auto-quit` →
   `pilot-startup.log`). Borrarla se habría llevado por delante el pad XY y las métricas. Lo que se
   va es su **camino del piloto**: `--pilot-page`, la elección de página (`PageSource`/`pilotRoot`),
   el respaldo embebido (`loadEmbeddedResource` + `NEURONiK_WebPilotAssets`, y con él el
   `FATAL_ERROR` de CMake que exigía `WebPilot/out` para poder configurar) y la maquinaria de
   omisión del arnés. Ahora sirve **solo** `WebUI/dist`, desde disco, y si el disco falla da la
   página de diagnóstico (que dice dónde la buscó) en vez de contestar con otra página.
2. **Los nombres "Pilot" se quedan.** El target (`NEURONiK_WebPilotHost`), su `.exe`
   (`NEURONiK Web Pilot.exe`), `Source/WebPilotHost.cpp`, `pilot-startup.log` y el helper
   `__pilotSendMidi` conservan el nombre. Renombrarlos toca CMake, `build.bat`, `start.bat`,
   `bridge-protocol.json` y tres tests (uno de ellos tiene el helper **pinchado por los dos lados**:
   el C++ y la suite de la página), y no era parte de este encargo. Queda anotado como deuda
   cosmética: mientras el nombre sea el único resto, el arnés no corre contra nada que se llame
   "piloto".
3. **El workspace pnpm de la WebUI era el del piloto.** Hallazgo que no estaba en ningún plan:
   `WebPilot/pnpm-workspace.yaml` listaba `'../WebUI'`, así que los enlaces de
   `WebUI/node_modules/@abdsynths/{shared,midi-keyb}` salían de ahí. Borrarlo sin más habría dejado
   a la WebUI sin poder resolver sus dos dependencias de workspace (el `pnpm install` desde WebUI
   camina hacia arriba, encuentra el workspace raíz de la suite —que **no** lista `ABDNeural/WebUI`—
   y deja `node_modules` sin enlaces). La WebUI estrena el suyo: `WebUI/pnpm-workspace.yaml`
   (ella + los dos paquetes compartidos) y `WebUI/pnpm-lock.yaml`.
4. **El contrato del protocolo nombraba ficheros muertos.** Su lista `implementations` decía
   `WebPilot/lib/bridge.js` y `WebPilot/app/page.jsx`, y `BridgeProtocolContractTest` **exige que
   existan** ("every implementation file the contract names exists"): ahora son
   `WebUI/src/bridge/bridgeCore.js`, `WebUI/src/app.js` y `Source/WebUI/NeuronikWebView.h`. De paso,
   `webviewBridgeDirectionTest.mjs` pasa a exigir **dos** emisores (`WebPilotHost.cpp` **y**
   `WebUI/NeuronikWebView.h`): el editor emite hacia la página desde 8.1 y el guard solo vigilaba
   uno.

**Las mudanzas (con `git mv`, para que se lean como mudanza y no como copia):**

| De | A | Consumidores repuntados |
|---|---|---|
| `WebPilot/generated/` | `WebUI/generated/` | `CMakeLists.txt` (`NEURONIK_PARAMETER_ARTIFACTS_DIR`), `Tests/ParameterExportTool.cpp` (destino por defecto), `build.bat` (paso 2/9) |
| `WebPilot/contracts/bridge-protocol.json` | `WebUI/contracts/bridge-protocol.json` | `CMakeLists.txt` (`NEURONIK_BRIDGE_PROTOCOL_JSON`), los dos tests del protocolo |
| `WebPilot/public/` (assets + worklet) | `WebUI/public/` | `WebUI/vite.config.js` (`publicDir`), `build_wasm.bat`, `sync_wasm.bat`, `start.bat` (guard de staleness) |
| `WebPilotVite/scripts/sync-wasm.mjs` | `WebUI/scripts/sync-wasm.mjs` | `build_wasm.bat` (paso 6/6), `sync_wasm.bat`, y un `pnpm sync:wasm` nuevo en la WebUI |
| `WebPilot/BRIDGE_PROTOCOL.md` | `DOCS/BRIDGE_PROTOCOL.md` | `Source/WebUI/ParameterBridge.h`, `HANDOFF` |
| `WEB_PILOT.md` | `DOCS/PILOT_RETIRED.md` (con aviso de que el piloto no existe) | la referencia del bridge en `HANDOFF` |
| — | `WebUI/pnpm-workspace.yaml` + `WebUI/pnpm-lock.yaml` | `pnpm install` de la WebUI |

**Lo que sale del árbol:** `WebPilot/` completo (app Next, `lib/`, `tests/`, `out/`, `package.json`,
`next.config.mjs`, `vitest.config.js`, su `.gitignore` y su workspace anidado) y `WebPilotVite/`
(el contra-piloto React); la bandera `build.bat nextui`; el camino `--pilot-page` de la bancada; el
snapshot `NEURONiK_WebPilotAssets` y su `FATAL_ERROR`; y `BridgeSelftest::PageCapabilities` con
`retiredPilotPage()` — su único usuario era la página retirada.

**Un cambio que se ve en el veredicto del arnés.** El `RESULT` deja de poder llevar el sufijo
`(MODELOS no aplicable en esta pagina; obligatorias en el plugin)`: no hay páginas a las que rebajar
el listón. `webuiSelftestContractTest.mjs` da la vuelta al check — antes exigía que la capacidad
fuera completa y que el omitido tuviera un único usuario; ahora **prohíbe** las dos cosas
(`PageCapabilities`, `retiredPilotPage`, `capabilitiesToUse`, `matrixSkipped`, `modelsSkipped`): no
basta con no usarla, no puede volver.

**Verificación de esta pasada:** `cmake -S . -B build-reference` **sin** `WebPilot/out` (el
`FATAL_ERROR` ya no existe y no lo echa de menos) · build Release **0 errores** (Standalone + VST3 +
bancada + 20 targets de test) · **21/21 ctest** · WebUI **185/185** con `pnpm test` y `pnpm build`
(`dist/worklet/` sale del `public/` mudado: la prueba de que el `publicDir` es el nuevo) · el
exportador regenera el contrato en `WebUI/generated/` con los mismos 70 parámetros · `--selftest` del
**Standalone**: MATRIZ + MODELOS + NATIVO→JS + JS→NATIVO + GENERAL + MIDI, `RESULT: OK`, exit 0 ·
`--selftest` de la **bancada**: las seis, exit 0, y `pilot-startup.log` con `page: WebUI/dist (la
pagina del plugin)`, `page root: .../WebUI/dist`, 4 recursos y sin línea de respaldo embebido ·
`node WebUI/scripts/sync-wasm.mjs` escribe en `WebUI/public/worklet/`.

**Lo que NO se ha tocado:** el VST3 con un host dentro sigue siendo 8.5; el panel nativo y el
`XYPad` siguen vivos *por* la bancada (su retirada es 8.4); y el anillo del valor modulado sigue
siendo el fleco abierto de 8.2.


## Documentación al día (2026-09-20)

Pasada cosmética de documentación, sin tocar código ni build:

- **`README.md` reescrito en lo operativo**: el build documentado es `build.bat` con sus modos
  (`tests`, `noselftest`, `nowasm`, `modelmaker`, directorio alternativo) y los artefactos en
  `build-reference/NEURONiK_artefacts/`; prerequisitos reales (JUCE por `JUCE_PATH` o `C:\JUCE`,
  Node+pnpm, emsdk en `C:\emsdk` para el WASM). Sección nueva de la versión web (`start.bat` 1-3,
  LOCAL MODE en el 8399, selftest de seis direcciones) y de la WebUI (contrato SSOT generado del
  APVTS, protocolo versionado, worklet WASM con paridad bit-exacta). `Scripts/manage.ps1` queda
  marcado como legado.
- **Secciones históricas anotadas** en este HANDOFF («Próximo trabajo recomendado», «Decisiones
  pendientes», «No hacer todavía») y nota de registro corrido bajo el título, para que nadie
  tome por vigente una decisión del arranque.
- **`DOCS/PLANS/ROADMAP.MD` pasa a stub**: era la copia congelada del 09-16 y había divergido de
  la raíz (que siguió creciendo con las Fases 7 y 8). El roadmap vivo es y sigue siendo
  `ROADMAP.md` en la raíz —lo citan este documento, `DOCS/PILOT_RETIRED.md`, `WebUI/README.md` y
  el test de paridad—; el README ahora enlaza a la raíz.

Sin verificación de build: esta pasada solo toca los tres documentos.

## 8.3, paso 1: el pad XY dibujado (2026-09-20)

**El encargo.** Cablear el XYPad de `@abdsynths/shared` en la WebUI como primera pieza del
8.3: pad para morphX/morphY con los nombres de los modelos A–D, con su test.

**Dónde vive cada cosa** (decisión de capa):

- **El COMPONENTE gana la capability en el compartido**: `ABDSharedAssets/components/xypad.js`
  (ya era del paquete — nació en la era del piloto) añade `corners` (opción) y `setCorners()`
  (en caliente): una etiqueta por esquina `[arriba-izq, arriba-der, abajo-izq, abajo-der]`,
  `''` oculta la suya. Con esquinas visibles el readout de porcentaje se esconde
  (`abd-xypad--corners`): las esquinas dicen QUÉ hay donde, la cruz dice DÓNDE, y el valor
  sigue en `aria-valuetext`. Estilos en `widgets.css`, 4 tests nuevos (16 en su fichero;
  suite 57/57) y `COMPONENTS.md` actualizado. Cero impacto en quien no la use: sin
  `corners`, el DOM del pad no cambia.
- **El wiring es de NEURONiK**: `WebUI/src/ui/xyPad.js` monta el compartido (200x150),
  coordina DOS gestos (uno por eje, fase completa `begin/change/end`; el `begin` anuncia el
  valor actual, igual que `handleGesture`) vía `pushParameter` — `handleChange` solo sabe
  cerrar UN id — y pinta las esquinas desde `state.models` con el mismo criterio de ranura
  vacía que las ranuras (`displayableName`, ahora exportada). Un paso de teclado viaja como
  `end` y SOLO en el eje que cambió. `paint` no pega con el dedo: durante un drag, el
  snapshot del host no mueve el pulgar.
- **La ficha MODELOS A–D es compuesta** (fábrica `visuals.js`): pad encima, ranuras debajo —
  el bloque MODEL del panel nativo, que era exactamente eso. `app.js` inyecta
  `onEdit → store.pushParameter`; el contrato de fuente de `appContract.test.js` vigilaba la
  línea del despachador y se actualizó con la composición.

**El encaje, por delante de la vista**: el pad necesita cuerpo real, así que
`SECTION_VISUALS['model-slots']` declara `minBodyHeight: 256` y `cardHeight()` lo respeta
(`max(pilas de celdas, cuerpo de vista)` — con filas de celdas no cambia nada). La banda 3
la cerraba el LFO (206); ahora la cierra MODELOS (298) → `CANVAS.height` 900 → **990** y
`--abd-canvas-h` a la par (el test de geometría exige el MISMO número en las dos SSOT). Los
knobs morphX/morphY SIGUEN en OSCILADOR: el pad es aditivo, `SECTION_PARAMETER_IDS` sigue en
70 y la paridad del informe no se mueve.

**Verificación de la pasada**: ABDSharedAssets **57/57** · WebUI **191/191** (`pnpm test`, 6
tests nuevos: montaje, teclado→`end` por eje cambiado, drag→dos gestos coordinados, paint
sin eco y sin pelea con el dedo, esquinas desde `modelsState`, composición) · `pnpm build`
en verde · informe de paridad `Tests/nativePanelParityReport.mjs`: *Estructura OK · 70
celdas web · contrato 70*. Sin tocar C++: el contrato de parámetros no cambia, no hay paso
2/9 que regenerar.

**Lo que NO es este paso**: el anillo del valor modulado que dibuja el pad nativo (ghost
ring con la posición base) sigue siendo el fleco abierto de 8.2 — necesita la telemetría del
puente. Y el espectral y el scope del mismo ítem 8.3 siguen esperando el canal de lectura a
30-60 Hz.

## 2026-09-20 (b): Load/Save Preset suben del menú Edit al File

Pedida por el usuario mientras probaba: la barra File/Edit/Help es del editor NATIVO
(`NEURONiKEditor::getMenuBarNames`), no de la página. `Load Preset...` y `Save Preset...`
viven ahora en **File** (entre New Session y Exit, como manda la convención); Edit queda
con Copy/Paste Patch, MIDI Channel, Voices, Zoom y Options. Los IDs de menú NO cambian
(1 y 2), así que `menuItemSelected` queda intacto.

**Hallazgo de la recompilación**: el editor lleva la WebUI EMBEBIDA
(`NEURONiK_WebUIAssets`, el fallback del 8.1) — el exe del 8.2 que probaba el usuario
servía la página vieja incrustada aunque `WebUI/dist` ya tuviera el pad. Recompilado el
Standalone (`--target NEURONiK_Standalone`): exe y embed llevan la página nueva. El VST3
se queda con el menú viejo hasta el próximo `build.bat`.

## 2026-09-20 (c): el teclado "no se distinguía" porque la página se SALÍA del viewport

**Diagnóstico (con captura del usuario):** el keybed compartido está BIEN (marfil sobre
`--kbd-bg` con sus fallbacks — nada que arreglar en `midi-keyb`). Lo que pasaba: la página
es un lienzo de diseño FIJO (1440x990) SIN ningún ajuste al viewport, y el editor es
redimensionable — en la ventana del usuario (~1424x780 CSS útiles tras la barra nativa)
sobraban ~210px por abajo: el pie y CASI TODA la franja del teclado quedaban fuera de
vista. El pad del 8.3 (lienzo 900→990) agravó un corte que ya existía en 8.2.

**Arreglo, en su capa (la página):** `WebUI/src/ui/fitStage.js` — `computeFit` (escala
acotada 0.25x–3x como el zoom nativo, centrado en el eje que sobra) y `mountFitStage`
(transform + margins + resize) sobre `#app` desde `app.js`; `body` con `overflow: hidden`
porque el transform no cambia el box de layout. Es el "escala del contenedor" que el
ROADMAP pone como sustituto del zoom. El componente compartido no se toca.

**Verificación:** WebUI **198/198** (`pnpm test`, 7 tests nuevos de fitStage) · `pnpm
build` verde · contrato de fuente de `app.js` actualizado (import con `CANVAS` + línea del
ajuste). El jsdom no mide layout: lo que se prueba es el CÁLCULO (escala/offsets/acotas)
y el ciclo de vida del listener de resize.

## 2026-09-20 (d): NEURONiK, primer consumidor del fondo tintable de la suite

**Que:** la WebUI adopta el mecanismo `.abd-theme-bg` de `@abdsynths/shared` (hoja nueva
`styles/components/backgrounds.css` del paquete): clase en el `body` de `index.html` e
import del css en `app.js`. Cero configuracion local: el tinte por defecto del mecanismo
es `var(--color-bg-base)` (#0a0e14) — el tema de tokens manda, como se diseno. La textura
entra por `soft-light` y se ve en los huecos entre tarjetas y en el letterbox del fit
(`#app` no pinta fondo propio); las tarjetas siguen opacas. El asset servido es
`bg_neutral.webp` SIN perdida (1.9 MB, pixel-exacto al PNG master de 2.8 MB): la lossy se
descarto con metricas — el degradado suave del grano se cuantiza en mesetas (banding) a
cualquier calidad, incluida q100. Vite emite el webp al dist (`bg_neutral-*.webp`).

**Verificacion:** WebUI **199/199** (1 `it` nuevo en appContract: clase en body + import
del css) · `pnpm build` verde con el webp en dist · Standalone recompilado (`--target
NEURONiK_Standalone`): exe y embed llevan la pagina con fondo. El VST3, hasta el proximo
`build.bat`. Ajuste fino de tinte por tema (`--abd-bg-tint`) pendiente de decidir temas.


## 2026-09-20 (e): la jornada se cierra en cinco commits temáticos

**Qué:** todo el trabajo del día quedó commiteado en ABDNeural. Los ficheros compartidos
entre temas (`app.js`, `appContract.test.js`, `main.css`, `HANDOFF.md`) se trocearon hunk
a hunk por tema con staging quirúrgico (`git hash-object` + `update-index` sobre versiones
intermedias verificadas en `.git/tmp-stage/`, sin tocar el árbol de trabajo):

- `791ed02` docs: README operativo, handoff registro corrido, roadmap viejo a stub, legado del piloto
- `f98ff5d` feat(webui): pad XY dibujado para morphX/morphY (8.3 paso 1)
- `b4d4e68` feat(editor): Load/Save Preset suben del menú Edit al File
- `3d95e88` fix(webui): ajuste del lienzo de diseño al viewport (fitStage)
- `5d0e412` feat(webui): primer consumidor del fondo tintable (.abd-theme-bg)

`git status` limpio y `git diff HEAD` vacío tras el cierre. El ROADMAP se puso al día en
la misma pasada: fila del zoom (el "escala del contenedor" prometido ya es fitStage),
nota de la reorganización del menú nativo y casilla de diálogos/menú anotada.

**Pendiente abierto:** `ABDSharedAssets` SIN commitear (~89 entradas: XYPad con `corners`,
`backgrounds.css`, `COMPONENTS.md`, contracts… mezcladas con trabajo previo sin trackear);
el VST3 sigue con el binario anterior hasta el próximo `build.bat`; del 8.3 quedan browser
de presets, LCD + menú MIDI, espectral/scope (necesitan canal de telemetría), MIDI learn y
menú/diálogos web; flecos: anillo de valor modulado (8.2) y tinte del fondo por decidir.


## 2026-09-20 (f): apuntado para más adelante — modo claro desde un menú "View"

**Idea del usuario, sin diseñar todavía:** un modo claro que se conmute desde un punto de
menú **"View"** en la nav-bar, como en otros synths nuestros. Referencia de temas: el
`themes.css` de ABDMS2000 (`data-theme` + tokens). Encaja con lo que ya existe: el fondo es
tintable por tema (`backgrounds.css`), así que el grueso del trabajo es un juego de tokens
claros + el ítem de menú, no CSS nuevo. Pendiente de decidir: si "View" es menú web nuevo
o entra en la casilla de diálogos/menú del 8.3 (ahí quedó apuntado en el ROADMAP).


## 2026-09-20 (g): ModelMaker sin C4996 — export a AudioFormatWriterOptions (y una mina en build.bat)

**Migración JUCE 8:** el export de audio del ModelMaker usaba la sobrecarga deprecada
`AudioFormat::createWriterFor(OutputStream*, ...)` (el C4996 del log del 18/09). Ahora:
`AudioFormatWriterOptions` (`withSampleRate/withNumChannels/withBitsPerSample`) y el stream
viaja en `unique_ptr` — la propiedad pasa al writer si abre; si falla, lo libera el scope
(la API vieja lo borraba por dentro). Solo `Source/ModelMaker/MainComponent.cpp` (~257): el
import no toca API deprecada — `AudioFormatManager::createReaderFor(const File&)` está
limpio en JUCE 8. Verificado dos veces: target suelto (cero C4996) y `build.bat modelmaker`
completo → RESULTADO: OK, 21/21 ctest, selftest de las seis direcciones en plugin Y bancada.
El bump de Version.h que deja la verificación (28→31 en tres builds) se descarta, como
aconseja el propio script.

**Mina preexistente en build.bat (fix incluido):** el bloque de aviso "WebUI dist no
existe" tenía un `)` sin escapar dentro del `if` (`echo ... interfaz). Selftest...`): al
PARSEAR el bloque, cmd cerraba el if ahí y el `.` siguiente lo mataba ("No se esperaba . en
este momento.", EXIT 255) ANTES del selftest de la bancada — sin banner de RESULTADO. Pasaba
en TODA pasada completa desde el endurecimiento del 09-19 (el grep de AVISO demostró que el
bloque nunca llegó a ejecutarse: moría al parsearlo). Escapado `^(...^)`; un scan del resto
de echoes con paréntesis no encontró más casos dentro de bloques.


## 2026-09-20 (h): modo claro de la SUITE diseñado en tokens.css (sin menú)

**Qué:** paleta clara completa en `[data-theme="light"]` del paquete compartido — el MISMO
juego de tokens de color/sombra del oscuro, con contraste WCAG medido (text-main 15.7-16.5,
text-muted 6.9-7.3, accent 5.4-5.7 como texto y 5.7 el blanco sobre accent). Decisiones de
diseño: el LCD NO cambia (autoiluminado como el hardware), LED/estados oscurecidos para
superficies claras, sombras suavizadas y profundidad invertida (elevado = más claro). Solo
color/sombra: tamaños/espaciados/fuentes se heredan — el test de contrato
(`tests/tokens.test.js`, 5 tests) vigila ambas direcciones: cobertura completa y sin
invenciones. De paso entraron al :root dos tokens que `widgets.css` consumía y NO existían
(`--color-bg-elev`, `--color-border`; valores dark bit-exacto a sus fallbacks). El fondo
tintable sigue al tema solo: el tinte por defecto es `var(--color-bg-base)` y
`backgrounds.css` re-resuelve el tinte en el elemento tematizado — funciona con
`data-theme` en `<html>` (MS2000) o en `<body>` (el demo, que ahora tiene botón "Light").
El menú "View" que lo conmute sigue pendiente: apuntado en el ROADMAP, fuera del DoD 8.3.

**Verificación:** ABDSharedAssets **62/62** (5 tests nuevos) · NEURONiK WebUI **199/199**
(sin cambios: consume tokens.css por import) · contraste medido con script (WCAG 2.1).


## 2026-09-20 (i): selector Dark/Light en la cabecera - el interruptor es compartido, el tema es de la suite

**Que:** para poder PROBAR el modo claro, la cabecera de la WebUI lleva ahora un selector de
temas montado con el `ThemeSwitcher` NUEVO del paquete compartido (`components/
themeSwitcher.js`, exportado por el barrel): dos temas, `dark` (el `:root`, se aplica SIN
atributo) y `light` (el bloque de tokens disenado hoy). Va en la esquina derecha, junto al
grupo de audio; SIN persistencia a proposito: cada carga arranca oscuro, asi selftest y
paridad nunca heredan el estado de una prueba manual. El menu "View" de navegacion sigue
siendo otra pieza (apuntado en el ROADMAP); este es el interruptor de prueba.

**Arquitectura (principio fijado por el usuario):** el interruptor es UNIVERSAL (paquete),
los temas son SOLO tokens de color, y todo lo demas (widgets, skins de forma, fondo
tintable, LCD, ajuste al viewport) es de la suite: un synth nuevo define sus bloques
`[data-theme=...]` y elige tipos de elemento, sin copiar CSS de widgets. ABDMS2000 aun
reparte su tema entre su `themes.css` local y el paquete: deuda conocida, sin tocar hoy.
`COMPONENTS.md` fija el principio.

**Verificacion:** ABDSharedAssets **68/68** (6 tests nuevos del switcher: data-theme en el
root elegido, dark=sin atributo, aria/estado activo, persistencia opcional, ids duplicados,
destroy con removeEventListener real) - NEURONiK WebUI **200/200** (1 it nuevo de contrato) -
`pnpm build` verde - paridad **70 celdas · contrato 70** (el selector no es celda de
parametro) - Standalone recompilado con el embed nuevo.


## 2026-09-20 (j): indice de commits del dia — donde esta TODO

**ABDNeural (10 commits):** `791ed02` docs (README/handoff/roadmaps/legado) · `f98ff5d` pad XY
8.3 · `b4d4e68` menu File · `3d95e88` fit al viewport · `5d0e412` fondo tintable · `fb419b4`
ModelMaker JUCE 8 · `850dd96` fix de build.bat · `9fbc417` selector Dark/Light · `3090370`
docs de la segunda mitad. Arbol limpio.

**ABDSharedAssets (5 commits, primera vez que se commitea la familia):** `62b36d1` ignore
node_modules · `fb32e9b` contracts esquema 2.0 + nuevos · `238c7b0` familia de controles +
tests (68) + ThemeSwitcher · `d1869f0` tokens claros + fondo tintable + audiolab/teclado ·
`4ebeb14` assets (fondos, renders, iconos) + COMPONENTS.md + demo. Fuera a proposito:
`abdbank/` (app completa dentro del paquete, probablemente extraviada: ABDBankManager ya es
repo propio — decidir su destino).

**Estado de la suite:** modo claro disenado y con contrato, selector universal, fondo
tintable, pad con corners — todo heredable por los synths definiendo solo tokens de color.
Pendientes del roadmap: 8.3 (browser, LCD, espectral+telemetria, MIDI learn, menu web),
"View" para el tema, fitStage por extraer al paquete, deuda de temas de ABDMS2000.

