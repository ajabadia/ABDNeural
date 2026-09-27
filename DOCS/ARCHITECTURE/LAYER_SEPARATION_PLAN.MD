# Diseño — Separación de capas en el análisis temporal: material polifónico (Fase 11)

> Estado: EN MARCHA (2026-09-25). Implementadas **11.1** (struct de capas
> aditivo, formato v2.1 con roundtrip, un solo escritor y clamp del v1 WASM),
> **11.2** (clustering por envolvente), **11.3** (motor que suma las capas con
> morphZ2/3) y **11.5** (la pestaña «CAPAS» del ModelMaker), cada una con su
> bloque de cierre en §7. Quedan **11.4** (WebUI) y **11.6** (formato ralo).
> Este documento extiende
> `TEMPORAL_MODELS_PLAN.MD` (Fase 10) para el material que hoy el análisis
> temporal no puede modelar honestamente: fuentes con **varias voces simultáneas**
> sobre la misma rejilla armónica (caso real: CZ-RRISE).
>
> La decisión central viene de la evidencia medida (§2): RRISE **no es** polifonía
> de dos raíces — es **una sola nota (E1, 41.62 Hz) con dos capas espectrales**
> que comparten la rejilla n·f0. El diseño generaliza eso: separación de capas
> sobre la rejilla común, no detección multi-pitch.

## 0. Resumen ejecutivo

Hoy `analyzeTemporal` detecta **una** f0 por ventana y produce un frame por
ventana. En material con dos capas espectrales sobre la misma serie armónica
(drone grave persistente + envolvente/formante que trepa por los armónicos
altos), el estimador por ventana persigue la cresta dominante: los frames
saltan de capa a capa y el morphZ barre un hibrido que no es ninguna de las
fuentes. Propuesta: cuando el material muestra **coexistencia de dos familias
de parciales con comportamiento temporal distinto**, el modelo se genera como
**capas** — cada capa con sus frames propios, cada frame con su peso temporal —
y el motor las suma tras el sampler. El coste de formato es un struct nuevo
opcional (v2.1); el motor, con capas=1, es bit-compat. El caso RRISE queda
modelable de punta a punta: **dos morphZ, dos personajes** (drone estable +
barrido brillante).

## 1. Alcance y supuestos

- Aplica a `SpectralAnalyzer::analyzeTemporal` (ModelMaker, offline) y al
  consumo de frames por el motor. No toca el análisis estático (`analyze`).
- La separación se hace **sobre la rejilla común** n·f0 (f0 detectada como hoy,
  con el refinado de fundamental débil ya en producción). No es detección
  multi-pitch: los parciales de todas las capas viven en la misma serie — lo
  que las separa es su **evolución temporal**.
- Un frame puede pertenecer a N capas si su energía se reparte; el formato lo
  permite, el algoritmo de §3 asigna cada pico a UNA capa por ventana.
- Presupuesto de capas: 1..3 (kPolydroneMaxLayers=3). La realidad de un patch
  monofónico CZ nunca supera 2 (drone + voz); 3 deja margen para
  sub-rumble + drone + voz.

## 2. Evidencia medida (CZ-RRISE, 9 ventanas Hann 8192)

Los 121 picos (8/ventana × 9 ventanas, ±15 Hz de deduplicación) caben en
**una rejilla f0 = 41.62 Hz** con residuo medio **2.8 cents** (máx 4.31 Hz),
índices n = 1..18 contiguos:

| capa | parciales | magnitud por ventana (t = 0..9.85 s) | lectura |
|---|---|---|---|
| **drone** | n=1..3 (42/83/125 Hz) | n1: 307,133,177,312,145,165,308,156,154 — constante | persistente, ~constante |
| **voz** | n=7..15 (291→624 Hz) | n7: 461,**0**,119,98,**0**,59,75,**0**,**0**; n15: 0,0,0,13,31,78,323,505,289 | **migrando**: cada armónico sube, canta y muere |

La "voz líder" no tiene pitch propio: es la envolvente espectral moviéndose
por la escalera de E1. Con el estimador actual (suelo 50 Hz, una f0/ventana)
el frame persigue 291→624 y el modelo "media" el drone con el barrido — el
diagnóstico de la sonda ("estático no aplicable: barrido") es el síntoma.

## 3. Algoritmo de separación (offline, en el analizador)

Después de la detección de f0 común (paso existente) y antes del emparejador
de parciales (Fase 10, §5.3):

1. **Tabla pico×ventana**: por ventana, los picos del espectro ya localizados
   por el peak-picking existente, asignados a su índice de rejilla n = round(f/f0)
   (guardia: |f − n·f0| > media-banda ⇒ pico no armónico, se ignora como hoy).
2. **Trazas por índice**: para cada n, el vector temporal de magnitud
   m[n][ventana]. Un índice es *activo* si tiene energía en ≥ 2 ventanas
   productivas.
3. **Matriz de afinidad capas×índices**: clustering jerárquico aglomerativo
   (single-linkage sobre distancia temporal): d(n₁,n₂) = 1 − cos(m[n₁], m[n₂])
   (coseno entre los vectores temporales de ambos índices). Los parciales de
   una capa comparten **forma de envolvente**, no magnitud — el coseno la
   captura (n=1 y n=3 del drone: vectores casi paralelos ⇒ misma capa; n=7 y
   n=15: soportes disjuntos ⇒ d≈1, capas distintas).
   Corte del dendrograma: capas = componentes con coseno ≥ su corte con su medoide
   (descriptores 0.55; EnvelopeCosine 0.60 dedicado, 2026-09-27 calibrado: con
   el dedicado TAMPOCO separa —ver sec 3.3 y sec 3 del dedicado);
4. **Asignación por ventana**: el pico de la ventana k en el índice n va a la
   capa cuya **energía total en esa ventana** sea dominante para n (los frames
   son discretos; no hay fracciones).
5. **f0 de capa**: heredan la f0 común (misma rejilla). El campo frameF0 (10.6)
   queda para el remapeo de pitch; la capa no lo duplica.
6. **Frames por capa**: cada capa produce su película con el pipeline existente
   (emparejador de Fase 10 restringido a los índices de la capa), normalización
   de energía comparable **entre capas** (§5 del plan madre), y un **peso
   temporal** w_l[k] = energía de la capa en la ventana k (normalizado al
   máximo de la capa).
7. **Guardia de degeneración**: si una capa acumula < 10 % de la energía total
   o sus índices activos < 2, se re-absorbe en la dominante (capas=1 ⇒ pipeline
   de hoy, bit-compat por construcción).

## 4. Formato v2.1 (`.neuronikmodel`)

El v2 actual (frames por slot con amps/offsets/frameF0) es un **caso especial**:
capas=1. Extensión aditiva y compatible:

```
"layers": {                       // OPCIONAL; ausente ⇒ capas=1 (v2 puro)
  "layerCount": 2,
  "layers": [ {
      "name": "drone",
      "weight": 1.0,              // mezcla estática de la capa (0..1)
      "frames": [ ... frames v2 ... ],
      "frameWeights": [1.0, 0.9, ...]   // peso temporal por frame
  }, {
      "name": "lead",
      ...
  } ]
}
```

- El lector v2 actual ignora "layers" → sigue leyendo el frame raíz
  (compatibilidad hacia atrás inmediata).
- `frameCount` raíz se mantiene = frames de la capa 1 (los lectores v2 antiguos
  suenan la capa drone, mejor que nada).
- Límite: 3 capas × 16 frames × (64+64+1) floats. Presupuesto WASM: el bloque
  por slot pasa de ~33 KB a ~100 KB en el caso 3×16 — revisar el presupuesto
  de 32 KB del bridge (TEMPORAL §6) o clampear layerCount a 1 en WASM v1.
- GUI ModelMaker: combo de capas + peso estático + scrubber por capa; sin
  edición de envolventes temporales (apuntado como extensión).
- **Extensión rala de parciales (§11, fase 11.6): diseño medido, sin implementar.** La
  misma v2.1 con la lista de índices activos por capa y ratios de inharmonicidad en vez de
  64 offsets en Hz; aditiva, opcional y con el modelo en memoria sin cambios.

## 5. Motor (consumo)

- `AdditiveVoice`/`NeurotikVoice`: por capa, el sampler (Fase 10.2) interpola
  con su morphZ propio (morphZ1/2/3 como destinos de matriz nuevos) y el peso
  temporal w_l[k] modula la salida de la capa antes de la suma. La mezcla
  estática weight_l se aplica como ganancia de capa.
- Suma después del sampler, antes del VCA — mismo gain staging que hoy.
- Paridad: con layers ausente (capas=1) el camino es **el actual bit a bit**
  (misma condición que 10.2: frames=1 ⇒ inerte). Con capas≥2 la paridad
  A–E no aplica (los modelos nuevos no existen aún); el test es el barrido
  de §7.

## 6. UI/UX

- Pad XY/XYZ inalterado: las capas son sub-índices, no geometría.
- Cajón MODELOS: fila por capa (nombre, peso, morphZ). El destino morphZ2/3
  aparece en la matriz solo si el modelo activo tiene ≥ 2 capas (como el
  atenuado de morphZ con modelo estático, TEMPORAL §8).
- ModelMaker: pestaña "Capas" con el dendrograma simplificado (trazas de
  energía por índice coloreadas por capa) — el feedback visual de por qué
  el analizador separó lo que separó.

## 7. Plan de fases (cada una commiteable y con test)

| fase | contenido | verificación |
|---|---|---|
| **11.1** | struct de capas + formato v2.1 + lector/escritor + clamp WASM | roundtrip v2.1 en ModelMakerRoundTripTest + v2 puro sigue cargando + paridad A–E bit-exacta (sin layers) |
| **11.2** | clustering + asignación en SpectralAnalyzer (offline) | test de trazas sintéticas (drone constante + barrido por armónicos altos ⇒ 2 capas con los índices correctos; drone solo ⇒ 1 capa) |
| **11.3** | motor: samplers por capa + morphZ2/3 + pesos temporales | test nativo "barrido por capas" (RRISE sintético: capa 1 plana, capa 2 barre; morphZ2 mueve la voz, morphZ1 no la toca) |
| **11.4** | WebUI: fila de capas en el cajón + matriz con morphZ2/3 condicional + contrato | suite WebUI + selftest E2E |
| **11.5** | ModelMaker GUI: pestaña Capas + ejemplo RRISE en assets | selftest E2E con modelo de 2 capas + sonda CLI reportando capas |
| **11.6** | formato ralo de parciales (lista de índices por capa + ratios de inharmonicidad, §11) | diseño MEDIDO (§11.2); al implementarlo: round-trip con cota, regla de densidad y la degradación pinneada |

### Implementado (2026-09-24) — 11.1

