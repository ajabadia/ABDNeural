# NEURONiK Roadmap

## Objetivo

Evolucionar NEURONiK desde su implementación actual en JUCE hacia una arquitectura con:

- Standalone y VST3/AU conservando el comportamiento actual.
- Interfaz web reutilizable dentro de WebView2.
- Versión web con el mismo núcleo DSP mediante WASM.
- Componentes y modelos reutilizables desde `ABDSharedCode`.

La migración será incremental. No se sustituirá la interfaz JUCE ni se modificará el motor DSP sin una prueba de regresión equivalente.

## Estado actual — 2026-09-16

- [x] Repositorio clonado y revisado.
- [x] Build Release de referencia generado.
- [x] Standalone de referencia comprobado manualmente.
- [x] VST3 de referencia generado.
- [x] Dependencia de parámetros centralizada en `Source/State/ParameterDefinitions.h`.
- [x] Prueba offline inicial del DSP registrada en CTest.
- [x] Crear una fachada DSP progresiva para eventos y buffers.
- [ ] Separar progresivamente el núcleo DSP de las abstracciones JUCE.
- [ ] Crear adaptador de parámetros para la interfaz web.
- [ ] Crear wrapper WASM.
- [ ] Probar una pantalla web dentro de WebView2.
- [ ] Decidir entre React/Vite y Next.js estático a partir de un prototipo real.

## Fases

### Fase 0 — Línea base y control de regresión

- [x] Compilar una copia independiente en `build-reference/`.
- [x] Comprobar el Standalone actual.
- [x] Añadir `Tests/DSPReferenceTest.cpp`.
- [x] Registrar `NEURONiK_DSPReferenceTest` con `enable_testing()` y `add_test(...)`.
- [x] Añadir `build.bat` (compilación + contrato + tests, con pausa final).
- [x] Eliminar `build.ps1` (desfasado: compilaba todos los targets y tocaba `Version.h`).
- [ ] Añadir casos de note-off y cambio de parámetros en la prueba DSP de referencia.
- [ ] Guardar referencias reproducibles de audio, no solo RMS/peak.

### Fase 1 — Frontera del núcleo DSP

Objetivo: conservar el DSP actual, pero definir una API que pueda ser utilizada por JUCE y WASM.

- [ ] Documentar la API actual de `ISynthesisEngine`.
- [x] Definir una fachada progresiva con `prepare`, eventos MIDI y `process`.
- [ ] Añadir parámetros a la fachada progresiva.
- [ ] Mantener temporalmente `juce::AudioBuffer` y `juce::MidiBuffer` en el adaptador JUCE.
- [ ] Separar los tipos de eventos de nota del transporte MIDI de JUCE.
- [ ] Añadir un adaptador JUCE sin cambiar el resultado sonoro.
- [ ] Comparar el nuevo adaptador con el Standalone de referencia.

### Fase 2 — Estado, parámetros y presets

- [x] Definir un contrato agnóstico de framework para parámetros.
- [x] Mapear cada ID de `ParameterDefinitions.h` a rango, default y tipo.
- [x] Mantener APVTS como fuente de verdad en el plugin durante la transición.
- [x] Crear conversión entre APVTS y el modelo común.
- [x] Preparar el contrato para React/Vite/Next sin acoplarlo a Next.js.
- [x] Publicar el contrato como artefactos versionados (`.json`, `.js`, `.d.ts`).
- [x] Conectar el panel del piloto a los descriptores reales.
- [x] Publicar el estado de implementación DSP por parámetro (`dspStatus`, `engines`, `dspNote`).
- [x] Conectar `midiChannel` al filtrado de entrada, con `allNotesOff` al cambiar de canal.
- [x] Conectar `masterBPM` y el sync de LFO en ambos motores.
- [x] Reutilizar la tabla de divisiones para `fxDelaySync`/`fxDelayDivision`.
- [x] Conectar el juego completo de chorus y reverb (`rate/depth`, `size/damping/width`).
- [ ] Validar de oído delay sync, chorus, reverb y la curva de velocidad con presets reales.
- [x] Decidir el destino de los 4 sin consumidor: `velocityCurve` y `midiThru` conectados,
      `unisonEnabled` retirado de la UI, `harmMix` retirado del layout.
