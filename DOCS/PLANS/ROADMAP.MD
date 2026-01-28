# NEURONiK: Master Development Roadmap & Specification

Este documento sirve como la guía técnica y filosófica para el desarrollo de NEURONiK, un instrumento de resíntesis inspirado en la tecnología de modelos neurales (Hartmann Neuron), diseñado para alta eficiencia en múltiples plataformas (PC, Mac, Linux, RPi).

---

## 🟢 FASE 0-8: FUNDAMENTOS Y SÍNTESIS [COMPLETADO]
*Objetivo: Establecer una base de código robusta y un motor de síntesis expresivo.*

- [x] **Arquitectura y UI**: Sistema de pestañas, abstracción de parámetros y optimización del hilo de audio.
- [x] **Motor de Modelos Espectrales**: Migración a un sistema de síntesis basado en modelos de 64 parciales, con capacidad de morphing lineal (A/B).
- [x] **Escultura de Materia Sonora**: Implementación de los parámetros `Inharmonicity` y `Roughness` para la transformación del timbre.

---

## 🟢 FASE 10: NAVEGACIÓN AVANZADA [COMPLETADO]
*Objetivo: Implementar un sistema de morphing 2D para un control tímbrico superior.*

- [x] **Tarea 10.1: Neural Pad (Componente X/Y)**: Se ha reemplazado el morphing lineal por un pad X/Y que permite la mezcla entre cuatro modelos espectrales (A, B, C, D).
- [x] **Tarea 10.2 (Parcial): Bugs Críticos Solucionados**: Se ha corregido el bug de la nota MIDI fija y se ha eliminado el visualizador duplicado de la UI.

---

## 🟢 FASE 14: REFINAMIENTO DE USABILIDAD (UI/UX) [COMPLETADO]
*Objetivo: Mejorar el flujo de trabajo y la claridad de la interfaz basándose en la experiencia de uso.*

- [x] **Tarea 14.1: Reorganización del Panel `RESONATOR`**: Se han movido los controles de `Inharmonicity` y `Roughness` desde el `DASHBOARD` al `RESONATOR` para agrupar todos los controles de escultura de timbre junto al XYPad.
- [x] **Tarea 14.2: Visualizador Espectral Global**: 
    -   Se ha movido el `SpectralVisualizer` a la cabecera de la UI, siempre visible en la mitad derecha.
    -   Se ha implementado una escala de potencia (gamma) para mejorar la visibilidad de armónicos de baja amplitud.
- [x] **Tarea 14.3: Botón de Aleatorización ("Randomize")**: Añadido botón "RANDOM" que genera timbres musicales coherentes y notifica cambios al host.

---

## 🟢 FASE 15: MODULACIÓN AVANZADA (LFOs) [COMPLETADO]
*Objetivo: Dotar a NEURONiK de LFOs como fuentes de modulación flexibles.*

- [x] **Tarea 15.1: Motor de LFO en DSP (Básico)**: Creada la clase `LFO` con formas de onda estándar, `Rate` (con DAW Sync) y `Depth`.
- [x] **Tarea 15.2: Matriz de Modulación (Simple)**: Diseñado e implementado el sistema que conecta las fuentes (LFOs) con los destinos (parámetros) en `NEURONiKProcessor`.
- [x] **Tarea 15.3: Interfaz de Usuario para Modulación**: Creada la nueva pestaña `LFO/MOD` en la UI para controlar los LFOs y la matriz de modulación.

---

## 🟢 FASE 12: GESTIÓN Y CONTROL TOTAL [COMPLETADO]
*Objetivo: Dotar a NEURONiK de un sistema de gestión de sonidos profesional y control MIDI completo.*

### Tarea 12.1: Sistema de Presets (.neuronikpreset) [COMPLETADO]
- [x] **Especificación**: Implementar la capacidad de guardar y cargar el estado completo del sintetizador.
- [x] **Backend**: Clase `PresetManager` implementada con persistencia en XML.
- [x] **UI Básica**: Implementado `ComboBox` (desplegable) en el panel `Parameters` para selección rápida, reemplazando los botones anticuados.
- [x] **UI Avanzada**: Creada pestaña "BROWSER" con 3 columnas (Bank, Preset, Info) y funcionalidad de búsqueda, inspirada en sintetizadores modernos como Osirus.

