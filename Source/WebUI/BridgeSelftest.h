/**
 * @file BridgeSelftest.h
 * @brief El selftest de seis direcciones del puente, compartido por la bancada
 *        (WebView2) y el editor del plugin (Fase 8, ticket 8.1 paso 2c).
 *
 * El arnes nacio DENTRO de `Source/WebPilotHost.cpp` (la bancada), que era la
 * unica superficie que hospedaba la pagina. El editor del plugin ya la hospeda
 * (paso 2), asi que el arnes se muda aqui y las dos superficies corren el MISMO
 * codigo en vez de una copia cada una — la misma razon por la que los tres
 * adaptadores viven en `BridgeAdapters.h`.
 *
 * Lo que comprueba, y por que son estas direcciones:
 *
 *   0. MATRIZ: pone la matriz de modulacion EN USO (una ruta real: fuente, destino y
 *      cantidad, por el APVTS, que es por donde la pondria un preset), abre el cajon
 *      lateral de esa ficha en la pagina y comprueba que el cajon muestra LAS DOS
 *      cosas: esta abierto con sus 4 rutas y 12 celdas, y sus controles ensenan la
 *      ruta configurada. Corre PRIMERO y deja el cajon ABIERTO a proposito: asi las
 *      direcciones siguientes pasan con la matriz en uso y el lienzo tapado, que es
 *      donde se rompe una UI (un anclaje que deja de ser el primero del documento, un
 *      cajon que roba el foco, un poll que deja de empujar...).
 *   1. NATIVO -> JS: mueve `masterLevel` por el APVTS (lo que haria un control
 *      nativo) y lee la posicion del slider de la pagina.
 *   2. JS -> NATIVO: dispara un `input` de verdad sobre ese slider (lo que
 *      produce un arrastre del usuario) y lee el APVTS.
 *   3. GENERAL: los 11 ids de la pestana GENERAL llegan al estado de la pagina
 *      con valor numerico (el pie de pagina los serializa como JSON).
 *   4. MIDI: una nota de la pagina llega al motor (FIFO de notas retenidas) y
 *      una rueda de modulacion inyectada en nativo se refleja en la pagina.
 *   5. MODELOS: cuatro `.neuronikmodel` (el dialecto JSON que escribe el Model
 *      Maker) entran por las ranuras A-D de ESTE proceso y la pagina —la de
 *      verdad, en su WebView— ensena los cuatro nombres en la ficha MODELOS
 *      A-D. Es la mitad "se ve en la pagina" de "cargar un modelo se ve y
 *      suena": la otra mitad (que el motor SUENE ese modelo) se mide en
 *      `Tests/ModelSlotTest.cpp`, porque un proceso con ventana no puede medir
 *      su propia salida de audio sin pelearse con el hilo de audio del host.
 *
 * Las direcciones 0 y 5 las exige la unica pagina que hay: la WebUI del plugin. Hasta el
 * ticket 8.4 el arnes podia declarar una direccion NO APLICABLE cuando el dueno servia la
 * exportacion retirada del piloto (`WebPilot/out`, anterior a la ficha MODELOS A-D y al
 * cajon de la matriz): esa maquinaria se fue con el piloto, porque no queda una segunda
 * pagina a la que rebajar el liston. Las seis direcciones son obligatorias en las dos
 * superficies, y el veredicto no necesita matices.
 *
 * No mueve el raton: usa el MISMO canal que usaria un usuario, que es lo que hace
 * que el veredicto signifique algo.
 *
 * OJO: un selftest no es neutro. Mueve parametros (el que comprueban las
 * direcciones 1-3), deja UNA RUTA DE LA MATRIZ CONFIGURADA (direccion 0), cambia de
 * motor y CARGA modelos en las cuatro ranuras del
 * APVTS (`modelPath<slot>`) — esto ultimo solo donde la pagina publica la ficha,
 * que en la otra superficie no se toca ni el APVTS ni el disco —, asi que no es algo
 * que se le lance a la sesion de un DAW que importe. Los cuatro ficheros de prueba
 * viven en el directorio temporal y se borran al terminar: no se toca ningun modelo
 * del usuario.
 *
 * El dueno del arnes decide cuando la pagina esta lista (`notifyPageReady`) y lo
 * bombea desde su timer (`tick`); el arnes no asume ningun formato de plugin. El
 * disparo (argv en el Standalone, variable de entorno en el VST3) y el destino
 * del veredicto (codigo de salida o log) son cosa del dueno, no del arnes.
 *
 * Todo es asincrono por diseno: cada salto es un `evaluateJavascript` con
 * callback o un `callAfterDelay`, asi que el hilo de mensajes nunca se bloquea.
 * De ahi las dos precauciones que la version de la bancada no necesitaba, porque
 * alli el proceso moria entero:
 *
 *   - un TIMEOUT (un hop perdido no puede dejar un editor colgado para siempre);
 *   - guarda de vida en TODOS los callbacks: dentro de un DAW el editor se puede
 *     cerrar con hops en vuelo, y un callback tardio no puede tocar memoria
 *     liberada (aqui el arnes muere antes que el navegador, no despues).
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include "Main/NEURONiKProcessor.h"

#include "State/ParameterDefinitions.h"

namespace NEURONiK::WebUI
{

/**
 * @brief Los anclajes de la pagina que el selftest consulta.
 *
 * Son un CONTRATO con la WebUI, no detalles de implementacion: la pagina los
 * publica a proposito y sus propios tests de vitest los fijan
 * (`WebUI/tests/panel.test.js`, `keyboard.test.js`, `paramStore.test.js`), para
 * que quitarlos rompa en la pagina ademas de aqui. El gemelo de este contrato en
 * C++ es `Tests/webuiSelftestContractTest.mjs`, que comprueba que cada anclaje de
 * esta lista sigue existiendo en `WebUI/src`.
 */