- `SpectralModel` gana el bloque de capas como extensión **aditiva**: `layerCount`
  (1..`kMaxLayers` = 3), `extraLayers[2]` (los frames de las capas 1..2), `layerWeights`,
  `layerFrameWeights` y `layerNames`. **La capa 0 ES la raíz** (`amplitudes` / `extraAmps` /
  `extraOffsets` / `extraF0` / `frameSpanHz`): no hay copia que pueda divergir y los
  lectores/plugins v1 la leen sin enterarse de que existen capas. La API de capas
  (`numFramesOf`, `ampAt`, `ampsOf`, `offsetAt`, `f0At`, `frameWeightAt`, `layerWeightAt`,
  `layerNameAt`, `setLayerCount`, …) resuelve la capa 0 contra la raíz, así que la
  asimetría del almacenaje no se filtra a quien la usa (motor 11.3 y lector/escritor
  hablan solo por ahí). Índices de capa **0-based**; con `layerCount == 1` (el default,
  todo el legado) los arrays quedan inertes y el camino es el de siempre, bit a bit.
- El dialecto `.neuronikmodel` pasa a tener **UN escritor**
  (`Source/Common/SpectralModelWriter.h`): antes había tres copias (el `exportModel()` de
  la GUI, el roundtrip y la sonda RealWav) y las tres tenían que cambiar a la vez. La GUI
  conserva la política (nombre, guardia de afinación, diálogo de fichero); el lector sigue
  en `PresetManager::loadModelFromFile`, que ahora entiende el v2.1.
- **v2 puro intacto**: sin capas no se emite `layers` y `format` sigue siendo el entero 2
  (el JSON no cambia ni un byte). Con capas, `format: 2.1` y
  `layers { layerCount, layers[ { name, weight, frames, frameWeights } ] }`. La raíz sigue
  llevando los frames de la capa 0, y el **nombre y peso de la capa 0 se leen del bloque**
  (en el v2 la raíz lleva el nombre del MODELO, no el de la capa). Un fichero con más de 3
  capas se **trunca** (no se rechaza), como uno con más de 16 frames; una capa sin frames
  válidos corta la lectura y las anteriores se conservan.
- **Clamp del v1 WASM**: `neuronikLoadModel` deja `layerCount = 1` explícito — los 128
  floats planos del puente (amplitudes + offsets) son la capa 0. El bloque por slot se
  queda en ~33 KB (frente a los ~100 KB de un 3×16) y la paridad nativa/WASM no se toca;
  la WebUI avisa de que suena la capa 0.
- **Presupuesto nativo (el que de verdad pagó la fase)**: el modelo pasa de ~8 KB a ~25 KB
  (2 capas extra de 16 frames × 129 floats), y `NEURONiKProcessor` quedaba en ~1 MB porque
  su cola de comandos llevaba **32 modelos inline** (800 KB) — dos instancias en un mismo
  `main` tumbaban `StatePersistenceTest` y `ModelSlotTest` al arrancar, sin un solo
  mensaje. La cola vive ahora en el heap (una reserva en el constructor; el hilo de audio
  solo indexa, no asigna) y el procesador lleva
  `static_assert (sizeof < 128 KB)`: la próxima regresión la para el compilador.
- **Verificación (2026-09-24)**: roundtrip v2.1 en `ModelMakerRoundTripTest` (2 capas:
  nombres, mezclas, frames bit a bit con el clamp de banda, `frameF0` por frame, pesos
  temporales, truncado a 3 y la ranura del engine exponiendo la capa 0) + el v2 puro sigue
  cargando igual + `ctest` 28/28 + **paridad A–E bit-exacta** (referencia nativa
  regenerada idéntica byte a byte; 5 escenarios × 9 casos, 0 ulps, 184 320 muestras) +
  selftest de la bancada en vivo (EXIT=0, las 4 ranuras cargadas y las dos direcciones
  ACCIONES en OK).

### Implementado (2026-09-25) — 11.2

- **`Source/ModelMaker/Analysis/LayerClustering.h`** (nuevo; modulo PURO: sin audio, sin
  estado, determinista — es la bateria de no-regresion del algoritmo). Camino de
  `clusterTraces(traces[64 x frames], numTraces, numFrames)`: descriptores por traza
  (SOPORTE = frames activos / frames, ENTROPIA temporal normalizada) -> aglomerativo de
  afinidad MEDIA -> medoide (colapsa las cadenas del single-linkage sin volver a cortar) ->
  clamp a `kMaxLayers` -> guardia de degeneracion -> orden final.
- **Desviacion MEDIDA respecto a §3.3 (el coseno no decide)**: la afinidad es 1 − distancia
  en el plano (soporte, entropia), no el coseno entre envolventes. Medido: el coseno
  literal entre el drone del RRISE y n7 da **0.69 ≥ 0.55** —los fusionaria— porque es ciego
  al soporte y una traza plana correlaciona con todas (lo que §10.4 anticipaba). El coseno
  se queda expuesto como `envelopeCosine()` para que el test lo MIDA: nadie vuelve a el sin
  medirlo.
- **La metrica es un PARAMETRO, no una constante (2026-09-26)**: `LayerMetric`
  (`Descriptors` por defecto, `EnvelopeCosine` = la letra de §3.3) entra por
  `clusterTraces(..., floor, metric)` y por `analyzeTemporal(..., fixedGrid, layerMetric)`, y
  la ultima llamada la publica `SpectralAnalyzer::lastLayerMetric()`. El punto de eleccion es
  UNO —la lambda de afinidad del aglomerativo—, asi que media, medoide y orden son los mismos
  con las dos metricas. Medido en el test de trazas (seccion E), el coseno pierde en los DOS
  juegos: **sinteticas** (voz en 2 de 8 frames) el coseno drone-voz da **0.49 < 0.55**, no
  fusiona nada y el clamp a 3 + la guardia de degeneracion colapsan en **1 capa** (con
  descriptores, las 2 correctas); **reales** (9 ventanas del RRISE) fusiona de mas (0.69) y
  da **1 capa**. End-to-end con el audio sintetico: **1 capa** con el coseno, 2 con
  descriptores. El coseno no falla por su forma —separa n7 de n15, de soportes disjuntos—
  sino por su ceguera al SOPORTE. Queda disponible para material de trazas LARGAS y
  disjuntas; para cambiarlo, un test y una medida.
- **Desviacion MEDIDA respecto a §3.7 (la guardia)**: "floja" (< 10 % de la energia total)
  exige ADEMAS ser EPISODICA (soporte medio < `kPersistentSupport` = 0.75). Medido en el
  caso de robustez: con la voz 20x mas fuerte que el drone este baja del 10 % de la energia
  y una guardia puramente energetica lo reabsorbia en la voz — la capa 0 pasaba a ser el
  barrido y un lector v2 viejo (que oye la raiz) oiria justo lo que no es el cuerpo. El
  corte 0.75 lo fijan las dos medidas del test: la capa reabsorbible suena en 4 de 8 frames
  (0.50) y el drone que sobrevive al x20 suena en 8 de 8 (1.00). El peso del soporte en la
  afinidad es 2 (medido: con los dos ejes a peso 1 la afinidad MEDIA del grupo de voz
  {n7, n15} contra el drone real del RRISE sale 0.56, justo por encima del corte, y el
  drone se absorbe; pesando el soporte x2 baja a 0.42).
- La guardia reabsorbe en el grupo **mas afin**, no en la dominante (§3.7 literal):
  absorberse en la dominante borraria justo la capa que el clustering acaba de ver.
- **Orden final (no estaba en §3)**: la capa 0 —la RAIZ, lo que oye un lector v2 viejo— es
  la mas PERSISTENTE (soporte medio; a igual soporte, energia). Medido: con el drone del
  RRISE en UN solo parcial, la voz suma MAS energia que el drone (2051 vs 1857), asi que
  ordenar por energia pondria el barrido en la raiz.
- **Analizador** (`SpectralAnalyzer::analyzeTemporal`): una SEGUNDA lectura del MISMO
  espectro contra la rejilla COMUN del llamador (una sola FFT por ventana; dos barridos de
  peak-picking) alimenta el clustering. Con >= 2 capas el modelo lo monta
  `buildLayeredModel`: cada capa escribe SOLO sus indices (los indices sin forma temporal o
  sin actividad se quedan en la capa 0 con su amplitud medida, que es donde los habria
  puesto el camino de una capa), f0 = la comun, pesos temporales = envolvente de la capa
  normalizada a su maximo, mezcla fiel 1.0 y nombres "capa N". Sin capas, el volcado es
  EXACTAMENTE el de antes.
- **Puerta de rejilla** (`gridIsCommon`, corte de 100 cents): si la f0 por ventana se
  separa mas de 100 cents de la del llamador (plegada en octava, como
  `measurePitchDeviation`: un salto de octava del estimador no es un cambio de rejilla, una
  quinta si), el material es un BARRIDO de pitch y no se parte — su representacion honesta
  es el camino de una capa con f0 por frame (10.6). Se expone `lastLayerGridRejected()`.
  El corte se llama hoy **`kOctaveFoldCents`** y `gridIsCommon` es exactamente el veredicto de
  la **puerta de plegado de octava** (§10.3, «Implementado» mas abajo), que publica la medida
  entera en `lastOctaveFold()`.
- **Test** (`Tests/LayerClusteringTest.cpp`, target `NEURONiK_LayerClusteringTest`, 29
  checks). A) trazas sinteticas: drone solo => 1 capa (y una traza plana x5 tampoco inventa
  capas); drone + voz que migra => 2 capas con los indices EXACTOS (la voz es UNA capa
  aunque cada armonico suene en ventanas distintas: la migracion la lleva el emparejador por
  indice, no el clustering); la medicion negativa del coseno + el reparto de las trazas
  REALES del RRISE; robustez (+-15 % de ruido y escalar la voz x20 no mueven un indice);
  guardias de degeneracion (una capa de UN parcial, o con < 10 % de la energia, se
  reabsorbe); clamp a 3; determinismo y bordes (un solo frame, 0 trazas, silencio). B) punta
  a punta: drone 200/400/600 Hz + voz que sube por n7..13 => 2 capas con la raiz = drone (y
  la voz NO duplicada en la raiz), la cresta de la capa de voz subiendo de frame, los pesos
  = su envolvente con pico 1; el mismo material con un barrido fuerte encima => 1 capa y
  `lastLayerGridRejected()` DISPARADA; y un timbre estatico conserva el volcado de siempre
  (frame canonico + extras, sin capas declaradas).
- **Verificacion (2026-09-25)**: test nuevo en verde y `ctest` **29/29** (los 28 de siempre
  + este). Medida sobre el banco real CZ101 (sonda `NEURONiK_ModelMakerRealWavProbe`):
  **SWEP1 es el unico que toma el camino de capas** (f0 estable, timbre por capas) y sale
  con **2 capas** — el cuerpo n1/n5 + la banda alta n19..64 contra la banda n2..n23 que
  barre (pico 0.46; frameWeights 1.00/0.68/0.71/1.00); BASS1, HAMOG, PAD1 y **RRISE siguen
  en 1 capa** (el RRISE barre 102 -> 601 Hz por ventana: la puerta lo rechaza, que es el
  comportamiento disenado y el caso §9 del test lo pinnea). Los assets versionados NO se
  regeneran en esta fase: el ejemplo de capas en assets es la 11.5 y el motor que SUMA las
  capas es la 11.3 (hoy el engine suena la capa 0, que es el contrato del v2.1).
