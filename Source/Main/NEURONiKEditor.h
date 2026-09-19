#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>

#include "NEURONiKProcessor.h"

#if defined(NEURONIK_HAS_WEBUI_VIEW)
 #include "WebUI/BridgeSelftest.h"
 #include "WebUI/NeuronikWebView.h"
#endif

/**
 * @class NEURONiKEditor
 * @brief El editor del plugin: la pagina de la WebUI sobre WebView2.
 *
 * Fase 8, ticket 8.1. La interfaz de NEURONiK es la web (`WebUI/dist`), servida
 * por `NEURONiK::WebUI::NeuronikWebView`, y el panel nativo de JUCE ya no se
 * monta. La retirada fisica de `Source/UI/**` es el ticket 8.4; aqui se deja de
 * usarlo, que es lo que hace que la pagina sea la interfaz de verdad.
 *
 * Lo que queda en C++ es lo que la pagina todavia no cubre y que el ticket 8.3
 * se lleva: la barra de menu (cargar/guardar preset, canal MIDI, voces, zoom,
 * opciones de audio del Standalone, ayuda MIDI). Es deliberadamente minima y
 * provisional, no una segunda interfaz.
 *
 * El AUDIO ES NATIVO dentro del plugin: el editor no crea AudioContext ni
 * AudioWorklet. La pagina lo detecta por `window.__JUCE__` y su politica de audio
 * (`WebUI/src/audio/policy.js`) devuelve `blocked`, con test que lo vigila.
 *
 * `NEURONIK_HAS_WEBUI_VIEW` lo define el CMake de los targets que montan la
 * pagina (Standalone y VST3). El target de tests que COPIA las fuentes del plugin
 * no lo define: alli este editor compila la variante sin interfaz, que existe
 * para que copiar `NEURONIK_SOURCES` no obligue a embeber un megabyte de UI en un
 * ejecutable de pruebas.
 *
 * SELFTEST DEL PUENTE (ticket 8.1 paso 2c). El editor es tambien quien corre las
 * cinco direcciones del puente, porque es la unica superficie que hospeda la
 * pagina en los dos formatos. Es, ademas, la superficie DONDE SE ENVIAN: su pagina
 * publica la ficha MODELOS A-D, asi que no se acoge a ninguna direccion declarada
 * no aplicable (eso esta reservado a la bancada del piloto, que sirve la pagina
 * retirada). Se pide por arranque, nunca por sorpresa:
 *
 *   - `NEURONiK.exe --selftest` (Standalone) — es un proceso, asi que el veredicto
 *     es su codigo de salida: 0 = las cinco direcciones se movieron, 1 = alguna no.
 *   - `NEURONIK_SELFTEST=1` (cualquiera, y el UNICO disparo del VST3, que no recibe
 *     argv porque lo lanza el DAW) — con esa variable el arnes corre dentro de
 *     pluginval o del DAW que abra el editor, que es como se comprueba el VST3 real.
 *
 * El veredicto sale por stdout y a un log (`NEURONIK_SELFTEST_LOG` fija la ruta;
 * por defecto, datos de usuario del sistema, porque el VST3 no puede escribir
 * junto a su propio binario).
 */
class NEURONiKEditor : public juce::AudioProcessorEditor,
                       public juce::MenuBarModel,
                       private juce::Timer
{
public:
    explicit NEURONiKEditor (NEURONiKProcessor&);
    ~NEURONiKEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // --- MenuBarModel (provisional: 8.3 lo lleva a la pagina) ---
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

    void showMidiSpecifications();

    /**
     * @brief Zoom de la interfaz.
     * @details El editor nativo escalaba knobs de JUCE con `setTransform()`; esos
     *          knobs ya no existen. Aqui el zoom es del contenido de la pagina
     *          (zoom CSS del documento), asi que no depende del tamano que le de
     *          el host al editor en cada DAW.
     */
    void setZoom (float scale);

private:
    /**
     * @brief Tamano base del editor.
     * @details El ancho NO es arbitrario: es el lienzo de diseno de la WebUI
     *          (`WebUI/src/contracts/sections.js`, CANVAS = 1440x900), donde los 70
     *          controles caben en un solo lienzo repartidos en tres bandas de
     *          fichas (ticket 8.2). A `menuBarHeight` se le suma el alto del
     *          lienzo para que el navegador reciba EXACTAMENTE esos 1440x900 y la
     *          pagina no tenga que desplazarse a tamano base.
     *
     *          OJO: el ancho del lienzo esta en tres sitios que tienen que decir lo
     *          mismo — esta constante, `CANVAS` en sections.js y `--abd-canvas-w`
     *          en WebUI/src/styles/main.css. Los dos del lado de la pagina se
     *          comprueban entre si en `WebUI/tests/sections.test.js`.
     */
    static constexpr int canvasWidth = 1440;
    static constexpr int canvasHeight = 900;
    static constexpr int menuBarHeight = 25;
    static constexpr int baseWidth = canvasWidth;
    static constexpr int baseHeight = canvasHeight + menuBarHeight;

    NEURONiKProcessor& processor;
    float zoomScale = 1.0f;

    juce::MenuBarComponent menuBar { this };
    std::unique_ptr<juce::FileChooser> chooser;   // los diálogos del menú provisional

#if defined(NEURONIK_HAS_WEBUI_VIEW)
    std::unique_ptr<NEURONiK::WebUI::NeuronikWebView> webView;

    /**
     * @brief El selftest de cinco direcciones, o null si el arranque no lo pidio.
     * @details El arnes vive aqui y no dentro del componente web porque es quien
     *          decide el veredicto (codigo de salida en el Standalone, log en el
     *          VST3), y eso es cosa del editor. Se declara DESPUES de `webView`
     *          para morir ANTES que el: el arnes pide scripts al navegador y no
     *          puede quedar vivo cuando el navegador ya no esta.
     */
    std::unique_ptr<NEURONiK::WebUI::BridgeSelftest> selftest;

    void startSelftestIfRequested();
    void selftestFinished (bool passed);
#else
    juce::Label noWebUiNotice;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NEURONiKEditor)
};