namespace SelftestPage
{
    /** El PRIMER `input[type=range]` del documento es `masterLevel`: el control
     *  base que la shell de la WebUI reserva a proposito para el selftest. */
    inline constexpr const char* firstRange = "input[type=range]";

    /** El pie de pagina con el estado NORMALIZADO serializado como JSON. */
    inline constexpr const char* stateCode = ".panel-footer code";

    /** La pestana del teclado (ahi viven las ruedas compartidas). */
    inline constexpr const char* keysTab = "[data-tab=\"keys\"]";

    /** La rueda de modulacion del teclado compartido (0..127). */
    inline constexpr const char* modWheelSlider = "#mod-wheel-container .kbd-wheel-slider";

    /** Atributo del disparador de un cajon lateral (lo que un usuario pulsa para
     *  abrirlo); el script compone el selector `[atributo]`. */
    inline constexpr const char* drawerTriggerAttribute = "data-drawer-trigger";

    /** Clase del cajon ABIERTO (`src/ui/drawer.js`); el script compone `.clase`. */
    inline constexpr const char* openDrawerClass = "drawer--open";

    /** Clase del velo VISIBLE que acompana a un cajon abierto (`src/ui/drawer.js`): el
     *  cajon y su velo comparten id, y el script compone `.clase`. */
    inline constexpr const char* visibleBackdropClass = "drawer-backdrop--visible";

    /** Clase de una ruta del cajon (4 en la matriz, con `data-slot` 1..4); el script
     *  compone `.clase`. */
    inline constexpr const char* drawerSlotClass = "drawer-slot";

    /** Atributo que marca una CELDA de control; el script compone
     *  `[atributo="id"]` para elegir el control de un parametro concreto. */
    inline constexpr const char* parameterCellAttribute = "data-parameter-id";

    /** Una fila de la ficha MODELOS A-D (una por ranura, con `data-slot`). */
    inline constexpr const char* modelSlotRow = ".model-slots__row";

    /** El nombre cargado dentro de esa fila (el dato que publica `modelsState`). */
    inline constexpr const char* modelSlotName = ".model-slots__name";

    /** El camino de envio de MIDI de la pagina.
     *  @note El nombre arrastra "pilot" de la era del piloto, pero es el que la
     *        pagina publica hoy (`WebUI/src/contracts/paramStore.js`). Renombrarlo
     *        es un cambio de la WebUI (y de sus tests), no de este ticket. */
    inline constexpr const char* midiHelper = "__pilotSendMidi";

    /** Los ids de la pestana GENERAL que tienen que estar en el estado. */
    inline constexpr const char* generalIds[] = {
        "engineType", "envAttack", "envDecay", "envSustain", "envRelease",
        "unisonDetune", "unisonSpread", "randomStrength",
        "freezeResonator", "freezeFilter", "freezeEnvelopes"
    };
}

/**
 * @class BridgeSelftest
 * @brief La maquina de estados del selftest de seis direcciones.
 *
 * Uso, desde el dueno (editor del plugin o bancada):
 *
 * @code
 *   selftest = std::make_unique<BridgeSelftest> (processor, evaluateFn, logFn, finishedFn);
 *   selftest->start();
 *   // en el timer:
 *   if (pageLoaded) selftest->notifyPageReady();
 *   selftest->tick();
 * @endcode
 */
