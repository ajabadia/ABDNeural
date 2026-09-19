# NEURONiK Web UI Pilot — RETIRADO (ticket 8.4, 2026-09-19)

> **Este documento es el registro del piloto. El piloto ya no existe.**
>
> Su código (`WebPilot/` Next, `WebPilotVite/` Vite), su exportación estática
> (`WebPilot/out`) y la bandera `--pilot-page` de la bancada se borraron en el commit de
> retirada del 2026-09-19; lo único que sobrevive es lo que se mudó: el contrato de
> parámetros (`WebUI/generated/`), el protocolo versionado
> (`WebUI/contracts/bridge-protocol.json`, con su especificación en
> `DOCS/BRIDGE_PROTOCOL.md`), los
> assets y el worklet (`WebUI/public/`) y el sincronizador de WASM
> (`WebUI/scripts/sync-wasm.mjs`). La interfaz que se envía es `WebUI/` (JS vainilla
> sobre Vite) y la historia de esa decisión está en `ROADMAP.md` (Fase 8) y `HANDOFF.md`.
>
> Se conserva porque su sección del bridge sigue siendo el relato de diseño de un
> componente VIVO (`Source/WebUI/ParameterBridge.{h,cpp}`), y porque las mediciones del
> A/B Next vs Vite (tamaño de bundle, arranque) son el porqué de la decisión.

## Objetivo

Validar si Next.js es una base razonable para una interfaz web embebida en WebView2 sin migrar todavía la interfaz JUCE completa ni conectar todo el DSP.

Este experimento debe ser pequeño, reversible y evaluable.

## Alcance del piloto

El piloto tendrá únicamente:

- una pantalla/panel de parámetros;
- `masterLevel`;
- `morphX`;
- `morphY`;
- `engineType` como selector;
- estado local simulado;
- indicador de cambios de parámetros;
- exportación estática de Next.js;
- carga en navegador y WebView2.

No incluirá inicialmente:

- Bank Manager;
- MIDI real;
- WASM;
- AudioWorklet;
- presets completos;
- bridge JUCE real;
- migración de `ABDSharedCode` completa.

## Arquitectura del experimento

```text
Next.js export estático
        ↓
Panel React del piloto
        ↓
modelo de parámetros simulado
        ↓
WebView2 de prueba
```

El modelo simulado debe conservar los IDs reales definidos en:

```text
Source/State/ParameterDefinitions.h
DSP_PARAMETERS.md
```

No se deben inventar IDs alternativos.

## Criterios de éxito

El piloto continúa si cumple todos estos puntos:

1. `next build` produce una exportación estática.
2. La página funciona sin servidor Node en producción.
3. El panel se carga en navegador con rutas relativas.
4. El mismo contenido se carga en WebView2.
5. Los cambios de parámetros actualizan el estado sin listeners duplicados.
6. El bundle y el arranque son razonables para una interfaz embebida.
7. La estructura permite sustituir el mock por un adaptador JUCE sin reescribir el panel.

## Criterios para abandonar Next.js

Se volverá a React/Vite si el piloto muestra que:

- la exportación estática complica innecesariamente los recursos;
- WebView2 necesita hacks específicos para cargar la aplicación;
- el ciclo de vida de React no encaja con el bridge;
- el bundle o el arranque son excesivos;
- el adaptador JUCE queda más complejo que el panel que resuelve;
- las ventajas de Next.js no aparecen en esta escala de aplicación.

Abandonar Next.js en este punto no invalida los componentes React ni el contrato de parámetros.

## Estado actual

- [x] Aplicación Next.js aislada creada en `WebPilot/`.
- [x] Configurado `output: 'export'`.
- [x] Modelo simulado de cuatro parámetros implementado.
- [x] Panel mínimo implementado.
- [x] `next build` y exportación estática verificados.
- [x] Crear un host JUCE/WebView2 mínimo separado.
- [x] Ejecutar el host y verificar visualmente la carga de `out/`.
- [x] Conectar el bridge de parámetros real (APVTS <-> WebView2) con tira nativa de comparación.
- [x] Registrar la decisión final en `ROADMAP.md` y `HANDOFF.md` (2026-09-19).

