# Arquitectura y Plan de Desarrollo: NEXUS MODEL MAKER

Este documento detalla la arquitectura propuesta y los pasos de desarrollo para la herramienta `NEXUS MODEL MAKER`, la aplicación de escritorio encargada de generar los archivos `.nexusmodel` a partir de audio.

---

## 1. Filosofía de Diseño

La decisión principal es desarrollar el `NEXUS MODEL MAKER` como un **proyecto completamente independiente** del plugin `NEXUS_Synth`. Esto aporta beneficios clave:

-   **Separación de Responsabilidades**: El plugin se mantiene ligero, enfocado exclusivamente en la síntesis en tiempo real. La herramienta de análisis, que es computacionalmente más intensiva, no añade peso ni complejidad al sintetizador.
-   **Mantenibilidad**: Aislar la lógica de análisis de la de síntesis hace que ambos proyectos sean más fáciles de depurar, mantener y evolucionar de forma independiente.
-   **Flexibilidad de Despliegue**: Podemos distribuir el sintetizador y la herramienta de creación de modelos como dos aplicaciones separadas.

## 2. Estructura de Proyectos

Para lograr una arquitectura limpia y evitar la duplicación de código, el repositorio se organizará en tres componentes principales:

### a) `NEXUS_Synth` (El Plugin de Audio)

-   **Tipo**: Plugin de Audio (VST3, AU, etc.).
-   **Responsabilidad**: Síntesis en tiempo real, gestión de presets, UI del sintetizador.
-   **Dependencia Clave**: Enlazará con `Nexus_Common` para entender la estructura de los archivos `.nexusmodel`.

### b) `NEXUS_ModelMaker` (La Herramienta de Creación)

-   **Tipo**: Aplicación de Escritorio con GUI (Standalone).
-   **Responsabilidad**: Cargar archivos de audio, ejecutar análisis FFT, y exportar los resultados como archivos `.nexusmodel` en formato JSON.
-   **Dependencia Clave**: Enlazará con `Nexus_Common` para usar la misma definición de `SpectralModel` al generar los archivos.

### c) `Nexus_Common` (Librería Compartida)

-   **Tipo**: Librería Estática (`.lib`/`.a`).
-   **Responsabilidad**: Contener todo el código y las definiciones de datos que deben ser idénticos en ambos proyectos.
-   **Contenido Principal**:
    -   `SpectralModel.h`: La definición de la `struct SpectralModel`. Si esta estructura cambia en el futuro (ej., añadiendo un campo `initialPhase`), solo se necesita modificar este fichero para que ambos proyectos se mantengan sincronizados.
    -   Posibles utilidades futuras: Funciones de parseo JSON, constantes compartidas, etc.

---

## 3. Plan de Desarrollo Detallado

### Fase A: Refactorización del Proyecto Principal

*Objetivo: Preparar la arquitectura actual para la coexistencia con el nuevo proyecto.*

1.  **Crear la estructura de directorios**:
    -   `D:/desarrollos/ABDNeural/Source/Common/`
2.  **Mover `SpectralModel`**:
    -   Crear `Source/Common/SpectralModel.h`.
    -   Cortar la `struct SpectralModel` del fichero `Source/DSP/CoreModules/Resonator.h` y pegarla en el nuevo `Source/Common/SpectralModel.h`.
3.  **Actualizar CMake**:
    -   Modificar el `CMakeLists.txt` principal para definir `Nexus_Common` como una librería estática (`add_library(Nexus_Common STATIC ...)`), incluyendo el fichero `SpectralModel.h`.
    -   Modificar la definición del `NEXUS_Synth` en CMake para que enlace con `Nexus_Common` (`target_link_libraries(NEXUS_Synth PRIVATE Nexus_Common)`).
    -   Asegurarse de que las rutas de inclusión son correctas.
4.  **Verificación**: Compilar y probar el `NEXUS_Synth` para asegurar que todo sigue funcionando perfectamente tras la refactorización.

### Fase B: Creación y Desarrollo de `NEXUS_ModelMaker`

*Objetivo: Implementar la funcionalidad completa de la herramienta de análisis.*

1.  **Creación del Proyecto**:
    -   Añadir un nuevo ejecutable en el `CMakeLists.txt` principal: `add_executable(NEXUS_ModelMaker ...)`
    -   Configurarlo como una aplicación JUCE de escritorio.
    -   Enlazarlo con `Nexus_Common` (`target_link_libraries(NEXUS_ModelMaker PRIVATE Nexus_Common)`).
2.  **Diseño de la Interfaz de Usuario (UI)**:
    -   Crear una clase `MainComponent` que contendrá la UI.
    -   Añadir los siguientes componentes JUCE:
        -   `juce::TextButton` para "Cargar Audio...".
        -   `juce::TextButton` para "Analizar".
        -   `juce::TextButton` para "Guardar Modelo...".
        -   Un componente personalizado para visualizar la forma de onda del audio cargado (usando `juce::AudioVisualiserComponent` como base).
3.  **Lógica de Carga y Análisis**:
    -   Implementar la lógica del botón "Cargar Audio..." para que use `juce::FileChooser` y cargue un archivo de audio en un `juce::AudioBuffer`.
    -   Implementar la lógica del botón "Analizar":
        -   Pedir al usuario (o usar una selección en la forma de onda) la región del audio a analizar.
        -   Crear una instancia de `juce::dsp::FFT`.
        -   Realizar la FFT sobre la ventana de datos seleccionada.
        -   Procesar el resultado de la FFT para identificar las frecuencias y amplitudes de los picos (armónicos).
        -   Rellenar una instancia de `SpectralModel` con las amplitudes de los primeros 64 armónicos encontrados.
4.  **Lógica de Exportación**:
    -   Implementar la lógica del botón "Guardar Modelo...":
        -   Crear un objeto `juce::var` de tipo `juce::DynamicObject`.
        -   Poblar este objeto con los datos del `SpectralModel`.
        -   Convertir el `juce::var` a una `juce::String` JSON usando `juce::JSON::toString()`.
        -   Usar `juce::FileChooser` para pedir al usuario una ubicación y guardar la `juce::String` en un archivo con la extensión `.nexusmodel`.

---

## 4. Conclusión

Este enfoque modular no solo es robusto, sino que también establece una base sólida para futuras expansiones. Si el `Model Maker` se vuelve más complejo, sus cambios nunca pondrán en riesgo la estabilidad del sintetizador principal.