class BridgeSelftest
{
public:
    /** @brief Ejecuta un script en la pagina y devuelve su resultado como texto
     *         (el dueno decide como hablar con su navegador). */
    using EvaluateFn = std::function<void (const juce::String& script,
                                           std::function<void (const juce::String&)> onResult)>;

    using LogFn = std::function<void (const juce::String& line)>;
    using FinishedFn = std::function<void (bool passed)>;

    /** @brief Presupuesto total. Un hop perdido termina en FAIL, nunca cuelga:
     *         en el VST3 el arnes corre dentro del editor del DAW. */
    static constexpr double timeoutMs = 30000.0;

    /** @brief Rutas de la matriz que la pagina monta en el cajon (mod1..mod4). */
    static constexpr int numMatrixSlots = 4;

    /** @brief Controles por ruta: fuente, destino y cantidad (ver `sections.js`). */
    static constexpr int numMatrixCellsPerSlot = 3;

    BridgeSelftest (NEURONiKProcessor& processorToUse,
                    EvaluateFn evaluateToUse,
                    LogFn logToUse,
                    FinishedFn finishedToUse)
        : processor (processorToUse),
          evaluateInPage (std::move (evaluateToUse)),
          log (std::move (logToUse)),
          onFinished (std::move (finishedToUse))
    {
    }

    ~BridgeSelftest()
    {
        // Los callbacks que quedaran en vuelo (una evaluacion del navegador, un
        // timer) comprueban esta bandera antes de tocar nada.
        lifetime->alive = false;
    }

    /** @brief Arranca el arnes. Idempotente. */
    void start()
    {
        if (started)
            return;

        started = true;
        stage = Stage::waitForPage;
        startedAtMs = juce::Time::getMillisecondCounterHiRes();

        // Los modelos entran ANTES de que la pagina este lista: el snapshot que el dueno
        // manda en `pageLoaded` ya los lleva, asi que la ficha MODELOS A-D nace con sus
        // cuatro nombres en vez de nacer en EMPTY y esperar a un segundo mensaje.
        loadSelfModels();

        log ("[selftest] waiting for the page to be ready...");
    }

    /** @brief La pagina ya cargo: a partir de aqui el arnes empieza a empujar. */
    void notifyPageReady() { pageReady = true; }

    /** @brief Late con el timer del dueno (el editor va a 33 Hz). */
    void tick()
    {
        if (! started || finished)
            return;

        if (juce::Time::getMillisecondCounterHiRes() - startedAtMs > timeoutMs)
        {
            log ("[selftest] TIMEOUT (" + juce::String (timeoutMs / 1000.0, 0)
                 + " s) en \"" + stageName() + "\" -> FAIL");
            finish (false);
            return;
        }

        if (stage == Stage::waitForPage && pageReady)
            openMatrixDrawer();
    }

    [[nodiscard]] bool isRunning() const noexcept { return started && ! finished; }
    [[nodiscard]] bool hasFinished() const noexcept { return finished; }
    [[nodiscard]] bool passed() const noexcept { return allOk; }

private:
    enum class Stage { idle, waitForPage, matrix, models, nativeToPage, pageToNative, generalState, midi, done };

    /** @brief La bandera de vida que comparten el arnes y sus callbacks. */
    struct Lifetime { bool alive = true; };

    /** @brief `callAfterDelay` con guarda de vida. */
    template <typename Fn>
    void afterDelay (int ms, Fn&& fn)
    {
        std::weak_ptr<Lifetime> weak = lifetime;

        juce::Timer::callAfterDelay (ms, [weak, fn = std::forward<Fn> (fn)]() mutable
        {
            if (auto locked = weak.lock())
                fn();
        });
    }

    /** @brief `evaluate` con guarda de vida. */
    void evaluate (const juce::String& script, std::function<void (const juce::String&)> onResult)
    {
        std::weak_ptr<Lifetime> weak = lifetime;

        evaluateInPage (script, [weak, onResult] (const juce::String& raw)
        {
            if (auto locked = weak.lock())
                onResult (raw);
        });
    }

    [[nodiscard]] const char* stageName() const noexcept
    {
        switch (stage)
        {
            case Stage::idle:         return "idle";
            case Stage::waitForPage:  return "espera de la pagina";
            case Stage::matrix:       return "MATRIZ";
            case Stage::models:       return "MODELOS";
            case Stage::nativeToPage: return "NATIVO -> JS";
            case Stage::pageToNative: return "JS -> NATIVO";
            case Stage::generalState: return "GENERAL";
            case Stage::midi:         return "MIDI";
            case Stage::done:         return "hecho";
        }

        return "?";
    }

    // ========================================================================
    // 0. MATRIZ: el cajon de la matriz, ABIERTO y con una ruta EN USO
    // ========================================================================

