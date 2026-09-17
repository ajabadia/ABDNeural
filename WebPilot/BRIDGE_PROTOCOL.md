# NEURONiK Bridge Protocol — contrato versionado

## Qué es

El protocolo del puente de parámetros entre el host JUCE y la WebUI (WebView2) es un
**contrato versionado**, igual que el contrato de descriptores de parámetros
(`WebPilot/generated/`). Esto significa:

- el fichero **`contracts/bridge-protocol.json` está versionado en git a propósito**;
- dos tests anti-drift lo comparan contra lo que cada lado compila/ejecuta realmente;
- cambiar el protocolo sin actualizar el contrato, o al revés, **rompe la suite**;
- cada divergencia es un diff revisable, no un fallo silencioso en runtime.

```text
WebPilot/contracts/bridge-protocol.json     EL contrato (fuente versionada)
Source/WebUI/ParameterBridge.h              literales del lado nativo (BridgeProtocolVersion, EventIds, Actions, Gestures)
WebPilot/lib/bridge.js                      literales del lado JS
WebPilot/BRIDGE_PROTOCOL.md                 este documento

NEURONiK_BridgeProtocolContractTest         C++: JSON vs constantes compiladas
NEURONiK_BridgeProtocolJs (node)            JS: JSON vs bridge.js + formas de mensaje reales
NEURONiK_WebViewBridgeDirection             guard de dirección del canal (ABDSharedCode)
```

## Canal y direcciones (JUCE 8)

Hay dos canales con direcciones opuestas. Confundirlos es el fallo silencioso que ya
costó una depuración larga en ABDMS2000 (el guard `NEURONiK_WebViewBridgeDirection`
existe para impedirlo):

```text
nativo -> JS   emitEventIfBrowserIsVisible("event", obj)
               Es el ÚNICO que dispara los addEventListener de la página.

JS -> nativo   window.__JUCE__.backend.emitEvent("nativeEvent", obj)
               Lo recoge el host con Options::withEventListener.

pageLoaded     window.__JUCE__.backend.emitEvent("pageLoaded", obj)
               Event id dedicado: el host cierra gestos abiertos y responde con un snapshot.
```

`evaluateJavascript("window.__JUCE__.backend.emitEvent(...)")` desde C++ NO funciona:
usa el canal JS->nativo, el mensaje se descarta en silencio y no hay error ni log.

## Formato del cable

Ambas direcciones transportan objetos JSON. `value` es SIEMPRE el valor normalizado
0..1 (la escala de `AudioProcessorParameter::getValue()`); `real` y `text` son
informativos y solo viajan nativo -> JS.

### nativo -> JS (event id `event`)

```jsonc
// Snapshot completo (carga de página, requestState, resincronización)
{ "action": "syncAllParams", "version": 3, "parameterCount": 70,
  "parameters": [ { "id": "masterLevel", "value": 0.8, "real": 0.8, "text": "0.80" } ] }

// Delta: un parámetro movido en el lado nativo (sondeo diferido por valor)
{ "action": "parameterChanged", "id": "filterCutoff", "value": 0.5, "real": 2400.0, "text": "2400 Hz" }
```

### JS -> nativo (event id `nativeEvent`)

```jsonc
// Cambio del usuario, con fase de gesto. gesture ausente significa "change".
{ "action": "parameterChanged", "id": "masterLevel", "value": 0.42, "gesture": "begin" }

// Petición de estado completo
{ "action": "requestState" }
```

### pageLoaded (event id `pageLoaded`)

```jsonc
{ "action": "pageLoaded" }   // el payload es ilustrativo: solo importa el event id
```

## Comportamiento fijado por el contrato

- **Sin eco**: lo que la página envía se marca como conocido; el sondeo nativo no se lo
  devuelve a la página que acaba de enviarlo.
- **Salida por sondeo**: `publishPendingChanges()` difiere por valor (timer de 30 ms);
  un parámetro puesto al valor que ya tenía no genera tráfico. Nunca se empuja desde
  listeners de parámetros: es la protección deliberada contra el modo de fallo
  "listener duplicado / evento perdido" del ROADMAP.
- **Gestos siempre cerrados**: `begin` abre como mucho un gesto por parámetro; `end`
  lo cierra; si la página se recarga en mitad de un arrastre, el host cierra lo que
  quedara abierto (`closeOpenGestures`). Un `end` sin `begin` se tolera y escribe el valor.
- **Entrada tolerante**: mensajes malformados, acciones desconocidas e ids fuera del
  layout se cuentan en `Stats`, nunca lanzan. Los ids desconocidos se cuentan aparte.
- **Clamping**: los valores JS fuera de 0..1 se acotan antes de entrar al APVTS.
- **Modo local**: sin `window.__JUCE__.backend`, todos los envíos son no-ops y la
  página funciona con estado propio (navegador normal, `next dev` aislado).

## Política de versiones

`version` (en el JSON) y `BridgeProtocolVersion::current` (en `ParameterBridge.h`)
deben coincidir; el test C++ lo comprueba.

| Cambio                                    | ¿Bump? | Qué hay que hacer                                   |
|-------------------------------------------|--------|-----------------------------------------------------|
| Añadir un campo OPCIONAL a un mensaje      | No     | Actualizar el JSON y este documento                  |
| Añadir una acción nueva                    | Sí*    | JSON + constantes C++ + bridge.js + tests + doc      |
| Renombrar un literal (id de evento, action)| Sí     | Igual que arriba; valorar tolerancia al anterior     |
| Cambiar la escala de `value`               | Sí     | **Ruptura total**: ambos lados en el mismo commit    |
| Cambiar solo la documentación              | No     | Commit normal                                        |

\* Un bump con código que sigue aceptando el formato anterior es compatible hacia atrás:
el receptor viejo ignora (y cuenta) las acciones que no conoce, que es justo el
comportamiento tolerante fijado arriba.

## Cómo regenerar / verificar

El contrato NO se genera: se edita a mano y los tests comprueban que ambos lados lo
respetan. Verificación:

```bash
cd ABDNeural
ctest --test-dir build-reference -C Release -R "BridgeProtocol|WebViewBridgeDirection" --output-on-failure
node Tests/bridgeProtocolContractTest.mjs          # también corre dentro de ctest
node Tests/webviewBridgeDirectionTest.mjs          # guard de dirección
```

Si falla el test C++: o el JSON no refleja lo que compila `ParameterBridge.h`, o el
fichero no existe (se restaurar desde git, nunca se genera).
