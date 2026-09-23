# Diseño — Modelos temporales: N frames + eje morphZ en el pad (Fase 10)

> Estado: DISEÑO (2026-09-22). No hay implementación. Este documento es el contrato
> técnico para la fase que convierte el SpectralModel estático en una película
> temporal — el salto que separa "espectro congelado" del paradigma Hartmann Neuron,
> donde el gesto del intérprete navega por la EVOLUCIÓN del análisis, no solo por el
> espacio A–D.
>
> Predecesores ya en producción: analizador multiframe con peak-picking sub-bin
> (commit af89d14, fuente real de los offsets) y modo bow del motor modal. El
> analizador nuevo ES el productor natural de los frames de este diseño.

## 0. Resumen ejecutivo

Hoy: cada slot A–D guarda UN frame (64 amplitudes + 64 offsets). El pad XY morfea
bilinealmente entre los 4 slots. Propuesta: cada slot pasa a guardar **N frames
temporales del mismo modelo** (por defecto 8, el formato admite 1..16), y un tercer
eje **morphZ** (0..1, mapeado a [frame0..frameN-1]) interpola linealmente entre
frames consecutivos **dentro de cada slot** antes del morfeo bilineal existente.
El pipeline de valores queda bit-compat (1 frame ⇒ morphZ inerte), el pad pasa de
2D a 3D por **modulación** (no por un tercer dedo), y el ModelMaker gana la pestaña
de análisis temporal.

## 1. Estructura de datos

### 1.1 SpectralModel extendido (`Source/Common/SpectralModel.h`)

```cpp
struct SpectralModel
{
    // Frames temporales. frames[0] es el frame CANONICO (el modelo entero
    // pre-2026). Los frames siguientes son la evolucion temporal.
    static constexpr int kMaxFrames = 16;

    std::array<std::array<float, 64>, kMaxFrames> amplitudes;       // [frame][partial]
    std::array<std::array<float, 64>, kMaxFrames> frequencyOffsets; // [frame][partial]
    int  frameCount = 1;        // 1..kMaxFrames; 1 = modelo estatico (legado)
    char name[64]   = {};       // ya existe en el JSON; sube al binario
    bool isValid    = false;

    int  numFrames() const noexcept { return frameCount; }
    // Frame interpolado en z (0..1 sobre frameCount-1) -> out. Real-time safe:
    // sin asignacion, llamada por sub-bloque desde la voz.
    void sampleFrame(float z, SpectralModel& out) const noexcept;
};
```

Memoria: 16×64×2 floats = **8 KB por slot**, 32 KB por motor — trivial frente a
los buffers existentes. Con un solo frame (`frameCount==1`) el layout coincide
funcionalmente con el struct de hoy: los presets y modelos legacy no migran bytes,
solo `frameCount` por defecto.

### 1.2 Ruta de valores del pad (nuevo parámetro `morphZ`)

- **APVTS**: `morphZ` (AudioParameterFloat 0..1, skew lineal, default **0.0** —
  el estado por defecto reproduce el modelo estático bit-a-bit; la paridad A–E
  no se mueve).
- **Contrato**: 72º parámetro generado (el export marca unrouted si falta su
  sección); sección MODELOS del cajón + knob en la ficha si el encaje lo permite
  (la banda de MODELOS es de 2 carriles; el destino natural es el EDITAR drawer).
- **Voz** (AdditiveVoice y NeurotikVoice, mismo patrón que morphX/Y):
  `morphZSmoother` (SmoothedValue, 0.02 s) + `modMorphZ` desde la matriz +
  `skip()` por sub-bloque. `modZ` comparte la tabla de destinos de la matriz
  (ver §4).
- Semántica de rango: `z` 0..1 mapea linealmente a [0 .. frameCount-1] del slot
  interpolado. Fuera de rango (frameCount==1): z no tiene efecto — clamp interno
  a 0.

## 2. Pipeline de render (el corazón)

### 2.1 Orden de interpolación: Z primero, XY después

```
frames slot:  A[z] B[z] C[z] D[z]      <- sampleFrame(z) por slot, UNA vez por sub-bloque
                     |                    (z = smoother + mod, ya suavizado)
                     v
bilineal XY existente: lerp(A,B,morphX), lerp(C,D,morphX), lerp(top,bottom,morphY)
                     |
                     v
           paridad/rollOff/shift/stretch/inharmonicidad (hoy, sin cambios)
```

Justificación: sampleFrame es O(128) por slot (4 slots ⇒ O(512)) y se ejecuta UNA
vez por sub-bloque de 32 muestras — **~16k floats/s**, despreciable. El bucle
interno de 64 parciales NO cambia: `updateHarmonicsFromModels(morphX, morphY)`
sigue consumiendo 4 SpectralModel, solo que ahora son los frames muestreados.
El latch de cambios (`anythingChanged`) gana `z != lastZ` y `frameCount > 1`.

