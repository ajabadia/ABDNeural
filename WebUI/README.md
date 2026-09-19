# WebUI — interfaz definitiva de NEURONiK

JS **vainilla** (sin framework) para correr sobre **WebView2**, con los componentes y el
tema de `@abdsynths/shared`. Es la interfaz de la **Fase 8** del `ROADMAP.md`: sustituye
a los paneles JUCE nativos (`Source/UI/**`), que se retiran en el ticket 8.4.

`ABDMS2000/WebUI/` se usa **solo como referencia de arquitectura** (módulos de puente,
contrato y UI por separado). No hay código compartido con ese proyecto: cada plugin tiene
su propia carpeta.

## Estado (2026-09-19)

- **Portado del piloto React** (sin cambios de comportamiento): el puente, el adaptador del
  contrato, la plomería de valores, el mapeo al worklet y el store que sustituye al hook.
- **Lienzo único (8.2, 2026-09-19)**: los **70 parámetros** repartidos en 7 fichas sobre un
  plano de 1440×900, sin pestañas, con la familia de controles compartida. Encaje medido (no
  estimado): 0 px de desborde. Ver «El lienzo» más abajo.
- **Política de audio (8.1)**: una sola señal decide quién suena; dentro de un host el motor
  del worklet no arranca. Incluye el motor portado (ciclo de vida del `AudioContext`).
- Suite en verde: **185 tests en 16 ficheros** (`cd WebUI && pnpm test`).
- **Cableado (8.1, 2026-09-19): esta carpeta ES la interfaz del plugin.** `NEURONiKEditor`
  monta `Source/WebUI/NeuronikWebView.h`, que sirve `WebUI/dist` desde una copia **embebida**
  en el binario (`juce_add_binary_data` → `NEURONiK_WebUIAssets`). `build.bat` la construye en
  su paso 4/9, antes de compilar el plugin (que la embebe en el enlace). El panel nativo de
  JUCE ya no se monta: la página lo sustituye, y `Source/UI/**` se borra en 8.4.
- **El piloto ya no existe (retirado 2026-09-19).** Su última conclusión sin integrar (el
  selftest) está **dentro** del plugin desde el paso 2c: vive en `Source/WebUI/BridgeSelftest.h` y
  lo corre el editor — el Standalone con `--selftest`, cualquier formato con `NEURONIK_SELFTEST=1`
  — con la bancada usando el MISMO arnés, no una copia. Sus tres SSOT se mudaron **a esta
  carpeta**: el contrato generado (`generated/`), el protocolo versionado (`contracts/`) y los
  assets + worklet (`public/`, que es el `publicDir` de `vite.config.js`). Ver `HANDOFF.md`,
  «Retirada del piloto».
- **Workspace propio.** `pnpm install` se corre **aquí dentro**: hasta la retirada, los enlaces de
  `node_modules/@abdsynths/*` los daba el workspace anidado del piloto, y este proyecto estrena el
  suyo (`WebUI/pnpm-workspace.yaml` + `WebUI/pnpm-lock.yaml`), porque el workspace raíz de la
  suite (`ABDSynths/pnpm-workspace.yaml`) no lista esta carpeta.

## Cómo se ejecuta

```bash
cd ABDNeural/WebUI && pnpm install  # workspace propio (incluye los paquetes compartidos)
cd ABDNeural/WebUI && pnpm test     # vitest (jsdom)
cd ABDNeural/WebUI && pnpm dev      # servidor de desarrollo en el navegador (modo local)
cd ABDNeural/WebUI && pnpm build    # -> WebUI/dist (assets + worklet desde public/)
```

Sin host (`window.__JUCE__` ausente) la página arranca en **modo local**: el store
funciona, pero ningún envío sale al plugin ni se pinta estado nativo.

## Qué hay aquí (y de dónde viene)

