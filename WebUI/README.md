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
- **Shell de la UI**: pestañas, control base, pantalla GENERAL y teclado compartido, con la
  suite en verde (**81 tests**).
- **Nada de esto está cableado todavía.** El host (`NEURONiK Web Pilot.exe`), `build.bat`,
  `start.bat` y CMake siguen sirviendo **el piloto React** (`WebPilot/out`). Cambiar quién
  hospeda la página es el ticket **8.1** (en el editor del plugin, no en la bancada del
  piloto) y eso va **antes** de cerrar 8.2. Esta carpeta compila a `WebUI/dist`, que hoy no
  consume nadie.

## Cómo se ejecuta

```bash
cd ABDNeural/WebPilot && pnpm install   # esta carpeta es miembro del workspace de WebPilot
cd ../WebUI && pnpm test                # vitest (jsdom)
cd ../WebUI && pnpm dev                 # servidor de desarrollo en el navegador (modo local)
cd ../WebUI && pnpm build               # -> WebUI/dist
```

Sin host (`window.__JUCE__` ausente) la página arranca en **modo local**: el store
funciona, pero ningún envío sale al plugin ni se pinta estado nativo.

## Qué hay aquí (y de dónde viene)

| Aquí | Origen | Qué es |
|---|---|---|
| `src/contracts/parameters.js` | `WebPilot/lib/parameters.js` | Adaptador del contrato generado: lookup, clamp, snap, `to/fromNormalized` (math de `NormalisableRange`), formateo, defaults y validación. |
| `src/contracts/paramValue.js` | `WebPilot/lib/paramValue.js` | Plomería normalizado ↔ unidades reales para un control. |
| `src/contracts/paramStore.js` | `WebPilot/lib/useParameterControls.js` | El pegamento del hook convertido en store vainilla: `getState()` / `subscribe()`, gestos, presets, MIDI y modelos. |
| `src/contracts/screens.js` | nuevo | Qué ids tiene cada pantalla, como datos (BRIDGE + GENERAL + el teclado). |
| `src/bridge/bridgeCore.js` | `WebPilot/lib/bridge.js` | Transporte del bridge WebView2 (contrapartida JS de `WebPilot/contracts/bridge-protocol.json`). |
| `src/wasm/audioParams.js` | `WebPilot/lib/audioParams.js` | Contrato → índices de `GlobalParams` del motor WASM (página **fuera** del plugin). |
| `src/ui/panel.js` | nuevo | Shell: pestañas, control base, filas de GENERAL y el footer con el estado. |
| `src/ui/keyboard.js` | nuevo | El teclado compartido (`@abdsynths/midi-keyb`) y su API de feedback desde el host. |
| `src/app.js` | nuevo | Arranque: monta panel y teclado, conecta el store. |

El **contrato generado** no se copia: `src/contracts/parameters.js` importa
`WebPilot/generated/parameters.generated.js`, que sigue siendo la única copia (la escribe
`NEURONiK_ParameterExport` en el paso 2/9 de `build.bat`). Cuando el piloto se retire
(8.4), ese directorio se muda aquí y el import es de una línea.

## El contrato con el host (no "simplificar")

El `--selftest` del host lee la página por selectores. Tres de ellos son contrato, y cada
uno tiene su test:

| Selector / handle | Quién lo lee | Test |
|---|---|---|
| el PRIMER `input[type=range]` = `#masterLevel` | `NATIVE -> JS` y `JS -> NATIVE` del selftest | `tests/panel.test.js`, `tests/keyboard.test.js` |
| `footer.panel-footer code` (JSON del estado normalizado) | comprobación GENERAL del selftest (11 ids) | `tests/panel.test.js` |
| `[data-tab="keys"]` y `#mod-wheel-container .kbd-wheel-slider` | comprobación MIDI del selftest | `tests/keyboard.test.js` |
| `window.__pilotReady`, `window.__pilotSendMidi` | métricas de arranque y MIDI del selftest | `tests/paramStore.test.js` |

El caso más frágil es el primero: **el teclado también monta inputs `type=range`** (las
ruedas), así que el orden de las pantallas (BRIDGE antes que KEYS) es lo que mantiene al
slider del control base en cabeza. Si alguien reordena las pestañas, el selftest falla.

## Qué NO está cableado (a propósito)

- **`build.bat` sigue construyendo el piloto React.** El paso 5/9 exporta `WebPilotVite`
  a `WebPilot/out`, que es lo que embebe el host y lo que sirve `start.bat`. Esta carpeta
  compila a `WebUI/dist` e **no** entra en ese circuito.
- **La paridad de 8.2 no está hecha**: la pantalla GENERAL lista los 11 ids con su valor
  real (leído del contrato), pero **sin widgets** — los knobs/sliders/toggles de la familia
  compartida son el trabajo de 8.2, igual que el reparto por pestañas del panel nativo.
- **Dentro del plugin el audio es NATIVO** (8.1): la página habla por el bridge (APVTS) y
  el motor WASM del worklet es para la página en el navegador. Dos motores sonando a la vez
  no es un caso soportado.
