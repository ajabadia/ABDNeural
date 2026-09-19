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
- **Shell de la UI**: pestañas, control base, pantalla GENERAL y teclado compartido.
- **Política de audio (8.1)**: una sola señal decide quién suena; dentro de un host el motor
  del worklet no arranca. Incluye el motor portado (ciclo de vida del `AudioContext`).
- Suite en verde: **96 tests en 11 ficheros**.
- **Cableado (8.1, 2026-09-19): esta carpeta ES la interfaz del plugin.** `NEURONiKEditor`
  monta `Source/WebUI/NeuronikWebView.h`, que sirve `WebUI/dist` desde una copia **embebida**
  en el binario (`juce_add_binary_data` → `NEURONiK_WebUIAssets`). `build.bat` la construye en
  su paso 4/10, antes de compilar el plugin (que la embebe en el enlace). El panel nativo de
  JUCE ya no se monta: la página lo sustituye, y `Source/UI/**` se borra en 8.4.
- **El piloto React sigue vivo solo mientras 8.1 no cierre su paso 2c** (el selftest de cuatro
  direcciones vive en su bancada y hay que portarlo). Después se retira, con la mudanza de las
  tres SSOT que hoy viven en `WebPilot/` (contrato generado, `bridge-protocol.json` y
  `public/`). Ver `ROADMAP.md`, «Retirada del piloto».

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
| `src/audio/audioWorkletEngine.js` | `WebPilot/lib/audioWorkletEngine.js` | Ciclo de vida del `AudioContext` + worklet y los mensajes al DSP, **con la guarda de la política de audio**. |
| `src/audio/policy.js` | nuevo | Quién posee el audio (regla de 8.1): nativo dentro de un host, worklet en el navegador. |
| `src/ui/panel.js` | nuevo | Shell: pestañas, control base, filas de GENERAL y el footer con el estado. |
| `src/ui/keyboard.js` | nuevo | El teclado compartido (`@abdsynths/midi-keyb`) y su API de feedback desde el host. |
| `src/app.js` | nuevo | Arranque: monta panel y teclado, conecta el store. |

El **contrato generado** no se copia: `src/contracts/parameters.js` importa
`WebPilot/generated/parameters.generated.js`, que sigue siendo la única copia (la escribe
`NEURONiK_ParameterExport` en el paso 2/10 de `build.bat`). Cuando el piloto se retire, ese
directorio se muda aquí y el import es de una línea.

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

## Política de audio (8.1) — una sola señal, dos consumidores

NEURONiK embarca el mismo DSP dos veces: el motor **nativo** del plugin y el módulo **WASM**
que corre en un AudioWorklet. Los dos a la vez no es un caso soportado (voces dobles, FX con
fase rara), así que la regla queda escrita en un sitio:

| Dónde corre la página | Quién pone el audio | Cómo se ve |
|---|---|---|
| Dentro de un host JUCE (editor del plugin, bancada del piloto) | El **plugin** | Solo un letrero: `AUDIO: motor nativo del plugin`. Sin botón. |
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

## Qué NO está hecho (a propósito)

- **La paridad de 8.2**: la pantalla GENERAL lista los 11 ids con su valor real (leído del
  contrato), pero **sin widgets** — los knobs/sliders/toggles de la familia compartida son el
  trabajo de 8.2, igual que el reparto por pestañas del panel nativo.
- **El arnés del selftest de cuatro direcciones** sigue en la bancada del piloto
  (`Source/WebPilotHost.cpp`). Portarlo a algo que abra Standalone y VST3 es el paso 2c, y es
  lo que cierra el E2E de la política de audio (hoy probada en unitario, no dentro de WebView2
  en el plugin).
- **La barra de menú del editor es provisional** (preset, canal MIDI, voces, zoom, ayuda): se
  la lleva 8.3 a la propia página.