### Tarea 12.2: Carga de Modelos Espectrales (.neuronikmodel) [COMPLETADO]
- [x] **UI**: Añadir cuatro botones ("Load A", "Load B", etc.) en el `OscillatorPanel`.
- [x] **Lógica**: Implementar la carga de archivos JSON.
- [x] **Soporte Headless**: El preset ahora guarda y restaura las rutas a los archivos `.neuronikmodel` utilizados automáticamente.

### Tarea 12.3: Mapeo de MIDI CC y Controles Estándar
- [x] **Sub-tarea 12.3.1 (Hard-coded)**: Implementar Pitch Bend, Sustain Pedal, Mod Wheel, Main Volume.
- [x] **Sub-tarea 12.3.2 (MIDI Learn) [UI IMPLEMENTADA]**: Implementar la lógica de mapeo dinámico, con gestión desde la UI de escritorio (menú contextual) y desde la UI headless (doble pulsación / pulsación larga).
- [x] **Sub-tarea 12.3.3 (MIDI Thru) [COMPLETADO]**: Implementado parámetro global y lógica de reenvío en `processBlock`.
- [x] **Sub-tarea 12.3.4 (Canal MIDI) [UI COMPLETADA]**: Añadir selector de canal MIDI de entrada (Omni, 1-16). Corrección de bug crítico en filtrado de canales (Float vs Int).
- [x] **Sub-tarea 12.3.8 (Tempo Standalone / Interno)**: Implementado parámetro `masterBPM` que actúa como fuente de tiempo cuando no hay host DAW/MIDI Clock. Añadido control visual en la pestaña GENERAL.
- [x] **Sub-tarea 12.3.5 (Pitch Bend como Modulador)**: Implementado Pitch Bend y Mod Wheel como fuentes en la matriz de modulación.
- [x] **Sub-tarea 12.3.6 (Corrección de Sincronía LFO)**: Corregido bug donde los LFOs avanzaban solo 1 muestra por bloque de audio. Implementada lógica de avance por bloques y mapeo de divisiones rítmicas.
- [x] **Sub-tarea 12.3.7 (Escalado de Modulación)**: Corregido bug de escalado en la matriz donde los valores modulados no respetaban el rango real del parámetro destino (especialmente crítico en Filtro y Envolventes).
- [x] **Sub-tarea 12.3.9 (Delay Sync)**: Implementada sincronización del Delay con el BPM (interno o del host) con selector de divisiones rítmicas.

---

## 🟢 FASE 16: REFINAMIENTO DE SONIDO Y USABILIDAD [COMPLETADO]
*Objetivo: Ajustar parámetros de síntesis y mejorar la claridad de la interfaz basándose en la experiencia de uso.*

- [x] **Tarea 16.1: Ajuste del Parámetro `Roughness`**: Reducir el rango máximo del parámetro `Roughness` a la mitad (ej. de 1.0 a 0.5) para que su efecto sea más musical y se controle la generación de ruido excesivo.
- [x] **Tarea 16.2: Visualización de Nombres en XY Pad**: Mostrar el nombre del modelo cargado en cada una de las cuatro esquinas (A, B, C, D) del XY Pad en la pestaña `RESONATOR`. Esto proporcionará una referencia visual clara de hacia dónde se está realizando el morphing.
- [x] **Tarea 16.3: Robustez de Audio y Polifonía**: Implementada opción de límite de voces (1-8 voces) en el menú "Edit" para gestión de recursos. Optimizado generador de aleatoriedad (Xorshift32) para eliminar cracks en alta polifonía. Solucionados clicks de re-disparo en modo mono.

---

## 🟢 FASE 17: GESTIÓN DE PRESETS AVANZADA [COMPLETADO]
*Objetivo: Mejorar la gestión y manipulación de presets.*

- [x] **Tarea 17.1: Copiar/Pegar Parámetros de Patch**: Implementada funcionalidad vía Portapapeles del sistema en el menú "Edit".
- [x] **Tarea 17.2: Gestor de Presets Avanzado**: Creada pestaña "BROWSER" y selector rápido en "GENERAL" para gestión completa.
- [x] **Tarea 17.3: Portabilidad Total (Embedded Models)**: Los presets (`.neuronikpreset`) ahora serializan internamente los datos de los modelos espectrales usados. Esto elimina la dependencia de archivos externos al compartir patches.

