/*
  ==============================================================================

    DspEffectsParityTest.cpp
    Migracion de efectos (continuacion de la Fase 1 [5/6], que movio la reverb):
    los envoltorios de producto de NEURONiK (Source/DSP/Effects/{Chorus,Delay,
    Saturation}.h, namespace NEURONiK::DSP::Effects) sobre los motores del modulo
    compartido ABDSharedCode::DspEffects, contra una REFERENCIA CONGELADA.

    Que fija este test:

      1. QUE LA MIGRACION FUE UNA MIGRACION, NO UNA REESCRITURA. Las clases
         Reference* de abajo son la copia literal del efecto tal y como estaba en
         Source/DSP/Effects/ antes de moverse a DspEffects (mismo suavizado
         incluido). Misma entrada determinista, mismo guion de parametros por
         bloque, dos renders, y se comparan muestra a muestra TODOS los canales:
         se exige 0 ulps.

         NOTA (2026-09-19, matematica determinista): las Reference* llaman ahora a
         abd::dsp::sin/atan (DspCore/DspMath.h) en vez de std::sin/atan. Sustituir
         la libm de la plataforma por una implementacion determinista es un cambio
         de sonido DELIBERADO (el motor es pre-1.0) y es lo que cierra la paridad
         bit a bit nativo <-> WASM del escenario C de WasmParityTest. Este test ya
         no prueba ESE cambio (seria circular): sigue probando que el envoltorio de
         producto y el motor compartido dan la misma salida bit a bit (suavizado,
         mapeo y orden incluidos) y que la cola no diverge.

         Aqui el margen no es negociable, al contrario que en el port de la
         reverb: no hay una implementacion ajena contra la que comparar (no es un
         port de JUCE), asi que la unica prueba de que el sonido no cambio es que
         la aritmetica sea identica. Si alguien toca el motor compartido, este
         test dice si cambio la salida y en que muestra.

      2. DETERMINISMO: dos objetos nuevos con el mismo guion dan identicos bit a
         bit (ni reloj, ni rand, ni estado oculto).

      3. COLA FINITA: saturacion + chorus + delay realimentado (fb 0.95) y una
         cola larga de silencio no producen NaN ni infinitos.

    Uso: NEURONiK_DspEffectsParityTest  (ctest lo ejecuta; no toma argumentos)

  ==============================================================================
*/

#include "DspCore.h"
#include "Effects/Chorus.h"
#include "Effects/Delay.h"
#include "Effects/Saturation.h"

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

/** Canales de entrada: 1..3. Se generan una vez y los dos renders usan lo mismo. */
std::vector<std::vector<float>> makeInput (int channels)
{
    std::vector<float> left (kNumSamples), right (kNumSamples);
    uint32_t state = 0x12345678u;

    for (size_t i = 0; i < kNumSamples; ++i)
    {
        const float t = (float) i / (float) kSampleRate;
        const float sine  = std::sin (2.0f * 3.14159265f * 220.0f * t) * 0.4f;
        const float noise = ((float) (nextLcg (state) >> 8) / (float) (1 << 24) - 0.5f) * 0.05f;
        left[i]  = sine + noise;
        right[i] = sine * 0.8f - noise;
    }

    std::vector<std::vector<float>> in;
    in.push_back (left);

    if (channels >= 2)
    {
        in.push_back (right);
    }

    if (channels >= 3)
    {
        // Tercer canal: ejercita el `channel % 2` del motor (comparte buffer con
        // el canal 0, como en el original).
        std::vector<float> third (kNumSamples);

        for (size_t i = 0; i < kNumSamples; ++i)
            third[i] = left[i] * 0.25f - right[i] * 0.5f;

        in.push_back (std::move (third));
    }

    return in;
}

//==============================================================================
// REFERENCIA CONGELADA - copia literal del efecto ANTES de moverlo a
// ABDSharedCode::DspEffects. No se toca: es el punto de comparacion. Si un dia
// se cambia a proposito el comportamiento, se actualiza la referencia en el
// mismo commit que documenta el cambio de sonido.

class ReferenceChorus
{
public:
    ReferenceChorus() : delayBuffer(2, 4096)
    {
        delayBuffer.clear();
    }