    /**
     * @brief Pone la matriz EN USO (una ruta de verdad) y abre su cajon en la pagina.
     *
     * @details Se corre PRIMERO y deja el cajon ABIERTO a proposito: las direcciones
     *          siguientes tienen que pasar con la matriz modulando y el lienzo tapado,
     *          que es donde se rompe una UI. La ruta se configura por el APVTS, que es
     *          por donde la pondria un preset, y los indices que se esperan en la
     *          pagina salen de las tablas de contrato (destino, que es append-only y
     *          cuyo orden ES estado de preset) y de la lista de fuentes, nunca escritos
     *          a mano aqui: si el destino cambia de sitio en la tabla, la direccion
     *          sigue midiendo lo mismo.
     */
    void openMatrixDrawer()
    {
        stage = Stage::matrix;

        // Ruta 1 = LFO 1 -> Filter Cutoff, cantidad +0.5 (real; -1..1).
        expectedSource = State::getModSources().indexOf ("LFO 1");
        expectedDestination = destinationIndexFor (State::IDs::filterCutoff);

        setParameterReal (State::IDs::mod1Source, (float) expectedSource);
        setParameterReal (State::IDs::mod1Destination, (float) expectedDestination);
        setParameterReal (State::IDs::mod1Amount, 0.5f);

        // La pagina se entera en el siguiente poll del dueno (30 ms): mismo margen que
        // las otras direcciones, quedarse corto antes que lento.
        afterDelay (400, [this]
        {
            evaluate (scriptOpenMatrixDrawer(), [this] (const juce::String& raw)
            {
                const auto parsed = juce::JSON::parse (raw);
                const auto* object = parsed.getDynamicObject();
                const auto field = [object] (const char* key)
                {
                    return object != nullptr ? object->getProperty (key) : juce::var();
                };

                const auto error = field ("error").toString();
                const auto opened = field ("opened").toString();
                const auto veil = field ("veil").toString();
                const auto slots = static_cast<int> (field ("slots"));
                const auto cells = static_cast<int> (field ("cells"));
                const auto source = static_cast<int> (field ("source"));
                const auto destination = static_cast<int> (field ("destination"));
                const auto amount = static_cast<double> (field ("amount"));

                // El cajon y su velo comparten id: si no coinciden, lo que se abrio no es
                // el dialogo que este disparador gobierna. `mod1Amount` = 0.5 real en un
                // rango -1..1 es 0.75 normalizado; el control compartido publica
                // normalizado, asi que se compara con margen en vez de exigir el texto.
                const auto ok = error.isEmpty()
                                    && opened.isNotEmpty() && opened == veil
                                    && slots == numMatrixSlots
                                    && cells == slots * numMatrixCellsPerSlot
                                    && source == expectedSource && destination == expectedDestination
                                    && amount > 0.5;

                log ("[selftest] MATRIZ: cajon \"" + opened + "\" (velo \"" + veil + "\") ABIERTO con "
                     + juce::String (slots) + " rutas y " + juce::String (cells)
                     + " celdas; ruta 1 = fuente " + juce::String (source) + " -> destino "
                     + juce::String (destination) + " (esperado " + juce::String (expectedSource)
                     + " -> " + juce::String (expectedDestination) + "), cantidad "
                     + juce::String (amount, 3)
                     + (error.isEmpty() ? juce::String() : "  [" + error + "]")
                     + " -> " + (ok ? "OK" : "FAIL"));
                matrixOk = ok;

                log ("[selftest] MATRIZ: el cajon se queda ABIERTO a proposito: las direcciones "
                     "siguientes corren con la matriz en uso y el lienzo tapado.");

                readModelSlots();
            });
        });
    }

    /** @brief Indice de preset del destino que mueve un parametro (-1 si no es destino). */
    static int destinationIndexFor (const char* parameterId)
    {
        const auto& table = State::getModDestinationTable();

        for (size_t index = 0; index < table.size(); ++index)
            if (table[index].parameterId != nullptr
                && juce::String (table[index].parameterId) == juce::String (parameterId))
                return static_cast<int> (index);

        return -1;
    }

    /** @brief Escribe un parametro en unidades REALES (indice, para las listas). */
    void setParameterReal (const char* parameterId, float realValue)
    {
        if (auto* parameter = processor.getAPVTS().getParameter (parameterId))
            parameter->setValueNotifyingHost (
                parameter->getNormalisableRange().convertTo0to1 (realValue));
    }

    // ========================================================================
    // 1. MODELOS: lo que este proceso ha cargado en A-D se lee en la pagina
    // ========================================================================

