# NEURONiK DSP Parameters

## Propósito

Este documento describe el contrato actual de parámetros entre:

```text
APVTS / interfaz JUCE
        ↓
NEURONiKProcessor::synchronizeEngineParameters()
        ↓
NeuronikEngine o NeurotikEngine
```

Será la referencia inicial para el adaptador web y el futuro piloto Next.js. Los IDs deben conservarse estables; la interfaz web no debe duplicar estos valores en otra lista independiente.

> Estado: inventario inicial, 2026-09-16. Los rangos proceden de `Source/State/ParameterDefinitions.h`.

## Convenciones

- Los parámetros `float` se guardan en el rango indicado.
- Los parámetros de elección almacenan el índice de la opción.
- Los parámetros booleanos almacenan `0` o `1`.
- Los tiempos de envolvente están expresados en segundos en APVTS y se convierten a milisegundos al sincronizar con las voces.
- `masterLevel`, niveles y mezclas usan normalmente valores normalizados `0..1`.

## Motor y nivel global

| ID | Tipo | Rango/opciones | Default | Uso DSP actual |
|---|---|---:|---:|---|
| `engineType` | choice | `NEURONiK`, `Neurotik` | `NEURONiK` | Selecciona la implementación del motor |
| `masterLevel` | float | `0..1` | `0.8` | Ganancia global |
| `masterBPM` | float | `20..400` | `120` | Base de tiempo del sync de LFO y del delay sincronizado |
| `midiThru` | bool | `0/1` | `0` | Opt-in: devuelve al host el MIDI que recibe el motor. Apagado, el plugin no emite MIDI |
| `midiChannel` | choice | `Omni`, `1..16` | `Omni` | Filtra el MIDI del host (el teclado en pantalla siempre suena) |
| `velocityCurve` | choice | `Linear`, `Soft`, `Hard` | `Linear` | Curva aplicada por el procesador a los note-on entrantes |
| `randomStrength` | float | `0..1` | `0.7` | Definido; revisar uso en motor |
| `freezeResonator` | bool | `0/1` | `0` | Definido; revisar uso en motor |
| `freezeFilter` | bool | `0/1` | `0` | Definido; revisar uso en motor |
| `freezeEnvelopes` | bool | `0/1` | `0` | Definido; revisar uso en motor |

## Núcleo aditivo / resonador

| ID | Tipo | Rango | Default | Uso DSP actual |
|---|---|---:|---:|---|
| `oscLevel` | float | `0..1` | `1` | Nivel de voz |
| `oscInharmonicity` | float | `0..1` | `0` | Stretching/inharmonicidad |
| `oscRoughness` | float | `0..0.5` | `0` | Entropía/roughness |
| `morphX` | float | `0..1` | `0` | Morphing espectral X |
| `morphY` | float | `0..1` | `0` | Morphing espectral Y |
| `resonatorRolloff` | float | `0.1..4` | `1` | Caída armónica |
| `resonatorParity` | float | `0..1` | `0.5` | Balance pares/impares |
| `resonatorShift` | float | `0.5..2` | `1` | Desplazamiento espectral |
| `unisonDetune` | float | `0..0.1` | `0.01` | Detune espectral |
| `unisonSpread` | float | `0..1` | `0.5` | Distribución del unison |
| `unisonEnabled` | bool | `0/1` | `0` | Sin ruta y sin control: retirado de la UI, la cantidad la dan detune y spread |

## Excitación Neurotik

Estos parámetros se utilizan principalmente cuando `engineType = Neurotik`.

| ID | Tipo | Rango | Default | Uso DSP actual |
|---|---|---:|---:|---|
| `oscExciteNoise` | float | `0..1` | `0.1` | Nivel de ruido de excitación |
| `excitationColor` | float | `0..1` | `0.5` | Color de la excitación |
| `impulseMix` | float | `0..1` | `0.8` | Mezcla de impulso |
| `resonatorRes` | float | `0.5..1` | `0.99` | Resonancia del banco |

## Envolvente de amplitud

| ID | Tipo | Rango | Default | Conversión |
|---|---|---:|---:|---|
| `envAttack` | float | `0.001..5 s` | `0.01` | `×1000` → ms |
| `envDecay` | float | `0.001..5 s` | `0.1` | `×1000` → ms |
| `envSustain` | float | `0..1` | `0.7` | Directo |
| `envRelease` | float | `0.01..5 s` | `0.5` | `×1000` → ms |

