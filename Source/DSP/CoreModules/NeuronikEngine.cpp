/*
  ==============================================================================

    NeuronikEngine.cpp
    Created: 30 Jan 2026
    Description: Implementation of the central synthesis engine.

  ==============================================================================
*/

#include "NeuronikEngine.h"
#include "../Synthesis/AdditiveVoice.h"
#include "../DSPUtils.h"

namespace NEURONiK::DSP {

NeuronikEngine::NeuronikEngine()
{
    // Pre-allocate 32 voices
    for (int i = 0; i < 32; ++i)
        voices.push_back(std::make_unique<Synthesis::AdditiveVoice>());
}

void NeuronikEngine::prepare(double sampleRate, int samplesPerBlock)
{
    BaseEngine::prepare(sampleRate, samplesPerBlock);
}

void NeuronikEngine::renderNextBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const int numSamples = buffer.getNumSamples();
    
    // 1. Update LFOs and Global Parameters
    updateParameters();

    // 2. Process MIDI events
    processMidiBuffer(midiMessages);

    // 3. Render Voices (Summing into buffer)
    for (auto& v : voices)
    {
        if (v->isActive())
            v->renderNextBlock(buffer, 0, numSamples);
    }

    // 4. Global FX & LFO Sampling
    applyGlobalFX(buffer);
}

void NeuronikEngine::applyModulation()
{
    // Snapshot LFO values from base
    float sources[6] = { 
        0.0f,                   // Off
        lfo1Value.load(),       // LFO 1
        lfo2Value.load(),       // LFO 2
        0.0f, // TODO: PB
        0.0f, // TODO: MW
        0.0f  // TODO: AT
    };

    // Reset voice mod values
    for (auto& v : voices) v->resetModulations();
    
    // Reset visualization values
    lastModulations.fill(0.0f);

    for (int i = 0; i < 4; ++i)
    {
        const auto& route = currentGlobalParams.modMatrix[i];
        if (route.source == 0 || route.destination == 0) continue;
        
        float rawMod = sources[juce::jlimit(0, 5, route.source)] * route.amount;
        
        // Update visualization
        if (route.destination >= 0 && route.destination < 64)
            lastModulations[route.destination] += rawMod;
        
        // Dest logic
        switch (route.destination)
        {
            case 1: for (auto& v : voices) v->modLevel += rawMod; break;
            case 2: for (auto& v : voices) v->modInharmonicity += rawMod; break;
            case 3: for (auto& v : voices) v->modRoughness += rawMod; break;
            case 4: for (auto& v : voices) v->modMorphX += rawMod; break;
            case 5: for (auto& v : voices) v->modMorphY += rawMod; break;
            case 6: for (auto& v : voices) v->modAmpAttack += rawMod; break;
            case 7: for (auto& v : voices) v->modAmpDecay += rawMod; break;
            case 8: for (auto& v : voices) v->modAmpSustain += rawMod; break;
            case 9: for (auto& v : voices) v->modAmpRelease += rawMod; break;
            case 10: for (auto& v : voices) v->modCutoff += rawMod * 18000.0f; break; 
            case 11: for (auto& v : voices) v->modFilterRes += rawMod; break; 
            // 12-16 Filter Env params (pending IVoice members)
            case 17: currentGlobalParams.saturationAmt += rawMod; break;
            case 18: currentGlobalParams.delayTime += rawMod; break; 
            case 19: currentGlobalParams.delayFB += rawMod; break;
            case 20: for (auto& v : voices) v->modParity += rawMod; break;
            case 21: for (auto& v : voices) v->modShift += rawMod; break; 
            case 22: for (auto& v : voices) v->modRolloff += rawMod; break;
            case 23: for (auto& v : voices) v->modExciteNoise += rawMod; break;
            case 24: for (auto& v : voices) v->modExciteColor += rawMod; break;
            case 25: for (auto& v : voices) v->modImpulseMix += rawMod; break;
            case 26: for (auto& v : voices) v->modResonance += rawMod; break;
            case 27: for (auto& v : voices) v->modUnison += rawMod; break;
            default: break;
        }
    }
}

