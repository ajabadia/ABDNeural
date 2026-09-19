/*
  ==============================================================================

    DspReverbParityTest.cpp
    Fase 1 [5/6]: dsp::Reverb (ABDSharedCode::DspEffects, DspEffects/DspReverb.h)
    contra juce::Reverb.

    Que fija este test:

      1. QUE ES EL MISMO ALGORITMO. Mismo sample rate, mismos parametros, misma
         entrada, y se comparan muestra a muestra los DOS canales (estereo) y el
         camino mono. Un tuning de comb equivocado, un spread distinto o un
         orden de operaciones cambiado mueven el resultado en ordenes de
         magnitud, no en ulps.

      2. LA POLITICA DE DENORMALES ES LO UNICO QUE DIFIERE. DSP_UNDENORMALISE_
         JUCE_POLICY selecciona el modo:
           0 (por defecto, lo que se envia): dspUndenormalise es no-op en todas
             las plataformas, para sostener la paridad nativo <-> WASM. Aqui se
             exige coincidencia con juce::Reverb dentro de la tolerancia
             documentada, y se imprime la distancia medida.
           1: replica literal de JUCE_UNDENORMALISE (que en x86 NO es un no-op:
             (x + 0.1f) - 0.1f redondea dos veces). Aqui se exige 0 ulps, es
             decir prueba que el port es literal. Lo compila el segundo target
             (NEURONiK_DspReverbJucePolicyTest), con el define por CMake.

      Con el modo 1 en verde, el port queda probado como port; el modo 0
      demuestra que la unica diferencia es la politica, y la cuantifica.

    MEDIDO (MSVC x64, Release, 48 kHz, 24 bloques de 512, estereo y mono):
      modo 1 -> 0 ulps en las 36.864 muestras comparadas (port literal).
      modo 0 -> maxDiff = 2.384e-07 (exactamente 2 ulps de 1.0) y <= 5.632 ulps
                en muestras de cola de  |x| pequena, que es la perturbacion que
                el macro de JUCE introducia. En ARM (JUCE_INTEL sin definir, o
                sea JUCE tampoco lava) los dos modos serian identicos: la
                distancia no es una constante del algoritmo, es la politica.

      3. DETERMINISMO: dos renders del port con reset() por medio son identicos
      bit a bit (sin reloj, sin rand, sin estado oculto).

    Uso: NEURONiK_DspReverbParityTest  (ctest lo ejecuta; no toma argumentos)

  ==============================================================================
*/

#include "DspEffects/DspReverb.h"

// Este test ejercita el modulo compartido por su nombre canonico (abd::dsp). El
// alias corto `dsp::` que usa el motor vive en los shims de Source/DSP/ y lo
// cubre Tests/MidiPortTest.cpp.

#include <juce_audio_basics/juce_audio_basics.h>

// El define lo inyecta CMake solo en el target de port literal; explicito aqui
// para no depender de que un identificador sin definir valga 0 en #if.
#ifndef DSP_UNDENORMALISE_JUCE_POLICY
  #define DSP_UNDENORMALISE_JUCE_POLICY 0
#endif

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int    kBlock      = 512;
constexpr int    kBlocks     = 24;
constexpr size_t kNumSamples = (size_t) kBlock * (size_t) kBlocks;

/** Presupuesto del modo 0 (ver cabecera). Se imprime siempre la distancia real.
    Margen deliberado sobre los 2.384e-07 medidos en x86: cubre la perturbacion
    de la politica de JUCE sin dejar de delatar cualquier cambio de algoritmo
    (que moveria la salida en ordenes de magnitud). */
constexpr float kMaxAbsDiffPolicy = 1.0e-6f;

int gFailures = 0;

void check (bool ok, const char* what)
{
    if (! ok)
    {
        std::printf ("[FAIL] %s\n", what);
        ++gFailures;
    }
}

//==============================================================================
/** LCG: la entrada del test no depende de rand() ni del reloj. */
inline uint32_t nextLcg (uint32_t& state) noexcept
{
    state = state * 1664525u + 1013904223u;
    return state;
}

struct Input
{
    std::vector<float> left, right;
};

