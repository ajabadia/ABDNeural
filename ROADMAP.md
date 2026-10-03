# NEURONiK Roadmap

## Objetivo

Evolucionar NEURONiK desde su implementación actual en JUCE hacia una arquitectura con:

- Standalone y VST3/AU conservando el comportamiento actual.
- Interfaz web reutilizable dentro de WebView2.
- Versión web con el mismo núcleo DSP mediante WASM.
- Componentes y modelos reutilizables desde `ABDSharedCode`.

La migración será incremental. No se sustituirá la interfaz JUCE ni se modificará el motor DSP sin una prueba de regresión equivalente.

## Estado actual — 2026-09-17 (tras cerrar las Fases 4 y 6)

- [x] Repositorio clonado y revisado.
- [x] Build Release de referencia generado.
- [x] Standalone de referencia comprobado manualmente.
- [x] VST3 de referencia generado.
- [x] Dependencia de parámetros centralizada en `Source/State/ParameterDefinitions.h`.
- [x] Prueba offline inicial del DSP registrada en CTest.
- [x] Crear una fachada DSP progresiva para eventos y buffers.
- [x] Crear adaptador de parámetros para la interfaz web (contrato generado del APVTS +
      `ParameterBridge` con presets; ver Fases 2-4).
- [x] Probar una pantalla web dentro de WebView2 (piloto BRIDGE + GENERAL + KEYS en el
      host; fallback embebido y selftest E2E de CUATRO direcciones: parámetros,
      presets, teclado/ruedas MIDI y snapshot; ver Fases 3-4 y 7).
- [x] Teclado MIDI compartido (`ABDSharedCode/MidiKeyboard` v0.2.0): tab KEYS con
      feedback sin eco (notas + ruedas) y panic; el host ahora renderiza audio
      (`AudioProcessorPlayer`) — el piloto suena.
- [x] Persistencia de estado validada con roundtrip del procesador real (Fase 4); el
      test destapó y arregló un bug real: los mappings de MIDI Learn no se guardaban
      en la sesión del DAW.
- [x] Decidir entre React/Vite y Next.js estático — spike A/B (Fase 6): la MISMA
      página pasa el selftest completo con ambos motores; Vite gana (471 KB vs 854 KB,
      build 2-6 s vs 15-25 s). SWITCH: `build.bat` compila Vite por defecto hacia
      `WebPilot/out`; Next queda como referencia tras `build.bat nextui`. Datos en
      HANDOFF.
- [x] Siembra determinista de `juce::Random` (LFO, NeurotikVoice, panel RANDOM) — la
      mina enterrada de la Fase 5 (WASM) ya está desactivada.
- [x] Pipeline de build: /MP, sin reconfiguración redundante de CMake, modo rápido
      `build.bat tests` (~19 s en caliente) y log espejo `build-last-run.log`. El WASM
      del worklet ya no queda fuera: `build.bat` lo compila como paso 4/9 antes de
      exportar la WebUI (aborta si falla; se omite con `nowasm`), y `build_wasm.bat`
      tiene su propio log espejo `wasm-last-run.log` y pausa final.
- [x] Separar progresivamente el núcleo DSP de las abstracciones JUCE (Fase 1,
      cerrada 2026-09-17: `DspEngineFacade` sin JUCE + paridad bit-exacta en `DSPReferenceTest`).
- [x] Crear wrapper WASM (Fase 5, primer hito cerrado 2026-09-17: módulo real de 89 KB
      renderizando audio, smoke test Node en verde). **Lección de ABDMS2000 aplicada y
      auditoría extendida a TODA la suite (2026-09-17)**: MS2000 duplicaba la lista de
      fuentes a mano y sufrió drift (4 fuentes del nativo no llegaban al WASM, incluido
      `SynthEngine.cpp`); la auditoría encontró el mismo modo de fallo latente en CZ101
      (GLOB nativo vs lista estática), JUNiO (46 ficheros de hueco) y EEP (57, mayoría
      principistas). Los cinco proyectos con doble build viven ahora del mismo patrón:
      lista single-source `DspSources.cmake` + exclusiones documentadas. Commits:
      MS2000 `1dc0fa584`, JUNiO `0f1a9b8`, EEP `a5f8d15`, CZ101 `600a160`,
      ABDSharedCode `a8643d5` (target `ABDShared::LutDSP` header-only que faltaba).
      Recetas duras rescatadas en `ABDEep/wasm/README_WASM_COMPILATION.md` (§2.A.0
      y lecciones 7-9): SSE bajo WASM (`-msimd128 -D__SSE__ -D__SSE2__ -include
      immintrin.h` + `JUCE_NO_INLINE_ASM=1`), quirk de `emsdk_env.bat` y la trampa
      headless de `juce_audio_processors` en JUCE 8.
- [x] AudioWorklet en el piloto WebPilot (Fase 5, segundo hito cerrado 2026-09-18):
      processor `neuronik-processor` (WebPilot/public/worklet/) instancia el
      módulo real vía processorOptions (el binario cruza como ArrayBuffer
      clonado; el scope del worklet no tiene fetch al mundo de la página). La
      página puentea snapshot de parámetros (mapeo contrato→GlobalParams en
      lib/audioParams.js, con test de contrato: 9 tests), motor, notas/wheels
      del teclado y panic. Botón SOUND ON (gesto de usuario para el
      AudioContext). Sincronización de artefactos: sync_wasm.bat /
      `pnpm sync:wasm` tras cada build_wasm.bat. Validado: vitest 55/55, build
      Vite 4.1 s (306 KB JS), smoke Node del módulo servido (peak 0.53),
      selftest del host exit 0 con la página nueva.
- [x] Matriz de paridad WASM<->nativo por sample rate y tamaño de bloque (Fase 5,
      2026-09-19): `WasmParityTest` genera 9 casos (44.1/48/96 kHz x 64/128/512) con la
      MISMA duración por escenario (los bloques se reescalan sobre una referencia de 128
      muestras) y `neuronik_wasm_parity.mjs` recorre la matriz entera reinicializando el
      módulo con cada pareja. **Y destapa un hallazgo:** con duración idéntica, el núcleo
      (A, B, E) es bit-exacto en los tres tamaños de bloque, pero `C_fx_panico` y
      `D_modmatrix` DEPENDEN del tamaño de bloque, porque dos rutas del motor son por
      bloque a propósito (el LFO se mantiene por bloque en `applyGlobalFX`; los smoothers
      de la reverb avanzan por bloque en `Effects/Reverb.h`). Importa porque el
      AudioWorklet renderiza en cuantos de 128 y un host nativo puede ir a 512: para esos
      dos caminos, web y nativo no darían la misma señal. Decisión abierta (ver Fase 5).
- [ ] Validar de oído delay sync, chorus, reverb y curva de velocidad (Fase 2,
      requiere presets reales y tus oídos).

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

- [x] Documentar la API actual de `ISynthesisEngine` (contrato en la cabecera: ciclo de
      vida, seguridad RT de `renderNextBlock`, hilos de los getters de visualización).
- [x] Definir una fachada progresiva con `prepare`, eventos MIDI y `process`.
- [x] Añadir parámetros a la fachada progresiva (`setGlobalParams`/`setPolyphony`;
      `GlobalParams` vive ya en `DspTypes.h`, POD sin JUCE).
- [x] Separar los tipos de eventos de nota del transporte MIDI de JUCE
      (`Runtime::Event` sin JUCE; el adaptador `Event`→`MidiMessage` vive SOLO en el
      `.cpp` de la fachada).
- [x] Quitar `juce::AudioBuffer` del motor (`dsp::AudioBuffer`, paso 2/6) y la frontera MIDI
      entera del motor (`dsp::MidiMessage` + `dsp::MidiBuffer`, paso 4/6): el único sitio
      donde queda el transporte MIDI de JUCE es el adaptador `Runtime::JuceMidiAdapter`,
      que los hosts JUCE usan para traducir. La fachada `Runtime/*` ya no incluye juce_*.
- [x] Quitar del motor los includes de JUCE, el macro de leak detector y el
      `DBG`/`jassertfalse` (paso 6/6): `dspDbg` y
      `dspDeclareNonCopyableWithLeakDetector` (Debug-only, como en JUCE), y el
      build del plugin/host queda con 0 avisos.
- [x] Quitar `juce::Reverb` del motor (paso 5/6, 2026-09-18): `Effects/DspReverb.h`
      es el port libre de JUCE (Freeverb, tunings idénticos) y `Effects/Reverb.h`
      queda como envoltorio de producto, con el efecto puro reutilizable tal cual.
      El port es **bit a bit idéntico** a `juce::Reverb` (0 ulps,
      `NEURONiK_DspReverbJucePolicyTest`) y **elimina la última divergencia de coma
      flotante entre nativo y WASM**: `JUCE_UNDENORMALISE` solo existe en x86 y no
      es un no-op, así que `juce::Reverb` calculaba distinto en cada build.
      Efecto medido: el escenario de paridad `C_fx_panico` baja de 16 ulps a 0 y su
      presupuesto vuelve a 0 (los cinco escenarios quedan bit-exactos).
      Por el camino se corrigió un bug latente del port de `dsp::HeapBlock`
      (`clear`/`allocate` tomaban bytes y JUCE toma elementos).
- [ ] Queda una dependencia deliberada de JUCE en `Source/DSP/`: la rama nativa de
      `Utils/SIMDWrapper.h` (`juce::dsp::SIMDRegister`, que es la implementación
      real y no un vestigio). `juce::Reverb` ya no está: es `dsp::Reverb`
      (DspEffects) desde el paso 5/6 y `Effects/Reverb.h` es el envoltorio.
- [x] Mover chorus, delay y saturación a `ABDSharedCode::DspEffects` (2026-09-19,
      continuación del paso 5/6): los tres efectos puros viven ya en el módulo
      compartido y `Source/DSP/Effects/` queda como envoltorios de producto, con
      paridad bit a bit (0 ulps) contra la referencia congelada de antes de la
      migración (`NEURONiK_DspEffectsParityTest`).
- [x] Renombrar `Source/DSP/DSPUtils.h` → `DspSafety.h` (2026-09-19): quitaba la
      homonimia con `ABDSharedCode/SynthCore/DSPUtils.h` (otra cosa: constantes,
      conversiones y waveshapers). No se unifica todavía — no hay código duplicado y
      estos helpers dependen del sustrato; el motivo y las condiciones para hacerlo
      quedan escritos en la cabecera del fichero. En el mismo paso se **borró**
      `sanitizeAudioBuffer` (0 llamadas): su política —escribir 0 en el buffer de
      salida— es la contraria a la del motor, que donde ve un NaN en un lazo de
      realimentación **reinicia la voz** (`AdditiveVoice`/`NeurotikVoice`), porque
      eso arregla el estado que diverge en vez de tapar el síntoma y no cuesta un
      barrido `isfinite` por muestra en el hilo de audio.
- [x] Añadir un adaptador JUCE sin cambiar el resultado sonoro (paridad BIT-EXACTA
      verificada: ruta directa vs ruta fachada, misma secuencia de notas, 0 tolerancia).
- [x] Comparar el nuevo adaptador con el Standalone de referencia (automatizado en
      `DSPReferenceTest`: el motor no cambió ni una línea, la comparación directa↔fachada
      es bit-exacta y el Standalone compila sin cambios).

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
- [x] Validar MIDI (HECHO 2026-09-17: teclado compartido `@abdsynths/midi-keyb` v0.2 en la
      página (tab KEYS), protocolo aditivo `midiNoteOn/Off/pitchBend/modWheel/panic` +
      `midiNoteState` nativo→JS con feedback sin eco; host con `AudioProcessorPlayer` (el piloto
      SUENA) y selftest de 4 direcciones en verde). La PERSISTENCIA de ajustes sigue pendiente.
- [x] Validar persistencia de estado (HECHO 2026-09-17 con `NEURONiK_StatePersistenceTest`,
      procesador REAL: session A edita parámetros + un mapping MIDI → getStateInformation →
      procesador B setStateInformation → 0 diferencias en el APVTS completo, mapping CC74→
      masterLevel restaurado, datos basura/extranjeros/antiguos ignorados sin corromper el
      estado vivo). EL TEST DESTAPÓ UN BUG REAL: `saveToValueTree` usaba asignación de
      ValueTree (`= midiNode`, que NO copia contenido, solo re-referencia) y TODO mapping de
      MIDI Learn se perdía silenciosamente al guardar la sesión del DAW. Arreglado con
      removeChild+appendChild; además la página ya se resincroniza sola (pageLoaded →
      syncAllParams) cuando el host reabre con estado del DAW.
- [x] Embebido de recursos y rutas relativas (VALIDADO 2026-09-17: snapshot sin rutas de
      error, `[embedded fallback: 8]` con el E2E del bridge en verde sobre la WebUI embebida).
- [x] Rebuild del EXE en cada cambio del bundle (HECHO 2026-09-17: `build.bat` reordenado —
      la WebUI va ANTES del host, que EMBIBE `out/` en el enlace; compilar el host antes dejaba
      dentro el bundle de la pasada anterior. Con el WASM dentro del build los pasos son hoy
      4/9 (WASM), 5/9 (WebUI) y 6/9 (host), en ese orden y por el mismo motivo).

### Fase 5 — WASM

- [x] Preparar un target WASM del núcleo DSP (hecho 2026-09-17: DSP real sin port, JUCE
      8.0.12 compilado con em++ sobre la frontera Fase 1; `build_wasm.bat` auto-arma VS+emsdk,
      salida `build-wasm/neuronik_dsp.js|.wasm` ES6+MODULARIZE, 89 KB; smoke test Node en
      verde: audio finito no nulo, drain de release, panic).
- [x] Exponer una API C/ABI mínima y estable (`Source/Wasm/NeuronikWasmBridge.cpp`:
      init/setEngine/process/setGlobalParams/layouts por offsetof/allNotesOff/numActiveVoices/
      getLfo; static_asserts de layout de 24 bytes por evento).
- [x] Crear `AudioWorklet` para el renderizado (cerrado 2026-09-18: el processor
      `neuronik-processor` vive en `WebPilot/public/worklet/` y el módulo se instancia por
      `processorOptions` porque el scope del worklet no tiene `fetch`; la página le pasa
      snapshot de parámetros, motor, notas/ruedas y panic, y lo arranca con SOUND ON.
      Detalle del hito en "Estado actual").
- [x] Comparar salida WASM con la salida nativa usando los mismos parámetros (cerrado: se
      ejecuta en cada `build_wasm.bat` — `NEURONiK_WasmParityTest` vuelca la referencia
      nativa y `neuronik_wasm_parity.mjs` la compara muestra a muestra. Presupuesto **0
      ulps** y guard absoluto 1e-6; los cinco escenarios son bit-exactos).
- [x] Añadir pruebas de audio no nulo, note-on/off y cambio de preset (smoke test Node:
      peak 0.53 finito, 1 voz activa, drain de release ~<2 s, allNotesOff OK; el preset
      vía setGlobalParams queda wire-up en UI).
- [x] Validar sample rates y tamaños de bloque (hecho con la matriz: 3 sample rates x 3
      tamaños de bloque, cada caso contra su propia referencia). **El resultado de esta
      validación es el hallazgo de abajo.**
- [x] **Resuelto (2026-09-19): las dos rutas que dependían del tamaño de bloque.** Las dos
      están arregladas y la matriz queda **15/15 celdas bit-exactas**
      (`blockSizeDependentScenarios` vacío). El diagnóstico que abrió el tema:
      Medido en nativo con duración idéntica en 64/128/512 muestras:
      - Núcleo (osciladores, resonador, envolventes, filtros, voz aditiva y Neurotik):
        **bit-exacto** en los tres (A, B, E: 0 diferencias de 8192 y 6144 muestras).
      - `D_modmatrix`: diverge desde el primer límite de bloque, porque
        `BaseEngine::applyGlobalFX` llama a `lfo.processBlock(numSamples)`, que devuelve
        **un** valor por bloque y lo mantiene: la modulación es una escalera de paso =
        `blockSize`. **Arreglado el 2026-09-19:** hoy D da también 0/6144 diferencias (ver
        el bloque de decisión de abajo).
      - `C_fx_panico`: diverge desde que termina la primera rampa (~448 muestras a 44.1/48
        kHz, ~960 a 96 kHz), porque `Effects/Reverb.h::processBlock` avanza sus smoothers
        de 20 ms **una vez por bloque** (un `getNextValue()` por llamada, fuera del bucle
        de muestras) y rearma la reverb con ese valor. **Arreglado el 2026-09-19:** hoy C
        da 0/12288 diferencias en las nueve celdas (ver el bloque de decisión de abajo).

      Por qué importa: el render web va en cuantos de 128 y un host nativo suele ir a 256
      o 512, así que para esos dos caminos web y nativo no dan la misma señal. Arreglarlo
      (LFO por muestra, smoothers de la reverb por muestra) **cambia el sonido** de los
      presets con FX y de los que usan la matriz de modulación, de modo que es una
      decisión de producto y no un refactor silencioso. Evidencia, reproducción y las dos
      causas en `HANDOFF.md`.
- [x] **HECHO (2026-09-19): las dos rutas se arreglan, en dos pasos separados.**
      - **Reverb: ARREGLADA (2026-09-19). Era un DEFECTO, no una dependencia de bloque.**
        `Effects/Reverb.h` avanzaba sus smoothers **una vez por bloque**, y esa rampa está
        declarada como 20 ms (`reset(sampleRate, 0.02)` = 960 pasos a 48 kHz). Con una llamada
        por bloque, la rampa NO dura 20 ms: dura 960 **bloques** — **~2,5 s con bloque 128 y
        ~10 s con bloque 512** a 44,1 kHz. Es decir, al cargar un preset la reverb no llega a
        su valor en 20 ms, se arrastra durante segundos, y el tiempo depende del buffer del
        host. Es el único envoltorio con este patrón: chorus, delay y saturación avanzan por
        muestra (verificado).
        **Cómo quedó:** el parámetro se lee y se aplica **por muestra** (bucle de una muestra
        contra `dsp::Reverb`, que solo expone API por bloque: `processStereo(l+i, r+i, 1)`),
        con un camino rápido de bloque entero para cuando ninguno de los cuatro smoothers
        está rampeando (mismo recorrido, una sola llamada). `setParameters()` se rearma solo
        cuando el valor suavizado cambia de verdad (`updateDamping()` no se paga por muestra).
        Con la reverb apagada el bloque se sigue saltando —equivalente: con wet 0 no toca la
        señal— pero los smoothers avanzan con `skip(numSamples)`, así que encenderla después
        arranca la rampa donde toca por tiempo.
        **Verificación (matriz completa, 9 celdas, 0 avisos con `/W4`):** `C_fx_panico` da
        **0/12288 diferencias y maxAbs 0,0** en los tres sample rates, así que
        `blockSizeDependentScenarios` pasa a `["D_modmatrix"]`. Contra el volcado del código
        anterior (48 kHz/128): A, B, D y E **bit-idénticos** (el camino de reverb apagada no
        se tocó) y C distinto en todo el render (maxAbs 8,9e-2; peak 0,528966 → 0,519034),
        que es el efecto buscado. Detalle en `HANDOFF.md`.
      - **Modulación: ARREGLADA (2026-09-19). La tasa de control no debe depender del buffer del
        host.** El valor del LFO se leía **una vez por bloque** (`applyGlobalFX` →
        `lfo.processBlock` → `applyModulation()`), así que la matriz era una escalera de paso =
        `blockSize`: 128 en la web y en un host de 128, 512 en uno de 512.
        **Cómo quedó:** el motor se renderiza en tramos de `BaseEngine::kControlBlockSize`
        (**64** muestras) mediante `renderVoicesWithControlRate()` — avanza los LFOs, aplica la
        matriz y solo después renderiza las voces de ese tramo —, con el sobrante **encadenado
        entre bloques del host** (`controlCarry`), de modo que la rejilla es de 64 muestras de
        audio para cualquier troceado. 64 = dos sub-bloques de voz (las voces ya trabajan en
        tramos de 32) y ~37 escalones por ciclo con el LFO a 20 Hz (frente a ~19 con 128).
        El `LFO::processBlock` avanza la fase **por muestra** (multiplicarla de golpe redondea
        distinto según el troceado; además desaparece la segunda implementación del S&H, que
        avanzaba la interpolación dos veces por muestra y por tanto sonaba distinto según el
        buffer del host). Las voces ya leían su snapshot de modulación una vez por llamada, así
        que no cambian: solo se les llama más a menudo.
        **Verificación:** los cinco escenarios dan 0 diferencias en las nueve celdas
        (`blockSizeDependentScenarios` baja de `["C_fx_panico","D_modmatrix"]` a `[]`). Contra
        el volcado anterior: A, B, C y E **bit-idénticos** y D distinto desde la muestra **64**
        exacta (maxAbs 2,1e-3 a 48 kHz/128), que es la primera frontera de la rejilla nueva.
        Detalle en `HANDOFF.md`.
      Ninguno de los dos se cuela en un commit de otra cosa: van con su propia verificación
      (la matriz de 9 casos debe quedarse sin escenarios dependientes, o con solo los que
      queden justificados por escrito). Las dos cumplieron: `blockSizeDependentScenarios` está
      **      vacío** y las 15 celdas son bit-exactas.
      El otro lado de esa moneda (regenerar el `.wasm` trackeado en
      `WebPilot/public/worklet/`, que era anterior a los arreglos) **destapó una diferencia de
      10 ulp en una muestra del escenario C a 44,1 kHz**: la libm del sistema (MSVC vs
      musl/emscripten) difiere 1 ulp en `sinf`/`atanf`, y el lazo del chorus la amplifica.
      **Resuelto de forma canónica (2026-09-19):** matemática determinista en el sustrato
      compartido (`ABDSharedCode/DspCore/DspMath.h`, `abd::dsp::sin/cos/atan` sin libm, usados
      por `DspChorus`/`DspSaturation`), con `-ffp-contract=off` en el build WASM. Misma
      decisión que con `JUCE_UNDENORMALISE`: uniformar la aritmética en vez de relajar el gate
      de 0 ulps. Detalle, evidencia y aviso de cambio de sonido en `HANDOFF.md`.
      Nota aparte, sin tocar: el mapeo del envoltorio (`dryLevel = 1 - mix*0.2`) se aplica
      sobre la escala interna del port (`dryScaleFactor = 2.0`, la misma que JUCE), así que
      con la reverb activa la señal seca va de 1,6 a 2,0 (un boost, no la unidad). Es
      anterior a este arreglo y es una decisión de producto, no un fallo del mismo.