- [x] Retirar `harmMix` del layout (de 71 a 70 parámetros) y migrar los presets al cargar.
- [x] Añadir pruebas de round-trip de presets, incluidos los ficheros con parámetros retirados.
- [x] Decidir la política de `WebPilot/generated/`: se versiona, para que el test anti-drift
      signifique algo en cualquier clone.
- [x] Cubrir `allNotesOff` (fase de release) en la prueba DSP de referencia.
- [ ] Ampliar la prueba DSP de referencia para cubrir note-on/off y cambio de preset.
- [x] Convertir "un preset antiguo suena igual" en una prueba: cargar un preset con `harmMix`
      produce exactamente el mismo estado de parámetros que sin él (neutralidad, no percepción).
- [ ] A/B de oído con la pareja de EXEs en `Versiones compiladas\`: guardar un preset en el build
      `pre-harmMix`, cargarlo en ambos y comparar. Ojo: en esta máquina no hay presets guardados
      todavía (`Documents\NEURONiK\Presets` no existe), así que hay que crearlo primero.

### Fase 3 — Piloto mínimo de Next.js

Esta fase no pretende migrar la interfaz completa. Su función es responder pronto si Next.js encaja en WebView2.

- [x] Crear un prototipo Next.js aislado en `WebPilot/`, sin mover todavía toda `ABDSharedCode`.
- [x] Usar `output: 'export'` y probar la build estática real.
- [x] Reutilizar los IDs reales de parámetros en el panel piloto.
- [x] Implementar un único panel pequeño con estado simulado.
- [x] Probar una modificación de parámetro y un indicador de estado en el mock.
- [x] Crear y compilar un host JUCE/WebView2 mínimo separado.
- [x] Cargar el host y verificarlo visualmente en WebView2.
- [x] Comprobar tamaño, arranque, rutas de recursos y ciclo de vida.
- [x] Sustituir el mock por el adaptador de parámetros real.
- [x] Medir el arranque real dentro de WebView2 (instrumentación del host).
- [x] Publicar en el contrato qué parámetros llegan realmente al DSP.
- [ ] Comparar interacción y valores con la interfaz JUCE.
- [ ] Repetir la medición con el host en modo Release y con el WebUI final, no solo piloto.
- [x] Repetir la medición del arranque: 5 ejecuciones en caliente dan 886-1166 ms (panel) y
      965-1293 ms (listo), mejores que el rango anterior. El pico de 2308 ms fue un primer arranque
      tras compilar, no una regresión.
- [ ] Medir el primer arranque de la sesión de forma aislada (tras reiniciar o vaciar la standby
      list) para atribuir o descartar el entorno de WebView2 como causa del pico.

Peso medido de la exportación estática (`WebPilot/out`, 2026-09-16):

```text
ficheros            23
raw                 643 538 B  (628 KB)
gzip                193 903 B  (189 KB)
JavaScript raw      586 331 B  (572 KB)   -> sobre todo runtime de Next/React
JavaScript gzip     176 000 B  (172 KB)   aprox.
documento (HTML)      6 948 B
```

Arranque medido dentro de WebView2 (Release, `--auto-quit`, 2026-09-16):

```text
                    mín      máx
