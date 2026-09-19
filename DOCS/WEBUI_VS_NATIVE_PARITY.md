# Lienzo web vs. superficies nativas — A/B con el mismo preset

Comparación del **lienzo de la WebUI** (8.2) con las **superficies nativas** de C++,
cargando el mismo preset. Se reproduce con:

```bash
cd ABDNeural
node Tests/nativePanelParityReport.mjs                       # init del contrato + informe completo
node Tests/nativePanelParityReport.mjs --quiet               # resumen (lo que corre ctest)
node Tests/nativePanelParityReport.mjs --preset "ruta/X.neuronikpreset"
```

El script extrae los DOS inventarios de sus fuentes y los cruza id a id, así que cada
número de aquí se puede volver a comprobar:

- **nativo**: patrones de `Source/UI/**` (`setupControl`, `setupChoice`,
  `setupAmountControl`, `setupRotaryControl` y los `...Attachment`), con los bucles de
  `ModulationPanel` expandidos (2 LFO × 5 controles y 4 rutas × 3);
- **web**: `WebUI/src/contracts/sections.js` (qué id va en cada ficha) + el contrato
  generado (`describeControl`: etiqueta, tipo y opciones).

En ctest corre como **`NEURONiK_NativePanelParity`**, pero solo por sus invariantes duras:
*ningún id del lienzo ni del C++ fuera del contrato*, *el lienzo no pinta dos veces el
mismo parámetro* y *ninguna superficie declarada ha desaparecido del árbol*. Que las dos
superficies se vean distintas es el informe, no un fallo.

> **Estado (2026-09-19, tras la primera retirada).** De las siete superficies nativas que
> comparaba este informe, **cuatro paneles de parámetros, el browser de presets, el LCD y
> los visualizadores sin montar ya no existen**: se compilaban sin que nadie los
> instanciara, y ahí vivían el temporizador que reescribía el APVTS y las etiquetas
> duplicadas a mano. Quedan las tres que sí se montan (GENERAL y XYPad en la bancada, el
> menú del editor). Las secciones 3-8 de este documento describen los paneles borrados y
> se conservan como el **inventario de lo que la web tiene que cubrir**, con las cifras de
aquel momento; la sección 1 es la que sigue viva.

## El preset comparado

"El mismo preset" son los **70 valores normalizados 0..1**: por defecto el **INIT del
contrato** (`defaultNormalized`, lo que carga un plugin recién abierto) y con `--preset`
un `.neuronikpreset` real (XML del APVTS: `<PARAM id=".." value="0..1"/>`). En el repo no
hay presets — viven en los datos de usuario — así que el número por defecto es el INIT.

Resultado con el INIT:

```text
Lienzo web ................. 70/70 del preset visibles (lienzo: 70 celdas, 7 fichas, 1440x900)
Algún control nativo ...... 66/70 (en los cinco paneles y el XYPad)
  montados (bancada+plug) .. 15/70
  montados en el PLUGIN .... 1/70 (el editor que se envía solo monta el WebView)
  en paneles sin instanciar  51/70
```

## 1. Quién está montado y quién no — la diferencia mayor

Tras la retirada del 2026-09-19, el árbol nativo que queda son **tres superficies** y las
tres se montan:

| Superficie nativa | params que pinta | montada en |
|---|---|---|
| GENERAL (`ParameterPanel`) | 12 | bancada (`WebPilotHost`) |
| XY PAD (morph) | 2 | bancada (`WebPilotHost`) |
| menú del editor (canal MIDI) | 1 | **plugin que se envía** |

```text
Lienzo web ................. 70/70 del preset visibles
Algún control nativo ...... 15/70
  montados (bancada+plug) .. 15/70
  montados en el PLUGIN .... 1/70 (el editor que se envía solo monta el WebView)
  en paneles sin instanciar  0/70   <- eran 51/70 antes de la retirada
```

`NEURONiKEditor` monta únicamente `webView`, `noWebUiNotice` y `menuBar`
(`addAndMakeVisible`), así que **en el plugin que se envía el panel nativo prácticamente no
existe**: de los 70 parámetros, **1** (el canal MIDI, en el menú) tiene control nativo
montado. Los 15 del nativo y los 55 que solo ve la página están listados por el script
(`node Tests/nativePanelParityReport.mjs`).