void NeuronikEngine::updateParameters()
{
    BaseEngine::updateParameters();
    applyModulation();

    // Propagate parameters to all voices
    for (auto& v : voices)
    {
        if (auto* av = dynamic_cast<::NEURONiK::DSP::Synthesis::AdditiveVoice*>(v.get()))
            av->setParams(pendingVoiceParams);
    }
}


void NeuronikEngine::getSpectralData(float* destination64) const
{
    bool found = false;
    for (const auto& v : voices)
    {
        if (v->isActive())
        {
            if (auto* av = dynamic_cast<Synthesis::AdditiveVoice*>(v.get()))
            {
                auto& partials = av->getResonator().getPartialAmplitudes();
                for (int i = 0; i < 64; ++i) destination64[i] = partials[i];
                found = true;
                break;
            }
        }
    }
    
    if (!found)
    {
        for (int i = 0; i < 64; ++i) destination64[i] = 0.0f;
    }
}

void NeuronikEngine::getEnvelopeLevels(float& amp, float& filter) const
{
    for (const auto& v : voices)
    {
        if (v->isActive())
        {
            if (auto* av = dynamic_cast<Synthesis::AdditiveVoice*>(v.get()))
            {
                amp = av->getAmpEnvelopeLevel();
                filter = av->getFilterEnvelopeLevel();
                return;
            }
        }
    }
    amp = 0.0f;
    filter = 0.0f;
}

void NeuronikEngine::getModulationValues(float* destination, int count) const
{
    if (destination == nullptr || count <= 0) return;
    
    int numToCopy = juce::jmin(count, (int)lastModulations.size());
    for (int i = 0; i < numToCopy; ++i)
        destination[i] = lastModulations[i];
        
    // Fill remaining with 0
    for (int i = numToCopy; i < count; ++i)
        destination[i] = 0.0f;
}

void NeuronikEngine::loadModel(const Common::SpectralModel& model, int slot)
{
    for (auto& v : voices)
    {
        if (auto* av = dynamic_cast<::NEURONiK::DSP::Synthesis::AdditiveVoice*>(v.get()))
            av->loadModel(model, slot);
    }
}

void NeuronikEngine::setVoiceParams(const NEURONiK::DSP::Synthesis::AdditiveVoice::Params& p)
{
    pendingVoiceParams = p;
}

void NeuronikEngine::handleMidiEvent(const juce::MidiMessage& m)
{
    int channel = m.getChannel();

    if (m.isNoteOn())
    {
        const int limit = activeVoiceLimit.load();
        for (int i = 0; i < limit; ++i)
        {
            if (!voices[i]->isActive())
            {
                voices[i]->setChannel(channel);
                voices[i]->noteOn(m.getNoteNumber(), m.getFloatVelocity());
                return;
            }
        }
        voices[0]->setChannel(channel);
        voices[0]->noteOn(m.getNoteNumber(), m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        for (auto& v : voices)
        {
            if (v->isActive() && v->getCurrentlyPlayingNote() == m.getNoteNumber() && v->getChannel() == channel)
                v->noteOff(m.getFloatVelocity(), true);
        }
    }
    else if (m.isPitchWheel())
    {
        float bendSemitones = ((float)m.getPitchWheelValue() - 8192.0f) / 8192.0f * 48.0f; // Scale to 48 semitones
        for (auto& v : voices)
        {
            if (v->isActive() && (v->getChannel() == channel || channel == 1))
                v->notePitchBend(bendSemitones);
        }
    }
    else if (m.isAftertouch() || m.isChannelPressure())
    {
        float pressureVal = m.isAftertouch() ? (float)m.getAfterTouchValue() : (float)m.getChannelPressureValue();
        float pressure = pressureVal / 127.0f;
        
        for (auto& v : voices)
        {
            if (v->isActive() && (v->getChannel() == channel || channel == 1))
                v->notePressure(pressure);
        }
    }
    else if (m.isController() && m.getControllerNumber() == 74)
    {
        float timbre = (float)m.getControllerValue() / 127.0f;
        for (auto& v : voices)
        {
            if (v->isActive() && (v->getChannel() == channel || channel == 1))
                v->noteTimbre(timbre);
        }
    }
}


} // namespace NEURONiK::DSP