- **Corte DEDICADO para EnvelopeCosine (2026-09-27, calibrado, negativa)**:
  `LayerClustering::kEnvelopeCosineCut = 0.60f` y `clusterTraces` elige corte
  según métrica (descriptores 0.55, coseno 0.60). Barrido 0.10..0.90 (paso 0.05,
  fino 0.45..0.75) sobre los DOS juegos de trazas del criterio:
  sintéticas 8 frames (drone plano vs voz en 2 frames: drone-voz 0.49,
  voz-voz adyacente 0.44, disjuntas 0.00; soporte 1.00 vs 0.25) y reales
  CZ-RRISE 9 ventanas (drone n1/n3 vs n7/n15: n1-n7 0.69, n1-n15 0.55,
  n7-n15 0.09; soporte 1.00 vs 0.56/0.67). Veredicto **NEGATIVO**: NINGÚN
  corte separa drone/voz en ambos a la vez. Sintéticas piden >0.49 para no
  fusionar drone-voz Y ≤0.44 para mantener la voz junta; reales piden >0.69
  Y ≤0.09: huecos INVERTIDOS y sin intersección. AGLOM (media) + medoide +
  clamp a 3 + guardia delgada colapsan el resto: sintéticas 6→3→1 en casi
  todo el rango (solo 0.40 da 3→1), reales 2→1 a 0.55 y 3→1 a ≥0.70.
  Con el dedicado 0.60: sintéticas 1 capa (descriptores 2), reales 1 capa
  (descriptores 2), punta a punta 1 capa (descriptores 2). El coseno no falla
  por el corte sino por ceguera al SOPORTE (traza plana correlaciona con toda
  parcial, disjuntas dan 0). El test lo pinnea (sec E) y el plan sec 3 usa el
  dedicado y documenta la negativa; el defecto sigue siendo descriptores.
- **Tabla comparativa sobre WAV reales (2026-09-27, sonda CLI, 4 frames)**:
  `ModelMakerRealWavProbe` corre Descriptors (0.55) y EnvelopeCosine (0.60
  dedicado) sobre cada WAV con la misma f0 / mismos 4 frames / mismo
  plegado, e imprime al final la tabla por patch (ademas del delta por
  WAV en el log). Resultado MEDIDO:

  | patch | f0 | plegado post/crudo | Descriptors | Cosine (dedicado) | veredicto |
  |---|---|---|---|---|---|
  | CZ-BASS1 | 124.3 Hz | 9.4/1194.4 mono | 1 (35 tr) | 2 (13+22) | DIFIEREN (FP del coseno) |
  | CZ-HAMOG | 124.3 | 0.0/0.0 mono | 1 (63) | 1 (63) | igual |
  | CZ-PAD1 | 124.7 | 4.2/4.2 mono | 1 (17) | 1 (17) | igual |
  | CZ-RRISE | 273.2 | 502.5/1702.5 BI | 1 skip | 1 skip | igual (BI, no paga) |
  | CZ-SWEP1 | 124.6 | 17.5/17.5 mono | 2 (48+16) | 1 (64) | DIFIEREN (FN del coseno) |

  Criterio sintético (8 frames): Descriptors 2/2, Cosine 1/1 (dedicado).
  En CZ101 reales el único que debe dar 2 capas es SWEP1 (lo hace
  Descriptors); Cosine falla en ambos sentidos: falso positivo en BASS1 y
  falso negativo en SWEP1. RRISE es BI-rejilla (pre-filtro 10.4: ambas
  métricas skip, 1 capa, no paga clustering). La tabla vive en la sonda
  (no es un test con umbral: es el dato del diseño, como el residuo por
  bandas) y la propia sonda la publica al final de cada corrida.

## 8. Riesgos y decisiones abiertas

- **Coseno vs correlación cruda**: el coseno ignora magnitud (dos capas con la
  misma forma pero escalas distintas caen juntas). Mitigación: la asignación
  por ventana (§3.4) usa energía total; el dendrograma solo agrupa *formas*.
  Con su corte DEDICADO (0.60, 2026-09-27) el coseno TAMPOCO separa (barrido
  0.10..0.90 en ambos juegos: huecos invertidos, sin intersección); subir el
  corte a 0.65 no lo arreglaba entonces (sec 3 dedicado) y tampoco ahora.
- **Transiciones**: los armónicos de la voz "migran" (n7 muere mientras n11
  nace — soportes disjuntos con solape parcial). El clustering por forma los
  separa; el emparejador por índice (Fase 10) conecta cada n consigo mismo
  entre frames — las transiciones quedan como energía que entra/sale por
  frameWeight, no como saltos de parcial. Es el comportamiento musical
  correcto (una envolvente que se desplaza).
- **Suelo de detección (50 Hz) — DECIDIDO (2026-09-25): la vía MANUAL.** E1 (41.62 Hz) queda
  fuera del ancla del HPS (50 Hz cuantizados al bin: **48.45 Hz** a 44.1 kHz) y el estimador lee
  su 2º armónico. Las alternativas están MEDIDAS con material sintético de la firma de bajo
  (fundamental 0.35 con 2f0 dominante) y con los cinco WAV del banco:

  - **Bajar el suelo a 35 Hz: NO-OP.** 16/16 modelos (banco + E1 + control A1) **byte a byte
    idénticos**. El ancla no era la restricción que ataba: en la firma de bajo el ancla es de
    todos modos el 2º armónico (es el pico más fuerte), y lo que impide bajar de ahí es la
    **puerta de "una octava abajo"**, que exige ±60 cents del ancla/2 mientras un bin a 40 Hz
    mide ~130 cents: es irrealizable por debajo de ~150 Hz. Revertido.
  - **Abrir esa puerta (±60 cents + un bin): arregla E1 y ROMPE CZ-SWEP1.** Con la puerta ancha
    el candidato de sub-octava entra (hps 2,27e9 frente a 8,07e3 del ancla: 12 dB de evidencia
    en la traza paso a paso) y E1 con fundamental débil pasa de 83,2 a 41,6 Hz (mediana −1,8
    cents); pero la escalera de octava deja de rescatarlo **con el caso para el que se afinó** y
    CZ-SWEP1 sale a 62,6 Hz (una octava abajo, 97,9 cents de error acústico). Revertido.
  - **La f0 MANUAL (elegida).** Con la rejilla escrita a mano el modelo es correcto (mediana
    −1,8 cents frente a **501** del automático) y `refineGrid()` la pule con los picos de todas
    las ventanas publicando SU residuo. Lo que faltaba para que la vía fuera real:
    - `analyzeTemporal` **re-estimaba la f0 por ventana por debajo del suelo**: un modelo
      temporal de E1 salía con la rejilla del usuario en el frame 0 y **una octava arriba en los
      demás** (medido: 41,6 / 83,2 / 83,2 / 83,2). Ahora, por debajo de `anchorFloorHz()`, la
      rejilla del llamador manda en TODOS los frames: la f0 por ventana existe para SEGUIR un
      pitch, y ahí no hay nada que seguir (el estimador no llega). Por encima del suelo el
      camino es el de siempre (los cinco modelos del banco: byte a byte idénticos).
    - El chequeo de discrepancia del ModelMaker **pisaba** el f0 escrito a mano (41,62 → 83,24)
      cuando la nota y la octava estaban seleccionadas: por debajo del suelo el detector no es
      evidencia. Y el indicador de rejilla declara el caso ("fijada a mano (el estimador no
      llega hasta aquí)") con el residuo de ESA rejilla.
  - **Verificación:** `SpectralAnalyzerTest` §6 (el valor del suelo, la medición negativa del
    estimador, el modelo y los frames con la rejilla a mano, la frontera por encima del suelo y
    `refineGrid`), `ctest` **29/29** y los cinco modelos del banco byte a byte idénticos. La
    sonda gana `PROBE_F0=<hz>` para medir la vía manual sin GUI.
  - Para material **por debajo de E1** (B0 = 30,87 Hz) o multi-raíz: la misma vía, con la f0 a
    mano.
  - **El modo "rejilla fija" (2026-09-25) hace que esa vía sea declarada.** Un toggle del ModelMaker
    apaga el seguimiento de pitch por ventana (`analyzeTemporal(..., fixedGrid: true)`), lo anuncia en
    el indicador, impide que el detector pise el f0 escrito y lo **escribe en el modelo**
    (`SpectralModel::gridFixed` → `"gridFixed": true`, leído de vuelta por el lector). La clave es
    opcional y ortogonal a las capas (no sube `"format"`; solo se emite cuando es true, así que los
    ficheros de siempre no cambian ni un byte: 16/16 modelos byte a byte idénticos). Es lo que faltaba
    para que "f0 manual" fuera un modo de trabajo y auditable: quien lea el fichero sabe que esa
    rejilla no la confirmó el estimador.

- **Presupuesto WASM**: capas×frames multiplica el bloque de modelos
  (TEMPORAL §6). v1 WASM: layerCount clamp a 1 (el worklet suena la capa 1)
  — la paridad nativa/WASM se mantiene y la WebUI avisa.
- **Nombre**: "capas" (layers) no "voces" (voices) — las capas son espectrales
  y estáticas; "voz" ya significa algo en el motor (voice = instancia de nota).

## 9. Lo que este diseño NO cubre (explícito)

- **Detección multi-pitch real** (dos raíces distintas, p. ej. bajo + acorde):
  fuera de alcance; requiere NMF o separación de fuentes de verdad. La
  exención de barrido de la sonda sigue siendo la guardia para ese material.
  **El caso que lo justifica está medido en 9.1**: SWEP1 —el material que más se parece a
  dos fuentes (fundamental débil con sub-oscilador fuerte, y dos capas en 11.2)— cierra con
  la MISMA mediana acústica (−4,5 cents) indexado en su nota (124,6 Hz) o en su sub-octava
  (**62,18 Hz**): una sola rejilla, dos envolventes. Lo que sí es bi-raíz se declara y se
  exime (RRISE: 502,5 cents post-plegado).
- **Envolventes temporales editables por capa** (dibujar frameWeights a mano):
  apuntado como extensión de la pestaña Frames.
- **Capas con f0 propia** (remapeo de pitch por capa): el formato lo permite
  (cada capa lleva frameF0 en sus frames) pero el algoritmo de §3 no lo genera;
  una capa con pitch distinto es material multi-pitch (§ arriba).
- **Fase 12 (resíntesis por frames con fase)**: la escalera siguiente del plan
  madre (TEMPORAL §9) — la separación de capas es ortogonal y la beneficia
  (resintetizar cada capa con su fase acumulada es más estable que una sola
  película polifónica).

### 9.1 La evidencia de SWEP1: un material, UNA rejilla (62,18 Hz)

> Medido con la sonda (`NEURONiK_ModelMakerRealWavProbe`) sobre
> `../ABDCZ101/DOCS/patches/CZ-SWEP1.wav`, 2026-09-25. Tres corridas del MISMO material: la
> rejilla automática y las dos indexaciones que el usuario puede declarar a mano
> (`PROBE_F0`).

SWEP1 es el material que más se parece a «dos fuentes»: fundamental débil con sub-oscilador
fuerte —la firma que el HPS aprendió a resolver— y, en la rejilla automática, **dos capas**
(48 + 16 índices). Todo eso cabe en **una sola rejilla**, y se ve indexándolo en las dos
octavas en que esa rejilla se puede leer:

| corrida | f0 de la rejilla | mediana acústica (top-8) | f0 por frame | plegado (post / crudo, saltos) | capas | temporal |
|---|---|---|---|---|---|---|
| automática | 124,6 Hz | −4,5 cents (max 212,9) | 124,6 / 124,6 / 124,6 | 17,5 / 17,5, 0 | **2** (48 + 16) | 43 198 B |
| nota declarada | 124,4 Hz | −4,5 cents (max 155,0) | 124,4 / 124,4 / 124,4 | 14,2 / 14,2, 0 | **2** (49 + 15) | 43 193 B |
| **sub-octava declarada** | **62,18 Hz** | **−4,5 cents** (max 97,9) | **124,3 / 124,1 / 123,3** | 14,2 / **1199,7**, 3 | **1** | **16 015 B** |

- **La misma mediana en las tres filas es la prueba, no una coincidencia.** La rejilla de
  2f0 es un **subconjunto** de la de f0: todo lo que explica 124,6 lo explica 62,18 con los
  índices duplicados, y ninguna de las dos añade una raíz. La ambigüedad es de **índice**
  (de una octava), no de **material** (de raíz). Quien la resuelve es la escalera del HPS,
  por la evidencia del 2f0 vacío —medida en la corrida de 124,6:
  `f0/2 = 61,9 Hz mag 117,9 | f0 = 124,1 mag 301,8 | 2f0 = 264,1 mag 2,94 (vacío) |
  3f0 = 372,4 mag 91,7`. Nota de procedencia: 62,18 Hz es la sub-octava del f0 medido
  (124,36/2) y es con la que se tabula aquí; los registros del proyecto tenían 61,9
  (escalera), 62,6 (al abrir la puerta de sub-octava, revertido) y 64,5 (chequeo de
  sub-muestreo) para el mismo candidato.
- **La sub-octava no es la misma rejilla «mal indexada» sin coste: se lleva por delante la
  separación y el tamaño.** Con la rejilla en 62,18 el estimador vuelve a leer una octava
  arriba —124,3 / 124,1 / 123,3, es decir **1199,7 cents CRUDOS en 3 de 4 ventanas**, que la
  puerta de plegado de §10.3 pliega a 14,2 y declara mono-rejilla— y **el clustering ya no
  ve dos capas: ve una** (16 015 B frente a 43 198). O sea: las capas de SWEP1 **no eran dos
  raíces**, eran **dos envolventes sobre la misma rejilla** (el cuerpo persistente y la banda
  que barre), que es literalmente el supuesto de §1 y la razón de que en §11 la lista de
  índices activos se declare **por capa**.
- **Por qué multi-rejilla queda fuera de alcance.** Harían falta dos raíces genuinamente
  distintas: las que **sobreviven al plegado de octava**. En el banco CZ101 solo lo hace
  RRISE, y por barrido, no por dos raíces (**502,5 cents post-plegado con 3 saltos de
  octava**). La respuesta del diseño para ese material es **declararlo y eximirlo**: una capa
  con f0 por frame, que es lo que el analizador ya hace, en vez de inventar capas sobre una
  rejilla que el material no comparte. Detectar y separar dos raíces de verdad es otro
  problema —plantillas comb sembradas, NMF harmónica, hoy 11.7— y su sitio es §10.4, no el
  reparto por envolvente de §3.
- **Consecuencia para el formato (§4 y §11).** Un `frameSpanHz` único por fichero y la
  rejilla heredada por todas las capas (11.1: la capa NO la duplica) son correctos por
  construcción para este material; una capa con f0 propia (arriba) sigue siendo material
  multi-pitch, y por eso queda fuera.

### 9.2 La f0 manual medida: RRISE sí, SWEP1 no es gratis

El §2 ya diagnosticó el caso RRISE —los 121 picos caben en una rejilla **41,62 Hz**, la "voz
líder" es la envolvente espectral subiendo por la escalera de E1 y el estimador, con su suelo
de 50 Hz, persigue 291→624 Hz— y el §8 apunta la vía honesta: la **f0 manual** del ModelMaker.
Faltaba la otra mitad: qué sale del analizador cuando se le da esa rejilla a mano. Medido el
2026-09-25 con la sonda sobre los dos WAV del banco que la piden, en modo declarado
(`PROBE_F0=<hz> PROBE_FIXED=1`: el usuario fija la rejilla y el analizador no la re-estima por
ventana). Confirma el §2 y lo precisa: los picos de cada ventana caen a pocos cents de
n·41,62, así que el "barrido" es artefacto de SEGUIMIENTO — lo que se mueve es la importancia
relativa de los armónicos, y el pico más fuerte sube de 291 Hz (ventana 1) a 666 Hz (ventana
9), del n=7 al n=16.

**RRISE (f0 = 41,62 Hz, E1): el síntoma desaparece y el modelo tiene cuerpo.**

| medida | automática | asset de hoy (291,46 Hz) | f0 manual (41,62) |
|---|---|---|---|
| encaje de los 100 picos medidos (9 ventanas) | 273,2 Hz: mediana 344,0 cents, 6 % ≤ 25 | 418,1 cents, 9 % | **0,9 cents, 100 %** |
| chequeo acústico de la sonda (top-8) | −337,5 / máx 386,0 | — | **−0,7 / máx 11,3** |
| f0 por frame (frames 1..3) | 102,2 / 526,2 / 601,2 Hz | 104,9 / 582,6 / 624,2 | **41,6 / 41,6 / 41,6** |
| plegado de octava | 502,5 cents, 3 saltos ⇒ bi-rejilla | — | **0,0 cents, 0 saltos ⇒ mono-rejilla** |
| parciales activos del temporal (4 frames) | 5 / 13 / 2 / 3 | 4 / 13 / 3 / 3 | **31 / 35 / 43 / 49** |

El último renglón es el musical: en el modelo nuevo la energía **migra por la escalera**
(dominante n=7 → n=10 → n=14 → n=15, con el drone n=1 firme en 42 Hz, 0,53 → 0,26), que es lo
que hace la pieza; en el de hoy el modelo tiene dos a trece parciales y el eje morph mueve la
rejilla entera, o sea un glissando. Ficheros generados: `CZ-RRISE-temporal.neuronikmodel`
(4 frames, `frameF0` = 41,62 en los tres, `gridFixed`) y el estático `CZ-RRISE.neuronikmodel`
(55 parciales activos, mediana −0,7 cents).

**SWEP1 (f0 = 62,5 Hz): mejor encaje, pero NO gratis.** El material es una serie sobre
62,5 Hz (pico más fuerte en 124-125 Hz, n=2; un n=1 a 0,4-0,7 de amplitud), así que esa
rejilla lo encaja mucho mejor que 124,6: mediana 8,9 cents y **95 %** de los 209 picos dentro
de un cuarto de tono, frente a 25,4 y **45 %** con la automática (máx 1 202,1 → 52,3; el
chequeo de la sonda baja su máximo de 212,9 a 97,9). El problema es el otro significado del
número: `frameSpanHz` es también la **referencia de reproducción** —el motor hace `n·base` con
`base` = la nota tocada—, así que un modelo sobre 62,5 suena una **octava arriba** al tocar la
nota del parche (B2). Es lo que la nota de `SpectralAnalyzer.h` llama "abrir la puerta de
sub-octava … ROMPE CZ-SWEP1": no rompe el encaje espectral —lo mejora—, rompe la
correspondencia tecla↔altura. Y al revés: con 124,6 el modelo **no puede** contener la serie
impar (62,5 queda por debajo de n=1, y 188/311/374 caen en n+½), así que pierde el drone de
sub-octava — la sonda lo enseña como diagnóstico (61,9 Hz, mag 117,9), no como parcial.

**Veredicto.** Para RRISE la f0 manual es mejora medida y además **refuerza** §9.1 y §10: el
"bi-rejilla / 502,5 cents" era el estimador persiguiendo la resonancia, no dos raíces — con la
rejilla declarada el plegado da 0,0 cents y una sola capa, o sea que **multi-rejilla sigue
fuera de alcance, y con menos motivo todavía**. Para SWEP1 es un **intercambio** (espectro
completo ⇔ octava del teclado), no una mejora: le falta un cambio estructural que hoy no
existe — **separar la rejilla de ANÁLISIS (el espaciado con el que se mide) de la referencia
de REPRODUCCIÓN (la nota a la que corresponde)**, que hoy son el mismo `frameSpanHz`.
Apuntado, no diseñado.

**Receta (reproducible).** Los ficheros de esta medición están en
`build-reference/probe-manual/` y salen de:

```text
PROBE_F0=<raiz real> PROBE_FIXED=1  NEURONiK_ModelMakerRealWavProbe.exe <wav>
```

En la GUI es lo mismo: escribir la f0 en el campo de pitch y marcar «Rejilla fija» (el modo
declarado que `PROBE_FIXED` reproduce). La rejilla elegida no es crítica al centésimo (41,62 /
41,70 / 41,90 dan 0,9 / 3,3 / 11,4 cents de mediana): lo que decide es **caer del lado correcto
del suelo del estimador**.

## 10. Investigación — separación de fuentes para material polifónico (2026-09-23)

> Estado: INVESTIGACIÓN (sonda empírica ejecutada, sin implementación). Pregunta:
> ¿la separación de fuentes (NMF y familia) mejora el análisis temporal de
> material tipo RRISE frente al clustering de la Fase 11? Evidencia medida con
> sonda NMF euclidea (updates multiplicativos, 150 iter, espectrograma 8192/Hann,
> 24 frames) sobre CZ-RRISE y barrido de rejilla por ventana sobre los tonales.

### 10.1 Taxonomía (qué aplica y qué no)