Esto **no es un descuido**: la decisión de la Fase 8 es que la interfaz sea la web y que los
paneles JUCE se **retiren** (8.4), así que lo que quedaba era la **referencia** de paridad, no
superficies vivas. Lo que importa es leerlo al revés — el A/B dice "la página cubre lo que el
nativo cubría y más", y no "las dos UIs se ven igual".

Para el usuario, esto es: cargar un preset en el plugin mueve **todo** por la página, y en el
nativo no habría dónde verlo.

### Lo que se retiró (2026-09-19)

Fuera del árbol y de `CMakeLists.txt` (`Source/UI/Panels/*`, `Source/UI/Browser/*`,
`LcdDisplay.*`, `LcdMenuManager.h`, `SpectralVisualizer.*`, `EnvelopeVisualizer.h`):

- **`Panels/ModulationPanel`**: es el que traía el bug de reescritura (ver abajo).
- **`Panels/{Oscillator,FilterEnv,FX}` y `PresetPanel`, `Browser/PresetBrowser`,
  `LcdDisplay`/`LcdMenuManager`, `SpectralVisualizer`, `EnvelopeVisualizer`**: sin una sola
  instancia en todo el árbol.
- **`ParameterPanel` y `XYPad` se quedan**: son lo que la bancada monta de verdad. El
  `EnvelopeVisualizer` de GENERAL (construido en cada apertura y nunca añadido al panel)
  también se fue, y su RANDOM ya no tiene una copia privada de la tabla de rangos: llama a
  `State/ParameterRandomizer`.

**El bug que se fue con el panel, y su arreglo de verdad (2026-09-19).**
`ModulationPanel::timerCallback` corría cada 100 ms y, si el destino de modulación seleccionado no
valía para el motor activo, escribía **Off** al parámetro (`setSelectedId(1,
juce::sendNotification)`). Un preset guardado con un destino exclusivo del otro motor perdía esa
ruta sin que nadie tocara nada. El filtrado era además **por índice** (`{21,22,23}` /
`{24,25,26,27}`, 1-based sobre un ComboBox, con comentarios que se contradecían entre 0-based y
1-based), atado al orden de `getModDestinations()`.

Con el panel fuera, el filtrado se llevó al **contrato**, que es donde debe vivir: cada destino
nombra el parámetro que mueve (`State/ParameterDefinitions.h`, tabla única en orden de preset), y
su motor sale de `engineCoverageFor()` — así que el gating ya no puede discrepar de la cobertura
que el propio contrato publica. El generador emite `optionEngines` (un motor por opción) y
`engineParameter` (quién elige el motor), y la página deshabilita lo que el motor activo no
consume **sin reescribir el valor**: lo marca. El reparto por índice es exactamente el que tenía
el panel retirado (**2 3 10-16 20 21 22** neuronik, **23 24 25 26** neurotik, 12 de los dos) y el
test de contrato lo pincha, junto con las etiquetas por índice (el índice es estado de preset) y
con que `getModDestinations()` liste la tabla (la lista estaba duplicada a mano en el layout).

## 2. Cuatro parámetros no tienen control nativo en ninguna parte

| id | contrato | ficha web | por qué importa |
|---|---|---|---|
| `oscLevel` | `implemented`, ambos motores | OSCILADOR | lo lee el DSP (`NEURONiKProcessor.cpp:254,286`); el nativo solo lo toca el RANDOMIZE |
| `midiThru` | `implemented`, `engines: host` | GLOBAL & MASTER | `NEURONiKProcessor.cpp:485` (eco MIDI al host) |
| `velocityCurve` | `implemented`, `engines: host` | GLOBAL & MASTER | `NEURONiKProcessor.cpp:406` (curva de velocity) |
| `unisonEnabled` | `notRouted`, `engines: none` | OSCILADOR | retirado: nada lo lee («unison amount comes from detune and spread») |