    void readModelSlots()
    {
        stage = Stage::models;

        // El snapshot viaja por el canal del navegador: la pagina tiene que recibirlo y
        // repintar antes de que tenga sentido preguntarle. Mismo margen que las otras
        // direcciones: quedarse corto antes que lento.
        afterDelay (400, [this]
        {
            evaluate (scriptReadModelSlots(), [this] (const juce::String& raw)
            {
                const auto expected = expectedSlotNames();
                const auto ok = raw == expected;

                log ("[selftest] MODELOS: la ficha A-D de la pagina muestra \"" + raw
                     + "\" (cargado en este proceso: \"" + expected + "\") -> "
                     + (ok ? "OK" : "FAIL"));
                modelsOk = ok;

                pushNativeToPage();
            });
        });
    }

    /** @brief Nombre con el que el arnes bautiza cada ranura (lo que la pagina ensena). */
    static juce::String modelName (int slot)
    {
        return "selftest-model-"
               + juce::String::charToString (static_cast<juce::juce_wchar> ('a' + slot));
    }

    /** @brief Los cuatro nombres en el orden del motor, unidos como los une el script. */
    static juce::String expectedSlotNames()
    {
        juce::StringArray names;

        for (int slot = 0; slot < 4; ++slot)
            names.add (modelName (slot));

        return names.joinIntoString ("|");
    }

