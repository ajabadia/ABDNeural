/**
 * @file WebPilotHost.cpp
 * @brief Bancada WebView2 de la WebUI del plugin: metricas de arranque MAS el puente
 *        de parametros real.
 * @details Sirve `WebUI/dist` —la pagina que el plugin embebe y envia— por un
 *          WebBrowserComponent resource provider, y espeja un APVTS real en los dos
 *          sentidos del canal WebView2:
 *
 *            - the APVTS is `State::createLayoutApvts()`, the exact parameter layout
 *              the plugin ships, so what the page shows cannot drift from the plugin;
 *            - outgoing transport is `ParameterBridge::Sender`, whose lambda calls
 *              `emitEventIfBrowserIsVisible("event", ...)` — the ONLY native -> JS
 *              direction that fires `window.__JUCE__.backend.addEventListener`
 *              (see ABDMS2000 PluginEditor.cpp:314 and the shared direction guard);
 *            - incoming events arrive on the `nativeEvent` listener, in the JS ->
 *              native direction, and are handed to `ParameterBridge::handleJsEvent`;
 *            - outgoing changes are polled (publishPendingChanges) on a 30 ms timer
 *              instead of parameter listeners, per the bridge design;
 *            - a native comparison strip renders the same parameters with real JUCE
 *              attachments, so a screen shot shows both sides moving together.
 *
 *          Measurements are appended to `pilot-startup.log` next to the executable
 *          and summarised in the window title. Pass `--auto-quit` to close the host
 *          automatically once the panel reports ready.
 *
 *          POR QUE SIGUE EXISTIENDO, ya retirado el piloto (ticket 8.4). La pagina no
 *          es suya (la sirve la WebUI del plugin, desde disco), pero esta bancada es
 *          la UNICA superficie que monta el panel nativo (`ParameterPanel`) y el
 *          `XYPad` de morph, y la unica que mide el arranque de la pagina
 *          (`--auto-quit` -> `pilot-startup.log`). Eso se queda; lo que se fue con el
 *          piloto es su exportacion React (`WebPilot/out`), la bandera `--pilot-page`
 *          y el snapshot que esta exe llevaba embebido de ella: ahora la unica pagina
 *          es `WebUI/dist`, se sirve SIEMPRE desde disco, y un fallo de disco da la
 *          pagina de diagnostico (que dice donde la busco y como arreglarlo) en vez de
 *          contestar con otra pagina. Corre las MISMAS seis direcciones del selftest
 *          que el plugin, sin omitidos: desde 8.4 no hay una segunda pagina a la que
 *          rebajar el liston.
 */

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>   // AudioProcessorPlayer
#include <juce_core/juce_core.h>

#include "Main/NEURONiKProcessor.h"
#include "Serialization/PresetManager.h"
#include "State/ParameterDescriptors.h"
#include "UI/ParameterPanel.h"
#include "UI/XYPad.h"
#include "WebUI/BridgeAdapters.h"
#include "WebUI/BridgeSelftest.h"
#include "WebUI/ParameterBridge.h"

#include <atomic>
#include <iostream>
#include <optional>
#include <vector>

namespace
{
    // The three bridge adapters live in WebUI/BridgeAdapters.h (ticket 8.1) so the
    // plugin editor can use the SAME ones instead of a copy per surface.
    using NEURONiK::WebUI::EngineModelsAdapter;
    using NEURONiK::WebUI::MidiInjectionAdapter;
    using NEURONiK::WebUI::PresetManagerAdapter;
    using NEURONiK::WebUI::RandomizerAdapter;
    using NEURONiK::WebUI::VisualizationSourceAdapter;

    //==============================================================================
    // Measurement state

    /** @brief Process start, used as the zero for every measurement. */
    double processStartMs = 0.0;