### Estado del piloto (2026-09-19): contra-piloto

El piloto **ya no es la UI que se va a enviar**. La decisión de stack de la Fase 8 (ROADMAP)
son **JS vainilla + componentes compartidos, sin framework**, y su implementación vive en
`ABDNeural/WebUI/`. El piloto se queda como contra-piloto —referencia y banco de pruebas de la
página que el host sirve hoy— hasta que 8.2 cierre la paridad de control; después se retira o se
deja como banco (8.4).

Lo que sigue siendo suyo y **no** se tira: el puente (`lib/bridge.js`), el adaptador del
contrato (`lib/parameters.js`), la plomería de valores (`lib/paramValue.js`), el mapeo al
worklet (`lib/audioParams.js`) y la lógica del hook (`lib/useParameterControls.js`). Todo eso
está **portado tal cual** a `WebUI/src/`. Lo que muere es el armazón React (`app/page.jsx`,
`lib/controls.jsx`).

Y una guarda que el piloto ganó en 8.1: dentro del host **no arranca el AudioWorklet**. Con
`bridgeAvailable` su control de audio pinta `AUDIO: NATIVO` en vez de ofrecer SOUND ON, porque
el audio lo pone el motor del plugin. La regla canónica está en
`WebUI/src/audio/policy.js`; la de aquí es su gemelo mientras el piloto siga sirviéndose.

### Documentos vecinos

| Documento | Para qué |
|---|---|
| `ROADMAP.md` (Fase 8) | La decisión de stack, el inventario de paridad 8.0 y el plan 8.1–8.5. |
| `HANDOFF.md` | El detalle de ejecución: qué se hizo, con qué medición y qué falta. |
| `WebUI/README.md` | La UI nueva: arquitectura, contrato con el host y cómo se prueba. |

## Resultado de la verificación manual (2026-09-16)

El host `NEURONiK Web Pilot` arranca en WebView2 y muestra el panel Next.js completo:

- `Master Level`;
- `Morph X`;
- `Morph Y`;
- `Engine Type`;
- indicador `STATIC EXPORT`;
- contador de cambios y JSON de estado;

y los controles responden (los sliders y el selector actualizan el estado del panel).

Esto valida la cadena completa sin ningún hack específico de WebView2:

```text
next build (output: 'export')
    → WebPilot/out/
    → ResourceProvider JUCE (backend WebView2)
    → https://juce.backend/
    → panel React servido y funcional
```

Conclusión: la exportación estática de Next.js **es compatible con WebView2** y las rutas absolutas que genera (`/_next/static/...`) se resuelven contra el origen `https://juce.backend/`, que es precisamente el origen que intercepta `WebBrowserComponent`.

## Bridge real de parámetros (2026-09-16)

El panel ya no mantiene su estado solo: existe un canal bidireccional entre un APVTS real y la
página, sobre el canal de eventos de JUCE 8 (`juce_WebBrowserComponent`).

```text
Source/WebUI/ParameterBridge.{h,cpp}    protocolo bidireccional, testeado sin WebView2
Source/WebPilotHost.cpp                 transporte: withEventListener + emitEventIfBrowserIsVisible
WebPilot/lib/bridge.js                  transporte JS: window.__JUCE__.backend (modo local si no hay JUCE)
WebPilot/app/page.jsx                   panel conectado al transporte, con fases de gesto
Tests/ParameterBridgeTest.cpp           protocolo contra el APVTS real del contrato
Tests/webviewBridgeDirectionTest.mjs    guard de dirección del canal (ABDSharedCode)
```

El APVTS que refleja el host es `State::createLayoutApvts()`: exactamente el layout del plugin
(70 parámetros), no una copia escrita a mano. El host añade además una **tira nativa de
comparación** (sliders JUCE con `SliderAttachment` sobre los mismos IDs) para verificar a la vista
que mover un lado mueve el otro.

Formato en el cable (ambas direcciones son objetos JSON; `value` SIEMPRE normalizado 0..1,
`real`/`text` solo informativos):