Los tres primeros son parámetros vivos que **solo** la página expone. El cuarto es el
único del preset que ninguna superficie usa, y **la página lo dice** (`cell--divergent` +
`dspNote` de tooltip) mientras el nativo no marca nada.

## 3. Etiquetas: 41 de los 66 ids compartidos cambian de nombre

El nativo abrevia en mayúsculas en el panel; la página usa el `name` del contrato. La
mayoría son la misma palabra en mayúsculas (`IMPULSE MIX`) o un recorte de ella
(`INHARMONICITY`, `DAMP`, `STRENGTH`), pero **18 ids no se leen como el mismo nombre
recortado**: el nativo dice otra cosa:

| id | nativo | web |
|---|---|---|
| `masterLevel` | `VOLUME` | Master Level |
| `fxSaturation` | `DRIVE` | Saturation |
| `fxReverbSize` | `ROOM` | Reverb Size |
| `resonatorParity` | `PARITY` | Odd/Even Balance |
| `resonatorRolloff` | `ROLL-OFF` | Harmonic Roll-off |
| `resonatorShift` | `SHIFT` | Spectral Shift |
| `unisonDetune` | `DETUNE` | Spectral Detune |
| `unisonSpread` | `SPREAD` | Spectral Spread |
| `filterEnvAmount` | `ENV AMT` | Filter Env Amount |
| `fxDelayTime` | `TIME` | Delay Time |
| `fxDelayFeedback` | `FEEDBACK` | Delay FB |
| `fxDelaySync` | `SYNC` | Delay Sync |
| `fxDelayDivision` | `DIV` | Delay Division |
| `fxReverbDamping` | `DAMP` | Reverb Damping |
| `lfo1RateHz` / `lfo2RateHz` | `Speed` | LFO 1/2 Rate |
| `lfo1Depth` / `lfo2Depth` | `Depth` | LFO 1/2 Depth |

Dos detalles más del nativo: `filterAttack/Decay/Sustain/Release` se llaman `ATTACK`…
igual que los del env de amplitud (ambiguo sin mirar en qué caja están; la ficha web los
desambigua) y `resonatorRes` tiene un **espacio de más** en la etiqueta (`" RESONANCE"`),
que además es el mismo texto que `filterRes`.

## 4. Los valores se leen distinto (mismo número, otro texto)

Los rotativos nativos **tiran el texto del propio parámetro**:

```cpp
// Source/UI/CustomUIComponents.h:276-280
ctrl.slider.textFromValueFunction = [](double v) { return juce::String(v, 2); };
```

→ dos decimales y **sin unidad**: `0.25`, `2400.00`. La página usa `formatValue` del
contrato: unidad cuando la hay (`s`, `Hz`, `%`), decimales según magnitud, nombre de la
opción en los `choice` y `On`/`Off` en los `bool`. Para el mismo preset, un cutoff se lee
`2400.00` en el nativo y `2.40 kHz` en el lienzo.

## 5. Tipo de control: 4 diferencias

`mod1Amount`…`mod4Amount` son **fader horizontal** en el nativo (`LinearHorizontal` +
caja de texto a la derecha) y **knob** en el lienzo. Los otros 62 coinciden en concepto
(knob / `ComboBox` / botón LED ↔ `Knob` / desplegable / `Toggle`).

## 6. Acciones sin par

| acción | nativo | web |
|---|---|---|
| RANDOMIZE (con los congelados `freeze*` como filtro) | sí (`ParameterPanel`, botón «RANDOM», bancada) | **sí desde 2026-09-19**: botón en la cabecera de GLOBAL & MASTER que pide la acción `randomize` al host; el sorteo lo hace `State/ParameterRandomizer` en el procesador (ver más abajo) |
| PANIC (corta el MIDI sonando) | no | sí (franja de teclado) |
| Carga de modelo espectral (`loadA..loadD`) | sí: ficha RESONADOR nativa, un diálogo `*.neuronikmodel` por ranura | **sí desde 2026-09-19**: ficha MODELOS A–D; el botón pide la acción `loadModel` y el diálogo lo abre el HOST (ver más abajo) |
| MIDI learn por control | sí: `UIUtils::setupRotaryControl` crea un `MidiLearner` por rotativo, y hay uno por cada `freeze*` y para el motor | **no** (8.3) |