| Aquí | Origen | Qué es |
|---|---|---|
| `src/contracts/parameters.js` | `WebPilot/lib/parameters.js` | Adaptador del contrato generado: lookup, clamp, snap, `to/fromNormalized` (math de `NormalisableRange`), formateo, defaults y validación. |
| `src/contracts/paramValue.js` | `WebPilot/lib/paramValue.js` | Plomería normalizado ↔ unidades reales para un control. |
| `src/contracts/paramStore.js` | `WebPilot/lib/useParameterControls.js` | El pegamento del hook convertido en store vainilla: `getState()` / `subscribe()`, gestos, presets, MIDI y modelos. |
| `src/contracts/sections.js` | nuevo | **Reparto y geometría del lienzo**: fichas, ids de cada una, bandas y la cuenta del encaje (`canvasHeight()`). |
| `src/contracts/screens.js` | nuevo | Ids del lienzo (`SCREEN_PARAMETER_IDS`) y los anclajes del host (`GENERAL_PARAMETER_IDS`, `KEYS_TAB_SELECTOR`). |
| `src/bridge/bridgeCore.js` | `WebPilot/lib/bridge.js` | Transporte del bridge WebView2 (contrapartida JS de `WebUI/contracts/bridge-protocol.json`). |
| `src/wasm/audioParams.js` | `WebPilot/lib/audioParams.js` | Contrato → índices de `GlobalParams` del motor WASM (página **fuera** del plugin). |
| `src/audio/audioWorkletEngine.js` | `WebPilot/lib/audioWorkletEngine.js` | Ciclo de vida del `AudioContext` + worklet y los mensajes al DSP, **con la guarda de la política de audio**. |
| `src/audio/policy.js` | nuevo | Quién posee el audio (regla de 8.1): nativo dentro de un host, worklet en el navegador. |
| `src/ui/panel.js` | nuevo | El lienzo: cabecera, bandas de fichas, franja de teclado y el pie con el estado. |
| `src/ui/controls.js` | nuevo | Una celda de parámetro con la familia compartida (`Knob`/`Toggle`/`Select`), en las dos escalas de valor, con el gating por motor. |
| `src/ui/drawer.js` | nuevo | El cajón lateral deslizante (patrón ABDMS2000/ABDEep): abrir/cerrar es una clase, el contenido NO se reconstruye. |
| `src/ui/modSummary.js` | nuevo | Resumen de las 4 rutas de modulación: la vista que queda en el lienzo mientras los controles viven en el cajón. |
| `src/ui/visuals.js` | nuevo | Fábrica de vistas de ficha (ADSR, resumen de la matriz), compartida por `app.js` y la suite del panel. |
| `src/ui/keyboard.js` | nuevo | El teclado compartido (`@abdsynths/midi-keyb`) y su API de feedback desde el host. |
| `src/app.js` | nuevo | Arranque: monta panel y teclado, conecta el store. |

El **contrato generado** no se copia: `src/contracts/parameters.js` importa
`../../generated/parameters.generated.js`, que es la única copia (la escribe
`NEURONiK_ParameterExport` en el paso 2/9 de `build.bat`, que ya apunta a esta carpeta). El
directorio se mudó aquí con la retirada del piloto:
antes vivía en `WebPilot/generated/` y el import atravesaba el repo.

`contracts/bridge-protocol.json` (el contrato versionado del protocolo) y `public/` (los assets
compartidos y el worklet) llegaron en la misma mudanza; `public/` es el `publicDir` de Vite, así
que el worklet que compila `build_wasm.bat` acaba en `dist/worklet/` sin pasos intermedios.

## El contrato con el host (no "simplificar")

El selftest de seis direcciones (`Source/WebUI/BridgeSelftest.h`, ticket 8.1 paso 2c) lee la
página por selectores. Son contrato, y cada uno tiene su test **y** su gemelo anti-drift en el
lado C++ (`Tests/webuiSelftestContractTest.mjs`, ctest):