    /** @brief Timings captured while the page boots, in milliseconds from start. */
    struct StartupMetrics
    {
        double optionsBuiltMs = -1.0;   // JUCE WebBrowserComponent constructed
        double documentMs = -1.0;       // document.readyState reached "interactive"
        double panelMs = -1.0;          // el panel de la pagina existe en el DOM
        double reactReadyMs = -1.0;     // window.__pilotReady, que publica la pagina
    };

    StartupMetrics metrics;

    /** @brief Process exit code decided by the selftest (-1 = no verdict yet).
     *  PilotComponent writes it when the selftest finishes; PilotApplication reads
     *  it on systemRequestedQuit so the process actually exits 0/1. Without this
     *  the exit code was ALWAYS 0 and build.bat's step 8 gate could never fail.
     */
    std::atomic<int> g_selftestExitCode { -1 };

    /** @brief Resource traffic observed by the provider. */
    std::atomic<int> servedCount { 0 };
    std::atomic<long long> servedBytes { 0 };
    std::atomic<int> missedCount { 0 };

    /** @brief Paths the provider could not resolve, kept for the report. */
    juce::CriticalSection missedPathsLock;
    juce::StringArray missedPaths;


    double nowMs()
    {
        return juce::Time::getMillisecondCounterHiRes() - processStartMs;
    }

    bool containsArgument (const juce::String& commandLine, const juce::String& flag)
    {
        return commandLine.contains (flag);
    }

    //==============================================================================
    // Which page this bench hosts

    /**
     * @brief La unica pagina que sirve esta bancada: `WebUI/dist`, la MISMA que
     *        embebe el plugin que se envia.
     *
     * Hasta el ticket 8.4 habia una segunda (`WebPilot/out`, con `--pilot-page`), y
     * elegir una u otra NO era un detalle de transporte: decidia que direcciones del
     * selftest eran aplicables. Con el piloto fuera queda una sola pagina, asi que la
     * eleccion (y la capacidad que la declaraba al arnes) desaparece.
     */
    juce::String pageSourceName()
    {
        return "WebUI/dist (la pagina del plugin)";
    }

    //==============================================================================
    // Asset location

    /**
     * @brief Locate a page root on disk. Tries the compile-time path first, then
     *        walks up from the executable and the current working directory.
     * @param repositoryRelative path from a repository root, e.g. "WebUI/dist".
     * @param compiledInPath     path baked in at configure time, or nullptr.
     * @returns an invalid file when nothing was found.
     */
    juce::File locateRoot (const char* repositoryRelative, const char* compiledInPath)
    {
        if (compiledInPath != nullptr)
        {
            const juce::File compiledIn (compiledInPath);

            if (compiledIn.getChildFile ("index.html").existsAsFile())
                return compiledIn;
        }

        auto walkUp = [repositoryRelative] (juce::File dir)
        {
            for (int i = 0; i < 8 && dir.isDirectory(); ++i)
            {
                const auto candidate = dir.getChildFile (juce::String (repositoryRelative));

                if (candidate.getChildFile ("index.html").existsAsFile())
                    return candidate;

                const auto parent = dir.getParentDirectory();

                if (parent == dir)
                    break;

                dir = parent;
            }

            return juce::File{};
        };

        if (auto found = walkUp (juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                                     .getParentDirectory());
            found.isDirectory())
            return found;

        if (auto found = walkUp (juce::File::getCurrentWorkingDirectory()); found.isDirectory())
            return found;

        return {};
    }

    /**
     * @brief `WebUI/dist`, la pagina del plugin, con el MISMO override de desarrollo
     *        que el plugin (`NeuronikWebView.h`): apuntar `NEURONIK_WEBUI_DEV_DIR` a
     *        la exportacion y recargar itera la UI en segundos. Vacio por defecto.
     */
    juce::File findWebUiRoot()
    {
        const juce::File devDir (juce::SystemStats::getEnvironmentVariable ("NEURONIK_WEBUI_DEV_DIR", {}));

        if (devDir.isDirectory() && devDir.getChildFile ("index.html").existsAsFile())
            return devDir;

       #if defined(NEURONiK_WEBUI_DIR)
        return locateRoot ("WebUI/dist", NEURONiK_WEBUI_DIR);
       #else
        return locateRoot ("WebUI/dist", nullptr);
       #endif
    }