| técnica | mecanismo | aplicabilidad a capas armónicas |
|---|---|---|
| NMF euclidea / KL | factorización V≈WH sin restricciones | aplicable; ambigua sobre rejilla compartida (§10.2) |
| NMF harmónica (Smaragdis '03, comb templates sembradas) | plantillas comb por f0 candidata | aplicable a fuentes con **espectro disjunto** (multi-raíz real) |
| HPSS (filtros de mediana) | percusivo vs armónico | NO: separa textura, no dos capas armónicas |
| REPET | repetición de fondo | NO: no hay patrón repetitivo en un barrido |
| Deep separation (Conv-TasNet…) | máscara aprendida | fuera de alcance del ModelMaker (pesos+entrenamiento); futuro preprocesador externo |

### 10.2 Resultados empíricos (RRISE)

- **La factorización K=2 es estable**: init aleatorio (mejor de 3 semillas) y
  init sembrado (comb drone n≤6 / voz n≥7) convergen a la MISMA solución
  (intercambio de etiquetas; recon 0.6072 vs 0.6073). La estructura bicapa es
  real y atractiva para el algoritmo.
- **Pero los componentes NO son las capas semánticas**: el reparto de energía
  por índice de rejilla se emborrona en n5–n13 (ambos componentes 40–60 % en
  los mismos bins) y los armónicos n7–n8 de la voz caen en el componente
  "drone". La ambigüedad de compartición es inherente: NMF explica energía,
  no respeta propiedad de índices. El clustering de la Fase 11 (coseno por
  envolvente + asignación entera por ventana) decide donde NMF no decide.
- **La mejora de recon es modesta**: K=1→2: 0.714→0.607 (la mayor parte del
  espectrograma es rango-1: una rejilla de evolución lenta, coherente con §2);
  K=3: 0.515 absorbe el racimo de ataque (n24–31) — claridad semántica
  decreciente.
- **Veredicto RRISE**: NMF como mecanismo de separación, NO; como validador o
  initializer para fuentes de espectro disjunto, SÍ (Fase 11.2+).

### 10.3 La puerta de decisión (hallazgo del barrido de tonales)

Barrido LS de f0 por ventana en [f0/2, 2f0]: BASS1 9.7 cents de dispersión,
HAMOG 0.7 — y **PAD1 1200.5 cents** (inestabilidad de octava, NO birrejilla).
Discriminador barato y con evidencia:

1. **Plegado de octava**: normalizar cada f0 por ventana a [f0, 2f0) (×2^k).
2. Dispersión post-plegado pequeña ⇒ una sola rejilla (PAD1: colapsa a ~0) ⇒
   pipeline actual + regla de fundamental débil. Nada que separar.
3. Dispersión post-plegado grande ⇒ raíces genuinamente distintas (una quinta
   sobrevive al plegado) ⇒ material bi-raíz: análisis por capas con NMF
   harmónica sembrada en las f0 candidatas, o exención honesta.

### 10.4 Consecuencia para las fases

- La fase 11.2 (clustering coseno) queda como mecanismo primario para la
  familia RRISE (rejilla compartida).
- La puerta 10.3 entra ANTES de 11.2 en el flujo del analizador: material
  mono-rejilla no paga clustering.
- NMF harmónica queda como fase futura (hoy **11.7**: la 11.6 la toma el formato ralo
  de parciales de §11) solo para raíces disjuntas, con plantillas comb sembradas (CPU libre:
  es offline).

### Implementado (2026-09-25) — la puerta de plegado de octava (§10.3)

- **La medida** (`SpectralAnalyzer::OctaveFold`): `foldOctaveCents(hz, ref)` reduce la desviacion
  en cents modulo 1200 a (-600, +600] —la equivalencia de octava: la misma convencion, y el mismo
  codigo, que `measurePitchDeviation`— y `measureOctaveFold(f0s, ref)` devuelve, sobre las f0 por
  ventana, la dispersion **post-plegado** (`foldCents`), la **cruda** (`rawCents`), las ventanas que
  la sostienen (`observations`), cuantas saltaron de octava (`octaveFlips`: desviacion cruda mayor
  de media octava) y el veredicto `biGrid` (post-plegado > `kOctaveFoldCents` = 100). Modulo PURO y
  determinista: el test lo prueba con vectores de f0 sinteticos, sin audio. **Sin datos** —sin
  ventanas, sin f0 positiva o sin referencia— `foldCents` es -1 y NO hay veredicto: no se exenta
  nada por falta de evidencia.
- **El plegado es lo que hace comparable la evidencia, y su razon de ser esta medida**: en
  CZ-BASS1 el estimador salta de octava en 1 de 4 ventanas (trayectoria 62,4 / 124,7 Hz) y la
  dispersion CRUDA es de **1194,4 cents**; plegada es de **9,4 cents** — una sola rejilla, con el
  artefacto del estimador fuera de la cuenta. Lo que NO colapsa al plegar es una raiz genuinamente
  distinta: una quinta (701,96 cents) sobrevive como **-498,04**, y una cuarta da el MISMO -498,04:
  la puerta ve «raiz distinta», no «hacia donde».
- **La puerta**: `gridIsCommon` —el predicado que ya decidia si las capas se rechazaban— es
  exactamente el veredicto de `measureOctaveFold` contra la rejilla del llamador (`!biGrid`), asi
  que las decisiones sobre el banco **no cambian** (era la misma comparacion con la octava plegada);
  lo que gana el analisis es que la medida se toma **siempre** (tambien cuando el clustering no
  llega a dar capas, y tambien si el material sale vacio) y se publica con nombre y numeros, asi
  que la sonda (11.5) puede declarar mono/bi-rejilla sin depender de que existan capas que
  rechazar. Consumirla en la UI del ModelMaker (el indicador de rejilla) queda para la 11.4: hoy
  el indicador se actualiza al detectar la f0, ANTES del analisis temporal, y ensenaria una medida
  vieja.
- **Medido con la sonda en los cinco WAV del banco CZ101** (`plegado: post-plegado (crudo),
  ventanas, saltos de octava`):

  | WAV | post-plegado | crudo | saltos | veredicto | capas |
  |---|---|---|---|---|---|
  | BASS1 | 9,4 | 1194,4 | 1 | mono-rejilla | 1 |
  | HAMOG | 0,0 | 0,0 | 0 | mono-rejilla | 1 |
  | PAD1 | 4,2 | 4,2 | 0 | mono-rejilla | 1 |
  | SWEP1 | 17,5 | 17,5 | 0 | mono-rejilla | **2** (48 + 16 indices) |
  | RRISE | 502,5 | 1702,5 | 3 | **bi-rejilla** | 1 |

  SWEP1 conserva sus 2 capas y su reparto (0,513 / 0,487 en el motor): la puerta no le toca nada.
  RRISE sigue sin partirse, ahora con el motivo escrito: sus ventanas (102,2 → 601,2 Hz) no
  comparten la rejilla ni despues de plegar (502,5 > 100). **Nota de metodologia**: el «PAD1 1200,5
  cents» de §10.3 venia del barrido de minimos cuadrados de la investigacion (otra medida); el
  estimador del analizador lee PAD1 estable (124,4..125,0 Hz, 0 saltos), y la inestabilidad de
  octava del ESTIMADOR —el caso que la puerta debe perdonar— es la que reproduce el test con un
  vector sintetico de 62/124 Hz.
- **Test** (`Tests/LayerClusteringTest.cpp`, mitad C: **20 checks nuevos**, 58 en el fichero): la
  medida pura (ventanas alrededor de la rejilla; el salto de octava del estimador, que plegado da 0
  con 1200 crudos; quinta y cuarta con la MISMA dispersion plegada; la frontera de 100 cents; sin
  datos no hay veredicto; la referencia solo importa por su octava; la rejilla declarada da 0) y dos
  casos de punta a punta: el barrido (571,5 post-plegado ⇒ bi-rejilla, modelo de una capa) y un
  **sub una octava por debajo del drone** (crudo 1200,01 con **7 de 8 ventanas** saltadas ⇒ plegado
  0,01 ⇒ mono-rejilla y las capas se quedan: el artefacto del estimador no exenta al material).

- **Referencia PERMANENTE (2026-09-25)**: la semantica del ESTIMADOR DE PITCH —de la que
  depende la puerta— queda PINNEADA en `DSP_PARAMETERS.md`, seccion «Analizador de pitch
  (ModelMaker) — semantica pinneada»: ancla y suelo, los TRES movimientos de la escalera de octava
  con su condicion EXACTA, la cuantizacion asimetrica de la bajada (por que 62 Hz es inalcanzable y
  64,5 si baja), las dos familias de «fundamental debil» (lo que discrimina es si la serie SIGUE o
  MUERE) y la tabla medida del banco CZ101. Es la casa documental de lo que si no se re-deriva en
  cada sesion (ya paso dos veces: el gemelo y la paridad del bin).

### Implementado (2026-09-25) — la puerta de plegado como PRE-FILTRO del analisis temporal (§10.4)

- **Que cambia.** §10.4 decia «la puerta 10.3 entra ANTES de 11.2: material bi-rejilla no paga
  clustering». Hasta ahora la puerta se medIa DESPUES de todo el analisis y el clustering se corria
  igualmente, para rechazarlo acto seguido (`if (layerCount > 1 && !gridIsCommon)`). Ahora es un
  **PRE-filtro**: `analyzeTemporal` va en **dos pasadas** —(1a) la f0 por ventana (la evidencia de la
  puerta), guardando el espectro de cada frame; la PUERTA; (1b) la medida— y la rejilla comun
  (`perFrameCommon`) y el `clustering` **solo corren si la puerta la declara COMUN**. En bi-rejilla no
  se mide la rejilla comun, no se corre el clustering y no se normaliza su tabla: se declara el modelo
  honesto de una capa con f0 por frame (10.6).
- **Por que dos pasadas y no una.** Para decidir ANTES hace falta la f0 de TODAS las ventanas, y la f0
  de cada frame sale de SU espectro. La pre-pasada guarda ese espectro (nFrames x 4097 floats, ~130 KB
  a 8 frames) para no repetir la FFT: el pre-filtro **ahorra** el trabajo del clustering, no lo
  duplica.
- **Salida BIT A BIT igual, medido.** Cuando la rejilla es comun el camino es el de siempre (mismos
  espectros, mismo `measurePartial`, mismo clustering); cuando no lo es, el volcado ya era el de una
  capa. Verificado con la sonda sobre los cinco WAV: la tabla de plegado sale identica (BASS1 9,4 /
  HAMOG 0,0 / PAD1 4,2 / SWEP1 17,5 mono-rejilla; RRISE 502,5 bi-rejilla) y **SWEP1 conserva sus 2
  capas (48 + 16 indices) y su 0,513 / 0,487 en el motor**. Lo unico observable que cambia: en
  bi-rejilla `lastLayerGridRejected()` pasa a true tambien cuando el clustering habria dado una sola
  capa (ya no se calcula) y aparece el diagnostico nuevo.
- **El diagnostico nuevo: `lastClusteringSkipped()`.** Dice que la puerta eximio al material del
  clustering (no se pago). `lastLayerGridRejected()` sigue diciendo que la representacion de capas
  quedo descartada; con el pre-filtro las dos van a true en bi-rejilla, pero significan cosas
  distintas (exencion ANTES vs rechazo DESPUES) y por eso son dos accesos.
- **Test** (`Tests/LayerClusteringTest.cpp`, mitad D: **8 checks nuevos**): bi-rejilla (el barrido) →
  `lastClusteringSkipped()` true, el clustering no corre, el modelo es de una capa con f0 por frame
  **y el analisis SI ocurre** (la f0 por frame sigue el barrido, f0 final 575,1 Hz); mono-rejilla
  (drone + voz) → no se exime y siguen las 2 capas; y el mismo material bi-rejilla con **REJILLA FIJA**
  declarada → `plegado 0` (la f0 por ventana es la del llamador) y NO se exime — la puerta la decide la
  POLITICA de f0, no el audio, que es lo que la hace pre-filtro del ANALISIS. Se actualizaron los
  `note()` de los casos 9 y 12 (ahora informan del SALTADO).

### Implementado (2026-09-25) — 11.3

- **El sampler por capa** (`Source/DSP/FrameSampler.h`): `sampleLayerFrame` interpola UNA
  capa con SU propio z y sus pesos (mezcla estatica `layerWeights` x peso temporal del
  frame `frameWeights`, interpolado), con el mismo algoritmo que `sampleFrame`: mismos
  extremos bit-exactos, mismo camino corto de offsets y misma f0 interpolada en dominio
  log. `sampleLayeredFrame` es el frame EFECTIVO de un slot: la SUMA de sus capas. Con
  `layerCount == 1` delega en el sampler de una capa, asi que el camino de siempre queda
  **bit-exacto** (`x * 1.0f` es exacto en IEEE-754, tambien para -0.0 y NaN).
- **Decision de arquitectura: una suma espectral, no un banco por capa.** El reparto que
  produce 11.2 da a cada indice UNA capa (los 64/64 indices del banco real se reparten sin
  dejar ninguno fuera), asi que sumar amplitudes por indice es EXACTO y no cuesta un banco
  mas: los 128 biquads siguen siendo 128. El offset y la f0 de cada parcial los aporta la
  capa que MAS suena en ese indice — con el reparto del analizador es su dueno exacto, y
  con capas solapadas a mano es una regla determinista y documentada, porque un parcial del
  motor tiene UNA frecuencia, no varias.
- **Los dos motores** (`Resonator` aditivo y `ResonatorBank` modal) ganan
  `setLayerMorphZ(layer, z)` (`setMorphZ` es la capa 0) y su cache de frames se invalida
  con CUALQUIERA de los tres z. Las voces (`AdditiveVoice`, `NeurotikVoice`) llevan
  `Params::morphZ2/morphZ3` con el mismo glide de 20 ms que morphZ y `modMorphZ2/3`
  (destinos 29/30 de la matriz) aplicados antes de actualizar coeficientes.
- **Contrato**: `morphZ2`/`morphZ3` (0..1, default 0.0 = frame canonico: con un modelo de
  una capa son inertes) y destinos "Morph Z 2"/"Morph Z 3" **al final** de la tabla (los
  `choice` de la matriz guardan indice de preset: insertar en medio re-mapearia los
  guardados). Contrato regenerado: **74 parametros** (72 celdas del lienzo + los 2 del pad),
  69 implementados, **31 destinos**.
- **Fallo encontrado en el camino y arreglado**: `IVoice::resetModulations` NO limpiaba
  `modMorphZ` — y la matriz SUMA en cada tramo de control, asi que una ruta a Morph Z
  acumulaba sin freno: el z de la voz corria hasta 1.0, ahi se quedaba y no volvia ni al
  quitar la ruta. Ahora se limpian los tres z (y el test lo pinnea).
- **WebUI (adelanto de lo que 11.4 necesita)**: la pagina POSEER los dos ids —el store
  ignora los mensajes de un id que no tiene, y el motor los mueve— y su control va donde el
  plan manda: la banda MODELOS, en el cajon, junto a MORPH-Z (las tres perillas del eje
  temporal, una por capa). Dos denominadores escritos a mano en los tests (10/27 y 23/28
  destinos) pasan a salir del contrato generado: fueron justo lo que rompio al crecer la
  tabla.
- **Verificacion**: `NEURONiK_LayerEngineTest` (nuevo, 16 checks) con un RRISE sintetico de
  dos capas (capa 0 = drone de dos frames; capa 1 = parcial que barre): bit-exactitud con
  una capa, la suma, los pesos (estatico x temporal), el barrido por capas medido con
  **Goertzel sobre el audio renderizado** (morphZ1 mueve el drone y NO toca al barrido;
  morphZ2 al reves) y el banco modal. `ctest` **30/30**, `vitest` **242/242**, contrato del
  selftest OK, piloto `--selftest` EXIT=0. Y con material REAL: la sonda sobre CZ-SWEP1 mide
  **2 capas (48 + 16 indices) con energia 0,513 / 0,487** en el motor —antes la capa 1 era
  MUDA— y 0,515 de la capa 1 con su propio `morphZ2` a 1.
- **Hallazgo pinneado (medido, NO arreglado)**: el remapeo de rejilla por frame de la 10.6
  no se activa. Los motores calculan `gridRatio = frameF0 / frame.f0At(0)` sobre el frame
  muestreado, pero el snapshot del sampler no copia `frameSpanHz`: `f0At(0)` sale 0 y el
  ratio queda en 1.0 siempre (medido: un frame con `frameF0` de 565,7 Hz da ratio 1).
  Activarlo mueve el sonido de material real ya existente, asi que queda como decision
  explicita, no como efecto colateral de esta fase.
- **Limitacion conocida — LEVANTADA (2026-09-26, fase 11.3 en el navegador):** el puente WASM
  ya no clava `layerCount = 1`: nuevo export `neuronikLoadModelLayers` (UNA llamada construye
  el slot entero, raiz v1 + capa 1), el worklet serializa la capa 1 extra (193 floats/frame +
  pesos) y el parser local (`localModels.js`) extrae la capa del v2.1. Un modelo de 2 capas
  (SWEP1 temporal) suena ENTERO en el navegador: parcial propio de capa 1 (n14, 1831 Hz) con
  energia x15.1 frente a la carga v1. El camino v1 queda bit a bit intacto (paridad 0 ulps).


### Implementado (2026-09-26) — 11.4

**Cada capa, su propio VOLUMEN: la ganancia por capa cruza DSP, frontera y pagina.** La suma
por capas de 11.3 daba a cada capa su propio eje z, pero el volumen lo fijaban SOLO los pesos
estaticos del fichero. Ahora `LayerGains` (una ganancia 0..1 por capa, default 1.0 = legado
bit-exacto) escala cada capa DESPUES de muestrearla, y el dueno del offset/f0 por indice lo
sigue decidiendo la misma regla sobre amplitudes ya escaladas (apagar una capa puede cederle
el mando del offset a la vecina: eso es lo que se oye). Camino completo: `setLayerGain` en
Resonator/ResonatorBank (latch en la invalidacion del cache, junto al de los z),
`layerGain2/3` como VoiceParams con glide de 20 ms en las dos voces,
`setVoiceLayerMorph(g2,g3)` como virtual de BaseEngine con read-modify-write en los dos
motores, export `neuronikSetVoiceLayerMorph` en la frontera (clampeo a [0,1]) y el mensaje
`neuronik:morph` del worklet llevando z2/z3 (undefined = sin cambio). En la pagina el ARO del
pad es el gesto macro: cada emision suya viaja como morphZ + morphZ2 + morphZ3, asi que cada
capa sigue SU linea de frames y la mezcla es la suma. **Semantica documentada: el resonador
NORMALIZA la suma de parciales (invNorm), asi que la ganancia de capa REPARTE el espectro
(como drawbars) — la capa 0 no baja al callar la 1, gana cuota; no es un master de la capa.**
Verificado: `NEURONiK_LayerEngineTest` seccion 7 (el camino de 3 args es el de ganancias en
reposo bit-exacto; ganancia 0 apaga SU parcial sin tocar la raiz; 0.5 conserva el offset del
z2), ctest 34/34, WASM reconstruido con paridad y E2E real en Node: al callar la capa 1, n14
(1831 Hz) cae 4.92e-3 -> 1.44e-3 y la raiz n1 SUBE 0.364 -> 0.539 (reparte, no baja); vitest
299/299 con el contrato del aro pineado en los tres z.


### Implementado (2026-09-26) — el VEREDICTO de la puerta dentro del panel CAPAS (11.5+)

La pestana CAPAS ensenaba COMO se repartio el material (indices, trazas, pesos) pero no POR QUE
la puerta de plegado dejo pasar (o exento) el clustering: la razon se adivinaba. Ahora viaja
DENTRO de la vista: `LayerView::FoldVerdict` (measured/biGrid/foldCents/rawCents/observations/
octaveFlips) la rellena `buildLayerView(model, analyzer.lastOctaveFold())` — la MISMA medida
(`measureOctaveFold`, 10.3) que consume la sonda y decide el clustering, cero politicas nuevas.
El panel pinta una fila sobre la leyenda: **bi-rejilla** (ambar, con cents post-plegado y
crudos, "exento de clustering") o **mono-rejilla** (verde suave, cents post-plegado, saltos de
octava del estimador plegados a acuerdo y ventanas); sin datos o en analisis estatico (la
puerta solo vive en `analyzeTemporal`) declara "sin medida" en vez de inventar un veredicto.
La vista de un argumento (el legado) tampoco declara. Pineado en `NEURONiK_LayerViewTest`
(seccion 4: mono medido, bi con la quinta que sobrevive al plegado, sin ventanas utiles y el
camino de un argumento sin veredicto); ctest 34/34.


### Implementado (2026-09-25) — 11.5

- **La pestana «CAPAS» del ModelMaker** (tercer panel, junto a WAVEFORM y SPECTRAL): una
  columna por parcial (n = 1..64) cuya ALTURA es el pico del parcial normalizado al GLOBAL y
  cuyo COLOR es su CAPA — la 0 es la RAIZ (cian), la 1 magenta, la 2 ambar —, la TRAZA de su
  envolvente por frame encima (normalizada a SU pico: la FORMA, no el nivel) y una leyenda con
  nombre, numero de indices y peso por capa. Es el feedback visual de POR QUE el analizador
  partio (o no) lo que partio: con una sola capa lo dice («mono-rejilla: el analisis no partio
  el material») y con dos ensena el reparto exacto.
- **Los datos de vista son un modulo PURO** (`Source/ModelMaker/Analysis/LayerView.h`,
  `buildLayerView(model)`): `layerOfPartial[64]` (-1 = inactivo), `peakOfPartial[64]`
  (global-normalizado = la altura), `envelope[64][frames]` (forma por parcial), la leyenda
  (`layers[l].{name, weight, frames, partials}`) y los contadores (`activePartials`,
  `highestPartial`). La GUI solo PINTA lo que devuelve; el invariante que explota es el de 11.2
  (cada indice pertenece a UNA capa).
- **Test** (`Tests/LayerViewTest.cpp`, target `NEURONiK_LayerViewTest`, **19 checks**): un modelo
  de dos capas (reparto por indice, inactivos a -1, leyenda, frames por capa, altura
  global-normalizada y forma propia), uno de una capa (todo a la capa 0, sin `layered`) y uno
  VACIO (sin activos ni trazas).
- **Footgun destapado y arreglado de paso**: `SpectralModel::amplitudes` / `frequencyOffsets`
  eran los UNICOS arrays del struct sin inicializador, asi que un `SpectralModel` por defecto
  llevaba **basura de pila** en los 64 parciales (el test del modelo vacio lo cazo: el mismo
  slot de pila reciclado devolvia los datos del caso anterior). Ahora van a `{}` como el resto.
  Sin cambio de audio: la paridad A–E sigue bit-exacta.
- **El ejemplo RRISE y la sonda ya estaban**: `Assets/Models/CZ-RRISE-temporal.neuronikmodel` es
  asset del selftest (y de los factory models) desde el 10.6, y la sonda ya reporta «capas en el
  motor» por WAV (adelanto de la 11.3). Lo que faltaba de la fase era la GUI.
- **Verificacion**: `ctest` **32/32** (el test nuevo incluido) y selftest del piloto **EXIT=0 /
  RESULT: OK**. La GUI compila (`NEURONiK_ModelMaker`); su aspecto es lo unico que no se
  automatiza (es WIN32): queda para el ojo.

## 11. Formato ralo de parciales — diseño (fase 11.6, extensión de v2.1)

> Estado: **DISEÑO (2026-09-25)**, sin implementar. Las cifras de esta sección están
> **medidas** sobre lo que produce hoy el analizador: los modelos temporales de los cinco
> WAV del banco CZ101 que deja la sonda en `build-reference/probe-models` (4 frames,
> `kMaxFrames` = 16). El coste «ralo» es una estimación con los **mismos floats** que
> imprime el escritor (repr de round-trip, 17 dígitos) y el mismo número de claves; el
> coste «denso» es la medida EXACTA de los dos arrays en el fichero.

### 11.1 Qué se propone y por qué

Hoy un frame es **denso por construcción**: 64 amplitudes + 64 offsets, se use el índice o
no. La propuesta es una **ortografía alternativa del mismo dato** dentro de v2.1:

- la lista de **índices activos** se declara UNA vez por capa (no por frame), y
- cada frame lleva, por índice, su **amplitud** y su **ratio de inharmonicidad**
  `r_n = f_n / (n·f0)` en lugar de un offset en Hz.

Son dos cosas distintas y conviene no confundirlas, porque las medidas dicen que **una es
una ganancia de tamaño y la otra es una ganancia de corrección**:

- **El ratio** no es un ahorro de bytes: es la forma **invariante a la transposición** del
  mismo dato, y es lo que hace utilizable el remapeo de pitch por frame de la Fase 10.6
  (hoy muerto: §8 y el bloque 11.3). Con offsets en Hz, mover la rejilla ×2 cambia el
  timbre (un offset de 2 Hz significa 100 cents en n=1 y 50 en n=2); con ratios, no.
- **El ralo** sí es tamaño, pero **solo cuando la capa es una banda fina**. El analizador
  mide los 64 índices contra su media banda, así que el fichero denso está lleno de
  amplitudes pequeñas pero distintas de cero: en material denso el ralo **pierde** (11.2).

La decisión de arquitectura que lo hace barato de implementar: **el formato es ralo, el
modelo en memoria sigue denso**. `SpectralModel` no gana ni un campo (64 amplitudes + 64
offsets por frame, como hoy), la conversión ocurre en el lector y el motor, el sampler y el
puente WASM no se enteran. Con eso, la paridad bit-exacta de los motores no corre ningún
riesgo por esta fase.

### 11.2 El problema de tamaño, medido (y las dos sorpresas)

Actividad por frame con el suelo de detección del propio analizador (`kPartialFloor` =
1e-3 = -60 dB; los índices activos son los que quedan por encima) y coste de los dos
arrays densos, que son **2240 B por frame** en los cuatro ficheros de 4 frames:

| fichero (unidad) | activos > 1e-3 por frame | zeros exactos | ralo/frame | veredicto |
|---|---|---|---|---|
| CZ-BASS1 (raíz) | 57 / 34 / 55 / 31 | 1 / 1 / 0 / 1 | 3725 / 2225 / 3606 / 2028 B | mixto: 2 pierden, 2 ganan |
| CZ-HAMOG (raíz) | 63 / 63 / 63 / 64 | 0 | 4121 / 4109 / 4130 / 4200 B | **pierde** (2×) |
| CZ-PAD1 (raíz) | 16 / 17 / 17 / 17 | 2 / 3 / 0 / 1 | 1027 / 1106 / 1111 / 1107 B | **gana 2,0-2,2×** |
| CZ-RRISE (raíz) | 5 / 13 / 2 / 3 | 0 / 3 / 24 / 29 | 322 / 831 / 130 / 194 B | **gana 7-17×** |
| CZ-SWEP1 capa 0 | 48 / 48 / 48 / 48 | 16 | 3151 / 3133 / 3145 / 3143 B | **pierde** 1,4× |
| CZ-SWEP1 capa 1 | 12 / 10 / 9 / 14 | 52 / 54 / 55 / 50 | 785 / 655 / 584 / 912 B | **gana 2,5-3,8×** |

- **Punto de cruce**: con el coste real de una entrada (≈**53,4 B** con los floats del
  escritor: `{"n":7,"a":0.508345...,"r":1.00018684...}`) el ralo empieza a ganar por debajo
  de **~41 activos de 64 (65 %)** y pierde por encima. La regla que se propone es por
  frame, no por formato: la escribe el escritor con esa cuenta.
- **Sorpresa 1 — el material denso existe.** BASS1 y HAMOG tienen 55-64 activos: sus frames
  son casi todos los índices con algo (armónicos medidos y colas de ventana). Un formato
  ralo aplicado a ciegas **engorda** esos ficheros un 30-40 %. De ahí la regla por frame.
- **Sorpresa 2 — lo que de verdad sobra hoy no es la densidad: es la duplicación.** En un
  fichero de capas, `frames[]` (la película de la raíz) y `layers[0].frames[]` son el MISMO
  contenido (comprobado en CZ-SWEP1: amplitudes y offsets byte a byte iguales, frame a
  frame). Son **8960 B de 43198 (20,7 %)** que el lector **ya ignora** (`for (l = 1; …)`: la
  capa 0 es la raíz y sus frames no se releen). El arreglo no necesita formato nuevo: un
  `"framesFrom": "root"` en la capa 0, o no emitir sus frames (11.3).
- **Contexto**: los arrays densos son 39 046 de los 43 198 B de CZ-SWEP1; el resto es
  `frames`/`layers`/pesos/nombre. Y de esos arrays, **el 23-29 % de todos los ficheros del
  banco son cifras de 17 dígitos** que el escritor imprime por defecto (medido por fichero:
  24 / 22 / 28 / 29 / 23 %). Un formato que quiera pesar menos tiene ahí más margen que en
  la densidad, y es una decisión aparte (11.8).

Los totales del banco, con los arrays densos como base (**53 760 B** en las seis unidades):

| variante | bytes | % del denso |
|---|---|---|
| ralo por frame, **solo cuando gana** (la regla que se propone) | **33 199** | 62 % |
| ralo por frame a ciegas | 40 708 | 76 % (pierde: HAMOG y la capa 0 de SWEP1 van densos) |
| lista de índices hoisteada en la capa | **30 797** | 57 % |

La lista hoisteada da el mejor total porque gana en todas las unidades **menos HAMOG** (su
unión de activos es 64: 9562 frente a 8960 del denso). Y lo que convence es que las dos
variantes se combinan: la lista hoisteada como forma normal de la capa (SWEP1 capa 0: 7213
frente a 8960 densos y 10 320 del ralo por frame; capa 1: 1896 frente a 2448) y, por frame,
escribir todos sus índices o solo los activos. En una capa del clustering la lista **es** el
reparto de 11.2 — los índices de la capa no cambian entre frames (medido: SWEP1 capa 0 con
48 activos idénticos en los 4 frames, 0 cambios), lo que cambia es la amplitud (SWEP1 capa
1: activos 12/10/9/14, cambios 8/11/9 → los ceros de un frame también son información:
"este índice calla aquí").

### 11.3 El registro (sintaxis v2.1)

Todo **aditivo y opcional**; sin ninguna de estas claves el fichero es el v2.1 de hoy, byte
a byte.

```
"layers": {
  "layerCount": 2,
  "layers": [
    { "name": "capa 1", "weight": 1.0, "framesFrom": "root" },   // la capa 0 ES la raiz
    { "name": "capa 2", "weight": 1.0,
      "indices": [7, 11, 12, 15, 17],           // 1-based, asc, sin repetir, <= 64
      "frames": [
        { "amps": [0.5083, 0.12, 0.0, 0.05, 0.31],
          "ratios": [1.00019, 0.99882, 1.0, 1.00214, 0.99931],
          "frameF0": 124.66 }                   // el f0 al que se refieren LOS RATIOS
      ],
      "frameWeights": [0.83, 0.97, 1.0, 0.96] }
  ] }
```

Reglas:

1. **`indices`**: la lista de la capa, hoisteada (11.2: es fija por construcción del
   clustering). `amps` y `ratios` tienen SU longitud, en ese orden. Ausente `indices` ⇒
   frame denso, como hoy.
2. **`framesFrom: "root"`** en la capa 0: sus frames son los de la raíz. Elimina la
   duplicación medida (8960 B en CZ-SWEP1) sin perder nada: el lector ya los ignora.
3. **`ratios[i]`** = `f/(n·f0)` del índice `indices[i]`, con el f0 **de ESE frame**
   (`frameF0`; si falta, `frameSpanHz` del fichero — el frame 0 de la capa 0 nunca lo
   declara, su rejilla ES `frameSpanHz`). 1.0 = armónico exacto. La referencia se declara
   porque **importa**: en CZ-BASS1, el mismo frame 2 con su f0 (62,35 Hz) da `r(n=1)` =
   1.00181 y contra el `frameSpanHz` (124,31 Hz) da 1.00091 — el doble.
4. **Densidad por frame**: si el frame tiene más de `kSparseMaxActive` índices activos
   (> `kPartialFloor`), el escritor escribe `amplitudes`/`frequencyOffsets` densos como hoy
   y **no** escribe `partials`; por debajo, escribe la lista. `kSparseMaxActive` = **41**
   (el cruce medido en 11.2 con el coste real de entrada; deja margen). Un frame nunca
   lleva las dos ortografías: si por lo que sea las lleva, **el lector prefiere la densa**,
   que es la vía trillada.
5. **`partials`** (forma corta del mismo bloque, para frames sueltos — los extra de un v2
   temporal o una capa sin lista): `"partials": [{"n":7,"a":0.508,"r":1.00019}, …]`. Misma
   semántica; se elige entre `indices`+`amps`/`ratios` y `partials` por tamaño (la primera
   gana en cuanto la lista se repite en ≥2 frames).
6. **Banda y clamp**: el invariante que sostiene el camino corto del sampler
   (`|offset| <= 0.5*gap`, gap = `frameSpanHz` o el armónico unitario) se traduce a
   ratios: `|r − 1| <= 0.5*gap/(n·f0)`. El escritor lo aplica al convertir (un ratio fuera
   de banda significa que el parcial pertenece al índice vecino, que es exactamente por lo
   que el análisis ya clampa) y el lector lo re-aplica al leer, para que un fichero escrito
   a mano no rompa el invariante.
7. **`"format"` sigue siendo 2.1.** No sube porque el bloque es una **ortografía del mismo
   dato**, no un dialecto nuevo: la raíz (lo que lee un plugin v1/v2) sigue densa y byte a
   byte igual, y un lector que no conozca las claves cae al camino denso. Lo que sí hay que
   hacer es **declarativo**, como el modo rejilla fija: el ModelMaker escribe el ralo solo
   con el toggle **«Ralo»** activado (por defecto NO) y la sonda con `PROBE_SPARSE=1`. Por
   defecto, los bytes de todos los ficheros de hoy no cambian → las baselines byte a byte
   (16 modelos del banco) se mantienen como red de seguridad.
8. **Índices fuera de rango o repetidos**: se ignoran (no se rechaza el fichero); `n` va
   de 1 a 64. Un `amps` más corto que `indices` se corta al más corto, igual que un fichero
   con más frames de los que caben se trunca.

### 11.4 Los ratios: definición, conversión y precisión

- **Definición**: `r_n = f_n / (n·f0_frame)`. La conversión al modelo denso de siempre es
  `offsetHz = (r_n − 1)·n·f0_frame`, y para leer, `r_n = 1 + offsetHz/(n·f0_frame)`.
- **Por qué el ratio y no el offset en Hz (medido en CZ-BASS1, la trayectoria con salto de
  octava: 124,31 / 124,66 / 62,35 / 123,63 Hz)**: el ratio por índice es estable frame a
  frame —n=5: 1.00047 / 1.00077 / 1.00198 / 1.00062 (**1,8 cents** de dispersión), n=1:
  1.9, n=4: 2.1, n=8: 1.1— mientras el offset en Hz del mismo índice salta de +0,29 a
  +0,62 Hz (x2) y, en los índices altos, de −5,36 a +3,09 Hz (n=25: 8,5 Hz de recorrido para
  el MISMO parcial). Con la rejilla quieta las dos formas son equivalentes; con un
  `frameF0` que se mueve, el Hz miente y el ratio no. Es la representación que hace
  **activable** el remapeo de 10.6 sin romper el timbre.
- **Precisión**: el ratio va como `float` con la impresión de round-trip del escritor
  (17 dígitos significativos) → error relativo ~6e-8, que en el offset es del orden de
  1e-7·|off| (mucho menor que un ulp de cents en cualquier caso audible). **La ortografía
  rala NO es bit-exacta** (divide y multiplica), así que no se usa donde el proyecto exige
  bit-exactitud: los assets versionados y las baselines se siguen escribiendo densos. El
  test fija la cota (11.7).
- **Alternativa considerada y no elegida (por ahora)**: ratios en **ppm enteros**
  (`"rppm": 190`), que imprimen ~6 caracteres en vez de ~17 (un 20 % del coste de entrada,
  medido sobre el mismo `{"n":7,…}`) a cambio de una unidad y un divisor. Se apunta como
  variante si el tamaño vuelve a apretar.

### 11.5 Qué cambia en cada pieza

| pieza | cambio |
|---|---|
| `SpectralModelWriter.h` | la decisión denso/ralo por frame, la lista en la capa, `framesFrom: "root"`, `partials` para frames sueltos |
| `PresetManager.cpp` (`readLayer`) | aceptar `indices`+`amps`/`ratios` y `partials`; convertir a los 64 arrays densos con el f0 del frame y el re-clamp de banda. Hoy exige `amplitudes` de 64 y `break` sin ellas: ese `break` es justo el que dejaría muda una capa rala en un lector viejo (11.6) |
| `SpectralModel` | **nada**: la representación en memoria no cambia |
| motor / `FrameSampler` / puente WASM | **nada**: siguen viendo 64 amplitudes y 64 offsets |
| analizador | nada: ya produce parciales por índice y sus offsets |
| ModelMaker / sonda | toggle «Ralo» (por defecto off) y `PROBE_SPARSE=1`; el indicador por capa del 11.4 puede mostrar el recuento de activos (es la misma cuenta que decide la ortografía) |

### 11.6 Compatibilidad y degradación

| fichero \ lector | v1 (plugin) | v2 (plugin) | v2.1 de hoy (pre-11.6) | 11.6 |
|---|---|---|---|---|
| v2/v2.1 denso (todo lo de hoy) | raíz | raíz + frames | todo | todo, igual |
| 2.1 con capas ralas | raíz | raíz (+ frames, densos) | **la capa rala se pierde** (`break` sin `amplitudes`): suena la raíz | todo |

- La degradación del lector viejo es la que el formato ya declara para quien no entiende
  capas: **suena la raíz**. Es la razón del toggle por defecto apagado: un fichero con
  capas ralas solo sale de la herramienta si un humano lo pidió (o si la sonda lo pidió
  para medir).
- La **raíz nunca se escribe rala**, así que ni el plugin v1 ni el v2 cambian de sonido con
  ningún fichero.

### 11.7 Verificación (el test de la fase)

1. **Round-trip**: denso → ralo → lector ⇒ los 64 arrays iguales con la cota de precisión
   de 11.4 (amplitudes bit-exactas; offsets `|Δ| <= 1e-4 Hz` y, sobre todo, `sampleFrame`
   con diferencia < 1e-6 de amplitud por parcial respecto al modelo denso). El test
   pinnea que la vía rala **no** pretende bit-exactitud.
2. **Regla de densidad**: un frame de `kSparseMaxActive` activos va denso y uno de
   `kSparseMaxActive − 1` va ralo (las dos ramas del escritor, con los bytes contados).
3. **La lista en la capa**: una capa con índices fijos y amplitudes que entran y salen da
   `indices` una sola vez, los ceros en los frames donde el índice calla y el mismo modelo
   al leer.
4. **`framesFrom: "root"`**: el fichero de capas pierde los 8960 B de la duplicación, el
   lector da el mismo modelo y las baselines de los 16 del banco siguen byte a byte
   idénticas (el alias solo se escribe con el toggle).
5. **Compatibilidad negativa** (la degradación, pinneada): quitar `partials`/`indices` de
   un fichero ralo deja el modelo sin esa capa — es el comportamiento documentado, no un
   fallo silencioso.
6. **Sonda**: `PROBE_SPARSE=0/1` sobre los cinco WAV del banco, mismo modelo y mismos
   parciales sonando, con los bytes de los dos dialectos al lado.

### 11.8 Lo que este diseño NO cubre (explícito)

- **Ralo en la raíz / en el v2 estático**: la identidad byte a byte del v2 (y que un v1
  suene la raíz) es un invariante duro; tocarlo pide subir la versión y una historia de
  compatibilidad propia.
- **La impresión de los floats** (medido: 23-29 % de los ficheros): se puede bajar sin
  tocar el formato, pero cambia TODOS los ficheros escritos y sus baselines — decisión
  aparte, más grande que esta.
- **Ley de cuerda rígida** (`r_n = sqrt(1 + B·n²)`: un solo `B` por capa en vez de 64
  ratios): es la extensión natural de «ratios de inharmonicidad» y encaja aquí, pero el
  material del banco no la pide (los ratios medidos se mueven en el rango 0,998-1,007, casi
  todos dentro de ±5 cents) y añade un modelo físico al formato. Apuntada.
- **Contenedor binario o comprimido** (y el presupuesto WASM de §8): el ralo ataca el JSON,
  no el contenedor.
- **Capa con f0 propia** (§9) y **el hueco de `frameSpanHz` en el snapshot del motor**
  (bloque 11.3): el remapeo que da sentido a los ratios sigue sin estar vivo; esto no lo
  arregla, lo deja listo para cuando se decida.

### 11.9 Offsets transpuestos: el ratio, EN VIVO (implementado 2026-09-25)

El §11 describe el ratio como la representación invariante a la transposición y lo deja dentro
del formato ralo. Son dos cosas independientes —el ralo es **tamaño**, el ratio es **semántica
del timbre**— y la segunda se puede tener hoy sin tocar la densidad ni los bytes de ningún
fichero: es lo que se implementó el 2026-09-25 como **modo declarado**, mismo patrón que
«rejilla fija».

**El problema, medido.** El motor suma el offset en Hz: `f_n = n·base + δ`. La desviación en
cents respecto del armónico es `1200·log2(1 + δ/(n·base))` y **depende de la nota**: un δ de
+18,4 Hz sobre la rejilla de 273,2 Hz de CZ-RRISE son **+113,1 cents** en la nota del análisis y
**+57,5** una octava arriba. La inharmonicidad del modelo no se transporta: se diluye (o se
dispara) con el teclado.

**El modo.** `SpectralModel::offsetsTranspose` (+ `offsetRootHz`, la rejilla de referencia). Con
el modo declarado el offset sigue siendo el Hz medido contra la rejilla del análisis, pero el
motor lo **escala** por `base/f0` antes de sumarlo:

```text
f_n = n·base + δ·(base/f0)   =>   cents = 1200·log2(1 + δ/(n·f0))   (independiente de base)
```

que es exactamente `r_n = 1 + δ/(n·f0)`, el ratio del §11.4. Implementación: `offsetScaleAt()`
en el modelo (factor **1.0 exacto** sin modo ⇒ el legado no se mueve) y **una multiplicación por
parcial en los DOS motores**: `Resonator` (aditivo) y `ResonatorBank` (modal).

**La rejilla de referencia y el hueco de `frameSpanHz`.** El motor lee el SNAPSHOT
(`frameCache`), que **no lleva `frameSpanHz`** (el hueco pinneado en §3c de `LayerEngineTest`
que deja inactivo el remapeo de 10.6). Por eso la referencia viaja en un campo propio,
`offsetRootHz`, que el sampler copia de `frameSpanHz`: es el mismo dato que `frameF0`/`gridRatio`
necesitan, pero en un campo aparte para **no encender el remapeo de 10.6** como efecto
colateral. `offsetScaleAt` cae a `frameSpanHz` cuando `offsetRootHz` está vacío (el modelo
fuente: GUI, tests, preview del ModelMaker).

**Alcance explícito.** La referencia es la rejilla CANÓNICA del fichero. Con `frameF0` por frame
(10.6) el ratio se define contra el f0 **de ese frame**, y eso solo se puede hacer bien cuando
10.6 esté vivo; el material del banco comparte rejilla, así que aquí se declara y se acota. En el
morfeo entre ranuras el factor se toma del slot A (el que ancla el morfeo): mezclar un modelo con
el modo declarado y otro sin él no tiene semántica única —los dos offsets no significan lo mismo—
y el modo es una declaración del MODELO, no un parámetro de mezcla.

**Formato.** `"offsetsTranspose": true`, opcional y ortogonal (no sube `"format"`, como
`gridFixed`): ausente = offset en Hz, que es todo lo de siempre. Los bytes de los modelos
actuales no cambian y un lector viejo no ve nada nuevo.

**Evidencia (sonda, 2026-09-25, `PROBE_TRANSPOSE=1`; el parcial elegido es el más desviado con
amplitud ≥ 0,05 de cada WAV):**

| WAV | parcial | δ (Hz) | rejilla | en la rejilla | octava ↑, offset en Hz | octava ↑, ratio |
| --- | --- | --- | --- | --- | --- | --- |
| CZ-RRISE | 1 | +18,4 | 273,2 | +113,1 cents | **+57,5** | **+113,1** |
| CZ-HAMOG | 19 | −15,0 | 124,3 | −11,1 | −5,5 | −11,1 |
| CZ-SWEP1 | 19 | +13,1 | 124,6 | +9,5 | +4,8 | +9,5 |
| CZ-BASS1 | 16 | −1,2 | 124,3 | −1,0 | −0,5 | −1,0 |
| CZ-PAD1 | 3 | +0,2 | 124,7 | +0,9 | +0,5 | +0,9 |

`Tests/TransposableOffsetsTest.cpp` fija las tres propiedades sin depender de la sonda: en la
nota de la rejilla el audio es **bit-exacto** con modo y sin modo; una octava arriba el parcial
**conserva los cents** (medido con Goertzel sobre el motor real: 64,4 → 65,2 cents con el modo,
33,0 sin él); y sin el modo se queda en el offset absoluto. El modo y su rejilla sobreviven al
sampler (los dos caminos) y el lector de producción devuelve la clave
(`ModelMakerRoundTripTest` §12).

**Y el ralo (§11) sigue siendo otra cosa.** Esto no toca la densidad: es el mismo dato denso de
siempre, interpretado mejor — y el ralo, cuando toque, ya tiene su ratio escrito aquí.