    void prepare(double sampleRate)
    {
        currentSampleRate = sampleRate;
        delayBuffer.setSize(2, static_cast<int>(sampleRate * 0.1)); // 100ms max delay
        delayBuffer.clear();
        phase = 0.0f;

        rateSmoother.reset(sampleRate, 0.02);
        depthSmoother.reset(sampleRate, 0.02);
        mixSmoother.reset(sampleRate, 0.02);
    }

    void setParameters(float rateHz, float depth, float mix) noexcept
    {
        rateSmoother.setTargetValue(rateHz);
        depthSmoother.setTargetValue(depth);
        mixSmoother.setTargetValue(mix);
    }

    void setMix(float mix) noexcept
    {
        mixSmoother.setTargetValue(mix);
    }

    void processBlock(dsp::AudioBuffer<float>& buffer)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        const int bufferSize = delayBuffer.getNumSamples();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float currentRate = rateSmoother.getNextValue();
            float currentDepth = depthSmoother.getNextValue();
            float currentMix = mixSmoother.getNextValue();

            float phaseInc = dsp::MathConstants<float>::twoPi * currentRate / static_cast<float>(currentSampleRate);

            // Modulation: LFO between 5ms and 30ms
            float mod = (abd::dsp::sin(phase) + 1.0f) * 0.5f; // 0 to 1
            float delaySamples = (0.005f + mod * 0.025f * currentDepth) * static_cast<float>(currentSampleRate);

            for (int channel = 0; channel < numChannels; ++channel)
            {
                float inputSample = buffer.getReadPointer(channel)[sample];

                // Write input to delay buffer
                delayBuffer.setSample(channel % 2, writePos, inputSample);

                // Read modulated position
                float readPos = static_cast<float>(writePos) - delaySamples;
                if (readPos < 0) readPos += static_cast<float>(bufferSize);

                int index1 = static_cast<int>(readPos);
                int index2 = (index1 + 1) % bufferSize;
                float fraction = readPos - static_cast<float>(index1);

                float delayedSample = (1.0f - fraction) * delayBuffer.getSample(channel % 2, index1) +
                                      fraction * delayBuffer.getSample(channel % 2, index2);

                // Mix
                float output = (inputSample * (1.0f - currentMix * 0.5f)) + (delayedSample * currentMix * 0.5f);
                buffer.getWritePointer(channel)[sample] = output;
            }

            phase += phaseInc;
            if (phase >= dsp::MathConstants<float>::twoPi) phase -= dsp::MathConstants<float>::twoPi;

            if (++writePos >= bufferSize) writePos = 0;
        }
    }

    void reset()
    {
        delayBuffer.clear();
        rateSmoother.setCurrentAndTargetValue(rateSmoother.getTargetValue());
        depthSmoother.setCurrentAndTargetValue(depthSmoother.getTargetValue());
        mixSmoother.setCurrentAndTargetValue(mixSmoother.getTargetValue());
    }

private:
    dsp::AudioBuffer<float> delayBuffer;
    int writePos = 0;
    float phase = 0.0f;
    double currentSampleRate = 44100.0;

    dsp::LinearSmoothedValue<float> rateSmoother { 1.0f };
    dsp::LinearSmoothedValue<float> depthSmoother { 0.2f };
    dsp::LinearSmoothedValue<float> mixSmoother { 0.0f };
};

class ReferenceDelay
{
public:
    ReferenceDelay() : delayBuffer(2, 96000) // Default 2s @ 48kHz
    {
        delayBuffer.clear();
    }

    void prepare(double sampleRate, int maxDelaySamples)
    {
        currentSampleRate = sampleRate;
        delayBuffer.setSize(2, maxDelaySamples + 1024);
        delayBuffer.clear();
        writePos = 0;

        timeSmoother.reset(sampleRate, 0.05); // 50ms ramp for delay time to avoid pitch jumps
        feedbackSmoother.reset(sampleRate, 0.02); // 20ms ramp
    }

    void setParameters(float timeInSeconds, float feedback, float mix = 0.5f) noexcept
    {
        dsp::ignoreUnused(mix);
        timeSmoother.setTargetValue(timeInSeconds * static_cast<float>(currentSampleRate));
        feedbackSmoother.setTargetValue(dsp::jlimit(0.0f, 0.95f, feedback));
    }

