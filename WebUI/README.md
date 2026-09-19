# WebUI — interfaz definitiva de NEURONiK

JS **vainilla** (sin framework) para correr sobre **WebView2**, con los componentes y el
tema de `@abdsynths/shared`. Es la interfaz de la **Fase 8** del `ROADMAP.md`: sustituye
a los paneles JUCE nativos (`Source/UI/**`), que se retiran en el ticket 8.4.

`ABDMS2000/WebUI/` se usa **solo como referencia de arquitectura** (módulos de puente,
contrato y UI por separado). No hay código compartido con ese proyecto: cada plugin tiene
su propia carpeta.

## Cómo se ejecuta

```bash
cd ABDNeural/WebPilot && pnpm install   # esta carpeta es miembro del workspace de WebPilot
cd ../WebUI && pnpm test                # vitest (jsdom)
cd ../WebUI && pnpm dev                 # servidor de desarrollo en el navegador (modo local)
cd ../WebUI && pnpm build               # -> WebUI/dist
```

Sin host (`window.__JUCE__` ausente) la página arranca en **modo local**: el store
funciona, pero ningún envío sale al plugin ni se pinta Estado nativo.

## Qué hay aquí (y de dónde viene)

Todo lo de `src/` es JS sin framework **portado del piloto React** (`WebPilot/`) sin
cambios de comportamiento; lo que muere con el piloto es su armazón (`app/page.jsx`,
`lib/controls.jsx`).

| Aquí | Origen | Qué es |
|---|---|---|
| `src/contracts/parameters.js` | `WebPilot/lib/parameters.js` | Adaptador del contrato generado: lookup, clamp, snap, `to/fromNormalized` (math de `NormalisableRange`), formateo, defaults y validación. |
| `src/contracts/paramValue.js` | `WebPilot/lib/paramValue.js` | Plomería normalizado ↔ unidades reales para un control. |
| `src/contracts/paramStore.js` | `WebPilot/lib/useParameterControls.js` | El pegamento del hook convertido en store vainilla: `getState()` / `subscribe()`, gestos, presets, MIDI y modelos. |
| `src/bridge/bridgeCore.js` | `WebPilot/lib/bridge.js` | Transporte del bridge WebView2 (contrapartida JS de `WebPilot/contracts/bridge-protocol.json`). |
| `src/wasm/audioParams.js` | `WebPilot/lib/audioParams.js` | Contrato → índices de `GlobalParams` del motor WASM (página fuera del plugin). |
| `src/app.js` | nuevo | Arranque: monta el store y pinta el andamiaje. |

El **contrato generado** no se copia: `src/contracts/parameters.js` importa
`WebPilot/generated/parameters.generated.js`, que sigue siendo la única copia (la escribe
`NEURONiK_ParameterExport` en el paso 2/9 de `build.bat`). Cuando el piloto se retire
(8.4), ese directorio se muda aquí y el import es de una línea.

## Qué NO está cableado todavía

- **`build.bat` sigue construyendo el piloto React.** El paso 5/9 exporta `WebPilotVite`
  a `WebPilot/out`, que es lo que embebe el host del piloto y lo que sirve `start.bat`.
  Esta carpeta compila a `WebUI/dist` y **no** entra en ese circuito: cambiar el motor de
  UI es un paso deliberado (ticket 8.2), no un efecto colateral.
- **`README` de la paridad.** Las seis pestañas nativas (GENERAL, RESONATOR, FILTER/ENV,
  FX, LFO/MOD, BROWSER), el LCD con su D-pad, el navegador de presets con tags, el MIDI
  Learn y los visualizadores son 8.2 y 8.3. Aquí solo está el andamiaje y el control base.
- **Dentro del plugin el audio es NATIVO** (decidido en 8.1): la página habla por el
  bridge (APVTS) y el motor WASM del worklet es para la página en el navegador. Dos
  motores sonando a la vez no es un caso soportado.

## El control base (y el selftest)

`src/app.js` mantiene a propósito un `<input type="range">` nativo para `masterLevel`: el
`--selftest` del host usa `document.querySelector('input[type=range]')` y ese control debe
ser el primero. `tests/appContract.test.js` lo vigila contra el código fuente, igual que
hacía el guardián del piloto.