| Selector / handle | Quién lo lee | Anclaje C++ | Test |
|---|---|---|---|
| el PRIMER `input[type=range]` = `#masterLevel` | `NATIVO -> JS` y `JS -> NATIVO` del selftest | `SelftestPage::firstRange` | `tests/panel.test.js`, `tests/keyboard.test.js` |
| `footer.panel-footer code` (JSON del estado normalizado) | comprobación GENERAL del selftest (11 ids) | `SelftestPage::stateCode` | `tests/panel.test.js` |
| `[data-tab="keys"]` y `#mod-wheel-container .kbd-wheel-slider` | comprobación MIDI del selftest | `SelftestPage::keysTab`, `SelftestPage::modWheelSlider` | `tests/keyboard.test.js`, `tests/panel.test.js` |
| `.model-slots__row` y `.model-slots__name` (ficha MODELOS A-D) | comprobación MODELOS del selftest (los 4 nombres cargados) | `SelftestPage::modelSlotRow`, `SelftestPage::modelSlotName` | `tests/modelSlots.test.js` |
| `[data-drawer-trigger]`, `.drawer--open`, `.drawer-slot` y `.drawer-backdrop--visible` (cajón lateral) | comprobación MATRIZ del selftest (abre el cajón y lee la ruta 1) | `SelftestPage::drawerTriggerAttribute`, `openDrawerClass`, `drawerSlotClass`, `visibleBackdropClass` | `tests/drawer.test.js`, `tests/panel.test.js` |
| `[data-parameter-id="id"]` (la celda de un control) | comprobación MATRIZ: la fuente y el destino se leen de su `<select>`, la cantidad del dial | `SelftestPage::parameterCellAttribute` | `tests/panel.test.js`, `tests/drawer.test.js` |
| `window.__pilotReady`, `window.__pilotSendMidi` | métricas de arranque y MIDI del selftest | `SelftestPage::midiHelper` | `tests/paramStore.test.js` |

Las **seis direcciones son obligatorias en las dos superficies**, y desde la retirada del piloto
no hay forma de omitir ninguna: la maquinaria que lo permitía (`BridgeSelftest::PageCapabilities`,
`retiredPilotPage()`, `--pilot-page`) se fue con la página retirada, que era su único usuario.
`Tests/webuiSelftestContractTest.mjs` no solo comprueba que no se use: **prohíbe** que vuelva
(ni las dos APIs, ni las marcas de omitido de MATRIZ y MODELOS).

La bancada y el plugin sirven la **MISMA** página (`WebUI/dist`) y corren las mismas seis
direcciones; lo único que cambia entre ellas es quién hospeda el `WebBrowserComponent`.

El caso más frágil es el primero: **el teclado también monta inputs `type=range`** (las
ruedas), así que el orden del documento es lo que mantiene al slider del control base en
cabeza: el fader va en la ficha GLOBAL & MASTER (primera banda) y las ruedas en la franja de
abajo. Si alguien monta el teclado antes que el lienzo, el selftest falla.

`[data-tab="keys"]` ya **no es una pestaña**: el lienzo único no tiene. El atributo se conserva
porque es el anclaje que el host pulsa (ahora pliega/despliega la franja de teclado, sin
desmontarla: el host lee la rueda ahí dentro).

Además de los selectores, el selftest comprueba que los **11 ids de GENERAL** estén en el
estado de la página: el guard anti-drift compara la lista del C++ (`SelftestPage::generalIds`)
con `GENERAL_PARAMETER_IDS` de `src/contracts/screens.js`, para que añadir un id en un solo lado
no pueda pasar.

## Política de audio (8.1) — una sola señal, dos consumidores

NEURONiK embarca el mismo DSP dos veces: el motor **nativo** del plugin y el módulo **WASM**
que corre en un AudioWorklet. Los dos a la vez no es un caso soportado (voces dobles, FX con
fase rara), así que la regla queda escrita en un sitio:

| Dónde corre la página | Quién pone el audio | Cómo se ve |
|---|---|---|
| Dentro de un host JUCE (editor del plugin, bancada WebView2) | El **plugin** | Solo un letrero: `AUDIO: motor nativo del plugin`. Sin botón. |
| Navegador (sin `window.__JUCE__`) | La **página** | Botón `SOUND ON` → AudioWorklet con el WASM. |

