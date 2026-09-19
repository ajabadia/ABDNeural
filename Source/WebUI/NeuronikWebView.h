/**
 * @file NeuronikWebView.h
 * @brief La WebUI del plugin dentro de WebView2 (Fase 8, ticket 8.1).
 *
 * Es el componente que hospeda la pagina (`WebUI/dist`) en el editor del plugin.
 * Todo lo que no sea "esta pagina" se delega:
 *
 *   - El contenedor (backend webview2, native integration, tema + hook
 *     `pageLoaded`, y el `resized()` que ajusta el navegador a los bounds) es la
 *     base COMPARTIDA `abd::webview2::JuceWebView2Component`, la misma que usan
 *     el teclado, HardwareMidiDetect, StudioTopology y ABDScope.
 *   - El pipeline de recursos (normalizar URL, MIME, catalogo embebido) es el
 *     proveedor COMPARTIDO `abd::webview2::webView2ResourceProvider`. Este
 *     fichero solo lo alimenta con el catalogo de NEURONiK; no reimplementa
 *     nada, que es justo lo que el inventario de homonimias de ABDSharedCode
 *     pide (un `PluginEditor_ResourceProvider` por plugin es deuda, no diseno).
 *   - El protocolo del puente y su backend son `ParameterBridge` + los tres
 *     adaptadores de `BridgeAdapters.h` (los mismos que usa la bancada del
 *     piloto, que es de donde salieron).
 *
 * Lo unico propio son tres cosas que solo el plugin puede saber: de donde salen
 * los bytes de la pagina (embebidos), que adaptadores van enchufados al puente, y
 * el zoom.
 */

#pragma once

#include <WebView2Bridge/JuceWebView2Component.h>
#include <WebView2Bridge/WebView2ResourceProvider.h>

#include "BridgeAdapters.h"
#include "ParameterBridge.h"

