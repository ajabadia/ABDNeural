/*
  ==============================================================================

    SpectralModel.h
    Created: 27 Jan 2026
    Description: Shared definition of the Spectral Model structure.
                 Used by both NEURONiK Plugin and Model Maker.

  ==============================================================================
*/

#pragma once

#include <array>
#include <juce_core/juce_core.h>

namespace NEURONiK::Common {

/**
 * @struct SpectralModel
 * @brief Holds a snapshot of 64 partials, including amplitudes and frequency offsets.
 * This structure defines a specific timbre that the Resonator can synthesize.
 *
 * FASE 10 (2026-09-22): el modelo es TEMPORAL. `amplitudes`/`frequencyOffsets`
 * siguen siendo EL frame canonico (indice 0) en layout 1D — todo el codigo v1
 * (bridge WASM con memcpy de 128 floats, ModelMaker, cargador de presets)
 * compila sin cambios y lee el frame 0. Los frames 1..N-1 de la evolucion
 * temporal viven en extraAmps/extraOffsets; frameCount==1 significa modelo
 * estatico (todo el legado) y el motor no toca nada de lo nuevo.
 */
struct SpectralModel
{
    static constexpr int kMaxFrames = 16;

    // FASE 11.1: presupuesto de CAPAS del plan de separacion (kPolydroneMaxLayers).
    // Una capa es una pelicula espectral propia sobre la MISMA rejilla n*f0; el
    // motor las suma. 1 capa = modelo v2 puro: todo el legado, bit a bit.
    static constexpr int kMaxLayers = 3;
    static_assert (kMaxLayers >= 2, "extraLayers necesita al menos una capa extra");

    // Frame 0 (canonico) — layout v1 EXACTO. Compat: bridge WASM, ModelMaker,
    // cargador de presets y cualquier consumidor externo.
    // 2026-09-25: inicializados en el CONSTRUCTOR ({}), como el resto de los
    // arrays del struct. Eran los UNICOS sin inicializador, asi que un
    // SpectralModel por defecto llevaba basura de pila en los 64 parciales; lo
    // destapo el test de la vista de capas (11.5), que lee el modelo entero.
    // Un modelo por defecto tiene que estar VACIO, no con lo que hubiera antes.
    std::array<float, 64> amplitudes {};
    std::array<float, 64> frequencyOffsets {};

    // Frames 1..kMaxFrames-1 (evolucion temporal; solo validos si
    // frameCount > k). frameCount==1 => estatico, aqui no hay nada util.
    std::array<std::array<float, 64>, kMaxFrames - 1> extraAmps;
    std::array<std::array<float, 64>, kMaxFrames - 1> extraOffsets;

    int  frameCount = 1;        // 1..kMaxFrames; 1 = modelo estatico (legado)
    // FASE 10: espaciado entre parciales del analisis (f0 del frame), en Hz.
    // 0 = desconocido (modelos sinteticos): el sampler usa el espaciado
    // armonico unitario n*f0. Dimensiona la envoltura de offsets (camino corto).
    float frameSpanHz = 0.0f;
    // FASE 10.6: raiz POR FRAME en Hz (0 = la del frame canonico). Un modelo
    // con frames f0 distintos "canta" el barrido de pitch del WAV original
    // (CZ-RRISE): el motor remapea la rejilla n*base al renderizar.
    std::array<float, kMaxFrames - 1> extraF0 {};
    // FASE 10.6: raiz del ULTIMO snapshot (solo la rellena sampleFrame;
    // 0 = sin remapeo: el modelo canonico/estatico no mueve la rejilla).
    float frameF0 = 0.0f;
    // 2026-09-25: REJILLA FIJA — el modo declarado del ModelMaker. true = el
    // usuario FIJO la f0 y el analizador no sigue el pitch por ventana: los
    // frameF0 son la rejilla declarada, no una trayectoria medida. Es
    // procedencia (un lector que remapee pitch debe saber que no hay trayectoria
    // que seguir y que el f0 no lo confirmo el estimador). Aditivo: en el JSON
    // solo aparece cuando es true, asi que los ficheros de siempre no cambian ni
    // un byte y los lectores viejos no ven nada nuevo.
    bool gridFixed = false;
    // 2026-09-25: OFFSETS TRANSPONIBLES (modo delta-n/n). true = los
    // frequencyOffsets guardados (en Hz) se midieron contra UNA rejilla de
    // referencia (offsetRootHz, normalmente frameSpanHz) y deben
    // INTERPRETARSE como ratios r_n = 1 + offset/(n*f0_ref): al renderizar a
    // otra base el motor los escala por baseFrequency/offsetRootHz, de modo
    // que la desviacion en cents de cada parcial respecto de su armonico
    // (offset/(n*f0)) NO cambia al transportar con el teclado. Sin el modo
    // (false, todo el legado) el factor es 1.0 exacto y el motor suma el
    // offset tal cual, bit a bit como siempre. Aditivo: en el JSON solo
    // aparece cuando es true, asi que los ficheros de siempre no cambian ni
    // un byte y los lectores viejos no ven nada nuevo.
    bool offsetsTranspose = false;
    // Rejilla de referencia (Hz) contra la que se midieron los offsets del
    // modo anterior; 0 = desconocida (el modo queda inerte: factor 1.0). El
    // sampler la TRANSPORTA al snapshot desde frameSpanHz, porque el motor
    // lee el snapshot muestreado, no el modelo fuente.
    float offsetRootHz = 0.0f;
    bool isValid = false;