## Filtro y envolvente de filtro

| ID | Tipo | Rango | Default | Conversión/uso |
|---|---|---:|---:|---|
| `filterCutoff` | float | `20..20000 Hz` | `20000` | Directo |
| `filterRes` | float | `0..1` | `0.1` | Directo |
| `filterEnvAmount` | float | `-1..1` | `0` | Directo |
| `filterAttack` | float | `0.001..5 s` | `0.01` | `×1000` → ms |
| `filterDecay` | float | `0.001..5 s` | `0.1` | `×1000` → ms |
| `filterSustain` | float | `0..1` | `0.7` | Directo |
| `filterRelease` | float | `0.01..5 s` | `0.5` | `×1000` → ms |

## Efectos

| ID | Tipo | Rango | Default | Uso actual |
|---|---|---:|---:|---|
| `fxSaturation` | float | `0..1` | `0` | Drive |
| `fxDelayTime` | float | `0.01..2 s` | `0.3` | Tiempo de delay |
| `fxDelayFeedback` | float | `0..0.95` | `0.4` | Feedback |
| `fxDelaySync` | choice | `Free`, `Tempo Sync` | `Free` | Definido; revisar integración |
| `fxDelayDivision` | choice | `1/1`, `1/2`, `1/4`, `1/8`, `1/16`, `1/32`, `1/4t`, `1/8t`, `1/16t` | `1/4` | Definido; revisar integración |
| `fxChorusRate` | float | `0.1..10 Hz` | `1` | Definido; el mapeo actual debe completarse |
| `fxChorusDepth` | float | `0..1` | `0.2` | Definido; el mapeo actual debe completarse |
| `fxChorusMix` | float | `0..1` | `0` | Mezcla de chorus |
| `fxReverbSize` | float | `0..1` | `0.5` | Definido; el mapeo actual debe completarse |
| `fxReverbDamping` | float | `0..1` | `0.5` | Definido; el mapeo actual debe completarse |
| `fxReverbWidth` | float | `0..1` | `1` | Definido; el mapeo actual debe completarse |
| `fxReverbMix` | float | `0..1` | `0` | Mezcla de reverb |

## LFO 1 y LFO 2

Los dos LFO tienen la misma estructura:

| ID | Tipo | Rango/opciones | Default |
|---|---|---:|---:|
| `lfoNWaveform` | choice | `Sine`, `Triangle`, `Saw Up`, `Saw Down`, `Square`, `Random S&H` | `Sine` |
| `lfoNRateHz` | float | `0.01..20 Hz` | `1` |
| `lfoNSyncMode` | choice | `Free`, `Tempo Sync` | `Free` |
| `lfoNRhythmicDivision` | choice | `1/1`, `1/2`, `1/4`, `1/8`, `1/16`, `1/32`, `1/4t`, `1/8t`, `1/16t` | `1/4` |
| `lfoNDepth` | float | `0..1` | `1` |

Sustituir `N` por `1` o `2`. La sincronización actual del procesador aplica waveform, rate y depth; los modos tempo/división deben validarse antes de usarlos como contrato web.

## Matriz de modulación

Hay cuatro slots (`1..4`):

| ID | Tipo | Rango/opciones |
|---|---|---|
| `modNSource` | choice | `Off`, `LFO 1`, `LFO 2`, `Pitch Bend`, `Mod Wheel`, `Aftertouch` |
| `modNDestination` | choice | `Off`, Osc Level, Inharmonicity, Roughness, Morph X/Y, envolventes, filtro, efectos y parámetros resonantes |
| `modNAmount` | float | `-1..1` |

El motor actual sincroniza source, destination y amount para los cuatro slots. Las fuentes de pitch bend, mod wheel y aftertouch están definidas, pero su valor efectivo debe cubrirse con pruebas expresivas.

## Diferencias conocidas del mapeo actual

`ParameterDefinitions.h` define más parámetros de los que `NEURONiKProcessor::synchronizeEngineParameters()` aplica actualmente a cada motor. Estado tras la pasada de conexiones del 2026-09-16:

- **Resueltos**: `midiChannel`, `masterBPM`, `velocityCurve`, `midiThru`, `lfo1/2SyncMode`,
  `lfo1/2RhythmicDivision`, delay sync/division, chorus rate/depth y reverb
  size/damping/width.
- **Retirado del layout**: `harmMix` (nadie lo leía; los presets antiguos que lo llevan se
  migran al cargar, ver más abajo).
- **Retirado del namespace de IDs**: `oscPitchCoarse` (2026-09-19). Estaba declarado en `IDs::`
  desde el primer día pero nunca entró en el layout, así que no lo leía el motor, ni el panel,
  ni ningún preset: era una promesa, no un cableado pendiente. Ver la sección de abajo.
- **Retirado de la UI**: `unisonEnabled` (el motor no lo lee).
- **Solo panel**: `randomStrength` y los tres `freeze*`.
- **Pendiente de oído**: la validación auditiva de los parámetros recién conectados y las
  fuentes expresivas de la matriz.

Esto no debe resolverse duplicando lógica en JavaScript. El adaptador web debe conservar los IDs y marcar explícitamente qué parámetros están implementados, pendientes o solo son host/MIDI.

## Contrato previsto para la web

La UI web debería trabajar inicialmente con un modelo como:

```ts
interface ParameterDescriptor {
  id: string;
  type: 'float' | 'choice' | 'bool';
  min?: number;
  max?: number;
  defaultValue: number | string | boolean;
  options?: string[];
  unit?: string;
  implementedBy?: ('neuronik' | 'neurotik' | 'host')[];
}
```

Este contrato es orientativo para el piloto y no debe sustituir todavía a `ParameterDefinitions.h`.

## Contrato generado (implementado)

El contrato ya no es solo orientativo: se genera desde el propio APVTS.

```text
Source/State/ParameterDefinitions.h  (createParameterLayout)
        ↓  NEURONiK_ParameterExport
WebUI/generated/parameters.generated.json
WebUI/generated/parameters.generated.js
WebUI/generated/parameters.generated.d.ts
```

Características:

- 70 parámetros derivados del layout real; no hay lista duplicada a mano.
- Cada entrada expone `id`, `name`, `group`, `unit`, `kind`, `minValue`, `maxValue`,
  `defaultValue`, `defaultNormalized`, `interval`, `skew`, `symmetricSkew`,
  `discrete`, `choices` y `defaultChoiceIndex`.
- `defaultValue` se publica desnormalizado (valor real); `defaultNormalized` conserva
  el `0..1` que reporta JUCE.
- Los parámetros de elección publican su lista completa de opciones y el índice
  por defecto.
- La agrupación (`group`) es la única metainformación de presentación escrita a mano;
  el resto se lee del parámetro real.

Regeneración:

```bash
cd ABDNeural
cmake --build build-reference --config Release --target NEURONiK_ParameterExport
./build-reference/Release/NEURONiK_ParameterExport.exe WebUI/generated
```

Validación:

```bash
ctest --test-dir build-reference -C Release -R NEURONiK_ParameterDescriptorTest --output-on-failure
```

`NEURONiK_ParameterDescriptorTest` comprueba coherencia con el layout, valores fijados
(`masterLevel`, `filterCutoff`, `engineType`, `fxDelayDivision`, `resonatorRes`, …) y que los
artefactos versionados coinciden con una exportación nueva, de modo que el contrato y la
WebUI no pueden divergir en silencio.

## Estado de implementación DSP en el contrato

Cada descriptor publica ahora `dspStatus`, `engines` y, cuando hace falta, `dspNote`.
Clasificación obtenida inspeccionando `NEURONiKProcessor::synchronizeEngineParameters()` y el
resto de referencias del código, no por suposición:

```text
implemented   65   leídos por el procesador y enviados al motor (o transforman el MIDI)
uiOnly         4   solo accionan un panel; el motor no los ve
notRouted      1   está en el APVTS pero no lo lee nadie fuera de su definición
notInLayout    0   (ninguno: oscPitchCoarse se retiró de IDs:: el 2026-09-19)
```