    const juce::File& webUiRoot()
    {
        static const juce::File root = findWebUiRoot();
        return root;
    }

    /** @brief El root de la pagina que esta bancada sirve (la unica: `WebUI/dist`). */
    const juce::File& servedRoot()
    {
        return webUiRoot();
    }

    //==============================================================================
    // Resource plumbing

    juce::String normalizePath(juce::String url)
    {
        if (const auto query = url.indexOfChar('?'); query >= 0)
            url = url.substring(0, query);
        if (const auto fragment = url.indexOfChar('#'); fragment >= 0)
            url = url.substring(0, fragment);

        if (url.startsWith("juce://"))
        {
            const auto hostEnd = url.indexOf(7, "/");
            url = hostEnd >= 0 ? url.substring(hostEnd) : "/";
        }
        else if (url.startsWith("https://juce.backend"))
        {
            url = url.substring(20);
        }

        if (url.isEmpty() || url == "/")
            url = "/index.html";
        while (url.startsWithChar('/'))
            url = url.substring(1);

        return juce::URL::removeEscapeChars(url);
    }

    juce::String mimeTypeFor(const juce::String& filename)
    {
        if (filename.endsWithIgnoreCase(".html")) return "text/html";
        if (filename.endsWithIgnoreCase(".css")) return "text/css";
        if (filename.endsWithIgnoreCase(".js") || filename.endsWithIgnoreCase(".mjs")) return "application/javascript";
        if (filename.endsWithIgnoreCase(".json") || filename.endsWithIgnoreCase(".map")) return "application/json";
        if (filename.endsWithIgnoreCase(".txt")) return "text/plain";
        if (filename.endsWithIgnoreCase(".svg")) return "image/svg+xml";
        if (filename.endsWithIgnoreCase(".png")) return "image/png";
        if (filename.endsWithIgnoreCase(".jpg") || filename.endsWithIgnoreCase(".jpeg")) return "image/jpeg";
        if (filename.endsWithIgnoreCase(".webp")) return "image/webp";
        if (filename.endsWithIgnoreCase(".woff2")) return "font/woff2";
        if (filename.endsWithIgnoreCase(".woff")) return "font/woff";
        if (filename.endsWithIgnoreCase(".ico")) return "image/x-icon";
        return "application/octet-stream";
    }

    juce::WebBrowserComponent::Resource toResource(const juce::MemoryBlock& data, const juce::String& mimeType)
    {
        std::vector<std::byte> bytes(static_cast<size_t>(data.getSize()));
        if (data.getSize() > 0)
            std::memcpy(bytes.data(), data.getData(), data.getSize());

        servedCount.fetch_add (1, std::memory_order_relaxed);
        servedBytes.fetch_add (static_cast<long long>(data.getSize()), std::memory_order_relaxed);

        return juce::WebBrowserComponent::Resource { std::move(bytes), mimeType.toStdString() };
    }

    juce::WebBrowserComponent::Resource resourceFromText(const juce::String& text)
    {
        juce::MemoryBlock block(text.toRawUTF8(), text.getNumBytesAsUTF8());
        return toResource(block, "text/html");
    }