    // ===================== FASE 11.1: CAPAS =================================
    // Extension ADITIVA y OPCIONAL: con layerCount == 1 (el default, y todo el
    // legado) los campos de aqui abajo quedan inertes y el sonido es el de
    // siempre. Los indices de capa son 0-based: la capa 0 ES la raiz; las capas
    // 1..kMaxLayers-1 viven en extraLayers.

    /** Los frames de una capa que NO es la raiz. La capa 0 no tiene copia aqui:
        SUS frames SON los de la raiz de arriba (amplitudes/extraAmps/
        extraOffsets/extraF0/frameSpanHz), asi que un lector v2 antiguo que solo
        lee la raiz suena la capa 0 sin enterarse de que hay mas — y no hay dos
        copias de lo mismo que puedan divergir. */
    struct LayerFrames
    {
        std::array<std::array<float, 64>, kMaxFrames> amplitudes {};
        std::array<std::array<float, 64>, kMaxFrames> frequencyOffsets {};
        std::array<float, kMaxFrames> f0 {};   // 0 = no declarada (rejilla comun)
        int frameCount = 1;
    };

    /** Capas 1..kMaxLayers-1 (la capa l vive en extraLayers[l-1]). */
    std::array<LayerFrames, kMaxLayers - 1> extraLayers {};

    /** Numero de capas activas (1..kMaxLayers). 1 = v2 puro. */
    int layerCount = 1;

    /** Mezcla estatica por capa (0..1). Indice 0 = la raiz. */
    std::array<float, kMaxLayers> layerWeights { 1.0f, 1.0f, 1.0f };

    /** Peso temporal por frame y capa (0..1). 1.0 = la capa suena tal cual: es
        el valor de un modelo v2 puro, que no trae frameWeights. */
    std::array<std::array<float, kMaxFrames>, kMaxLayers> layerFrameWeights = []
    {
        std::array<std::array<float, kMaxFrames>, kMaxLayers> rows {};

        for (auto& row : rows)
            row.fill (1.0f);

        return rows;
    } ();

    /** Nombre de capa que viaja en el fichero v2.1 (vacio en v2 puro). */
    std::array<juce::String, kMaxLayers> layerNames {};

    int numFrames() const noexcept { return frameCount; }

    /** @brief Factor que escala UN offset del modo transpuesto cuando la
        nota renderizada es `baseHz`: baseHz/rejilla. Sin el modo declarado,
        o sin rejilla de referencia, devuelve 1.0 EXACTO (el legado: el
        offset se suma tal cual).

        La rejilla es `offsetRootHz` —la que TRANSPORTA el sampler al
        snapshot, porque el snapshot no lleva frameSpanHz— y si no esta
        rellena cae a `frameSpanHz`: sobre el modelo FUENTE (la GUI, un
        test, el preview) el helper dice lo mismo que dira el motor. */
    float offsetScaleAt (float baseHz) const noexcept
    {
        if (! offsetsTranspose) return 1.0f;

        const float root = offsetRootHz > 0.0f ? offsetRootHz : frameSpanHz;
        return root > 0.0f ? baseHz / root : 1.0f;
    }

