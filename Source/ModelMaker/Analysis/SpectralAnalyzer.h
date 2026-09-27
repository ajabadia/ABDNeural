/*
  ==============================================================================

    SpectralAnalyzer.h
    Created: 27 Jan 2026
    Description: Engine for extracting 64 harmonic partials from a waveform using FFT.

  ==============================================================================
*/

#pragma once

#include <array>
#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "../../../Source/Common/SpectralModel.h"
#include "LayerClustering.h"
#include "GridIndicator.h"   // 2026-09-27: las bandas del residuo viven aqui

namespace NEURONiK::ModelMaker::Analysis {

class SpectralAnalyzer
{
public:
    SpectralAnalyzer();
    ~SpectralAnalyzer() = default;

    /**
     * Performs analysis on a provided audio buffer.
     *
     * Multiframe (2026-09-22): el espectro es la MEDIA de hasta 6 ventanas
     * repartidas por TODO el fichero (antes: solo las primeras 8192 muestras,
     * con lo que un ataque o un silencio inicial definian el modelo entero).
     * Las ventanas con RMS bajo (-80 dBFS) se descartan como silencio.
     *
     * @param audio Audio buffer (mono or stereo - will mix to mono).
     * @param sampleRate The sample rate of the audio.
     * @param rootFrequency The fundamental frequency (F0) to search harmonics for.
     * @param fixedGrid REJILLA FIJA (2026-09-25, modo declarado del ModelMaker):
     *                  el f0 lo ha fijado el usuario y NO lo confirmo el
     *                  estimador. No cambia el analisis (que ya usa la rejilla
     *                  del llamador) — solo lo DECLARA en el modelo
     *                  (`gridFixed`) para que el fichero diga de donde salio su
     *                  rejilla. En analyzeTemporal, ademas, es la unica forma de
     *                  pedir que NO se siga el pitch por ventana en material
     *                  normal.
     */
    NEURONiK::Common::SpectralModel analyze(const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency,
                                            bool fixedGrid = false);

    /**
     * Attempts to detect the fundamental frequency (F0) of the audio.
     *
     * 2026-09-24: la lectura YA NO depende solo de la cabecera. El HPS del
     * primer tramo es la SEMILLA y fitGridLeastSquares() ajusta la rejilla
     * por minimos cuadrados sobre los picos de TODAS las ventanas activas
     * del fichero antes de devolver la raiz.
     */
    float detectPitch(const juce::AudioBuffer<float>& audio, double sampleRate);

    /** SUELO DEL ESTIMADOR (2026-09-25): la frecuencia mas baja que el ANCLA
        del HPS puede leer — los 50 Hz del ancla de detectPitchImpl CUANTIZADOS
        al bin (44.1 kHz: bin 9 = 48.45 Hz). Por debajo, el HPS de una ventana
        no puede leer la raiz del material: medido con E1 = 41.62 Hz con
        fundamental debil (0.35) y 2f0 dominante, detectPitch devuelve el 2o
        armonico (83.2 Hz) y el modelo entero sale UNA OCTAVA ARRIBA (501 cents
        de error acustico). La via honesta ahi es la f0 MANUAL del ModelMaker
        (DOCS/ARCHITECTURE/LAYER_SEPARATION_PLAN.MD, seccion 8): con la rejilla
        a mano el modelo es correcto (medido: mediana -1.8 cents) y
        analyzeTemporal NO re-estima la f0 por ventana por debajo de este suelo.
        Las dos alternativas se MIDIERON (2026-09-25): bajar el ancla a 35 Hz
        es un NO-OP (16/16 modelos del banco + E1 byte a byte identicos) y
        abrir la puerta de sub-octava arregla E1 pero ROMPE CZ-SWEP1 (62.6 Hz,
        una octava abajo). Es la fuente unica del suelo: detectPitchImpl y la
        politica de analyzeTemporal lo preguntan aqui. */
    static float anchorFloorHz (double sampleRate) noexcept;

