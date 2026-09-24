/**
 * @file BridgeSelftest.h
 * @brief El selftest de siete direcciones del puente, compartido por la bancada
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
 *   6. ACCIONES: la accion de ficha de la pagina (RANDOM, la unica de
 *      `SECTION_ACTIONS`) se pulsa como la pulsa un usuario —SU boton, con su
 *      estado real— y el sorteo que hace el procesador se mide por su EFECTO, no
 *      por su estadistica: el APVTS tiene que haberse movido y el pie de la
 *      pagina —el estado NORMALIZADO que el host ya parsea— tiene que ensenar
 *      esos MISMOS numeros. Es la vuelta entera del canal (pagina -> nativo ->
 *      APVTS -> pagina) sobre una accion de ESTADO, que no viaja como edicion de
 *      parametro. Como el cajon de la matriz se queda ABIERTO a proposito, y con
 *      un modal delante la ficha no es alcanzable, la direccion lo cierra antes
 *      por su velo: el clic de fuera, que es el gesto que daria un usuario.
 *
 * Las direcciones 0 y 5 las exige la unica pagina que hay: la WebUI del plugin. Hasta el
 * ticket 8.4 el arnes podia declarar una direccion NO APLICABLE cuando el dueno servia la
 * exportacion retirada del piloto (`WebPilot/out`, anterior a la ficha MODELOS A-D y al
 * cajon de la matriz): esa maquinaria se fue con el piloto, porque no queda una segunda
 * pagina a la que rebajar el liston. Las siete direcciones son obligatorias en las dos
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
 * que se le lance a la sesion de un DAW que importe. La ultima direccion ADEMAS
 * sortea el timbre entero: pone `randomStrength` a 1, los tres `freeze*` a 0 y pulsa
 * el RANDOM de la pagina, asi que al terminar el APVTS no tiene los valores con los
 * que empezo. Los cuatro ficheros de prueba
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
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include "Main/NEURONiKProcessor.h"

 // Modelo temporal real embebido (juce_add_binary_data en CMakeLists.txt):
 // CZ-BASS1 analizado por la sonda del ModelMaker -> 4 frames v2.
 #include "NeuronikSelftestAssets.h"

#include "State/ParameterDefinitions.h"
 // La TABLA del sorteo: la direccion 6 no escribe a mano que parametros tienen que
 // moverse, los deriva del mismo sitio del que los saca el RANDOMIZE.
 #include "State/ParameterRandomizer.h"

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

    /** Clase del cajon ABIERTO (mueble compartido @abdsynths/shared); el script compone `.clase`. */
    inline constexpr const char* openDrawerClass = "drawer--open";

    /** Clase del velo VISIBLE que acompana a un cajon abierto (mueble compartido
     *  `@abdsynths/shared/components`): el
     *  cajon y su velo comparten id, y el script compone `.clase`. */
    inline constexpr const char* visibleBackdropClass = "drawer-backdrop--visible";

    /** Clase de una ruta del cajon (4 en la matriz, con `data-slot` 1..4); el script
     *  compone `.clase`. */
    inline constexpr const char* drawerSlotClass = "drawer-slot";

    /** Atributo que marca una CELDA de control; el script compone
     *  `[atributo="id"]` para elegir el control de un parametro concreto. */
    inline constexpr const char* parameterCellAttribute = "data-parameter-id";

    /** Atributo que marca el BOTON de una accion de ficha (el panel lo escribe como
     *  `dataset.action`); el script compone `[atributo="id"]`. */
    inline constexpr const char* actionButtonAttribute = "data-action";

    /** La accion de ficha RANDOM de `contracts/sections.js` (`SECTION_ACTIONS`): el
     *  sorteo lo hace el procesador, la pagina solo lo pide. */
    inline constexpr const char* randomizeAction = "randomize";

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
 * @brief La maquina de estados del selftest de siete direcciones.
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

    /** @brief Objetivos del sorteo que la pagina NO publica en su pie: `morphX` y
     *         `morphY` viven en el pad XY (`SECTION_VISUALS`), no en el estado que el
     *         lienzo serializa, asi que faltar esos dos es lo esperado. */
    static constexpr int numPadOnlyTargets = 2;

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
    enum class Stage { idle, waitForPage, matrix, models, nativeToPage, pageToNative, generalState, midi, actions, done };

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
            case Stage::actions:      return "ACCIONES";
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

        // Ranura A: modelo TEMPORAL real embebido (fase 10.5). Se escribe a
        // fichero y se revalida con el lector de produccion ANTES de entrar en
        // el engine: si el binario dejase de ser legible (formato v2 roto, swap
        // de asset...), la direccion lo grita aqui y no da OK mudo.
        {
            const juce::File temporalFile = directory.getChildFile (modelName (0) + ".neuronikmodel");
            temporalFile.replaceWithData (NeuronikSelftestAssets::CZBASS1temporal_neuronikmodel,
                                          (size_t) NeuronikSelftestAssets::CZBASS1temporal_neuronikmodelSize);

            const auto parsed = NEURONiK::Serialization::PresetManager::loadModelFromFile (temporalFile);
            const bool temporalOk = parsed.isValid && parsed.frameCount == 4;
            log (juce::String ("[selftest] MODELOS: asset temporal embebido -> ")
                 + (temporalOk ? "OK (4 frames v2, " + juce::String (temporalFile.getFileName()) + ")"
                               : "FAIL (formato v2 ilegible)"));

            const auto loaded = processor.loadModel (temporalFile, 0);
            log ("[selftest] MODELOS: " + temporalFile.getFileName() + " -> ranura 0: "
                 + (loaded ? "cargado (temporal v2)" : "RECHAZADO"));
        }

        for (int slot = 1; slot < 4; ++slot)
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

                                // ACCIONES va ULTIMA: su sorteo mueve los parametros
                                // de su tabla entera, asi que detras de las direcciones
                                // que miden valores concretos (GENERAL, MIDI) y nunca
                                // delante, o las invalidaria.
                                pressPageActions();
                            });
                        });
                    });
                });
            });
        });
    }

    // ========================================================================
    // 5. ACCIONES (la accion de ficha de la pagina: RANDOM)
    // ========================================================================

    /**
     * @brief Pulsa el RANDOM de la pagina y comprueba que el sorteo SE VE.
     *
     * @details Tres cosas, en este orden y por una razon cada una:
     *
     *   1. el sorteo obedece a `randomStrength` y a los tres `freeze*`, asi que se
     *      ponen en su posicion mas exigente (fuerza 1, nada congelado) por el
     *      APVTS — el mismo camino que un preset. El arnes ya declara en su cabecera
     *      que NO es neutro;
     *   2. se pulsa el BOTON de la pagina, con su etiqueta y su estado leidos EN LA
     *      PAGINA: si las acciones dejasen de habilitarse con host, esto lo dice. El
     *      cajon de la matriz que la direccion 0 dejo ABIERTO se cierra antes por su
     *      velo (el clic de fuera), porque con un modal delante la ficha no es
     *      alcanzable por un usuario;
     *   3. el EFECTO se mide dos veces: en el APVTS (su tabla entera tiene que haberse
     *      movido) y en el pie de la pagina (los MISMOS numeros). Un sorteo roto mueve
     *      cero; uno que no vuelve mueve el APVTS y deja el pie quieto.
     *
     * La tabla de objetivos sale de `State::getRandomizeTargets()`, no de una lista
     * escrita aqui: si el RANDOMIZE cambia de opinion sobre QUE sortea, esta direccion
     * mide lo nuevo sin tocar el arnes.
     */
    void pressPageActions()
    {
        stage = Stage::actions;

        // Fuerza entera y ningun congelado: asi "el sorteo movio el APVTS" es una
        // MEDIDA y no una casualidad (`readRandomizeStrength` recorta a 0..1 y
        // `readFreezeFlags` lee > 0.5, o sea que 1 y 0 son los extremos).
        setParameterReal (State::IDs::randomStrength, 1.0f);
        setParameterReal (State::IDs::freezeResonator, 0.0f);
        setParameterReal (State::IDs::freezeFilter, 0.0f);
        setParameterReal (State::IDs::freezeEnvelopes, 0.0f);

        beforeRandomize = readRandomizeValues();

        // Las cuatro escrituras de arriba llegan a la pagina en el siguiente poll del
        // dueno: mismo margen que las otras direcciones.
        afterDelay (400, [this]
        {
            evaluate (scriptPressRandomize(), [this] (const juce::String& raw)
            {
                const auto parsed = juce::JSON::parse (raw);
                const auto* object = parsed.getDynamicObject();
                const auto field = [object] (const char* key)
                {
                    return object != nullptr ? object->getProperty (key) : juce::var();
                };

                const auto error = field ("error").toString();
                const auto label = field ("label").toString();
                const auto closed = static_cast<int> (field ("closed"));
                const auto enabled = static_cast<int> (field ("enabled"));

                // El sorteo corre en el PROCESADOR y su efecto vuelve por el mismo canal
                // (parameterChanged) en el siguiente poll del dueno: el margen de
                // siempre, que aqui cubre DOS saltos (pulsacion -> APVTS -> pagina).
                afterDelay (600, [this, error, label, closed, enabled]
                {
                    const auto after = readRandomizeValues();
                    const auto total = static_cast<int> (after.size());
                    int moved = 0;

                    for (size_t index = 0; index < after.size(); ++index)
                        if (std::abs (after[index] - beforeRandomize[index]) > 1.0e-6f)
                            ++moved;

                    // Un objetivo puede caer, por azar, en el valor que ya tenia: la tabla
                    // sortea dentro de su INTENCION y el parametro redondea a su intervalo.
                    // Por eso se admite un par de quietos en vez de exigir los 23; un
                    // sorteo roto mueve 0 o 1, asi que la separacion sigue siendo clara.
                    const auto movedEnough = moved >= total - 2;

                    evaluate (scriptReadRandomizeValues(),
                              [this, error, label, closed, enabled, after, moved, total, movedEnough]
                              (const juce::String& pageRaw)
                    {
                        const auto pageParsed = juce::JSON::parse (pageRaw);
                        const auto* pageObject = pageParsed.getDynamicObject();
                        const auto* pageValues = pageObject != nullptr
                                                     ? pageObject->getProperty ("values").getDynamicObject()
                                                     : nullptr;
                        const auto& targets = State::getRandomizeTargets();

                        const auto missing = pageObject != nullptr
                                                 ? static_cast<int> (pageObject->getProperty ("missing"))
                                                 : static_cast<int> (targets.size());
                        int pageSeen = 0;
                        int mismatched = 0;
                        juce::String worst;

                        for (size_t index = 0; index < targets.size() && index < after.size(); ++index)
                        {
                            const auto id = juce::String (targets[index].id);

                            if (pageValues == nullptr || ! pageValues->hasProperty (id))
                                continue;

                            ++pageSeen;

                            const auto pageValue = static_cast<double> (pageValues->getProperty (id));
                            const auto nativeValue = static_cast<double> (after[index]);

                            if (std::abs (pageValue - nativeValue) > 1.0e-3)
                            {
                                ++mismatched;

                                if (worst.isEmpty())
                                    worst = id + ": pagina " + juce::String (pageValue, 4)
                                            + " vs APVTS " + juce::String (nativeValue, 4);
                            }
                        }

                        // El pie no lo ensena todo y no tiene por que: su lista de ids es el
                        // contrato de la PAGINA (los 70 del lienzo). Los dos objetivos del pad
                        // XY no estan ahi, asi que se admite que falten esos DOS — y ninguno
                        // mas: media tabla sin publicar es una pagina que dejo de enterarse.
                        const auto pageOk = pageSeen > 0 && missing <= numPadOnlyTargets
                                                && mismatched == 0;

                        const auto ok = error.isEmpty() && enabled == 1 && closed == 1
                                            && movedEnough && pageOk;

                        log ("[selftest] ACCIONES: RANDOM de la pagina (\"" + label + "\""
                             + (enabled == 1 ? ", habilitado" : ", DESHABILITADO")
                             + ") con el cajon de la matriz "
                             + (closed == 1 ? "cerrado" : "SIN CERRAR")
                             + "; APVTS movido en " + juce::String (moved) + "/" + juce::String (total)
                             + " objetivos; el pie publica " + juce::String (pageSeen)
                             + " de esos ids (" + juce::String (missing) + " sin publicar, "
                             + juce::String (numPadOnlyTargets) + " son del pad XY), "
                             + juce::String (mismatched) + " discrepancia(s)"
                             + (worst.isEmpty() ? juce::String() : " [" + worst + "]")
                             + (error.isEmpty() ? juce::String() : "  [" + error + "]")
                             + " -> " + (ok ? "OK" : "FAIL"));
                        actionsOk = ok;

                        finish (matrixDirectionOk() && modelsDirectionOk() && nativeToPageOk
                                    && pageToNativeOk && generalOk && midiOk && actionsOk);
                    });
                });
            });
        });
    }

    /** @brief Valores NORMALIZADOS del APVTS para la tabla del sorteo, en su orden. */
    std::vector<float> readRandomizeValues()
    {
        const auto& targets = State::getRandomizeTargets();
        std::vector<float> values;

        values.reserve (targets.size());

        for (const auto& target : targets)
        {
            if (const auto* parameter = processor.getAPVTS().getParameter (target.id))
                values.push_back (parameter->getValue());
            else
                values.push_back (-1.0f);   // la tabla y el APVTS los cruza su propio test
        }

        return values;
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
        // Calificado: con MODELOS en el centro (8.3) hay MAS de un cajon y el
        // primero del DOM ya no es el de la matriz.
        const auto trigger = juce::String ("'[") + SelftestPage::drawerTriggerAttribute
                           + "=\"modMatrix\"]'";
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

    /**
     * @brief Pulsa el RANDOM de la ficha (SU boton) y devuelve, en JSON, que vio.
     *
     * El cajon de la matriz se queda ABIERTO a proposito desde la direccion 0, y con un
     * modal delante la ficha no es alcanzable: primero se cierra por su VELO (el clic de
     * fuera, el mismo gesto que daria un usuario) y se informa de si quedo cerrado.
     * Despues se lee el boton de la accion —etiqueta y estado— ANTES de pulsarlo: un
     * boton deshabilitado no dispara su listener, y eso es justo lo que hay que ver si la
     * pagina deja de habilitar las acciones cuando no hay host.
     */
    static juce::String scriptPressRandomize()
    {
        const auto veil = juce::String ("'.") + SelftestPage::visibleBackdropClass + "'";
        const auto open = juce::String ("'.") + SelftestPage::openDrawerClass + "'";
        const auto button = juce::String ("'[") + SelftestPage::actionButtonAttribute
                          + "=\"" + SelftestPage::randomizeAction + "\"]'";

        return "(() => { try {"
               "  const veil = document.querySelector(" + veil + ");"
               "  if (veil) veil.click();"
               "  const stillOpen = document.querySelector(" + open + ");"
               "  const button = document.querySelector(" + button + ");"
               "  if (!button) return JSON.stringify({ error: 'NO_RANDOM_BUTTON' });"
               "  const seen = { closed: stillOpen ? 0 : 1, enabled: button.disabled ? 0 : 1,"
               "                 label: button.textContent };"
               "  button.click();"
               "  return JSON.stringify(seen);"
               " } catch (e) { return JSON.stringify({ error: e.message }); } })()";
    }

    /** @brief Lo que el pie de la pagina publica para la tabla del sorteo (JSON). */
    static juce::String scriptReadRandomizeValues()
    {
        juce::StringArray ids;

        for (const auto& target : State::getRandomizeTargets())
            ids.add (juce::String ("'") + target.id + "'");

        return "(() => { try {"
               "  const state = JSON.parse(document.querySelector('"
                    + juce::String (SelftestPage::stateCode) + "').textContent);"
               "  const ids = [" + ids.joinIntoString (",") + "];"
               "  const values = {}; let missing = 0;"
               "  for (const id of ids) {"
               "    if (typeof state[id] === 'number') values[id] = state[id]; else ++missing; }"
               "  return JSON.stringify({ values: values, missing: missing });"
               " } catch (e) { return 'RANDOM_FAIL: ' + e.message; } })()";
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
    bool actionsOk = false;   // el RANDOM de la pagina movio el APVTS y volvio pintado

    double startedAtMs = 0.0;
    float modWheelPageValue = -1.0f;
    std::vector<float> beforeRandomize;   // APVTS de la tabla del sorteo, antes de pulsar

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeSelftest)
};

} // namespace NEURONiK::WebUI
