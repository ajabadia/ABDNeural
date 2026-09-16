/**
 * @file WebPilotHost.cpp
 * @brief Minimal JUCE/WebView2 host used to validate the Next.js static export pilot.
 * @details Independent from NEURONiKEditor: it only loads `WebPilot/out` through a
 *          WebBrowserComponent resource provider so we can check that a Next.js
 *          `output: 'export'` build runs inside WebView2 before migrating any UI.
 *
 *          It also measures the pilot, because those numbers feed the decision
 *          between Next.js and a plain React/Vite bundle:
 *            - time from process start to the WebView2 backend being constructed;
 *            - time to document interactive, to the first painted panel and to the
 *              React effect marker `window.__pilotReady`;
 *            - how many resources the page actually requests and how many bytes
 *              the provider serves (including cache hits, which are re-requested).
 *
 *          Measurements are appended to `pilot-startup.log` next to the executable
 *          and summarised in the window title. Pass `--auto-quit` to close the host
 *          automatically once the panel reports ready, which makes the measurement
 *          scriptable.
 */

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>

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
    juce::WebBrowserComponent::Resource diagnosticPage(const juce::String& requestedPath)
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
            "<p>The host could not serve <code>/" + requestedPath + "</code> from the Next.js static export.</p>"
            "<p>Resolved export root: <code>" + rootText + "</code></p>"
            "<p>Expected layout:</p><ul>"
            "<li><code>ABDNeural/WebPilot/out/index.html</code></li>"
            "<li><code>ABDNeural/WebPilot/out/_next/static/chunks/*.js</code></li></ul>"
            "<p>Fix: run <code>pnpm install --ignore-workspace &amp;&amp; pnpm build</code> inside "
            "<code>ABDNeural/WebPilot</code>, then rebuild this host.</p>"
            "</div></body></html>";

        return resourceFromText(html);
    }

    std::optional<juce::WebBrowserComponent::Resource> loadPilotResource(const juce::String& url)
    {
        const auto relativePath = normalizePath(url);

        // JUCE serves its own frontend script; never shadow it.
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
        explicit PilotComponent (bool shouldAutoQuit)
            : browser(juce::WebBrowserComponent::Options{}
                          .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
                          .withNativeIntegrationEnabled(false)
                          .withResourceProvider(loadPilotResource)),
              autoQuit (shouldAutoQuit)
        {
            metrics.optionsBuiltMs = nowMs();

            addAndMakeVisible (browser);
            browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

            startTimer (20);
        }

        void resized() override
        {
            browser.setBounds (getLocalBounds());
        }

    private:
        static constexpr double pollTimeoutMs = 20000.0;

        void timerCallback() override
        {
            if (finished)
                return;

            if (nowMs() > pollTimeoutMs)
            {
                finish ("timeout");
                return;
            }

            // Probing document state is what lets us time the real panel, not just
            // the navigation, without depending on the juce.js frontend script.
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
                finish ("ready");
            }
        }

        void finish (const juce::String& reason)
        {
            if (finished)
                return;

            finished = true;
            stopTimer();

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

        juce::WebBrowserComponent browser;
        const bool autoQuit;
        bool probeInFlight = false;
        bool finished = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PilotComponent)
    };

    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (bool autoQuit)
            : DocumentWindow("NEURONiK Web Pilot",
                             juce::Colours::black,
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new PilotComponent (autoQuit), true);
            centreWithSize(760, 620);
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

            window = std::make_unique<MainWindow> (containsArgument (commandLine, "--auto-quit"));
        }

        void shutdown() override
        {
            window.reset();
        }

        void systemRequestedQuit() override
        {
            quit();
        }

        void anotherInstanceStarted(const juce::String&) override {}

    private:
        std::unique_ptr<MainWindow> window;
    };
}

START_JUCE_APPLICATION(PilotApplication)