    /** REJILLA FIJADA A MANO (2026-09-25): el mismo ajuste por minimos
        cuadrados que detectPitch pero con la semilla del USUARIO. Deja al
        ModelMaker PULIR el f0 escrito a mano y REPORTAR su residuo y su numero
        de picos en el indicador — lo que detectPitch no puede hacer por debajo
        del suelo, porque su HPS no llega. */
    float refineGrid (const juce::AudioBuffer<float>& audio, double sampleRate, float seedHz);

    /** FASE 10.6: HPS refinado POR VENTANA. El llamador (analyzeTemporal)
        publica el espectro del frame en magnitudeSpectrum antes de llamar. */
    float detectPitchFromSpectrum(double sampleRate);

    /** 2026-09-23: GUARDIA de desviacion de pitch para el analisis ESTATICO.
        Un modelo estatico describe UNA rejilla n*f0; si el pitch del material
        se desplaza mas de pitchGuardCents (con la octava plegada) entre
        ventanas, el modelo saldria des-afinado en silencio. El llamador
        consulta lastPitchGuardCents(): 0 (de pie) o > 0 (guardia disparada).
        El analisis temporal (analyzeTemporal) sigue el pitch por ventana y
        NO paga esta guardia: es la representacion legitima de los barridos. */
    static constexpr float pitchGuardCents = 150.0f;
    void measurePitchDeviation (const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency);
    float lastPitchGuardCents() const noexcept { return lastGuardCents; }

    /** 2026-09-24: INDICADOR de rejilla para feedback inmediato en la
        UI del ModelMaker. Tras cada detectPitch(): residuo RMS en cents
        de los picos de TODAS las ventanas contra la rejilla k*f0
        devuelta y numero de observaciones que la sostienen. Bajo = el
        material ES una serie armonica; alto = offsets, ruido o barrido
        (no hay UNA rejilla). -1 = sin datos (material insuficiente). */
    float lastGridResidualCents() const noexcept { return gridResidCents; }
    int   lastGridObservations()   const noexcept { return gridObsCount; }

    /** 2026-09-27: picos de la sub-rejilla del ultimo ajuste (no contados
        en residuo ni en LS). 0 = nada entrelazado: la sonda no tenia
        que filtrar nada. */
    int lastGridInterlacedCount() const noexcept { return gridInterlacedCount; }
    int lastGridTotalObservations() const noexcept { return gridObsCount + gridInterlacedCount; }

    /** 2026-09-26: LA FRASE del residuo, una sola para las TRES superficies del
        aviso de pitch —la fila del ModelMaker, el dialogo que BLOQUEA la
        exportacion del estatico des-afinado y el reporte de la sonda RealWav—,
        para que no puedan decir cosas distintas del MISMO material. Vive aqui,
        junto a la medida que cita, porque el analizador es quien mide: la GUI y
        la sonda solo la citan. -1 sigue siendo "n/d (material insuficiente)" en
        las tres. */
    juce::String gridResidualNotice() const;

    /** BANDAS del residuo (2026-09-25): el numero de arriba se lee por bandas
        —verde = el material ES una rejilla, amarillo = una rejilla con
        estructura de sobra (sub-armonico, offsets), naranja = no hay UNA
        rejilla (barrido, ruido)—. Viven aqui, junto al residuo que clasifican,
        para que la UI y los tests miren el MISMO umbral: el ModelMaker pinta el
        indicador con ellos y Tests/Cz101ResidualRangesTest.cpp los usa sobre los
        WAV reales del banco CZ101 (los tres tonales en verde, SWEP1 en amarillo,
        RRISE en naranja). */
    /* 2026-09-27: las constantes viven en el MODELO PURO del indicador
       (Analysis/GridIndicator.h, con SU test); aqui quedan como alias para
       que el test de rangos del banco CZ101 y la UI sigan leyendo el mismo
       numero de la misma fuente. */
    static constexpr float residualGreenCents = GridIndicatorModel::residualGreenCents;
    static constexpr float residualAmberCents = GridIndicatorModel::residualAmberCents;