    /** @brief Amplitud del parcial i en el frame k (0..frameCount-1), sin copia. */
    float ampAt (int frame, int partial) const noexcept
    {
        return frame <= 0 ? amplitudes[(size_t) partial]
                          : extraAmps[(size_t) frame - 1][(size_t) partial];
    }

    /** @brief Offset de frecuencia del parcial i en el frame k. */
    float offsetAt (int frame, int partial) const noexcept
    {
        return frame <= 0 ? frequencyOffsets[(size_t) partial]
                          : extraOffsets[(size_t) frame - 1][(size_t) partial];
    }

    float* ampsOf (int frame) noexcept
    {
        return frame <= 0 ? amplitudes.data() : extraAmps[(size_t) frame - 1].data();
    }

    float* offsetsOf (int frame) noexcept
    {
        return frame <= 0 ? frequencyOffsets.data() : extraOffsets[(size_t) frame - 1].data();
    }

    /** @brief Raiz (f0) del frame k en Hz. Frame 0 => frameSpanHz (canonico). */
    float f0At (int frame) const noexcept
    {
        return frame <= 0 ? frameSpanHz : extraF0[(size_t) frame - 1];
    }

    /** @brief Fija la raiz del frame k (solo frames >= 1: el canonico es frameSpanHz). */
    void setF0At (int frame, float f0Hz) noexcept
    {
        if (frame >= 1 && frame < kMaxFrames)
            extraF0[(size_t) frame - 1] = f0Hz;
    }

    const float* ampsOf (int frame) const noexcept
    {
        return frame <= 0 ? amplitudes.data() : extraAmps[(size_t) frame - 1].data();
    }

    const float* offsetsOf (int frame) const noexcept
    {
        return frame <= 0 ? frequencyOffsets.data() : extraOffsets[(size_t) frame - 1].data();
    }

    // --- API de CAPAS (Fase 11.1) -------------------------------------------
    // Un solo vocabulario para 1..kMaxLayers capas: la capa 0 se resuelve contra
    // la raiz de arriba y las capas 1..2 contra extraLayers. El lector/escritor
    // v2.1 (PresetManager / SpectralModelWriter.h) y el motor (Fase 11.3) hablan
    // SOLO por aqui, asi que la asimetria del almacenaje (la capa 0 ES la raiz)
    // no se filtra al llamante. El indice de capa va clampeado a
    // [0, kMaxLayers-1] y el de frame a [0, kMaxFrames-1].

    bool isLayered() const noexcept { return layerCount > 1; }

    void setLayerCount (int layers) noexcept
    {
        layerCount = juce::jlimit (1, kMaxLayers, layers);
    }

    /** @brief Frames de la capa l (una capa no declarada tiene 1). */
    int numFramesOf (int layer) const noexcept
    {
        if (layer <= 0) return frameCount;

        return extraLayers[(size_t) (juce::jlimit (1, kMaxLayers - 1, layer) - 1)].frameCount;
    }

    void setNumFramesOf (int layer, int frames) noexcept
    {
        const int count = juce::jlimit (1, kMaxFrames, frames);

        if (layer <= 0) { frameCount = count; return; }

        extraLayers[(size_t) (juce::jlimit (1, kMaxLayers - 1, layer) - 1)].frameCount = count;
    }

    /** @brief Amplitud del parcial i en el frame f de la capa l. */
    float ampAt (int layer, int frame, int partial) const noexcept
    {
        return ampsOf (layer, frame)[(size_t) partial];
    }

    /** @brief Offset de frecuencia del parcial i en el frame f de la capa l. */
    float offsetAt (int layer, int frame, int partial) const noexcept
    {
        return offsetsOf (layer, frame)[(size_t) partial];
    }