    void processBlock(dsp::AudioBuffer<float>& buffer)
    {
        dsp::ScopedNoDenormals noDenormals;
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        const int bufferSize = delayBuffer.getNumSamples();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float currentDelay = timeSmoother.getNextValue();
            float currentFB = feedbackSmoother.getNextValue();

            for (int channel = 0; channel < numChannels; ++channel)
            {
                float inputSample = buffer.getReadPointer(channel)[sample];

                // Read from delay buffer (Linear Interpolation)
                float readPos = static_cast<float>(writePos) - currentDelay;
                if (readPos < 0) readPos += static_cast<float>(bufferSize);

                int index1 = static_cast<int>(readPos);
                int index2 = (index1 + 1) % bufferSize;
                float fraction = readPos - static_cast<float>(index1);

                float delayedSample = (1.0f - fraction) * delayBuffer.getSample(channel % 2, index1) +
                                      fraction * delayBuffer.getSample(channel % 2, index2);

                // Write to delay buffer (Input + Feedback)
                delayBuffer.setSample(channel % 2, writePos, inputSample + (delayedSample * currentFB));

                // Mix
                buffer.getWritePointer(channel)[sample] += delayedSample * 0.5f;
            }

            if (++writePos >= bufferSize) writePos = 0;
        }
    }

    void reset()
    {
        delayBuffer.clear();
        timeSmoother.setCurrentAndTargetValue(timeSmoother.getTargetValue());
        feedbackSmoother.setCurrentAndTargetValue(feedbackSmoother.getTargetValue());
    }

private:
    dsp::AudioBuffer<float> delayBuffer;
    int writePos = 0;
    double currentSampleRate = 44100.0;

    dsp::LinearSmoothedValue<float> timeSmoother;
    dsp::LinearSmoothedValue<float> feedbackSmoother;
};

class ReferenceSaturation
{
public:
    ReferenceSaturation() noexcept = default;
    ~ReferenceSaturation() = default;

    void prepare(double sampleRate) noexcept
    {
        driveSmoother.reset(sampleRate, 0.02); // 20ms ramp
    }

    void setAmount(float amount) noexcept {
        driveSmoother.setTargetValue(1.0f + (amount * 4.0f));
    }

    void setDrive(float drive) noexcept {
        setAmount(drive);
    }

    inline float processSample(float input) noexcept
    {
        float x = input * driveSmoother.getNextValue();
        return abd::dsp::atan(x) * 0.63661977236f; // 2/PI constant
    }

    void processBlock(dsp::AudioBuffer<float>& buffer) noexcept
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        for (int s = 0; s < numSamples; ++s)
        {
            float currentDrive = driveSmoother.getNextValue();

            // Optimization: if drive is approx 1.0, do nothing (1.0 is the baseline)
            if (currentDrive > 1.001f)
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    float x = buffer.getSample(ch, s) * currentDrive;
                    buffer.setSample(ch, s, abd::dsp::atan(x) * 0.63661977236f);
                }
            }
        }
    }

    void resetState() noexcept
    {
        driveSmoother.setCurrentAndTargetValue(1.0f);
    }

private:
    dsp::LinearSmoothedValue<float> driveSmoother { 1.0f };
};

//==============================================================================
/** Copia el bloque `b` del render al `dsp::AudioBuffer` que recibe el efecto.

    Se copia (en vez de construir un AudioBuffer que apunte a los canales) para
    que el efecto vea exactamente el mismo tipo y la misma forma de acceso que en
    el motor: un AudioBuffer propio, procesado por bloques. */
void copyIntoBlock (const std::vector<std::vector<float>>& src, int start, dsp::AudioBuffer<float>& block)
{
    for (int ch = 0; ch < block.getNumChannels(); ++ch)
        std::memcpy (block.getWritePointer (ch),
                     src[(size_t) ch].data() + (size_t) start,
                     sizeof (float) * (size_t) block.getNumSamples());
}

void copyBackFromBlock (const dsp::AudioBuffer<float>& block, std::vector<std::vector<float>>& dst, int start)
{
    for (int ch = 0; ch < block.getNumChannels(); ++ch)
        std::memcpy (dst[(size_t) ch].data() + (size_t) start,
                     block.getReadPointer (ch),
                     sizeof (float) * (size_t) block.getNumSamples());
}