Por qué Z-primero y no XY-primero: interpolar en Z antes del morfeo significa
que A–D siguen siendo "el" modelo en cada esquina del pad — el gesto Z nunca
inventa timbres que no están en ningún slot. XY-primero crearía una cuarta
dimensión implícita (morfeo de películas) cuyo espacio de diseño no está
explorado y cuya UI no existe.

### 2.2 Calidad de interpolación

- **Amplitudes**: lerp lineal entre frames consecutivos (i = floor(z·(N-1)),
  frac = resto). Lineal es correcto: frames consecutivos del mismo sonido son
  cercanos; el cruce entre frames lejanos ya lo cubre el morfeo XY.
- **Offsets**: lerp lineal con una guardia nueva — si |offset_i − offset_{i+1}| >
  medio espaciado armónico en algún parcial, se interpola el offset por camino
  corto envolviendo por el armónico (offset relativo a n·f0, no absoluto), para
  que un parcial que "salta" de armónico vecino no barredore todo el espectro.
- **Fase del parcial**: hoy la fase de cada senoide es continua en la frecuencia
  (phaseIncrements acumula). Al cambiar z, la frecuencia de cada parcial se mueve
  suavemente por el lerp de offsets — **sin gestión especial**: el acumulador de
  fase ya es un pitch-shifter continuo. Es la propiedad que hace barato todo el
  diseño (el Neuron original re-faseaba por frame; aquí no hace falta).

### 2.3 Motor modal (NeurotikVoice/ResonatorBank)

Mismo pipeline: sampleFrame(z) por slot → los 4 modelos alimentan el cálculo de
frecuencias de los biquads que ya existe. El retuning de biquads por sub-bloque
ya está presupuestado (los smoothers de cutoff/res lo hacen hoy). Único matiz:
el seteo de Q no se toca — los frames modulan FREQ+OFFSET, no resonancia.

## 3. Formato `.neuronikmodel` v2 (JSON)

El formato actual es JSON (`{amplitudes[64], frequencyOffsets[64], name,
description}` — dialecto fijado por `ModelMakerRoundTripTest`). Extensión
compatible:

```json
{
  "format": 2,
  "name": "Piano C4", "description": "...",
  "frames": [
    { "amplitudes": [...64], "frequencyOffsets": [...64] },
    { "amplitudes": [...64], "frequencyOffsets": [...64] }
  ]
}
```

Reglas de compatibilidad:
- **Lectura v1**: sin `format` (o `format: 1`) ⇒ un solo frame copiado de
  `amplitudes`/`frequencyOffsets` raíz. El plugin actual sigue leyendo v2 con
  `frameCount>1` ignorando los frames extra (carga frames[0] como hoy) — los
  presets antiguos NUNCA se rompen.
- **Escritura**: el ModelMaker escribe v2 siempre; `frames` con un solo elemento
  cuando el análisis es estático.
- **Test de contrato**: extender `ModelMakerRoundTripTest` con un modelo de 3
  frames: roundtrip exacto, lectura v1 de un v2 de 1 frame, y lectura v2 por un
  "plugin v1" (parser que ignora frames) sin fallo.

## 4. UI/UX — el tercer eje en la familia de pads

### 4.1 El pad no se estira: morphZ es eje de MODULACIÓN

Un pad 3D en pantalla es juguete. La casa ya tiene la respuesta: la matriz de
modulación. Reparto propuesto:

- **Pad XY** (ficha MODELOS, centro del lienzo): sin cambios visuales. Añade un
  anillo fino exterior (el mismo lenguaje del anillo de valor modulado de los
  knobs) que muestra el valor ACTUAL de z cuando algo modula morphZ.
- **Knob MORPH-Z** en el drawer EDITAR de MODELOS: el control manual del eje
  (una "línea de tiempo" de 0..1 sobre los frames del modelo).
- **Fuente de modulación nueva**: `morphZ` como DESTINO en la matriz (destino
  nuevo al final de la tabla — las fuentes/destinos se guardan por índice, la
  lección del 8.3). Fuentes naturales: LFO2 (barrido cíclico del modelo =
  "animación"), ENV2 (evolución temporal por nota = el gesto Neuron por
  excelencia), mod-wheel.
- El caso "navegar el tiempo con la rueda de modulación" reproduce literalmente
  el NUKE del Neuron VS.

### 4.2 ModelMaker — pestaña "Frames"

- Vista de película: los N frames en fila, cada uno con su mini-espectro
  (barras de 64), el activo resaltado; scrubber de z arrastrable que refresca
  la vista interpolada.
- Análisis: botón "Analizar evolución" que ejecuta el analizador multiframe
  N-ventanas y reparte las ventanas a frames (ver §5). Frame 0 = ventana de
  mayor energía (el frame canónico).
- Control de frames: añadir frame (duplica el activo), borrar, copiar de slot
  vecino, normalizar por frame.

### 4.3 Cajón MODELOS

