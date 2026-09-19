#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>

#include "NEURONiKProcessor.h"

#if defined(NEURONIK_HAS_WEBUI_VIEW)
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
    static constexpr int baseWidth = 1100;
    static constexpr int baseHeight = 720;
    static constexpr int menuBarHeight = 25;

    NEURONiKProcessor& processor;
    float zoomScale = 1.0f;

    juce::MenuBarComponent menuBar { this };
    std::unique_ptr<juce::FileChooser> chooser;   // los diálogos del menú provisional

#if defined(NEURONIK_HAS_WEBUI_VIEW)
    std::unique_ptr<NEURONiK::WebUI::NeuronikWebView> webView;
#else
    juce::Label noWebUiNotice;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NEURONiKEditor)
};