    /** @brief Readable failure page: a missing export should never render as a blank window. */
    juce::WebBrowserComponent::Resource diagnosticPage (const juce::String& requestedPath)
    {
        const auto root = servedRoot();
        const auto rootText = root.isDirectory() ? root.getFullPathName() : juce::String("<not found>");
        const auto fix = juce::String ("<p>Fix: run <code>pnpm build</code> inside <code>ABDNeural/WebUI</code> "
                                        "(or point <code>NEURONIK_WEBUI_DEV_DIR</code> at it), then rebuild this host.</p>");

        const juce::String html =
            "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>NEURONiK WebUI bench</title>"
            "<style>body{background:#12161c;color:#e6edf3;font-family:system-ui,Segoe UI,sans-serif;padding:32px}"
            "code{background:#1f2630;padding:2px 6px;border-radius:4px}"
            ".card{border:1px solid #2c3542;border-radius:10px;padding:20px;max-width:760px}"
            "h1{font-size:18px;margin:0 0 12px}p{line-height:1.5}li{margin:4px 0}</style></head><body>"
            "<div class=\"card\"><h1>Page assets not found</h1>"
            "<p>This bench could not serve <code>/" + requestedPath + "</code> from "
            + pageSourceName() + ".</p>"
            "<p>Resolved page root: <code>" + rootText + "</code></p>"
            "<p>Expected layout:</p><ul>"
            "<li><code>ABDNeural/WebUI/dist/index.html</code></li>"
            "<li><code>ABDNeural/WebUI/dist/assets/*.js</code></li></ul>"
            + fix +
            "</div></body></html>";

        return resourceFromText(html);
    }

    std::optional<juce::WebBrowserComponent::Resource> loadPageResource(const juce::String& url)
    {
        const auto relativePath = normalizePath(url);

        // JUCE serves its own frontend script; never shadow it. Native integration is
        // enabled in this host, so juce.js is REQUIRED: it installs window.__JUCE__,
        // which the page needs to send and receive parameter traffic.
        if (relativePath == "juce.js" || relativePath.endsWith("/juce.js"))
            return std::nullopt;

        const auto root = servedRoot();
        if (root.isDirectory())
        {
            const auto file = root.getChildFile(relativePath.replace("/", juce::File::getSeparatorString()));
            if (file.existsAsFile())
            {
                juce::MemoryBlock data;
                if (file.loadFileAsData(data))
                    return toResource(data, mimeTypeFor(file.getFileName()));
            }
        }

        // A disk miss has NO fallback: esta exe no lleva ninguna copia embebida de la
        // pagina (lo que embebe la WebUI es el PLUGIN, no la bancada), y contestar con
        // otra pagina no es un fallback. El documento pide la pagina de diagnostico.
        //
        // Only the document request gets a visible page; missing sub-resources stay 404.
        if (relativePath == "index.html" || relativePath.isEmpty())
            return diagnosticPage(relativePath);

        missedCount.fetch_add (1, std::memory_order_relaxed);

        const juce::ScopedLock lock (missedPathsLock);
        if (! missedPaths.contains (relativePath))
            missedPaths.add (relativePath);

        return std::nullopt;
    }

    //==============================================================================
    // Window / application

    /** @brief Writes one measurement block to pilot-startup.log. */
    void appendReport (const juce::String& reason, const juce::String& titleText)
    {
        const auto formatMs = [] (double value)
        {
            return value < 0.0 ? juce::String ("not observed") : juce::String (value, 1) + " ms";
        };

        const auto served = servedCount.load();
        const auto bytes = servedBytes.load();

        juce::String report;
        report << "# run " << juce::Time::getCurrentTime().toString (true, true, true, true)
               << "  reason=" << reason << "\n";
        report << "  page                : " << pageSourceName() << "\n";
        report << "  page root           : "
               << (servedRoot().isDirectory() ? servedRoot().getFullPathName()
                                              : juce::String ("<not found on disk>"))
               << "\n";
        report << "  options built       : " << formatMs (metrics.optionsBuiltMs) << "\n";
        report << "  document interactive: " << formatMs (metrics.documentMs) << "\n";
        report << "  panel in DOM        : " << formatMs (metrics.panelMs) << "\n";
        report << "  react ready         : " << formatMs (metrics.reactReadyMs) << "\n";
        report << "  resources served    : " << served
               << " (" << juce::String (bytes / 1024) << " KB)" << "\n";
        report << "  resource misses     : " << missedCount.load();

        {
            const juce::ScopedLock lock (missedPathsLock);
            if (! missedPaths.isEmpty())
                report << " (" << missedPaths.joinIntoString (", ") << ")";
        }

        report << "\n";

        const auto logFile = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                                 .getParentDirectory()
                                 .getChildFile ("pilot-startup.log");

        report << "  log                 : " << logFile.getFullPathName() << "\n\n";

        std::cout << report << std::flush;

        if (logFile.appendText (report))
        {
            juce::Logger::writeToLog ("[WebPilotHost] " + titleText);
        }
    }

