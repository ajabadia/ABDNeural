/**
 * @file BridgeSelftest.h
 * @brief El selftest de diez direcciones del puente, compartido por la bancada
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
 *   1b. ENV-RUTAS: pulsa la ruta ENV de la ficha ENVOLVENTES (un <button> del
 *      lienzo) y mide el salto completo a la MATRIZ: cajon ABIERTO con su velo,
 *      SU slot resaltado y los controles del cajon ensenando la ruta que dice el
 *      APVTS. La ruta se DERIVA del APVTS (el primer slot con fuente ENV), no
 *      escrita a mano. Corre CON el cajon de la direccion 0 delante y demuestra
 *      ademas que el opener cierra al hermano y abre el suyo.
 *   1c. AGUJA: una nota de la pagina SUENA en el motor y sus DOS envolventes se
 *      VEN: el frame envelopes[amp, filter] de la telemetria pinta la aguja
 *      horizontal sobre cada curva ADSR (data-visible + 'd' del path), en las
 *      DOS vistas (lienzo y cajon). Tres medidas: ocultas en silencio, visibles
 *      con nivel alto en el sustain (cruzado con getEnvelopeLevelsForUI: dos
 *      caras del mismo canal) y ocultas tras el release — la aguja no se queda
 *      clavada cuando la nota muere.
 *   1d. NATIVO -> JS: mueve `masterLevel` por el APVTS (lo que haria un control
 *      nativo) y lee la posicion del slider de la pagina.
 *   2. JS -> NATIVO: dispara un `input` de verdad sobre ese slider (lo que
 *      produce un arrastre del usuario) y lee el APVTS.
 *   3. GENERAL: los 11 ids de la pestana GENERAL llegan al estado de la pagina
 *      con valor numerico (el pie de pagina los serializa como JSON).
 *   4. MIDI: una nota de la pagina llega al motor (FIFO de notas retenidas) y
 *      una rueda de modulacion inyectada en nativo se refleja en la pagina.
 *   5. MODELOS: los SEIS `.neuronikmodel` del banco CZ101 que viajan embebidos se
 *      escriben al directorio temporal del arnes, se releen con el lector de
 *      produccion (los dos dialectos del v2: denso de 1 frame y con f0 por frame
 *      de 4) y los cuatro del banco entran por las ranuras A-D de ESTE proceso;
 *      la pagina —la de verdad, en su WebView— ensena los cuatro nombres en la
 *      ficha MODELOS A-D. Es la mitad "se ve en la pagina" de "cargar un modelo
 *      se ve y suena": la otra mitad (que el motor SUENE ese modelo) se mide en
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
 *   8. ZRING: el anillo morph-Z GIRANDO con la matriz por el camino NATIVO del
 *      plugin: la ruta LFO 2 -> Morph Z (destino 28) se escribe por el APVTS
 *      (como la dejaria un preset), el motor REAL del plugin la aplica en su
 *      render de audio y la telemetria (frame.modulation[28] -> setZMod) pinta
 *      el arco .zring-mod. Se muestrea el arco en la pagina viva (evaluate
 *      siacrono), se reconstruye el PERIODO entre cristas (~1000 ms a 1 Hz) y
 *      se hace el control negativo A/B/A: fuente Off clava el arco en 0 y al
 *      volver LFO 2 se reanuda. Es la otra mitad de la ruta que el motor local
 *      del navegador ya tiene pineada en _t59_e2e_zring.mjs: dos caminos, una
 *      sola verdad.
 *   7. MORPH: el pad XY y el anillo morph-Z con el modelo REAL de fabrica dentro.
 *      Carga `CZ-SWEP1.neuronikmodel` en la ranura D —el fichero que instala el
 *      plugin y al que apunta el `modelPath3` del preset de banco CZ101-BANK— y
 *      lo busca en la ESQUINA del pad, que sale de la misma fuente que la ficha
 *      MODELOS A-D: si el morph y las ranuras dejan de hablar del mismo motor,
 *      esta direccion lo dice. Despues mide las tres caras del pad: que pinte lo
 *      que dice el APVTS (motor -> pagina), que un arrastre del pad y un gesto
 *      del aro dejen los tres parametros donde el gesto los dejo (pagina ->
 *      motor) y —lo que ningun test de manejadores puede ver— que el HIT-TESTING
 *      de la pagina viva deje el centro del pad en el pad y el trazo del aro en
 *      el aro: el overlay del aro cubria el pad entero y se comia sus gestos.
 *
 * Las direcciones 0 y 5 las exige la unica pagina que hay: la WebUI del plugin. Hasta el
 * ticket 8.4 el arnes podia declarar una direccion NO APLICABLE cuando el dueno servia la
 * exportacion retirada del piloto (`WebPilot/out`, anterior a la ficha MODELOS A-D y al
 * cajon de la matriz): esa maquinaria se fue con el piloto, porque no queda una segunda
 * pagina a la que rebajar el liston. Las direcciones son obligatorias en las dos
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
 * sortea el timbre DOS veces: la primera con `randomStrength` a 1 y los tres
 * `freeze*` a 0 —para que "el sorteo movio el APVTS" sea una medida y no una
 * casualidad—, y la segunda con `freezeResonator` a 1 —para que "el sorteo respeta
 * los congelados" tambien lo sea: con el banco espectral congelado, su mitad de la
 * tabla no puede moverse y la otra mitad si. El congelado se devuelve a 0 al acabar
 * de medir, pero el APVTS se queda sorteado, asi que al terminar no tiene los valores
 * con los que empezo. Los SEIS assets del banco se escriben al directorio temporal
 * del arnes y se borran al terminar: no se toca ningun modelo del usuario. La ultima direccion si carga un modelo REAL —el de fabrica, en la
 * ranura D— porque es lo que hay que ver en el pad: es el mismo fichero que resuelve
 * un preset, no una copia, y `installFactoryModels()` nunca regraba lo que el usuario
 * ya tiene en ese directorio.
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

 // Los SEIS modelos del banco CZ101 embebidos (juce_add_binary_data en
 // CMakeLists.txt), analizados por la sonda del ModelMaker desde los WAV reales:
 // cuatro estaticos v2 (1 frame) y dos temporales v2 (4 frames con f0 por frame).
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

    /** La vista del pad XY de la ficha MODELOS, por el numero con el que
     *  `ui/xyPad.js` la marca (`dataset.visual`); el script compone
     *  `[data-visual="..."]` y busca el pad DENTRO de ella. */
    inline constexpr const char* padVisual = "model-xy";

    /** La SUPERFICIE del pad (`@abdsynths/shared` xypad.js): el elemento que
     *  escucha el arrastre y publica el valor pintado (`aria-valuenow`,
     *  `aria-valuetext`) y el pulgar que lo pinta. */
    inline constexpr const char* padSurface = ".abd-xypad__pad";

    /** El aro morph-Z de esta pagina (overlay propio, fase 10.3). */
    inline constexpr const char* zRing = ".xy-pad__zring";

    /** El arco de VALOR del aro: `stroke-dasharray` de 0 a 100 por `pathLength`. */
    inline constexpr const char* zRingFill = ".zring-fill";

    /** El arco de MODULACION del aro (setZMod): la contribucion con signo de
        la matriz sobre morphZ, pintada de la base al valor EFECTIVO. */
    inline constexpr const char* zRingMod = ".zring-mod";

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
 * @brief La maquina de estados del selftest de ocho direcciones.
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
    // 60 s: la colecta del arco ZRING (evaluate siacrono x N sobre la pagina
    // viva) anade segundos reales al final del arnes y el presupuesto de 30 s
    // de las direcciones previas se quedaba corto.
    static constexpr double timeoutMs = 90000.0;

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
    void notifyPageReady()
    {
        if (pageReady)
            return;

        pageReady = true;

        // El presupuesto (30 s) es para los SALTOS, asi que empieza cuando la
        // pagina puede contestar. Contado desde `start()` se lo comia la carga
        // de la pagina -medido: 14 s- y el FAIL lo pagaba la ultima direccion
        // (MORPH) por un retraso que no era suyo. La cota no se pierde: sigue
        // habiendo 30 s desde aqui, y un hop perdido acaba en veredicto.
        startedAtMs = juce::Time::getMillisecondCounterHiRes();
    }

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
    enum class Stage { idle, waitForPage, matrix, models, envRoutes, needle, nativeToPage, pageToNative, generalState, midi, actions, morph, zring, done };

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
            case Stage::envRoutes:    return "ENV-RUTAS";
            case Stage::needle:       return "AGUJA";
            case Stage::nativeToPage: return "NATIVO -> JS";
            case Stage::pageToNative: return "JS -> NATIVO";
            case Stage::generalState: return "GENERAL";
            case Stage::midi:         return "MIDI";
            case Stage::actions:      return "ACCIONES";
            case Stage::morph:        return "MORPH";
            case Stage::zring:        return "ZRING";
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

                envRouteJump();

                // NOTA: needleDirection() corre despues de envRouteJump() (su
                // callback de veredicto encadena pushNativeToPage(); AGUJA se
                // engancha alli para que su nota suene con el modelo ya cargado).
            });
        });
    }

    // ========================================================================
    // 1b. ENV-RUTAS: las rutas clicables de ENVOLVENTES abren la MATRIZ en SU slot
    // ========================================================================

    /**
     * @brief Pulsa la ruta ENV de la ficha ENVOLVENTES y mide el salto a la MATRIZ.
     *
     * @details La ficha ENVOLVENTES pinta, bajo cada curva, las rutas que la matriz
     *          tiene asignadas a SU fuente (ENV 1 = fuente 6, ENV 2 = fuente 7) y
     *          cada fila es un BOTON que abre el cajon de la MATRIZ resaltando SU
     *          slot. Es el unico gesto del lienzo que cruza dos fichas, y el selftest
     *          no lo cubria: si el cable del opener se pierde (un repintado que
     *          reconstruye la vista, un cambio del contrato de slots), el clic se
     *          queda mudo y nadie lo sabe.
     *
     *          La ruta que se pulsa se DERIVA del APVTS (que es por donde la pondria
     *          un preset), nunca escrita a mano: busca el primer slot mod1..mod4 con
     *          fuente = ENV y toma SU slot (1..4) y SU fila pintada. La direccion 0
     *          configuro mod1 como LFO 1 -> Filter Cutoff, asi que aqui se pulsa la
     *          ruta de ENV (la de fabrica: mod2 = ENV 2 -> Filter Cutoff) y el slot
     *          esperado es el que el APVTS dice, no un literal.
     *
     *          Lo que se mide, en el MISMO viaje: la fila existe y es clicable, el
     *          cajon de la MATRIZ queda ABIERTO (con su velo), el slot resaltado es
     *          EL del boton pulsado y los controles del cajon ensenan la ruta que
     *          dice el APVTS. Como la direccion 0 deja su cajon abierto a proposito,
     *          este salto se hace CON un modal delante — y demuestra ademas que el
     *          opener cierra al hermano y abre el suyo (el mismo gesto que daria un
     *          usuario con dos cajones en pantalla).
     */
    void envRouteJump()
    {
        stage = Stage::envRoutes;

        // El OPINION del APVTS no sirve aqui: el Standalone de JUCE restaura el
        // filterState de la ultima sesion ANTES del selftest, y esa sesion puede
        // traer la matriz del usuario (fabrica: mod1 = ENV 1, mod2 = ENV 2; la
        // sesion del arnes solo ensena mod1 = LFO 1). Lo que decide el gesto es
        // lo que la pagina PINTA (su snapshot): el script busca UNA fila ENV
        // pintada, la pulsa y devuelve SU slot — el APVTS se lee DESPUES, para
        // cruzar que el cajon ensena lo mismo que el motor tiene. Sin hardcodeo:
        // ninguna ruta ni indice escritos a mano (los ENV salen de getModSources()).
        const auto env1 = State::getModSources().indexOf ("ENV 1");
        const auto env2 = State::getModSources().indexOf ("ENV 2");

        afterDelay (400, [this, env1, env2]
        {
            evaluate (scriptEnvRouteJump (env1, env2), [this, env1, env2]
                      (const juce::String& raw)
            {
                const auto parsed = juce::JSON::parse (raw);
                const auto* object = parsed.getDynamicObject();
                const auto field = [object] (const char* key)
                {
                    return object != nullptr ? object->getProperty (key) : juce::var();
                };

                const auto error = field ("error").toString();
                const auto rows = static_cast<int> (field ("rows"));
                const auto clicked = static_cast<int> (field ("clicked"));
                const auto opened = field ("opened").toString();
                const auto veil = field ("veil").toString();
                const auto highlighted = static_cast<int> (field ("highlighted"));
                const auto wasOpenBefore = field ("wasOpenBefore").toString();

                // Lo que el APVTS dice del slot PULSADO (cruzado con lo que el
                // cajon ensena: motor y pagina tienen que contar lo mismo).
                const auto sourceId = juce::String ("mod") + juce::String (clicked) + "Source";
                const auto destinationId = juce::String ("mod") + juce::String (clicked) + "Destination";
                int apvtsSource = -1;
                int apvtsDestination = -1;

                if (clicked >= 1 && clicked <= numMatrixSlots)
                {
                    if (auto* source = processor.getAPVTS().getParameter (sourceId))
                        apvtsSource = (int) std::round (
                            source->getNormalisableRange().convertFrom0to1 (source->getValue()));

                    if (auto* destination = processor.getAPVTS().getParameter (destinationId))
                        apvtsDestination = (int) std::round (
                            destination->getNormalisableRange().convertFrom0to1 (destination->getValue()));
                }

                const auto pageSource = static_cast<int> (field ("source"));
                const auto pageDestination = static_cast<int> (field ("destination"));

                // La fila tiene que ser de una fuente ENV (6/7), el cajon abierto
                // es el de la MATRIZ con su velo, y el slot resaltado es EL del
                // boton. El cajon ensena la MISMA ruta que el APVTS tiene en ese
                // slot. wasOpenBefore=true (la direccion 0 deja el suyo abierto):
                // el gesto demuestra ademas que el opener cierra al hermano.
                const auto isEnvRoute = apvtsSource == env1 || apvtsSource == env2;
                const auto ok = error.isEmpty()
                                    && rows >= 1 && clicked >= 1 && clicked <= numMatrixSlots
                                    && isEnvRoute
                                    && opened == "drawer-modMatrix" && opened == veil
                                    && highlighted == clicked
                                    && pageSource == apvtsSource
                                    && pageDestination == apvtsDestination;

                log ("[selftest] ENV-RUTAS: " + juce::String (rows)
                     + " fila(s) ENV pintadas; clic en la del slot " + juce::String (clicked)
                     + " -> cajon \"" + opened + "\" (velo \"" + veil + "\"), slot resaltado "
                     + juce::String (highlighted) + ", cajon ensenando " + juce::String (pageSource)
                     + " -> " + juce::String (pageDestination) + " (APVTS: " + juce::String (apvtsSource)
                     + " -> " + juce::String (apvtsDestination) + ")"
                     + (wasOpenBefore.isNotEmpty() ? " [cajon previo: " + wasOpenBefore + ", cerrado por el salto]" : juce::String())
                     + (error.isEmpty() ? juce::String() : "  [" + error + "]")
                     + " -> " + (ok ? "OK" : "FAIL"));
                envRoutesOk = ok;

                // El cajon de la MATRIZ queda ABIERTO (era el estado que la direccion
                // 0 ya queria); las direcciones siguientes siguen con el modal delante.
                // La AGUJA va detras: necesita el motor SONANDO (la nota entra por el
                // teclado de la pagina) y el modelo ya en las ranuras.
                needleDirection();
            });
        });
    }

    // ========================================================================
    // 1c. AGUJA: los niveles REALES de las envolventes pintan sobre las curvas
    // ========================================================================

    /**
     * @brief Pulsa una nota, deja que el motor SUENE y mide las agujas de las
     *        curvas ADSR (envelopes[amp, filter] del frame de telemetria).
     *
     * @details El motor publica, en cada processBlock, el nivel de las dos
     *          envolventes (uiEnvelope/uiFEnvelope) y el puente los manda en el
     *          frame `envelopes: [amp, filter]` a ~15 Hz. La pagina los pinta como
     *          una AGUJA horizontal sobre cada curva (data-visible + el 'd' del
     *          path): indice 0 -> ENV 1 (amplitud), indice 1 -> ENV 2 (filtro), en
     *          las DOS vistas (lienzo y cajon). Es la mitad "se ve" de "el motor
     *          suena y la pagina lo ensena" — y su fallo clasico es silencioso:
     *          un frame que deja de viajar o un setLevel que se pierde no da error,
     *          la curva simplemente se queda sin aguja.
     *
     *          La nota entra por el MISMO helper de la pagina que usa la direccion
     *          MIDI (el teclado de la pagina: el gesto de un usuario), el motor
     *          corre con audio real (el Standalone/VST3 tienen dispositivo; sin
     *          el, las voces no avanzan y la aguja no aparece). Se mide tres veces:
     *          oculta en silencio, VISIBLE durante el sustain (nivel alto en las
     *          DOS agujas: attack/decay cortos y sustain alto de fabrica) y oculta
     *          tras el release. El nivel que pinta la aguja se cruza con el que el
     *          motor publica (getEnvelopeLevelsForUI) por el mismo margen del canal:
     *          dos caras del mismo numero, medidas por caminos distintos.
     */
    void needleDirection()
    {
        stage = Stage::needle;

        // En silencio las agujas tienen que estar ocultas: el frame existe (a ~15
        // Hz desde el arranque) pero su nivel <= NEEDLE_FLOOR y la vista lo esconde.
        evaluate (scriptReadNeedles(), [this] (const juce::String& silentRaw)
        {
            // El piño entra por el teclado de la pagina (el gesto de un usuario,
            // mismo camino que la direccion MIDI). ADSR de fabrica (attack 10 ms,
            // decay 100 ms, sustain 0.7): sostenida, vive en el sustain.
            evaluate (scriptKeysAndNoteOn (60, 0.9f), [this, silentRaw] (const juce::String& onRaw)
            {
                // La nota ya esta en el FIFO del motor desde este momento; el
                // camino pagina->motor->telemetria->pagina tiene latencia real
                // (el poll del dueno, el diff de frames, el render) que NO se
                // apuesta a un sleep: se SONDEA con margen (patron ZRING).
                needleWaitingVisible = true;
                needleSample (30, [this, silentRaw, onRaw]
                {
                    evaluate (scriptNoteOff (60), [this, silentRaw, onRaw] (const juce::String& offRaw)
                    {
                        needleWaitingVisible = false;
                        // El release de la ADSR de la SESION puede durar mas que
                        // el de fabrica (500 ms): presupuesto de 4.5 s (150 tomas),
                        // con salida temprana en cuanto las cuatro se escondan.
                        needleSample (150, [this, silentRaw, onRaw, offRaw]
                        {
                            const auto silent = parseNeedles (silentRaw);
                            const auto held = parseNeedles (needleHeldReading);
                            const auto released = parseNeedles (needleReleasedReading);

                            const auto noteEntered = onRaw == "ON_SENT";
                            const auto noteLeft = offRaw == "OFF_SENT";

                            // 1. SILENCIO: sin nota, las cuatro agujas ocultas (el
                            //    frame viaja, el nivel esta bajo el suelo).
                            // 2. SOSTENIDO: las cuatro VISIBLES con nivel alto, y el
                            //    nivel que la pagina ensena es el que el MOTOR publica
                            //    (getEnvelopeLevelsForUI: dos caras del mismo canal).
                            //    La coherencia pagina<->motor manda sobre el umbral
                            //    absoluto (el preset de la sesion manda la ADSR; el
                            //    attack de 10 ms deja la ENV 1 bajando antes de que
                            //    la primera telemetria llegue): mismas senales, dos
                            //    caminos, margen del diff del puente (~1/255).
                            const auto hiddenWhenSilent = needlesHidden (silent);
                            const auto coherentAmp = held.ampLevel > 0.0
                                && std::abs (held.ampLevel - needleNativeAmp) < 0.08;
                            const auto coherentFilter = held.filterLevel > 0.0
                                && std::abs (held.filterLevel - needleNativeFilter) < 0.08;
                            // Sin puerta absoluta: la ADSR de la sesion manda (su
                            // sustain puede vivir bajo 0.2); lo que se exige es que
                            // la pagina ensene el MISMO nivel que publica el motor.
                            const auto sustained = needleHeldVisible
                                && coherentAmp && coherentFilter;

                            // El release de la SESION puede ser LARGO (su cola no
                            // cruza el suelo del dibujo en el presupuesto): lo que
                            // se exige entonces es que la pagina descienda EN
                            // COHERENCIA con el motor — la misma historia, dos
                            // caminos. Si la cola ya murio, ocultas y santas Pascuas.
                            const auto nativeAmpNow = readEnvelopeLevelForUI (true);
                            const auto nativeFilterNow = readEnvelopeLevelForUI (false);
                            const auto ampFading = ! released.canvasAmp
                                || (released.ampLevel > 0.0
                                    && released.ampLevel < held.ampLevel * 0.5
                                    && std::abs (released.ampLevel - nativeAmpNow) < 0.08);
                            const auto filterFading = ! released.canvasFilter
                                || (released.filterLevel > 0.0
                                    && released.filterLevel < held.filterLevel * 0.5
                                    && std::abs (released.filterLevel - nativeFilterNow) < 0.08);
                            const auto hiddenAfterRelease = needlesHidden (released);
                            const auto releasedOk = hiddenAfterRelease || (ampFading && filterFading);

                            const auto ok = noteEntered && noteLeft
                                && hiddenWhenSilent && sustained && releasedOk;

                            log ("[selftest] AGUJA: silencio " + needlesText (silent)
                                 + " (" + (hiddenWhenSilent ? juce::String ("ocultas") : juce::String ("SE VEIAN"))
                                 + "); nota 60 por la pagina "
                                 + (noteEntered ? juce::String ("entra") : juce::String ("NO ENTRA"))
                                 + ", motor amp " + juce::String (needleNativeAmp, 2) + " / filtro "
                                 + juce::String (needleNativeFilter, 2)
                                 + ", agujas " + needlesText (held)
                                 + " (" + (sustained ? juce::String ("cantando") : juce::String ("SIN NIVEL"))
                                 + "); nota fuera "
                                 + (noteLeft ? juce::String ("sale") : juce::String ("NO SALE"))
                                 + ", agujas " + needlesText (released)
                                 + " (" + (releasedOk
                                     ? (hiddenAfterRelease ? juce::String ("ocultas") : juce::String ("en cola, coherente con motor"))
                                     : juce::String ("SE QUEDARON"))
                                 + ") -> " + (ok ? "OK" : "FAIL"));
                            needleOk = ok;

                            pushNativeToPage();
                        });
                    });
                });
            });
        });
    }

    /** @brief Nivel de UI de UNA envolvente (el que el motor publica para el frame). */
    [[nodiscard]] float readEnvelopeLevelForUI (bool amp)
    {
        float a = 0.0f, f = 0.0f;
        processor.getEnvelopeLevelsForUI (a, f);
        return amp ? a : f;
    }

    /**
     * @brief Sondeo acotado de las agujas (patron ZRING): una lectura cada 30 ms
     *        hasta que la condicion se cumple o se agotan las tomas.
     *
     * En la fase de nota (waiting=true) el exito es "las cuatro visibles"; en la
     * de release (waiting=false), "las cuatro ocultas". La ULTIMA lectura del
     * tramo queda en needleHeldReading/needleReleasedReading y los niveles del
     * motor se muestrean EN la toma que dio el exito: lo que se compara es el
     * mismo instante, no instantes distintos. La fase de nota RETIENE la toma
     * mas COHERENTE (el minimo desfase pagina<->motor de las dos agujas): en la
     * bancada el frame de la pagina llega con retraso y la toma "de nivel mas
     * alto" puede cazar el attack de la pagina contra el sustain del motor.
     */
    void needleSample (int count, std::function<void()> onDone)
    {
        if (count <= 0 || ! lifetime->alive)
        {
            if (lifetime->alive) onDone();
            return;
        }

        evaluate (scriptReadNeedles(), [this, count, onDone] (const juce::String& raw)
        {
            const auto reading = parseNeedles (raw);

            if (needleWaitingVisible)
            {
                // La fase de nota RETIENE la toma mas COHERENTE: el minimo
                // desfase pagina<->motor sumado de las DOS agujas, leido EN el
                // mismo instante de la toma. La de nivel mas alto heria en
                // hosts con la telemetria retrasada (bancada): su mejor nivel
                // de pagina llega 1-2 periodos tarde y el motor ya esta en
                // otro punto del sustain.
                const auto nativeAmpAtSample = readEnvelopeLevelForUI (true);
                const auto nativeFilterAtSample = readEnvelopeLevelForUI (false);
                const auto skew = std::abs (reading.ampLevel - nativeAmpAtSample)
                    + std::abs (reading.filterLevel - nativeFilterAtSample);

                if (needlesVisible (reading) && skew < needleBestSkew)
                {
                    needleHeldReading = raw;
                    needleBestSkew = skew;
                    needleHeldVisible = true;
                    needleNativeAmp = nativeAmpAtSample;
                    needleNativeFilter = nativeFilterAtSample;
                }

                if (needleHeldVisible && count > 1)
                {
                    // Ya se sabe que cantan: unas tomas mas para clavar el
                    // sustain (la mejor toma manda en la comparacion).
                    afterDelay (30, [this, count, onDone] { needleSample (count - 1, onDone); });
                    return;
                }

                if (needleHeldVisible)
                {
                    onDone();
                    return;
                }
            }
            else
            {
                needleReleasedReading = raw;

                if (needlesHidden (reading))
                {
                    onDone();
                    return;
                }
            }

            // El evaluate responde en ~1 ms: sin pacing, las tomas se consumirian
            // antes de que la primera telemetria llegue a la pagina (~66 ms).
            afterDelay (30, [this, count, onDone]
            {
                needleSample (count - 1, onDone);
            });
        });
    }

    /** @brief Nombre con el que el arnes llena cada ranura (lo que la pagina ensena):
        el nombre del FICHERO del asset, que es lo que publica el processor
        (`getFileNameWithoutExtension`), no la clave "name" del JSON. */
    static juce::String modelName (int slot)
    {
        return juce::String (selfModel (slot).fileName);
    }

    /** @brief Los cuatro nombres en el orden del motor, unidos como los une el script. */
    static juce::String expectedSlotNames()
    {
        juce::StringArray names;

        for (int slot = 0; slot < numSelfSlots; ++slot)
            names.add (modelName (slot));

        return names.joinIntoString ("|");
    }

    /** @brief Directorio temporal de los modelos de prueba (se borra al terminar). */
    static juce::File modelDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("neuronik-selftest-models");
    }

    // ========================================================================
    // Los SEIS assets del banco CZ101 embebidos (CMakeLists: NEURONiK_SelftestAssets)
    // ========================================================================

    /** @brief Cuantos modelos de ejemplo viajan embebidos: los seis del banco CZ101. */
    static constexpr int numSelfModels = 6;

    /** @brief Cuantas ranuras del motor ocupan los cuatro primeros (A-D). */
    static constexpr int numSelfSlots = 4;

    /**
     * @brief Un modelo de ejemplo del E2E: el asset embebido y lo que debe medir.
     *
     * `fileName` es el nombre del FICHERO, no la clave "name" del JSON: el
     * processor publica los nombres de las ranuras como
     * `getFileNameWithoutExtension()` y eso es lo que la pagina ensena, asi que es
     * tambien contra lo que se compara la lectura de la ficha A-D.
     *
     * `frames` es el dialecto que el asset declara: 1 = v2 denso estatico, 4 = v2
     * con f0 por frame (el temporal). Entre los seis, los dos dialectos del v2
     * entero pasan por el lector de produccion.
     */
    struct SelfModel
    {
        const char* fileName;
        const char* data;
        int         size;
        int         frames;
    };

    /** @brief Los seis modelos del banco, en el orden del catalogo (A-D y luego los dos temporales). */
    static const SelfModel& selfModel (int index)
    {
        static const SelfModel models[] = {
            { "CZ-BASS1", NeuronikSelftestAssets::CZBASS1_neuronikmodel,
              NeuronikSelftestAssets::CZBASS1_neuronikmodelSize, 1 },
            { "CZ-HAMOG", NeuronikSelftestAssets::CZHAMOG_neuronikmodel,
              NeuronikSelftestAssets::CZHAMOG_neuronikmodelSize, 1 },
            { "CZ-PAD1", NeuronikSelftestAssets::CZPAD1_neuronikmodel,
              NeuronikSelftestAssets::CZPAD1_neuronikmodelSize, 1 },
            { "CZ-SWEP1", NeuronikSelftestAssets::CZSWEP1_neuronikmodel,
              NeuronikSelftestAssets::CZSWEP1_neuronikmodelSize, 1 },
            { "CZ-RRISE-temporal", NeuronikSelftestAssets::CZRRISEtemporal_neuronikmodel,
              NeuronikSelftestAssets::CZRRISEtemporal_neuronikmodelSize, 4 },
            { "CZ-BASS1-temporal", NeuronikSelftestAssets::CZBASS1temporal_neuronikmodel,
              NeuronikSelftestAssets::CZBASS1temporal_neuronikmodelSize, 4 },
        };

        return models[juce::jlimit (0, numSelfModels - 1, index)];
    }

    /**
     * @brief Escribe los SEIS assets del banco al directorio temporal y llena A-D.
     *
     * Dos pasos, y los dos se miden:
     *
     *   1. VALIDACION: cada asset embebido se escribe a fichero y se RELEE con el
     *      lector de produccion (`PresetManager::loadModelFromFile`). Se exige
     *      `isValid` y el numero de frames que el asset declara (1 en los cuatro
     *      estaticos, 4 en los dos temporales), asi que los dos dialectos del v2
     *      —denso y con f0 por frame— pasan por el lector de verdad. Es la mitad
     *      que la ficha de la pagina NO puede medir: si un asset deja de ser
     *      legible (formato roto, asset cambiado), el veredicto lo dice.
     *
     *   2. RANURAS: los cuatro primeros (CZ-BASS1, CZ-HAMOG, CZ-PAD1, CZ-SWEP1 —
     *      los mismos cuatro que el preset de banco CZ101-BANK pone en las
     *      esquinas del pad) entran por las ranuras A-D del processor, y de ahi
     *      a la ficha MODELOS de la pagina.
     *
     * El material es el REAL del banco (los seis `.neuronikmodel` que tambien
     * viajan en `NEURONiK_FactoryModels`), no JSON sintetico: si el plugin dejase
     * de entender el dialecto del Model Maker, las ranuras se quedarian mudas y
     * esta direccion lo diria en voz alta en vez de dar OK con ranuras vacias.
     */
    void loadSelfModels()
    {
        const auto directory = modelDirectory();
        directory.deleteRecursively();
        directory.createDirectory();

        modelsAssetsOk = true;

        for (int index = 0; index < numSelfModels; ++index)
        {
            const auto& asset = selfModel (index);
            const juce::File file = directory.getChildFile (juce::String (asset.fileName)
                                                                + ".neuronikmodel");

            file.replaceWithData (asset.data, (size_t) asset.size);

            const auto parsed = NEURONiK::Serialization::PresetManager::loadModelFromFile (file);
            const bool ok = parsed.isValid && parsed.frameCount == asset.frames;

            modelsAssetsOk = modelsAssetsOk && ok;

            log ("[selftest] MODELOS: asset " + file.getFileName() + " -> "
                 + (ok ? "OK (" + juce::String (parsed.frameCount)
                             + (parsed.frameCount == 1 ? " frame v2)" : " frames v2, f0 por frame)")
                       : "FAIL (el lector de produccion no lo entendio)"));
        }

        for (int slot = 0; slot < numSelfSlots; ++slot)
        {
            const juce::File file = directory.getChildFile (modelName (slot) + ".neuronikmodel");
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
     *   4. con eso medido la direccion NO se cierra: pasa el testigo a la segunda pasada
     *      (`pressFreezeGuard`), que congela el banco espectral y exige que solo se mueva
     *      lo que no esta congelado. Un sorteo que ignora los `freeze*` mueve la tabla
     *      entera y alli se ve.
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

                        // La direccion no se cierra aqui: el sorteo con TODO suelto ya esta
                        // medido, y ahora toca lo contrario —que los congelados de verdad
                        // congelen—, que es la mitad de la promesa del boton.
                        pressFreezeGuard();
                    });
                });
            });
        });
    }

    /**
     * @brief Segunda pasada de ACCIONES: el sorteo RESPETA los congelados.
     *
     * @details La pasada anterior midio el sorteo con los tres `freeze*` a 0; esta mide
     * lo contrario, que es la otra mitad de la promesa del boton: con el banco
     * espectral congelado, sus 13 objetivos de los 23 no pueden moverse. Los otros
     * dos congelados se dejan SUELTOS a proposito — asi "no se
     * movio" significa "estaba congelado", y no "el sorteo dejo de correr".
     *
     * Se pulsa el MISMO boton de la pagina (el gesto de un usuario, el mismo anclaje
     * del contrato) y el veredicto sale del APVTS, que es donde el sorteo ocurre:
     *
     *   - CONGELADOS: cero movimientos. Ni uno: un sorteo que ignora el `freeze*` los
     *     mueve todos, y la holgura del azar no aplica aqui (un congelado no tiene
     *     por que "caer" en su valor: no se le sortea nada);
     *   - SUELTOS: se mueven todos menos los dos de holgura de siempre (el azar puede
     *     dejar un objetivo en el valor que ya tenia), que es lo que impide que un
     *     sorteo que NO corre pase por "respetuoso con los congelados".
     *
     * El `freezeResonator` era de la MEDIDA y se devuelve a 0: el APVTS se queda
     * sorteado (eso el arnes ya lo declara), pero no con un congelado que el usuario
     * no pidio.
     */
    void pressFreezeGuard()
    {
        setParameterReal (State::IDs::freezeResonator, 1.0f);

        beforeFreezeGuard = readRandomizeValues();

        // El margen de siempre: la escritura llega a la pagina en el siguiente poll.
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

                const auto error = object != nullptr ? field ("error").toString()
                                                     : juce::String ("NO_RESULT");
                const auto label = field ("label").toString();
                const auto closed = static_cast<int> (field ("closed"));
                const auto enabled = static_cast<int> (field ("enabled"));

                afterDelay (600, [this, error, label, closed, enabled]
                {
                    const auto after = readRandomizeValues();
                    const auto& targets = State::getRandomizeTargets();

                    int frozenMoved = 0;
                    int looseMoved = 0;
                    int looseTotal = 0;
                    juce::String worst;

                    for (size_t index = 0; index < targets.size() && index < after.size(); ++index)
                    {
                        const auto changed = std::abs (after[index] - beforeFreezeGuard[index]) > 1.0e-6f;

                        if (targets[index].freeze == State::FreezeGroup::resonator)
                        {
                            if (changed)
                            {
                                ++frozenMoved;

                                if (worst.isEmpty())
                                    worst = juce::String (targets[index].id) + ": "
                                            + juce::String (beforeFreezeGuard[index], 4) + " -> "
                                            + juce::String (after[index], 4);
                            }
                        }
                        else
                        {
                            ++looseTotal;
                            if (changed)
                                ++looseMoved;
                        }
                    }

                    const auto looseMovedEnough = looseMoved >= looseTotal - 2;
                    const auto ok = error.isEmpty() && enabled == 1 && closed == 1
                                        && frozenMoved == 0 && looseTotal > 0 && looseMovedEnough;

                    log ("[selftest] ACCIONES: RANDOM con freezeResonator (\"" + label + "\""
                         + (enabled == 1 ? ", habilitado" : ", DESHABILITADO")
                         + ") movio " + juce::String (looseMoved) + "/" + juce::String (looseTotal)
                         + " objetivos SUELTOS y " + juce::String (frozenMoved)
                         + " objetivos CONGELADOS"
                         + (worst.isEmpty() ? juce::String() : " [" + worst + "]")
                         + (error.isEmpty() ? juce::String() : "  [" + error + "]")
                         + " -> " + (ok ? "OK" : "FAIL"));

                    freezeGuardOk = ok;

                    // El congelado era de la medida, no del usuario.
                    setParameterReal (State::IDs::freezeResonator, 0.0f);

                    // El arnes NO se cierra aqui: queda la ultima direccion —el pad XY
                    // y el aro con el modelo real dentro—, y es ella la que decide.
                    morphDirection();
                });
            });
        });
    }

    // ========================================================================
    // 8. MORPH: el pad XY y el anillo morph-Z, con el modelo REAL cargado
    // ========================================================================

    /** @brief Lo que la pagina ensena del pad y del aro en una lectura. */
    struct MorphState
    {
        bool ok = false;
        juce::String error;
        double x = -1.0;         // aria-valuenow de la superficie (eje x)
        double y = -1.0;         // style.top del pulgar, des-invertido (y = 1 - top)
        double thumbX = -1.0;    // style.left del pulgar: la MISMA x, pintada
        double z = -1.0;         // aria-valuenow del aro
        double fill = -1.0;      // primer numero del dasharray del arco (0..100)
        juce::String corners;    // las cuatro esquinas del pad, en el orden del motor
        juce::String slots;      // los cuatro nombres de la ficha MODELOS A-D
        int padHit = -1;         // hit-testing en el centro del pad: 1 si es del pad
        int ringHit = -1;        // hit-testing en el trazo del aro: 1 si es del aro
    };

    /**
     * @brief Lee el pad y el aro de la pagina (`MorphState`).
     *
     * El recolector es UNO (`morphCollectorJs`) y lo comparten las dos lecturas
     * —antes del gesto y despues— para que midan exactamente lo mismo: dos
     * recolectores paralelos podrian divergir sin que nadie lo notase.
     */
    static MorphState parseMorph (const juce::String& raw)
    {
        MorphState state;

        // El `var` de `JSON::parse` tiene que SOBREVIVIR al puntero: un temporal
        // muere al final de la EXPRESION, y `getDynamicObject()` de el deja el
        // objeto liberado — en Release las propiedades se leen del recuerdo, que
        // contesta "no existe" a todas (medido en vivo: con el JSON crudo correcto
        // delante, el pad y el aro salian a -1 y las esquinas vacias). Los otros
        // lectores del arnes ya guardan el `var` en un local; este era el unico
        // que encadenaba las dos llamadas.
        const auto parsed = juce::JSON::parse (raw);
        const auto* object = parsed.getDynamicObject();

        if (object == nullptr)
        {
            state.error = "sin JSON: " + raw;
            return state;
        }

        state.error = object->getProperty ("error").toString();

        if (state.error.isNotEmpty())
            return state;

        const auto number = [object] (const char* key, double fallback)
        {
            return object->hasProperty (key) ? (double) object->getProperty (key) : fallback;
        };

        state.ok = true;
        state.x = number ("x", -1.0);
        state.y = number ("y", -1.0);
        state.thumbX = number ("thumbX", -1.0);
        state.z = number ("z", -1.0);
        state.fill = number ("fill", -1.0);
        state.corners = object->getProperty ("corners").toString();
        state.slots = object->getProperty ("slots").toString();
        state.padHit = object->hasProperty ("padHit") ? (int) object->getProperty ("padHit") : -1;
        state.ringHit = object->hasProperty ("ringHit") ? (int) object->getProperty ("ringHit") : -1;
        return state;
    }

    /** @brief El valor NORMALIZADO de un parametro del APVTS (-1 si no existe). */
    [[nodiscard]] float readParameter (const char* parameterId) const
    {
        if (const auto* parameter = processor.getAPVTS().getParameter (parameterId))
            return parameter->getValue();

        return -1.0f;
    }

    /**
     * @brief El recolector JS: pad, aro, esquinas del pad y ficha A-D.
     *
     * `.abd-xypad__thumb` y `.abd-xypad__corner` son del componente COMPARTIDO
     * (`@abdsynths/shared`): el anclaje declarado es la superficie del pad y el
     * aro de esta pagina, y el script compone el resto (mismo criterio que el
     * `data-parameter-id` de la matriz, que compone el selector de la celda).
     */
    static juce::String morphCollectorJs()
    {
        const auto visual = juce::String ("'[data-visual=\"") + SelftestPage::padVisual + "\"]'";
        const auto pad = juce::String ("'") + SelftestPage::padSurface + "'";
        const auto ring = juce::String ("'") + SelftestPage::zRing + "'";
        const auto fill = juce::String ("'") + SelftestPage::zRingFill + "'";
        const auto rows = juce::String ("'") + SelftestPage::modelSlotRow + "'";
        const auto rowName = juce::String ("'") + SelftestPage::modelSlotName + "'";

        return "  const morphState = () => {"
               "    const visual = document.querySelector(" + visual + ");"
               "    if (!visual) return { error: 'NO_VISUAL' };"
               "    const padEl = visual.querySelector(" + pad + ");"
               "    const ring = visual.querySelector(" + ring + ");"
               "    if (!padEl) return { error: 'NO_PAD' };"
               "    if (!ring) return { error: 'NO_RING' };"
               "    const fill = ring.querySelector(" + fill + ");"
               "    const thumb = padEl.querySelector('.abd-xypad__thumb');"
               "    const corner = (position) => {"
               "      const el = padEl.querySelector('.abd-xypad__corner[data-corner=\"' + position + '\"]');"
               "      return el ? el.textContent : ''; };"
               "    const name = (row) => { const el = row.querySelector(" + rowName + ");"
               "      return el ? el.textContent : ''; };"
               "    return {"
               "      x: Number(padEl.getAttribute('aria-valuenow')),"
               "      y: 1 - parseFloat(thumb.style.top) / 100,"
               "      thumbX: parseFloat(thumb.style.left) / 100,"
               "      z: Number(ring.getAttribute('aria-valuenow')),"
               "      fill: parseFloat(fill.getAttribute('stroke-dasharray')),"
               "      corners: ['tl', 'tr', 'bl', 'br'].map(corner).join('|'),"
               "      slots: Array.from(document.querySelectorAll(" + rows + ")).map(name).join('|') };"
               "  };";
    }

    /** @brief El pad y el aro tal como estan AHORA (JSON). */
    static juce::String scriptReadMorph()
    {
        return "(() => { try {"
               "\n" + morphCollectorJs() + "\n"
               "  return JSON.stringify(morphState());"
               " } catch (e) { return JSON.stringify({ error: e.message }); } })()";
    }

    /**
     * @brief Un gesto de USUARIO: hit-test primero, dispatch sobre lo que devuelve.
     *
     * El pad se arrastra a la fraccion pedida de su rect (las dos desde su esquina
     * superior izquierda: la `y` de pantalla esta invertida respecto al valor, que
     * es lo que hay que medir) y el aro a la vuelta pedida sobre su trazo. Los dos
     * se despachan sobre `document.elementFromPoint`, que es lo que hace el
     * navegador con un dedo: asi el gesto mide ademas la GEOMETRIA. Un aro que se
     * come el pad —o una caja que se come el trazo— sale aqui, y no en un test de
     * manejadores: esos pasan igual con la pagina tapada, que es exactamente como
     * el pad estuvo sin recibir gestos con el aro de overlay encima.
     *
     * @param padFractionX fraccion del ancho del pad donde soltar.
     * @param padFractionY fraccion de su ALTO (desde arriba): el valor que queda
     *                     es `1 - padFractionY`.
     * @param ringTurn     vuelta del aro (0 en las 12, horario, 1 = completa).
     */
    static juce::String scriptMorphGesture (double padFractionX, double padFractionY, double ringTurn)
    {
        const auto visual = juce::String ("'[data-visual=\"") + SelftestPage::padVisual + "\"]'";
        const auto pad = juce::String ("'") + SelftestPage::padSurface + "'";
        const auto ring = juce::String ("'") + SelftestPage::zRing + "'";

        return "(() => { try {"
               "\n" + morphCollectorJs() + "\n"
               "  const visual = document.querySelector(" + visual + ");"
               "  if (!visual) return JSON.stringify({ error: 'NO_VISUAL' });"
               "  const padEl = visual.querySelector(" + pad + ");"
               "  const ring = visual.querySelector(" + ring + ");"
               "  if (!padEl) return JSON.stringify({ error: 'NO_PAD' });"
               "  if (!ring) return JSON.stringify({ error: 'NO_RING' });"
               "  const padRect = padEl.getBoundingClientRect();"
               "  const svg = ring.querySelector('svg');"
               "  const ringRect = svg.getBoundingClientRect();"
               // El svg es un viewBox cuadrado (100x100) en una caja que no lo es:
               // `preserveAspectRatio: meet` escala por el lado MENOR.
               "  const scale = Math.min(ringRect.width, ringRect.height) / 100;"
               "  if (!(padRect.width > 0 && padRect.height > 0)) return JSON.stringify({ error: 'PAD_SIN_LAYOUT' });"
               "  if (!(scale > 0)) return JSON.stringify({ error: 'ARO_SIN_LAYOUT' });"
               "  const send = (el, type, x, y) => el.dispatchEvent(new PointerEvent(type, {"
               "    clientX: x, clientY: y, bubbles: true, cancelable: true,"
               "    pointerId: 1, pointerType: 'mouse', isPrimary: true }));"
               "  const padHitX = padRect.left + padRect.width / 2;"
               "  const padHitY = padRect.top + padRect.height / 2;"
               "  const padDropX = padRect.left + padRect.width * " + juce::String (padFractionX) + ";"
               "  const padDropY = padRect.top + padRect.height * " + juce::String (padFractionY) + ";"
               "  const padTarget = document.elementFromPoint(padHitX, padHitY);"
               "  send(padTarget, 'pointerdown', padHitX, padHitY);"
               "  send(padTarget, 'pointermove', padDropX, padDropY);"
               "  send(padTarget, 'pointerup', padDropX, padDropY);"
               "  const rcx = ringRect.left + ringRect.width / 2;"
               "  const rcy = ringRect.top + ringRect.height / 2;"
               "  const angle = 2 * Math.PI * " + juce::String (ringTurn) + " - Math.PI / 2;"
               "  const dropX = rcx + 47 * scale * Math.cos(angle);"
               "  const dropY = rcy + 47 * scale * Math.sin(angle);"
               "  const ringTarget = document.elementFromPoint(dropX, dropY);"
               "  send(ringTarget, 'pointerdown', dropX, dropY);"
               "  send(ringTarget, 'pointermove', dropX, dropY);"
               "  send(ringTarget, 'pointerup', dropX, dropY);"
               "  return JSON.stringify(Object.assign(morphState(), {"
               "    padHit: padTarget && padTarget.closest(" + pad + ") ? 1 : 0,"
               "    ringHit: ringTarget && ringTarget.closest(" + ring + ") ? 1 : 0 }));"
               " } catch (e) { return JSON.stringify({ error: e.message }); } })()";
    }

    // ========================================================================
    // 8. ZRING: el anillo morph-Z GIRANDO (telemetria nativa del plugin)
    // ========================================================================

    /** El script de UNA instantanea del arco de modulacion: su span en guiones
        (el `stroke-dasharray` que renderZ escribe como "span 100"). */
    juce::String zringArcScript() const
    {
        const auto ring = juce::String ("'") + SelftestPage::zRing + "'";
        const auto mod = juce::String ("'") + SelftestPage::zRingMod + "'";

        return "(() => { try {"
               "  const ring = document.querySelector(" + ring + ");"
               "  if (!ring) return JSON.stringify({ error: 'NO_RING' });"
               "  const mod = ring.querySelector(" + mod + ");"
               "  if (!mod) return JSON.stringify({ error: 'NO_MOD_ARC' });"
               "  const dash = (mod.getAttribute('stroke-dasharray') || '0 100').trim().split(/\\s+/);"
               "  const off = mod.getAttribute('stroke-dashoffset') || '0';"
               "  return JSON.stringify({ span: Number (dash[0]), start: -Number (off) });"
               " } catch (e) { return JSON.stringify({ error: e.message }); } })()";
    }

    /** Toma `count` instantaneas del arco, una por evaluate (siacrono y
        secuencial: cada vuelta ve la pagina de su momento) y con la marca de
        tiempo REAL de llegada: el periodo se mide contra el reloj de pared, no
        contra un paso asumido. */
    void zringSample (int count, std::function<void()> onDone)
    {
        if (count <= 0 || ! lifetime->alive)
        {
            if (lifetime->alive) onDone();
            return;
        }

        evaluate (zringArcScript(), [this, count, onDone] (const juce::String& raw)
        {
            const auto parsed = juce::JSON::parse (raw);
            const auto* object = parsed.getDynamicObject();
            double span = -1.0;

            if (object != nullptr)
            {
                if (object->hasProperty ("error"))
                    log ("[selftest] ZRING: la pagina dijo \""
                             + object->getProperty ("error").toString() + "\"");
                if (object->hasProperty ("span"))
                    span = (double) object->getProperty ("span");
            }

            double start = -1.0;

            if (object != nullptr && object->hasProperty ("start"))
                start = (double) object->getProperty ("start");

            zringTimes.push_back (juce::Time::getMillisecondCounterHiRes());
            zringSpans.push_back (span);
            zringStarts.push_back (start);

            // PACING: el evaluate de esta bancada responde en ~1 ms, asi que
            // una colecta seguida de 240 tomas solo cubriria ~200 ms de arco
            // (menos de un periodo de 1 Hz) y la ventana de reanudacion se
            // acabaria antes de que la primera telemetria llegue a la pagina.
            // Una toma cada 30 ms reparte las N tomas en tiempo REAL.
            afterDelay (30, [this, count, onDone]
            {
                zringSample (count - 1, onDone);
            });
        });
    }

    /** Periodo medio (ms) entre cristas locales >= 70% del maximo, con el reloj
        de pared de la colecta; outMax deja el maximo del arco (en guiones). */
    double zringPeriodMs (double& outMax) const
    {
        outMax = 0.0;
        for (const double s : zringSpans) outMax = juce::jmax (outMax, s);

        if (outMax <= 1.0 || zringTimes.size() < 8) return -1.0;
        const double peakLevel = outMax < 30.0 ? 0.7 * outMax : 0.35 * outMax;

        std::vector<double> peakTimes;

        for (size_t i = 1; i + 1 < zringSpans.size(); ++i)
            if (zringSpans[i] >= peakLevel && zringSpans[i] >= zringSpans[i - 1]
                    && zringSpans[i] > zringSpans[i + 1])
                peakTimes.push_back (zringTimes[i]);

        // Meseta: el arco se cuantiza a guiones enteros, asi que el maximo
        // local puede repetirse en muestras consecutivas (~66 ms de telemetria
        // frente a ~1 us de la subida). Crista = PRIMERA muestra que alcanza
        // el nivel y separada >= 350 ms de la crista anterior (un periodo de
        // 1 Hz no puede tener dos cristas validas a menos distancia).
        std::vector<double> deduped;
        for (const double t : peakTimes)
            if (deduped.empty() || t - deduped.back() > 250.0)
                deduped.push_back (t);
        peakTimes = deduped;

        if (peakTimes.size() < 2) return -1.0;

        double sum = 0.0;
        for (size_t i = 1; i < peakTimes.size(); ++i)
            sum += peakTimes[i] - peakTimes[i - 1];

        return sum / (double) (peakTimes.size() - 1);
    }

    /** Volcado compacto de la colecta: ~20 spans y el paso medio real. */
    juce::String zringTrace() const
    {
        if (zringSpans.empty()) return "(vacio)";

        const int stride = juce::jmax (1, (int) zringSpans.size() / 20);
        juce::String text = "spans[" + juce::String ((int) zringSpans.size()) + "] ";

        for (size_t i = 0; i < zringSpans.size(); i += (size_t) stride)
            text << (int) zringSpans[i] << " ";

        double dtSum = 0.0;
        int dtCount = 0;
        for (size_t i = 1; i < zringTimes.size(); ++i)
        {
            dtSum += zringTimes[i] - zringTimes[i - 1];
            ++dtCount;
        }

        return text + "| dt medio " + juce::String (dtCount > 0 ? dtSum / dtCount : 0.0, 1) + " ms";
    }

    void zringDirection()
    {
        stage = Stage::zring;

        // La ruta por el APVTS, como la dejaria un preset: la matriz usa
        // INDICES de contrato, nunca literales escritos a mano.
        zringLfo2Source = State::getModSources().indexOf ("LFO 2");
        zringDest28 = destinationIndexFor (State::IDs::morphZ);

        setParameterReal (State::IDs::mod1Source, (float) zringLfo2Source);
        setParameterReal (State::IDs::mod1Destination, (float) zringDest28);
        setParameterReal (State::IDs::mod1Amount, 1.0f);
        setParameterReal (State::IDs::lfo2RateHz, 1.0f);
        setParameterReal (State::IDs::lfo2Depth, 1.0f);
        setParameterReal (State::IDs::lfo2SyncMode, 0.0f);   // Free: el rate manda
        setParameterReal (State::IDs::lfo2RhythmicDivision, 0.0f);
        setParameterReal (State::IDs::morphZ, 0.0f);   // base en reposo: el arco manda

        log ("[selftest] ZRING: ruta por APVTS = LFO 2 (fuente " + juce::String (zringLfo2Source)
             + ") -> Morph Z (destino " + juce::String (zringDest28)
             + "), amount 1.0, LFO2 a 1.0 Hz; el arco lo pinta frame.modulation[28].");

        afterDelay (600, [this]
        {
            // A: LFO 2 al 100% — periodo medido entre cristas del arco.
            zringSample (480, [this]
            {
                zringMaxOn = 0.0;
                zringPeriodOn = zringPeriodMs (zringMaxOn);
                // Arco CON signo (2026-09-27): la senoide COMPLETA esta pintada
                // (horario + antihorario), asi que el arco culmina 2 veces por
                // periodo: |sin| de 1 Hz -> ~500 ms entre culminaciones.
                const bool periodOk = zringPeriodOn > 350.0 && zringPeriodOn < 700.0;   // |sin| 1 Hz
                const bool sweepOk = zringMaxOn > 40.0;   // el arco sube hasta ~100 guiones
                log ("[selftest] ZRING: arco con LFO2 -> 28: max " + juce::String (zringMaxOn, 1)
                     + " guiones, periodo " + juce::String (zringPeriodOn, 0)
                     + " ms (esperado ~1000) -> " + (periodOk && sweepOk ? "OK" : "FAIL"));
                log ("[selftest] ZRING: diagnostico A: " + zringTrace());

                int paintedNegative = 0;
                for (const double s : zringStarts)
                    if (s > 5.0 && s < 95.0) ++paintedNegative;

                log ("[selftest] ZRING: arco con signo: "
                     + juce::String (paintedNegative) + "/" + juce::String ((int) zringStarts.size())
                     + " instantaneas en el lado ANTIHORARIO (semionda negativa pintada)");
                zringNegativeSeen = paintedNegative > 0;

                zringTimes.clear();
                zringSpans.clear();
                zringStarts.clear();

                // B: fuente Off — el arco clavado a 0.
                setParameterReal (State::IDs::mod1Source, 0.0f);
                afterDelay (600, [this]
                {
                    zringSample (24, [this]
                    {
                        double maxOff = 0.0;
                        for (const double s : zringSpans) maxOff = juce::jmax (maxOff, std::abs (s));
                        const bool offOk = maxOff < 2.0;
                        log ("[selftest] ZRING: control negativo (fuente Off): |arco| max "
                             + juce::String (maxOff, 1) + " guiones -> "
                             + (offOk ? "OK" : "FAIL"));

                        zringTimes.clear();
                        zringSpans.clear();
                        zringStarts.clear();

                        // A de nuevo: LFO 2 vuelve y el arco se reanuda.
                        setParameterReal (State::IDs::mod1Source, (float) zringLfo2Source);
                        afterDelay (600, [this, offOk]
                        {
                            zringSample (280, [this, offOk]
                            {
                                double maxBack = 0.0;
                                zringPeriodMs (maxBack);
                                const bool backOk = maxBack > 40.0;
                                log ("[selftest] ZRING: A de nuevo (LFO 2 vuelve): max "
                                     + juce::String (maxBack, 1) + " guiones -> "
                                     + (backOk ? "OK" : "FAIL"));
                                log ("[selftest] ZRING: diagnostico A2: " + zringTrace());

                                zringOk = zringPeriodOn > 350.0 && zringPeriodOn < 700.0
                                              && zringMaxOn > 40.0 && zringNegativeSeen
                                              && offOk && backOk;

                                log (juce::String ("[selftest] ZRING: la ruta nativa LFO 2 -> Morph Z (destino ")
                                     + juce::String (zringDest28)
                                     + ") gira el anillo con el arco CON SIGNO (semionda negativa incluida) -> "
                                     + (zringOk ? "OK" : "FAIL"));

                                // Ultima direccion: ella cierra el veredicto entero.
                                finish (matrixDirectionOk() && envRoutesDirectionOk() && needleDirectionOk()
                                            && modelsDirectionOk() && modelsAssetsOk
                                            && nativeToPageOk
                                            && pageToNativeOk && generalOk && midiOk && actionsOk
                                            && freezeGuardOk && morphOk && zringOk);
                            });
                        });
                    });
                });
            });
        });
    }

    /**
     * @brief Ultima direccion: el pad XY y el aro morph-Z, con un modelo REAL.
     *
     * @details El pad de la ficha MODELOS no es un dibujo: mueve `morphX`/`morphY`
     *          (dos parametros del contrato) y el aro mueve `morphZ`, el tercer
     *          eje de la fase 10.3. La direccion lo mide con el material de
     *          fabrica dentro:
     *
     *            1. carga `CZ-SWEP1.neuronikmodel` en la ranura D — el fichero que
     *               `installFactoryModels()` instala y al que apunta el
     *               `modelPath3` del preset de banco CZ101-BANK, no una copia:
     *               con el material real dentro, la ESQUINA del pad lo ensena, y
     *               esa es la prueba de que las ranuras y el morph hablan del
     *               mismo motor;
     *            2. motor -> pagina: escribe morphX/Y/Z en el APVTS (0,2 / 0,8 /
     *               0,6, tres valores DISTINTOS) y comprueba que el pad y el aro se
     *               pintan con ESOS: el pulgar, los `aria-*` y el guion del arco;
     *            3. pagina -> motor: un arrastre del pad (a 0,75 / 0,25) y un gesto
     *               del aro (1/4 de vuelta) con PointerEvent de verdad, y despues
     *               el APVTS: los tres parametros donde el gesto los dejo;
     *            4. la GEOMETRIA de la pagina viva, que es lo que ningun test de
     *               manejadores puede ver: el centro del pad tiene que caer en el
     *               pad (el aro de overlay NO se lo come) y el trazo del aro en el
     *               aro (una caja sin `pointer-events: none` se comia los dos, y
     *               el pad quedo sin gestos en todo su interior hasta esta
     *               direccion).
     *
     * El APVTS se queda con CZ-SWEP1 en D: lo que el usuario pidio ver es esto
     * mismo —el modelo real cargado— y el arnes no lo devuelve a su estado previo.
     */
    void morphDirection()
    {
        stage = Stage::morph;

        const auto realModel = NEURONiKProcessor::factoryModelsDirectory()
                                   .getChildFile ("CZ-SWEP1.neuronikmodel");
        const auto realName = realModel.getFileNameWithoutExtension();
        const auto realLoaded = realModel.existsAsFile() && processor.loadModel (realModel, 3);

        log ("[selftest] MORPH: " + realModel.getFileName() + " -> ranura D (3): "
             + (realLoaded ? "cargado desde " + realModel.getFullPathName()
                           : "RECHAZADO: el modelo de fabrica no esta en su sitio"));

        // Por el APVTS, que es por donde lo pondria un preset (y el pad, al soltar).
        setParameterReal (State::IDs::morphX, 0.2f);
        setParameterReal (State::IDs::morphY, 0.8f);
        setParameterReal (State::IDs::morphZ, 0.6f);

        log ("[selftest] MORPH: script = " + scriptReadMorph());

        // El poll del dueno los lleva a la pagina en 30 ms: el margen de siempre.
        afterDelay (500, [this, realLoaded, realName]
        {
            evaluate (scriptReadMorph(), [this, realLoaded, realName] (const juce::String& raw)
            {
                const auto painted = parseMorph (raw);

                const auto paintedOk = painted.ok
                                           && std::abs (painted.x - 0.2) < 0.02
                                           && std::abs (painted.y - 0.8) < 0.02
                                           && std::abs (painted.thumbX - 0.2) < 0.02
                                           && std::abs (painted.z - 0.6) < 0.02
                                           && std::abs (painted.fill - 60.0) < 1.0;

                // La ranura D es la ESQUINA inferior derecha del pad (`br`): el mismo
                // orden que pinta el XYPad nativo con `setModelNames`.
                juce::StringArray expectedCorners;

                for (int slot = 0; slot < 3; ++slot)
                    expectedCorners.add (modelName (slot));

                expectedCorners.add (realName);
                const auto cornersText = expectedCorners.joinIntoString ("|");

                // Y la ficha A-D publica los mismos cuatro nombres: una vista no puede
                // ensenar el modelo real y la otra seguir con el de prueba.
                const auto slotsText = cornersText;
                const auto realOk = painted.ok && painted.corners == cornersText
                                        && painted.slots == slotsText;

                log ("[selftest] MORPH: crudo de la lectura = \"" + raw + "\"");

                const auto afterLoad = [this, realLoaded, realName, painted, paintedOk,
                                        realOk, cornersText, raw]
                {
                    // 3 y 4 en el MISMO viaje: el gesto y lo que la pagina ensena despues.
                    evaluate (scriptMorphGesture (0.75, 0.75, 0.25),
                              [this, realLoaded, realName, painted, paintedOk, realOk, cornersText, raw]
                              (const juce::String& gestureRaw)
                    {
                        const auto gest = parseMorph (gestureRaw);

                        // 4. GEOMETRIA: el pad recibe su propio gesto en el centro y el aro
                        // es agarrable en su trazo. Es lo que se rompio con el overlay.
                        const auto geometryOk = gest.ok && gest.padHit == 1 && gest.ringHit == 1;

                        // 3b. La pagina se queda pintada donde la dejo el gesto (eco local).
                        const auto echoOk = gest.ok
                                                && std::abs (gest.x - 0.75) < 0.02
                                                && std::abs (gest.y - 0.25) < 0.02
                                                && std::abs (gest.z - 0.25) < 0.02;

                        // La pagina empuja al APVTS en el mismo evento: este margen cubre
                        // un solo salto (JS -> nativo), no la vuelta.
                        afterDelay (400, [this, realLoaded, realName, painted, paintedOk,
                                          realOk, cornersText, raw, gest, geometryOk, echoOk,
                                          gestureRaw]
                        {
                            const auto nativeX = readParameter (State::IDs::morphX);
                            const auto nativeY = readParameter (State::IDs::morphY);
                            const auto nativeZ = readParameter (State::IDs::morphZ);
                            const auto nativeOk = std::abs (nativeX - 0.75) < 0.02
                                                      && std::abs (nativeY - 0.25) < 0.02
                                                      && std::abs (nativeZ - 0.25) < 0.02;
                            const auto ok = realLoaded && paintedOk && realOk
                                                && geometryOk && echoOk && nativeOk;

                            log ("[selftest] MORPH: " + realName + " en la ranura D; esquinas del pad \""
                                 + gest.corners + "\" y ficha A-D \"" + gest.slots
                                 + "\" (esperado \"" + cornersText + "\") -> "
                                 + (realOk ? "OK" : "FAIL"));
                            log ("[selftest] MORPH: APVTS 0,2/0,8/0,6 -> pad X "
                                 + juce::String (painted.x, 3) + " / Y " + juce::String (painted.y, 3)
                                 + " (pulgar " + juce::String (painted.thumbX, 3) + "), aro "
                                 + juce::String (painted.z, 3) + " con arco "
                                 + juce::String (painted.fill, 1) + " -> "
                                 + (paintedOk ? "OK" : "FAIL"));
                            log ("[selftest] MORPH: gesto (pad a 0,75/0,25, aro a 1/4 de vuelta) -> APVTS "
                                 + juce::String (nativeX, 3) + "/" + juce::String (nativeY, 3) + "/"
                                 + juce::String (nativeZ, 3) + ", la pagina pinto "
                                 + juce::String (gest.x, 3) + "/" + juce::String (gest.y, 3) + "/"
                                 + juce::String (gest.z, 3) + " -> "
                                 + (nativeOk && echoOk ? "OK" : "FAIL"));
                            log ("[selftest] MORPH: crudo del gesto = \"" + gestureRaw + "\"");
                            log (juce::String ("[selftest] MORPH: hit-testing en la pagina viva (centro del pad ")
                                 + (gest.padHit == 1 ? "en el pad" : "TAPADO")
                                 + ", trazo del aro " + (gest.ringHit == 1 ? "en el aro" : "INALCANZABLE")
                                 + ") -> " + (geometryOk ? "OK" : "FAIL")
                                 + (gest.error.isEmpty() ? juce::String() : "  [" + gest.error + "]"));

                            morphOk = ok;

                            // Queda la ULTIMA direccion: el anillo girando con la
                            // matriz por el camino nativo (telemetria del plugin).
                            zringDirection();

                            // El veredicto lo cierra la ULTIMA direccion (ZRING, ya
                            // lanzada): las direcciones son obligatorias y ninguna se
                            // puede omitir — todas las banderas viajan en SU finish.
                        });
                    });
                };

                afterLoad();
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

    /** @brief Veredicto de ENV-RUTAS: la ruta clicable de ENVOLVENTES lleva a la MATRIZ (su slot). */
    [[nodiscard]] bool envRoutesDirectionOk() const noexcept { return envRoutesOk; }

    /** @brief Veredicto de AGUJA: los niveles reales pintan sobre las curvas y mueren con la nota. */
    [[nodiscard]] bool needleDirectionOk() const noexcept { return needleOk; }

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
        // declaradas no aplicables, asi que un OK es un OK de las ocho direcciones.
        log (juce::String ("[selftest] RESULT: ") + (passed ? "OK" : "FAIL"));

        if (onFinished != nullptr)
            onFinished (passed);
    }

    // ========================================================================
    // Los scripts. Viven aqui, en un solo sitio, porque son el contrato con la
    // pagina; los anclajes salen de `SelftestPage`. El recolector del pad y del
    // aro es UNO (`morphCollectorJs`), compartido por las dos lecturas de la
    // ultima direccion: el antes y el despues no pueden medir cosas distintas.
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

    /**
     * @brief El salto ruta->matriz: pulsa UNA fila ENV de la ficha ENVOLVENTES y
     *        mide lo que la pagina ensena despues.
     *
     * Las filas son `.env-route` (botones reales, con data-env-route = id del
     * destino y data-slot = RUTA n). El resalte del slot lo escribe el panel como
     * data-slot-highlight en cada .drawer-slot del cajon (true SOLO en el elegido).
     * Los controles del cajon se leen como en scriptOpenMatrixDrawer (selectedIndex
     * de los selects), DENTRO del cajon abierto.
     *
     * El slot del boton NO se escribe desde fuera (el Standalone de JUCE restaura
     * el filterState de la ultima sesion y la matriz puede venir del usuario): el
     * script ELIGE la primera fila pintada de una fuente ENV (los selects del cajon
     * abierto antes del salto dicen la fuente de cada slot) y devuelve SU slot. El
     * comparador C++ cruza esa eleccion con lo que el APVTS dice del slot.
     */
    static juce::String scriptEnvRouteJump (int env1Index, int env2Index)
    {
        const auto open = juce::String ("'.") + SelftestPage::openDrawerClass + "'";
        const auto veil = juce::String ("'.") + SelftestPage::visibleBackdropClass + "'";
        const auto slotSelector = juce::String ("'.") + SelftestPage::drawerSlotClass + "'";

        return "(() => { try {"
               // Las filas de ENVOLVENTES: botones con data-env-route (el resumen de
               // la matriz usa otra clase; el estado vacio es un <span>, no boton).
               "  const rows = Array.from(document.querySelectorAll('button[data-env-route]'));"
               "  if (rows.length === 0) return JSON.stringify({ error: 'NO_ENV_ROUTE', rows: 0 });"
               // Cual es de ENV? El cajon abierto ANTES del salto (la direccion 0 deja
               // el de la matriz) ensena la fuente de cada slot: elijo la primera fila
               // cuyo slot tenga fuente ENV 1/" + juce::String (env2Index) + " en ese cajon.
               "  const before = document.querySelector(" + open + ");"
               "  const sourceOf = (slot) => { if (!before) return -1;"
               "    const s = Array.from(before.querySelectorAll('.drawer-slot'))"
               "      .find((x) => Number(x.dataset.slot) === slot);"
               "    if (!s) return -1;"
               "    const el = s.querySelector('[data-parameter-id=\"mod' + slot + 'Source\"] select');"
               "    return el ? el.selectedIndex : -1; };"
               "  const envSource = (slot) => { const idx = sourceOf(slot);"
               "    return idx === " + juce::String (env1Index) + " || idx === " + juce::String (env2Index) + "; };"
               "  const row = rows.find((r) => envSource(Number(r.dataset.slot)));"
               "  if (!row) return JSON.stringify({ error: 'NO_ENV_ROW_VISIBLE', rows: rows.length });"
               "  const slot = Number(row.dataset.slot);"
               "  row.click();"
               "  const drawer = document.querySelector(" + open + ");"
               "  if (!drawer) return JSON.stringify({ error: 'NO_DRAWER_OPEN', rows: rows.length });"
               "  const veil = document.querySelector(" + veil + ");"
               // El resalte: la fila del cajon con data-slot-highlight='true' (el
               // panel escribe true/false en TODAS, el elegido es el unico true).
               "  const highlightedRow = Array.from(drawer.querySelectorAll(" + slotSelector + "))"
               "    .find((s) => s.dataset.slotHighlight === 'true');"
               // El cajon abierto tiene que ser el de la MATRIZ y ensenar la ruta que
               // el APVTS tiene en el slot del boton (los selects de ESE slot).
               "  const slotRows = Array.from(drawer.querySelectorAll(" + slotSelector + "));"
               "  const own = slotRows.find((s) => Number(s.dataset.slot) === slot);"
               "  const choice = (host, parameterId) => { if (!host) return -1;"
               "    const el = host.querySelector('[data-parameter-id=\"' + parameterId + '\"] select');"
               "    return el ? el.selectedIndex : -1; };"
               "  return JSON.stringify({"
               "    rows: rows.length,"
               "    clicked: slot,"
               "    opened: drawer.dataset.drawer,"
               "    veil: veil ? veil.dataset.drawerBackdrop : '',"
               "    highlighted: highlightedRow ? Number(highlightedRow.dataset.slot) : 0,"
               "    source: choice(own, 'mod' + slot + 'Source'),"
               "    destination: choice(own, 'mod' + slot + 'Destination'),"
               "    wasOpenBefore: before ? before.dataset.drawer : ''"
               "  });"
               " } catch (e) { return JSON.stringify({ error: e.message }); } })()";
    }

    /**
     * @brief Las CUATRO agujas de las curvas ADSR: 2 del lienzo + 2 del cajon.
     *
     * El envoltorio de la vista es `.env-curves` (lienzo) y `.env-blocks`
     * (cajon), con una columna/bloque `[data-envelope=env|filter]` por ADSR y
     * SU aguja como path `.envelope-curve__level` con data-visible y el 'd' que
     * pinta el nivel (`M0,y Lwidth,y`: y menor = nivel mas alto). El orden del
     * frame es [amp, filter] = [ENV 1 ('env'), ENV 2 ('filter')].
     */
    static juce::String scriptReadNeedles()
    {
        return "(() => { try {"
               "  const read = (hostClass, prefix) => {"
               "    const host = document.querySelector('.' + hostClass);"
               "    if (!host) return null;"
               "    const column = host.querySelector('[data-envelope=\"' + prefix + '\"]');"
               "    if (!column) return null;"
               "    const needle = column.querySelector('.envelope-curve__level');"
               "    if (!needle) return null;"
               "    const d = needle.getAttribute('d') ?? '';"
               "    const match = d.match(/[Ll]\\s*([0-9.]+)\\s*,\\s*([0-9.]+)/);"
               "    return { visible: needle.dataset.visible === 'true', y: match ? Number(match[2]) : -1 };"
               "  };"
               // La vista del cajon vive DENTRO del mueble (sigue en el DOM aunque
               // el cajon este cerrado; su lectura es igualmente valida).
               "  const canvasAmp = read('env-curves', 'env');"
               "  const canvasFilter = read('env-curves', 'filter');"
               "  const blocksAmp = read('env-blocks', 'env');"
               "  const blocksFilter = read('env-blocks', 'filter');"
               // El alto del viewBox da la escala de y (SSOT del dibujo:
               // ENVELOPE_VIEWBOX = 100x48, PAD = 2): nivel = (H - PAD - y)/(H - 2*PAD).
               "  const level = (n) => { if (!n || n.y < 0) return -1; const H = 48, pad = 2;"
               "    return n.visible ? Math.min(1, Math.max(0, (H - pad - n.y) / (H - pad * 2))) : 0; };"
               "  return JSON.stringify({"
               "    canvasAmp: canvasAmp, canvasFilter: canvasFilter,"
               "    blocksAmp: blocksAmp, blocksFilter: blocksFilter,"
               "    ampLevel: level(canvasAmp), filterLevel: level(canvasFilter),"
               "    blocksAmpLevel: level(blocksAmp), blocksFilterLevel: level(blocksFilter)"
               "  });"
               " } catch (e) { return JSON.stringify({ error: e.message }); } })()";
    }

    /** @brief Lectura cruda de las agujas, parseada (los cuatro objetos o vacio). */
    struct NeedleReading
    {
        bool valid = false;
        bool canvasAmp = false, canvasFilter = false, blocksAmp = false, blocksFilter = false;
        double ampLevel = -1.0, filterLevel = -1.0;
    };

    static NeedleReading parseNeedles (const juce::String& raw)
    {
        NeedleReading reading;
        const auto parsed = juce::JSON::parse (raw);
        const auto* object = parsed.getDynamicObject();

        if (object == nullptr)
            return reading;

        const auto flag = [object] (const char* key)
        {
            const auto needle = object->getProperty (key);

            if (auto* needleObject = needle.getDynamicObject())
                return static_cast<bool> (needleObject->getProperty ("visible"));

            return false;
        };

        reading.canvasAmp = flag ("canvasAmp");
        reading.canvasFilter = flag ("canvasFilter");
        reading.blocksAmp = flag ("blocksAmp");
        reading.blocksFilter = flag ("blocksFilter");
        reading.ampLevel = static_cast<double> (object->getProperty ("ampLevel"));
        reading.filterLevel = static_cast<double> (object->getProperty ("filterLevel"));
        reading.valid = true;

        return reading;
    }

    static bool needlesHidden (const NeedleReading& reading)
    {
        return reading.valid && ! reading.canvasAmp && ! reading.canvasFilter
            && ! reading.blocksAmp && ! reading.blocksFilter;
    }

    static bool needlesVisible (const NeedleReading& reading)
    {
        return reading.valid && reading.canvasAmp && reading.canvasFilter
            && reading.blocksAmp && reading.blocksFilter;
    }

    static juce::String needlesText (const NeedleReading& reading)
    {
        if (! reading.valid)
            return "ILEGIBLE";

        return juce::String ("env=") + (reading.canvasAmp ? "ON" : "off")
             + "/flt=" + (reading.canvasFilter ? "ON" : "off")
             + " (bloques " + (reading.blocksAmp ? "ON" : "off")
             + "/" + (reading.blocksFilter ? "ON" : "off") + ")";
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
    bool envRoutesOk = false;     // la ruta ENV de la ficha ENVolventes lleva a la MATRIZ (su slot resaltado)
    bool needleOk = false;        // las agujas de las curvas pintan envelopes[amp, filter] en vivo
    // Estado del sondeo de la AGUJA (ver needleSample): la ultima lectura de cada
    // fase y los niveles del motor EN la toma que dio el exito.
    bool needleWaitingVisible = true;
    bool needleHeldVisible = false;
    double needleBestSkew = 1.0e9;
    float needleNativeAmp = 0.0f, needleNativeFilter = 0.0f;
    juce::String needleHeldReading, needleReleasedReading;
    bool modelsOk = false;        // los cuatro nombres de las ranuras A-D en la pagina
    bool modelsAssetsOk = false;  // los seis assets del banco, releidos con el lector real
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
    bool freezeGuardOk = false;   // con el banco congelado, el sorteo movio solo lo suelto
    std::vector<float> beforeFreezeGuard; // APVTS antes de la pasada con freezeResonator a 1
    bool morphOk = false;         // el pad XY y el aro morph-Z, con el modelo real en D
    bool zringOk = false;         // el aro morph-Z GIRANDO con LFO2 -> 28 (telemetria nativa)

    // Estado de la colecta ZRING: instantaneas del arco (span en guiones) con
    // su marca de tiempo REAL (reloj de pared) y los numeros ya medidos.
    std::vector<double> zringTimes;
    std::vector<double> zringSpans;
    std::vector<double> zringStarts;   // guion donde NACE el arco (>5 y <95 = lado antihorario)
    double zringPeriodOn = -1.0;
    double zringMaxOn = 0.0;
    int zringLfo2Source = -1;
    int zringDest28 = -1;
    bool zringNegativeSeen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeSelftest)
};

} // namespace NEURONiK::WebUI