/** Guion del chorus: barrido de rate/depth/mix por bloque (ejercita los
    smoothers de las DOS implementaciones con la misma secuencia). */
template <typename Effect>
std::vector<std::vector<float>> renderChorus (const std::vector<std::vector<float>>& in)
{
    Effect fx;
    fx.prepare (kSampleRate);

    std::vector<std::vector<float>> out = in;
    dsp::AudioBuffer<float> block ((int) out.size(), kBlock);

    for (int b = 0; b < kBlocks; ++b)
    {
        const float t = (float) b / (float) (kBlocks - 1);
        fx.setParameters (0.7f + t * 5.0f, 0.05f + t * 0.9f, t);

        copyIntoBlock (out, b * kBlock, block);
        fx.processBlock (block);
        copyBackFromBlock (block, out, b * kBlock);
    }

    return out;
}

/** Guion del delay: tiempos cortos (para que haya repeticiones reales dentro del
    render) y feedback que pasa por encima del recorte de 0.95. */
template <typename Effect>
std::vector<std::vector<float>> renderDelay (const std::vector<std::vector<float>>& in)
{
    Effect fx;
    fx.prepare (kSampleRate, static_cast<int> (kSampleRate * 2.0));

    std::vector<std::vector<float>> out = in;
    dsp::AudioBuffer<float> block ((int) out.size(), kBlock);

    for (int b = 0; b < kBlocks; ++b)
    {
        const float t = (float) b / (float) (kBlocks - 1);
        fx.setParameters (0.02f + t * 0.1f, 0.2f + t * 0.79f, 0.5f);

        copyIntoBlock (out, b * kBlock, block);
        fx.processBlock (block);
        copyBackFromBlock (block, out, b * kBlock);
    }

    return out;
}

/** Guion de la saturacion por bloque: el primer bloque va con drive exactamente
    1.0 (la puerta de bypass del producto), el resto barre hasta drive 5.0. */
template <typename Effect>
std::vector<std::vector<float>> renderSaturationBlock (const std::vector<std::vector<float>>& in)
{
    Effect fx;
    fx.prepare (kSampleRate);

    std::vector<std::vector<float>> out = in;
    dsp::AudioBuffer<float> block ((int) out.size(), kBlock);

    for (int b = 0; b < kBlocks; ++b)
    {
        const float t = (float) b / (float) (kBlocks - 1);
        fx.setDrive (t);

        copyIntoBlock (out, b * kBlock, block);
        fx.processBlock (block);
        copyBackFromBlock (block, out, b * kBlock);
    }

    return out;
}