    /** PUERTA DE PLEGADO DE OCTAVA (2026-09-25, plan seccion 10.3): la
        DISPERSION de las f0 por ventana DESPUES de plegar la octava es el
        discriminador mono-rejilla / bi-rejilla.

        Plegar es la equivalencia musical de octava aplicada a la medida: la
        desviacion en cents se reduce modulo 1200 al intervalo (-600, +600].
        Es lo que hace COMPARABLE la evidencia de varias ventanas: una
        inestabilidad de octava del ESTIMADOR —una ventana lee f0 y otra 2f0
        (medido en CZ-BASS1: 1 de 4 ventanas salta, 1194.4 cents de dispersion
        CRUDA)— colapsa a ~0 y el material sigue siendo de UNA rejilla: lo que
        se ha movido es la respuesta del estimador, no el material. Y una
        ventana que salta de octava arriba o abajo da el MISMO veredicto: el
        plegado solo ve la clase de octava. Lo que NO colapsa es una raiz
        genuinamente distinta: una quinta (701.96 cents) sobrevive al plegado
        como -498.04, y una cuarta da el MISMO -498.04 (la puerta ve "raiz
        distinta", no "hacia donde"); por encima del corte el material es
        bi-rejilla, la representacion de UNA rejilla por capas no es honesta y
        el material se exenta del clustering (plan 10.3, punto 3). */
    struct OctaveFold
    {
        /** Dispersion POST-PLEGADO (max |desviacion| plegada, cents): es la que
            decide. -1 = sin datos (ninguna ventana con f0 utilizable). */
        float foldCents = -1.0f;

        /** Dispersion CRUDA (sin plegar): deja a la vista el salto de octava
            del estimador (1200 = exactamente una octava). */
        float rawCents = 0.0f;

        /** Ventanas que sostienen la medida (f0 > 0). */
        int observations = 0;

        /** Ventanas cuya desviacion CRUDA pasa de media octava: el estimador
            salto de octava respecto de la referencia, y el plegado las cuenta
            como acuerdo (medido en CZ-BASS1: 1 de 4; en el test, 7 de 8). */
        int octaveFlips = 0;

        /** Veredicto. true = la dispersion post-plegado pasa de
            kOctaveFoldCents: raices genuinamente distintas (bi-rejilla) y el
            material no paga clustering. Sin datos NO hay veredicto (false: no
            se exenta nada por falta de evidencia). */
        bool biGrid = false;
    };

    /** Corte del discriminador (cents post-plegado): la linea que separa una
        rejilla de dos. Evidencia MEDIDA con la sonda sobre el banco CZ101
        (2026-09-25): BASS1 9.4, HAMOG 0.0, PAD1 4.2 y SWEP1 17.5 cents
        post-plegado (mono-rejilla; BASS1 con 1194.4 CRUDOS porque el estimador
        salta de octava en 1 de 4 ventanas) frente a RRISE 502.5 (bi-rejilla).
        El salto de octava del estimador plegado vale 0 (el test lo pinnea con
        62/124 Hz: 1200 crudos) y una quinta o una cuarta dan 498.04. Es ademas
        la tolerancia de la f0 por ventana contra la rejilla del llamador,
        porque el plegado contra esa rejilla ES esta medida (ver gridIsCommon). */
    static constexpr float kOctaveFoldCents = 100.0f;

    /** Desviacion de hz respecto de refHz en cents, PLEGADA a (-600, +600] (la
        equivalencia de octava: misma convencion que measurePitchDeviation).
        0 si alguno de los dos no es una frecuencia positiva. */
    static float foldOctaveCents (float hz, float refHz) noexcept;

    /** LA MEDIDA de la puerta: dispersion de las f0 por ventana plegadas
        contra refHz. Modulo PURO y determinista —el test de la puerta lo prueba
        con vectores de f0 sinteticos, sin audio—. Sin refHz util o sin ninguna
        ventana con f0 > 0 devuelve foldCents = -1 y biGrid = false. */
    static OctaveFold measureOctaveFold (const std::vector<float>& frameF0s, float refHz);