### RANDOMIZE: de método privado del panel a acción del estado (2026-09-19)

Se podía perder con el panel, así que se movió a `State/ParameterRandomizer` (probado sin
procesador ni UI) y el puente gana la acción `randomize`, sin campos: la fuerza sale del
parámetro `randomStrength` y los congelados del APVTS, así que la página no puede pedir un
sorteo que el instrumento no haga. Dos defectos que se corrigieron de paso, con test:

- **mezclaba unidades**: promediaba el valor actual en REAL con el sorteo en NORMALIZADO
  (`jmap(strength, currentValue, random0to1)`), así que todo parámetro cuyo rango real no
  fuera 0..1 acababa clavado en su máximo (`filterCutoff` saltaba a 20 kHz). Ahora la mezcla
  es lineal en unidades reales y se convierte a normalizado una vez;
- **la tabla se salía del parámetro**: `resonatorRes` pedía desde 0.3 con un rango real
  0.5..1. Ahora el test exige que cada ventana quepa en su parámetro (y el sorteo se recorta
  por si acaso).

Sin host el botón queda **deshabilitado** y el store no finge nada (`randomize()` devuelve
false en modo local): no hay APVTS que sortear.

### Slots de modelo A–D: la única acción que contesta TARDE (2026-09-19)

El nativo tenía cuatro botones `loadA..loadD` en el bloque MODEL
(`OscillatorPanel::buttonClicked`): abrían un `juce::FileChooser` de `*.neuronikmodel`, cargaban
en `processor.loadModel(file, slot)` (slot 0 = A) y **rotulaban la ranura con el nombre del
fichero elegido**. Ese `loadModel` no devolvía nada y salía en silencio si el fichero no era un
modelo válido, así que un fichero inservible dejaba el nombre puesto hasta que el timer de
10 Hz lo corregía leyendo `getModelNames()`. Es decir: la ranura podía decir una cosa y el
motor tener otra, durante un tick.

En el lienzo, las ranuras son la ficha **MODELOS A–D**, y la carga es una acción del puente:

- **`loadModel { slot }`** (JS→nativo) es la única acción cuya respuesta llega DESPUÉS: la
  página no tiene sistema de ficheros y no puede nombrar rutas, así que el host abre el diálogo
  (`EngineModelsAdapter`, el mismo `FileChooser` que tenía el panel) y contesta más tarde.
- **`modelsState`** ahora viaja con **`name`** por ranura ("EMPTY" cuando no hay nada), porque
  el nombre solo lo sabe el motor y las dos superficies tienen que enseñar la MISMA lista.
- **`modelError { slot, detail }`** es la otra mitad: cancelar el diálogo, elegir un fichero que
  no es un modelo o pedir una ranura fuera de rango (o fraccionaria: se rechaza, no se trunca)
  contestan con el motivo. `NEURONiKProcessor::loadModel` pasó a devolver `bool` para poder
  decirlo: antes no había forma de saber si la carga había ocurrido.
- En la página, un nombre con `isValid: false` es un preset que apunta a un fichero que ya no
  está: se **marca** en ámbar (mismo criterio que el gating del `Select`), no se oculta. Sin host
  los cuatro botones quedan deshabilitados y `loadModel()` devuelve false.

Las ranuras son del MOTOR, no del APVTS (un preset lleva `modelPath<slot>`, no una copia de los
parciales), así que la ficha **no ocupa celda**: el encaje de los 70 no cambia.

**Comprobado en el plugin real (2026-09-19).** Que la carga *se vea* es la quinta dirección del
selftest (los cuatro nombres leídos en la ficha de la página del Standalone, con la página de
verdad en su WebView2); que *suene* es `Tests/ModelSlotTest.cpp`, con el procesador real: cada
esquina de morph (A 0,0 · B 1,0 · C 0,1 · D 1,1) suena el parcial de SU ranura y ninguno de los
otros. Esa prueba destapó que el lector solo entendía el dialecto **XML** del formato y que el
ModelMaker escribe **JSON**, así que los ficheros de la propia herramienta no cargaban (ranura
con nombre y sin sonido); ahora se leen los dos dialectos.