### Fase 6 — Consolidación del framework

- [x] Comparar el piloto Next.js con una implementación equivalente React/Vite
      (`WebPilotVite/`, misma página importada 1:1, cero copias).
- [x] Medir tamaño del bundle y tiempo de arranque (471 KB vs 854 KB; build 2-6 s vs
      15-25 s; arranque ~7 s empatado — lo domina el arranque frío de WebView2).
- [x] Validar WebView2 y exportación estática real, no solo `dev server` (selftest
      completo del host en disco y con snapshot embebido, fallback 0).
- [x] Elegir la opción con menor complejidad operativa (Vite: sin turbopack.root,
      config local, build 4x más rápido; switch hecho en `build.bat` 2026-09-17).
- [x] Evitar que `ABDSharedCode` dependa directamente de Next.js (los paquetes
      compartidos son vanilla y la página no importa nada de `next/*`).

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
- [x] Pantalla GENERAL en Next.js (2026-09-17): pestañas BRIDGE/GENERAL en UNA página (sin
      rutas nuevas: el snapshot embebido sirve por basename y una segunda ruta colisionaría con
      index.html). Knobs (ParamKnob), toggles de freeze (ParamToggle) y motor (ParamChoice)
      sobre la familia compartida; gráfico ADSR en SVG puro con tokens del tema; grupos ENGINE /
      AMPLITUDE ENVELOPE / SPECTRAL UNISON / RANDOM, los mismos de ParameterPanel.cpp. El hook
      gobierna el estado de las dos pestañas a la vez (un preset o un gesto nativo se ve donde
      mires). vitest 42/42 y selftest del host en verde.
      NOTA: masterLevel conserva su input[type=range] nativo (el selftest lo conduce) y la
      validación del footer usa ahora el validador NORMALIZADO del hook (el de unidades reales
      daba errores falsos con envAttack, min 0.001).
- [x] Teclado (`createKeyboard` de `ABDSharedCode/MidiKeyboard` v0.2, feedback API:
      `setPitchBend`/`setModWheel`/`notesOffVisual` sin eco) + mensajes MIDI en el
      bridge (aditivo a v1: `midiNoteOn/midiNoteOff/midiPitchBend/midiModWheel/midiPanic`
      + `midiNoteState`). HECHO 2026-09-17; ver Fase 4.
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

### Fase 8 — Interfaz definitiva en WebView2 (2026-09-19)

> **Decisión tomada (2026-09-19):** la interfaz de NEURONiK es la **web sobre WebView2**, y
> los paneles JUCE nativos **se retiran**. Alcance: solo este proyecto (ABDMS2000 ya va por
> ahí y no se toca). Plataforma: **Windows-only por ahora** — WebView2 directo, sin capa de
> abstracción de host; si algún día se quiere macOS, el punto de entrada es
> `juce::WebBrowserComponent` (WKWebView), y lo que hay que decidir entonces son los dos
> caminos del bridge, no la UI.
>
> **Lo que ya está pagado:** el contrato de parámetros es SSOT (generado del APVTS), el
> `ParameterBridge` y el protocolo del bridge tienen contrato propio y test
> (`BridgeProtocolContractTest`, `webviewBridgeDirectionTest.mjs`), el host del piloto
> (`Source/WebPilotHost.cpp`, ~1000 líneas tras sacar los adaptadores a
> `Source/WebUI/BridgeAdapters.h`) **ya habla con el procesador real** —sus adaptadores
> mueven el APVTS, los presets, los modelos y el MIDI en las dos direcciones— y la página ya
> suena con el motor WASM en el AudioWorklet. O sea: falta la UI de verdad y retirar la nativa,
> no la fontanería.

**8.0 Inventario de paridad (función por función, 2026-09-19)**

Base: `Source/Main/NEURONiKEditor.{h,cpp}` (667 líneas), `Source/UI/**` (paneles,
visualizadores, LCD, browser, MIDI learner, tema) y `Source/Main/MidiMappingManager`.
La UI nativa son **seis pestañas** (GENERAL, RESONATOR, FILTER/ENV, FX, LFO/MOD,
BROWSER) bajo una cabecera estilo hardware (LCD 2 líneas + D-pad MENU/OK/‹ ›/^ v) y
una barra de menú File/Edit/Help.

| Función nativa | Dónde vive | ¿En la web hoy? | Destino en la web |
|---|---|---|---|
| Tab GENERAL | `UI/ParameterPanel.cpp` (276) | **Sí** (ficha GLOBAL & MASTER del lienzo, con cajón desde 8.3; el fader MASTER queda visible en su ficha) | 8.2 |
| Tab RESONATOR | `UI/Panels/OscillatorPanel.cpp` (216) | **Sí** (fichas RESONADOR y MODELOS A–D) | 8.2: el bloque MODEL queda cubierto (los 9 knobs del motor, repartidos entre OSCILADOR y RESONADOR; el XYPad como `morphX`/`morphY`; y las ranuras `loadA..loadD` en la ficha MODELOS A–D) |
| Tab FILTER/ENV | `UI/Panels/FilterEnvPanel.cpp` (114) + `UI/EnvelopeVisualizer.h` | **Sí** (2026-09-22, separada en DOS fichas: FILTRO — 3 knobs en la banda del motor tras el RESONADOR — y ENVOLVENTES — las dos ADSR + curvas, junto a MODELOS A–D) | Hecho: la curva ADSR (8.2) y la separación 8.3; la envolvente del filtro es la ENV 2 de la matriz (fuentes 6/7) |
| Tab FX | `UI/Panels/FXPanel.cpp` (169) | **Sí** (ficha EFECTOS) | 8.2 |
| Tab LFO/MOD | `UI/Panels/ModulationPanel.cpp` (189) | **Sí** (fichas LFO 1 & 2 — frontal rate+depth 2×2, cajón EDIT con forma/sync/división, formas de onda como fila LED con glifos — y MATRIZ DE MODULACIÓN al cajón con resumen) | 8.2; las SALIDAS de los LFO ya son fuentes 1/2 de la matriz, y las envolventes entraron como fuentes 6/7 (2026-09-22) |
| Tab BROWSER | `UI/Browser/PresetBrowser.{h,cpp}` + `PresetListModels.h` | **Solo una barra** select+save | 8.3: bancos/categorías, lista, búsqueda, **tags** con sugerencias, metadatos, LOAD/SAVE AS/DELETE, LOAD BANK/SAVE BANK |
| Preset rápido (combo + SAVE + DEL) | `UI/Panels/PresetPanel.cpp` (124) | Parcial | Se subsume en el navegador (una sola superficie de presets) |
| **MIDI Learn por control** (mouseUp sobre el control) + persistencia | `UI/MidiLearner.{h,cpp}` + `Main/MidiMappingManager` | No | 8.3: acción del bridge ("aprende el próximo CC", cancelar, borrar) y mapeo/guardado en el procesador |
| **Menú MIDI del LCD** (8 destinos CC + RESET ALL) | `UI/LcdMenuManager.h` (`ItemType::MidiCC` / `Action`) | No | 8.3 con el LCD |
| **LCD 2 líneas + D-pad** (estados Idle/Navigation/Edit) | `UI/LcdDisplay.{h,cpp}` (166) + `UI/LcdMenuManager.h` | No | 8.3: árbol GLOBAL/RESONATOR/FILTER/EFFECTS/MIDI CONTROL, con ítems que **dependen del `engineType`** |
| Visualizador espectral (64 parciales) | `UI/SpectralVisualizer.{h,cpp}` (97) | **Sí** (2026-09-20): `WebUI/src/ui/spectral.js` (64 barras suscritas a `onTelemetry`) compuesto en la ficha MODELOS A–D; ranuras compactadas a 2×2 para caber en el presupuesto del lienzo | Hecho; queda pulido visual (paleta/escala) si la bancada lo pide |
| XYPad (morph X/Y + nombres de modelo) | `UI/XYPad.{h,cpp}` (150) | **Sí** (8.3, 2026-09-20): pad dibujado en la ficha MODELOS A–D con los nombres en las esquinas (XYPad compartido + `setCorners`); los knobs morphX/morphY siguen en OSCILADOR — el pad es aditivo, las 70 celdas intactas | Hecho; el anillo de modulación sigue siendo el fleco 8.2 |
| **Feedback de modulación en cada control** (anillo/overlay del valor modulado) | `UI/CustomUIComponents.h` (`ModulatedSlider` + `Main/ModulationTargets.h`) | **Sí** (2026-09-20): anillo en el `Knob` compartido (`setModulation`, doble trazo como el nativo, token `--color-mod-ring`) + consumidor `modulationRings.js` (telemetría → anillos vía la tabla SSOT `MOD_DESTINATIONS` del exportador C++, con anti-drift a ambos lados) | Hecho (lo que la tabla llamaba 8.2); la semántica es la nativa: contribución con signo, normalizada contra el rango del destino |
| Barra de menú File/Edit/Help (cargar preset, zoom, specs MIDI, info RANDOM/FREEZE) | `NEURONiKEditor.cpp` (`getMenuBarNames`/`menuItemSelected`) | No | 8.3 como botones de cabecera o menú web; **los ítems de audio del Standalone no se migran** (los pone el wrapper de JUCE). Nota 09-20: en el nativo, Load/Save Preset ya viven en File (reorganización propia, sin migración web) |
| Diálogo de ayuda + especificaciones MIDI de fábrica | `UI/HelpDialog.h` + `showMidiSpecifications()` | No | 8.3 como overlay |
| Teclado en pantalla | `juce::MidiKeyboardComponent` + `MidiKeyboardState` | **Sí** (teclado compartido, como franja fija abajo) | Resuelto: franja fija como en nativo, plegable con el botón TECLADO |
| Zoom del editor (base 800×600) | `setZoom()` | **Parcial** (2026-09-20): el ajuste automático al viewport está hecho — `WebUI/src/ui/fitStage.js` escala el lienzo 0.25x–3x y lo centra en cada resize; el editor ya es redimensionable | El "escala del contenedor" prometido aquí ya es el comportamiento por defecto; el zoom manual por menú web queda para 8.3 |
| Tema por producto | `UI/ThemeManager.{h,cpp}` | No | Tokens de `@abdsynths/shared` (el mecanismo ya existe) |
| Look&feel nativo (knobs, `GlassBox`, `CustomButton`, `LedIndicator`) | `UI/CustomUIComponents.{h,cpp}` | N/A | **No se migra**: muere con el código nativo; su sitio son los componentes compartidos + CSS |

**Lo que decide si esto es una migración y no un rediseño:** tres funciones de esa tabla
no son "poner knobs" y son las que se olvidan al estimar — el **navegador de presets con
tags**, el **LCD con su máquina de estados y el menú de MIDI CC**, y el **feedback de
modulación en cada control** (que no es UI nueva: es una capacidad que hoy no tiene el
componente compartido, así que también toca a `@abdsynths/shared`).

**Decisión de stack: JS vanilla + componentes compartidos (no React)**

Evaluado el 2026-09-19, con el piloto React ya funcionando:

- **Lo que ya es la SSOT es vanilla.** `@abdsynths/shared` exporta `./components`
  (`components/index.js`), sin dependencia de ningún framework: sus widgets son clases DOM
  con ciclo `update()`/`destroy()`. React no los reutiliza, los **envuelve** — que es
  exactamente lo que hace `WebPilot/lib/controls.jsx` (un hook por tipo de widget). Esa capa
  extra es superficie de bugs, no ahorro.
- **La referencia que funciona es vanilla.** `ABDMS2000/WebUI/src` (`app.js`, `bridge/`,
  `panels/`, `ui/`, `components/`, `contracts/`) es la UI WebView2 **enviada y con suite
  vitest** del ecosistema, y está construida sobre los mismos componentes compartidos. Reusar
  su arquitectura es más barato y más honesto que inventar una.
- **El presupuesto de carga es de plugin, no de web.** El piloto React pesa 306 KB de JS
  (89 KB gzip) para tres pantallas de demo; React aporta ~45 KB gzip de runtime más el
  wrapper por widget, dentro de un plugin donde el bundle viaja embebido.
- **Un solo stack de UI en el ecosistema.** Dos (React aquí, vanilla en MS2000) es la misma
  clase de deuda que el inventario de homonimias de esta semana: dos formas de hacer lo
  mismo, que divergen en silencio.