options built       535 ms   622 ms    construcción del backend WebView2
panel in DOM       1232 ms  1386 ms    el panel ya existe en el DOM
react ready        1366 ms  1518 ms    window.__pilotReady (effect de React)
```

- 5 ejecuciones (4 en caliente, 1 con el perfil WebView2 borrado): el perfil frío no cambia el
  resultado de forma apreciable, así que el coste está en la construcción del backend
  (~0,55 s, antes de cargar nada nuestro) más ~0,75 s de carga y ejecución del bundle.
- Tráfico real de la página: **8 peticiones, 476 KB sin comprimir** (el chunk legacy `noModule`
  no se descarga) y un único 404, `favicon.ico`, que es inocuo.
- Registro: `pilot-startup.log` junto al ejecutable del host; stdout también lo imprime.

Referencia interna: el bundle Vite de `ABDMS2000` publica `index.js` de 208 kB para una
interfaz mucho mayor. Los ~572 KB de JS del piloto corresponden casi por completo al runtime
del framework, no a la pantalla, así que este número es el coste base de adoptar Next.js y debe
pesarse en el punto de decisión frente a una base React/Vite equivalente.

Verificado el 2026-09-16: el panel Next.js (`masterLevel`, `morphX`, `morphY`, `engineType`)
se carga y responde dentro de WebView2, servido por un `ResourceProvider` JUCE sobre
`https://juce.backend/`, sin parches específicos de plataforma. La exportación estática de
Next.js es viable como base de la WebUI embebida; la decisión definitiva sobre Next.js
frente a React/Vite queda pendiente de la conexión del adaptador de parámetros real.

#### Punto de decisión del piloto

No se continuará con una migración amplia hasta poder demostrar:

```text
Next.js estático → panel piloto → adaptador de parámetros → WebView2
```

Si el panel funciona con una complejidad razonable, se continuará con React/Next.js. Si aparecen problemas de empaquetado, estado, recursos o bridge que no compensen sus ventajas, se volverá a React/Vite sin haber comprometido la UI principal.

### Fase 4 — WebView2 y bridge

Solo comienza después de superar el punto de decisión del piloto. (El 2026-09-16 empezó
adelantándose al punto de decisión formal, como prueba acotada del transporte: el riesgo que
quería despejar era exactamente el modo de fallo silencioso del canal.)

- [x] Crear el adaptador WebView2/JUCE (protocolo `ParameterBridge` + transporte en el host;
      la dirección del canal está protegida por el guard compartido de `ABDSharedCode`).
- [x] Versionar el protocolo como contrato (`WebPilot/contracts/bridge-protocol.json` +
      `BRIDGE_PROTOCOL.md`), con tests anti-drift en C++ y JS que fijan literales, formas de
      mensaje y comportamientos en ambos lados.
- [x] Enviar cambios de parámetros al procesador nativo (JS -> nativo sobre `nativeEvent`,
      con clamping, conteo de IDs desconocidos y fases de gesto begin/change/end).
- [x] Recibir snapshots del estado y reflejarlos en la UI (nativo -> JS sobre `event`,
      snapshot completo bajo demanda y deltas por sondeo sin eco ni duplicados).
- [x] Tira nativa de comparación en el host (sliders JUCE con attachment sobre el mismo APVTS).
- [x] Verificar a la vista la doble dirección en el host recompilado (mover el slider nativo
      mueve la página, y viceversa). Confirmado por el usuario (captura: 0.67/0.564/0.399 iguales
      en ambos lados) y por el selftest automatizado del host (`--selftest`, NATIVO->JS y
      JS->NATIVO OK, exit 0).
- [x] Validar presets por el bridge (2026-09-17): `listPresets`/`loadPreset`/`savePreset` JS->nativo
      responden `presetList`/`presetError` (aditivo a v1). Nombres sanitizados en nativo (sin
      separadores ni `..`); un load correcto cierra gestos abiertos y resincroniza TODO el estado
      (snapshot completo). `PresetController` como interfaz para testear sin directorio real;
      adaptador a `PresetManager` en el host. Barra de presets en la página (select + guardar).
      Tests: sección 8 de `ParameterBridgeTest`, literales en el contrato C++/mjs, vitest del hook.