// Generado por juce_add_binary_data (target NEURONiK_WebUIAssets): la copia
// EMBEBIDA de WebUI/dist que viaja dentro del binario.
#include "NeuronikWebUIAssets.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace NEURONiK::WebUI
{

/**
 * @class NeuronikWebView
 * @brief La pagina del plugin, con su puente ya conectado.
 *
 * Ciclo de vida: la base construye el navegador y navega a la raiz del proveedor;
 * cuando la pagina avisa (`pageLoaded`) se le manda el estado completo. El editor
 * solo tiene que llamar a `poll()` desde su timer.
 *
 * Dentro del plugin el AUDIO ES NATIVO: este componente no toca el AudioContext
 * ni lo necesita (el procesador ya corre en el host). La pagina lo sabe por
 * `window.__JUCE__` y su politica de audio devuelve `blocked` ahi, asi que el
 * AudioWorklet no arranca nunca dentro del plugin. Ver `WebUI/src/audio/policy.js`.
 */
class NeuronikWebView final : public abd::webview2::JuceWebView2Component
{
public:
    explicit NeuronikWebView (NEURONiKProcessor& processorToWrap,
                              const juce::String& initialTheme = "audiolab")
        // El listener del puente va en la lista de la base porque el navegador se
        // construye con sus opciones; el lambda captura `this` y desreferencia
        // `bridge` en el momento del evento (nunca durante la construccion, que es
        // lo que permite declararlo despues).
        // OJO: no pasar `pageLoaded` aqui. La base ya registra ese id, y un
        // segundo `withEventListener` con el mismo id lo REEMPLAZA en JUCE, lo que
        // se comeria la reaplicacion del tema. El trabajo propio de pageLoaded va
        // en `onPageLoaded()`.
        : abd::webview2::JuceWebView2Component (
              resourceProvider,
              { { BridgeEventIds::jsToNative,
                  [this] (const juce::var& message)
                  {
                      if (bridge != nullptr)
                          bridge->handleJsEvent (message);
                  } } },
              initialTheme),
          processor (processorToWrap)
    {
        bridge = std::make_unique<ParameterBridge> (processor.getAPVTS());

        // El transporte: el navegador de la base. `emitEventIfBrowserIsVisible`
        // es la unica mitad que el guard de direccion del ecosistema acepta.
        bridge->setSender ([this] (const juce::var& message)
        {
            auto& browser = getWebBrowser();

            if (browser.isVisible())
                browser.emitEventIfBrowserIsVisible (BridgeEventIds::nativeToJs, message);
        });

        // Los cuatro adaptadores: el puente manda el cable, el procesador la
        // conducta. El procesador sobrevive al puente, asi que el puntero pelado
        // es seguro (mismas reglas que en la bancada del piloto).
        presetAdapter = std::make_unique<PresetManagerAdapter> (processor);
        bridge->setPresetController (presetAdapter.get());

        midiAdapter = std::make_unique<MidiInjectionAdapter> (processor);
        bridge->setMidiController (midiAdapter.get());

        // El adaptador de modelos tambien CARGA (los slots A..D de la pagina: los
        // loadA..loadD del panel nativo). El dialogo es asincrono, asi que la
        // respuesta sale de sus dos callbacks y no del acto de pedirla: el cable lo
        // sigue teniendo el puente. Orden de miembros: los adaptadores se declaran
        // DESPUES del puente, asi que mueren ANTES que el, y el FileChooser suelta
        // su callback pendiente al destruirse — una respuesta tardia no encuentra
        // ni un puente muerto ni un navegador a medio apagar.
        modelsAdapter = std::make_unique<EngineModelsAdapter> (
            processor,
            [this] (int) { bridge->sendModelsState(); },
            [this] (int slot, const juce::String& detail) { bridge->sendModelError (slot, detail); });
        bridge->setModelController (modelsAdapter.get());

        // El RANDOMIZE dejo de ser un boton del panel nativo (se retira): la pagina
        // pide la accion y el procesador sortea, con los mismos congelados.
        randomizeAdapter = std::make_unique<RandomizerAdapter> (processor);
        bridge->setRandomizeController (randomizeAdapter.get());
    }

    ~NeuronikWebView() override
    {
        // El navegador (miembro de la base) muere DESPUES que este destructor, asi
        // que se corta el transporte primero: una respuesta tardia no encuentra
        // un puente a medio destruir.
        if (bridge != nullptr)
            bridge->setSender ({});
    }

    /**
     * @brief Publica lo que cambio en el procesador hacia la pagina.
     * @details Lo llama el timer del editor. Son dos cosas de periodicidad
     *          distinta: los cambios de parametro en cada tick, y el estado MIDI
     *          externo (notas retenidas + ruedas) cada ~180 ms, para que el
     *          teclado de la pagina refleje el MIDI del DAW. Sin este poll, editar
     *          un parametro desde el host no llegaria a la pagina.
     */
    void poll()
    {
        if (bridge == nullptr)
            return;

        bridge->publishPendingChanges();

        if (++midiStateTick >= midiStateTicks)
        {
            midiStateTick = 0;

            bridge->sendMidiNoteState (processor.getHeldNotes(),
                                       processor.externalPitchBend.load(),
                                       processor.externalModWheel.load());
        }
    }

    /**
     * @brief Escala visualmente la pagina (zoom del WebBrowserComponent).
     * @details Se aplica como zoom CSS del documento, no como transform del
     *          componente: asi el zoom no depende del tamano que le de el host y
     *          el contenido escala en vez de reflowear. Es lo que sustituye al
     *          `setTransform()` del editor nativo, que escalaba knobs de JUCE que
     *          ya no existen.
     */
    void setPageZoom (float scale)
    {
        pageZoom = juce::jlimit (0.5f, 3.0f, scale);
        applyPageZoom();
    }

    [[nodiscard]] float getPageZoom() const noexcept { return pageZoom; }

    /** @brief El puente, para diagnostico y para el selftest del editor. */
    [[nodiscard]] ParameterBridge* getBridge() noexcept { return bridge.get(); }

    /**
     * @brief Corre un script en la pagina y devuelve su resultado como texto.
     * @details Es el camino que usa el selftest del puente (ticket 8.1 paso 2c):
     *          el MISMO `evaluateJavascript` del navegador, con el resultado ya
     *          aplanado a texto (`NO_RESULT` explicito en vez de puntero nulo),
     *          para que el arnes compartido no sepa nada de JUCE ni del backend.
     *
     *          Se llama directo (sin `callAsync`, a diferencia del zoom): quien lo
     *          pide es siempre un timer, nunca el callback de carga del navegador,
     *          asi que no hay reentrada que evitar — y una `callAsync` extra solo
     *          anadiria una ventana en la que el componente ya no esta.
     */
    void evaluate (const juce::String& script, std::function<void (const juce::String&)> onResult)
    {
        getWebBrowser().evaluateJavascript (
            script,
            [onResult] (juce::WebBrowserComponent::EvaluationResult result)
            {
                if (onResult == nullptr)
                    return;

                onResult (result.getResult() != nullptr ? result.getResult()->toString()
                                                        : juce::String ("NO_RESULT"));
            });
    }

    /**
     * @brief La pagina ya aviso (`pageLoaded`).
     * @details Es la senal de "listo" que el selftest espera antes de empujar:
     *          antes de este momento no hay documento al que preguntar nada.
     */
    [[nodiscard]] bool isPageLoaded() const noexcept { return pageLoaded; }

protected:
    /**
     * @brief `pageLoaded`: tema (base) + cerrar gestos + estado completo.
     * @details Una recarga deja gestos abiertos a medias; cerrarlos aqui evita
     *          que un host lo grabe como un toque de automatizacion pegado.
     */
    void onPageLoaded() override
    {
        abd::webview2::JuceWebView2Component::onPageLoaded();   // reaplica el tema

        pageLoaded = true;

        applyPageZoom();

        if (bridge != nullptr)
        {
            bridge->closeOpenGestures();
            bridge->sendFullSnapshot();
        }
    }

private:
    static constexpr int midiStateTicks = 6;   // 6 x 30 ms = ~180 ms

    /**
     * @brief El catalogo del `juce_add_binary_data` de esta pagina.
     * @details El proveedor compartido espera exactamente los cuatro simbolos que
     *          JUCE genera; aqui solo se los damos con nuestro namespace.
     */
    static const abd::webview2::BinaryAssetsCatalog& assetsCatalog()
    {
        static const abd::webview2::BinaryAssetsCatalog catalog {
            NeuronikWebUIAssets::namedResourceListSize,
            NeuronikWebUIAssets::namedResourceList,
            NeuronikWebUIAssets::originalFilenames,
            NeuronikWebUIAssets::getNamedResource
        };

        return catalog;
    }

    /**
     * @brief Override de DESARROLLO: servir la pagina desde disco.
     * @details Con `NEURONIK_WEBUI_DEV_DIR` apuntando a `WebUI/dist`, iterar la UI
     *          es `pnpm build` (segundos) y recargar, en vez de recompilar el
     *          plugin entero. Es la unica funcion de la bancada del piloto que el
     *          plugin no tenia, y por eso se trae aqui antes de retirar la bancada.
     *
     *          Por defecto la variable NO esta puesta y esto es un no-op: el
     *          binario sirve su copia embebida y no toca el disco. Ninguna ruta
     *          queda grabada en el plugin, asi que el DoD de "cero rutas
     *          absolutas" sigue cumpliendose: es una decision de arranque, no una
     *          dependencia del build.
     *
     *          Se reutiliza la normalizacion de URL y el MIME del modulo
     *          compartido en vez de duplicarlos.
     */
    static std::optional<juce::WebBrowserComponent::Resource> resolveDevOverride (const juce::String& url)
    {
        const auto devDir = juce::SystemStats::getEnvironmentVariable ("NEURONIK_WEBUI_DEV_DIR", {});

        if (devDir.isEmpty())
            return std::nullopt;

        const juce::File root (devDir);

        if (! root.isDirectory())
            return std::nullopt;

        const auto decodedPath = abd::webview2::normalizeResourcePath (url);

        // JUCE sirve su propio script (instala window.__JUCE__, que es media
        // conversacion): nunca se tapa, ni desde disco ni desde el catalogo.
        if (decodedPath == "juce.js" || decodedPath.endsWith ("/juce.js"))
            return std::nullopt;

        const auto file = root.getChildFile (decodedPath.replace ("/", juce::File::getSeparatorString()));

        if (! file.existsAsFile())
            return std::nullopt;   // lo que no este en disco cae al embebido

        juce::MemoryBlock data;

        if (! file.loadFileAsData (data))
            return std::nullopt;

        std::vector<std::byte> bytes (static_cast<size_t> (data.getSize()));

        if (data.getSize() > 0)
            std::memcpy (bytes.data(), data.getData(), static_cast<size_t> (data.getSize()));

        return juce::WebBrowserComponent::Resource {
            std::move (bytes),
            abd::webview2::getMimeTypeForFilename (file.getFileName()).toStdString()
        };
    }

    /** @brief Embebido primero (el VST3 no tiene cwd); disco solo si es dev. */
    static std::optional<juce::WebBrowserComponent::Resource> resourceProvider (const juce::String& url)
    {
        if (auto fromDisk = resolveDevOverride (url))
            return fromDisk;

        // Sin prefijos de ABDSharedAssets a proposito: esta pagina IMPORTA los
        // estilos compartidos (`@abdsynths/shared/styles/...`) y Vite los empaqueta
        // en su bundle, asi que en tiempo de ejecucion no necesita nada del disco.
        // El plugin no depende del sistema de ficheros para pintar su interfaz.
        return abd::webview2::webView2ResourceProvider (url, assetsCatalog());
    }

    void applyPageZoom()
    {
        // Miliseimas enteras en vez de formatear el float: el separador decimal de
        // `juce::String(float, int)` depende del locale y un "1,25" rompe el JS.
        const auto milli = juce::jlimit (500, 3000, juce::roundToInt (pageZoom * 1000.0f));

        const juce::String zoomLiteral = juce::String (milli / 1000) + "." + juce::String (milli % 1000);

        const juce::String js = "document.documentElement.style.zoom = '" + zoomLiteral + "';";

        // callAsync: el zoom puede pedirse desde el menu (que ya corre en el
        // message thread) y desde pageLoaded, y evaluateJavascript no debe
        // reentrar en el callback de carga.
        juce::MessageManager::callAsync ([this, js] { getWebBrowser().evaluateJavascript (js); });
    }

    NEURONiKProcessor& processor;

    std::unique_ptr<ParameterBridge> bridge;
    std::unique_ptr<PresetManagerAdapter> presetAdapter;
    std::unique_ptr<MidiInjectionAdapter> midiAdapter;
    std::unique_ptr<EngineModelsAdapter> modelsAdapter;
    std::unique_ptr<RandomizerAdapter> randomizeAdapter;

    float pageZoom = 1.0f;
    int midiStateTick = 0;
    bool pageLoaded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeuronikWebView)
};

} // namespace NEURONiK::WebUI