- **Lo que NO se tira del piloto:** `lib/bridge.js`, `lib/paramValue.js`, `lib/audioParams.js`
  y la lógica de `useParameterControls.js` son **JS sin framework** y se portan tal cual
  (el contrato→`GlobalParams` para el worklet, el backend de JUCE con fallback a "LOCAL
  MODE", la conversión de valores normalizados). Lo que muere es el armazón React
  (`app/page.jsx` y `lib/controls.jsx`).
- **Coste asumido, con su mitigación:** en vanilla el estado (70 parámetros + presets +
  estado MIDI + LCD) se sincroniza a mano. Mitigación: la estructura de MS2000 (un módulo de
  puente, uno de contrato, uno de UI) y **vitest desde el minuto uno** — el paquete del
  piloto hoy no tiene script de test, el de MS2000 sí.
- **`WebPilotVite` (React) quedó como contra-piloto** hasta cerrar 8.2; **se retiró** en el
  commit de retirada del piloto (2026-09-19) junto con `WebPilot/`, sin dejar una segunda
  implementación de la UI. Lo único que se mudó de ahí fue `scripts/sync-wasm.mjs` (a
  `WebUI/scripts/`), que no era del piloto sino del build del WASM.

**8.0.1 El andamiaje vainilla (hecho, 2026-09-19)**

Nace `ABDNeural/WebUI/` — proyecto Vite vainilla **propio** (no se comparte código con
`ABDMS2000/WebUI`, que es solo referencia de arquitectura).

- [x] `src/bridge/bridgeCore.js` <- `WebPilot/lib/bridge.js` (transporte WebView2, protocolo
      versionado: el mismo que ya prueban `ParameterBridgeTest` y `BridgeProtocolContractTest`).
- [x] `src/contracts/parameters.js` <- `WebPilot/lib/parameters.js` (adaptador del contrato
      generado; sigue importando la **copia única** de `WebPilot/generated/`, no se duplica).
- [x] `src/contracts/paramValue.js` <- `WebPilot/lib/paramValue.js`.
- [x] `src/wasm/audioParams.js` <- `WebPilot/lib/audioParams.js` (contrato → `GlobalParams`).
- [x] `src/contracts/paramStore.js`: la lógica de `useParameterControls.js` convertida en store
      vainilla (`getState()` / `subscribe()`), con gestos, presets, MIDI y modelos. Sustituye
      el `useState`/`useMemo`/`useRef` por un objeto de estado y una lista de oyentes.
- [x] `src/app.js`: arranque vainilla (store + estado del bridge + resumen del contrato) y el
      `<input type="range">` base de `masterLevel` que el `--selftest` del host necesita.
- [x] Suite portada: **61 tests** en 6 ficheros (`pnpm test`), incluido el guardián del control
      base y un guardián explícito de "cero framework".
- [x] Medición, para el objetivo de bundle de 8.5: **32,4 KB de JS (6,3 KB gzip)** frente a los
      306 KB (89 KB gzip) del piloto React, con las mismas dependencias compartidas.
- **Ya no hay "fuera de circuito":** durante 8.2 `build.bat` seguía exportando `WebPilotVite` a
  `WebPilot/out` (lo que embebía el host del piloto y servía `start.bat`), y esta carpeta compilaba
  a `WebUI/dist` sin entrar ahí. **El piloto se retiró el 2026-09-19**: `WebUI/dist` es la única
  página, la que embebe el plugin, la que sirve la bancada y la que sirve `start.bat`.

**8.2 (arranque) La shell vainilla — hecho 2026-09-19, a propósito ANTES de 8.1**

La base de 8.2, escrita y verde, **sin cablear a nada**: el host, `build.bat`, `start.bat`
y CMake siguen sirviendo el piloto React. Se hace antes de 8.1 porque la shell es
agnóstica de quién la hospeda (hospedar la página es ResourceProvider + adaptadores, los
mismos con cualquier página), así que tenerla verde no se tira y permite A/B contra el
piloto. **8.1 sigue pendiente y es el siguiente paso.**

- [x] `src/contracts/screens.js`: los ids de cada pantalla como datos (BRIDGE, GENERAL,
      KEYS), todos del contrato generado. La agrupación definitiva (GENERAL, RESONATOR,
      FILTER/ENV, FX, LFO/MOD, BROWSER, como el panel nativo) es 8.2 de verdad.
- [x] `src/ui/panel.js`: shell con pestañas, el **control base** (`masterLevel`) y la
      pantalla GENERAL con los 11 ids y su valor real leído del contrato. **Sin widgets**:
      los knobs/sliders/toggles de la familia compartida son 8.2 (se dice en la propia UI,
      no se disfraza).
- [x] `src/ui/keyboard.js`: el teclado compartido (`@abdsynths/midi-keyb`) con la API de
      feedback del host (`setPitchBend`/`setModWheel`, la vía silenciosa).
- [x] Los selectores que el `--selftest` del host lee quedan **pinchados en tests** (primer
      `input[type=range]` = `masterLevel`, `footer.panel-footer code` como JSON,
      `[data-tab="keys"]` y `#mod-wheel-container .kbd-wheel-slider`): así no se rompen en
      silencio dentro de WebView2. Detalle en `WebUI/README.md`.
- [x] Suite: **81 tests** en 9 ficheros, 96 tras el trabajo de 8.1 sobre la política de audio
      (`cd WebUI && pnpm test`). Bundle: 62,4 KB de JS
      (15,6 KB gzip) frente a los 306 KB (89 KB) del piloto React.
- **Fuera de circuito:** `WebUI/dist` no lo consume nadie todavía. El cableado del host es
      un paso deliberado y va con 8.1 (en el **editor del plugin**, no en la bancada del
      piloto).

> **Superado por el lienzo único** (8.2, más abajo): las pestañas BRIDGE/GENERAL/KEYS se
> retiraron. GENERAL vive ahora como ficha GLOBAL & MASTER dentro del lienzo y KEYS es la
> franja fija de abajo; el atributo `data-tab="keys"` se conserva porque es el anclaje que
> pulsa el selftest del host.

**8.1 El editor del plugin hospeda la página**
- [ ] Mover a `NEURONiKEditor` lo que hoy vive en `WebPilotHost`: `WebBrowserComponent` +
      `ResourceProvider` (disco en dev con hot-reload, embebido en release — mismo patrón que
      ABDMS2000), y los tres adaptadores (`PresetManagerAdapter`, `MidiInjectionAdapter`,
      `EngineModelsAdapter`). El host (bancada) se queda como banco de pruebas — y desde la
      retirada del piloto (2026-09-19) sigue siendo el único consumidor del `XYPad` nativo.
      - [x] **Paso 1 (2026-09-19):** los tres adaptadores salen de `WebPilotHost.cpp` a
        `Source/WebUI/BridgeAdapters.h` (header-only: no añade fuentes ni entradas en CMake),
        para que el editor use los MISMOS tres en vez de una copia por superficie. El host
        del piloto los incluye con `using` y no cambia de comportamiento.
        *Pendiente: compilar.*
      - [x] **Paso 2 (2026-09-19): la página ES el editor.** Dos decisiones tomadas aquí:
        la página **sustituye** ya al panel nativo (no conviven), y lo que embebe el plugin es
        `WebUI/dist` (la shell vainilla), no el piloto React. Detalle:
        - `Source/WebUI/NeuronikWebView.h`: subclase de la base compartida
          `abd::webview2::JuceWebView2Component` (backend webview2, native integration,
          `resized()` que ajusta el navegador a sus bounds, tema y hook `pageLoaded`) con el
          `ParameterBridge` y los **tres adaptadores de `BridgeAdapters.h`** enchufados — los
          mismos que usa la bancada, no una copia. El `resized()` no lleva lógica propia: la
          base ya ajusta el navegador, así que no había regresión que introducir.
        - `NEURONiKEditor` pasa a ser el editor de la página: barra de menú **provisional**
          (preset, canal MIDI, voces, zoom, opciones de audio del Standalone, ayuda MIDI — se
          la lleva 8.3), `paint()` con el color del chasis para el hueco de arranque de
          WebView2 (8.5 lo mide), y el poll del puente (cambios de parámetro cada ticket,
          estado MIDI externo cada ~180 ms). Se dejan de montar los paneles nativos.
        - **Zoom = zoom del contenido** (zoom CSS del documento), no un `setTransform()`: los
          knobs de JUCE que escalaba el editor nativo ya no existen, y así el zoom no depende
          del tamaño que dé el host en cada DAW.
        - **`NEURONIK_HAS_WEBUI_VIEW`**: lo define el CMake de los targets que montan la
          página (Standalone y VST3). El target de tests que **copia** `NEURONIK_SOURCES`
          (`StatePersistenceTest`) no lo define y compila el editor en su variante sin
          interfaz: copiar las fuentes no obliga a embeber 68 KB de UI en un ejecutable de
          pruebas, y el `#else` es honesto (un aviso en pantalla), no un fallo de enlace.
      - [x] **Paso 2c — HECHO (2026-09-19): el selftest de cuatro direcciones vive en el
        plugin, y la bancada usa EL MISMO.** El arnés sale de `Source/WebPilotHost.cpp` a
        `Source/WebUI/BridgeSelftest.h` (misma jugada que los adaptadores en el paso 1: una
        sola implementación para las dos superficies) y quien lo corre en el plugin es el
        **editor**, porque es la única superficie que hospeda la página en los dos formatos.
        - **Disparo, y por qué dos vías:** `NEURONiK.exe --selftest` en el Standalone (es un
          proceso: el veredicto es su **código de salida**, y así `build.bat` no exporta
          variables) y `NEURONIK_SELFTEST=1` en cualquier formato — el VST3 se lanza desde el
          DAW y **no recibe argv**, así que la variable es su único disparo posible (sirve
          igual dentro de pluginval o del DAW que abra el editor).
        - **Veredicto:** stdout + log (`NEURONIK_SELFTEST_LOG` fija la ruta; por defecto,
          datos de usuario del sistema, porque el VST3 no puede escribir junto a su binario).
        - **Dos precauciones que la bancada no necesitaba** porque allí moría el proceso
          entero: **timeout** de 30 s (un hop perdido no puede dejar un editor colgado) y
          **guarda de vida en TODOS los callbacks** (el editor se puede cerrar con hops en
          vuelo; el arnés muere antes que el navegador y un callback tardío no toca memoria
          liberada).
        - **Anclajes:** los selectores que el arnés consulta son un contrato con la página y
          viven en un solo sitio (`SelftestPage`), fijados por los dos lados en
          `Tests/webuiSelftestContractTest.mjs` (incluye los 11 ids de GENERAL contra
          `GENERAL_PARAMETER_IDS`). Verificado que el guard falla de verdad (mutación
          temporal de un anclaje → exit 1).
        - **`build.bat` 10/10** ejecuta PRIMERO el selftest del plugin (la superficie que se
          envía) y después el de la bancada, que sigue contando hasta el commit de retirada.
        - **Verificado:** Standalone con `--selftest` → las cuatro direcciones OK, exit 0;
          Standalone con `NEURONIK_SELFTEST=1` (sin argv) → OK, exit 0; bancada del piloto con
          el arnés compartido → OK, exit 0 (sin regresión en el port). El **VST3 real** (un
          DAW/pluginval abriendo el editor) queda para 8.5: ahí el arnés ya está puesto y su
          disparo y su log están documentados.
      - **Regresiones del paso 2 arregladas por el camino** (el `build.bat` del usuario se
        paró en 5/10 y el log solo enseñaba la primera):
        - **Compilación (103 errores dentro de `juce_StandaloneFilterWindow.h`).** El paso 2
          quitó `#include <juce_audio_utils/...>` del editor, y ese header **no incluye sus
          dependencias** (`juce_audio_devices` + `AudioProcessorPlayer`): confía en el
          `JuceHeader.h` del wrapper. Ahora se incluyen explícitas en el guard de standalone,
          con el motivo escrito.
        - **Enlace del VST3 (10 `LNK2019` de `ParameterBridge`).** El mismo commit dejó
          `ParameterBridge.cpp` fuera de los targets del plugin (solo lo añadían la bancada y
          los tests), así que la lib del wrapper quedaba con `NEURONiKEditor.obj` pidiendo el
          puente. Va en el `foreach` de los targets que montan página, no en
          `NEURONIK_SOURCES`: quien lo necesita es la página, no el motor.
      - [x] **Paso 2b — hecho (2026-09-19): el proveedor de recursos del plugin NO se
        escribe aquí.** `ABDSharedCode/WebView2Bridge/WebView2ResourceProvider.*`
        (`abd::webview2`) ya implementa el pipeline entero — `normalizeResourcePath`,
        `getMimeTypeForFilename`, `resolveEmbeddedAsset` (con `BinaryAssetsCatalog`, que se
        rellena desde el `BinaryData` generado) y el fallback de assets compartidos — y ya lo
        consumen `HardwareMidiDetect` y ABDScope. El plugin **adopta el compartido**, que es
        justo lo que el DoD pide: embebido primero y **cero rutas absolutas** (el VST3 no carga
        desde el cwd del build).
        - La bancada del piloto **sí** conserva su proveedor propio, y por un solo motivo: hace
          *disco primero* con hot-reload para el desarrollo, que el compartido no ofrece. Esa
          diferencia vive solo en la bancada, cuyo destino decide 8.4.
        - Escribir un `PluginEditor_ResourceProvider` propio de NEURONiK sería añadir una
          homonimia que `ABDSharedCode/docs/homonimias-cabeceras.md` lista como deuda en su
          prioridad P5 — y 8.5 cuenta esas homonimias entre lo que la migración NO debe revivir.
        - **Hecho:** `ABDSharedCode` ahora **define** `ABDShared::WebView2Bridge` (INTERFACE,
          con `WebView2ResourceProvider.cpp` propagado y `juce_gui_extra`). Hasta ahora
          ninguna de las superficies que lo sondeaban lo encontraba, así que **nadie compilaba
          el proveedor**: el `.cpp` estaba en el repo y en ningún binario. Va bajo
          `if(NOT EMSCRIPTEN)` porque el bridge es `juce_gui_extra`, que no existe en los
          builds WASM. `NeuronikWebView.h` lo alimenta con el catálogo del
          `juce_add_binary_data` (`NEURONiK_WebUIAssets`, con `HEADER_NAME`/`NAMESPACE`
          explícitos para no nacer como un segundo `BinaryData::`, que es el nombre genérico
          que usa la bancada).
        - **Sin prefijos de `ABDSharedAssets`:** la página IMPORTA los estilos compartidos
          (`@abdsynths/shared/styles/...`) y Vite los empaqueta en su bundle, así que en
          tiempo de ejecución el plugin no necesita el disco para pintar su interfaz. El
          fallback compartido existe pero no se pide.
        - **Disco solo como override de desarrollo:** `NEURONIK_WEBUI_DEV_DIR` apuntando a
          `WebUI/dist` sirve la página desde disco (iterar la UI en segundos en vez de
          recompilar el plugin). Apagado por defecto y **sin ninguna ruta grabada en el
          binario**, así que el DoD de "cero rutas absolutas" se mantiene: es una decisión de
          arranque, no una dependencia del build. Es la única función que tenía la bancada del
          piloto, y por eso se trae antes de retirarla.
- [x] **Decidido y fijado en código (2026-09-19):** dentro del plugin el audio es nativo y la
      página solo habla por el bridge (APVTS). Una sola señal — `window.__JUCE__`, la MISMA que
      usa el puente, vía `nativeBackend()` — decide quién posee el audio: `WebUI/src/audio/policy.js`.
      El motor del worklet (`WebUI/src/audio/audioWorkletEngine.js`, portado del piloto) **no
      arranca** dentro de un host: devuelve `blocked` y ni siquiera construye un `AudioContext`
      (hay test que lo vigila espiando el constructor). La página que el host sirve **hoy** (el
      piloto) lleva la misma guarda: dentro del host no pinta SOUND ON ni arranca nada.
      - Pendiente de este bullet: la comprobación **E2E**. El `--selftest` corre hoy contra el
        *host del piloto*, no contra el plugin, así que la política está probada en unitario y
        no en WebView2 real. Se cierra en 8.1, cuando el editor hospede la página.
- [x] Editor: tamaño (1100×720 base, redimensionable con límites) y zoom del contenido; el
      `resized()` no lleva lógica propia porque la base compartida ajusta el navegador.
- [x] **`build.bat` reordenado a WASM → WebUI → plugin** (antes el plugin iba primero y
      embebería el bundle de la pasada anterior): el `.wasm` llega a `WebUI/dist/worklet` por
      el `publicDir`, y el plugin embebe `WebUI/dist`. Con guard: si el worklet embebido no
      coincide con el recién compilado, aborta. Numeración 1/10…10/10.
- **DoD:** Standalone y VST3 abren la página y el selftest de seis direcciones pasa igual
  que en el host del piloto; cero rutas absolutas (el VST3 no carga desde el cwd del build).
  **Estado (2026-09-19):** el Standalone y su selftest están verificados (las seis
  direcciones OK, exit 0, MODELOS y MATRIZ incluidas); el VST3 compila, enlaza y embebe la
  página, y su selftest ya está implementado y documentado, pero la comprobación con un host
  dentro (DAW o pluginval) es 8.5 — no se apunta como verificada.

**Retirada del piloto — HECHA 2026-09-19 (decidida para ir JUSTO DESPUÉS de 8.1)**

Decisión: el piloto se retira en cuanto el paso 2c esté hecho, porque ya no tiene ninguna
función propia. Pero **"el piloto" son tres cosas distintas** y solo una es el piloto:

1. **El arnés React** (`WebPilotVite/`, `WebPilot/app/`, `WebPilot/lib/`, `WebPilot/out/`,
   `WebPilot/tests/`). Sus conclusiones están **integradas**: `bridge.js`, `paramValue.js`,
   `audioParams.js`, `parameters.js`, la lógica de `useParameterControls.js` y
   `audioWorkletEngine.js` están portados verbatim a `WebUI/src/`, y el armazón React
   (`app/page.jsx`, `lib/controls.jsx`) muere por decisión (8.0). Sus 57 tests están cubiertos
   por los 96 de `WebUI`. → **Se puede borrar.**
2. **La bancada** (`Source/WebPilotHost.cpp` + su target). Su única función era el
   disco-primero con hot-reload, y eso ya vive en el plugin como `NEURONIK_WEBUI_DEV_DIR`
   (paso 2b). Su selftest **ya no es único**: desde el paso 2c vive en
   `Source/WebUI/BridgeSelftest.h` y lo corre el editor, y desde el 2026-09-19 la bancada **sirve
   la MISMA página que el plugin** (`WebUI/dist`, por defecto) y corre las mismas cinco
   direcciones; `--pilot-page` sigue sirviendo su exportación retirada mientras esté en el árbol,
   y es el único caso en el que MODELOS se declara no aplicable. Le queda su snapshot embebido del
   piloto (`NEURONiK_WebPilotAssets`) y poco más. → **Lo que se borra es su camino del piloto y su
   snapshot; la bancada SE QUEDA** (ver 8.4: es la única consumidora del `XYPad` nativo y de las
   métricas de arranque, y esa decisión se tomó al ver el árbol, no al planearlo).
3. **Tres artefactos que NO son del piloto**, aunque vivan en su carpeta. Son la SSOT de
   cosas que siguen siendo ciertas, y **se mudan, no se borran**:

| Artefacto | Quién lo consume hoy | Destino |
|---|---|---|
| `WebPilot/generated/` (contrato de parámetros) | `WebUI/src/contracts/parameters.js` (import), `CMakeLists.txt` (`NEURONIK_PARAMETER_ARTIFACTS_DIR` → `ParameterDescriptorTest`), `build.bat` paso 2/10, `Tests/ParameterExportTool.cpp` | `WebUI/generated/` (o `contracts/generated/`), con los 4 consumidores repuntados |
| `WebPilot/contracts/bridge-protocol.json` (protocolo del bridge, versionado) | `CMakeLists.txt` (`NEURONIK_BRIDGE_PROTOCOL_JSON`), `Tests/BridgeProtocolContractTest.cpp`, `Tests/bridgeProtocolContractTest.mjs` | `WebUI/contracts/` o `Source/WebUI/contracts/` |
| `WebPilot/public/` (**es el `publicDir` de la WebUI**) | `WebUI/vite.config.js`, `build_wasm.bat` (`sync-wasm.mjs` escribe ahí el `.wasm`), y de ahí sale `WebUI/dist/worklet/` | `WebUI/public/`, con `vite.config.js`, `sync-wasm.mjs` y los guards de staleness repuntados |

Además, dos consumidores leen **rutas del piloto como fuente**:
`Tests/bridgeProtocolContractTest.mjs` lee `WebPilot/lib/bridge.js` (ya portado a
`WebUI/src/bridge/bridgeCore.js`) y `Tests/webviewBridgeDirectionTest.mjs` declara
`emitters: ['WebPilotHost.cpp']` (el emisor pasa a ser el componente web del editor). Los dos
hay que repuntarlos **en el mismo commit** que borra los ficheros, o la suite rompe.

- **Orden:** ~~2c (selftest al plugin)~~ **HECHO 2026-09-19** → ~~commit de retirada~~
  **HECHO 2026-09-19**. 8.2 sigue con el campo libre.
- **DoD — comprobado:** `grep -rn "WebPilot"` fuera del histórico y de los comentarios que citan
  el traslado no devuelve ninguna dependencia viva (lo único que queda con ese nombre es el
  propio fichero/target de la bancada, que se conserva a propósito, y el campo `nativeTransport`
  del contrato, que apunta a él); `build.bat` va de 10 a **9 pasos** (sin la exportación React;
  el host sigue, porque la bancada se queda); **21/21 ctest** y el selftest corre las seis
  direcciones sin omitidos en las dos superficies.

**Lo que cambió respecto al plan, y por qué (2026-09-19, al hacer el commit):**

| Pieza | Plan | Qué se hizo |
|---|---|---|
| La bancada | borrarla | **se queda**, sin `--pilot-page`, sin snapshot embebido y sin el `FATAL_ERROR` que exigía `WebPilot/out`. Al mirar el árbol: es la ÚNICA superficie que monta `ParameterPanel` + `XYPad` (el comentario del procesador ya lo decía: *"its only consumer"*) y la única que mide el arranque (`--auto-quit` → `pilot-startup.log`). Borrarla se habría llevado por delante el pad XY y las métricas, que es justo lo que NO se quería perder |
| `generated/` | `WebUI/generated/` o `contracts/generated/` | `WebUI/generated/` (los 3 consumidores repuntados: `CMakeLists.txt`, `ParameterExportTool.cpp`, `build.bat`) |
| `bridge-protocol.json` | `WebUI/contracts/` o `Source/WebUI/contracts/` | `WebUI/contracts/`; su especificación (`BRIDGE_PROTOCOL.md`) a **`DOCS/`**, y `WEB_PILOT.md` → **`DOCS/PILOT_RETIRED.md`** con un aviso de que el piloto ya no existe |
| `public/` | `WebUI/public/` | `WebUI/public/`, con `vite.config.js`, `sync-wasm.mjs` (mudado a `WebUI/scripts/` y con `pnpm sync:wasm`) y los guards de staleness repuntados |
| `webviewBridgeDirectionTest.mjs` | repuntar al emisor nuevo | lista **los dos** emisores: `WebPilotHost.cpp` **y** `WebUI/NeuronikWebView.h` (el editor emite desde 8.1; el guard solo vigilaba uno) |
| El workspace pnpm | — (no estaba en el plan) | **hallazgo**: los enlaces de `WebUI/node_modules/@abdsynths/*` los daba el workspace anidado del piloto (`WebPilot/pnpm-workspace.yaml`, que listaba `'../WebUI'`). Sin él, `pnpm install` en WebUI camina al workspace raíz de la suite (que no la lista) y deja `node_modules` sin enlaces. La WebUI estrena el suyo (`WebUI/pnpm-workspace.yaml` + `WebUI/pnpm-lock.yaml`) |
| El contrato del protocolo | — | su lista `implementations` nombraba ficheros muertos (`WebPilot/lib/bridge.js`, `WebPilot/app/page.jsx`) y el test C++ **exige que existan**: ahora son `WebUI/src/bridge/bridgeCore.js`, `WebUI/src/app.js` y `Source/WebUI/NeuronikWebView.h` |

**8.2 Paridad de control (los 70 parámetros) — LIENZO ÚNICO hecho 2026-09-19**

**Decisión revisada de la primera pasada: sin pestañas de parámetros.** Los hermanos de la
suite no lo hacen así (ABDMS2000: editor de 1080×680 con `slideDrawer` por sección; ABDEep:
1200×768; el patrón "ficha con lo principal + panel deslizante" es el de MS2000/EEP/CZ101),
pero NEURONiK tiene 70 parámetros y el encaje se MIDIÓ antes de decidir: caben en un lienzo de
**1440×900** con el dial a 48 px, así que no se parte en fichas-con-cajón. El cajón lateral
sigue siendo la salida natural si una sección crece una fila de más (el reparto es dato:
`sections.js` y su test de encaje lo dirán antes de que nadie lo vea recortado).

- [x] `src/contracts/sections.js`: reparto y geometría como SSOT — 7 fichas en 3 bandas de 12
      carriles, todas a 2 filas, con `canvasHeight()` = la cuenta del encaje.
- [x] Los 70 parámetros con la **familia compartida**: 46 floats a `Knob`, 5 bools a `Toggle`,
      19 listas a desplegable. `masterLevel` sigue siendo un `range` nativo (es el control base
      del `--selftest` del host: 8.1 paso 2c), y es el único fuera de la familia.
- [x] Los `dspStatus != implemented` se marcan, no se esconden (`cell--divergent`).
- [x] `tests/sections.test.js`: encaje medido (alto calculado ≤ lienzo, fichas a 2 filas,
      bandas que llenan el ancho) y geometría de la CSS igual a la del reparto.
- [x] Medición en motor real (Chrome headless a 1440×900): **0 px de desborde**, sin scroll;
      70 celdas (45 knob / 5 toggle / 19 select), teclado de 36 teclas y rueda de modulación;
      primer `input[type=range]` = `masterLevel`. La primera cuenta dejaba fuera bordes,
      huecos de fila y relleno del armazón: desbordaba 33 px y lo cazó esta medición.
- [x] **`Select` compartido y en las 19 listas (2026-09-19).** El hueco está cerrado: la familia
      tiene `components/select.js` (`ABDSharedAssets`), y el lienzo ya no construye ningún
      `<select>` a mano. Modelo de valor por ÍNDICE (el hermano discreto del boolean de `Toggle`)
      porque en un `choice` el mapeo índice<->normalizado lleva el skew del propio parámetro y
      meterlo aquí arrastraría matemática del APVTS a la capa compartida. Sigue siendo un
      `<select>` nativo por teclado/lectores/WebView2, con `options` que aceptan
      `{ label, disabled, note }` y `setDisabled()` dinámico (lo que el gating necesita). Es el
      único de la familia sin `drag-core`: arrastrar por 28 opciones elige por accidente, y un
      test fija que un drag no cambia el valor. Choca de nombre con `.abd-select` de
      `controls.css` (la librería CSS previa): el layout del bloque es opt-in
      (`.abd-select--labelled`), así que un `<select>` suelto sigue viéndose igual.
- [x] **A/B contra el nativo con el mismo preset (2026-09-19).** `Tests/nativePanelParityReport.mjs`
      extrae los DOS inventarios de sus fuentes (los patrones de `Source/UI/**` y el contrato +
      `sections.js`) y los cruza id a id con el preset cargado (INIT del contrato, o un
      `.neuronikpreset` real por `--preset`). El informe está anotado en
      **`DOCS/WEBUI_VS_NATIVE_PARITY.md`**; el script corre en ctest como
      `NEURONiK_NativePanelParity` (test 19) por sus dos invariantes: ningún id del lienzo ni del
      C++ fuera del contrato, y ninguna celda repetida. Con el INIT: web **70/70**, nativo
      **66/70** con algún control y **1/70** montado en lo que se envía (los paneles están
      compilados y sin instanciar: se retiran en 8.4), **4** parámetros sin control nativo en
      ningún sitio (`oscLevel`, `midiThru`, `velocityCurve`, `unisonEnabled`), **41/66** etiquetas
      y **4/66** tipos distintos.
      Lo que el A/B cambia en esta lista: el nativo filtra los **destinos de la matriz por motor**
      (por ÍNDICE, en un timer) y la página no consume `engines`, que es el único hueco del DoD
      que es **función** y no presentación; el resto (etiquetas abreviadas, `juce::String(v, 2)`
      sin unidad) es cosmética deliberada de un panel que se retira.
- [x] **RANDOMIZE fuera del panel y con el bug de unidades corregido (2026-09-19).** Vivía en
      `ParameterPanel::randomizeParameters()` y se iba a perder con el panel. Ahora es
      `State/ParameterRandomizer` (tabla de intención + congelados, **sin procesador ni UI**) y el
      puente gana la acción `randomize`; la página tiene el botón en la cabecera de GLOBAL &
      MASTER y el store no lo ofrece sin host. Dos defectos corregidos con test
      (`NEURONiK_ParameterRandomizerTest`, test 20): la mezcla promediaba REAL con NORMALIZADO
      (`jmap(strength, currentValue, random0to1)`, que clavaba el cutoff en 20 kHz) y la ventana de
      `resonatorRes` (0.3..0.95) **no cabía** en su parámetro (0.5..1). El test exige ahora que
      cada ventana quepa, que el sorteo no toque lo congelado, que a fuerza 0 no cambie nada y que
      la mezcla sea lineal en Hz.
- [x] **Primera retirada del árbol muerto nativo (2026-09-19).** Borrados de `CMakeLists.txt` y
      del árbol los **cuatro paneles de parámetros**, el browser de presets, el LCD y los
      visualizadores que nadie instanciaba (`SpectralVisualizer`, `EnvelopeVisualizer`). OJO: el
      overload de `ParameterPanel` sobre `VerticalSliderControl` **no era muerto** (es el master
      vertical), así que se queda, igual que el panel y el XYPad que la bancada monta. Con
      `ModulationPanel` se va su `timerCallback`, que **reescribía el APVTS** (destino de
      modulación → Off) cada 100 ms. Quedan `ParameterPanel` y `XYPad` porque la bancada los
      monta; el A/B (`NEURONiK_NativePanelParity`) pasa de 66 a **15** ids nativos y **exige** que
      su lista de superficies siga al árbol.
- **DoD:** un preset cargado tiene que dar el **mismo valor real** en las dos superficies (lo da:
  mismo APVTS y misma matemática del contrato) y ninguna función de la UI puede mover un parámetro
  que el motor ignore en silencio. La lectura literal de "se ve idéntico" **no aplica** y queda
  descartada con evidencia: el nativo abrevia las etiquetas (41/66) y lee los valores con dos
  decimales sin unidad, y de los 70 parámetros solo **1** tiene control nativo montado. Lo que
  queda de 8.2 es lo que no es "un parámetro" (8.3), más estos flecos abiertos:
  **el anillo del valor modulado** (fila de arriba de la tabla: el dato existe en el procesador,
  pero NO viaja en el cable — ver el fleco abierto más abajo).
- [x] **Reparto del lienzo tras las mudanzas 8.3 (estado 2026-09-22).** Tres bandas de fichas
      (carriles, suma comprobada por test): **OSCILADOR 6 + RESONADOR 2 + FILTRO 4** (12),
      **ENVOLVENTES 5 + MODELOS A–D 2 + EFECTOS 5** (12; MODELOS al centro entre envolventes
      y efectos — el morfeo es el corazón del motor — y en el lienzo solo el pad XY, el
      espectral y las 4 ranuras viven en el cajón), **LFO 2 + MATRIZ 6 + GLOBAL & MASTER 4**
      (12, fondo). Los cajones EDIT (MODELOS, LFO, MATRIZ, GLOBAL) parten el detalle sin
      mover los 70 controles: cada id sigue en SU sección, el store y la cobertura no cambian.
- [x] **Curva ADSR en la ficha FILTRO & ENVOLVENTE (2026-09-19).** `src/ui/envelopeCurve.js`
      (matemática pura + pintor SVG) dibuja la envolvente de amplitud desde los cuatro `env*`, y va
      en la **celda libre** de la ficha (11 controles en 6x2): el encaje no cambia, y un test lo
      exige (`ids + 1 <= columns * rows`). No es una celda de parámetro (`.card__visual`, no
      `.cell`), así que "70 celdas" sigue significando lo mismo. Los tiempos se comprimen con √
      (1 ms a 5 s en el mismo ancho) y el sostenido tiene tramo propio. `EnvelopeVisualizer` nativo
      ya no existe: la web es la única que lo dibuja.
- [x] **La curva ADSR pasa al paquete compartido (2026-09-29).** `src/ui/envelopeCurve.js` se
      **borró**: el dibujo lo da `@abdsynths/shared` (`createEnvelopeCurve` +
      `styles/components/envelope.css`) y esta web no guarda copia — Zero-Copy aplicado a un
      componente, no a un asset. Enriquecida con el editor de tres esquinas del Mz950
      (AGPLv3, reescrito limpio: el sustain es la altura de la esquina del decay, y arrastra
      las dos cosas). Aquí se queda su SSOT: que ids, la matriz de skew y el `captionClass`.
      `ficha envelopes` y `cajon envelopes` quedan pixel a pixel contra su referencia.
- [x] **Gating de destinos por motor en el CONTRATO (2026-09-19).** La página ya no ofrece los 28
      destinos siempre: los 4 `mod*Destination` deshabilitan las opciones que el motor activo no
      consume, con el motivo en la opción (`title`), y **nunca reescriben el valor** — una selección
      que no vale para el motor activo se queda y se marca (`.abd-select[data-divergent]`), que era
      exactamente la corrupción silenciosa del timer nativo. La fuente es el contrato: `optionEngines`
      por opción + `engineParameter` (el id del selector), derivados en C++ de la **tabla única** de
      destinos (`State/ParameterDefinitions.h`: etiqueta + parámetro que mueve, en orden) cruzada con
      `engineCoverageFor()`, más la cobertura de cada opción del selector. Con eso el reparto de
      neuronik/neurotik sale por índice EXACTO del que tenía el panel retirado (**2 3 10-16 20 21 22**
      neuronik, **23-26** neurotik, 12 de los dos), y el test de contrato lo pincha además de las
      etiquetas por índice (¡el índice es estado de preset!) y de que `getModDestinations()` liste la
      tabla. Un recorrido de humo nuevo: la tabla de `Source/Main/ModulationTargets.h` decía "los ids
      deben cuadrar con el índice" y ya no tiene que decirlo a mano.
- [x] **Matriz de modulación al cajón lateral (2026-09-19).** Con 70 controles a la vez el lienzo se
      leía apretado, así que una ficha puede declarar `drawer` (`sections.js`): sus celdas se montan en
      un cajón (`src/ui/drawer.js`, patrón de ABDMS2000/ABDEep/ABDCZ101 — panel fijo a la derecha,
      fondo, ESC) y en el lienzo queda el **resumen de las 4 rutas** (`src/ui/modSummary.js`) más el
      botón que lo abre. Dos diferencias con el de los hermanos, deliberadas y escritas en la
      cabecera: el contenido NO se reconstruye al abrir (las 70 celdas están siempre en el documento:
      el selftest y la suite cuentan celdas, y reconstruir perdería el gesto en curso), y abrir/cerrar
      es una clase CSS. Los ids siguen en el reparto, así que el store, el recuento y la cobertura de
      los 70 no cambian. Medido en Chrome: **0 px de desborde** con el cajón fuera de pantalla, 12
      celdas en el cajón y **0** en la rejilla, 4 rutas en el resumen, `masterLevel` sigue siendo el
      primer `range`.
      - **Verificado con el cajón ABIERTO y la matriz EN USO (2026-09-19).** Es la dirección **0 del
        selftest** (corre la primera): configura una ruta real por el APVTS (LFO 1 → Filter Cutoff,
        cantidad +0.5 — los índices salen de la tabla de destinos y de la lista de fuentes, no
        escritos a mano), **pulsa el disparador** de la ficha y lee del cajón abierto sus 4 rutas,
        sus 12 celdas y los controles de la ruta 1 (fuente y destino como `selectedIndex`, la
        cantidad en el `aria-valuenow` del dial). El cajón se queda **abierto a propósito**, así que
        las otras cinco direcciones corren con la matriz en uso y el lienzo tapado: es la regresión
        que importa (un anclaje que deja de ser el primero del documento, un cajón que roba el foco,
        un poll que deja de empujar). Y el arnés **se cazó a sí mismo**: la primera versión comparaba
        `data-drawer` con `data-drawer-trigger`, que no son el mismo id (uno lleva el prefijo
        `drawer-` del DOM y el otro el de la sección) → la dirección falló en cuanto se corrió, sin
        dar el OK que parecía.
- [x] **Slots de modelo A–D y su carga (2026-09-19).** Los `loadA..loadD` del panel nativo (el
      bloque MODEL de `OscillatorPanel`: cuatro botones + botones de fichero `*.neuronikmodel`) son
      una ficha propia del lienzo, **MODELOS A–D** (`span: 2` en la banda del LFO, que pasa a
      compartir con la matriz: la matriz baja de 8 a 6 carriles porque en el lienzo solo alberga el
      resumen). No son parámetros —una ranura es del MOTOR: un preset lleva `modelPath<slot>`, no una
      copia de los parciales— así que la ficha **no tiene celdas**: el encaje de los 70 no se mueve
      (y un test lo fija, junto al reparto de esa banda). La carga es una acción nueva del puente,
      `loadModel`, porque la página no tiene sistema de ficheros ni puede nombrar rutas: **el diálogo
      lo abre el host** (`EngineModelsAdapter` con el `juce::FileChooser` que tenía el panel nativo) y
      contesta ASINCRONAMENTE con un `modelsState` fresco (con `name` por ranura, para que las dos
      superficies muestren la misma lista) o con `modelError` (cancelado, fichero inservible, ranura
      fuera de rango) — una carga nunca falla en silencio, que era el defecto que dejaba una ranura
      con nombre y sin sonido. Medido en Chrome a 1440x900: **0 px de desborde** con la ficha nueva,
      70 celdas, 8 fichas y `masterLevel` como primer `range`.
      - **Verificado cargando de verdad (2026-09-19), y destapó un fallo viejo:** el lector de
        modelos solo entendía un dialecto XML (`<NEURONIK_MODEL amplitudes=".." offsets=".."/>`) y
        **el ModelMaker escribe JSON** (`{amplitudes[64], frequencyOffsets[64], name, description}`),
        así que los ficheros de la propia herramienta NO cargaban. Ahora se leen los dos dialectos y
        lo que no es un modelo se rechaza, sin tocar la ranura. `Tests/ModelSlotTest.cpp` (**26
        comprobaciones**, con el procesador real) pincha el formato, las cuatro ranuras (y el rechazo
        sin renombrar), la recarga desde un preset (`modelPath<slot>`), el `modelsState` del adaptador
        real (con `name` e `isValid`, y sin abrir ningún diálogo), el **suena** (con una nota sonando,
        cada esquina de morph (A 0,0 · B 1,0 · C 0,1 · D 1,1) tiene en la tabla de parciales del motor
        el parcial de SU ranura y ninguno de los otros, con RMS de audio real) y **cambiar de motor
        antes de que el host fije la tasa** (un preset restaurado antes de `prepareToPlay` puede
        mover `engineType`, y el motor nuevo se construía preparado con 0: divisores a cero y
        `0xC0000094` en el primer bloque con nota — era el crash que este test destapó, y por eso
        `parameterChanged` ya no prepara un motor cuando `getSampleRate()` es 0).
      - **La ficha se ve en la PÁGINA del plugin:** es la quinta dirección del selftest
        (`BridgeSelftest.h`): escribe los cuatro modelos (en el JSON del ModelMaker, a propósito — si
        el plugin dejase de entender ESE formato, la dirección lo diría en voz alta), los carga por
        `NEURONiKProcessor::loadModel` y lee los cuatro nombres en la ficha de la página real. Un
        proceso con ventana no puede medir su propia salida de audio sin pelearse con el hilo del
        host, y por eso "suena" vive en el test y "se ve" en el arnés.
      - **Qué pasó con "dónde SÍ y dónde NO":** mientras el piloto estuvo en el árbol, su página
        (anterior a la ficha) obligó a que el dueño del arnés pudiera declarar una dirección NO
        APLICABLE —`BridgeSelftest::PageCapabilities::retiredPilotPage()`, con la capacidad completa
        como defecto—. **Esa maquinaria se retiró con el piloto (ticket 8.4)**: `--pilot-page` ya no
        existe, no hay una segunda página a la que rebajar el listón y las seis direcciones son
        obligatorias en las dos superficies. `Tests/webuiSelftestContractTest.mjs` fija que no
        vuelva: ni `PageCapabilities`, ni `retiredPilotPage`, ni ninguna marca de omitido.
- [ ] **Anillo del valor modulado (fleco abierto; el dato ahora TAMBIÉN viaja: `telemetryFrame.modulation[]` desde el 2026-09-20).** El procesador SÍ
      publica la modulación viva (`NEURONiKProcessor::getModulationValueForUI()` sobre
      `modulationValues[]`, que llenaba el `ModulatedSlider` nativo), pero **no viaja en el cable**:
      el protocolo del puente no tiene canal de modulación. Hacerlo bien es un cambio de frontera
      (controlador + mensaje aditivo tipo `midiNoteState` + poll del host en C++, estado en el store,
      y el anillo como opción del `Knob` compartido) y por eso queda fuera de esta pasada: dibujar el
      anillo con la CANTIDAD del slot no sería el valor modulado, sería otra cosa con el mismo nombre.

- [x] **Separación FILTRO / ENVOLVENTES y ENV 1/ENV 2 como fuentes de la matriz (2026-09-22).**
      La antigua ficha FILTRO & ENVOLVENTE (11 controles) se parte en DOS: **FILTRO**
      (`filterCutoff`, `filterRes`, `filterEnvAmount` — banda del motor, tras el RESONADOR) y
      **ENVOLVENTES** (las dos ADSR completas + sus curvas dibujadas, junto a MODELOS A–D). La
      envolvente del filtro deja de estar enterrada: **ENV 1 (amplitud) y ENV 2 (filtro) son las
      fuentes 6 y 7 de la matriz** — añadidas AL FINAL de `getModSources()` a propósito, porque
      los choices se guardan por índice y insertar en medio re-mapearía los presets guardados.
      Tres piezas y su prueba:
      - **Preset NUEVO:** `mod1Source` default = 6 (ENV 1 → destino 1, Osc Level) y `mod2Source`
        default = 7 (ENV 2 → destino 10, Filter Cutoff), amounts 1.0: toda ruta nueva nace con
        las envolventes cableadas, visibles y editables en la matriz.
      - **Preset EXISTENTE:** `insertEnvModRoutes` (`PresetMigration`, invocada desde
        `PresetManager` al cargar) inserta ENV1→Level y ENV2→Cutoff en la primera ranura libre
        (mod1, luego mod2; solo si source y destino están a 0/Off). Sin sitio, no toca nada. El
        sonido es IDÉNTICO: en el DSP "sin ruta" = factor de routing 1.0 (sentinela en
        `AdditiveVoice`/`NeurotikVoice`), exactamente el cableado hard-wired de siempre — la
        migración solo lo hace VISIBLE y editable. `filterEnvAmount` sigue vivo como profundidad
        (y su default pasó 0 → 1.0: con 0 la ruta insertada nacía MUDA — 0 × amount = 0 —,
        defecto que la prueba de escala destapó; solo presets nuevos, la migración no toca el
        valor guardado). Nuevos destinos 12/13 (Flt Attack/Decay) responden solo a ENV 2.
      - **DSP:** ambas voces aplican los factores con semántica de REEMPLAZO dentro del
        sumatorio (ENV2: `fEnv × (fEnvAmount × amount) × 18000 Hz` sobre el cutoff; ENV1 sobre el
        nivel del VCA) — amount 1.0 reproduce el sonido de siempre, bit-exacto.
      - **Prueba:** `PresetRoundTripTest` añade la sección "ENV route migration" (inserta 2,
        índices 6→1 y 7→10, amounts 1.0, ningún otro parámetro tocado, ranuras ocupadas → 0,
        `filterEnvAmount` sobrevive); paridad WASM↔nativo bit-exacta 9/9 y selftest E2E de seis
        direcciones en verde tras el cambio de defaults. Nota estructural: ENV2→Cutoff solo
        actúa en el motor aditivo (NeurotikVoice no tiene filtro ni envolvente de filtro —
        mejorable, apuntado en 9).

**8.3 Lo que no es "un parámetro"** (aquí está el grueso del trabajo)
- [ ] **Presets**: navegador con lista/categorías, guardar, renombrar, borrar y estado del
      preset actual (el `PresetBrowser` nativo, hoy reducido a una barra select+save).
- [ ] **LCD + navegación tipo hardware**: `LcdDisplay` + `LcdMenuManager` + D-pad
      (MENU/OK/flechas) y los botones de comando. Es lo que da carácter al instrumento; si se
      simplifica, que sea una decisión escrita, no un olvido.
      **La MECÁNICA ya existe (nota 2026-09-20)**: la familia LCD universal vive en
      `@abdsynths/shared` (lcdMachine: máquina pura Idle/Nav/Edit con árbol inyectado y hooks;
      lcdScreen: ping-pong + preview + cola con prioridad; lcdPanel: D-pad con hold-repeat) y su
      gemelo C++ en `ABDSharedCode/LcdDisplay` (`ABDShared::LcdDisplay`). Queda lo del synth:
      el árbol GLOBAL/RESONATOR/FILTER/EFFECTS/MIDI CONTROL (recuperable del `LcdMenuManager.h`
      retirado, en git) y los hooks al bridge. Guía: `ABDSharedAssets/docs/LCD_GUIDE.md`.
- [ ] **Visualización en vivo**: `SpectralVisualizer`, scope flotante y `XYPad` (morph X/Y).
      Requiere un canal de datos de solo lectura nativo→web a ~30-60 Hz, con presupuesto de
      CPU medido y sin asignar en el hilo de audio (snapshot con `AudioThreadSnapshot`).
      El SCOPE exige ademas un campo `wave[]` en el frame de telemetria (auditado
      2026-09-21: hoy el frame lleva spectral/envelopes/lfos/modulation/morph pero
      NO forma de onda) — cambiar con la SSOT del exportador del canal.
  - [x] **El XYPad ya está** (2026-09-20, SIN el canal): la ficha MODELOS A–D monta el pad
        dibujado con los nombres en esquinas; edita y refleja morphX/morphY por el puente de
        parámetros que ya existía. El espectral y el scope sí necesitan el canal de lectura.
- [ ] **MIDI Learn**: `MidiLearner` + `MidiMappingManager` (aprender, asignar, borrar,
      persistir). El aprendizaje es UI (la web pide "aprende el próximo CC"); el mapeo y su
      guardado siguen en el procesador.
- [ ] **Diálogos y menú**: ayuda, especificaciones MIDI, y el zoom manual (el ajuste automático al viewport ya está: `fitStage`, 2026-09-20).
- **DoD:** no queda ninguna función de la UI nativa sin equivalente web; la lista de arriba
  está toda tachada o con su "no se migra porque…" escrito en este documento.

- **Más adelante (apuntado 2026-09-20, fuera de este DoD):** **modo claro conmutado desde un
  menú "View"** en la nav-bar, al estilo de otros synths de la suite. Referencia de temas:
  `ABDMS2000/WebUI/src/styles/themes.css` (`data-theme` + tokens). El fondo ya es tintable
  (`backgrounds.css`: `--abd-bg-tint`), así que el grueso es UN NUEVO JUEGO DE TOKENS + el
  ítem de menú — no CSS nuevo. Cuándo y con qué alcance, por decidir.

**8.4 Retirada del panel nativo**
- [ ] Borrar `Source/UI/**` (paneles, visualizadores, tema, LCD, browser, MIDI learner) y las
      dependencias del editor (`MenuBarComponent`, `MidiKeyboardComponent`, `TabbedComponent`,
      `FileChooser`, `Timer`, listener de `MidiKeyboardState`).
- [ ] Limpiar `CMakeLists.txt` (fuentes y enlaces de juce_gui_basics/gui_extra que ya no hagan
      falta), y decidir el destino de `Source/WebPilotHost.cpp` y su target: banco de pruebas
      de desarrollo o borrado.
- [ ] Adaptar los tests que hoy dependen de la UI nativa.
- **DoD:** `grep -r "Source/UI"` en CMake no devuelve nada; el plugin compila sin paneles
  nativos y el editor sigue siendo el mismo binario de antes (más pequeño).

**8.5 Cierre**
- [ ] **pluginval nivel 10** con la UI web (es el criterio de la Fase 0 y del ecosistema).
- [ ] **Arranque del editor**: medido, no estimado. El dato que ya tenemos es ~7 s en frío en
      el host del piloto (lo domina el arranque de WebView2): si al abrir el editor el hueco en
      blanco es perceptible, hay que pintar algo nativo mientras carga, y eso se mide aquí.
- [ ] **Dependencia de WebView2**: el runtime de Edge no está garantizado en toda máquina
      Windows. Decidir qué hace el plugin si falta (aviso en el hueco del editor con enlace al
      instalador; nunca un crash ni una ventana vacía).
- [ ] **Tamaño del bundle**: el piloto va en 306 KB de JS + 89 KB de WASM; el objective es que
      la UI real no lo duplique: un bundle, un motor de UI, sin copias de componentes
      compartidos.
- [ ] Revisar que la migración no revive homonimias: al retirar `Source/UI` desaparecen
      `PresetBrowser.h`, `PresetManager` de UI y los `PluginEditor_ResourceProvider` propios del
      inventario de `ABDSharedCode/docs/homonimias-cabeceras.md`.

### Riesgos de la Fase 8

| Riesgo | Mitigación |
|---|---|
| Se subestima 8.3: los visualizadores y el MIDI Learn no son "poner knobs" | Es la fase más larga y va después de la paridad de control, no mezclada con ella |
| El hueco en blanco al abrir el editor (WebView2 frío) se percibe como que el plugin no carga | Medir en 8.5 y pintar un estado de carga nativo mientras el browser arranca |
| Dos motores sonando si el worklet se activa dentro del plugin | Fijado en código (2026-09-19): `WebUI/src/audio/policy.js` decide por `window.__JUCE__` y el motor del worklet se niega a arrancar dentro de un host; la misma guarda en la página del piloto. Falta la comprobación E2E, que cae en 8.1 con el selftest del editor |
| El hot-reload de dev funciona en el host del piloto y no en el editor del plugin (rutas relativas del VST3) | ResourceProvider con fallback embebido, probado en 8.1 en los dos formatos |
| Retirar el nativo antes de tener paridad deja el plugin sin UI usable | El nativo no se borra hasta que 8.2 y 8.3 estén tachadas; durante la migración la página es la principal y el nativo el respaldo, y el borrado es el último paso con su propio commit |

**9. ModelMaker a web (FUTURA — se planifica, no se empieza hasta cerrar la Fase 8)**

Apuntada 2026-09-20. Hoy `NEURONiK_ModelMaker` es una app JUCE independiente (WIN32, fuera
del build por defecto a propósito: es otro entregable; se compila con `build.bat modelmaker`,
y su `Version.h` SOLO se incrementa en builds marcadas release (`build.bat modelmaker
release`, mecanismo cambiado el 2026-09-20). Flujo: LOAD AUDIO → análisis espectral de 64 parciales con
detección de pitch → A/B PLAY ORIGINAL/PLAY MODEL (comparte `Oscillator`/`Resonator` con el
plugin) → REC → EXPORT `*.neuronikmodel` (JSON `{amplitudes[64], frequencyOffsets[64], name,
description}`). Migrarla a web la libera de Win32 y de la herramienta aparte (el
C4996 de JUCE 8 se resolvió el 2026-09-20: export migrado a AudioFormatWriterOptions).

- [x] **Mejorar el ANALIZADOR (auditado 2026-09-21, CERRADO 2026-09-22).** Partia de: hoy
      `SpectralAnalyzer::analyze` (a) usa SOLO los primeros 8192 muestras con la
      ventana aplicada sobre un buffer no envasado, (b) muestrea la magnitud en la
      frecuencia armonica EXACTA sin busqueda del pico en bins vecinos y (c) deja
      `frequencyOffsets` a cero (TODO en el codigo) — la mitad del modelo que
      morphean los motores nunca se genera. Plan: analisis multiframe (varias
      ventanas Hop-aligned, amplitud = media/max por parcial), peak-picking local
      (±2 bins alrededor del armonico esperado) e interpolacion parabolica para
      precision sub-bin -> offsets reales. Referencias: la deteccion de pitch (HPS)
      ya existe y se queda; el objetivo es que los modelos ANALIZADOS aprovechen
      el morphX/morphY de verdad.
      **CERRADO (2026-09-22/23):** el analizador hace ya las tres cosas. `analyze` es multiframe
      (media de hasta 6 ventanas repartidas por TODO el fichero, con las de RMS bajo descartadas),
      con peak-picking sub-bin (ventana de media banda + interpolacion parabolica) y
      `frequencyOffsets` REALES (el TODO resuelto). Y `detectPitch` anade el ajuste de rejilla por
      minimos cuadrados sobre los picos de todas las ventanas. Lo pinnea
      `Tests/SpectralAnalyzerTest.cpp`; el detalle vive en la Fase 10.6 y en `DSP_PARAMETERS.md`.
- [ ] **`unisonSpread` esta MUERTO en el motor** (auditado 2026-09-21): expuesto en
      APVTS y WebUI, guardado en Resonator, usado NUNCA en el render — el knob no
      hace nada. O se implementa (spread -> anchura de detune por parcial:
      `f_i * (1 + detune * (1 + spread * i / 64))`, que ademas abre el camino del
      ensanche estereo por capa unison) o se retira del contrato SSOT. Mientras
      tanto es un parametro mentiroso en la UI.

### Fase 10 (IMPLEMENTADA 2026-09-23) — Modelos temporales: N frames + morphZ

Diseño completo en `docs/ARCHITECTURE/TEMPORAL_MODELS_PLAN.MD` (2026-09-22). El salto
al paradigma Neuron: cada slot A-D guarda N frames temporales (1..16, default 1 =
modelo estático bit-compat) y un tercer eje `morphZ` interpola los frames de cada slot
ANTES del morfeo bilineal XY (Z-primero: el gesto temporal nunca inventa timbres fuera
de los slots). UI: sin pad 3D — morphZ es knob en el drawer EDITAR de MODELOS y destino
NUEVO de la matriz de modulación (LFO2 = animación cíclica, ENV2 = evolución por nota,
mod-wheel = NUKE). Formato `.neuronikmodel` v2 compatible (v1 sigue leyéndose; v2 se
lee como v1 por los plugins viejos). Piezas delicadas: emparejamiento de parciales
entre frames (greedy por cercanía) y presupuesto de 8 KB por slot (16 frames x 64 x 2
floats). Plan de fases 10.1-10.6 con verificación por fase en el documento.
Predecesores ya en producción: analizador multiframe sub-bin (cerrado 2026-09-22) y
modo bow del motor modal.

Implementado (2026-09-23): 10.1 struct+serialización v2+sampleFrame (extremos
bit-exactos, camino corto envuelto por frameSpanHz, roundtrip en ModelMakerRoundTripTest);
10.2 morphZ en motor (smoother por voz, destino 28 de la matriz, FrameSamplerTest 11/11,
paridad WASM-nativo 9/9 bit-exacta con defaults); 10.3 WebUI (contrato 72 parámetros,
knob MORPH-Z en el cajón de MODELOS, suite 233/233); 10.4 analizador temporal
(analyzeTemporal: N frames por ventanas del fichero, emparejador por índice, normalización
global que conserva el decaimiento, combo FRAMES en la GUI del ModelMaker,
TemporalAnalysisTest 14/14, sonda con WAVs reales CZ101 → 4 frames v2 → recarga).
Cierre del 10.5 (2026-09-23): el anillo z ya vive en el pad XY de MODELOS (aro SVG en la
capa de wiring de NEURONiK: gesto por ángulo 0..360°, teclado ±0.01/±0.1/Home/End, paint sin
eco; 5 pins en tests/xyPad.test.js, suite 238/238, gesto y teclado verificados en vivo sobre
el dist) y el encaje se cerró por el camino B: morphX/morphY dejan las celdas del oscilador
(el pad XY es su único control del lienzo), RESONADOR agrupa el cuarteto modal (resonancia,
impulso, bow) y el knob MORPH-Z vive en el cajón EDIT. El modelo de ejemplo temporal (CZ-BASS1-temporal,
4 frames, vía sonda) está versionado en Assets/Models/ y EMBEBIDO como asset del plugin
(NEURONiK_SelftestAssets): la dirección MODELOS del selftest del bridge carga ahora el
modelo temporal v2 REAL en la ranura A (revalidado con el lector de producción antes de
entrar en el engine) y los sintéticos estáticos en B-D — las seis direcciones en OK en
las dos superficies (Standalone y bancada WebView2).

**El banco CZ101, jugable de fabrica (2026-09-24):** los modelos de los CINCO patches
estan versionados en `Assets/Models` (seis ficheros: los cuatro patches tonales, el barrido
CZ-RRISE y el temporal de BASS1) y EMBEBIDOS en el plugin como `NEURONiK_FactoryModels`;
`installFactoryPresets()` escribe ademas los SIETE presets en `Documents/NEURONiK/Presets`
(solo si faltan, sustituyendo `{{FACTORY_MODELS}}` por el directorio real de modelos, y los
seis modelos van al mismo directorio). El septimo, **CZ101-BANK**, es el banco entero en UN
preset: los cuatro patches TONALES en las cuatro ranuras (A BASS1, B HAMOG, C PAD1, D SWEP1),
que es el orden que lee el morfeo bilineal del resonador (`lerp(A,B,morphX)` arriba,
`lerp(C,D,morphX)` abajo, `morphY` entre ambos), de modo que el pad XY deja de elegir un
timbre y pasa a tocar el banco: esquina A el bajo, X hacia HAMOG, Y hacia el pad y la
diagonal hacia el SWEP1. Contrato en `FactoryPresetAudioTest` §5: las cuatro ranuras
cargadas por su `modelPath`, la tabla de parciales que publica el motor DISTINTA en la
esquina A y en el centro, y RMS finito en ambas posiciones (el render del banco suma cuatro
modelos, asi que el banco no lleva el check de cents de los presets de un solo modelo).

Cierre del 10.6 (2026-09-23): frames con **f0 por frame** — el modelo "canta" el pitch del
WAV. `SpectralModel` gana `extraF0[15]` (raíz por frame, frame 0 = canónico) y el sampler
interpola la raíz en dominio log (el barrido es lineal en semitonos) entregando el ratio
que el motor multiplica por parcial (`Resonator` y `ResonatorBank`: `partialFreq *=
gridRatio`; 1.0 en modelos estáticos — bit-compat). Formato v2 con `frameF0` opcional por
frame; analizador con `detectPitchFromSpectrum` (HPS por ventana, frame 0 conserva la raíz
global). Verificado: ctest 26/26 (emparejador con armónicos estables, roundtrip de
extraF0, sampler z=0.5 interpola la raíz), suite WebUI 233/233, y la aceptación con
barrido monofónico sintético (293.7→440 Hz): el modelo temporal captura la trayectoria
344.5→387.9→436.0 Hz y el motor la reproduce al mover morphZ.

Hallazgo (2026-09-23): **CZ-RRISE no es polifonía** — material EN CAPAS sobre UNA rejilla
(E1, 41.62 Hz: los 121 picos de 9 ventanas caben con residuo medio 2.8 cents; el "drone"
son n=1..3 y la "voz líder" es la envolvente espectral trepando por n=7..15). Con una
sola rejilla no es representable: la guardia de desviación de pitch del analizador (plegada
en cents sobre (−600,+600] contra la raíz del análisis) lo reporta como material no
cuasi-monotónico en vez de producir un modelo des-afinado en silencio, y la sonda lo
exime del check acústico estático. El diseño de la Fase 11 (separación de capas:
clustering coseno por envolvente, formato v2.1 de layers, morphZ2/3 por capa) vive en
`DOCS/ARCHITECTURE/LAYER_SEPARATION_PLAN.MD`, con su §10 de investigación (NMF medido:
estable pero no semántico sobre rejilla compartida; puerta de plegado de octava como
discriminador mono/bi-rejilla).

**Herramienta permanente — sonda CLI del ModelMaker** (`NEURONiK_ModelMakerRealWavProbe`,
`Tests/ModelMakerRealWavProbe.cpp`, en CMake sin `add_test`: sonda de verificación, no
test). Conduce headless el flujo EXACTO de producción (WAV → detectPitch →
analyze/analyzeTemporal → serialización v2 → PresetManager → sampleFrame → engine real)
y verifica en tres capas: (1) ciclo completo con el engine, (2) validación acústica con
DFT independiente + check de sub-octava (`PROBE_HALVE_F0=1` como demo de disparo), (3)
reporte de la guardia de desviación de pitch. Registro y métricas por WAV en HANDOFF
entradas (n), (r) y (s). Es la vara de medir para cualquier cambio futuro del analizador
o del formato: correrla sobre los cinco WAVs de CZ101 es la batería de no-regresión
acústica de facto.
      RECOMENDACION (2026-09-21): IMPLEMENTAR, no retirar — es la llave del
      widening estereo por voz (parcial i desviado `+spread*i/64` en L y
      `-spread*i/64` en R de la capa unison), el unico hueco real de imagen que
      tiene la suite (voces mono + chorus global). Coste bajo: solo el bucle de
      `updateHarmonics` por canal; la formula ya esta apuntada arriba.

- [ ] **Decidir el alojamiento**: segunda página del MISMO bundle WebUI (ruta aparte; la
      bancada ya sirve `WebUI/dist`) vs app Vite aparte en el workspace. Por defecto, página
      del mismo bundle: cero infraestructura nueva.
- [ ] **Integrarla DENTRO del propio plugin como segunda herramienta** (apuntado 2026-09-20):
      la misma página del bundle, presentada como ventana modal que se abre desde un punto de
      menú "View" en la nav-bar — el mismo menú ya apuntado para el modo claro (nota del 8.3,
      fuera de su DoD). Así el synth no abre otra app: abre su taller. La casilla de alojamiento
      decide dónde VIVE el código (página del bundle vs app aparte); esta decide cómo se PRESENTA
      (modal sobre el lienzo vs página navegable).
- [ ] **Portar el ANÁLISIS al WASM**: la extracción de 64 parciales + detección de pitch es
      C++ propio del ModelMaker y NO vive en el motor WASM (el DSP de playback sí, con
      paridad bit-exacta de la Fase 5). Es el grueso del trabajo técnico de esta fase.
- [ ] **Audio de entrada** por `decodeAudioData` del navegador (sin JUCE `AudioFormat`).
- [ ] **A/B paridad**: PLAY ORIGINAL/PLAY MODEL con el resonador WASM, comparado contra el
      ModelMaker nativo con las reglas de comparación de la casa antes de retirar nada.
- [ ] **Export**: descarga del `.neuronikmodel` en el MISMO dialecto JSON que lee el plugin
      (el selftest ya escribe ese dialecto a propósito — es el test de contrato del lado
      receptor). `File System Access API` o `<a download>`.
- [ ] **Destino del exe nativo**: herramienta de referencia (congelada) o retirada estilo
      8.4, con su commit propio y su nota.


---

### Fase 11 (EN MARCHA 2026-09-24) — Separación de capas: el material polifónico de una sola rejilla

Diseño completo en `DOCS/ARCHITECTURE/LAYER_SEPARATION_PLAN.MD` (2026-09-23) con su §10 de
investigación. La evidencia medida sobre CZ-RRISE (121 picos de 9 ventanas en UNA rejilla
E1 = 41,62 Hz: el "drone" son n=1..3 y la "voz líder" es la envolvente trepando por
n=7..15) dice que **no es polifonía de dos raíces**: son CAPAS espectrales sobre la MISMA
serie armónica. El motor las sumará después del sampler, cada una con su morphZ
(morphZ2/3 como destinos nuevos de la matriz) y su peso temporal `w_l[k]`; con una sola
capa el camino es el actual bit a bit. Plan 11.1–11.5 con verificación por fase en el
documento.

Implementado la **11.1** (2026-09-24): struct de capas aditivo (`layerCount`, `extraLayers`,
`layerWeights`, `layerFrameWeights`, `layerNames` + API uniforme de capas; la capa 0 ES la
raíz, así que un lector v1/v2 sigue sonando esa capa), formato `.neuronikmodel` **v2.1**
(bloque `layers` opcional con name/weight/frames/frameWeights y `format: 2.1`; el v2 puro
no cambia ni un byte; máximo 3 capas, truncado como con los frames), **un solo escritor**
del dialecto (`Source/Common/SpectralModelWriter.h`: la GUI, el roundtrip y la sonda
dejaban de tener tres copias) y **clamp del v1 WASM** (`neuronikLoadModel` →
`layerCount = 1`, el bloque por slot sigue en ~33 KB). Consecuencia nativa medida: el
modelo pasa de ~8 KB a ~25 KB y `NEURONiKProcessor` tocaba el megabyte (32 modelos INLINE
en su cola de comandos = 800 KB) — dos instancias en un mismo `main` tumbaban
`StatePersistenceTest` y `ModelSlotTest` al arrancar. La cola pasa al heap y el procesador
queda con un `static_assert` de talla. Verificado: `ctest` 28/28, paridad A–E bit-exacta
(5×9, 0 ulps; referencia nativa idéntica byte a byte) y selftest de la bancada en vivo
EXIT=0.

Implementado la **11.2** (2026-09-25): el clustering por forma de envolvente
(`Source/ModelMaker/Analysis/LayerClustering.h`, modulo puro y determinista: descriptores
soporte/entropia -> aglomerativo de afinidad media -> medoide -> clamp a 3 -> guardia ->
orden) y su cableado en `SpectralAnalyzer::analyzeTemporal`, que ahora lee el MISMO espectro
una segunda vez contra la rejilla COMUN del llamador y, con >= 2 capas, monta el modelo por
capas: cada capa escribe solo sus indices, comparte la f0 comun, lleva pesos temporales = su
envolvente (pico 1) y la capa 0 —la RAIZ que oye un lector v2 viejo— es la mas PERSISTENTE.
Dos desviaciones MEDIDAS del diseno: la metrica que decide son los descriptores y no el
coseno de §3.3 (medido: el coseno entre el drone del RRISE y n7 da 0.69 ≥ 0.55, los
fusionaria) y una capa "floja" solo se reabsorbe si ademas es episodica (soporte < 0.75;
medido: con la voz 20x mas fuerte el drone baja del 10 % de la energia y la guardia
energetica sola se lo comia, dejando el barrido en la raiz). Ademas la puerta de rejilla:
material cuyo pitch barre (> 100 cents entre ventanas) no se parte — el test lo pinnea con un
barrido fuerte encima. Verificado: `NEURONiK_LayerClusteringTest` (10 casos, 29 checks) en
verde y `ctest` **29/29**; sobre el banco real CZ101, SWEP1 es el unico WAV que toma el camino
de capas (2 capas: cuerpo + banda que barre) y RRISE sigue en 1 por la puerta (102 -> 601 Hz
por ventana). Los assets versionados no se regeneran hasta la 11.3 (el motor que suma capas)
y la 11.5 (el ejemplo de capas en assets).

**Decisión cerrada (2026-09-25) — el suelo de detección de E1: la vía es la f0 MANUAL.** E1
(41.62 Hz) queda fuera del ancla del HPS (50 Hz cuantizados al bin = 48,45 Hz) y el estimador
lee su 2º armónico (83,2 Hz; 501 cents de error acústico con la rejilla automática). Las dos
alternativas al f0 a mano se midieron y **ninguna se queda**: bajar el ancla a 35 Hz es un
**no-op** (16/16 modelos del banco + E1 byte a byte idénticos: el ancla de la firma de bajo es
de todos modos su 2º armónico, y lo que ata es la puerta de "una octava abajo", ±60 cents
cuando un bin a 40 Hz mide 130) y abrir esa puerta arregla E1 pero **rompe CZ-SWEP1** (62,6 Hz,
una octava abajo: el caso para el que se afinó el HPS de 2026-09-23). Aplicada la vía manual:
`refineGrid()` pule el f0 escrito a mano sobre los picos de todas las ventanas, `analyzeTemporal`
**ya no re-estima la f0 por ventana por debajo del suelo** (antes un modelo temporal de E1 salía
con la rejilla del usuario en el frame 0 y una octava arriba en los demás: medido 41,6 / 83,2 /
83,2 / 83,2), el chequeo de discrepancia del ModelMaker deja de pisar la rejilla a mano y el
indicador declara el caso con el residuo de ESA rejilla. Verificado con `SpectralAnalyzerTest`
§6 (seis comprobaciones), `ctest` **29/29** y los cinco modelos del banco byte a byte idénticos;
la sonda gana `PROBE_F0=<hz>` para medir la vía manual sin GUI. Diseño y medidas completas en
`DOCS/ARCHITECTURE/LAYER_SEPARATION_PLAN.MD` §8.

**MODO "REJILLA FIJA" (2026-09-25) — la rejilla manual pasa a ser declarada, no adivinada.** Un
toggle del ModelMaker ("Rejilla fija") apaga el seguimiento de pitch por ventana
(`analyzeTemporal(..., fixedGrid: true)`: los `frameF0` son la rejilla declarada en TODOS los
frames), lo anuncia en el indicador de rejilla, impide que el detector pise el f0 escrito y lo
**escribe en el modelo** (`SpectralModel::gridFixed` → `"gridFixed": true`, leído de vuelta por el
lector de producción). La clave es opcional y ortogonal a las capas: no sube `"format"` y solo se
emite cuando es true, así que los ficheros de siempre no cambian ni un byte (medido: los 16 modelos
del banco + sonda siguen byte a byte idénticos). Medido sobre material real: CZ-BASS1, cuyas ventanas
saltan de octava de verdad (124,7 / 62,4 / 123,6 Hz), sale con 124,3 en los cuatro frames cuando la
rejilla se declara. Verificado con `SpectralAnalyzerTest` §7, `ModelMakerRoundTripTest` §11 y `ctest`
**29/29**; la sonda gana `PROBE_FIXED=1` (combinable con `PROBE_F0`).

Implementado la **11.3** (2026-09-25): el motor **suma las capas**. `FrameSampler` gana el
sampler por capa (`sampleLayerFrame`: SU propio z y sus pesos —mezcla estatica x peso temporal
del frame— con el mismo algoritmo y los mismos extremos bit-exactos que `sampleFrame`) y
`sampleLayeredFrame`, el frame efectivo de un slot = la SUMA de sus capas; con `layerCount == 1`
delega, asi que el legado sigue **bit a bit**. La decision de arquitectura es una suma espectral
y no un banco por capa: el reparto de 11.2 da a cada indice UNA capa, asi que sumar por indice es
exacto y los 128 biquads siguen siendo 128 (el offset/f0 de cada parcial lo aporta la capa que mas
suena en el). Los dos motores (aditivo y modal) ganan `setLayerMorphZ` y cachean por los tres z;
las voces, `morphZ2`/`morphZ3` con el mismo glide y los destinos de matriz **29/30** al final de la
tabla. Contrato regenerado (74 parametros, 31 destinos) y la pagina posee los dos ids en el cajon
de MODELOS. Fallo encontrado en el camino: `resetModulations` no limpiaba `modMorphZ`, asi que una
ruta de matriz a Morph Z se acumulaba sin freno (el z corria a 1.0 y ahi se quedaba); arreglado y
pinneado. Verificado: `NEURONiK_LayerEngineTest` nuevo (16 checks: el barrido por capas medido con
Goertzel sobre el audio, no sobre la tabla de parciales), `ctest` **30/30**, `vitest` **242/242**,
contrato del selftest OK, piloto EXIT=0, y con material real la sonda mide las dos capas de
CZ-SWEP1 sonando (0,513 / 0,487). El bloque de la fase —con el hallazgo pinneado de que el
remapeo de rejilla de 10.6 sigue inactivo porque el snapshot no lleva `frameSpanHz`— esta en
`DOCS/ARCHITECTURE/LAYER_SEPARATION_PLAN.MD`.


Implementada la **puerta de plegado de octava** (2026-09-25, plan §10.3): la dispersion de las f0
por ventana **despues de plegar la octava** es el discriminador mono-rejilla / bi-rejilla, y el
corte del plegado era justo la comparacion que ya hacia `gridIsCommon` a mano — ahora tiene nombre
(`kOctaveFoldCents`), medida publicada (`lastOctaveFold()`: post-plegado, crudo, ventanas y saltos
de octava) y test, se toma en **toda** llamada a `analyzeTemporal` (tambien sin capas) y la sonda la
imprime por WAV. Medido en el banco CZ101: BASS1 9,4 cents post-plegado con **1194,4 crudos** (un
salto de octava del estimador en 1 de 4 ventanas: una sola rejilla), HAMOG 0,0, PAD1 4,2, SWEP1 17,5
(conserva sus 2 capas y su 0,513 / 0,487) y RRISE **502,5 ⇒ bi-rejilla** (sigue sin partirse, ahora
con el motivo escrito: 102,2 → 601,2 Hz por ventana no comparten rejilla ni plegadas). Sin datos no
hay veredicto. `Tests/LayerClusteringTest.cpp` gana la mitad C (**20 checks** nuevos: la medida pura
con f0 sinteticos —quinta y cuarta sobreviven al plegado con la MISMA dispersion— y dos casos de
punta a punta, incluido un sub una octava por debajo con 7 de 8 ventanas saltadas que **sigue siendo
mono-rejilla**). `ctest` **30/30**.

**Diseñado (2026-09-25) — el formato ralo de parciales (fase 11.6, extensión de v2.1).** Sin
implementar: el diseño está **medido** sobre lo que produce hoy el analizador (los modelos
temporales de los cinco WAV en `build-reference/probe-models`) y vive en
`DOCS/ARCHITECTURE/LAYER_SEPARATION_PLAN.MD` §11. La idea es una **ortografía** del mismo dato dentro
de v2.1: la lista de índices activos se declara una vez por CAPA y cada frame lleva amplitudes +
**ratios de inharmonicidad** `r = f/(n·f0)` en vez de 64 offsets en Hz. El ratio no es un ahorro de
bytes sino la forma invariante a la transposición —con la trayectoria de BASS1 (124,3 / 124,7 / 62,4 /
123,6 Hz) el ratio del mismo parcial se mueve 1-2 cents entre frames mientras el offset en Hz del
índice 25 recorre 8,5 Hz— y es lo que hace activable el remapeo de 10.6 sin romper el timbre. El
tamaño está medido y **no es lo que parecía**: el analizador mide los 64 índices contra su media
banda, así que en material denso (HAMOG 63-64 activos, BASS1 55-57) el ralo **engorda** el fichero,
mientras en bandas finas lo encoge 7-17× (RRISE) o 2,5-3,8× (capa 1 de SWEP1); con la regla «ralo solo
cuando es más pequeño» los arrays del banco bajan de 53 760 a 33 199 B (62 %) y con la lista
hoisteada a 30 797 (57 %). Dos sorpresas del mismo fichero: **`frames[]` y `layers[0].frames[]` son el
mismo contenido byte a byte** (8960 B de CZ-SWEP1, el 20,7 %, que el lector ya ignora: se propone
`"framesFrom": "root"`) y el **23-29 %** de todos los ficheros son cifras de 17 dígitos del escritor
(mejora mayor que la densidad, y decisión aparte). La raíz nunca se escribe rala (el v1/v2 sigue
sonando la raíz) y el ralo es declarativo: toggle «Ralo» en el ModelMaker (por defecto NO) y
`PROBE_SPARSE=1` en la sonda. Al implementarlo: round-trip con cota de precisión (la vía rala **no**
es bit-exacta), la regla de densidad en sus dos ramas y la degradación del lector viejo pinneada.

**Implementado (2026-09-25) — offsets transpuestos (Δn/n): la inharmonicidad sigue al teclado.** El
motor sumaba el offset en Hz (`f_n = n·base + δ`), así que la desviación en cents respecto del
armónico (`δ/(n·base)`) cambiaba con la nota tocada. Ahora el modelo puede **declarar** que sus
offsets son ratios medidos contra la rejilla de análisis (`"offsetsTranspose": true`, opcional y sin
subir `"format"`, como «rejilla fija») y el motor los **escala** por `base/f0` parcial a parcial: los
cents quedan invariantes (`1200·log2(1 + δ/(n·f0))`), que es el ratio `r_n` de §11.4 **en vivo y sin
tocar la densidad** del fichero. Sin el modo el factor es 1.0 exacto y el legado es bit a bit
(comprobado sobre el audio, no sobre la tabla de parciales: `Tests/TransposableOffsetsTest.cpp`, que
además mide con Goertzel los cents de la misma nota una octava arriba con y sin el modo). La
referencia viaja en un campo propio del snapshot (`offsetRootHz`, copiado de `frameSpanHz` por el
sampler) porque el snapshot no lleva `frameSpanHz` — el hueco pinneado que mantiene inactivo el
remapeo de 10.6: se usa el mismo dato sin encender aquello. Medido con la sonda
(`PROBE_TRANSPOSE=1`, cinco WAV): en CZ-RRISE el fundamental se desvía +113,1 cents sobre su rejilla y
**+57,5** una octava arriba sin el modo, mientras el modo lo mantiene en +113,1; en HAMOG −11,1 →
−5,5; en SWEP1 +9,5 → +4,8. `ctest` **31/31**.

**Implementado (2026-09-25) — los seis modelos del banco CZ101, assets del selftest E2E.** La bancada
del puente (`BridgeSelftest`) llenaba las ranuras A-D con material de prueba: UN modelo real embebido
(el temporal de CZ-BASS1) y tres JSON sintéticos de un parcial. Ahora viajan EMBEBIDOS los SEIS
`.neuronikmodel` del banco (`Assets/Models`, los que genera la sonda desde los WAV de
`ABDCZ101/DOCS/patches`) y el selftest es autocontenido: los seis se escriben a su directorio temporal
y se RELEEN con el lector de producción —los dos dialectos del v2: denso de 1 frame y con f0 por frame
de 4—, y los cuatro del banco (BASS1, HAMOG, PAD1, SWEP1, los mismos que el preset CZ101-BANK pone en
las esquinas del pad) entran por las ranuras A-D, así que la ficha MODELOS de la página enseña nombres
REALES en vez de `selftest-model-a`. La validación de los seis assets entra además en el veredicto
(`modelsAssetsOk`): antes un asset ilegible se registraba como FAIL en el log y el selftest igual daba
OK. Medido: el selftest del piloto en EXIT=0, con `CZ-BASS1|CZ-HAMOG|CZ-PAD1|CZ-SWEP1` en las esquinas
del pad y en la ficha A-D. `ctest` **31/31**.

**Medido (2026-09-25) — la f0 MANUAL en RRISE y SWEP1, y sus dos veredictos distintos.** El §2 del
plan de capas ya diagnosticaba RRISE (los 121 picos en una rejilla de 41,62 Hz; el estimador, con su
suelo de 50 Hz, persiguiendo la resonancia) y la vía honesta era la f0 manual del ModelMaker.
Faltaba medir qué sale cuando se le da a mano. Con `PROBE_F0=41.62 PROBE_FIXED=1` el síntoma
desaparece y el modelo **tiene cuerpo**: la f0 por frame pasa de 102,2/526,2/601,2 Hz a 41,6 en las
tres, el plegado de 502,5 cents con 3 saltos a **0,0 con 0** (mono-rejilla), el encaje de los 100
picos medidos de 6 % a **100 %** dentro de un cuarto de tono (mediana 0,9 cents) y los parciales
activos de 5/13/2/3 a **31/35/43/49** — con la energía migrando por la escalera (dominante n=7 → n=10
→ n=14 → n=15, drone n=1 firme), que es la pieza. Y **refuerza** el cierre de §9.1/§10: aquel
"bi-rejilla" era seguimiento, no dos raíces, así que multi-rejilla sigue fuera de alcance con menos
motivo todavía. En SWEP1 la f0 manual (62,5 Hz) mejora el encaje igual de claro —mediana 8,9 cents y
95 % de los 209 picos dentro de un cuarto de tono, frente a 25,4 y 45 %— pero **no es gratis**: el
`frameSpanHz` es también la referencia de reproducción (el motor hace `n·base`), así que el parche
sonaría una octava arriba al tocar su nota; es exactamente lo que la nota de `SpectralAnalyzer.h`
llama "abrir la puerta de sub-octava … ROMPE CZ-SWEP1". Para SWEP1 falta separar la rejilla de
ANÁLISIS de la referencia de REPRODUCCIÓN (hoy el mismo número): apuntado, no diseñado. Los ficheros
generados quedan en `build-reference/probe-manual/`, sin promover a assets.

**Pinneado (2026-09-25) — el caso GEMELO de la escalera: la bajada x1/2 SI existe, pero la rejilla la
cuantiza.** `Tests/SpectralAnalyzerTest.cpp` pinchaba un solo sentido de la escalera: la firma REAL
del SWEP1 (fundamental debil con sub-octava fuerte, 124 Hz y una serie que MUERE tras el 3er parcial,
248 casi vacio) NO baja al sub-armonico — el HPS refinado pilla la nota del parche. Faltaba el
simetrico: fundamental tambien debil con el 2f0 dominante, pero con la serie CONTINUA hacia arriba
(193,5 y 258 poblados), donde el periodo real es el del sub-armonico y la regla x1/2 SI tiene que
degradar el ancla una octava. Es el caso 4b, con la firma MEDIDA, y el discriminador entre los dos
queda escrito en el test: en SWEP1 la serie muere (la puerta x2 `m4 < 0.25·m3` no deja subir al 2f0),
en el gemelo sigue. Medido: `detectPitch` 64,50 Hz (no 129), reparto del modelo n1=0,100 / n2=1,000
(fundamental debil, 2o dominante, al reves que en el SWEP1) y la via por ventana 64,50/64,50 en los
frames 0 y 3 — la bajada no es artefacto de la ventana unica del HPS. `ctest` **31/31** y el selftest
del piloto **EXIT=0**.

**Y el hallazgo que el gemelo destapa: la cuantizacion es ASIMETRICA.** La peticion era «que caiga a
62», y **62 no es alcanzable** con la rejilla real: a 44,1 kHz / 8192 el bin mide 5,383 Hz, la regla
x1/2 solo puede bajar UNA octava y solo dentro de ±60 cents de `ancla/2`, y a ~62 Hz ese margen es
ANCHO DE UN BIN, asi que el ancla tiene que caer en un bin PAR. Con el ancla en el bin 23 (las 62,3 Hz
de la sub-octava real del SWEP1) las dos candidatas —bin 11 = 59,2 y bin 12 = 64,6— quedan a ±75
cents y la bajada es inalcanzable; con el ancla en el bin 24 (129 Hz) el bin 12 (64,5) cae dentro y la
bajada SI ocurre. Por eso la raiz del gemelo es **64,5 Hz** y no 62, y queda razonado en el comentario
del propio test. La regla de SUBIR (bandas de ±25 % sobre f0/2f0/3f0/4f0) no sufre esto porque su
ventana es relativa, no una resta sobre el ancla; la de BAJAR si. Apuntado, no cambiado: estrechar la
ventana de bajada a «bin mas cercano a ancla/2» seguiria teniendo el mismo problema de paridad, y
ensanchar la ventana reabriria el falso positivo del SWEP1. La solucion real sigue siendo la f0 manual
/ separar rejilla de analisis y referencia de reproduccion, no tocar el umbral.

**Implementado (2026-09-25) — la puerta de plegado de octava como PRE-FILTRO del analisis temporal
(plan §10.4).** El plan pedia que la puerta entra ANTES del clustering (material bi-rejilla no paga su
coste); hasta ahora se medIa DESPUES de todo el analisis y el clustering se corria igual, para
rechazarlo acto seguido. `analyzeTemporal` va ahora en dos pasadas: (1a) la f0 por ventana —la
evidencia de la puerta— guardando el espectro de cada frame; la PUERTA; (1b) la medida, donde la
rejilla comun (`perFrameCommon`) y el `clustering` SOLO corren si la puerta la declara COMUN. El
espectro se guarda para no repetir la FFT: el pre-filtro ahorra el trabajo, no lo duplica. La salida es
BIT A BIT la de antes (medido con la sonda: la tabla de plegado sale identica y SWEP1 conserva sus 2
capas —48 + 16 indices— y su 0,513 / 0,487 en el motor); lo unico observable que cambia es el
diagnostico: nace `lastClusteringSkipped()` y en bi-rejilla `lastLayerGridRejected()` pasa a true
tambien cuando el clustering habria dado una sola capa (ya no se calcula). Test: mitad D de
`Tests/LayerClusteringTest.cpp` (**8 checks nuevos**) — bi-rejilla eximida con el analisis intacto,
mono-rejilla no eximida (2 capas) y el mismo material con REJILLA FIJA (plegado 0, no se exime: la
puerta la decide la politica de f0, no el audio). `ctest` **31/31** y el selftest del piloto
**EXIT=0**.

**Documentado (2026-09-25) — la semantica del estimador de pitch queda PINNEADA para futuras
sesiones.** `DSP_PARAMETERS.md` gana la seccion «Analizador de pitch (ModelMaker) — semantica
pinneada»: donde vive (offline; `detectPitch` para el fichero, `detectPitchFromSpectrum` por ventana),
el ancla y su suelo (48,45 Hz a 44,1 kHz; por debajo el estimador lee su 2o armonico: E1 41,62 →
83,24), los **tres movimientos** de la escalera de octava con su condicion EXACTA (`÷2` por producto
`|1200·log2(prodHz/(0,5·ancla))| ≤ 60` con soporte ≥ 0,01·ancla; `÷2` por bandas `mOdd < 0,15·m2 &&
m3 < 0,15·m2`; `×2` por bandas `m2` dominante con `m4 < 0,25·m3`), la **cuantizacion asimetrica** (por
que 62 Hz es inalcanzable con el ancla en bin impar y 64,5 si baja con el ancla en bin par), las dos
familias de «fundamental debil» (lo que discrimina es si la serie SIGUE o MUERE, no la potencia del
sub-armonico), la puerta de plegado como PRE-FILTRO, la tabla medida del banco CZ101 (BASS1 9,4 /
HAMOG 0,0 / PAD1 4,2 / SWEP1 17,5 mono-rejilla; RRISE 502,5 bi-rejilla) y las guardias
(`pitchGuardCents` 150, `kPartialFloor` -60 dB, `kMinLsObservations` 4). Es la referencia que evita
volver a redescubrir el gemelo del SWEP1 y la paridad del bin. Nota: `STYLES_GUIDE` (ABDSharedAssets)
NO era el sitio — es el sistema de diseno (tokens/CSS), su §4b trata de tiles de imagen.

**Implementado (2026-09-25) — la pestana «CAPAS» del ModelMaker (Fase 11.5).** La GUI del ModelMaker
gana el tercer panel —junto a WAVEFORM y SPECTRAL— que ensena POR QUE el analizador partio (o no) el
material: una columna por parcial (n=1..64) con la ALTURA = su pico normalizado al GLOBAL y el COLOR
= su CAPA (0 = la raiz, cian; 1 magenta; 2 ambar), la TRAZA de su envolvente por frame encima y una
leyenda con nombre, numero de indices y peso por capa; con una sola capa lo dice («mono-rejilla: el
analisis no partio el material»). Los datos de vista son un modulo PURO
(`Source/ModelMaker/Analysis/LayerView.h`, `buildLayerView`) para que la GUI solo pinte y el test lo
pruebe sin ventana: **19 checks** en `Tests/LayerViewTest.cpp` (target `NEURONiK_LayerViewTest`) con
un modelo de dos capas (reparto por indice, inactivos a -1, leyenda, frames por capa, altura
global-normalizada y forma propia), uno de una capa y uno vacio. El ejemplo RRISE en assets y el
reporte de capas de la sonda ya existian (10.6 / 11.3). Footgun destapado y arreglado en el camino:
`SpectralModel::amplitudes` / `frequencyOffsets` eran los unicos arrays del struct sin inicializador
(un modelo por defecto llevaba basura de pila en los 64 parciales); ahora van a `{}` como el resto,
sin cambio de audio (paridad A–E bit-exacta). `ctest` **32/32** y el selftest del piloto **EXIT=0**.

**Verificado (2026-09-25) — el anillo morphZ del pad gira con LFO2 -> destino 28 en el navegador
(motor local WASM).** Pedido como comprobación tras los *fixes* del engine standalone. Antes de poder
mirar hubo que arreglar un TDZ que dejaba la página **EN BLANCO**: `routeControls` (bloque nuevo de
ENVOLVENTES en `WebUI/src/app.js`) usaba `drawerVisualSpec` antes de su `const`; ahora el spec se
declara por encima de las rutas y `drawerVisual` —que necesita las rutas resueltas— donde estaba.
Medido en Chromium sobre la ruta puesta por la UI real (`mod1Source = LFO 2`, `mod1Destination = 28` =
«Morph Z», amount 1.00, que la ficha del lienzo enseña como «1 LFO 2 -> Morph Z 1.00»): el arco de
`.zring-mod` se refresca cada **89 ms** (los 32 bloques × 128 muestras a 48 kHz del meter del
worklet), barre **0..100** entero (la semionda positiva del seno; con la base de morphZ a 0 la
negativa clava el arco en 0 en ~47 % de las muestras) y su periodo es **~1,07 s** con LFO 2 a 1,0 Hz.
Al bajar el *rate* a **0,6 Hz** el periodo pasa a **1,69-1,78 s** (teórico 1,667 s): el acoplamiento
con ESA fuente queda fijado. Control negativo A/B/A: con la fuente en `Off` el arco queda clavado en 0
durante las 30 actualizaciones siguientes y al volver a `LFO 2` se reanuda. El anillo es un círculo
SVG real de **195 px** (`.xy-pad__zring`, `inset:-7px` sobre el pad) y en reposo lo único visible es
el arco de modulación (`.zring-mod`, #00c3ff 4,5 px al 45 %; `.zring-fill` es la base de morphZ y
`.zring-track` va sin trazo). WebUI **255/256**: el único fallo es la aserción obsoleta
`return createEnvelopeBlocks({` de `WebUI/tests/appContract.test.js` (el fuente ya usa
`const blocks = createEnvelopeBlocks({` + `blocks.setRouteOpener`), WIP de ENVOLVENTES ajeno a esta
comprobación.

**Pedido (2026-09-25) — ENVOLVENTES: tamaños y destinos; y la matriz de N slots.**
Tres cosas, en este orden. **(1) Tamaños**: los gráficos eran demasiado grandes. En el **cajón** la
curva no tenía altura propia — mandaba la proporción intrínseca del SVG (viewBox 100×48) sobre los
~570 px de ancho, ~275 px por curva — y ahora tiene **altura fija de 120 px**; en el **frontal** las
curvas bajan un ~6 % (157 px) y las dos columnas ganan **aire** (gap 10 → 18 px, 21 px reales entre
dibujos: se leen como dos gráficos, no como uno partido). La banda del lienzo NO cambia de alto (la
fija `--abd-env-body`, espejo del `minBodyHeight` del contrato): el recorte sale del relleno de la
columna. **(2) Destinos junto a cada gráfico**: los bloques del cajón ganan su fila de rutas (mismo
vocabulario que el lienzo, destino → **+100 %**), leídas de la matriz — con «IR A LA RUTA» intacto
como salto. **(3) Fuera el destino precableado en el TEXTO**: los rótulos «ENV 1 · AMP» / «ENV 2 ·
FILTER» y el subtítulo pasan a identidad (**ENV 1 / ENV 2**); el papel lo declara la ruta real, así
que reencaminar la fuente ya no deja un rótulo mintiendo. WebUI **256/256**.

**Plan (no empezado) — la matriz de N slots y el destino precableado del motor.** Falta el cambio de
fondo, que sí toca el cable y el audio. Medido antes de proponerlo: **(a) la matriz asume 4 rutas en
20 sitios del C++** (`ParameterDefinitions.h`, `NEURONiKProcessor.cpp`, `BridgeSelftest.h`), en la
**tabla de offsets del puente WASM** (`NeuronikWasmBridge.cpp:207-216`, campos 22..33), en el worklet
(`neuronik-worklet.js`, índices + `INT_FIELDS`) y en el WebUI (`sections.js`, `envelopeViews.js`,
`wasm/audioParams.js` y 4 suites). **(b) El precableado ENV 2 → Cutoff es de UNA línea**:
`AdditiveVoice.cpp:231` (`env2Depth = currentParams.fEnvAmount * modEnvCutoff`) + `fEnvAmount` en el
`AdditiveVoice`. La etapa 1 sube la matriz a **32 slots** y para no romper el contrato por detrás
**añade** los slots 5..32 al final del layout (34..117) en vez de desplazar los índices viejos; los
presets de 4 rutas siguen cargando y el resto entran en Off/0. La etapa 2 convierte `filterEnvAmount`
en el **amount por defecto de la ruta ENV 2 → Filter Cutoff** (100 %, y desactivar = 0 %), con la
migración de presets, el cc79 del mapeo MIDI y el knob de la ficha FILTRO como consecuencias a
cerrar. La etapa 3 — **ENV 1 → Amp** como ruta — es la más delicada: hoy la amplitud la gatea el
`ampEnvelope` de la voz y no pasa por la matriz.

**Hecho (2026-09-25) — el indicador de rejilla del ModelMaker es CLICABLE.** Un clic sobre la linea
«Rejilla: f0 … Hz | residuo … cents» carga esa f0 —la que midio el detector— en el editor de pitch,
para no tener que copiarla a mano. La carga reutiliza la pareja que ya usaba el camino de grabar:
texto a 2 decimales + `updateRootNoteFromFreq`, que escribe los combos de nota/octava **sin
notificacion**; sin ese detalle habria ida y vuelta por la nota mas cercana y la rejilla se
cuantizaria (64,50 Hz no es 65,41 de C2). El clic NO re-analiza: cargar el dato y analizar siguen
siendo dos decisiones, y el unico que analiza es el boton Analizar. Sin material (`detectedFrequency
<= 0`) el clic no hace nada, y el cursor de mano + el tooltip lo anuncian — un texto que se puede
pulsar y no lo parece es una trampa. Nota de API: JUCE **8.0.12** no tiene `Label::onClick` (si
`onTextChange`/`onEditorShow`), asi que va por `addMouseListener(this, false)` + `mouseUp`, con el
guardia de arrastre del propio Label. Compila (`NEURONiK_ModelMaker.exe`); el clic en si es WIN32 y
queda para el ojo.

**Hecho (2026-09-25) — el aviso de pitch inestable del ModelMaker cita TAMBIEN el residuo de la
rejilla.** Tras ANALYZE, cuando la guardia dispara, la etiqueta dice «Pitch inestable (N cents): el
modelo estatico quedaria des-afinado; usa mas frames  |  residuo M cents (K picos)». Las dos cifras
contestan a preguntas distintas y por eso van juntas: la guardia mide **cuanto** se ha movido el
pitch (> 150 cents entre ventanas, con la octava plegada) y el residuo mide **de que clase** es el
material (RMS en cents de los picos contra la rejilla k*f0) — residuo bajo = el material SI es una
rejilla y lo que se mueve es el pitch (la respuesta son mas frames); residuo alto = el material no es
una rejilla (offsets, ruido, barrido), y mas frames no arreglan eso. No se recalcula nada: se cita el
ULTIMO residuo publicado (`detectPitch`/`refineGrid` sobre el MISMO buffer, el que la fila del
indicador ya esta enseñando), asi que el aviso y el indicador **no pueden contradecirse**; sin dato
(`-1`) el aviso dice «residuo n/d (material insuficiente)». Defecto que salio al paso y queda
arreglado: el aviso compartia renglon con el nombre del fichero en el header (~220 px de ancho a
800), asi que el texto YA se recortaba a media frase; ahora tiene **fila propia de ancho completo**
bajo el indicador de rejilla, que **solo se descuenta mientras hay aviso** (sin aviso el area de
abajo no pierde un pixel), con la fuente escalando con el zoom. Test nuevo en `SpectralAnalyzerTest`
§5(c), sobre un barrido de una octava (220 -> 440 Hz, 44,1 kHz): el analisis estatico da 1 frame, la
guardia dispara a **595,8 cents** y el residuo **sigue publicado tras `analyze()`** — 284,7 cents con
10 picos, la banda alta — porque `analyze()` no invalida el residuo que publico `detectPitch` (si lo
invalidara, el aviso diria "n/d" con material delante). `ctest` 32/32.

**Hecho (2026-09-25) — los RANGOS del residuo de rejilla quedan fijados como test del `ctest`
(`NEURONiK_Cz101ResidualRangesTest`).** El residuo que el indicador del ModelMaker enseña deja de ser
solo un numero en pantalla: los CINCO WAV reales del banco CZ101 entran al `ctest` y cada uno declara
su rango medido, su banda y su guardia. Medido el 2026-09-25: CZ-BASS1 **6,2** cents (verde), CZ-HAMOG
**4,5** (verde), CZ-PAD1 **7,1** (verde), CZ-SWEP1 **25,2** (amarillo) y CZ-RRISE **243,9** (naranja)
con la guardia disparada a **548 cents** — el unico de los cinco que la dispara. Los tres tonales son
UNA rejilla; SWEP1 sigue siendolo pero con un sub-armonico de sobra por debajo (por eso amarillo); y el
barrido RRISE no es una rejilla, que es justo el caso en el que la UI enseña el aviso de pitch con el
residuo al lado. El test no re-declara los umbrales: la banda se lee de las constantes nuevas
`SpectralAnalyzer::residualGreenCents` (15) y `residualAmberCents` (40), **extraidas de los literales
15,0f/40,0f que usaba el `MainComponent`**, de modo que el pincel del indicador y el test miran el
MISMO numero — cruzar una frontera de banda pasa a ser una decision que hay que venir a declarar aqui,
no una regresion silenciosa. Cada material comprueba ademas sus observaciones minimas (100/100/50/100/10:
PAD1 es el mas pobre en picos y RRISE el peor de todos con 36), que la f0 detectada este en rango
audible y el estado de la guardia (de pie en los cuatro cuasi-monotonicos, disparando solo en el
barrido). Disponibilidad: el material vive en el repo hermano, asi que CMake inyecta
`NEURONiK_CZ101_WAV_DIR` (= `../ABDCZ101/DOCS/patches`) solo si existe; sin inyeccion el test **saltea
y pasa** (ABDNeural compila sin el banco), pero con la ruta inyectada los cinco ficheros son
obligatorios — un WAV que falte es FAIL, no skip. `ctest` **33/33**.

**Hecho (2026-09-25) — el distintivo del cajón de la MATRIZ deja de ser un literal: cuenta las rutas
ASIGNADAS.** El badge del cajón EDIT decía `4 RUTAS` aunque el preset tuviera dos puestas (los defaults
del contrato asignan ENV 1 y ENV 2 y dejan `Off` las otras dos): era el inventario de la ficha, no el
estado. Ahora la ficha lo **declara** (`drawer.liveBadge`: los ids de FUENTE y la etiqueta) y el panel lo
**recalcula con cada snapshot** —una ruta está cuando su fuente no es la primera opción, `Off`—,
escribiéndolo con `setHeader` del mueble compartido, que reescribe el dato **sin reconstruir** el cuerpo
(justo el motivo por el que el cajón no se re-renderiza al abrir). Los cajones inventario (2 ADSR, 4 LFO,
4 RANURAS, 8 GLOBAL) no declaran `liveBadge` y conservan su literal: no cambian con el uso. El índice
`id -> view-model` que el paint necesita para leer un `choice` (sus `options`) sale de `section.controls`
—los descriptores—, no del array de celdas ya montadas, que no guarda el descriptor. Verificado:
`vitest` de la WebUI **258/258** y el contrato del selftest del host OK.

**Hecho (2026-09-26) — el residuo de rejilla se cita con UNA frase en las TRES superficies del aviso de
pitch.** El aviso ya no habla distinto segun donde se lea: la fila del ModelMaker montaba el texto a mano,
el dialogo `Modelo des-afinado` —el que BLOQUEA la exportacion del estatico— solo citaba la desviacion, y
la sonda RealWav ponia el residuo en su propia linea, con otro formato (`residuo=243.9 cents obs=36`) y
sin el caso sin datos. Ahora la frase vive en el analizador (`SpectralAnalyzer::gridResidualNotice`),
junto a la medida que cita —el analizador es quien mide; la GUI y la sonda solo la citan—, y las tres
superficies escriben la MISMA, incluido "residuo n/d (material insuficiente)" cuando el material no da
para medirla. Medido con los WAV reales del banco CZ101: CZ-RRISE dice "guardia: desviacion de pitch 548
cents (...) | residuo 243.9 cents (36 picos)" y CZ-PAD1 "residuo 7.1 cents (110 picos)" — las cifras
canonicas del ticket (aj). Verificado: `NEURONiK_SpectralAnalyzerTest` OK (0 fallos) con dos checks que
fijan la forma de la frase, la GUI y la sonda compilan sin errores nuevos y `ctest` **33/33**.

**Hecho (2026-09-26) — la huella del modelo de 25 KB, medida y con presupuesto por voz/slot.** El
presupuesto que manda no es el modelo suelto ni el WASM: es CUANTAS COPIAS del modelo lleva cada voz.
`Resonator` y `ResonatorBank` guardan 4 slots (A-D) + 4 caches de frame = **8 modelos por voz** (195,6 KB
de los 202,5 KB que mide `AdditiveVoice`), y los motores PRE-ASIGNAN 32 voces —`setPolyphony` solo mueve
`activeVoiceLimit`; no libera las que sobran—, asi que un motor preparado pesa **6,46 MB** (aditivo) /
**6,37 MB** (neurotik) y un `NEURONiKProcessor` con polifonia 16, **7,20 MB** de heap mas 34 KB de pila
(la cola de comandos, 32 modelos = 801 024 B, ya vive en el heap). Medido el 2026-09-26 con un contador
propio de bytes VIVOS sobre `operator new`/`delete` (el heap de verdad, no una estimacion): modelo
25 032 B; `Resonator` 206 592 B; `ResonatorBank` 208 272 B; `AdditiveVoice` 207 392 B; `NeurotikVoice`
208 784 B; `NEURONiKProcessor` en la pila 34 096 B. `Tests/MemoryBudgetTest.cpp` fija los techos —modelo
26 KB, voz 224 KB, motor 7,5 MB, procesador 8,5 MB— con `static_assert` para la talla de compilacion y
checks de heap para lo demas, mas guardias de coherencia del propio presupuesto. Verificado:
`NEURONiK_MemoryBudgetTest` en verde y `ctest` **34/34**.

**Hecho (2026-09-26) — reserva PEREZOSA de voces: el motor ya no pre-asigna 32.** Los dos motores
creaban sus 32 voces en el constructor para CUALQUIER polifonia: `setPolyphony()` solo movia
`activeVoiceLimit` y las 32 seguian ahi. Ahora `BaseEngine::ensureVoices(count)` crece la lista hasta el
limite y nada mas —`getNumAllocatedVoices()` lo dice—, con la capacidad reservada de una vez
(`voices.reserve(kMaxVoices)`) para que crecer NUNCA reasigne el buffer que el hilo de audio ya indexa.
Medido con MemoryBudgetTest: el motor aditivo nace en 16 voces y **3,23 MB** (antes 6,46), el neurotik en
8 y **1,60 MB** (antes 6,37), y un `NEURONiKProcessor` con polifonia 16 baja de 7,20 a **4,04 MB**; al
techo de 32 el peor caso sigue en 6,46 / 6,37 / 7,20 MB. El crecimiento lo hace el hilo de mensajes bajo
el MISMO cerrojo del cambio de motor (`getCallbackLock`); bajar la polifonia NO devuelve voces (una voz
puede estar sonando su cola; la reserva es de TECHO). Verificado: `NEURONiK_MemoryBudgetTest` en verde
—seccion 3 nueva: reserva 16/8 al nacer, 32 al subir, 32 tras bajar a 4, pendiente ~206,5 KB por voz— y
`ctest` **34/34**.

**Hecho (2026-09-26) — modulo WASM reconstruido y worklet sincronizado.** `build_wasm.bat` recompilo el
DSP real (emscripten + Ninja) desde las fuentes con el struct de capas y corrio su cadena completa:
referencia nativa (`NEURONiK_WasmParityTest`), paridad Node, smoke y `sync-wasm.mjs`. Evidencia de que las
capas no movieron el comportamiento: la referencia nativa (`build-wasm/parity-native.json`) sale **byte a
byte identica** a la de ayer (md5 `bd80f7678261961299d791bb4d6358b7`), la paridad da **0 ulps en los 9
casos de la matriz A-E** (184 320 muestras) y el smoke renderiza audio (`peak=0.53199 finite=true`). El
BINARIO si cambio de talla —el `.wasm` pasa de 100 729 a 101 416 bytes (+687; el struct es mas grande)—,
pero el `.js` de pegamento es identico (`d529f87b…`) y el audio no: layout, no comportamiento.
`WebUI/public/worklet` queda sincronizado (mismos md5 que `build-wasm/`) y `ctest` **34/34**.

**Hecho (2026-09-26) — la métrica del clustering es SELECCIONABLE, y el coseno de §3.3 está
medido: pierde en los dos juegos.** La desviación de 11.2 dejaba el coseno «expuesto para que el
test lo mida», que es decir que no se podía ELEGIR. Ahora la afinidad es un parámetro —
`LayerMetric` (`Descriptors` por defecto, `EnvelopeCosine` = la letra del plan)— que entra por
`clusterTraces(..., floor, metric)` y por `analyzeTemporal(..., fixedGrid, layerMetric)`, y la
última llamada la publica `SpectralAnalyzer::lastLayerMetric()` (se publica aunque la puerta de
plegado exima el material y el clustering no corra). El punto de elección es UNO: la lambda de
afinidad del aglomerativo; media, medoide, clamp y orden no distinguen de qué métrica vienen.
Medido en el test de trazas (sección E nueva, **74 checks OK**): en las trazas SINTÉTICAS del
criterio de aceptación (la voz ocupa 2 de 8 frames) el coseno drone-voz da **0.49 < 0.55**, no
fusiona NADA, y el clamp a `kMaxLayers` + la guardia de degeneración colapsan el resultado en
**1 capa** donde los descriptores dan las 2 correctas; en las trazas REALES de 9 ventanas del
RRISE el coseno falla al revés —fusiona drone y n7 (0.69 ≥ 0.55)— y también da 1 capa; y con el
audio de punta a punta el coseno da **1 capa** frente a las 2 de los descriptores. El coseno no
falla por su forma (separa n7 de n15, de soportes disjuntos) sino por su ceguera al SOPORTE:
una traza plana correlaciona con todo lo que dure parte del fichero, y lo que no correlaciona
tampoco llega al corte. Por eso el defecto sigue siendo descriptores, y ahora está demostrado
con las dos métricas en la mano. Verificado: `NEURONiK_LayerClusteringTest` OK (74 checks,
0 fallos) y `ctest` **34/34**.

**Hecho (2026-09-26) — la sonda diagnostica REJILLAS ENTRELAZADAS y el parcial 4 de SWEP1 deja de
parecer un error de 212 cents.** La validación acústica del probe mide cada parcial fuerte del
modelo contra el pico real de su banda; en material construido sobre una sub-oscilación (f0/2 con
familias propias: el CZ 62/124) el pico real de esa banda puede ser un **impar de f0/2** — miembro
de la OTRA familia de la fuente, que ninguna rejilla k·f0 representa. Antes contaba como error
brutal del modelo (SWEP1: −212,9 cents); ahora la firma se detecta (pico a menos de un cuarto de
f0 de un impar de f0/2 y a más de un cuarto de su armónico nominal), la línea lo explica y deja
de alimentar el máximo: SWEP1 máx 212,9 → **16,6**; BASS1 (que también es bi-familiar: 4 de 8 del
top-8 caen en los impares 7/11/15/19 de 62,2 Hz) 229,0 → **8,0**. **Decisión sobre el check
acústico: NO se exime el material — se corrige la contabilidad.** La mediana contractual se toma
ahora sobre la familia que la rejilla representa (con reserva: menos de 3 propios ⇒ sobre todos)
porque con la mitad del top-8 ocupado por la otra familia el contrato quedaba a merced de la
riqueza de la fuente; las medianas no se movieron (BASS1 −3,4, HAMOG −1,6, PAD1 −0,1, SWEP1 −4,5),
el mínimo de 3 parciales validados sigue contando a los entrelazados (siguen siendo evidencia
acústica) y las guardias de siempre quedan intactas: barrido exento (RRISE), `HALVE_F0` sigue
disparando el fail de sub-octava en material mono-familiar (PAD1). Verificado: los cinco WAV del
banco CZ101 → `RESULT: OK`, y el diagnóstico imprime el impar exacto de cada pico explicado.

**Hecho (2026-09-26) — el pad XY, el anillo morphZ y las cuatro ranuras funcionan SIN host: la
página es su propio motor.** Faltaban dos caminos y se abren con UNA frontera nueva cada uno.
**El morph nunca llegó al DSP por ningún camino**: morphX/morphY/morphZ son `VoiceParams` (no
GlobalParams) y el puente WASM no tenía export que los tocara — en el plugin los escribe
`NEURONiKProcessor::synchronizeEngineParameters` desde el APVTS, pero el worklet no tiene APVTS.
Nuevo: `setMorph(x,y)`/`setMorphZ(z)` en los dos motores (read-modify-write de
`pendingVoiceParams`, el mismo canal RT-safe de siempre) y `neuronikSetVoiceMorph(x,y,z)` en
`NeuronikWasmBridge.cpp`, con clampeo a [0,1] en la frontera. **Las ranuras solo se llenaban vía
host** (`loadModel` abre el diálogo nativo; sin host nadie contesta `modelsState`): ahora el store
acepta una via local — `loadModel(slot, { requestLocalFile })` pide el fichero al input oculto de
`app.js`, `loadLocalModel()` lo parsea al MISMO shape de `modelsState` (parser del dialecto del
escritor único; un v2.1 de capas avisa «(capa 0)» en el nombre porque el v1 del puente suena la
raíz) y el estado `models` viaja al worklet por el canal `neuronik:models` que ya existía. La
ficha RANURAS habilita CARGAR sin host (`localModelReady`; sin host y sin camino local, deshabilitado
como siempre — no se finge nada). El botón SOUND ON sincroniza modelos y morph al arrancar y al
cambiar de motor (el motor nuevo despierta en los defaults del struct y la página re-aplica su
posición). **Verificado en Chromium real** (vite :5199, sin `__JUCE__`): SOUND ON → badge «AUDIO:
motor local del navegador (WASM) · ON · 48.0 kHz» con el binario nuevo (101 751 B, export incluido);
CARGAR en la ranura A con un `.neuronikmodel` generado al vuelo → «SMOKE-BASS, 1/4 cargados» y la
nota 48 enciende el meter del worklet («1 voz activa» → «0» al soltar: el modelo suena); el pad
responde (readout X 85% / Y 85%) y el store lleva `morphX/morphY` nuevos que `syncEngine` cruza al
motor por `neuronik:morph`. **De regalo, el meter del worklet destapó un TDZ real del arranque**:
`modRings` vivía dentro del bloque de montaje y el primer frame (~48 Hz) llegaba antes que la
declaración — `modRings is not defined` en consola; sube a nivel de módulo. Paridad WASM reconstruida:
0 ulps en los 15 escenarios (3 tasas × 5), referencia nativa **byte a byte la misma**
(`bd80f767…`), worklet sincronizado (mismo md5 que `build-wasm/`), `ctest` **34/34** y la suite de
la WebUI **286/295** (los 9 rojos son `lcdTop`/`sections`/`appContract`-EDICIÓN en vuelo de otro
agente; el de `appContract` era mío y está corregido a la doble vía).

**Hecho (2026-09-26) — el motor del navegador SUMA las capas del modelo (11.3 cruzado al
WASM): un modelo de 2 capas suena entero en modo local, no solo su capa 0.** La suma ya
existia en nativo (`sampleLayeredFrame`, 11.3) pero la frontera la escondia: `neuronikLoadModel`
(v1) solo cruza 128 floats planos y `loadModel` REEMPLAZA el struct del slot, asi que cualquier
segunda llamada habria borrado la raiz. Frontera nueva en UNA llamada: export
`neuronikLoadModelLayers(slot, engineType, data, isValid, extraData, layerFrames, layerWeight,
frameWeights)` — raiz por memcpy del layout v1 + capa 1 desde `extraData` con el layout
`{amps[64], offsets[64], frameF0}` repetido `layerFrames` veces (193 floats/frame) seguido de
los pesos temporales, peso estatico como escalar; frames clampeados a kMaxFrames (16) y sin
capa extra queda `layerCount = 1` (el camino v1 es identico, paridad 0 ulps intacta). El worklet
reserva scratch de capas (`3*64*16+17` floats) y en `neuronik:models`, si el shape local trae
`layers` (v2.1), serializa y llama al export nuevo; ante cualquier duda cae al v1. El parser
local (`localModels.js`) gana `extractExtraLayer`: validacion amplitud a amplitud (no finito ->
0, capa rota -> no se anuncia), capas 2+ truncadas documentado, y el nombre deja de llevar el
aviso «(capa 0)». Verificacion: test nativo del export en `NEURONiK_LayerEngineTest` (seccion
6: layout reconstruido == sampleLayeredFrame directo, morphZ2 mueve el offset; ctest 34/34),
WASM reconstruido (102 857 B) con paridad 0 ulps, y E2E real en Node contra el binario del
navegador con el SWEP1 temporal: el parcial n14 (1831 Hz, solo capa 1, amp 0.433 en frame 0)
pasa de energia 2.9e-4 (v1) a 4.4e-3 (**x15.1**) y el RMS global sube un 42%. Leccion del E2E:
el probe se elige por amplitud EN EL FRAME 0 (z2=0), no por el maximo global — n4 es el mas
fuerte de la capa pero es 0 en frame 0 y produce un falso negativo.

**Hecho (2026-09-26) — cada capa, su PROPIO volumen: el anillo del pad mezcla capas (11.4).**
La suma por capas de 11.3 daba a cada capa su propio eje z, pero el volumen lo fijaban solo
los pesos estaticos del fichero: la pagina no podia mezclar. Nueva ganancia por capa
(`LayerGains`, default 1.0 = legado bit-exacto) que escala cada capa despues de muestrearla,
con dueno del offset decidido sobre amplitudes ya escaladas; viaja como VoiceParams
(`layerGain2/3`, glide 20 ms) -> `setVoiceLayerMorph` en los motores -> export
`neuronikSetVoiceLayerMorph` -> mensaje `neuronik:morph` con z2/z3 (undefined = sin cambio).
El ARO del pad es el gesto macro: cada fase (begin/change/end, puntero y teclado) viaja como
morphZ + morphZ2 + morphZ3, y cada capa sigue SU linea de frames desde su reposo — la mezcla
es la suma. Semantica honesta y documentada: el resonador normaliza la suma de parciales, asi
que la ganancia REPARTE el espectro (drawbars): al callar la capa 1, n14 (1831 Hz) cae 3.4x y
la raiz gana cuota; no hay master por capa. Verificado: seccion 7 de LayerEngineTest
(bit-exacto en reposo, 0 apaga su parcial, 0.5 conserva el offset; ctest 34/34), WASM
reconstruido (105 192 B, paridad 0 ulps, smoke OK), E2E Node contra el binario real y vitest
299/299.

**Hecho (2026-09-26) — el panel CAPAS del ModelMaker dice POR QUE: el veredicto de la puerta
de plegado (mono/bi-rejilla y sus cents) vive dentro de la vista.** La pestana ensenaba el
reparto (indices, trazas, pesos) pero la razon de la puerta se adivinaba. Nueva
`LayerView::FoldVerdict` rellenada por `buildLayerView(model, analyzer.lastOctaveFold())` — la
misma `measureOctaveFold` (10.3) que decide el clustering y cita la sonda, sin politicas
nuevas. Fila nueva sobre la leyenda: bi-rejilla en ambar (cents post-plegado y crudos, exento
de clustering) o mono-rejilla en verde suave (cents, saltos de octava plegados a acuerdo,
ventanas); sin datos —o en analisis estatico, donde la puerta no se mide— dice "sin medida" y
no inventa. Verificado: seccion 4 de `NEURONiK_LayerViewTest` (mono medido, bi con la quinta
que sobrevive al plegado, sin ventanas y el camino de 1 argumento sin veredicto); ctest 34/34.

**Hecho (2026-09-26) — la comprobacion manual del anillo morphZ con LFO2 -> destino 28 es un
E2E de verdad (`_t59_e2e_zring.mjs`), con su periodo medido y su control negativo.** El mismo
motor local (build-wasm/neuronik_dsp.wasm) y el mismo mecanismo que la pagina (mirror de
GlobalParams con los layouts base+matriz concatenados, `neuronikSetGlobalParams`) montan la
ruta Ruta 1 = LFO 2 -> Morph Z y leen `neuronikGetMod(28)` — exactamente lo que pinta
`.zring-mod`. Pineado: periodo **1.000 s** con LFO2 a 1.0 Hz (3 cristas) y **1.666 s** a
0.6 Hz (teorico 1.6667), barrido seno completo ±1.000 (= amount) con la semionda negativa
incluida (base z=0), control A/B/A: fuente Off clava el arco en 0.000000 y al volver a
LFO 2 se reanuda. Dos lecciones de cable quedaron dichas en el propio test: sin
`neuronikSetEngine` el motor no existe y todo mide 0, y el mirror a ceros apaga el LFO
(rate 0/depth 0): la escena debe escribir lfo2 rate/depth como hace la pagina. No toca
codigo de produccion: es la red de seguridad de la ruta que ya sonaba.

**Hecho (2026-09-26) — el giro del anillo morphZ queda probado por el camino NATIVO del
plugin: la direccion ZRING del selftest mide el arco en la bancada WebView2.** La ruta LFO 2
-> Morph Z (destino 28) se escribe por el APVTS (como la dejaria un preset, con sync Free
explicito: el RANDOM de ACCIONES sorteaba el modo y el rate dejaba de mandar), el motor REAL
del plugin la aplica en su render y la telemetria (frame.modulation[28] -> setZMod) pinta el
arco .zring-mod. La colecta es evaluate siacrono PACED a 30 ms (el evaluate de la bancada
responde en ~1 ms: sin pacing, 240 tomas cubrian 200 ms de arco, menos de un periodo) con
marca de tiempo de reloj de pared, y el periodo sale de cristas deduplicadas por meseta (el
arco se cuantiza a guiones). Medido en la bancada: periodo **1002.5 ms** a 1.0 Hz, arco hasta
**100** guiones, fuente Off -> **0.0** exacto, y al volver LFO 2 -> **99**. Es la otra mitad
de la ruta que el motor local del navegador ya tiene pineada en `_t59_e2e_zring.mjs`: dos
caminos, una sola verdad. Presupuesto del arnes: 30 -> 90 s. (El veredicto global del run
dependia ademas de AGUJA, direccion en vuelo de otro agente.)

**Hecho (2026-09-27) — el arco de modulacion del anillo morphZ muestra la CONTRIBUCION CON
SIGNO: se acabo el recorte, la semionda negativa se ve.** Antes el arco pintaba de la base al
efectivo CLAMPEADO 0..1 (la matematica de la voz), asi que con morphZ en reposo la semionda
negativa de un LFO era invisible (el clamp la dejaba a 0). Ahora el arco nace en la base y
corre en el SENTIDO del signo — horario si suma, ANTIHORARIO si resta, envolviendo por las
12 si pasa de la vuelta (el dash de un circulo cerrado envuelve solo) — y ensena la
contribucion completa; el efectivo lo sigue clampeando la voz en el motor. Pineado en vitest
(300/300: semionda negativa con base 0 -> offset -5/span 95; negativa con base 0.25 termina
en la base; 0.9+0.5 envuelve 50 guiones sin recorte) y en la bancada nativa: ZRING mide ahora
el periodo de |sin| — **497.5 ms** a 1 Hz, dos culminaciones por periodo — y cuenta
**182/480 instantaneas** del arco en el lado antihorario (la semionda negativa pintada);
fuente Off -> 0.0 exacto y reanudacion -> 100. Nota de bancada: el selftest sirve
`WebUI/dist`, no `src` — sin `npm run build` mide la pagina vieja.

**Hecho (2026-09-27) — el modelo puro del indicador de rejilla sale de la GUI**
— El texto de la fila "Rejilla: f0 ... | residuo ... | N picos", su color por
bandas del residuo y lo que el clic carga en el editor de pitch viven ahora en
`Source/ModelMaker/Analysis/GridIndicator.h`, un modulo PURO sin JUCE
(string/cmath/cstdio/cstdint): `GridIndicatorModel` con las 6 entradas (f0,
residuo, picos, FIJA, TRANSPONIBLES, bajo el suelo del estimador) y las salidas
`band()`/`argb()`/`text()`/`clickLoadsHz()`/`clickEditorText()`. Las constantes
de las bandas (verde 15.0, ambar 40.0 cents) viven AHI: `SpectralAnalyzer`
las aliasa, asi que el test de rangos del banco CZ101 y la UI siguen leyendo el
mismo numero de la misma fuente. `MainComponent::updateGridIndicator` solo
RECOGE la entrada y PINTA lo que el modelo devuelve (el bloque que anadia los
avisos a mano quedo fuera: viajan dentro del texto del modelo, en el orden de
siempre — FIJA, TRANSPONIBLES, o "fijada a mano" si el HPS no llega; los 4 RGBA
del ternario viejo intactos). El clic queda descrito por el modelo: entrega la
f0 SIN cuantizar (64.50 no es 65.41 de C2) y el texto del editor a 2 decimales.
Test propio sin JUCE (`NEURONiK_GridIndicatorTest`, 28 checks): bandas y sus
fronteras inclusivas, RGBA exactos, texto exacto aviso a aviso y su orden, clic
sin material (0/"") y con material. ctest **35/35**.

**Hecho (2026-09-27) — el indicador de rejilla se alcanza y acciona con el teclado**
— Tab ENFOCA la fila "Rejilla: f0 ..." (setWantsKeyboardFocus; sin material
cargado no entra en el ciclo: un activable sin accion no roba un paso de
teclado) y Space/Enter ACTUAN el mismo camino que el clic (keyPressed ->
useDetectedFrequency, la convencion de JUCE para lo activable). El foco se VE:
paintOverChildren dibuja un anillo blanco redondeado alrededor de la fila solo
mientras la tiene (un globalFocusChangeListener lo repinta al entrar y al
salir; se da de baja en el destructor antes de destruir miembros), y el
tooltip lo anuncia: "Clic o Espacio/Enter: cargar la f0 detectada en el editor
de pitch". El guard de material vive en useDetectedFrequency, asi que sin f0
la tecla no hace nada — igual que el clic; Tab desde el indicador sigue
llegando al editor de pitch. ctest **35/35** y arranque de bancada OK.

**Hecho (2026-09-27) — los distintivos de los cajones EDIT son DATOS VIVOS**
— El patrón `liveBadge` de la MATRIZ (rutas asignadas) pasa a familia con tres
modos (`ui/panel.js::liveDrawerBadge`, el snapshot entero entra): `assigned`
cuenta rutas con fuente != Off; `loaded` (MODELOS) cuenta ranuras del MOTOR con
modelo — la MISMA verdad que la vista model-slots (`displayableName`: nombre !=
'EMPTY'; un entry con `isValid: false` cuenta como cargada, porque lo que falla
es el fichero) y el total sale de `MODEL_SLOT_LABELS`, sin declarar el 4 dos
veces; `touched` (GLOBAL) cuenta celdas del cajón apartadas del default del
contrato generado (descriptor con skew -> `defaultNormalized` del generated ->
"0 en real", nunca undefined; margen 1/8192 de normalizado, medio paso de la
rejilla del wire — generoso con el redondeo y estricto con el gesto). Los ids
que cuenta `touched` los DERIVA `drawerFor` (ids de la ficha: las celdas del
cajón, sin frontal ni baseline), así que la ficha no replica la regla del
reparto; masterLevel no cuenta (no es celda del cajón, contrato 8.1 2c). Un id
ausente del snapshot cuenta como sin asignar / en default: no se inventa nada.
Los literales (4 RANURAS, 8 GLOBAL) quedan para lo que son: el inventario
antes del primer paint. Tests: el badge de MODELOS con slots del puente
(Campana + Fantasma divergente + Metal = 3/4, coherente con el "1/4 cargados"
de la vista) y el de GLOBAL con BPM movido, toggle ON y vuelta al default
(2/8 -> 1/8: el badge sigue la verdad del snapshot, no un historial). Vitest
**310/310**.

**Hecho (2026-09-27) — el anillo del pad BAILA sin host: la MATRIZ de fabrica en
MODO LOCAL apunta un LFO al eje temporal del pad**
— El anillo ya pintaba el destino 28 (con signo, envolviendo), pero sin host nadie lo
movia: el motor WASM nace con la matriz del contrato (slots 3/4 en Off), asi que
`_neuronikGetMod(morphZ)` valia 0 y el arco se quedaba en su base. La pagina ahora la
SIEMBRA en local (`paramStore::seedLocalMorphZRoute`): **LFO 2 -> Morph Z al 100%** en el
primer slot LIBRE (el 3; los 1/2 son las rutas de las envolventes y no se tocan), solo si
el slot sigue virgen y el store posee los tres ids — con host devuelve false y manda el
APVTS. Fuente y destino se resuelven por LABEL contra las `choices` del contrato, nunca
por indice literal. Y para que la ruta LLEGUE al motor, `onAudioEngineChange` re-aplica el
snapshot de la pagina al saltar a `ready`: `syncEngine()` se corta sin motor y SOUND ON
arranca despues del primer paint, asi que era el unico hueco por el que la matriz (y los
modelos, y el pad) no entraban a un motor recien arrancado. Medido contra el binario real
del worklet por el mismo camino de la pagina (`Tests/localMorphZRouteTest.mjs`,
`NEURONiK_LocalMorphZRoute`): fields `[28,2] [29,28] [30,1]`, `GetMod(28)` de **-1.0000 a
1.0000**, periodo **998.7 ms** y control en Off **0** exacto. ctest **38/38**; vitest
**338/338** (siembra y sus dos NO en `paramStore.test.js`, el viaje de la ruta en
`workletMorph.test.js`, el cableado y el re-sync en `appContract.test.js`).

**Hecho (2026-09-27) — el navegador RECUERDA sus ranuras de modelo: la ultima carga
sobrevive al F5**
El plugin vuelve a sus cuatro ranuras porque el PRESET lleva la ruta del fichero
(`modelPath<slot>`); la pagina no tiene preset ni sistema de ficheros, asi que cada
recarga empezaba con las cuatro EMPTY y habia que volver a buscar los mismos ficheros.
`paramStore::loadLocalModel` guarda ahora en `localStorage` el TEXTO CRUDO del
.neuronikmodel y `restoreLocalModels` repuebla las ranuras al arrancar (solo MODO LOCAL:
con host manda `modelsState`, el preset). Se guarda el texto y no el objeto parseado
porque el lector es UNO: recuperar vuelve a cruzar `parseModelText`, el mismo parser que
un fichero elegido a mano — `readModelFile` se parte en `readModelText` + parser. Todo
best-effort: sin almacen o sin cupo la carga funciona igual (y un aviso lo dice), y una
entrada que ya no parsea se descarta con el `modelError` de siempre en vez de romper el
arranque. Medido contra el binario real del worklet por el mismo camino de la pagina
(`Tests/localModelCacheTest.mjs`, `NEURONiK_LocalModelCache`): la ranura recuperada vuelve
bit a bit por el mismo parser y la misma nota suena distinto (RMS 0.49542 con la ranura
vacia vs 0.36251 con la recordada). ctest **39/39**; vitest **359/359** (memoria por
ranura y sus dos NO -sin almacen, con host- en `paramStore.test.js`, el round trip del
texto en `localModels.test.js`, el almacen en `localModelCache.test.js` y el orden del
arranque local en `appContract.test.js`).

**Hecho (2026-09-27) — el smoke del MODO LOCAL es un test de navegador (Playwright) y caza el
bug de la pagina muda**
El smoke del modo local -SOUND ON, ranura de modelo, nota y pad- se comprobaba a mano y su
resultado vivia en HANDOFF. Ahora `WebUI/e2e/localMode.spec.js` (+ `playwright.config.js`) lo
recorre en Chromium de verdad sobre `dist`, con cinco casos: SOUND ON -> badge del motor WASM y
la fila 3 de la MATRIZ cruzando por `neuronik:params`; el anillo del pad bailando con los dos
signos (y `morphZMod` de -0.99 a +1.00); una nota -> «1 voz activa» y 0 al soltar; la ranura A
cargando el CZ-BASS1 real por el dialogo del input oculto y sobreviviendo al F5; el pad moviendo
morphX/morphY. Es el UNICO test que arranca el AudioWorklet y espia la frontera pagina -> worklet
(envuelve `MessagePort.prototype.postMessage` y `AudioWorkletNode`): los de node leen el WASM a
mano y el vitest corre en jsdom. Dos trampas del entorno, medidas: `--mute-audio` (sin el, el
reloj del AudioContext no avanza y el worklet no procesa nunca, ni headless ni con ventana) y ~4 s
de arranque del servicio de audio (el test espera al reloj, y si no arranca los casos que
necesitan procesar se saltan con el motivo). Y el hallazgo: la pagina NO mandaba nada al worklet
-`engineSnapshot = engine` guardaba el MISMO objeto vivo del modulo, asi que `wasReady` era
siempre true y el re-sync a `ready` no disparaba-, de modo que el motor procesaba con los
defaults del struct y el anillo del pad no bailaba. Se arregla con una COPIA del snapshot; el
test se comprobo rojo con el bug puesto. ctest **40/40** (`NEURONiK_WebUiLocalModeE2e`) y vitest
**359/359**.

**Hecho (2026-09-27) — el distintivo VIVO de MODELOS y GLOBAL cuelga en el lienzo y abre su cajón**

El dato vivo de una ficha (0/4 RANURAS, 0/8 GLOBAL) solo se leía abriendo el cajón, que cerrado sale
desplazado fuera de pantalla. Ahora cuelga también en la CABECERA de la ficha, junto al EDIT, como botón
que abre su cajón: el mismo gesto que la franja de GLOBAL & MASTER. Lo piden las fichas que lo declaran
(`liveBadge.onCard`), no todas, y se repinta con el MISMO cálculo que el distintivo del cajón (un dato, dos
destinos). El chip muestra solo la fracción (`0/4`): la cabecera de MODELOS mide 211 px de diseño y con el
rótulo entero el chip empujaba el EDIT fuera de la ficha —medido, no supuesto-; el rótulo se queda en el
`title` y en la etiqueta accesible, que además contiene el texto visible. vitest **361/361** y
Playwright **6/6** (el caso nuevo además mide el encaje en el navegador y ve el chip subir a `1/4` al
cargar un modelo de verdad).

**Hecho (2026-09-27) — los distintivos vivos también saben de motores (modo `active`)**

Nuevo `liveBadge.mode: 'active'`: cuenta las celdas de la ficha que consume el motor que está
sonando, con la cobertura que el propio gating usa (`engines` por parámetro, `optionEngines` por
opción para las celdas gateadas), no con una cuenta a mano: una celda gateada está activa si la opción
seleccionada es alcanzable, y una normal si su parámetro la comparten los dos motores, la consume
el procesador (`host`) o la consume el motor activo. El LFO 1 & 2 lo declara y muestra 10/10 con
ambos motores (los dos LFO son DSP compartido); el motor se resuelve por contrato cuando el selector
no es celda de la ficha. Las fichas donde el número Sí se mueve —FILTRO 2/2 → 0/2, RESONADOR
0/3 → 3/3, ENVOLVENTES 8/8 → 4/8— no tienen cajón donde colgarlo aún. vitest **364/364** y
Playwright **7/7**, este último midiendo el gating real en el navegador en los dos motores.

**Hecho (2026-09-27) — la MATRIZ tiene conmutador para la ruta local del pad**

La siembra LFO 2 → Morph Z (la que hace bailar el anillo del pad sin host) se plantaba sola y solo se
podía quitar a mano, con el cajon abierto. Ahora la cabecera de la ficha lleva el boton que la enciende y
apaga y el desplegable con los LFO, con la siembra como valor inicial: arrancar sin la ruta es una decisión
del usuario, arrancarla es la de fábrica. El estado vive en el store (`state.localMorphRoute`, con la
lista de LFO sacada de la tabla de fuentes del contrato) y lo pinta el panel con el mismo snapshot que
las celdas. OFF devuelve la fila 3 a VIRGEN —los defaults del propio contrato de sus tres ids— y ON la
escribe aunque el usuario la hubiera tocado antes, porque es un gesto explícito y no una siembra; con host
no hace nada y se ve deshabilitado. Medido con el WASM real: apagada, `GetMod(28)` da 0 exacto y el anillo
se queda quieto; encendida, barre los dos signos con periodo 998.7 ms, y con LFO 1 la fila viaja con el
índice 1 y el anillo vuelve a bailar. ctest **40/40**, vitest **373/373**, Playwright **8/8**.

**Hecho (2026-09-27) — la ficha RANURAS puede OLVIDAR una ranura (y el aviso va con el del error)**

Cargar era el unico gesto: un modelo equivocado se quedaba ahi para siempre, y en modo local la memoria lo
devolvia en cada F5. Cada fila con modelo lleva ahora su OLVIDAR: la ranura vuelve a EMPTY (con la
entrada vacia de fabrica, para que el motor la descargue por el mismo canal `neuronik:models` que la
carga) y su texto sale de la memoria del navegador, de modo que un F5 no la devuelve. Medido contra el
WASM real: la misma nota suena bit a bit igual que con una ranura nunca cargada. El aviso sale por la
MISMA linea de estado que un fallo de carga, con su tono propio —`✓` acierto, `⚠` acierto a medias (la
sesion se vacio pero el navegador no solto la memoria, verificado releiendo el almacen)—, y si hay fallo
de carga ese manda. Con host el boton no aparece: la ranura es del preset. ctest **40/40**, vitest
**395/395**, Playwright **9/9**.

**Hecho (2026-09-27) — regresion visual del lienzo: una referencia por ficha**

La suite decia que el lienzo EXISTE y que sus textos dicen lo que deben, pero no que se pinte bien: un
token de color invertido o un boton empujado fuera de su ficha no rompen ninguna asercion de texto.
Ahora cada ficha del lienzo tiene su foto de referencia en `e2e/snapshots/` y se compara con
`toHaveScreenshot` (patron ABDMS2000), con el lienzo entero en los dos temas para cazar tambien los
cambios de reparto. La lista de fichas sale del contrato (`SECTIONS`), el viewport es el tamano de
diseno del SSOT y el spec EXIGE escala 1 antes de capturar, porque `mountFitStage` escala el lienzo con
`transform` y una referencia reescalada seria una foto borrosa. El umbral esta medido, no copiado: el
ruido entre corridas es de 0 pixeles y la regresion mas pequena construida (el color de un distintivo)
mueve 77, asi que el `maxDiffPixels: 100` del hermano aqui PASABA en verde; queda 20. Test de ctest
aparte (`NEURONiK_WebUiVisualRegression`) con `RESOURCE_LOCK` sobre el `dist` compartido y puerto
propio. ctest **41/41**, Playwright **11/11** la visual y **9/9** el smoke.

**Pendiente (2026-09-27)** — el indice completo de lo que quedo abierto en la sesion (decisiones que
son tuyas, huecos con sitio exacto y verificaciones no hechas) esta en `HANDOFF.md`, seccion "PENDIENTES:
todo lo que quedo abierto en esta sesion". Lo que decide la proxima direccion, en una linea cada uno:
persistir el conmutador de la ruta del pad; `unloadModel` en el plugin para que OLVIDAR exista con host;
alinear los dos bloques de CZ101 que nacen fuera de fabrica; y el gris por celda del gating, que hoy solo
apaga opciones de un choice y no knobs.

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