    class PilotComponent final : public juce::Component,
                                 private juce::Timer
    {
    public:
        PilotComponent (bool shouldAutoQuit, bool runSelftest)
            : browser (makeBrowserOptions()),
              processor(),
              autoQuit (shouldAutoQuit),
              selftest (runSelftest)
        {
            // The REAL processor, not a stand-in: the bridge mirrors the plugin's own
            // APVTS and the native panel below the page is the tab the editor ships.
            bridge = std::make_unique<NEURONiK::WebUI::ParameterBridge> (processor.getAPVTS());
            bridge->setSender ([this] (const juce::var& message)
            {
                if (browser.isVisible())
                    browser.emitEventIfBrowserIsVisible (
                        NEURONiK::WebUI::BridgeEventIds::nativeToJs, message);
            });

            // Render the plugin through the default audio device. Without this the
            // processor's processBlock NEVER runs here: page keyboard notes would pile
            // up in the MIDI FIFO unheard, and engine telemetry would stay frozen. This
            // also makes the page audible.
            audioDeviceManager.initialiseWithDefaultDevices (0, 2);
            audioPlayer.setProcessor (&processor);
            audioDeviceManager.addAudioCallback (&audioPlayer);

            // Preset management for the page: the bridge owns the wire, this
            // adapter owns the plugin behaviour. The processor (and with it the
            // preset manager) outlives the bridge, so a bare pointer is safe.
            presetAdapter = std::make_unique<PresetManagerAdapter> (processor);
            bridge->setPresetController (presetAdapter.get());

            // Page keyboard/wheels -> processor FIFO. The processor outlives the
            // bridge, so a bare pointer is safe (same rules as the preset adapter).
            midiAdapter = std::make_unique<MidiInjectionAdapter> (processor);
            bridge->setMidiController (midiAdapter.get());

            // Spectral models for the page: the bridge publishes the engine's
            // current slots (file-backed mirror) so the WASM path can morph
            // between the SAME partials the plugin renders — and serves the
            // loadModel action (the loadA..loadD dialog the native panel had).
            // That dialog is ASYNCHRONOUS, so its answer is sent from the
            // adapter's callbacks: the bridge keeps owning the wire. A pending
            // answer cannot outlive the bridge here either, because the chooser
            // is a member of the adapter and juce::FileChooser drops its pending
            // async callback when it is destroyed.
            modelsAdapter = std::make_unique<EngineModelsAdapter> (
                processor,
                [this] (int) { bridge->sendModelsState(); },
                [this] (int slot, const juce::String& detail) { bridge->sendModelError (slot, detail); });
            bridge->setModelController (modelsAdapter.get());

            // RANDOMIZE as a bridge action: the bench keeps the capability the
            // native panel used to offer (the panel is on its way out, the action
            // is not). The page asks, the processor randomises.
            randomizeAdapter = std::make_unique<RandomizerAdapter> (processor);
            bridge->setRandomizeController (randomizeAdapter.get());

            // El cuarto adaptador: telemetria visual para el canal nativa->web
            // (la bancada la ejercita con audio real, igual que el editor).
            visualizationAdapter = std::make_unique<VisualizationSourceAdapter> (processor);
            bridge->setTelemetryController (visualizationAdapter.get());

            addAndMakeVisible (browser);
            nativePanel = std::make_unique<NEURONiK::UI::ParameterPanel> (processor);
            addAndMakeVisible (nativePanel.get());

            // Exercise (c) of the checklist: the native XYPad follows morphX/morphY
            // through the processor's uiMorph* telemetry, so a page edit (or a native
            // RANDOM push) is visibly mirrored by the pad.
            xyPad = std::make_unique<NEURONiK::UI::XYPad> (processor, processor.getAPVTS());
            addAndMakeVisible (xyPad.get());

            browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

            startTimer (30);

            if (selftest)
            {
                // The SAME harness the plugin editor runs (ticket 8.1 step 2c), so
                // the bench cannot drift from the surface that ships. Here it only
                // gets this browser and this verdict: the exit code is what build.bat
                // reads, exactly as before.
                selftestRunner = std::make_unique<NEURONiK::WebUI::BridgeSelftest> (
                    processor,
                    [this] (const juce::String& script, std::function<void (const juce::String&)> onResult)
                    {
                        browser.evaluateJavascript (
                            script,
                            [onResult] (juce::WebBrowserComponent::EvaluationResult result)
                            {
                                if (onResult == nullptr)
                                    return;

                                onResult (result.getResult() != nullptr
                                              ? result.getResult()->toString()
                                              : juce::String ("NO_RESULT"));
                            });
                    },
                    [] (const juce::String& line) { std::cout << line << "\n"; },
                    [this] (bool passed)
                    {
                        selftestPassed = passed;
                        g_selftestExitCode.store (passed ? 0 : 1, std::memory_order_relaxed);
                        finish (passed ? "selftest-ok" : "selftest-fail");
                    });

                selftestRunner->start();
            }
        }

        ~PilotComponent() override
        {
            stopTimer();
            audioDeviceManager.removeAudioCallback (&audioPlayer);
            audioPlayer.setProcessor (nullptr);
            bridge->setSender ({});   // the browser dies before the bridge does
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            nativePanel->setBounds (bounds.removeFromBottom (stripHeight));

            xyPad->setBounds (bounds.removeFromRight (260));   // morph mirror next to the page
            browser.setBounds (bounds);
        }

    private:
        static constexpr int stripHeight = 240;   // the real GENERAL tab needs more than the old strip
        static constexpr double pollTimeoutMs = 20000.0;

        // ============================================================================
        // --selftest: the six-direction check lives in WebUI/BridgeSelftest.h.
        //
        // It used to be a state machine right here, which is exactly why only this
        // bench could run it. Since ticket 8.1 step 2c the plugin editor hosts the
        // page too, so the harness is shared and both surfaces exercise the SAME
        // directions instead of two copies that drift: the bench only plugs in this
        // browser and this verdict (the plugin plugs in the editor's web view). Las dos
        // superficies sirven la MISMA pagina (`WebUI/dist`) y corren las SEIS direcciones
        // sin omitidos desde que el piloto se retiro (ticket 8.4).
        // ============================================================================

        /** @brief Browser options, with the transport of the parameter bridge. */
        juce::WebBrowserComponent::Options makeBrowserOptions()
        {
            // Native integration MUST be enabled: without it there is no window.__JUCE__
            // at all, and both directions of the channel are dead. The direction guard
            // covers the other classic failure (backend.emitEvent from C++).
            return juce::WebBrowserComponent::Options{}
                .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                .withNativeIntegrationEnabled (true)
                .withResourceProvider (loadPageResource)
                .withEventListener (NEURONiK::WebUI::BridgeEventIds::jsToNative,
                                    [this] (const juce::var& message)
                                    {
                                        if (bridge != nullptr)
                                            bridge->handleJsEvent (message);
                                    })
                .withEventListener (NEURONiK::WebUI::BridgeEventIds::pageLoaded,
                                    [this] (const juce::var&)
                                    {
                                        if (bridge == nullptr)
                                            return;

                                        // A (re)loaded page knows nothing: close the gestures
                                        // it left open mid drag, then hand it the full state.
                                        bridge->closeOpenGestures();
                                        bridge->sendFullSnapshot();
                                    });
            // NOTE: withEventListener requires the member initialisation order used here
            // (browser declared before bridge) because the listener lambdas capture this
            // and dereference bridge lazily at event time, never during construction.
        }

        void timerCallback() override
        {
            // No audio callback here: without this poll, the APVTS-derived ui*
            // telemetry (morph coords, envelope params) would freeze and the
            // native XYPad would never move.
            processor.refreshUiTelemetryFromApvts();

            bridge->publishPendingChanges();

            // Mirror the plugin's external MIDI view on the page keyboard every
            // ~180 ms (each 6th tick of the 30 ms timer): held notes come from the
            // same FIFO the processor drains, wheels from the atomics.
            if (++midiStateTick >= 6)
            {
                midiStateTick = 0;
                bridge->sendMidiNoteState (processor.getHeldNotes(),
                                           processor.externalPitchBend.load(),
                                           processor.externalModWheel.load());
            }

            // Telemetria visual (~15 Hz): mismo contrato que en el editor.
            if (++telemetryTick >= telemetryTicks)
            {
                telemetryTick = 0;
                bridge->sendTelemetry();
            }

            if (finished)
                return;

            if (nowMs() > pollTimeoutMs)
            {
                finish ("timeout");
                return;
            }

            nativePanel->repaint();   // the real panel repaints its own timers; labels need no manual refresh

            if (selftestRunner != nullptr)
            {
                if (metrics.reactReadyMs >= 0.0)
                    selftestRunner->notifyPageReady();

                selftestRunner->tick();

                if (finished)   // the selftest can finish inside its own tick
                    return;
            }

            // Probing document state is what lets us time the real panel, not just
            // the navigation.
            if (probeInFlight)
                return;

            probeInFlight = true;

            browser.evaluateJavascript (
                "(() => [document.readyState,"
                " document.querySelector('.panel') ? '1' : '0',"
                " window.__pilotReady ? '1' : '0'].join('|'))()",
                [this] (juce::WebBrowserComponent::EvaluationResult result)
                {
                    probeInFlight = false;

                    if (const auto* value = result.getResult())
                        handleProbe (value->toString());
                });
        }

        void handleProbe (const juce::String& probe)
        {
            const auto parts = juce::StringArray::fromTokens (probe, "|", {});

            if (parts.size() < 3)
                return;

            if (metrics.documentMs < 0.0 && (parts[0] == "interactive" || parts[0] == "complete"))
                metrics.documentMs = nowMs();

            if (metrics.panelMs < 0.0 && parts[1] == "1")
                metrics.panelMs = nowMs();

            if (metrics.reactReadyMs < 0.0 && parts[2] == "1")
            {
                metrics.reactReadyMs = nowMs();

                // In selftest mode the selftest owns the exit (it still has to push
                // both directions after this point); otherwise report and leave.
                if (! selftest)
                    finish ("ready");
            }
        }

        void finish (const juce::String& reason)
        {
            if (finished)
                return;

            finished = true;
            stopTimer();

            // A selftest that ends without a verdict (e.g. a timeout because the
            // page never became ready) is a FAIL: publish exit 1 or the process
            // would report success. The harness verdict callback stores the code
            // BEFORE finishing, so this never overrides a real verdict.
            if (selftest && g_selftestExitCode.load (std::memory_order_relaxed) < 0)
                g_selftestExitCode.store (1, std::memory_order_relaxed);

            const auto title =
                "NEURONiK Web Pilot — panel " + juce::String (metrics.panelMs, 0) + " ms / ready "
                + juce::String (metrics.reactReadyMs, 0) + " ms / " + juce::String (servedCount.load())
                + " resources";

            if (auto* topLevel = getTopLevelComponent())
                topLevel->setName (title);

            appendReport (reason, title);

            if (autoQuit)
                juce::Timer::callAfterDelay (400, [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); });
        }