Muestra el recuento de frames por ranura (`selftest-model-a · 8 frames`) y el
scrubber de z compartido con el drawer. Sin recuento = modelo estático v1 (o
v2 de 1 frame): la UI no distingue, el motor tampoco.

## 5. Cadena de análisis temporal (ModelMaker)

Hoy el analizador multiframe promedia las ventanas a UN frame. Extensión:

1. N ventanas (por defecto 8) repartidas por el fichero, mismas guardias que
   hoy (RMS < umbral ⇒ ventana descartada; si caen menos de 2, modelo estático).
2. Cada ventana productiva produce un frame completo por el pipeline existente
   (peak-picking ±bins adaptativos + sub-bin parabólico + offsets).
3. **Coherencia de parciales entre frames** (la pieza nueva, y la delicada):
   los parciales de un frame se ordenan por frecuencia y se emparejan con los
   del frame canónico por índice de cercanía (greedy por vecino más próximo con
   coste |Δf| normalizado); un parcial sin pareja hereda offset y amplitud 0 del
   hueco. Sin este paso, el lerp entre frames conecta parciales que no son el
   mismo armónico y el morfeo cruza sonidos en vez de evolucionarlos.
4. Normalización: cada frame a energía comparable (media de amplitud constante
   entre frames), NO cada frame a 1.0 — conservar el acento temporal.

Presupuesto: el análisis es offline (ModelMaker), N·(FFT existente) — nada
nuevo en coste.

## 6. Telemetría y bridge

- `lastModulations` gana la entrada de morphZ (el anillo del pad y del knob
  reutilizan el canal existente).
- Modelo activo por slot ya viaja por el canal de telemetría nativa→web; añadir
  `frameCount` al mensaje de modelos (un int) para que la WebUI pinte el
  scrubber solo cuando hay película.
- WASM: el struct es plain-old-data; el bridge de modelos copia los 4 slots
  por memcpy — cambiar a frames multiplica el tamaño del bloque, presupuesto
  32 KB por instancia, sin cambio de API.

## 7. Plan de fases (cada una commiteable y con test)

| fase | contenido | verificación |
|---|---|---|
| **10.1** | struct + serialización v2 + sampleFrame() | roundtrip de frames en ModelMakerRoundTripTest + paridad A–E bit-exacta (frames=1 ⇒ sin cambio) |
| **10.2** | morphZ: APVTS, smoothers, matriz (destino nuevo), sampleFrame por sub-bloque en ambas voces | paridad 9/9 con default 0 + test nativo "barrido z" (modelo de 2 frames: z=0 = frame0 bit-exacto, z=1 = frame1 bit-exacto, z=0.5 entre ambos) |
| **10.3** | WebUI: knob MORPH-Z en drawer + anillo en pad + contrato 72 | suite WebUI + selftest E2E |
| **10.4** | ModelMaker: análisis temporal + pestaña Frames + emparejamiento de parciales | test del emparejador (seno con vibrato sintético ⇒ frames que siguen el vibrato) |
| **10.5** | Docs (ROADMAP/STYLES no aplica; HANDOFF) + model de ejemplo de 8 frames en los assets del selftest | selftest E2E con modelo temporal |

Las fases 10.1–10.2 son el núcleo (motor), 10.3–10.4 son superficie. Cada fase
deja la suite en verde; ninguna reescribe presets.

## 8. Riesgos y decisiones abiertas

- **Emparejamiento de parciales (10.4)**: el greedy puede fallar con sonidos
  politónicos o ruidosos. Mitigación: v2 guarda frames por los que el analizador
  ya pasó el emparejador; el usuario puede editar a mano en la pestaña Frames.
  No bloquea 10.1–10.3 (con frames hechos a mano, el motor funciona igual).
- **morphZ modulado + modelo de 1 frame**: clamp a 0 — sin efecto. La matriz no
  debe ofrecer el destino morphZ si el modelo activo es estático (la UI puede
  mostrarlo atenuado; el DSP lo ignora igualmente).
- **Reproducción determinista**: z con modulación de LFO es determinista (el
  LFO lo es); con mod-wheel depende del intérprete pero no de hilos — sin
  riesgo de paridad.
- **Formato abierto**: `frameCount > kMaxFrames` en un fichero futuro ⇒ clamp a
  16 y flag en la carga (los 16 primeros frames son los canónicos).

## 9. Lo que este diseño NO cubre (explícito)

- **Interpolación espectral por FFT/resíntesis de fase**: los offsets van por
  lerp de parciales; no hay cruce de dominio frecuencial general. Si el
  emparejador falla en material real, la Fase 11 (resíntesis por frames con
  ventana de Hann y fase acumulada por bin) es la escalera siguiente — mucho
  mayor coste de CPU por voz.
- **Frames de excitación del motor modal** (película del impulso/arco): solo
  los resonadores son time-variant aquí. Apuntado como extensión natural.
- **Un 5º slot "película pura"** independiente de A–D: el diseño mantiene el
  pad XY como geometría canónica y el tiempo como sub-índice de cada esquina.
