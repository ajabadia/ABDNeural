/**
 * @file WebPilotHost.cpp
 * @brief JUCE/WebView2 host for the Next.js pilot: startup metrics PLUS the real
 *        parameter bridge.
 * @details Loads `WebPilot/out` through a WebBrowserComponent resource provider and
 *          now also mirrors a real APVTS both ways over the WebView2 channel:
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
 *          TEMPORARY drill: `--selftest-force-fail` forces the FAIL verdict (exit code 1)
 *          without running the E2E, to validate that the verdict reaches the process exit
 *          code. Remove the flag once both paths (0 and 1) have been verified.
 */

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "Main/NEURONiKProcessor.h"
#include "State/ParameterDescriptors.h"
#include "UI/ParameterPanel.h"
#include "WebUI/ParameterBridge.h"

#ifdef NEURONIK_HAS_PILOT_ASSETS
 #include "BinaryData.h"   // juce_add_binary_data(NEURONiK_WebPilotAssets): embedded WebUI snapshot
#endif

#include <atomic>
#include <iostream>
#include <optional>
#include <vector>

namespace
{
    //==============================================================================
    // Measurement state

    /** @brief Process start, used as the zero for every measurement. */
    double processStartMs = 0.0;

    /** @brief Timings captured while the page boots, in milliseconds from start. */
    struct StartupMetrics
    {
        double optionsBuiltMs = -1.0;   // JUCE WebBrowserComponent constructed
        double documentMs = -1.0;       // document.readyState reached "interactive"
        double panelMs = -1.0;          // the pilot panel exists in the DOM
        double reactReadyMs = -1.0;     // window.__pilotReady set by the React effect
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
    std::atomic<int> embeddedServed { 0 };   // served from the exe's embedded snapshot

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
    // Asset location

    /** @brief Locate the directory holding the Next.js static export (`WebPilot/out`).
     *  Tries the compile-time path first, then walks up from the executable and the
     *  current working directory. Returns an invalid file when nothing is found.
     */
    juce::File findPilotRoot()
    {
       #if defined(NEURONiK_WEBPILOT_OUT_DIR)
        const juce::File compiledIn(NEURONiK_WEBPILOT_OUT_DIR);
        if (compiledIn.getChildFile("index.html").existsAsFile())
            return compiledIn;
       #endif

        auto walkUp = [] (juce::File dir)
        {
            for (int i = 0; i < 8 && dir.isDirectory(); ++i)
            {
                const auto candidate = dir.getChildFile("WebPilot").getChildFile("out");
                if (candidate.getChildFile("index.html").existsAsFile())
                    return candidate;

                const auto parent = dir.getParentDirectory();
                if (parent == dir)
                    break;
                dir = parent;
            }
            return juce::File{};
        };

        if (auto found = walkUp(juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                    .getParentDirectory());
            found.isDirectory())
            return found;

        if (auto found = walkUp(juce::File::getCurrentWorkingDirectory()); found.isDirectory())
            return found;

        return {};
    }