        // Declaration order matters: the browser's event listeners capture `this` and
        // dereference `bridge` at event time, so `bridge` must never be destroyed while
        // the browser is alive -> browser first, then processor, bridge, panel last.
        juce::WebBrowserComponent browser;
        NEURONiKProcessor processor;
        // The bridge only holds a PresetController* to this adapter; declared
        // AFTER processor (it references its PresetManager) and destroyed with
        // the component, before the processor's own members go away.
        std::unique_ptr<PresetManagerAdapter> presetAdapter;
        std::unique_ptr<MidiInjectionAdapter> midiAdapter;
        std::unique_ptr<EngineModelsAdapter> modelsAdapter;
        std::unique_ptr<RandomizerAdapter> randomizeAdapter;
        std::unique_ptr<VisualizationSourceAdapter> visualizationAdapter;
        std::unique_ptr<NEURONiK::WebUI::ParameterBridge> bridge;

        // Audio plumbing de la bancada: default device + the standard JUCE player
        // that pulls the processor's processBlock. The processor is declared BEFORE
        // these (they reference it), and destroyed AFTER them (the callback must be
        // gone before the processor dies) — member order handles both.
        juce::AudioDeviceManager audioDeviceManager;
        juce::AudioProcessorPlayer audioPlayer;
        int midiStateTick = 0;
        // Telemetry decimation: emit every Nth poll (~15 Hz over the 30 ms poll;
        // the bridge value-diffs on top, so an idle synth adds no traffic).
        static constexpr int telemetryTicks = 2;
        int telemetryTick = 0;
        std::unique_ptr<NEURONiK::UI::ParameterPanel> nativePanel;
        // Declared after processor/bridge so it is destroyed BEFORE them (it reads
        // the APVTS and the IVisualizationSource in its 30 Hz timer).
        std::unique_ptr<NEURONiK::UI::XYPad> xyPad;
        const bool autoQuit;
        const bool selftest;
        // The shared five-direction check (WebUI/BridgeSelftest.h), or null when
        // --selftest was not passed. Declared AFTER `browser` on purpose: it asks
        // the browser for scripts, so it has to die BEFORE it.
        std::unique_ptr<NEURONiK::WebUI::BridgeSelftest> selftestRunner;
        bool selftestPassed = false;   // the verdict, for the report and the exit code
        bool probeInFlight = false;
        bool finished = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PilotComponent)
    };

    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow (bool autoQuit, bool runSelftest)
            : DocumentWindow("NEURONiK Web Pilot",
                             juce::Colours::black,
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new PilotComponent (autoQuit, runSelftest), true);
            centreWithSize(900, 760);
            setResizable(true, true);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    class PilotApplication final : public juce::JUCEApplication
    {
    public:
        const juce::String getApplicationName() override { return "NEURONiK Web Pilot"; }
        const juce::String getApplicationVersion() override { return "0.1.0"; }
        bool moreThanOneInstanceAllowed() override { return true; }

        void initialise(const juce::String& commandLine) override
        {
            processStartMs = juce::Time::getMillisecondCounterHiRes();

            std::cout << "[page] sirviendo " << pageSourceName() << "\n"
                      << "[page] root: "
                      << (servedRoot().isDirectory() ? servedRoot().getFullPathName()
                                                     : juce::String ("<no esta en disco>"))
                      << std::endl;

            // --selftest implies --auto-quit: the check runs unattended and the exit
            // code is the verdict (0 = every applicable direction moved, 1 = something didn't).
            const auto runSelftest = containsArgument (commandLine, "--selftest");
            const auto shouldAutoQuit = containsArgument (commandLine, "--auto-quit") || runSelftest;

            window = std::make_unique<MainWindow> (shouldAutoQuit, runSelftest);
        }

        void shutdown() override
        {
            window.reset();
        }

        void systemRequestedQuit() override
        {
            // The selftest verdict decides the process exit code (0/1); -1 means no
            // selftest ran (manual close / --auto-quit) and the default 0 stays.
            if (const auto code = g_selftestExitCode.load(); code >= 0)
                setApplicationReturnValue (code);

            quit();
        }

        void anotherInstanceStarted(const juce::String&) override {}

    private:
        std::unique_ptr<MainWindow> window;
    };
}

START_JUCE_APPLICATION(PilotApplication)
