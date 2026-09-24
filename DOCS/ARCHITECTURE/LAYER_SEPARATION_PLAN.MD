# Diseño — Separación de capas en el análisis temporal: material polifónico (Fase 11)

> Estado: DISEÑO (2026-09-23). No hay implementación. Este documento extiende
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
   Corte del dendrograma: capas = componentes con coseno ≥ 0.55 con su medoide;
   clamp a kPolydroneMaxLayers.
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

## 8. Riesgos y decisiones abiertas

- **Coseno vs correlación cruda**: el coseno ignora magnitud (dos capas con la
  misma forma pero escalas distintas caen juntas). Mitigación: la asignación
  por ventana (§3.4) usa energía total; el dendrograma solo agrupa *formas*.
  Si material real produce capas espurias, subir el corte a 0.65.
- **Transiciones**: los armónicos de la voz "migran" (n7 muere mientras n11
  nace — soportes disjuntos con solape parcial). El clustering por forma los
  separa; el emparejador por índice (Fase 10) conecta cada n consigo mismo
  entre frames — las transiciones quedan como energía que entra/sale por
  frameWeight, no como saltos de parcial. Es el comportamiento musical
  correcto (una envolvente que se desplaza).
- **Suelo de detección (50 Hz)**: E1 (41.62 Hz) está fuera del rango del
  estimador. RRISE funciona en el diseño porque la rejilla común la fija el
  usuario/editar (o el refinado detecta 2f0=83 dominante — su firma entra en
  la regla de fundamental débil). Decisión abierta: bajar el suelo a 35 Hz en
  `detectPitchImpl` (barato, pero re-abre la caja de sub-armónicos que el
  refinado acaba de cerrar) o dejar que el usuario fije f0 a mano en el
  ModelMaker (más honesto). Propuesta: opción manual primero, suelo abajo
  solo si el uso lo pide.
- **Presupuesto WASM**: capas×frames multiplica el bloque de modelos
  (TEMPORAL §6). v1 WASM: layerCount clamp a 1 (el worklet suena la capa 1)
  — la paridad nativa/WASM se mantiene y la WebUI avisa.
- **Nombre**: "capas" (layers) no "voces" (voices) — las capas son espectrales
  y estáticas; "voz" ya significa algo en el motor (voice = instancia de nota).

## 9. Lo que este diseño NO cubre (explícito)

- **Detección multi-pitch real** (dos raíces distintas, p. ej. bajo + acorde):
  fuera de alcance; requiere NMF o separación de fuentes de verdad. La
  exención de barrido de la sonda sigue siendo la guardia para ese material.
- **Envolventes temporales editables por capa** (dibujar frameWeights a mano):
  apuntado como extensión de la pestaña Frames.
- **Capas con f0 propia** (remapeo de pitch por capa): el formato lo permite
  (cada capa lleva frameF0 en sus frames) pero el algoritmo de §3 no lo genera;
  una capa con pitch distinto es material multi-pitch (§ arriba).
- **Fase 12 (resíntesis por frames con fase)**: la escalera siguiente del plan
  madre (TEMPORAL §9) — la separación de capas es ortogonal y la beneficia
  (resintetizar cada capa con su fase acumulada es más estable que una sola
  película polifónica).

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
- NMF harmónica queda como fase futura (11.6?) solo para raíces disjuntas,
  con plantillas comb sembradas (CPU libre: es offline).
