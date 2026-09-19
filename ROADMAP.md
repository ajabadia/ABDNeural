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
| Tab GENERAL | `UI/ParameterPanel.cpp` (276) | **Parcial** (la página copia esa pestaña) | 8.2: cerrar paridad con el contrato |
| Tab RESONATOR | `UI/Panels/OscillatorPanel.cpp` (216) | No | 8.2 + **slots de modelo A–D** (`loadA..loadD`) y carga de modelo espectral |
| Tab FILTER/ENV | `UI/Panels/FilterEnvPanel.cpp` (114) + `UI/EnvelopeVisualizer.h` | No | 8.2 + curva ADSR dibujada en la propia pestaña |
| Tab FX | `UI/Panels/FXPanel.cpp` (169) | No | 8.2 |
| Tab LFO/MOD | `UI/Panels/ModulationPanel.cpp` (189) | No | 8.2 (LFO 1/2 completos + 4 rutas de matriz) |
| Tab BROWSER | `UI/Browser/PresetBrowser.{h,cpp}` + `PresetListModels.h` | **Solo una barra** select+save | 8.3: bancos/categorías, lista, búsqueda, **tags** con sugerencias, metadatos, LOAD/SAVE AS/DELETE, LOAD BANK/SAVE BANK |
| Preset rápido (combo + SAVE + DEL) | `UI/Panels/PresetPanel.cpp` (124) | Parcial | Se subsume en el navegador (una sola superficie de presets) |
| **MIDI Learn por control** (mouseUp sobre el control) + persistencia | `UI/MidiLearner.{h,cpp}` + `Main/MidiMappingManager` | No | 8.3: acción del bridge ("aprende el próximo CC", cancelar, borrar) y mapeo/guardado en el procesador |
| **Menú MIDI del LCD** (8 destinos CC + RESET ALL) | `UI/LcdMenuManager.h` (`ItemType::MidiCC` / `Action`) | No | 8.3 con el LCD |
| **LCD 2 líneas + D-pad** (estados Idle/Navigation/Edit) | `UI/LcdDisplay.{h,cpp}` (166) + `UI/LcdMenuManager.h` | No | 8.3: árbol GLOBAL/RESONATOR/FILTER/EFFECTS/MIDI CONTROL, con ítems que **dependen del `engineType`** |
| Visualizador espectral (64 parciales) | `UI/SpectralVisualizer.{h,cpp}` (97) | No | 8.3 vía canal de lectura nativo→web (`IVisualizationSource`) |
| XYPad (morph X/Y + nombres de modelo) | `UI/XYPad.{h,cpp}` (150) | No | 8.3, mismo canal |
| **Feedback de modulación en cada control** (anillo/overlay del valor modulado) | `UI/CustomUIComponents.h` (`ModulatedSlider` + `Main/ModulationTargets.h`) | No | 8.2: es una función del **control compartido**, no del panel — hoy `@abdsynths/shared` no la tiene |
| Barra de menú File/Edit/Help (cargar preset, zoom, specs MIDI, info RANDOM/FREEZE) | `NEURONiKEditor.cpp` (`getMenuBarNames`/`menuItemSelected`) | No | 8.3 como botones de cabecera o menú web; **los ítems de audio del Standalone no se migran** (los pone el wrapper de JUCE) |
| Diálogo de ayuda + especificaciones MIDI de fábrica | `UI/HelpDialog.h` + `showMidiSpecifications()` | No | 8.3 como overlay |
| Teclado en pantalla | `juce::MidiKeyboardComponent` + `MidiKeyboardState` | **Sí** (tab KEYS, teclado compartido) | Ya resuelto; decidir si vuelve a ser franja fija como en nativo |
| Zoom del editor (base 800×600) | `setZoom()` | No | Se sustituye por editor redimensionable + escala del contenedor |
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
- **`WebPilotVite` (React) queda como contra-piloto** hasta que 8.2 esté cerrada; después se
  retira o se deja como banco de pruebas, pero **no** como segunda implementación de la UI.

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
- **Fuera de circuito a propósito:** `build.bat` sigue exportando `WebPilotVite` a `WebPilot/out`
  (lo que embebe el host del piloto y sirve `start.bat`). Esta carpeta compila a `WebUI/dist` y
  no entra ahí: cambiar el motor de UI es un paso deliberado de 8.2, no un efecto colateral.

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

