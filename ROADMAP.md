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

Solo comienza después de superar el punto de decisión del piloto.


- [ ] Crear el adaptador WebView2/JUCE.
- [ ] Enviar cambios de parámetros al procesador nativo.
- [ ] Recibir snapshots del estado y reflejarlos en la UI.
- [ ] Validar presets, MIDI y persistencia.
- [ ] Embebido de recursos y rutas relativas.
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

## Criterios de aceptación

No se avanzará de fase si se cumple alguna de estas condiciones:

- El Standalone deja de compilar.
- El audio cambia sin una explicación y una referencia documentada.
- Los presets no conservan sus parámetros.
- El bridge genera listeners duplicados o eventos perdidos.
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
