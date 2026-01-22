# NEXUS: Master Development Roadmap & Specification

Este documento sirve como la guía técnica y filosófica para el desarrollo de NEXUS, un instrumento de resíntesis inspirado en la tecnología de modelos neurales (Hartmann Neuron), diseñado para alta eficiencia en múltiples plataformas (PC, Mac, Linux, RPi).

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

## 🟣 FASE 9: OPTIMIZACIÓN Y DESPLIEGUE (APLAZADO)
*Objetivo: Rendimiento garantizado en Raspberry Pi y estabilidad en DAWs comerciales.*

- [ ] **Tarea 9.1: SIMD Optimization (Neon/SSE)**: Refactorizar `Resonator::processSample` para usar instrucciones vectoriales.
- [ ] **Tarea 9.2: Escalabilidad Dinámica**: Implementar modos de calidad (Ultra/Eco) para adaptarse a diferentes hardwares.

---

## 🔵 FASE 11: NEXUS MODEL MAKER (Herramienta de Creación de Sonidos)
*Objetivo: Crear una aplicación de escritorio para analizar archivos de audio y generar los archivos `.nexusmodel`.*

- [ ] **Tareas 11.1 a 11.7**: Desarrollo de una aplicación JUCE independiente con UI, motor de análisis FFT, y capacidad de exportar modelos. Incluye un plan de evolución con algoritmos avanzados (LPC, CQT) y la capacidad de importar modelos del Hartmann Neuron.

---

## 🟡 FASE 12: GESTIÓN Y CONTROL TOTAL
*Objetivo: Dotar a NEXUS de un sistema de gestión de sonidos profesional y control MIDI completo.*

### Tarea 12.1: Sistema de Presets (.nexuspreset)
-   **Especificación**: Implementar la capacidad de guardar y cargar el estado completo del sintetizador.
-   **Tecnología**: Aprovechar `juce::AudioProcessorValueTreeState` para serializar el estado a XML y guardarlo en un archivo.
-   **UI**: Añadir botones de "Save" y "Load" en la interfaz, junto con un display para el nombre del preset actual.

### Tarea 12.2: Carga de Modelos Espectrales (.nexusmodel)
-   **Especificación**: Permitir al usuario cargar los archivos `.nexusmodel` en cada uno de los cuatro slots (A, B, C, D) del `Resonator`.
-   **UI**: Añadir cuatro botones ("Load A", "Load B", etc.) en el `OscillatorPanel`, cerca del `XYPad`.
-   **Lógica**: Implementar el parseo del archivo JSON `.nexusmodel` y cargar los datos en el `Resonator`.

### Tarea 12.3: Mapeo de MIDI CC y Controles Estándar
-   **Especificación**: Dotar a NEXUS de un control MIDI completo, combinando un sistema de mapeo dinámico ("MIDI Learn") con soporte para los controles estándar más importantes.
-   **Nota General**: Toda la funcionalidad MIDI deberá ser documentada en el `README.md`.

#### Sub-tarea 12.3.1: Soporte para Controles MIDI Estándar (Hard-coded)
-   **Objetivo**: Implementar el manejo de los mensajes MIDI más comunes para una experiencia "plug-and-play".
-   **Controles a Implementar**:
    -   **Pitch Bend**: Conectar el mensaje de Pitch Wheel para modular la frecuencia base de las voces activas.
    -   **Sustain Pedal (CC #64)**: Implementar la lógica de "hold" para las notas, manteniendo las voces activas incluso después de soltar las teclas.
    -   **Modulation Wheel (CC #1)**: Mapear por defecto a un parámetro expresivo, como `morphY` o la intensidad de un futuro LFO.
    -   **Main Volume (CC #7)**: Conectar directamente al parámetro `masterLevel`.
    -   **Program Change**: Implementar la lógica para cambiar entre presets del sintetizador.

#### Sub-tarea 12.3.2: Sistema de Mapeo Dinámico ("MIDI Learn")
-   **Objetivo**: Permitir al usuario asignar cualquier control MIDI CC a cualquier parámetro del sintetizador.
-   **UI**: Implementar un menú contextual (clic derecho) en cada control (sliders, knobs, XYPad) con la opción "MIDI Learn".
-   **Lógica**:
    1.  Al activar "MIDI Learn", el sistema esperará el siguiente mensaje MIDI CC.
    2.  Al recibirlo, creará una asociación entre ese número de CC y el ID del parámetro.
    3.  Este mapa de asociaciones se guardará como parte del preset (`.nexuspreset`).
    4.  El `processBlock` procesará los mensajes MIDI CC entrantes y actualizará los parámetros correspondientes según el mapa activo.

---

## 🟤 FASE 13: INTERFAZ DE HARDWARE VIRTUAL (Pantalla LCD)
*Objetivo: Reemplazar el título estático por una pantalla LCD informativa como puente para una futura integración con hardware físico.*

### Tarea 13.1: Diseño del Componente LCD (`LcdDisplay`)
-   **Especificación**: Crear un componente JUCE que simule una pantalla LCD de caracteres, con texto pixelado y efecto de "glow".
-   **Funcionalidad**: Deberá ser capaz de mostrar varias líneas de texto con diferentes alineaciones.

### Tarea 13.2: Lógica de Información Contextual
-   **Especificación**: La pantalla mostrará información relevante basada en la interacción del usuario.
-   **Lógica**: Mostrar el nombre y el valor del último parámetro modificado. Cuando no se interactúa, mostrará el nombre del preset actual.

### Tarea 13.3: Integración en la Interfaz Principal
-   **Especificación**: Reemplazar la `juce::Label` del título en `NEXUSEditor` por el nuevo componente `LcdDisplay`.

### Tarea 13.4: Abstracción para Hardware Físico
-   **Especificación**: Diseñar la comunicación con el `LcdDisplay` de forma modular.
-   **Sugerencia**: Crear una clase `DisplayController` que gestione qué información mostrar. El `LcdDisplay` (y en el futuro, un `SerialLcdController` para RPi) se suscribirán a los cambios de este controlador.

---

## ⚪ TAREAS PENDIENTES DE AUDITORÍA Y CALIDAD
- [x] **(BUG)** El teclado MIDI siempre reproduce la misma nota, ignorando el pitch. - **SOLUCIONADO**
- [x] **(UI)** Eliminar el visualizador duplicado o no funcional. - **SOLUCIONADO**
- [ ] Implementar Unit Tests para los algoritmos de interpolación.
- [x] Documentar cada módulo siguiendo el formato de `reglas.md`.
- [ ] Validar consumo de CPU en RPi 4/5.