Input makeInput()
{
    Input in;
    in.left.resize (kNumSamples);
    in.right.resize (kNumSamples);

    uint32_t state = 0x12345678u;

    for (size_t i = 0; i < kNumSamples; ++i)
    {
        const float t = (float) i / (float) kSampleRate;
        const float sine  = std::sin (2.0f * 3.14159265f * 220.0f * t) * 0.4f;
        const float noise = ((float) (nextLcg (state) >> 8) / (float) (1 << 24) - 0.5f) * 0.05f;
        in.left[i]  = sine + noise;
        in.right[i] = sine * 0.8f - noise;
    }

    return in;
}

/** Parametros compartidos por los dos objetos (los structs son distintos). */
struct SharedParams
{
    float roomSize = 0.7f, damping = 0.6f, wetLevel = 0.25f;
    float dryLevel = 0.8f, width = 1.0f, freezeMode = 0.0f;
};

//==============================================================================
/** Render estereo del port. Devuelve [canal izq | canal der]. */
std::vector<float> renderPort (const Input& in, const SharedParams& p, int channels, bool resetBefore)
{
    abd::dsp::Reverb reverb;
    reverb.setSampleRate (kSampleRate);

    abd::dsp::Reverb::Parameters params;
    params.roomSize = p.roomSize; params.damping = p.damping; params.wetLevel = p.wetLevel;
    params.dryLevel = p.dryLevel; params.width = p.width;     params.freezeMode = p.freezeMode;
    reverb.setParameters (params);

    if (resetBefore)
        reverb.reset();

    std::vector<float> l = in.left, r = in.right;
    std::vector<float> out;
    out.reserve (channels == 2 ? kNumSamples * 2 : kNumSamples);

    for (int b = 0; b < kBlocks; ++b)
    {
        float* pl = l.data() + (size_t) b * kBlock;
        float* pr = r.data() + (size_t) b * kBlock;

        if (channels == 1) reverb.processMono (pl, kBlock);
        else               reverb.processStereo (pl, pr, kBlock);
    }

    out.insert (out.end(), l.begin(), l.end());
    if (channels == 2) out.insert (out.end(), r.begin(), r.end());
    return out;
}

/** Render estereo de juce::Reverb, con la misma preparacion. */
std::vector<float> renderJuce (const Input& in, const SharedParams& p, int channels)
{
    juce::Reverb reverb;
    reverb.setSampleRate (kSampleRate);

    juce::Reverb::Parameters params;
    params.roomSize = p.roomSize; params.damping = p.damping; params.wetLevel = p.wetLevel;
    params.dryLevel = p.dryLevel; params.width = p.width;     params.freezeMode = p.freezeMode;
    reverb.setParameters (params);

    std::vector<float> l = in.left, r = in.right;
    std::vector<float> out;
    out.reserve (channels == 2 ? kNumSamples * 2 : kNumSamples);

    for (int b = 0; b < kBlocks; ++b)
    {
        float* pl = l.data() + (size_t) b * kBlock;
        float* pr = r.data() + (size_t) b * kBlock;

        if (channels == 1) reverb.processMono (pl, kBlock);
        else               reverb.processStereo (pl, pr, kBlock);
    }

    out.insert (out.end(), l.begin(), l.end());
    if (channels == 2) out.insert (out.end(), r.begin(), r.end());
    return out;
}

//==============================================================================
uint32_t bitsOf (float x) noexcept
{
    uint32_t u = 0;
    std::memcpy (&u, &x, sizeof (u));
    return u;
}

/** Distancia en ulps con el mapa monotono para floats con signo. */
uint32_t ulpDistance (float a, float b) noexcept
{
    const uint32_t ia = bitsOf (a);
    const uint32_t ib = bitsOf (b);
    const uint32_t ta = (ia & 0x80000000u) ? (0x80000000u - ia) : ia;
    const uint32_t tb = (ib & 0x80000000u) ? (0x80000000u - ib) : ib;
    return ta > tb ? ta - tb : tb - ta;
}

