#pragma once

#include "../Common/SpectralModel.h"

#include <array>
#include <cmath>

namespace NEURONiK::Common {

/**
 * @brief Interpola el frame z (0..1 mapeado a [0..frameCount-1]) en `out`.
 *
 * `out` sale como snapshot estático (frameCount=1): es lo que el morfeo
 * bilineal existente consume hoy. Real-time safe (sin asignaciones).
 * Coste O(128) por llamada — la llamada es UNA por sub-bloque de 32 muestras
 * y por slot (4 slots => ~16k floats/s, despreciable).
 *
 * La frecuencia del parcial i es baseFreq*n_i + offset_i: interpolar el
 * offset linealmente ES interpolar la frecuencia linealmente. Un offset
 * absoluto grande (parcial que "salta" de armónico vecino) barredorea el
 * espectro — el diseño (TEMPORAL_MODELS_PLAN §2.2) manda interpolación por
 * camino corto envolviendo por el espaciado armónico. Sin f0 aqui, la guardia
 * conservadora es medio espaciado armónico UNITARIO (0.5*n sobre el offset)
 * cuando el fichero no declara frameSpanHz; si lo declara, la envoltura usa
 * 0.5*span (dimensión Hz real). El escritor (serialización v2) acota
 * |offset| < 0.5*span: una sola envoltura basta y los extremos z=0/z=1 se
 * copian BIT-EXACTOS (ramas tempranas).
 */
/** Copia el frame k de src a out como snapshot estático (bit-exacto). */
inline void copyFrame (const SpectralModel& src, int k, SpectralModel& out) noexcept
{
    const float* a = src.ampsOf (k);
    const float* o = src.offsetsOf (k);
    for (int i = 0; i < 64; ++i)
    {
        out.amplitudes[(size_t) i]       = a[i];
        out.frequencyOffsets[(size_t) i] = o[i];
    }
}

inline void sampleFrame (const SpectralModel& src, float z, SpectralModel& out) noexcept
{
    const int n = src.frameCount;
    out.frameCount = 1;
    out.isValid = src.isValid;
    // 2026-09-25: OFFSETS TRANSPONIBLES — el modo y su rejilla de
    // referencia viajan al snapshot (el motor lee el snapshot, no el
    // modelo fuente): sin esto el motor no sabria que escalar.
    out.offsetsTranspose = src.offsetsTranspose;
    out.offsetRootHz = src.frameSpanHz;

    if (n <= 1)
    {
        out.amplitudes = src.amplitudes;             // ruta v1: copia trivial
        out.frequencyOffsets = src.frequencyOffsets;
        out.frameF0 = 0.0f;                          // estatico: sin remapeo
        return;
    }

    // Extremos: copia directa del frame (bit-exacto, sin redondeo de lerp).
    // En los extremos el snapshot ES el frame: sin remapeo (frameF0 = 0).
    if (z <= 0.0f) { copyFrame (src, 0, out); out.frameF0 = 0.0f; return; }
    if (z >= 1.0f) { copyFrame (src, n - 1, out); out.frameF0 = 0.0f; return; }

    const float t = juce::jlimit (0.0f, 1.0f, z) * (float) (n - 1);
    const int i0 = (int) t;
    const float frac = t - (float) i0;

    const float* a0   = src.ampsOf (i0);
    const float* a1   = src.ampsOf (i0 + 1);
    const float* off0 = src.offsetsOf (i0);
    const float* off1 = src.offsetsOf (i0 + 1);

    // FASE 10.6: la raiz por frame se interpola en dominio LOG (musical:
    // el camino D4->E5 de un barrido es lineal en semitonos, no en Hz) y
    // viaja como ratio (f0_interpolada / f0_del_frame_0) para que el motor
    // remapee la rejilla con UNA multiplicacion por parcial.
    const float f0a = src.f0At (i0);
    const float f0b = src.f0At (i0 + 1);
    float ratio = 1.0f;
    if (f0a > 0.0f && f0b > 0.0f)
        ratio = std::exp2 ((std::log2 (f0b) - std::log2 (f0a)) * frac);
    out.frameF0 = f0b > 0.0f ? f0a * ratio : 0.0f;

    const float span = src.frameSpanHz > 0.0f ? src.frameSpanHz : 0.0f;

    for (int i = 0; i < 64; ++i)
    {
        out.amplitudes[(size_t) i] = a0[i] + (a1[i] - a0[i]) * frac;

        // Camino corto: envuelve el delta por el espaciado (span real en Hz;
        // fallback armónico unitario n*f0 si el fichero no lo declara). Con el
        // clamp del escritor (|offset| < span/2) una envoltura basta.
        const float fallback = (float) (i + 1);
        const float gap      = span > 0.0f ? span : fallback;
        const float d        = off1[i] - off0[i];
        const float wrapped  = (d >  0.5f * gap) ? d - gap
                             : (d < -0.5f * gap) ? d + gap
                                                 : d;
        out.frequencyOffsets[(size_t) i] = off0[i] + wrapped * frac;
    }
}


// ====================== FASE 11.3: muestreo POR CAPA ======================
//
// Hasta aqui un slot sonaba SU capa 0: el motor leia el frame raiz y las capas
// que el analizador habia separado (y que el fichero v2.1 guarda) no llegaban al
// sonido. Estas dos piezas son el camino que SI las suma, hablando solo por el
// vocabulario de capas de SpectralModel (11.1), que es el unico que conoce la
// asimetria del almacenaje (la capa 0 ES la raiz).
//
//   - sampleLayerFrame: el sampler de UNA capa, con SU propio z y sus pesos.
//   - sampleLayeredFrame: el frame efectivo de un slot = la SUMA de sus capas.
//
// Con layerCount == 1 el resultado es BIT-EXACTO al de sampleFrame: los pesos
// por defecto son 1.0 y `x * 1.0f` es exacto en IEEE-754 (tambien para -0.0 y
// para NaN), que es la condicion que exige la paridad A-E. El legado no se mueve.

/** @brief Los tres z del motor, uno por capa (morphZ, morphZ2, morphZ3). */
using LayerMorphZ = std::array<float, SpectralModel::kMaxLayers>;

/**
 * FASE 11.4: la GANANCIA de cada capa en el frame efectivo. La suma de
 * capas ya era por-capa con su propio z (11.3); esto anade el VOLUMEN
 * por capa: 1.0 = la capa suena entera, 0.0 = callada (y su z deja de
 * importar). Default 1.0 = el legado, bit a bit.
 */
using LayerGains = std::array<float, SpectralModel::kMaxLayers>;

/** @brief Todas las ganancias a 1.0: cada capa suena entera (el legado). */
inline LayerGains restLayerGains() noexcept
{
    LayerGains g {};
    g.fill (1.0f);
    return g;
}

/** @brief Todos los z a 0: el reposo de todo el legado (frame canonico). */
inline LayerMorphZ restLayerMorphZ() noexcept
{
    LayerMorphZ z {};
    z.fill (0.0f);
    return z;
}

/** @brief Peso de la capa `layer` en el frame k: mezcla estatica x temporal. */
inline float layerFrameGain (const SpectralModel& src, int layer, int k) noexcept
{
    return src.layerWeightAt (layer) * src.frameWeightAt (layer, k);
}

/** @brief Copia el frame k de la capa `layer` a `out`, con los pesos de la capa. */
inline void copyLayerFrame (const SpectralModel& src, int layer, int k, SpectralModel& out) noexcept
{
    const float gain = layerFrameGain (src, layer, k);
    const float* a = src.ampsOf (layer, k);
    const float* o = src.offsetsOf (layer, k);

    for (int i = 0; i < 64; ++i)
    {
        out.amplitudes[(size_t) i]       = a[i] * gain;
        out.frequencyOffsets[(size_t) i] = o[i];
    }
}

/**
 * @brief Interpola la CAPA `layer` de `src` en el punto z (0..1).
 *
 * Mismo algoritmo que sampleFrame —mismos extremos bit-exactos, mismo camino
 * corto de offsets por el espaciado, misma f0 interpolada en dominio log— pero
 * sobre los frames de la capa y escalando la amplitud por los pesos de la capa:
 * la mezcla estatica (`layerWeights`) por el peso temporal del frame interpolado
 * (`frameWeights`).
 *
 * La f0 por capa se interpola igual que en el camino de siempre y viaja como
 * ratio (f0_interpolada / f0_del_frame_0) para que el motor remapee la rejilla
 * con una multiplicacion por parcial. Una capa que no declara su f0 (0) no
 * remapea: es la rejilla comun del fichero.
 */
inline void sampleLayerFrame (const SpectralModel& src, int layer, float z, SpectralModel& out) noexcept
{
    const int n = src.numFramesOf (layer);
    const float gain = src.layerWeightAt (layer);

    out.frameCount = 1;
    out.isValid = src.isValid;
    // 2026-09-25: OFFSETS TRANSPONIBLES — el modo y su rejilla viajan al
    // snapshot (misma regla que sampleFrame; las capas comparten la
    // rejilla de analisis, asi que la referencia es frameSpanHz).
    out.offsetsTranspose = src.offsetsTranspose;
    out.offsetRootHz = src.frameSpanHz;

    if (n <= 1)
    {
        const float weight = gain * src.frameWeightAt (layer, 0);
        const float* a = src.ampsOf (layer, 0);
        const float* o = src.offsetsOf (layer, 0);

        for (int i = 0; i < 64; ++i)
        {
            out.amplitudes[(size_t) i]       = a[i] * weight;
            out.frequencyOffsets[(size_t) i] = o[i];
        }

        out.frameF0 = 0.0f;                          // estatico: sin remapeo
        return;
    }

    // Extremos: copia directa del frame (bit-exacto, sin redondeo de lerp).
    if (z <= 0.0f) { copyLayerFrame (src, layer, 0, out); out.frameF0 = 0.0f; return; }
    if (z >= 1.0f) { copyLayerFrame (src, layer, n - 1, out); out.frameF0 = 0.0f; return; }

    const float t = juce::jlimit (0.0f, 1.0f, z) * (float) (n - 1);
    const int i0 = (int) t;
    const float frac = t - (float) i0;

    const float w0 = src.frameWeightAt (layer, i0);
    const float w1 = src.frameWeightAt (layer, i0 + 1);
    const float weight = gain * (w0 + (w1 - w0) * frac);

    const float* a0   = src.ampsOf (layer, i0);
    const float* a1   = src.ampsOf (layer, i0 + 1);
    const float* off0 = src.offsetsOf (layer, i0);
    const float* off1 = src.offsetsOf (layer, i0 + 1);

    const float f0a = src.f0At (layer, i0);
    const float f0b = src.f0At (layer, i0 + 1);
    float ratio = 1.0f;
    if (f0a > 0.0f && f0b > 0.0f)
        ratio = std::exp2 ((std::log2 (f0b) - std::log2 (f0a)) * frac);
    out.frameF0 = f0b > 0.0f ? f0a * ratio : 0.0f;

    const float span = src.frameSpanHz > 0.0f ? src.frameSpanHz : 0.0f;

    for (int i = 0; i < 64; ++i)
    {
        out.amplitudes[(size_t) i] = (a0[i] + (a1[i] - a0[i]) * frac) * weight;

        const float fallback = (float) (i + 1);
        const float gap      = span > 0.0f ? span : fallback;
        const float d        = off1[i] - off0[i];
        const float wrapped  = (d >  0.5f * gap) ? d - gap
                             : (d < -0.5f * gap) ? d + gap
                                                 : d;
        out.frequencyOffsets[(size_t) i] = off0[i] + wrapped * frac;
    }
}

/**
 * @brief El frame EFECTIVO de un slot con capas: la SUMA de sus capas.
 *
 * Cada capa se muestrea en SU punto z (morphZ, morphZ2, morphZ3) y con SUS
 * ganancias (FASE 11.4) y SUS pesos, y las amplitudes se SUMAN indice a
 * indice: eso es "el motor suma las
 * capas". Con layerCount == 1 delega en sampleLayerFrame sin acumular, asi que
 * el camino de siempre es bit-exacto.
 *
 * El offset (y la f0) de cada parcial lo aporta la capa que MAS suena en ese
 * indice. Con el reparto que produce el analizador —cada indice pertenece a UNA
 * capa (11.2)— eso es exactamente su dueno, y el resultado es identico al de un
 * banco por capa; con capas solapadas a mano es una regla determinista y
 * documentada, porque un parcial del motor tiene UNA frecuencia, no varias.
 */
inline void sampleLayeredFrame (const SpectralModel& src, const LayerMorphZ& layerZ,
                                const LayerGains& layerGains, SpectralModel& out) noexcept;

/**
 * @brief El frame EFECTIVO (3 args, ganancias en reposo) — delega en el de 4.
 *
 * Cada capa se muestrea en SU punto z (morphZ, morphZ2, morphZ3) y con SUS
 * pesos, y las amplitudes se SUMAN indice a indice: eso es "el motor suma las
 * capas". Con layerCount == 1 delega en sampleLayerFrame sin acumular, asi que
 * el camino de siempre es bit-exacto.
 *
 * El offset (y la f0) de cada parcial lo aporta la capa que MAS suena en ese
 * indice. Con el reparto que produce el analizador —cada indice pertenece a UNA
 * capa (11.2)— eso es exactamente su dueno, y el resultado es identico al de un
 * banco por capa; con capas solapadas a mano es una regla determinista y
 * documentada, porque un parcial del motor tiene UNA frecuencia, no varias.
 */
inline void sampleLayeredFrame (const SpectralModel& src, const LayerMorphZ& layerZ,
                                SpectralModel& out) noexcept
{
    sampleLayeredFrame (src, layerZ, restLayerGains(), out);
}

/**
 * FASE 11.4: la misma suma con GANANCIA por capa. Cada capa se muestrea
 * en su z, se escala por SU ganancia (estatica ya multiplicada dentro
 * del frame por layerFrameGain) y se acumula. Con todas las ganancias a
 * 1.0 el camino es el de 11.3 bit a bit (escalar por 1.0 es exacto), y
 * el dueno del offset/f0 por indice lo decide la MISMA regla (mas
 * amplitud absoluta DESPUES de escalar: apagar una capa puede cederle
 * el mando del offset a la vecina, que es lo que se oye).
 */
inline void sampleLayeredFrame (const SpectralModel& src, const LayerMorphZ& layerZ,
                                const LayerGains& layerGains, SpectralModel& out) noexcept
{
    if (src.layerCount <= 1)
    {
        sampleLayerFrame (src, 0, layerZ[0], out);
        return;
    }

    // Una capa muestreada a la vez (scratch de 25 KB: la pila del hilo de audio
    // aguanta de sobra, y no hay asignacion ninguna).
    SpectralModel layerFrame;
    float layerF0[SpectralModel::kMaxLayers] {};
    float layerEnergy[SpectralModel::kMaxLayers] {};

    sampleLayerFrame (src, 0, layerZ[0], layerFrame);
    if (layerGains[0] != 1.0f)
        for (int i = 0; i < 64; ++i)
            layerFrame.amplitudes[(size_t) i] *= layerGains[0];
    out = layerFrame;
    layerF0[0] = layerFrame.frameF0;

    // Quien manda en cada indice: la capa con mas amplitud absoluta ahi.
    float best[64];
    for (int i = 0; i < 64; ++i)
    {
        best[i] = std::abs (out.amplitudes[(size_t) i]);
        layerEnergy[0] += best[i];
    }

    for (int l = 1; l < src.layerCount; ++l)
    {
        sampleLayerFrame (src, l, layerZ[(size_t) l], layerFrame);
        layerF0[(size_t) l] = layerFrame.frameF0;

        float energy = 0.0f;

        if (layerGains[(size_t) l] != 1.0f)
            for (int i = 0; i < 64; ++i)
                layerFrame.amplitudes[(size_t) i] *= layerGains[(size_t) l];

        for (int i = 0; i < 64; ++i)
        {
            const float amp = layerFrame.amplitudes[(size_t) i];
            const float mag = std::abs (amp);
            energy += mag;

            if (mag > best[i])
            {
                best[i] = mag;
                out.frequencyOffsets[(size_t) i] = layerFrame.frequencyOffsets[(size_t) i];
            }

            out.amplitudes[(size_t) i] += amp;
        }

        layerEnergy[(size_t) l] = energy;
    }

    // La f0 del snapshot es la de la capa DOMINANTE (la que mas energia aporta):
    // con la rejilla comun del analizador todas coinciden, y con capas que
    // cantan pitches distintos la rejilla la manda quien suena.
    int dominant = 0;
    for (int l = 1; l < src.layerCount; ++l)
        if (layerEnergy[(size_t) l] > layerEnergy[(size_t) dominant])
            dominant = l;

    out.frameF0 = layerF0[(size_t) dominant];
}

} // namespace NEURONiK::Common