    /** PUERTA DE PLEGADO de la ultima llamada a analyzeTemporal(): se mide
        SIEMPRE, aunque el clustering no llegue a dar capas, para que la sonda
        (11.5) pueda declarar mono-rejilla / bi-rejilla sin depender de que
        existan capas que rechazar. (La UI del ModelMaker la consumira en la
        11.4: su indicador se actualiza al detectar la f0, antes del analisis
        temporal.) */
    const OctaveFold& lastOctaveFold() const noexcept { return lastFold; }


    /** HPS refinado (2026-09-23): el ESBOZO del pico del espectro (+-1 bin)
        elige el ANCLA antes de multiplicar; el producto k=2..4 corre sobre los
        bins CRUDOS (hps[i] = m[i]*m[2i]*m[3i]*m[4i]); el pico del producto
        queda acotado a la ventana [ancla/2, ancla*2] y solo puede bajar UNA
        octava (guardia de soporte); y la escalera de octava musical decide
        entre f0 y 2f0 (impares vacios / fundamental debil). Corrige la
        fundamental debil con armonico 2 dominante (CZ-SWEP1: 62 -> 124 Hz); es
        no-op en series armonicas limpias. Medido (2026-09-24): aplicar el
        esbozo a TODOS los bins antes del producto (en vez de solo al ancla) no
        mueve ni un modelo del banco CZ ni una decision de octava en 28 casos
        sinteticos, asi que los factores crudos se quedan a proposito: quien
        decide la octava es el ancla + la escalera, no el producto. Consume
        magnitudeSpectrum. */
    float detectPitchImpl(double sampleRate) const;

    /** Maximo local +-1 bin alrededor de center en magnitudeSpectrum. */
    int refineSpectralPeak(int center) const;
    /** FASE 10.6 + 11.2. La f0 por frame es el remapeo de pitch del analisis
        temporal: cada ventana se mide contra SU raiz. DOS EXCEPCIONES (2026-09-25)
        en las que la rejilla del llamador manda en TODOS los frames:
        (1) el modo REJILLA FIJA (`fixedGrid = true`, el modo declarado del
        ModelMaker: el usuario fija el pitch y pide que no se siga), y (2) la
        rejilla por debajo de anchorFloorHz(), donde el HPS de la ventana no puede
        leerla y una trayectoria una octava arriba es peor que no seguir nada
        (ver el plan de capas, seccion 8). En los dos casos el modelo sale con
        `gridFixed` a true y en el JSON aparece "gridFixed": true. */
    NEURONiK::Common::SpectralModel analyzeTemporal(const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency, int frameCount,
                                                    bool fixedGrid = false,
                                                    LayerMetric layerMetric = LayerMetric::Descriptors);

    /** FASE 11.2: CAPAS de la ultima llamada a analyzeTemporal(). 1 = camino
        de una sola capa (el de siempre, bit a bit); >= 2 = el modelo salio
        con capas por forma de envolvente. lastLayerOfTrace() dice a que capa
        fue a parar cada parcial (0 = inactivo o forma sin soporte). Para la
        UI (11.4) y la sonda CLI (11.5). */
    int lastLayerCount() const noexcept { return lastClustering.layerCount; }

    int lastLayerOfTrace (int partial) const noexcept
    {
        return lastClustering.layerOfTrace[(size_t) juce::jlimit (0, LayerClustering::kMaxTraces - 1, partial)];
    }

    int lastLayerTraces (int layer) const noexcept
    {
        if (layer < 0 || layer >= LayerClustering::kMaxLayers) return 0;
        return lastClustering.layerTraces[(size_t) layer];
    }

    /** 2026-09-27: residuo por capa publicado tras analyzeTemporal().
        -1 = sin datos (capa sin picos validos o sin capa). Verde <= 15 etc.
        La capa entrelazada, cuando existe, aporta SU residuo contra la
        sub-rejilla impar de f0/2 (no contra k*f0): es la medida que cae a
        verde y que la sonda debe reportar. */
    float lastLayerGridResidCents (int layer) const noexcept;
    int   lastLayerGridObsCount (int layer) const noexcept;
    juce::String lastLayerBand (int layer) const;