- [ ] Validar MIDI y persistencia (teclado + mensajes MIDI en el bridge, protocolo v2 aditivo).
- [x] Embebido de recursos y rutas relativas (VALIDADO 2026-09-17: snapshot sin rutas de
      error, `[embedded fallback: 8]` con el E2E del bridge en verde sobre la WebUI embebida).
- [ ] Rebuild del EXE en cada cambio del bundle.

### Fase 5 — WASM

- [ ] Preparar un target WASM del núcleo DSP.
- [ ] Exponer una API C/ABI mínima y estable.
- [ ] Crear `AudioWorklet` para el renderizado.
- [ ] Comparar salida WASM con la salida nativa usando los mismos parámetros.
- [ ] Añadir pruebas de audio no nulo, note-on/off y cambio de preset.
- [ ] Validar sample rates y tamaños de bloque.

### Fase 6 — Consolidación del framework

- [ ] Comparar el piloto Next.js con una implementación equivalente React/Vite.
- [ ] Medir tamaño del bundle y tiempo de arranque.
- [ ] Validar WebView2 y exportación estática real, no solo `dev server`.
- [ ] Elegir la opción con menor complejidad operativa.
- [ ] Evitar que `ABDSharedCode` dependa directamente de Next.js.

### Fase 7 — Familia de controles compartidos (ABDSharedAssets, 2026-09-16)

Extraída y acordada con el usuario: los knobs/sliders/botones no serán un modelo único;
cada synthe elige su skin. NEURONiK es el primer consumidor del paquete compartido.

- [x] Crear la familia de controles en `ABDSharedAssets/components/` con el contrato de
      la familia (`Wheel`): Knob, Slider, Toggle + `drag-core.js` DRY compartido.
- [x] Sistema de skins: mapa de renderers por tipo de control, `applySkin()` despacha por
      `CONTROL_KIND`, fallback por-tipo a 'vector'. `registerSkin()` para skins de proyecto.
- [x] Skins incluidas: `vector` (SVG/CSS sin assets), `ms2000` (extraída de ABDMS2000),
      `junio` (sprites PNG extraídos de ABDJUNiO601: knob, slider cap/slot, botones 6 colores).
- [x] Assets compartidos en `ABDSharedAssets/assets/junio/` (15 PNG, fuente única).
- [x] Tests: 24/24 en verde (contrato, clamping, semántica onChange/setValue, gestos,
      despacho de skins, fallback, destroy sin fugas).
- [x] Demo única: sección 8 en `demo/demo.html` con la familia completa en las 3 skins;
      `demo/proto/` deprecada y eliminada (no mostraba nada único; `npm run demo` sirve la
      raíz del paquete, abrir `/demo/demo.html`).
- [x] Wrappers React (`WebPilot/lib/controls.jsx`): `useSharedControl` monta el control
      imperativo una vez (StrictMode-safe), React -> `setValue` programático (sin eco),
      `onChange/onDragStart/onDragEnd` -> callbacks con refs estables, `destroy()` al
      desmontar. `ParamSlider`/`ParamKnob`/`ParamToggle`/`ParamChoice` sobre el contrato.
- [x] Glue de página: `WebPilot/lib/useParameterControls.js` (estado normalizado + push
      al bridge con fases de gesto) y `WebPilot/lib/paramValue.js` (mapeo real<->0..1,
      snap de intervalo, encoding de choices N/(count-1)). Bug corregido de pasada: la
      página convertía real->normalizado con la función inversa (fallaría con skew≠1).
- [x] Consumo por paquete: `@abdsynths/shared` como `file:../../ABDSharedAssets` en el
      WebPilot (instalado con `pnpm install --ignore-workspace`), export `./components`
      añadido al paquete, `tokens.css` sin `@import` remoto (build offline). `masterLevel`
      conserva `input[type=range]` nativo a propósito: es el que conduce el `--selftest`
      del host; el resto de la página usa la familia compartida. Página con wrappers y
      build estático en verde; 15 tests nuevos del piloto.