> Historial (2026-09-16): la primera versión de esta tabla marcaba 54/5/12 porque los cuatro
> parámetros de sync del LFO (`lfo1/2SyncMode`, `lfo1/2RhythmicDivision`) se dieron por
> implementados sin comprobarlo. Una auditoría exhaustiva (referencia por referencia en `Source/`,
> excluyendo `ParameterDefinitions.h`, `ParameterDescriptors.*` y `UI/`) los dejó fuera: el panel
> los ata a un combo, pero el motor no los consume. `unisonEnabled` también pasó a `notRouted`.
> Después se conectaron el filtrado de canal, el tempo y los efectos completos (63/4/4), y en la
> pasada de "sin consumidor" se conectaron la curva de velocidad y el MIDI thru (65/4/2) y,
> por último, `harmMix` se retiró del layout, con lo que la tabla queda en 65/4/1 y el total baja
> de 71 a 70 parámetros (este total es el del layout, que no cambia al retirar el ID: los 71
> contaban también el `notInLayout`). Último cambio (2026-09-19): `oscPitchCoarse` se retira
> del namespace de IDs y `notInLayout` pasa a 0 — el contrato ya no tiene ningún fantasma.

- `engines` indica dónde se consume: `both` (DSP compartido), `neuronik`, `neurotik`,
  `host` (procesador/capa MIDI) o `none`.
  Ejemplos verificados: `filterCutoff` es solo Neuronik; `resonatorRes`, `oscExciteNoise`,
  `excitationColor` e `impulseMix` son solo Neurotik; `midiChannel`, `velocityCurve`, `midiThru`,
  `fxDelaySync` y `fxDelayDivision` son `host`.
- `uiOnly`: `randomStrength` y los tres `freeze*`. Solo los lee
  `ParameterPanel::randomizeParameters()`: escalan y protegen el botón de aleatorizar.
- `notRouted`: `unisonEnabled`, por decisión explícita y no por olvido. Ver la sección
  "Parámetros sin consumidor: decisión tomada".

## Conexiones implementadas (2026-09-16)

### `midiChannel` — filtrado de entrada

Nuevo módulo `Source/Main/MidiChannelFilter.h/.cpp`, aplicado en
`NEURONiKProcessor::processBlock()` **antes** de inyectar la FIFO de la UI. Decisiones de diseño:

- El teclado en pantalla **siempre suena**, sea cual sea el canal configurado: el filtro afecta
  solo al MIDI que llega del host. Si se filtrara después de la inyección, el teclado virtual
  se silenciaría en cualquier canal distinto de 1.
- Los mensajes de sistema (sysex, clock, transporte) reportan canal 0 y **siempre pasan**;
  filtrarlos rompería la sincronía y los volcados de patches.
- El búfer auxiliar se reutiliza y se intercambia con `swapWith`, así que no hay asignaciones en
  el hilo de audio tras los primeros bloques.
- `allNotesOff()` se añadió a `ISynthesisEngine` (implementado una vez en `BaseEngine`) y se
  dispara con un flag atómico cuando cambia el canal, para que no queden notas colgadas. Solo
  ejecuta la fase de release de cada voz: no hay clic. Sirve también como pánico, y se expone en
  `DspEngineFacade::allNotesOff()` para la futura frontera web. `NEURONiK_DSPReferenceTest`
  verifica que las voces se liberan y terminan (≈4,4 s con el release de 500 ms por defecto, ya
  que la envolvente decae de forma multiplicativa hasta `1e-4`).
- Se eliminó el hack `(v->getChannel() == channel || channel == 1)` de aftertouch, pitch bend y
  CC74 en ambos motores: trataba el canal 1 como "todos", lo que contradice cualquier filtrado
  real.

### `masterBPM` y sync de LFO / delay

El LFO ya implementaba tempo sync; faltaba el cableado:

1. `GlobalParams` ahora lleva `bpm` y el juego completo de chorus y reverb.
2. `BaseEngine::updateParameters()` aplica `setSyncMode`, `setTempoBPM` y `setRhythmicDivision`
   a ambos LFO, además de `chorus.setParameters()` y `reverb.setParameters()`.
3. `NEURONiKProcessor::fillGlobalParams()` (nuevo) sustituye el bloque que estaba duplicado en
   las dos ramas del motor y rellena todo, BPM incluido.