---

## 🟢 FASE 18: NEURONiK MODEL MAKER [COMPLETADO]
*Objetivo: Permitir al usuario crear sus propios timbres (.neuronikmodel) a partir de muestras de audio.*

- [x] **Tarea 18.1: Infraestructura de Librería Compartida**: Separar la definición de `SpectralModel` en una carpeta `Common`.
- [x] **Tarea 18.2: Aplicación Model Maker (Standalone)**: Crear el proyecto de aplicación de escritorio JUCE independiente.
- [x] **Tarea 18.3: Análisis FFT y Extracción de Armónicos**: Implementar carga de audio y algoritmo de detección de picos espectrales para los 64 parciales.
- [x] **Tarea 18.4: Exportación de Modelos**: Guardar el resultado en formato JSON compatible con NEURONiK.

## 🟢 FASE 18.5: REFINAMIENTO DE DETECCIÓN DE PITCH [COMPLETADO]
*Objetivo: Mejorar la precisión del análisis espectral para la creación de modelos.*

- [x] **Tarea 18.5.1: Algoritmo HPS (Harmonic Product Spectrum)**: Implementado para una detección robusta de la frecuencia fundamental.
- [x] **Tarea 18.5.2: UI de Confirmación**: Añadido display de frecuencia detectada y campo de edición manual para correcciones del usuario antes del análisis.
- [x] **Tarea 18.5.3: Integración**: El análisis de armónicos ahora usa la frecuencia confirmada (detectada o manual) para la extracción precisa de parciales.

---

## 🟢 FASE 19: ESCULTURA ESPECTRAL AVANZADA [COMPLETADO & VERIFICADO]
*Objetivo: Implementar la lógica DSP para los parámetros "Legacy" ya definidos en la UI.*

- [x] **Tarea 19.1: Parity (Odd/Even Balance)**: Control de balance entre armónicos pares e impares.
- [x] **Tarea 19.2: Spectral Shift**: Multiplicador de frecuencia para desplazamiento de formantes.
- [x] **Tarea 19.3: Harmonic Roll-off**: Control de caída de amplitud de armónicos superiores (Brillantez).
- [x] **Tarea 19.4: Integración en ModMatrix**: Los 3 parámetros ahora son destinos de modulación.

- [x] **Tarea 19.1: Lógica de Parity (Odd/Even)**: Ajustada la ganancia de parciales impares/pares dinámicamente según el balance.
- [x] **Tarea 19.2: Spectral Shift**: Desplazamiento del espectro de armónicos por encima o debajo de su frecuencia fundamental (estilo formante).
- [x] **Tarea 19.3: Harmonic Roll-off**: Implementada la pendiente de caída global (exponencial) para suavizar o endurecer el timbre.

---

## 🟢 FASE 20: EXPANSIÓN DE EFECTOS (FX 2.0) [COMPLETADO]
*Objetivo: Añadir profundidad espacial y carácter al sonido.*

- [x] **Tarea 20.1: Reverb de Alta Calidad**: Implementado algoritmo de reverb (juce::Reverb) con controles de Size, Damp, Width y Mix.
- [x] **Tarea 20.2: Chorus Estéreo**: Implementado chorus de tres voces con modulación LFO interna.
- [x] **Tarea 20.3: Panel de Control de Efectos**: Creado `FXPanel` dedicado con sliders de cristal y visualización de estado.

---

## 🟢 FASE 21: EXPANSIÓN DE CONTROL MIDI [COMPLETADO]
*Objetivo: Mejorar la capacidad interpretativa y control externo del instrumento.*

- [x] **Tarea 21.1: Curvas de Velocidad y Aftertouch**: Mapeo de presión y velocidad (Linear, Soft, Hard) a parámetros de síntesis.
- [x] **Tarea 21.2: Mapeo MIDI Estándar y Gestión LCD**: 
    -   [x] Mapeo CC dinámico con persistencia y resolución de conflictos.
    -   [x] Menú "MIDI CONTROL" en LCD para asignación de CCs y reset de fábrica.
    -   [x] Soporte para Channel Pressure como fuente de modulación.
- [x] **Tarea 21.3: Tabla de Especificaciones MIDI**:
    -   [x] Implementar diálogo de ayuda con la tabla de implementación MIDI de fábrica.
    -   [x] Añadir acceso desde la barra de menú superior (Help -> MIDI Specs).