    /** @brief Directorio temporal de los modelos de prueba (se borra al terminar). */
    static juce::File modelDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("neuronik-selftest-models");
    }

    /**
     * @brief Escribe un modelo por ranura y lo carga en A-D.
     *
     * El fichero es el JSON que exporta el Model Maker (64 parciales + offsets),
     * a proposito: si el plugin dejase de entender ESE formato, las ranuras se
     * quedarian mudas y esta direccion lo diria en voz alta en vez de dar OK con
     * cuatro ranuras vacias.
     */
    void loadSelfModels()
    {
        const auto directory = modelDirectory();
        directory.deleteRecursively();
        directory.createDirectory();

        for (int slot = 0; slot < 4; ++slot)
        {
            juce::DynamicObject::Ptr model = new juce::DynamicObject();
            juce::Array<juce::var> amplitudes;
            juce::Array<juce::var> offsets;

            for (int i = 0; i < 64; ++i)
            {
                amplitudes.add (i == slot * 2 ? 1.0f : 0.0f);   // un parcial distinto por ranura
                offsets.add (0.0f);
            }

            model->setProperty ("amplitudes", amplitudes);
            model->setProperty ("frequencyOffsets", offsets);
            model->setProperty ("name", modelName (slot));
            model->setProperty ("description", "Created with NEURONiK Model Maker");

            const auto file = directory.getChildFile (modelName (slot) + ".neuronikmodel");
            file.replaceWithText (juce::JSON::toString (juce::var (model.get())));

            const auto loaded = processor.loadModel (file, slot);

            log ("[selftest] MODELOS: " + file.getFileName() + " -> ranura "
                 + juce::String (slot) + ": " + (loaded ? "cargado" : "RECHAZADO"));
        }
    }

    // ========================================================================
    // 1. NATIVO -> JS
    // ========================================================================

    void pushNativeToPage()
    {
        stage = Stage::nativeToPage;

        // Por donde lo haria un control nativo: el valor del parametro, que es lo
        // que el puente publica en el siguiente tick del dueno.
        if (auto* parameter = processor.getAPVTS().getParameter ("masterLevel"))
            parameter->setValueNotifyingHost (0.25f);

        // La pagina se entera en el siguiente tick del poll del dueno (30 ms);
        // 400 ms es margen de sobra para quedarse corto antes que lento.
        afterDelay (400, [this]
        {
            evaluate (scriptReadFirstRange(), [this] (const juce::String& raw)
            {
                // La pagina guarda normalizado 0..1 y el control edita unidades
                // reales; en `masterLevel` las dos escalas coinciden (0..1, sin
                // skew), asi que el input lleva 0.25 salvo redondeo de texto.
                const auto pageValue = raw.getFloatValue();
                const auto ok = raw.isNotEmpty() && raw != "NO_SLIDER" && raw != "NO_RESULT"
                                    && std::abs (pageValue - 0.25f) < 0.02f;

                log ("[selftest] NATIVO -> JS: masterLevel nativo = 0.25, slider de la pagina = "
                     + raw + " -> " + (ok ? "OK" : "FAIL"));
                nativeToPageOk = ok;

                pushPageToNative();
            });
        });
    }

    // ========================================================================
    // 2. JS -> NATIVO
    // ========================================================================

    void pushPageToNative()
    {
        stage = Stage::pageToNative;

        // Un evento `input` de verdad, el mismo que dispara un arrastre del
        // usuario: la pagina lo trata por su camino normal (no hay atajo).
        evaluate (scriptSetFirstRange ("0.75"), [this] (const juce::String& raw)
        {
            // La pagina necesita un instante para re-renderizar y empujar por el
            // puente; el APVTS se aplica en el siguiente poll del dueno.
            afterDelay (600, [this, raw]
            {
                const auto* parameter = processor.getAPVTS().getParameter ("masterLevel");
                const auto nativeValue = parameter != nullptr ? parameter->getValue() : -1.0f;
                const auto ok = raw == "DISPATCHED" && std::abs (nativeValue - 0.75f) < 0.02f;

                log ("[selftest] JS -> NATIVO: slider de la pagina a 0.75, masterLevel nativo = "
                     + juce::String (nativeValue, 4) + " -> " + (ok ? "OK" : "FAIL"));
                pageToNativeOk = ok;

                pushGeneralState();
            });
        });
    }

    // ========================================================================
    // 3. GENERAL (los 11 ids de la pestana)
    // ========================================================================

    void pushGeneralState()
    {
        stage = Stage::generalState;

        if (auto* parameter = processor.getAPVTS().getParameter ("envAttack"))
            parameter->setValueNotifyingHost (0.5f);

        afterDelay (400, [this]
        {
            evaluate (scriptReadGeneralState(), [this] (const juce::String& raw)
            {
                const auto parsed = juce::JSON::parse (raw);
                const auto* object = parsed.getDynamicObject();
                const auto envAttack = object != nullptr
                                           ? static_cast<double> (object->getProperty ("envAttack"))
                                           : -1.0;
                const auto* missing = object != nullptr
                                          ? object->getProperty ("missing").getArray()
                                          : nullptr;
                const auto* bad = object != nullptr
                                      ? object->getProperty ("bad").getArray()
                                      : nullptr;

                const auto ok = object != nullptr
                                    && missing != nullptr && missing->isEmpty()
                                    && bad != nullptr && bad->isEmpty()
                                    && std::abs (envAttack - 0.5) < 0.02;

                log ("[selftest] GENERAL: ids de la pagina = " + juce::String (numGeneralIds)
                     + ", envAttack = " + juce::String (envAttack, 3)
                     + " (nativo 0.5) -> " + (ok ? "OK" : "FAIL"));
                generalOk = ok;

                pushMidi();
            });
        });
    }

    // ========================================================================
    // 4. MIDI (nota de la pagina -> motor, rueda de nativo -> pagina)
    // ========================================================================

    void pushMidi()
    {
        stage = Stage::midi;

        // Rueda de modulacion a 0.5 por donde la recibiria el MIDI externo.
        processor.injectController (1, 1, 64);

        afterDelay (600, [this]
        {
            // Cambia a KEYS (ahi viven las ruedas compartidas) y pulsa el DO central.
            evaluate (scriptKeysAndNoteOn (60, 0.9f), [this] (const juce::String& raw)
            {
                afterDelay (400, [this, raw]
                {
                    const auto held = processor.getHeldNotes();
                    const auto noteOnArrived = raw == "ON_SENT"
                        && std::find (held.begin(), held.end(), 60) != held.end();

                    evaluate (scriptReadModWheel(), [this, noteOnArrived] (const juce::String& modRaw)
                    {
                        // La rueda compartida pinta 0..127: se normaliza.
                        modWheelPageValue = (modRaw.isNotEmpty() && modRaw != "NO_MOD"
                                                 && modRaw != "NO_RESULT")
                                                ? modRaw.getFloatValue() : -1.0f;

                        evaluate (scriptNoteOff (60), [this, noteOnArrived] (const juce::String& offRaw)
                        {
                            afterDelay (400, [this, noteOnArrived, offRaw]
                            {
                                const auto held2 = processor.getHeldNotes();
                                const auto noteOffArrived = offRaw == "OFF_SENT"
                                    && std::find (held2.begin(), held2.end(), 60) == held2.end();

                                const auto ok = noteOnArrived && noteOffArrived
                                                    && modWheelPageValue >= 0.0f
                                                    && std::abs (modWheelPageValue - 0.5f) < 0.1f;

                                log ("[selftest] MIDI: nota 60 de la pagina en nativo = "
                                     + juce::String (noteOnArrived ? "on" : "STUCK")
                                     + "/" + juce::String (noteOffArrived ? "off" : "STUCK")
                                     + ", rueda de modulacion en la pagina = "
                                     + juce::String (modWheelPageValue, 2)
                                     + " (nativo 0.5) -> " + (ok ? "OK" : "FAIL"));
                                midiOk = ok;

                                finish (matrixDirectionOk() && modelsDirectionOk() && nativeToPageOk
                                            && pageToNativeOk && generalOk && midiOk);
                            });
                        });
                    });
                });
            });
        });
    }

    /** @brief Veredicto de MODELOS: los cuatro nombres de la ficha A-D, en la pagina. */
    [[nodiscard]] bool modelsDirectionOk() const noexcept { return modelsOk; }

    /** @brief Veredicto de MATRIZ: el cajon abierto, con la ruta configurada, en la pagina. */
    [[nodiscard]] bool matrixDirectionOk() const noexcept { return matrixOk; }

    void finish (bool passed)
    {
        if (finished)
            return;

        finished = true;
        allOk = passed;
        stage = Stage::done;

        // Los modelos de prueba no se quedan en el disco del usuario ni cuando el
        // veredicto es FAIL o TIMEOUT: la ficha de la pagina los marca divergentes en
        // cuanto desaparecen, que es justo lo que hay que ver al reabrir el editor.
        modelDirectory().deleteRecursively();

        // El veredicto se basta con la palabra: desde el ticket 8.4 no hay direcciones
        // declaradas no aplicables, asi que un OK es un OK de las seis direcciones.
        log (juce::String ("[selftest] RESULT: ") + (passed ? "OK" : "FAIL"));

        if (onFinished != nullptr)
            onFinished (passed);
    }

    // ========================================================================
    // Los scripts. Viven aqui, en un solo sitio, porque son el contrato con la
    // pagina; los anclajes salen de `SelftestPage`.
    // ========================================================================

    static juce::String scriptReadFirstRange()
    {
        const auto anchor = juce::String ("'") + SelftestPage::firstRange + "'";

        return "document.querySelector(" + anchor + ")"
               " ? String(document.querySelector(" + anchor + ").value)"
               " : 'NO_SLIDER'";
    }

    static juce::String scriptSetFirstRange (const juce::String& value)
    {
        const auto anchor = juce::String ("'") + SelftestPage::firstRange + "'";

        return "(() => { const s = document.querySelector(" + anchor + ");"
               " if (!s) return 'NO_SLIDER';"
               " const setter = Object.getOwnPropertyDescriptor(window.HTMLInputElement.prototype,'value').set;"
               " setter.call(s, '" + value + "');"
               " s.dispatchEvent(new Event('input', { bubbles: true }));"
               " return 'DISPATCHED'; })()";
    }

    static juce::String scriptReadGeneralState()
    {
        juce::StringArray ids;

        for (const auto* id : SelftestPage::generalIds)
            ids.add (juce::String ("'") + id + "'");

        return "(() => { try {"
               "  const state = JSON.parse(document.querySelector('"
                    + juce::String (SelftestPage::stateCode) + "').textContent);"
               "  const ids = [" + ids.joinIntoString (",") + "];"
               "  return JSON.stringify({"
               "    missing: ids.filter((id) => !(id in state)),"
               "    bad: ids.filter((id) => typeof state[id] !== 'number')"
               "             .map((id) => id + '=' + String(state[id])),"
               "    envAttack: state.envAttack });"
               " } catch (e) { return 'GENERAL_FAIL: ' + e.message; } })()";
    }

    static juce::String scriptKeysAndNoteOn (int note, float velocity)
    {
        return "(() => { try {"
               "  document.querySelector('" + juce::String (SelftestPage::keysTab) + "')?.click();"
               "  if (!window." + juce::String (SelftestPage::midiHelper) + ") return 'NO_MIDI_HELPER';"
               "  window." + juce::String (SelftestPage::midiHelper) + "({ action: 'midiNoteOn', note: "
                    + juce::String (note) + ", velocity: " + juce::String (velocity, 2) + " });"
               "  return 'ON_SENT';"
               " } catch (e) { return 'MIDI_FAIL: ' + e.message; } })()";
    }

    static juce::String scriptNoteOff (int note)
    {
        return "(() => { try {"
               "  window." + juce::String (SelftestPage::midiHelper) + "({ action: 'midiNoteOff', note: "
                    + juce::String (note) + " });"
               "  return 'OFF_SENT';"
               " } catch (e) { return 'MIDI_FAIL: ' + e.message; } })()";
    }

    /** @brief El selector de la celda de un parametro: `[data-parameter-id="id"]`. */
    static juce::String cellSelector (const char* parameterId)
    {
        // El anclaje es el ATRIBUTO (es el token que el contract-test pincha por los dos
        // lados); el selector lo compone este script, que es quien lo usa:
        //   atributo "data-parameter-id" + id -> [data-parameter-id="mod1Source"]
        return juce::String ("[") + SelftestPage::parameterCellAttribute
                   + "=\"" + juce::String (parameterId) + "\"]";
    }

    /**
     * @brief Abre el cajon de la matriz y devuelve, en JSON, lo que la pagina ensena.
     *
     * El clic es el MISMO que daria un usuario (el disparador de la ficha), no una
     * clase puesta a mano: si el disparador deja de abrir el cajon, esto lo dice. Los
     * controles se leen por el `data-parameter-id` de su celda: la fuente y el destino
     * como `selectedIndex` del `<select>` nativo, la cantidad en el `aria-valuenow`
     * (normalizado) del dial del `Knob` compartido.
     */
    static juce::String scriptOpenMatrixDrawer()
    {
        const auto trigger = juce::String ("'[") + SelftestPage::drawerTriggerAttribute + "]'";
        const auto open = juce::String ("'.") + SelftestPage::openDrawerClass + "'";
        const auto slot = juce::String ("'.") + SelftestPage::drawerSlotClass + "'";
        const auto veil = juce::String ("'.") + SelftestPage::visibleBackdropClass + "'";

        return "(() => { try {"
               "  const trigger = document.querySelector(" + trigger + ");"
               "  if (!trigger) return JSON.stringify({ error: 'NO_TRIGGER' });"
               "  trigger.click();"
               "  const drawer = document.querySelector(" + open + ");"
               "  if (!drawer) return JSON.stringify({ error: 'NO_DRAWER_OPEN' });"
               "  const veil = document.querySelector(" + veil + ");"
               // Los controles se buscan DENTRO del cajon abierto: si alguno estuviera
               // fuera, el cajon no estaria ensenando la ruta.
               "  const choice = (selector) => { const el = drawer.querySelector(selector);"
               "    return el ? el.selectedIndex : -1; };"
               "  const knob = (selector) => { const el = drawer.querySelector(selector);"
               "    return el ? Number(el.getAttribute('aria-valuenow')) : -1; };"
               "  return JSON.stringify({"
               "    opened: drawer.dataset.drawer,"
               "    veil: veil ? veil.dataset.drawerBackdrop : '',"
               "    slots: drawer.querySelectorAll(" + slot + ").length,"
               "    cells: drawer.querySelectorAll('.cell').length,"
               "    source: choice('" + cellSelector (State::IDs::mod1Source) + " select'),"
               "    destination: choice('" + cellSelector (State::IDs::mod1Destination) + " select'),"
               "    amount: knob('" + cellSelector (State::IDs::mod1Amount) + " .abd-knob__dial'),"
               "  });"
               " } catch (e) { return JSON.stringify({ error: e.message }); } })()";
    }

    static juce::String scriptReadModelSlots()
    {
        return "(() => {"
               "  const rows = document.querySelectorAll('" + juce::String (SelftestPage::modelSlotRow) + "');"
               "  if (rows.length !== 4) return 'SLOTS=' + rows.length;"
               "  const names = [];"
               "  for (const row of rows) { const name = row.querySelector('"
                    + juce::String (SelftestPage::modelSlotName) + "');"
               "    names.push(name ? name.textContent : 'NO_NAME'); }"
               "  return names.join('|'); })()";
    }

    static juce::String scriptReadModWheel()
    {
        return "(() => { const el = document.querySelector('"
                    + juce::String (SelftestPage::modWheelSlider) + "');"
               " return el ? String(Number(el.value) / 127) : 'NO_MOD'; })()";
    }

    static constexpr int numGeneralIds = (int) (sizeof (SelftestPage::generalIds)
                                                / sizeof (SelftestPage::generalIds[0]));

    NEURONiKProcessor& processor;
    EvaluateFn evaluateInPage;
    LogFn log;
    FinishedFn onFinished;

    std::shared_ptr<Lifetime> lifetime = std::make_shared<Lifetime>();

    Stage stage = Stage::idle;
    bool started = false;
    bool pageReady = false;
    bool finished = false;
    bool allOk = false;

    bool matrixOk = false;        // el cajon abierto, con la ruta configurada, en la pagina
    bool modelsOk = false;        // los cuatro nombres de las ranuras A-D en la pagina
    int expectedSource = -1;      // indices de la ruta 1, derivados de las tablas del contrato
    int expectedDestination = -1;
    bool nativeToPageOk = false;
    bool pageToNativeOk = false;
    bool generalOk = false;   // los 11 ids en la pagina + propagacion nativo -> pagina
    bool midiOk = false;      // nota de la pagina en el motor + rueda de nativo en la pagina

    double startedAtMs = 0.0;
    float modWheelPageValue = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeSelftest)
};

} // namespace NEURONiK::WebUI
