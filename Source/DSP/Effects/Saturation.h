/*
  ==============================================================================

    Saturation.h
    Created: 22 Jan 2026
    Description: Envoltorio de producto de la saturacion por soft-clipping.

    Separacion de responsabilidades (migracion de efectos, igual que la reverb
    en la Fase 1 [5/6]): la forma pura del efecto vive en el modulo compartido
    ABDSharedCode::DspEffects (DspEffects/DspSaturation.h, namespace abd::dsp),
    que es una utilidad estatica sin estado. Aqui queda lo especifico de este
    producto: el mapeo amount -> drive (1.0 + amount * 4.0), su suavizado de 20ms
    y la puerta de "drive ~ 1.0 = bypass" (la forma no es la identidad en drive
    = 1, asi que saltarse el calculo es una decision de producto, no un atajo
    aritmetico).

    La API publica no cambia: el motor (BaseEngine) sigue llamando a prepare(),
    setDrive(), processSample(), processBlock() y resetState() igual.

  ==============================================================================
*/

#pragma once

#include "DspCore.h"
#include "DspEffects/DspSaturation.h"

namespace NEURONiK::DSP::Effects {

/**
 * Saturation effect using a soft-clipping sigmoid function.
 *
 * Thread-safety: processSample is real-time safe.
 */
class Saturation
{
public:
    Saturation() noexcept = default;
    ~Saturation() = default;

    /**
     * Initializes the smoother with the sample rate.
     */
    void prepare(double sampleRate) noexcept
    {
        driveSmoother.reset(sampleRate, 0.02); // 20ms ramp
    }

    /**
     * Sets the amount of saturation.
     * @param amount Range [0.0, 1.0]
     */
    void setAmount(float amount) noexcept {
        driveSmoother.setTargetValue(1.0f + (amount * 4.0f)); // Scale drive for noticeable effect
    }

    void setDrive(float drive) noexcept {
        setAmount(drive);
    }

    /**
     * Processes a single sample.
     */
    inline float processSample(float input) noexcept
    {
        return dsp::Saturation::processSample(input, driveSmoother.getNextValue());
    }

    /**
     * Processes an entire buffer of samples.
     */
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
                    buffer.setSample(ch, s,
                        dsp::Saturation::processSample(buffer.getSample(ch, s), currentDrive));
                }
            }
        }
    }

    /**
     * Resets the effect state.
     */
    void resetState() noexcept
    {
        driveSmoother.setCurrentAndTargetValue(1.0f);
    }

private:
    dsp::LinearSmoothedValue<float> driveSmoother { 1.0f };

    dspDeclareNonCopyableWithLeakDetector(Saturation)
};

} // namespace NEURONiK::DSP::Effects