---

## 🟢 FASE 20.8: AUDIO RECORDING & ROOT NOTE (MODEL MAKER) [COMPLETADO]
*Objetivo: Facilitar la creación de modelos permitiendo grabación directa y gestión precisa de la nota raíz.*

- [x] **Tarea 20.8.1: Grabación de Audio**:
    -   [x] Habilitación de entradas de audio en `deviceManager`.
    -   [x] Implementación de lógica de grabación circular en buffer temporal.
    -   [x] Botón "REC" dinámico con estados de grabación/parada.
- [x] **Tarea 20.8.2: Selección de Nota Raíz Dividida**:
    -   [x] Reemplazo de selector único por selectores independientes de Nota y Octava.
    -   [x] Modo "Auto" por defecto para sincronización con detección de pitch.
- [x] **Tarea 20.8.3: Sincronización y Validación de Pitch**:
    -   [x] Detección automática de pitch tras carga/grabación de audio.
    -   [x] Lógica de discrepancia: aviso al usuario si la nota seleccionada varía > 1 semitono del pitch detectado.
    -   [x] Refactorización de proceso de análisis para soportar diálogos asíncronos.

---

## 🟢 FASE 22: MEJORA DE ALEATORIZACIÓN [COMPLETADO]
*Objetivo: Dar más control sobre el motor de Randomización.*

- [x] **Tarea 22.1: Randomization Strength**: Añadir un slider ("STRENGTH") que controle cuánto se desvían los parámetros de su valor actual al pulsar RANDOM.
- [x] **Tarea 22.2: Bloqueo de Parámetros (Freeze)**: Permitir "bloquear" ciertos módulos (ej. Envolventes) para que no sean afectados por la aleatorización.

---

## 🟢 FASE 23: FEEDBACK VISUAL Y ANIMACIÓN [COMPLETADO]
*Objetivo: Hacer que la interfaz refleje el estado dinámico del motor de síntesis.*

- [x] **Tarea 23.1: Animación de Controles Modulados**: Los Knobs muestran un anillo cian que se mueve en tiempo real reflejando la modulación.
- [x] **Tarea 23.2: Animación del Neural Pad**: Representación visual del movimiento del punto de morphing y "ghost cursor" para indicar el objetivo.
- [x] **Tarea 23.3: Visualización de Envolventes**: Gráfico ADSR interactivo que se ilumina con la intensidad de la envolvente de las voces activas.
- [x] **Tarea 23.4: Refinamiento "Knobs 2.0" [COMPLETADO]**: 
    -   Mejorada la estética de los anillos de modulación.
    -   Limitada la visualización a **2 decimales** en todos los controles para mayor legibilidad.
    -   Añadida profundidad visual y unificación de colores.

---

## 🟢 FASE 24: REDISEÑO DE INTERFAZ DE EFECTOS [COMPLETADO]
*Objetivo: Organizar y limpiar la pestaña FX para mejorar la usabilidad.*

- [x] **Tarea 24.1: Reorganización FX [COMPLETADO]**: 
    -   Rediseñado `FXPanel` agrupando controles en módulos visuales "GlassBox".
    -   Reorganización vertical de los selectores de Delay (Sync/Div) para maximizar el ancho de los desplegables.
    -   Aumento de tamaño del knob de `Drive` para coherencia visual con el resto de efectos.
- [x] **Tarea 24.2: Animación FX [COMPLETADO]**: Aplicado el sistema de feedback visual (anillos de modulación) a todos los efectos.
- [x] **Tarea 24.3: Indicadores de Actividad**: Añadidos LEDs virtuales que indican si un efecto está activo o procesando señal.

---

## 🟢 FASE 25: ENVOLVENTE DE FILTRO Y REFINAMIENTO BROWSER [COMPLETADO]
*Objetivo: Expandir las capacidades de síntesis y perfeccionar la estética del sistema de navegación.*

- [x] **Tarea 25.1: Envolvente de Filtro ADSR**:
    -   Implementación DSP en cada voz y lógica de seguimiento de picos (Peak Tracking) para la UI.
    -   Creación de la pestaña `FILTER/ENV` con visualizador dinámico.