## 7. Destinos de modulación: el nativo los filtra por motor, la página no

El contrato marca `engines` por parámetro y la lista de 28 destinos tiene entradas
exclusivas de cada motor. El nativo las **desactiva en un timer de 100 ms**, y lo hace
**por ÍNDICE**:

```cpp
// Source/UI/Panels/ModulationPanel.cpp:160-186
for (int idx : { 21, 22, 23 }) combo.setItemEnabled(idx, isNeuronik);   // NEURONiK
for (int idx : { 24, 25, 26, 27 }) combo.setItemEnabled(idx, !isNeuronik);  // Neurotik
```

La página entrega `engines` en el view-model (`describeControl`) y **nadie lo consume**:
los 28 destinos están siempre disponibles, así que se puede elegir uno que el motor
actual ignore. Además, el gating nativo es frágil por construcción: si la lista de
destinos cambia de orden, esos índices desactivan lo que no toca.

## 8. Visualizadores: el nativo tiene tres, el lienzo ninguno

- `SpectralVisualizer` y `XYPad` (morph) → solo en la bancada; el XYPad es el control
  nativo de `morphX`/`morphY`.
- `EnvelopeVisualizer` → **creado y no montado**: `ParameterPanel.cpp:44-49` lo construye
  y el comentario dice «Visualizer removed from General per user request (no space)».
- La página no tiene ningún visualizador. Consecuencia con un preset cargado: un preset
  lleva **APVTS + hasta 4 slots de modelo espectral** (64 parciales cada uno, fuera del
  APVTS; ver `bridge-protocol.json`, `syncModels`), y en el lienzo esos datos solo
  alimentan al motor WASM — no hay nada que los dibuje.
  > **Actualizado (2026-09-19):** del slot ya se dibuja su **identidad** (letra A–D, nombre
  > cargado y si el motor pudo cargarlo), y se puede cambiar desde la página con la acción
  > `loadModel` (ficha MODELOS A–D, ver la sección 6). Lo que sigue sin dibujarse son los
  > **64 parciales**, que son el `SpectralVisualizer` de 8.3.

## 9. Presets: ninguna de las dos superficies tiene browser

- **nativo**: menú del editor con `Load Preset...` / `Save Preset...` (file chooser,
  `NEURONiKEditor.cpp:238-319`) + `PresetBrowser`/`PresetPanel` compilados y **sin montar**.
- **web**: el store guarda `presetState` y el protocolo soporta `presetList`/`loadPreset`/
  `savePreset` desde la página, pero **ningún módulo lo pinta** (`presetState` solo aparece
  en `paramStore.js` y su test). La página puede *pedir* un preset, pero hoy el usuario no
  tiene dónde elegirlo.

O sea: el preset llega igual a las dos superficies (lo carga el host y la página refleja
los 70 valores), pero **elegirlo hoy solo se puede desde el host**.

## 10. Lo que NO es una diferencia

- **Ids y rangos**: los 70 ids del lienzo están en el contrato, el C++ no enlaza ninguno
  fuera del contrato, el lienzo no repite celdas, y las dos superficies leen el mismo
  APVTS/contrato → el **mismo valor real** para el mismo preset.
- **Opciones de los desplegables**: coinciden letra a letra. Las tres listas que el nativo
  escribe a mano en `ModulationPanel.cpp:119-121` (6 formas de LFO, `Free`/`Tempo Sync`, 9
  divisiones) son exactamente los `choices` del contrato, y fuentes/destinos de la matriz
  salen de `getModSources()`/`getModDestinations()`, que es la misma fuente que genera el
  contrato. Única diferencia cosmética: el combo de motor antepone `Engine: `.
- **Semántica de los `bool`**: botón LED latcheado en ambos (`aria-pressed` ↔ estado del
  `TextButton`).

## Cómo se mantiene

El informe se regenera con el script; si el lienzo o un panel cambian, los números de este
documento hay que volver a sacarlos de ahí. Las dos invariantes (ids fuera del contrato,
celda duplicada) fallan solas en ctest, así que este documento puede quedar desfasado en
cifras pero no en silencio.