    const float* ampsOf (int layer, int frame) const noexcept
    {
        const int l = juce::jlimit (0, kMaxLayers - 1, layer);
        const int f = juce::jlimit (0, kMaxFrames - 1, frame);

        return l == 0 ? ampsOf (f)
                      : extraLayers[(size_t) (l - 1)].amplitudes[(size_t) f].data();
    }

    float* ampsOf (int layer, int frame) noexcept
    {
        const int l = juce::jlimit (0, kMaxLayers - 1, layer);
        const int f = juce::jlimit (0, kMaxFrames - 1, frame);

        return l == 0 ? ampsOf (f)
                      : extraLayers[(size_t) (l - 1)].amplitudes[(size_t) f].data();
    }

    const float* offsetsOf (int layer, int frame) const noexcept
    {
        const int l = juce::jlimit (0, kMaxLayers - 1, layer);
        const int f = juce::jlimit (0, kMaxFrames - 1, frame);

        return l == 0 ? offsetsOf (f)
                      : extraLayers[(size_t) (l - 1)].frequencyOffsets[(size_t) f].data();
    }

    float* offsetsOf (int layer, int frame) noexcept
    {
        const int l = juce::jlimit (0, kMaxLayers - 1, layer);
        const int f = juce::jlimit (0, kMaxFrames - 1, frame);

        return l == 0 ? offsetsOf (f)
                      : extraLayers[(size_t) (l - 1)].frequencyOffsets[(size_t) f].data();
    }

    /** @brief Raiz (f0) del frame f de la capa l. La capa 0 hereda la raiz del
        frame canonico (frameSpanHz) y los frames extra de arriba; las capas
        1..2 llevan la suya (0 = no declarada: la rejilla comun del fichero). */
    float f0At (int layer, int frame) const noexcept
    {
        const int l = juce::jlimit (0, kMaxLayers - 1, layer);
        const int f = juce::jlimit (0, kMaxFrames - 1, frame);

        return l == 0 ? f0At (f) : extraLayers[(size_t) (l - 1)].f0[(size_t) f];
    }

    void setF0At (int layer, int frame, float f0Hz) noexcept
    {
        const int l = juce::jlimit (0, kMaxLayers - 1, layer);
        const int f = juce::jlimit (0, kMaxFrames - 1, frame);

        if (l == 0) { setF0At (f, f0Hz); return; }   // capa 0: el canonico es frameSpanHz

        extraLayers[(size_t) (l - 1)].f0[(size_t) f] = f0Hz;
    }

    /** @brief Peso temporal del frame f de la capa l (1.0 = sin modulacion). */
    float frameWeightAt (int layer, int frame) const noexcept
    {
        const int l = juce::jlimit (0, kMaxLayers - 1, layer);
        const int f = juce::jlimit (0, kMaxFrames - 1, frame);

        return layerFrameWeights[(size_t) l][(size_t) f];
    }

    void setFrameWeightAt (int layer, int frame, float weight) noexcept
    {
        const int l = juce::jlimit (0, kMaxLayers - 1, layer);
        const int f = juce::jlimit (0, kMaxFrames - 1, frame);

        layerFrameWeights[(size_t) l][(size_t) f] = juce::jlimit (0.0f, 1.0f, weight);
    }

    /** @brief Mezcla estatica de la capa l (0..1). La capa 0 es la raiz. */
    float layerWeightAt (int layer) const noexcept
    {
        return layerWeights[(size_t) juce::jlimit (0, kMaxLayers - 1, layer)];
    }

    void setLayerWeightAt (int layer, float weight) noexcept
    {
        layerWeights[(size_t) juce::jlimit (0, kMaxLayers - 1, layer)] =
            juce::jlimit (0.0f, 1.0f, weight);
    }

    const juce::String& layerNameAt (int layer) const noexcept
    {
        return layerNames[(size_t) juce::jlimit (0, kMaxLayers - 1, layer)];
    }

    void setLayerNameAt (int layer, const juce::String& name)
    {
        layerNames[(size_t) juce::jlimit (0, kMaxLayers - 1, layer)] = name;
    }
};

} // namespace NEURONiK::Common