- [x] **Tarea 25.2: Overhaul Estético del Browser**:
    -   Migración completa al esquema de colores NEURONiK (Glass Dark).
    -   Implementación de gradientes de cristalería en las selecciones de bancos y presets.
    -   Unificación de fuentes y bordes neon de baja intensidad.

---

## 🟢 FASE 13: INTERFAZ DE HARDWARE VIRTUAL (Pantalla LCD) [COMPLETADO]
*Objetivo: Reemplazar el título estático por una pantalla LCD informativa y funcional, clave para el modo headless.*

- [x] **Especificación de Hardware**: La pantalla LCD será de tipo 16x2 (16 caracteres, 2 filas) compatible con estándares industriales.
- [x] **Tarea 13.1: Emulación Matriz LCD**: Implementación del componente `LcdDisplay` con efectos de persistencia y scrolling.
- [x] **Tarea 13.2: LcdMenuManager**: State machine para navegación jerárquica (Idle, Menu, Edit).
- [x] **Tarea 13.3: Hardware D-Pad**: Reemplazo del encoder por botones físicos (`< > ^ v`) con lógica de "Turbo Acceleration".
- [x] **Tarea 13.4: CustomButton Component**: Unificación estética de botones bajo el esquema "Neural Glass".
- [x] **Tarea 13.5: Mover Título a Pestaña GENERAL**: Reubicación de branding para optimizar el área de cabecera.

### Referencia de Menús para Hardware (Estilo Vintage)
Esta sección recopila información sobre estructuras de menú y valores típicos de sintetizadores hardware para guiar el desarrollo de la interfaz LCD.