    const juce::File& pilotRoot()
    {
        static const juce::File root = findPilotRoot();
        return root;
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
    juce::WebBrowserComponent::Resource diagnosticPage([[maybe_unused]] const juce::String& requestedPath)
    {
        const auto root = pilotRoot();
        const auto rootText = root.isDirectory() ? root.getFullPathName() : juce::String("<not found>");

        const juce::String html =
            "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>NEURONiK Web Pilot</title>"
            "<style>body{background:#12161c;color:#e6edf3;font-family:system-ui,Segoe UI,sans-serif;padding:32px}"
            "code{background:#1f2630;padding:2px 6px;border-radius:4px}"
            ".card{border:1px solid #2c3542;border-radius:10px;padding:20px;max-width:760px}"
            "h1{font-size:18px;margin:0 0 12px}p{line-height:1.5}li{margin:4px 0}</style></head><body>"
            "<div class=\"card\"><h1>Web pilot assets not found</h1>"
            "<p>The host could not serve <code>/\" + requestedPath + \"</code> from the Next.js static export.</p>"
            "<p>Resolved export root: <code>" + rootText + "</code></p>"
            "<p>Expected layout:</p><ul>"
            "<li><code>ABDNeural/WebPilot/out/index.html</code></li>"
            "<li><code>ABDNeural/WebPilot/out/_next/static/chunks/*.js</code></li></ul>"
            "<p>Fix: run <code>pnpm install --ignore-workspace && pnpm build</code> inside "
            "<code>ABDNeural/WebPilot</code>, then rebuild this host.</p>"
            "</div></body></html>";

        return resourceFromText(html);
    }

    // Defined below loadPilotResource; forward-declared so the disk-first
    // fallback order never depends on helper definition order.
    std::optional<juce::WebBrowserComponent::Resource> loadEmbeddedResource (const juce::String& relativePath);

    std::optional<juce::WebBrowserComponent::Resource> loadPilotResource(const juce::String& url)
    {
        const auto relativePath = normalizePath(url);

        // JUCE serves its own frontend script; never shadow it. Native integration is
        // enabled in this host, so juce.js is REQUIRED: it installs window.__JUCE__,
        // which the page needs to send and receive parameter traffic.
        if (relativePath == "juce.js" || relativePath.endsWith("/juce.js"))
            return std::nullopt;

        const auto root = pilotRoot();
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

        // Disk misses fall back to the WebUI snapshot embedded in the exe (see
        // NEURONiK_WebPilotAssets in CMakeLists.txt): the host stays usable when the
        // export folder is not next to it — and this is how the VST3 will serve it.
        if (auto embedded = loadEmbeddedResource(relativePath))
            return embedded;

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
    // Embedded WebUI fallback (see NEURONiK_WebPilotAssets in CMakeLists.txt)

    /** @brief Serve `relativePath` from the snapshot embedded in the exe.
     *  @details The disk folder ALWAYS wins (fresh `pnpm build` output); this only
     *           runs when the export folder is not next to the executable, so the
     *           host stays usable standalone — and it is exactly how the final VST3
     *           will carry its WebUI. Matching is by path suffix / basename, and
     *           the snapshot's error routes (`404/index.html`, `404.html`,
     *           `_not-found/…`) are never allowed to answer a normal request:
     *           the resource list is alphabetical, so `404/index.html` used to
     *           shadow the real `index.html` (the page then never reached
     *           react-ready and the selftest timed out).
     */
    std::optional<juce::WebBrowserComponent::Resource> loadEmbeddedResource (const juce::String& relativePath)
    {
       #ifdef NEURONIK_HAS_PILOT_ASSETS
        // The BinaryData resource names are mangled to C identifiers (dots and
        // dashes become underscores), so match by the ORIGINAL filename recorded
        // alongside each resource and fetch the payload through the resource NAME
        // (never the path).
        const auto basePath = relativePath.fromLastOccurrenceOf ("/", false, true);

        // Keep each side of the error-route split (see the class doc above): a
        // normal request may only match normal resources, and an error-route
        // request only matches error-route resources.
        const auto errorRoute = relativePath.startsWith ("404/")
                                    || relativePath == "404.html"
                                    || relativePath.startsWith ("_not-found/");

        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const auto* resourceName = BinaryData::namedResourceList[i];
            const auto original = juce::String (BinaryData::getNamedResourceOriginalFilename (resourceName))
                                      .replaceCharacter ('\\', '/');

            if (errorRoute != (original.contains ("/404/") || original.endsWith ("/404.html")
                               || original.contains ("/_not-found/")))
                continue;

            if (! (original == relativePath || original.endsWith (relativePath)
                   || original.endsWith (basePath)))
                continue;

            int size = 0;

            if (const auto* data = BinaryData::getNamedResource (resourceName, size))
            {
                embeddedServed.fetch_add (1, std::memory_order_relaxed);
                return toResource (juce::MemoryBlock (data, (size_t) size), mimeTypeFor (relativePath));
            }
        }
       #else
        juce::ignoreUnused (relativePath);
       #endif

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
        report << "  options built       : " << formatMs (metrics.optionsBuiltMs) << "\n";
        report << "  document interactive: " << formatMs (metrics.documentMs) << "\n";
        report << "  panel in DOM        : " << formatMs (metrics.panelMs) << "\n";
        report << "  react ready         : " << formatMs (metrics.reactReadyMs) << "\n";
        report << "  resources served    : " << served
               << " (" << juce::String (bytes / 1024) << " KB)"
               << " [embedded fallback: " << embeddedServed.load() << "]" << "\n";
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
        PilotComponent (bool shouldAutoQuit, bool runSelftest, bool forceSelftestFailDrill)
            : browser (makeBrowserOptions()),
              processor(),
              autoQuit (shouldAutoQuit),
              selftest (runSelftest),
              forceSelftestFail (forceSelftestFailDrill)
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

            addAndMakeVisible (browser);
            nativePanel = std::make_unique<NEURONiK::UI::ParameterPanel> (processor);
            addAndMakeVisible (nativePanel.get());
            browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

            startTimer (30);

            if (selftest)
                startSelftest();
        }

        ~PilotComponent() override
        {
            stopTimer();
            bridge->setSender ({});   // the browser dies before the bridge does
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            nativePanel->setBounds (bounds.removeFromBottom (stripHeight));
            browser.setBounds (bounds);
        }

    private:
        static constexpr int stripHeight = 240;   // the real GENERAL tab needs more than the old strip
        static constexpr double pollTimeoutMs = 20000.0;
        static constexpr double selftestStepMs = 250.0;

        // ============================================================================
        // --selftest: automated two-direction check of the bridge.
        //
        // I cannot move a mouse, so this exercises the SAME channel a manual test would:
        //   NATIVE -> JS: setValueNotifyingHost on the APVTS, then read the page's slider
        //                 position back via evaluateJavascript.
        //   JS -> NATIVE: dispatch a real 'input' event on the page's slider (exactly what
        //                 a user drag produces), then read the APVTS parameter.
        // The script prints SELFTEST: OK / SELFTEST: FAIL and the process exits 0/1.
        //
        // TEMPORAL drill: `--selftest-force-fail` salta el E2E y publica FAIL a propósito,
        // para probar que el veredicto llega de verdad al exit code del proceso
        // (antes de g_selftestExitCode salía SIEMPRE 0). Retirar tras validar.
        // ============================================================================

        enum class SelftestStage { idle, waitReady, nativePushed, jsPushed, forcedFail };

        void startSelftest()
        {
            // TEMPORAL drill (--selftest-force-fail): nada de E2E — veredicto FAIL
            // inmediato en el primer tick, para poder comprobar el exit 1 sin tráfico
            // de WebView2. Eliminar junto con el stage forcedFail tras validar.
            if (forceSelftestFail)
            {
                selftestStage = SelftestStage::forcedFail;
                std::cout << "[selftest] FORCED-FAIL drill: skipping the E2E; exit code 1 is published on purpose.\n";
                return;
            }

            selftestStage = SelftestStage::waitReady;
            std::cout << "[selftest] waiting for the page to be ready...\n";
        }

        void selftestTick()
        {
            switch (selftestStage)
            {
                case SelftestStage::waitReady:
                {
                    if (metrics.reactReadyMs < 0.0)
                        return;   // the normal probe loop sets reactReadyMs when the page is up

                    selftestStage = SelftestStage::nativePushed;

                    // --- NATIVE -> JS --------------------------------------------
                    // Move the parameter the way the native UI would.
                    if (auto* parameter = processor.getAPVTS().getParameter ("masterLevel"))
                        parameter->setValueNotifyingHost (0.25f);

                    // The page's own state updates from the bridge on the next poll tick
                    // (30 ms); give it a generous 400 ms before reading the slider.
                    juce::Timer::callAfterDelay (400, [this]
                    {
                        browser.evaluateJavascript (
                            "document.querySelector('input[type=range]')"
                            " ? String(document.querySelector('input[type=range]').value)"
                            " : 'NO_SLIDER'",
                            [this] (juce::WebBrowserComponent::EvaluationResult result)
                            {
                                const auto raw = result.getResult() != nullptr
                                                   ? result.getResult()->toString()
                                                   : juce::String ("NO_RESULT");
                                selftestNativePushed (raw);
                            });
                    });
                    break;
                }

                case SelftestStage::forcedFail:
                    selftestFinish();   // TEMPORAL drill: verdict FAIL y exit 1, él solo cierra
                    break;

                case SelftestStage::nativePushed:
                    break;   // waiting on the async evaluation above

                case SelftestStage::jsPushed:
                    break;   // waiting on the async evaluation above

                case SelftestStage::idle:
                default:
                    break;
            }
        }

        void selftestNativePushed (const juce::String& sliderValue)
        {
            // The page stores normalised 0..1 and the control edits real units; for
            // masterLevel both scales coincide (0..1, no skew), so the input's value
            // should carry 0.25 (modulo float text rounding).
            const auto pageValue = sliderValue.getFloatValue();
            const auto ok = sliderValue.isNotEmpty() && sliderValue != "NO_SLIDER"
                                && sliderValue != "NO_RESULT"
                                && std::abs (pageValue - 0.25f) < 0.02f;

            std::cout << "[selftest] NATIVE -> JS: native masterLevel = 0.25, page slider = "
                      << sliderValue << " -> " << (ok ? "OK" : "FAIL") << "\n";
            selftestNativeToJsOk = ok;

            // --- JS -> NATIVE ------------------------------------------------
            // Dispatch a real input event on the first slider, the same event a user
            // drag fires. The React onChange picks it up and pushes the change through
            // the bridge; the poller applies it to the APVTS.
            selftestStage = SelftestStage::jsPushed;

            browser.evaluateJavascript (
                "(() => { const s = document.querySelector('input[type=range]');"
                " if (!s) return 'NO_SLIDER';"
                " const setter = Object.getOwnPropertyDescriptor(window.HTMLInputElement.prototype,'value').set;"
                " setter.call(s, '0.75');"
                " s.dispatchEvent(new Event('input', { bubbles: true }));"
                " return 'DISPATCHED'; })()",
                [this] (juce::WebBrowserComponent::EvaluationResult result)
                {
                    const auto raw = result.getResult() != nullptr
                                       ? result.getResult()->toString()
                                       : juce::String ("NO_RESULT");

                    // React needs a moment to re-render and push through the bridge;
                    // the APVTS is applied on the host's next 30 ms poll.
                    juce::Timer::callAfterDelay (600, [this, raw]
                    {
                        const auto* parameter = processor.getAPVTS().getParameter ("masterLevel");
                        const auto nativeValue = parameter != nullptr ? parameter->getValue() : -1.0f;
                        const auto okJs = raw == "DISPATCHED" && std::abs (nativeValue - 0.75f) < 0.02f;

                        std::cout << "[selftest] JS -> NATIVE: page slider set to 0.75, native masterLevel = "
                                  << juce::String (nativeValue, 4) << " -> " << (okJs ? "OK" : "FAIL") << "\n";
                        selftestJsToNativeOk = okJs;

                        selftestFinish();
                    });
                });
        }

        void selftestFinish()
        {
            const auto allOk = selftestNativeToJsOk && selftestJsToNativeOk;

            std::cout << "[selftest] RESULT: " << (allOk ? "OK" : "FAIL") << "\n";
            selftestPassed = allOk;

            // Publish the verdict for PilotApplication: build.bat reads the exit code.
            g_selftestExitCode.store (allOk ? 0 : 1, std::memory_order_relaxed);

            // Report the startup metrics plus the verdict, then leave: the process
            // exit code is what scripts and build.bat read. The forced-fail drill gets
            // its own reason so a drill run is never confused with a real failure.
            finish (allOk ? "selftest-ok"
                          : (forceSelftestFail ? "selftest-forced-fail" : "selftest-fail"));
        }

        /** @brief Browser options, with the transport of the parameter bridge. */
        juce::WebBrowserComponent::Options makeBrowserOptions()
        {
            // Native integration MUST be enabled: without it there is no window.__JUCE__
            // at all, and both directions of the channel are dead. The direction guard
            // covers the other classic failure (backend.emitEvent from C++).
            return juce::WebBrowserComponent::Options{}
                .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                .withNativeIntegrationEnabled (true)
                .withResourceProvider (loadPilotResource)
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
            // native XYPad/EnvelopeVisualizer would never move in the pilot.
            processor.refreshUiTelemetryFromApvts();

            bridge->publishPendingChanges();

            if (finished)
                return;

            if (nowMs() > pollTimeoutMs)
            {
                finish ("timeout");
                return;
            }

            nativePanel->repaint();   // the real panel repaints its own timers; labels need no manual refresh

            if (selftest)
            {
                selftestTick();

                if (finished)   // e.g. the forced-fail drill finishes on its very first tick
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
            // would report success. selftestFinish() stores the code BEFORE
            // finishing, so this never overrides a real verdict.
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
        std::unique_ptr<NEURONiK::WebUI::ParameterBridge> bridge;
        std::unique_ptr<NEURONiK::UI::ParameterPanel> nativePanel;
        const bool autoQuit;
        const bool selftest;
        const bool forceSelftestFail;   // TEMPORAL drill flag, see startSelftest()
        SelftestStage selftestStage = SelftestStage::idle;
        bool selftestNativeToJsOk = false;
        bool selftestJsToNativeOk = false;
        bool selftestPassed = false;
        bool probeInFlight = false;
        bool finished = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PilotComponent)
    };

    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow (bool autoQuit, bool runSelftest, bool forceSelftestFail)
            : DocumentWindow("NEURONiK Web Pilot",
                             juce::Colours::black,
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new PilotComponent (autoQuit, runSelftest, forceSelftestFail), true);
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

            // --selftest implies --auto-quit: the check runs unattended and the exit
            // code is the verdict (0 = both directions moved, 1 = something didn't).
            // TEMPORAL drill: --selftest-force-fail contiene "--selftest" como subcadena,
            // así que runSelftest sale true solo y shouldAutoQuit con él; el flag fuerza
            // el veredicto FAIL (exit 1 esperado).
            const auto forcedFailDrill = containsArgument (commandLine, "--selftest-force-fail");
            const auto runSelftest = containsArgument (commandLine, "--selftest");
            const auto shouldAutoQuit = containsArgument (commandLine, "--auto-quit") || runSelftest;

            window = std::make_unique<MainWindow> (shouldAutoQuit, runSelftest, forcedFailDrill);
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