    /** 2026-09-27: capa entrelazada dedicada (sub-rejilla f0/2). -1 = no hay. */
    int lastEntrelazadaLayer() const noexcept { return entrelazadaLayerIndex; }
    bool lastHasEntrelazada() const noexcept { return entrelazadaLayerIndex >= 0 && entrelazadaLayerIndex < LayerClustering::kMaxLayers; }

    const LayerClustering& lastClusteringRef() const noexcept { return lastClustering; }

    /** FASE 11.2 (2026-09-26): metrica de afinidad con la que se decidieron las
        capas de la ultima llamada a analyzeTemporal() (ver LayerMetric). El
        camino por defecto es LayerMetric::Descriptors; EnvelopeCosine es la
        letra del plan (seccion 3.3) y se mide en el test de trazas. */
    LayerMetric lastLayerMetric() const noexcept { return lastMetric; }

    /** FASE 11.2 + PRE-FILTRO 10.4: la puerta de rejilla rechazo las capas (el
        material barre el pitch: su representacion es el camino de una capa con
        f0 por frame). Desde el PRE-FILTRO (10.4) la puerta decide ANTES de
        correr el clustering, asi que en material bi-rejilla esto va a true SIN
        haber calculado capas: la marca dice que la representacion de capas
        quedo descartada, no que se calcularan y se tiraran (ver
        lastClusteringSkipped()). */
    bool lastLayerGridRejected() const noexcept { return lastLayerRejected; }

    /** FASE 10.3 + PRE-FILTRO 10.4 (2026-09-25): la ultima llamada a
        analyzeTemporal() eximio al material del CLUSTERING por la puerta de
        plegado de octava: la f0 de las ventanas no cabe en la rejilla del
        llamador ni plegada, asi que la rejilla comun NO se midio y el
        clustering no corrio (el material no paga su coste). El modelo honesto
        es el de una capa con f0 por frame (10.6). false = mono-rejilla: el
        clustering corre y sus capas (si las hay) son las del modelo. */
    bool lastClusteringSkipped() const noexcept { return clusteringSkipped; }

    /** Una ventana del audio (mix mono) -> espectro acumulado en out. */
    void averageWindow(const float* left, const float* right, int start, int count, std::vector<float>& out);

private:
    float lastGuardCents = 0.0f;

    /** Estado del ultimo ajuste de rejilla (ver accessores publicos). */
    float gridResidCents = -1.0f;
    int   gridObsCount = 0;
    /** 2026-09-27: diagnostico de sub-rejilla del ultimo ajuste. */
    int   gridInterlacedCount = 0;
    /** 2026-09-27: residuo por capa tras analyzeTemporal() (ver accessores). */
    float layerGridResidCents[LayerClustering::kMaxLayers] = { -1.0f, -1.0f, -1.0f };
    int   layerGridObsCount[LayerClustering::kMaxLayers] = { 0, 0, 0 };
    int   entrelazadaLayerIndex = -1;

    /** PUERTA DE PLEGADO: el veredicto de la ultima llamada a
        analyzeTemporal() (ver lastOctaveFold()). */
    OctaveFold lastFold;

    /** FASE 11.2: capas de la ultima llamada a analyzeTemporal() + la marca
        de que la puerta de rejilla las rechazo (ver accessores publicos). */
    LayerClustering lastClustering;
    bool lastLayerRejected = false;
    /** Metrica pedida en la ultima llamada (ver lastLayerMetric()). */
    LayerMetric lastMetric = LayerMetric::Descriptors;
    /** FASE 10.3 + PRE-FILTRO 10.4: la puerta de plegado eximio al material del
        clustering en la ultima llamada a analyzeTemporal() (ver el acceso). */
    bool clusteringSkipped = false;