```
Estructura General
text
GLOBAL
├── TUNE > 440 | Master Tune (A4=440Hz ±100 cents) [web:6]
├── TRANS > 0 | Transpose (-12..+12 semitones) [web:6]
└── VOL > 100 | Master Volume (0-127) [web:9]

PRESET
├── BANK > A1 | Bank Select (A-H, 1-8) [web:6]
├── NAME > BASS | Preset Name (8-16 chars, ej. BASS, LEAD1) [web:6]
└── LEV > 100 | Preset Level (0-127) [web:9]
MIDI Configuración
text
MIDI
├── CH > 1 | Channel (1-16, OMNI) [web:9]
├── OUT > ON | Output (ON/OFF) [web:6]
├── THRU > ON | Thru (ON/OFF) [web:6]
├── PROG > ON | Program Change (ON/OFF) [web:9]
└── BEND > 2 | Bend Range (0-12 semitones) [web:6]
Efectos y Controles
text
EFFECT
├── PORT > 0 | Portamento Time (0-99) [web:6]
├── GLID > 12 | Glide Note (±12 semitones) [web:6]
├── CHOR > ON | Chorus (ON/OFF) [web:6]
├── MOD.W > 50 | Mod Wheel Depth (0-99) [web:6]
├── MOD.A > 50 | Mod Aftertouch Depth (0-99) [web:6]
└── AMP.A > 15 | Amp Aftertouch Range (0-15) [web:6]

INST
├── OCT > 0 | Octave Shift (-2..+2) [web:6]
├── DETU > 5 | Detune (0-50 cents) [web:6]
├── VEL > 7 | Velocity Sens (0-7, LINE1/LINE2) [web:6]
└── BEND > ±2 | Bender Range (±2, ±5, ±7, ±12) [web:18]
Envolventes (Scroll Páginas)
text
ENV.DCO1 (Pitch Env LINE1)
├── STEP1 > R99 L99 | Rate1/Level1 (0-99) [web:6]
├── SUS > 3 | Sustain Point (1-8) [web:6]
└── END > 0 | End Point (0-99) [web:6]

ENV.DCW1 (Timbre Env LINE1)
├── STEP1 > R50 L80 | Rate1/Level1 (0-99) [web:6]
└── KF > ON | Key Follow (ON/OFF) [web:6]
Valores típicos en LCD: Abviaturas como "TUNE=440", scroll con PAGE UP/DOWN para subscreens; números 0-99 comunes por resolución 7-bit.  //// Parámetros Globales
TUNE: Master tuning (A=440Hz ±50 cents)

TRANS: Transpose (-12 a +12 semitonos)

VOL: Master volume (0-127)

OCT: Octave shift (-2 a +2)

PORT: Portamento time (0-99 ms)

MIDI y Controles
CH: MIDI channel (1-16, OMNI)

OUT: MIDI out (ON/OFF)

THRU: MIDI thru (ON/OFF)

BEND: Pitch bend range (±2, ±5, ±7, ±12)

VEL: Velocity sensitivity (0-7, LINEAR)

Envolventes y Osciladores
ATT: Attack time (0-99)

DEC: Decay time (0-99)

SUS: Sustain level (0-99)

REL: Release time (0-99)

DETN: Detune (0-50 cents)

DCW: DCW level (0-99)

Efectos y Modulación
CHOR: Chorus depth (0-99, ON/OFF)

MOD: Modulation depth (0-99)

LFO.R: LFO rate (0-99)

LFO.S: LFO shape (TRI, SAW, SQR)

ENV: Envelope select (DCO1, DCW1, DCA)

Estos valores se mostraban en formato "PARAM=VAL" con scroll para subs, adaptados a límites de LCD vintage.  //// Los sintetizadores de los 80/90 expandían sus LCD con más parámetros detallados en subscreens, mostrando valores numéricos precisos y opciones abreviadas para envelopes, LFO y efectos. Aquí va una ampliación con más ejemplos reales de Casio CZ, DX7 y similares, incluyendo rangos completos.

Envolventes Detalladas
text
ENV.DCO (Pitch Envelope)
├── RATE1 > 0-99 | Attack rate
├── LEVEL1 > 0-99 | Peak level
├── RATE2 > 0-99 | Decay rate
├── RATE3 > 0-99 | Sustain rate
├── RATE4 > 0-99 | Release rate
└── KEYF > ON/OFF | Key follow

ENV.DCW (Wave Envelope)
├── STEP1 > R99 L99 | Rate/Level step 1 (8 steps)
├── SUSPT > 1-8 | Sustain step
└── KFS > -2..+2 | Key scale (-2 a +2)
LFO y Modulación
text
LFO
├── WAVE > TRI,SAW,SQR,RND | Waveform (Triangle, Saw, Square, Random)
├── RATE > 0-99 | Speed (0=slow, 99=fast)
├── DLY > 0-99 | Delay time
├── PMD > 0-99 | Pitch mod depth
├── AMD > 0-99 | Amp mod depth
└── SYNC > ON/OFF | Key sync
Valores típicos: PMD=30 para vibrato sutil; RATE=50 para tremolo medio.


Filtros y Amplificador
text
FILTER
├── FC > 0-99 | Cutoff frequency
├── RES > 0-99 | Resonance (self-osc ~99)
├── EG.INT > -50..+50 | Envelope intensity
└── KEYF > 0-2 | Keyboard track (0=off,1=normal,2=wide)

DCA (Amp)
├── LEVEL > 0-99
└── VEL > 0-7 (LINEAR1-2,FIXED)
Efectos Comunes
CHORUS: DEP=0-15, SPD=0-15, DL=0-15 (Depth, Speed, Delay)

REVERB: TIME=0-99, LEVEL=0-99 (raro en 80s portables)

NOISE: LEVEL=0-99, TYPE=WHIT/PINK

VELO CURVES: LINE1, LINE2, EXP1-3 (DX7 style)


Valores MIDI Expandidos
text
MIDI
├── LOCAL > ON/OFF | Local control
├── PROGCH > 1-16 | Program change ch
├── SYSX > ON/OFF | SysEx transmit
├── CLK > INT/EXT | Clock source (Internal/External)
└── PRIOR > HIGH/LOW | Priority (Poly/Mono)
Pantallas como "MIDI CH=10 LOC=ON" o scroll "PROG=ON THRU=OFF"   ////  Mixer y Niveles
text
MIXER
├── DCO1 > 0-99 | Osc 1 level
├── DCO2 > 0-99 | Osc 2 level  
├── NOIS > 0-99 | Noise level
├── EXT > 0-99 | External input
└── BAL > L50-R50 | Stereo balance
Key Scaling y Velocity
text
KEY SCALE
├── BREAK > C3 | Break point (C-1 a G9)
├── L.SLOPE > -10..+10 | Left slope
├── R.SLOPE > -10..+10 | Right slope
└── L.CURV > -7..+7 | Left curve

VELOCURV > LIN1,LIN2,EXP1-3 | Velocity curve [web:16]
Arpeggiator y Secuencer
text
ARPEGG
├── MODE > UP,DN,UPDN,RND | Up/Down/UpDown/Random
├── RANGE > 1-4 | Octave range
├── BPM > 30-300 | Tempo
└── HOLD > ON/OFF | Hold mode
Patch y Memoria
text
MEMORY
├── SAVE? > YES/NO | Save current patch
├── LOAD > A01 | Load preset (A01-H88)
├── NAME > LEAD1 | 8-char name (scroll)
└── COPY > 1->2 | Copy src->dst [web:6]
Global Avanzado
text
SYSTEM
├── TOUCH > LIGHT,NORM,HEAVY | Keyboard sensitivity
├── CURSOR > BLINK,OFF | Display cursor
├── CONTRAST > 0-15 | LCD contrast
├── DUMP > ALL,BANK | SysEx dump type
└── RESET > YES/NO | Factory reset [web:9]
Pantalla típica: "MIX DCO1=99 DCO2=50" o "ARPG UP RNG=3 BPM=120". Scroll con [PAGE] para ver pasos completos de envelopes (8 steps en CZ)  /// NO TODOS LOS PONDREMOS, PERO HAY ALGUNOS QUE HABRÁ QUE DESARROLLAR ALGO, TAMBIÉN FALTAN LOS CASOS COMO NUESTROS CONTROLES, LA ASIGNACION DE CC A CADA CONTROL,

los LCD mostraban valores de bancos vía MSB (CC#0, 0-127) y LSB (CC#32, 0-127) para seleccionar presets más allá de 128, combinados con Program Change (PC 0-127). Estos se visualizaban abreviados como "BANK MSB=00 LSB=01 PC=05" o "BNK=1 PRG=16" en scroll.

Estructura MIDI Bank Select
text
BANK SELECT
├── MSB > 00-127 | Most Significant Byte (CC#0, coarse bank)
├── LSB > 00-127 | Least Significant Byte (CC#32, fine bank)
---

## 🟡 FASE 26: AUDITORÍA DE ARQUITECTURA Y BUENAS PRÁCTICAS (MANTENIMIENTO)
*Objetivo: Mejorar la robustez técnica, el rendimiento y la portabilidad del motor NEURONiK.*

### 🛠️ Tarea 26.1: Acceso a Parámetros en el Hilo de Audio (DSP)
- [x] **Problema**: Zipper noise y overhead de atómicos en el bucle de proceso.
- [x] **Solución**: Implementar `juce::SmoothedValue<float>` para todos los parámetros del DSP.
- [x] **Acción**: Añadidas rampas de suavizado (20-50ms) en todos los efectos globales y parámetros de voz.

### 📜 Tarea 26.2: Gestión de Identificadores ("Magic Strings")
- [x] **Problema**: Uso de literales de cadena para IDs de parámetros es propenso a errores de compilación indetectables.
- [x] **Solución**: Centralizar IDs en un namespace `Params` con `static constexpr const char*`.
- [x] **Acción**: Refactorizados todos los IDs y APVTS a constantes estáticas de tiempo de compilación.

### 🏗️ Tarea 26.3: Portabilidad del Sistema CMake
- [x] **Problema**: Rutas hardcodeadas (e.g., `C:\JUCE`) rompen la compilación en otros sistemas/entornos.
- [x] **Solución**: Usar `find_package(JUCE CONFIG REQUIRED)` y permitir `JUCE_PATH` vía entorno/CLI.
- [x] **Acción**: Eliminadas las rutas absolutas de `CMakeLists.txt` y `manage.ps1`. Implementada detección dinámica vía `env:JUCE_PATH`.

### 📐 Tarea 26.4: UI Dinámica y Responsiva
- [ ] **Problema**: Layouts de tamaño fijo dificultan el redimensionado y portabilidad de la UI.
- [ ] **Solución**: Migrar a `juce::FlexBox`, `juce::Grid` o división proporcional con `Rectangle`.
- [ ] **Acción**: Refactorizar métodos `resized()` para eliminar coordenadas "mágicas".

### 🐧 Tarea 26.5: Scripts Multiplataforma
- [ ] **Problema**: Flujo de compilación limitado a Windows por `manage.ps1`.
- [ ] **Solución**: Crear `manage.sh` (Bash) equivalente para macOS/Linux.
- [ ] **Acción**: Asegurar paridad de funcionalidades en los scripts de automatización.