/** Guion de la saturacion muestra a muestra (el camino de processSample). */
template <typename Effect>
std::vector<float> renderSaturationSample (const std::vector<float>& in)
{
    Effect fx;
    fx.prepare (kSampleRate);

    std::vector<float> out (in.size());

    for (size_t i = 0; i < in.size(); ++i)
    {
        if (i % (size_t) kBlock == 0)
            fx.setDrive ((float) i / (float) in.size());

        out[i] = fx.processSample (in[i]);
    }

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

std::vector<float> flatten (const std::vector<std::vector<float>>& v)
{
    std::vector<float> out;
    for (const auto& channel : v)
        out.insert (out.end(), channel.begin(), channel.end());

    return out;
}

void compareAgainstFrozen (const char* label, const std::vector<float>& moved, const std::vector<float>& frozen)
{
    check (moved.size() == frozen.size(), "longitud de salida distinta");

    uint32_t maxUlp = 0;
    size_t mismatch = 0, worstIdx = 0;

    for (size_t i = 0; i < moved.size() && i < frozen.size(); ++i)
    {
        const uint32_t ulp = ulpDistance (moved[i], frozen[i]);

        if (ulp != 0) ++mismatch;
        if (ulp > maxUlp) { maxUlp = ulp; worstIdx = i; }
    }

    std::printf ("[info] %-26s maxUlp=%-3u mismatched=%zu/%zu  (peor #%zu)\n",
                 label, maxUlp, mismatch, moved.size(), worstIdx);

    check (mismatch == 0, "la salida migrada no coincide bit a bit con la referencia congelada");
}

//==============================================================================
void checkDeterminism()
{
    const auto in = makeInput (2);

    const std::vector<float> first  = flatten (renderDelay<NEURONiK::DSP::Effects::Delay> (in));
    const std::vector<float> second = flatten (renderDelay<NEURONiK::DSP::Effects::Delay> (in)); // objeto nuevo

    bool identical = (first.size() == second.size());

    for (size_t i = 0; i < first.size() && identical; ++i)
        identical = (bitsOf (first[i]) == bitsOf (second[i]));

    check (identical, "dos renders del delay no son identicos bit a bit");
}

/** Saturacion + chorus + delay realimentado (fb 0.95) y una cola larga de
    silencio: ni NaN ni infinitos. */
void checkTailIsFinite()
{
    NEURONiK::DSP::Effects::Saturation saturation;
    NEURONiK::DSP::Effects::Chorus     chorus;
    NEURONiK::DSP::Effects::Delay      delay;

    saturation.prepare (kSampleRate);
    chorus.prepare (kSampleRate);
    delay.prepare (kSampleRate, static_cast<int> (kSampleRate * 2.0));

    saturation.setDrive (0.6f);
    chorus.setParameters (3.0f, 0.8f, 1.0f);
    delay.setParameters (0.02f, 0.95f, 0.5f);

    dsp::AudioBuffer<float> block (2, kBlock);
    block.clear();
    block.setSample (0, 0, 1.0f);   // impulso

    bool finite = true;

    for (int b = 0; b < 200; ++b)
    {
        saturation.processBlock (block);
        chorus.processBlock (block);
        delay.processBlock (block);

        for (int ch = 0; ch < block.getNumChannels(); ++ch)
            for (int s = 0; s < block.getNumSamples(); ++s)
                if (! std::isfinite (block.getSample (ch, s)))
                    finite = false;

        block.clear();   // cola larga de silencio
    }

    check (finite, "la cola (saturacion + chorus + delay realimentado) produjo NaN o infinito");
}

} // namespace

//==============================================================================
int main()
{
    std::printf ("[info] DspEffectsParityTest: envoltorios de NEURONiK sobre DspEffects\n");
    std::printf ("[info] contra la referencia congelada de Source/DSP/Effects (0 ulps esperado)\n");

    const auto stereo = makeInput (2);
    const auto mono   = makeInput (1);
    const auto triple = makeInput (3);

    compareAgainstFrozen ("chorus estereo",
                          flatten (renderChorus<NEURONiK::DSP::Effects::Chorus> (stereo)),
                          flatten (renderChorus<ReferenceChorus> (stereo)));

    compareAgainstFrozen ("chorus mono",
                          flatten (renderChorus<NEURONiK::DSP::Effects::Chorus> (mono)),
                          flatten (renderChorus<ReferenceChorus> (mono)));

    compareAgainstFrozen ("chorus 3 canales",
                          flatten (renderChorus<NEURONiK::DSP::Effects::Chorus> (triple)),
                          flatten (renderChorus<ReferenceChorus> (triple)));

    compareAgainstFrozen ("delay estereo",
                          flatten (renderDelay<NEURONiK::DSP::Effects::Delay> (stereo)),
                          flatten (renderDelay<ReferenceDelay> (stereo)));

    compareAgainstFrozen ("delay mono",
                          flatten (renderDelay<NEURONiK::DSP::Effects::Delay> (mono)),
                          flatten (renderDelay<ReferenceDelay> (mono)));

    compareAgainstFrozen ("saturacion (bloques)",
                          flatten (renderSaturationBlock<NEURONiK::DSP::Effects::Saturation> (stereo)),
                          flatten (renderSaturationBlock<ReferenceSaturation> (stereo)));

    compareAgainstFrozen ("saturacion (por muestra)",
                          renderSaturationSample<NEURONiK::DSP::Effects::Saturation> (stereo[0]),
                          renderSaturationSample<ReferenceSaturation> (stereo[0]));

    checkDeterminism();
    checkTailIsFinite();

    if (gFailures == 0)
    {
        std::printf ("[OK] efectos migrados: 0 ulps contra la referencia congelada\n");
        return 0;
    }

    std::printf ("[FALLO] %d comprobacion(es)\n", gFailures);
    return 1;
}