```text
nativo -> JS  (emitEventIfBrowserIsVisible "event")
  { action: "syncAllParams", version, parameterCount, parameters: [{ id, value, real, text }] }
  { action: "parameterChanged", id, value, real, text }

JS -> nativo  (backend.emitEvent "nativeEvent", recibido con withEventListener)
  { action: "parameterChanged", id, value, gesture: "begin" | "change" | "end" }
  { action: "requestState" }

JS -> nativo  (backend.emitEvent "pageLoaded", listener dedicado)
  el host cierra gestos abiertos y responde con syncAllParams
```

Decisiones de diseño:

- **Salida por sondeo, no por listeners**: `publishPendingChanges()` (timer de 30 ms en el host)
  difunde por valor; un parámetro puesto al valor que ya tenía no genera tráfico. Es la protección
  deliberada contra el modo de fallo "listener duplicado / evento perdido" del ROADMAP.
- **Sin eco**: lo que la página envía se marca como conocido; el sondeo no se lo devuelve.
- **Gestos siempre cerrados**: si la página se recarga en mitad de un arrastre, el host cierra el
  gesto (`closeOpenGestures()`) para que el parámetro no quede "en automatización" para siempre.
- **Entrada tolerante**: mensajes malformados e IDs desconocidos se cuentan, nunca lanzan.
- **Modo local**: sin `window.__JUCE__` (navegador normal, `next dev` solo) la página funciona
  exactamente como antes del bridge; el distintivo muestra `LOCAL MODE` frente a `BRIDGE LIVE`.
- **Dirección del canal vigilada**: el guard compartido (`NEURONiK_WebViewBridgeDirection` en ctest)
  garantiza que el C++ nunca usa `backend.emitEvent` (canal JS->nativo) hacia el WebUI, el fallo
  silencioso que ya costó una depuración larga en ABDMS2000.

El protocolo completo es además un **contrato versionado**, al nivel del de parámetros:

```text
WebPilot/contracts/bridge-protocol.json     contrato versionado en git
WebPilot/BRIDGE_PROTOCOL.md                 especificación y política de versiones
NEURONiK_BridgeProtocolContractTest         C++: JSON vs constantes compiladas del bridge
NEURONiK_BridgeProtocolJs (node)            JS: JSON vs bridge.js + formas de mensaje reales
```

Cambiar un literal o una forma de mensaje en un solo lado rompe la suite; el detalle (incluida la
política de cuándo se incrementa la versión) está en `BRIDGE_PROTOCOL.md`.

## Contrato real de parámetros (sustituye al mock)

El panel ya no usa una lista escrita a mano. Lee los descriptores generados desde el APVTS:

```text
lib/parameters.js                        (adaptador: lookup, escalado, formato, validación)
lib/bridge.js                            (transporte del canal, ver sección anterior)
generated/parameters.generated.js        (70 parámetros reales)
generated/parameters.generated.d.ts      (tipos para TypeScript)
generated/parameters.generated.json      (instantánea de datos)
```

Los IDs, rangos, intervalos, `skew`, defaults y listas de opciones son reales, y ahora también lo
es el transporte: el estado de la página parte del snapshot del host y ambos lados se actualizan
entre sí.

Estos tres ficheros **se versionan a propósito** (no están en `.gitignore`): el test anti-drift los
compara contra una exportación nueva, así que tenerlos en el repositorio es lo que da valor a esa
comprobación en cualquier clone. Detalle en `HANDOFF.md`, sección del contrato de parámetros.

Regeneración e integridad:

```bash
cd ABDNeural
./build-reference/Release/NEURONiK_ParameterExport.exe WebPilot/generated
ctest --test-dir build-reference -C Release -R NEURONiK_ParameterDescriptorTest --output-on-failure
```

El contrato incluye además el estado real de cada parámetro (`dspStatus`, `engines`, `dspNote`):

```text
70 parámetros · 65 conectados al DSP · 4 solo UI · 1 sin ruta · 1 fuera del layout
```