4. `fxDelaySync`/`fxDelayDivision` se resuelven en el host: con sync activo el tiempo del delay
   es la duración de la nota elegida al tempo actual.

**Corrección de semántica en el LFO**: `getSyncedRateHz()` multiplicaba por la división en lugar
 de dividir, así que con la etiqueta "1/8" el LFO daba un ciclo cada dos negras. Ahora la
división se interpreta como **longitud** de nota (1.0 = 1/4, 0.5 = 1/8, 4.0 = redonda), que es lo
que documenta `LFO.h` y lo que espera el usuario al elegir en el combo. Nadie llamaba a esa ruta
antes, así que no hay presets afectados salvo los que tuvieran `Tempo Sync` guardado, que hasta
ahora no hacían nada. El límite superior de tempo del LFO pasó de 300 a 400 BPM para cubrir el
rango del parámetro.

La tabla de divisiones vive en `Source/DSP/CoreModules/RhythmicDivision.h` y la comparten el LFO
y el cálculo del delay, así que no pueden divergir.

### Efectos que solo llegaban parcialmente

`fxChorusRate/Depth` y `fxReverbSize/Damping/Width` existían solo en la interfaz. Los defaults del
APVTS (rate 1.0, depth 0.2, size 0.5, damping 0.5, width 1.0) coinciden exactamente con los
valores iniciales de los smoothers del DSP, así que conectarlos **no altera ningún preset**
existente: solo hace que los controles respondan.

La UI web puede consultar esto en `generated/parameters.generated.js` (`dspStatus`, `engines`,
`dspNote`, `CONTRACT_SUMMARY`) para avisar en vez de ofrecer un control que no hace nada.

## Divergencias conocidas y su seguimiento

**No hay ninguna abierta (2026-09-19).** El mecanismo sigue en pie y vacío a propósito:
`getUnroutedParameterIds()` devuelve la lista de IDs declarados en `IDs::` que no están en el
layout, el exportador los publica como `notInLayout` en el contrato y la suite de regresión
falla en cuanto la lista cambia en cualquier dirección. Así, el día que alguien declare un
parámetro fuera del layout, aparece en el contrato y en el test en vez de quedarse invisible.

El histórico de lo que pasó por aquí:

| ID | Qué se hizo |
|---|---|
| `harmMix` (2026-09-16) | Retirado del layout: nadie lo leía y no existe un "harmonic mix" en la voz aditiva |
| `oscPitchCoarse` (2026-09-19) | Retirado del namespace de IDs: ver la sección siguiente |

## Parámetros sin consumidor: decisión tomada (2026-09-16)

Se revisaron los cuatro y no todos se resolvieron igual. El criterio aplicado en los cuatro casos
fue el mismo: **no cambiar el sonido de ningún preset existente sin decirlo**.

| ID | Decisión | Motivo |
|---|---|---|
| `velocityCurve` | **Conectado** | Su default es `Linear`, que es la identidad: conectar la curva no altera la respuesta de ningún preset guardado |
| `midiThru` | **Conectado, opt-in** | El control decía una cosa y el plugin hacía otra (ecoaba siempre). Ahora manda el parámetro: con él apagado no se emite MIDI |
| `unisonEnabled` | **Control retirado de la UI** | Conectarlo como puerta habría silenciado el unison de todos los presets existentes (su default es off). La cantidad ya la gobiernan detune y spread, así que el toggle solo prometía algo que no hacía |
| `harmMix` | **Retirado del layout** | Nadie lo leía y no existe un "harmonic mix" en la voz aditiva: implementarlo sería diseñar sonido sin encargo. Mantener un parámetro muerto "por compatibilidad" solo tiene sentido si alguien lo lee. Ver *Migración de presets* |
| `oscPitchCoarse` | **Retirado del namespace de IDs** | No estaba en el layout, así que no lo leía el motor, ni el panel, ni ningún preset: retirarlo no cambia ni el sonido ni el layout (70 parámetros, los mismos), solo borra la promesa. Ver el detalle abajo |

### Detalles de implementación