La señal es `window.__JUCE__` —la misma que usa el puente para saber si hay host, leída una
sola vez en `bridgeCore.js::nativeBackend()`—, así que la política **no puede** discrepar del
estado del bridge. `startAudioEngine()` consulta la guarda antes de nada: dentro de un host
devuelve estado `blocked` y no llega a construir un `AudioContext`.

## Servir la página desde disco mientras se itera (dev)

El plugin embebe `WebUI/dist`, así que sin nada más cada retoque de CSS pediría recompilar el
plugin. Para el bucle rápido (y para lo que queda de 8.2) está el override de desarrollo,
apagado por defecto:

```bat
set NEURONIK_WEBUI_DEV_DIR=D:\desarrollos\ABDSynths\ABDNeural\WebUI\dist
```

Con esa variable, el plugin sirve del disco y lo que no encuentre cae a la copia embebida; sin
ella no toca el disco. Ninguna ruta va grabada en el binario (el VST3 funciona copiado a
cualquier sitio), así que el «cero rutas absolutas» del DoD se mantiene: es una decisión de
arranque, no una dependencia del build.

## El lienzo (8.2): un solo plano, y su encaje medido

NEURONiK se pinta en **un solo lienzo de 1440×900**, sin pestañas de parámetros: 7 fichas en
3 bandas de 12 carriles, todas a dos filas, siguiendo la agrupación del panel nativo
(OSCILADOR + RESONADOR + GLOBAL / FILTRO&ENV + EFECTOS / LFO + MATRIZ), más la franja de
teclado fija
abajo. Los hermanos de la suite no lo hacen así — ABDMS2000 (editor 1080×680) y ABDEep
(1200×768) reparten en cajones (`slideDrawer`) —, pero con 70 parámetros el cajón no hacía
falta: caben, y se comprobó antes de escribirlo.

- **El reparto es DATO**, no marcado: `src/contracts/sections.js`. Añadir una ficha o mover un
  parámetro de sitio es editar un array; el panel recorre ese dato.
- **El encaje es un TEST**: `tests/sections.test.js` calcula el alto con la misma geometría que
  usa la CSS (bordes y huecos incluidos), exige que quepa en 900 px y que la CSS declare las
  mismas medidas. Si una ficha crece una fila, falla aquí.
- **Y se midió en un motor real** (iframe + Chrome headless a 1440×900, arnés temporal ya
  borrado): `scrollHeight == clientHeight == 900` y `scrollWidth == clientWidth == 1440`, o sea
  **cero desborde y cero barras**; 70 celdas (45 knob / 5 toggle / 19 desplegable), 36 teclas y
  la rueda de modulación montadas, y el primer `input[type=range]` sigue siendo `masterLevel`.
  jsdom no calcula layout, así que esto no lo puede decir la suite: es lo que cazó los 33 px de
  desborde que la primera cuenta (sin bordes ni huecos de fila) no veía.
- **Tipo por descriptor**: `float` → `Knob`, `bool` → `Toggle`, `choice` → `Select`. Los cuatro
  salen de la familia compartida (el hueco de la lista se cerró el 2026-09-19 con
  `@abdsynths/shared/components/select.js`); el único control fuera de ella es el `range` nativo
  de `masterLevel`, que es el control base del selftest del host (8.1 paso 2c).
- **Gating por motor en las 4 listas de destinos**: el contrato trae `optionEngines` (un motor por
  opción) y `engineParameter`; la celda deshabilita lo que el motor activo no consume, explica el
  motivo en la opción (`title`) y **no reescribe el valor** — lo marca
  (`.abd-select[data-divergent]`), que es lo que el timer del panel nativo hacía mal. Sin el motor
  en el snapshot no se aplica gating: no se inventa cuál está activo.