**8.1 El editor del plugin hospeda la página**
- [ ] Mover a `NEURONiKEditor` lo que hoy vive en `WebPilotHost`: `WebBrowserComponent` +
      `ResourceProvider` (disco en dev con hot-reload, embebido en release — mismo patrón que
      ABDMS2000), y los tres adaptadores (`PresetManagerAdapter`, `MidiInjectionAdapter`,
      `EngineModelsAdapter`). El host del piloto se queda como banco de pruebas hasta 8.4.
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
      - [ ] **Paso 2c:** el arnés del selftest de cuatro direcciones, portado de
        `Source/WebPilotHost.cpp` a algo que abra **Standalone y VST3** (hoy ese arnés solo
        existe en la bancada). Es la última conclusión del piloto sin integrar, y la puerta
        para retirarlo — ver "Retirada del piloto".
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
- **DoD:** Standalone y VST3 abren la página y el selftest de cuatro direcciones pasa igual
  que en el host del piloto; cero rutas absolutas (el VST3 no carga desde el cwd del build).

**Retirada del piloto (decidido 2026-09-19; va JUSTO DESPUÉS de 8.1, no en 8.4)**

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
   (paso 2b). **Antes de borrarla hay que portar su selftest de cuatro direcciones** (paso 2c),
   que es la única implementación que existe. → **Se borra tras 2c.**
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

- **Orden:** 2c (selftest al plugin) → commit de retirada (mudar las 3 SSOT + repuntar los 4
  consumidores + borrar React/bancada) → 8.2 con el campo libre.
- **DoD:** `grep -rn "WebPilot"` fuera del histórico de git y de comentarios que citan el
  traslado no devuelve dependencias vivas; `build.bat` llega a 9 pasos (sin la exportación React
  ni el host); la suite completa sigue verde y el selftest corre contra el plugin.

**8.2 Paridad de control (los 70 parámetros)**
- [ ] Repartir la página con la misma agrupación que el panel nativo: GENERAL
      (`ParameterPanel`), OSC (`OscillatorPanel`), FILTER+ENV (`FilterEnvPanel`), FX
      (`FXPanel`), MOD (`ModulationPanel`).
- [ ] Cubrir los parámetros que hoy solo existen en nativo — incluidos los que el piloto no
      toca: matriz de modulación (4 slots con destino), LFO 1/2 completos, sync/división,
      filtro y sus envolventes, delay sync/división, y los `uiOnly` (freeze ×3, random).
- [ ] Nada de tablas de parámetros a mano: todo desde el contrato generado, y los que no
      estén implementados se marcan (`dspStatus`), no se esconden.
- **DoD:** un preset cargado se ve idéntico en web y en el panel nativo antes de retirarlo;
  ningún control de la UI mueve un parámetro que el motor ignore en silencio.

**8.3 Lo que no es "un parámetro"** (aquí está el grueso del trabajo)
- [ ] **Presets**: navegador con lista/categorías, guardar, renombrar, borrar y estado del
      preset actual (el `PresetBrowser` nativo, hoy reducido a una barra select+save).
- [ ] **LCD + navegación tipo hardware**: `LcdDisplay` + `LcdMenuManager` + D-pad
      (MENU/OK/flechas) y los botones de comando. Es lo que da carácter al instrumento; si se
      simplifica, que sea una decisión escrita, no un olvido.
- [ ] **Visualización en vivo**: `SpectralVisualizer`, scope flotante y `XYPad` (morph X/Y).
      Requiere un canal de datos de solo lectura nativo→web a ~30-60 Hz, con presupuesto de
      CPU medido y sin asignar en el hilo de audio (snapshot con `AudioThreadSnapshot`).
- [ ] **MIDI Learn**: `MidiLearner` + `MidiMappingManager` (aprender, asignar, borrar,
      persistir). El aprendizaje es UI (la web pide "aprende el próximo CC"); el mapeo y su
      guardado siguen en el procesador.
- [ ] **Diálogos y menú**: ayuda, especificaciones MIDI, y el zoom.
- **DoD:** no queda ninguna función de la UI nativa sin equivalente web; la lista de arriba
  está toda tachada o con su "no se migra porque…" escrito en este documento.

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

---

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