- `velocityCurve` — nuevo módulo `Source/Main/VelocityCurve.h/.cpp` (`Soft` = `v^0.6`,
  `Hard` = `v^1.7`). Se aplica **después** de inyectar la FIFO del teclado virtual, criterio
  contrario al del filtro de canal: la curva es parte de la respuesta del instrumento, no de un
  puerto. `Linear` no reserva ni recorre nada, así que el caso por defecto no cuesta nada en el
  hilo de audio. Detalle que evita notas colgadas: una velocidad redondeada a 0 se lee como
  *note-off*, así que el resultado se acota a `1/127` como mínimo.
- `midiThru` — `NEEDS_MIDI_OUTPUT` pasa a `TRUE` en CMake, porque `producesMidi()` ya devolvía
  `true` y la ficha del plugin decía lo contrario. El motor sigue recibiendo toda la entrada; el
  toggle decide si se devuelve al host, y con el default (`false`) el plugin no emite MIDI. Es
  **cambio de comportamiento** respecto al eco accidental anterior, y está documentado como tal.
- `unisonEnabled` — el parámetro se conserva en el APVTS (compatibilidad de presets) pero ya no
  hay control: se retiró de `ParameterPanel` junto con su attachment.
- `oscPitchCoarse` — **retirado del namespace de IDs (2026-09-19)**, y el motivo no es el ahorro
  de trabajo sino que el encargo no existe: no hay spec vigente, ni control en el panel, ni un
  preset que lo lleve, y el draft del que salió (`DOC/3 - ParameterDefinitions.h`, con
  `oscPitchFine`, `oscPitchOctave` y `oscHarmonicCount`, ninguno adoptado) no se sigue. Lo que
  **sí** existe es el camino de pitch por voz (MPE: `EventType::PitchBend` cruza la frontera, el
  procesador inyecta el bend de 14 bits y `IVoice::notePitchBend(semitones)` recalcula
  `pow(2, semis/12)` en cada voz) — un *coarse tune* global es otra cosa. Si algún día se quiere,
  es una feature con su tarea: se implementa en el host como transposición de las notas
  entrantes, junto a `velocityCurve` y `midiChannel`, sin tocar motor ni ABI WASM.

No queda ningún parámetro muerto en el layout: los dos que había se han resuelto en direcciones
distintas, por motivos distintos (uno se conectó porque su default era neutro; el otro se retiró
porque conectarlo habría cambiado el sonido de todos los presets). Y desde el 2026-09-19 tampoco
queda ningún ID declarado fuera del layout: los fantasmas se han ido por los dos lados.

## Migración de presets (retirada de `harmMix`)

Los presets son un volcado del estado del APVTS, así que un parámetro retirado simplemente queda
como hijo inerte en los ficheros antiguos. Eso significa que:

- **Cargar** un preset antiguo funciona: los hijos desconocidos se ignoran sin error.
- **Volver a guardar** ese preset escribía el id muerto otra vez, arrastrándolo hacia adelante
  para siempre. Eso sí era un problema real.

`Source/Serialization/PresetMigration.{h,cpp}` resuelve lo segundo: antes de `replaceState()` se
eliminan los hijos `<PARAM>` cuyo `id` no esté entre los parámetros actuales del procesador. La
función es pura sobre un `ValueTree`, así que se prueba sin tocar ficheros de usuario.

Dos detalles aprendidos al escribir las pruebas:

1. **Un parámetro ausente vuelve a su default**, no conserva el valor anterior. Es el
   comportamiento del APVTS y es el deseable: un preset parcial de una versión antigua no hereda
   valores del preset que se cargó antes.
2. **`METADATA` se retira del estado al cargar.** Ese hijo es metadato del *fichero* (etiquetas),
   no estado del plugin; si se quedaba dentro, al reescribir el preset aparecía duplicado.

Cobertura en `Tests/PresetRoundTripTest.cpp`: round trip completo (round trip de valores reales
incluido un rango con `skew` y dos `choice`), preset legado con `harmMix` y con un id futuro
inexistente, re-guardado sin ids muertos, etiquetas que sobreviven, fichero inexistente, fichero
malformado y preset sin parámetros.


El contrato describe el parámetro APVTS; no garantiza su efecto. La validación auditiva de los
parámetros recién conectados (`delay sync`, `chorus rate/depth`, `reverb size/damping/width`)
sigue pendiente de oído, porque no hay forma de cubrirla con un test automático.