- [x] Validar visualmente el nuevo page.jsx en el host: `--selftest` en verde tras el
      out/ nuevo (nativo->JS y JS->nativo OK) y arrastre 1:1 corregido en drag-core
      (ver corrección en HANDOFF).
- [ ] Pantalla GENERAL en Next.js usando los controles compartidos + tokens del tema.
- [ ] Teclado (`createKeyboard` de `ABDSharedCode/MidiKeyboard`) + mensajes MIDI en el
      bridge (protocolo v2 aditivo: `midiNoteOn/midiNoteOff/...`).
- [ ] Adoptar la familia en ABDMS2000 y ABDJUNiO601 cuando migren su WebUI (sin tocar
      nada hoy: sus controles actuales siguen funcionando).

#### Paso 1 — ParameterPanel real en el host (2026-09-16, en compilación)

Ejecutado a ciegas (sin compilar, por acuerdo de turno) a la espera del `build.bat` del
usuario:

- [x] El host instancia el `NEURONiKProcessor` real (fuera el APVTS de juguete
      `createLayoutApvts`): el bridge puentea el APVTS del plugin de verdad y las
      divergencias uiOnly/notRouted se comportan igual que en el plugin.
- [x] La tira de comparación (`NativeStrip`, 4 sliders) se sustituye por el
      **`ParameterPanel` real** (la pestaña GENERAL del editor: envolvente, unison,
      freeze, RANDOM, selector de motor). El E2E de migración pasa a ser: mover un
      control del panel nativo real debe mover la página, y viceversa.
- [x] **Assets de la WebUI embebidos en el exe**: `juce_add_binary_data
      (NEURONiK_WebPilotAssets)` con `WebPilot/out/**` (sin `_not-found`). El host
      sirve DISCO primero (out/ fresco) y BINARIO como fallback (exe autocontenido;
      es la vía que usará el VST3). El informe imprime `[embedded fallback: N]`.
- [x] Validado (2026-09-16 23:43, `build.bat`): el host enlaza todo el plugin, 10/10 tests
      y selftest del bridge OK; fixes posteriores (exit codes, snapshot 404) revalidados el
      2026-09-17.
- [ ] Verificación visual: panel nativo real y página moviéndose mutuamente — queda la
      (a) RANDOM→morphX/Y + slider→VOLUME y la (c) XYPad nativo (ya integrado en el host
      como columna derecha); la (b) fallback embebido está hecha.

## Criterios de aceptación

No se avanzará de fase si se cumple alguna de estas condiciones:

- El Standalone deja de compilar.
- El audio cambia sin una explicación y una referencia documentada.
- Los presets no conservan sus parámetros.
- El bridge genera listeners duplicados o eventos perdidos.
  (Mitigado por diseño en el puente actual: la salida es por sondeo diferido por valor, la
  entrada es idempotente y `Tests/ParameterBridgeTest.cpp` fija ambas cosas.)
- La versión web depende accidentalmente de un servidor Node en producción.
- El bundle web no funciona dentro del EXE embebido.

## Riesgos principales

1. **Acoplamiento JUCE-DSP:** actualmente las interfaces usan `AudioBuffer`, `MidiBuffer`, `MidiMessage`, `Random` y clases de smoothing de JUCE.
2. **Divergencia de parámetros:** APVTS, UI web y WASM no deben convertirse en tres fuentes de verdad independientes.
3. **Diferencias de audio:** WASM y nativo deben compararse con pruebas automatizadas y no solo a oído.
4. **Coste de mantener dos interfaces:** durante la transición, la UI JUCE seguirá siendo la referencia.
5. **Recursos WebView2:** WASM, AudioWorklet, fuentes e imágenes deben tener rutas compatibles con el empaquetado JUCE.

## Regla de trabajo

Cada cambio importante debe incluir:

1. una prueba o comprobación de regresión;
2. comparación con `build-reference/`;
3. actualización de este roadmap;
4. actualización de `HANDOFF.md` si cambia la arquitectura o el procedimiento.