- **La matriz de modulación vive en un CAJÓN**: una ficha puede declarar `drawer` en el reparto y
  sus celdas se montan dentro (`src/ui/drawer.js`), agrupadas por ruta; en el lienzo quedan el
  resumen de las 4 rutas (`src/ui/modSummary.js`) y el botón. El cajón cuelga del documento
  (`position: fixed`) como en los hermanos, pero **no reconstruye el contenido al abrir**: las 70
  celdas están siempre en el documento (el selftest y la suite cuentan celdas, y reconstruir
  perdería un gesto en curso). Los `ids` siguen en `sections.js`, así que el store y la cobertura
  no cambian: lo que cambia es DÓNDE se pinta cada celda.
- **Acciones y vistas de ficha son DATO**, no casos especiales del panel: `SECTION_ACTIONS`
  (RANDOM, en la cabecera de GLOBAL & MASTER: pide `randomize` al host, que es quien tiene los
  rangos y los congelados) y `SECTION_VISUALS` (la curva ADSR de FILTRO & ENVOLVENTE y el resumen
  de la matriz) viven en `contracts/sections.js` y las resuelve `src/ui/visuals.js`; el panel solo
  pinta lo que recibe. Esa fábrica vive en su módulo, y no dentro de `app.js`, porque el harness de
  `tests/panel.test.js` la duplicaba y montaba una curva para cualquier vista declarada.
- **La curva ADSR** (`src/ui/envelopeCurve.js`) ocupa la celda LIBRE de su ficha (11 controles en
  6x2), así que no mueve la geometría; no es una celda de parámetro (`.card__visual`), y comprime
  los tiempos con √ para que 1 ms y 5 s se lean en el mismo ancho.
- **Las ranuras de modelo A–D** (`src/ui/modelSlots.js`, ficha MODELOS) no son parámetros: una
  ranura es del MOTOR (un preset lleva `modelPath<slot>`, no una copia de los parciales), así que la
  ficha no ocupa celda. El nombre cargado llega en `modelsState.name` (la página no puede leer el
  disco) y **CARGAR pide la acción `loadModel` al host**, que es el único que puede abrir un
  diálogo: la respuesta llega DESPUÉS, como `modelsState` fresco o como `modelError` (cancelado,
  fichero inservible, ranura fuera de rango — nunca en silencio). Un nombre con `isValid: false` se
  marca en ámbar, no se oculta, y sin host los cuatro botones están deshabilitados.

## Qué NO está hecho (a propósito)

- **De 8.2 queda el anillo** del valor modulado (abajo).
- **El anillo del valor modulado**, y con el dato localizado: el procesador publica la modulación
  viva (`getModulationValueForUI()`), pero **no viaja en el cable** — el protocolo del puente no
  tiene canal de modulación. Hacerlo bien es un cambio de frontera (controlador y mensaje aditivo
  tipo `midiNoteState` + poll del host + estado en el store + el anillo en el `Knob` compartido).
  Se deja fuera a propósito: un anillo con la CANTIDAD del slot no es el valor modulado.
- **La retirada del piloto** ya está hecha (2026-09-19): las tres SSOT se mudaron aquí, el
  piloto React (`WebPilot/`, `WebPilotVite/`) se borró y la bancada perdió su camino del piloto
  (`--pilot-page`) y su snapshot embebido. El arnés ya no es una de las deudas: desde el paso 2c
  corre en el editor y la bancada usa el mismo. El E2E de la política de audio queda cerrado en el
  Standalone (las seis direcciones OK, exit 0: MODELOS —los cuatro `.neuronikmodel` entran por el
  puente y los nombres se leen en la ficha— y MATRIZ —se configura una ruta, se abre el cajón y se
  lee la ruta de sus controles—, con las OTRAS cinco corriendo con el cajón abierto); el VST3 con
  un host dentro es 8.5.
- **La barra de menú del editor es provisional** (preset, canal MIDI, voces, zoom, ayuda): se
  la lleva 8.3 a la propia página.