El panel muestra ese resumen y marca con un asterisco cualquier control que no llegue al motor.

## Medición del host

Peso de `WebPilot/out`: 23 ficheros, 628 KB en crudo, 189 KB gzip, de los cuales unos 572 KB de JS
en crudo (172 KB gzip) son casi todo runtime de framework.

Arranque real en WebView2 (Release, 2026-09-16, medición repetida):

```text
                                   panel en DOM      react ready
5 ejecuciones en caliente            886 - 1166 ms    965 - 1293 ms
  backend WebView2 construido        341 - 515 ms
  recursos                           8 peticiones, 476 KB, 0-1 miss (favicon.ico)

control frio/caliente                1112 ms          1198 ms     (copia recien escrita del exe)
original en caliente                 1004 ms          1079 ms

outlier medido al compilar           2308 ms          2438 ms     (una sola vez)
```

Lectura de los datos:

- **No hay regresión sostenida.** Las cinco ejecuciones en caliente son incluso mejores que el
  rango medido antes (1232-1518 ms), y el bundle no ha crecido (8 peticiones, 476 KB).
- El outlier de 2308 ms apareció **una vez**, en el primer arranque tras recompilar.
- Intenté reproducirlo escribiendo una copia nueva del exe (fichero frío, misma carpeta, mismos
  recursos): 1112 ms, solo ~110 ms más que en caliente. Eso **descarta la caché de ficheros del exe**
  como causa.
- Queda por tanto como causa más probable la creación del entorno/perfil de WebView2 en esa
  primera sesión. No se ha comprobado aislando el perfil porque vive fuera del repositorio
  (`%LOCALAPPDATA%`); una medición limpia requeriría reiniciar o vaciar la standby list.

Borrar el perfil de WebView2 no cambia el resultado de forma apreciable (observación previa).
Reproducible con:

```bash
cd ABDNeural
"./build-reference/NEURONiK_WebPilotHost_artefacts/Release/NEURONiK Web Pilot.exe" --auto-quit
# resultado en pilot-startup.log, junto al ejecutable
```

Nota sobre el aviso de Turbopack en el build de la WebUI:

```text
Warning: Next.js ignored pnpm-workspace.yaml in D:\desarrollos\ABDSynths because it is outside
the current Git repository (D:\desarrollos\ABDSynths\ABDNeural).
```

Es esperado y **no hay que "arreglarlo"**: el piloto está instalado a propósito fuera del workspace
(`pnpm install --ignore-workspace`), así que Next encuentra el `pnpm-workspace.yaml` del monorepo,
comprueba que está fuera de este repositorio y lo ignora. Apuntar `turbopack.root` a ese directorio
haría justo lo contrario de lo que buscamos (arrastrar el workspace a un piloto que queremos
aislado). Se deja como está a propósito; revisitarlo solo si algún día el piloto pasa a ser parte
del workspace.

## Orden de implementación

1. ~~Crear una aplicación Next.js aislada en `WebPilot/`.~~
2. ~~Configurar `output: 'export'`.~~
3. ~~Implementar el modelo simulado de cuatro parámetros.~~
4. ~~Crear el panel mínimo.~~
5. ~~Verificar `next build` y exportación estática.~~
6. ~~Ejecutar `NEURONiK_WebPilotHost` y verificar visualmente la carga.~~
7. ~~Sustituir el mock por el adaptador de parámetros real.~~
8. ~~Medir el arranque real dentro de WebView2 (instrumentación del host).~~
9. Repetir la medición con el host en caliente y actualizar la tabla de arriba.
10. ~~Conectar el bridge de parámetros real (JUCE -> WebView2) y comparar contra la UI JUCE.~~
    (protocolo + transporte + guard de dirección + tira nativa de comparación; verificación
    interactiva de doble dirección pendiente de un `build.bat`)
11. Registrar la decisión final sobre Next.js en `ROADMAP.md` y `HANDOFF.md`.

## Regla de alcance

No añadir nuevas pantallas, presets ni conexión de audio hasta cerrar formalmente este punto de decisión.