    /** 2026-09-24: AJUSTE DE REJILLA POR MINIMOS CUADRADOS previo a la
        lectura. La semilla del HPS ancla una banda por armonico (media del
        espaciado, igual que measurePartial) y CADA ventana activa repartida
        por el fichero aporta sus picos sub-bin como observaciones
        p_i = k_i*f0; el ajuste a traves del origen del modelo p_i = k_i*f0
        es f0* = SUM(k_i*p_i) / SUM(k_i*k_i). Promedia el ruido de la
        deteccion y el offset medio de la fuente sobre decenas de
        observaciones en vez de quedarse con una sola ventana. Dos pasadas:
        la segunda recoloca las bandas con el f0 ya ajustado. Con menos de
        kMinLsObservaciones observaciones, sin fondos o a mas de media
        octava de la semilla (barridos: no hay UNA rejilla), se devuelve la
        semilla. */
    float fitGridLeastSquares (const juce::AudioBuffer<float>& audio, double sampleRate, float seedHz);

    /** 2026-09-24: mismo minimos cuadrados POR FRAME para
        analyzeTemporal. Opera sobre el espectro ya publicado del
        frame (magnitudeSpectrum): los picos sub-bin de SUS parciales
        ajustan la raiz del frame a traves del origen, con dos
        pasadas y las mismas guardias que la rejilla global (minimo
        kMinLsObservaciones, solver valido y +-600 cents de la
        semilla HPS). La semilla sigue siendo la respuesta cuando el
        frame no tiene UNA rejilla. No toca el indicador de la UI. */
    float fitGridFromSpectrum (double sampleRate, float seedHz);
    /** 2026-09-27: BATERIA DE FAMILIAS DE OCTAVA (proto HPS) — decision
        por RESIDUO GLOBAL sobre TODAS las ventanas. El proto exploraba las
        tres familias {seed/2, seed, seed*2} sinteticas con series debiles /
        continuas; aqui la bateria vive nativa en OctaveFamilyTest.cpp y la
        decision vive en el analizador: cada candidato se ajusta por LS sobre
        los picos de TODAS las ventanas (mismo collect/solve/residuo que
        fitGridLeastSquares) y se elige el de menor residuo RMS en cents
        entre los validos (>=kMinLsObservations y a <600 cents del candidato).
        Empate a <0.5 cents => mas observaciones; si persiste, el mas cercano
        a la semilla. Candidatos por debajo del suelo del ancla no se
        consideran (la via por debajo del suelo es manual). */
public:
    struct OctaveCandidate
    {
        float candidateHz = 0.0f;
        float fittedHz    = 0.0f;
        float residCents  = -1.0f;
        int   observations = 0;
        int   interlaced   = 0;
        bool  valid = false;
    };
    std::array<OctaveCandidate, 3> evaluateOctaveCandidates (const juce::AudioBuffer<float>& audio, double sampleRate, float seedHz);
    float resolveOctaveByGlobalResidual (const juce::AudioBuffer<float>& audio, double sampleRate, float seedHz);
    // DEBUG helpers para sonda entrelazada (public for probe) — types public
    struct InterlacedFrame { std::array<float,64> amps{}; std::array<float,64> offs{}; std::array<bool,64> interlaced{}; };
    std::vector<InterlacedFrame> debugPerFrameCommon(const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency, int frameCount);
    std::array<int,64> debugInterlacedCountPerPartial(const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency);

private:


    /** Observaciones minimas del ajuste para creerle mas que a la semilla. */
    static constexpr int kMinLsObservations = 4;
    // FFT Configuration
    static constexpr int fftOrder = 13; // 2^13 = 8192 points
    static constexpr int fftSize = 1 << fftOrder;

    /** Ventanas analizadas por analyze(): cubre ficheros de hasta ~1 s con 6
        disparos; mas largo no mejora el modelo (espectro estacionario). */
    static constexpr int maxFrames = 6;

    /** Umbral de actividad por ventana (RMS): debajo, silencio y fuera. */
    static constexpr float kFrameRmsFloor = 1.0e-4f;

    /** Un parcial por debajo de esta fraccion del maximo global no es tal:
        suelo de deteccion (-60 dB) para no asignar offsets al ruido. */
    static constexpr float kPartialFloor = 1.0e-3f;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris };

    std::vector<float> fftData;
    std::vector<float> magnitudeSpectrum;

    /** Espectro temporal reutilizable entre llamadas/frames. */
    std::vector<float> frameSpectrum;

    /** Medida de UN parcial: magnitud (bin del pico) y desviacion sub-bin.
        2026-09-27: REJILLAS ENTRELAZADAS. Un pico sobre la sub-rejilla
        (impar de f0/2) NO es inharmonicidad de k*f0, sino energia de la
        otra familia (phasor CZ). El flag `interlaced` lo marca para que
        offsets (offset=0) y residuo (no entra al LS) no se contaminen.
        2026-09-27 (+capa entrelazada): `peakHz` conserva la posicion real del
        pico sub-bin para que la capa entrelazada suene en SU rejilla (impar
        de f0/2) aunque su indice sea el de k*f0. */
    struct PartialMeasurement
    {
        float amplitude = 0.0f;
        float offsetHz = 0.0f;
        float peakHz = 0.0f;        // posicion real del pico (bin+delta)*binWidth
        bool  interlaced = false;   // pico de la sub-rejilla (f0/2 impar)
    };

    /** 2026-09-27: diagnostico de rejilla entrelazada. Un pico a <0.25*f0
        de un impar de f0/2 y a >0.25*f0 de k*f0 es la otra familia. */
    static bool isInterlacedPeak(float peakHz, float targetFreq, float rootFrequency) noexcept;

    /**
     * Peak-picking (ventana = mitad del espaciado entre armonicos f0) sobre
     * el espectro medio + interpolacion parabolica
     * sub-bin (en dB) del pico local. Devuelve la magnitud del pico y el
     * desajuste del parcial real respecto a n*f0 (frequencyOffset del modelo).
     * Sin pico por encima del suelo -> amplitude medida y offset 0.
     */

    PartialMeasurement measurePartial(float targetFreq, float rootFrequency, double sampleRate) const;

    /** FFT de una ventana ya copiada en fftData: deja magnitudes 0..fftSize/2. */
    void transformCurrentFrame(int numSamples);

    /** FASE 11.2 + PUERTA DE PLEGADO: la f0 de TODAS las ventanas cabe en la
        rejilla del llamador UNA VEZ PLEGADA LA OCTAVA (la convencion de
        measurePitchDeviation)? Basta con que una ventana se salga de
        kOctaveFoldCents para que las capas se rechacen. Es exactamente el
        veredicto de measureOctaveFold() contra la rejilla del llamador, y el
        que publica lastOctaveFold(): con capas, todas compartirian ESA rejilla;
        si el material la mueve, el modelo honesto es el de una capa con f0 por
        frame (10.6). */
    bool gridIsCommon (const std::vector<float>& frameF0s, float rootFrequency) const;

    /** FASE 11.2: modelo con capas a partir de la lectura de la rejilla
        COMUN (perFrameCommon) y la asignacion del clustering. La capa 0 es
        la dominante y ocupa la RAIZ (compatibilidad v2: un lector viejo
        suena esa capa); cada capa escribe SOLO sus indices y su peso
        temporal w_l[f] normalizado al maximo de la capa. */
    NEURONiK::Common::SpectralModel buildLayeredModel (
        const std::vector<std::array<PartialMeasurement, 64>>& frames,
        const LayerClustering& clusters, int nFrames, float rootFrequency) const;
    float layerResidualCents (const std::vector<std::array<PartialMeasurement, 64>>& frames,
                              const std::vector<int>& tracesOfLayer,
                              float rootFrequency, bool isEntrelazada, int nFrames) const;
    NEURONiK::Common::SpectralModel buildEntrelazadaModel (
        const std::vector<std::array<PartialMeasurement, 64>>& frames,
        const std::vector<int>& entrelazadaTraces,
        const LayerClustering& fallbackClusters,
        int nFrames, float rootFrequency) const;
};

} // namespace NEURONiK::ModelMaker::Analysis