void compareAgainstJuce (const char* label, int channels)
{
    const Input in = makeInput();
    SharedParams p;

    const std::vector<float> port = renderPort (in, p, channels, false);
    const std::vector<float> juce = renderJuce (in, p, channels);

    check (port.size() == juce.size(), "longitud de salida distinta");

    uint32_t maxUlp = 0;
    float maxDiff = 0.0f;
    size_t mismatch = 0, worstIdx = 0;

    for (size_t i = 0; i < port.size() && i < juce.size(); ++i)
    {
        const uint32_t ulp = ulpDistance (port[i], juce[i]);
        const float diff = std::fabs (port[i] - juce[i]);

        if (ulp > maxUlp) { maxUlp = ulp; worstIdx = i; }
        if (diff > maxDiff) maxDiff = diff;

#if DSP_UNDENORMALISE_JUCE_POLICY
        if (ulp != 0) ++mismatch;
#else
        if (! (diff <= kMaxAbsDiffPolicy)) ++mismatch;
#endif
    }

    std::printf ("[info] %-8s maxUlp=%-10u maxDiff=%.3e mismatched=%zu/%zu  (peor #%zu)\n",
                 label, maxUlp, (double) maxDiff, mismatch, port.size(), worstIdx);

#if DSP_UNDENORMALISE_JUCE_POLICY
    std::printf ("[info] politica de JUCE (DSP_UNDENORMALISE_JUCE_POLICY=1): se exige 0 ulps\n");
    check (mismatch == 0, "el port no coincide bit a bit con juce::Reverb bajo la politica de JUCE");
#else
    std::printf ("[info] politica del port (no-op uniforme): se exige maxDiff <= %.1e\n",
                 (double) kMaxAbsDiffPolicy);
    check (mismatch == 0, "el port se aleja de juce::Reverb mas alla de la tolerancia documentada");
#endif
}

/** reset() devuelve el objeto a un estado exacto: dos renders identicos bit a bit. */
void checkDeterminism()
{
    const Input in = makeInput();
    SharedParams p;

    const std::vector<float> first  = renderPort (in, p, 2, false);
    const std::vector<float> second = renderPort (in, p, 2, true);   // mismo objeto nuevo + reset()

    bool identical = first.size() == second.size();
    for (size_t i = 0; i < first.size() && identical; ++i)
        identical = (bitsOf (first[i]) == bitsOf (second[i]));

    check (identical, "dos renders del port no son identicos bit a bit");
}

/** Una cola larga de silencio no debe producir NaN ni infinitos (denormales y
    realimentacion de los comb filters con error). */
void checkTailIsFinite()
{
    abd::dsp::Reverb reverb;
    reverb.setSampleRate (kSampleRate);

    abd::dsp::Reverb::Parameters params;
    params.roomSize = 0.9f; params.damping = 0.1f; params.wetLevel = 0.5f;
    params.dryLevel = 0.0f; params.width = 0.5f;   params.freezeMode = 0.0f;
    reverb.setParameters (params);

    std::vector<float> l ((size_t) kBlock, 0.0f), r ((size_t) kBlock, 0.0f);
    l[0] = 1.0f;

    bool finite = true;
    for (int b = 0; b < 200; ++b)
    {
        reverb.processStereo (l.data(), r.data(), kBlock);

        for (int i = 0; i < kBlock; ++i)
        {
            if (! std::isfinite (l[i]) || ! std::isfinite (r[i]))
                finite = false;

            l[i] = 0.0f;
            r[i] = 0.0f;
        }
    }

    check (finite, "la cola del reverb produjo NaN o infinito");
}

} // namespace

//==============================================================================
int main()
{
#if DSP_UNDENORMALISE_JUCE_POLICY
    std::printf ("[info] DspReverbParityTest (modo JUCE: prueba de port literal)\n");
#else
    std::printf ("[info] DspReverbParityTest (modo enviado: politica no-op uniforme)\n");
#endif

    compareAgainstJuce ("estereo", 2);
    compareAgainstJuce ("mono", 1);
    checkDeterminism();
    checkTailIsFinite();

    if (gFailures == 0)
    {
        std::printf ("[OK] dsp::Reverb: %d comprobaciones\n", 4);
        return 0;
    }

    std::printf ("[FALLO] %d comprobacion(es)\n", gFailures);
    return 1;
}
