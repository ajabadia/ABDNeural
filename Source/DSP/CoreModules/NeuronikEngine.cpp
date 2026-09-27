/*
  ==============================================================================

    NeuronikEngine.cpp
    Created: 30 Jan 2026
    Description: Implementation of the central synthesis engine.

  ==============================================================================
*/

#include "DspCore.h"
#include "NeuronikEngine.h"
#include "../Synthesis/AdditiveVoice.h"
#include "../DspSafety.h"

namespace NEURONiK::DSP {

NeuronikEngine::NeuronikEngine()
{
    // Reserva PEREZOSA: nace con las voces de su limite (16 por defecto), no 32.
    // `ensureVoices` crece cuando `setPolyphony` sube el techo.
    ensureVoices (activeVoiceLimit.load());
}

std::unique_ptr<IVoice> NeuronikEngine::createVoice(int)
{
    return std::make_unique<Synthesis::AdditiveVoice>();
}

void NeuronikEngine::prepare(double sampleRate, int samplesPerBlock)
{
    BaseEngine::prepare(sampleRate, samplesPerBlock);
}

void NeuronikEngine::renderNextBlock(dsp::AudioBuffer<float>& buffer, dsp::MidiBuffer& midiMessages)
{
    // 1. Parametros del bloque (config de LFO, FX y params de voz)
    updateParameters();

    // 2. Process MIDI events
    processMidiBuffer(midiMessages);

    // 3. Voces + modulacion, en tramos de tasa de control fija: el LFO se lee y la
    //    matriz se aplica cada kControlBlockSize muestras, no cada bloque del host.
    renderVoicesWithControlRate(buffer);

    // 4. Global FX
    applyGlobalFX(buffer);
}

void NeuronikEngine::applyModulation()
{
    // Snapshot LFO values from base. Las fuentes ENV (6/7) se resuelven
    // per-voz dentro de los casos per-note: aqui valen 0 y nunca entran
    // por el camino LFO (una envolvente no es global, es de cada nota).
    float sources[8] = { 
        0.0f,                   // Off
        lfo1Value.load(),       // LFO 1
        lfo2Value.load(),       // LFO 2
        0.0f, // TODO: PB
        0.0f, // TODO: MW
        0.0f, // TODO: AT
        0.0f, // ENV 1: per-voz (VCA)
        0.0f  // ENV 2: per-voz (ADSR del filtro)
    };

    // Reset voice mod values
    for (auto& v : voices) v->resetModulations();
    
    // Reset visualization values
    lastModulations.fill(0.0f);

    for (int i = 0; i < 4; ++i)
    {
        const auto& route = currentGlobalParams.modMatrix[i];
        if (route.source == 0 || route.destination == 0) continue;
        
        float rawMod = sources[dsp::jlimit(0, 5, route.source)] * route.amount;
        
        // Update visualization
        if (route.destination >= 0 && route.destination < 64)
            lastModulations[route.destination] += rawMod;
        
        // Dest logic. Los casos PER-NOTE (1, 10, 12, 13) preguntan por la
        // fuente: los LFOs suman como siempre; si la ruta es ENV, la voz
        // recibe la suma de amounts (sintesis de reemplazo, ver arriba).
        switch (route.destination)
        {
            case 1:
                if (route.source == 6)
                    for (auto& v : voices) v->modEnvLevel = route.amount; // factor (sobrescribe)
                else
                    for (auto& v : voices) v->modLevel += rawMod;
                break;
            case 2: for (auto& v : voices) v->modInharmonicity += rawMod; break;
            case 3: for (auto& v : voices) v->modRoughness += rawMod; break;
            case 4: for (auto& v : voices) v->modMorphX += rawMod; break;
            case 5: for (auto& v : voices) v->modMorphY += rawMod; break;
            case 6: for (auto& v : voices) v->modAmpAttack += rawMod; break;
            case 7: for (auto& v : voices) v->modAmpDecay += rawMod; break;
            case 8: for (auto& v : voices) v->modAmpSustain += rawMod; break;
            case 9: for (auto& v : voices) v->modAmpRelease += rawMod; break;
            case 10:
                if (route.source == 7)
                    for (auto& v : voices) v->modEnvCutoff = route.amount; // factor (sobrescribe)
                else
                    for (auto& v : voices) v->modCutoff += rawMod * 18000.0f; 
                break;
            case 11: for (auto& v : voices) v->modFilterRes += rawMod; break; 
            // Destinos de la ADSR del filtro (13-16) y de su profundidad (12):
            // solo responden a ENV 2 (un LFO no retrigunea envolventes).
            // 12 = "Filter Env Amt": SUMA al factor de routing de la ruta
            // ENV 2 -> Filter Cutoff (base 1.0) — es la profundidad del knob
            // filterEnvAmount retirado (2026-09-26), viviendo en la matriz.
            // 13-16 REEMPLAZAN el attack/decay programado: 1+valor repichea
            // cada vez que el modulador recicla.
            case 12:
                if (route.source == 7)
                    for (auto& v : voices) v->modEnvFltDepth += route.amount;
                break;
            case 13:
                if (route.source == 7)
                    for (auto& v : voices) v->modEnvFltAttack += route.amount;
                break;
            case 14:
                if (route.source == 7)
                    for (auto& v : voices) v->modEnvFltDecay += route.amount;
                break;
            case 15:
                if (route.source == 7)
                    for (auto& v : voices) v->modEnvFltSustain += route.amount;
                break;
            case 16:
                if (route.source == 7)
                    for (auto& v : voices) v->modEnvFltRelease += route.amount;
                break;
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
            case 28: for (auto& v : voices) v->modMorphZ += rawMod; break;
            // FASE 11.3: los z de las capas 1 y 2 (destinos 29/30, al final de la
            // tabla: los choice de la matriz se guardan por INDICE).
            case 29: for (auto& v : voices) v->modMorphZ2 += rawMod; break;
            case 30: for (auto& v : voices) v->modMorphZ3 += rawMod; break;
            default: break;
        }
    }
}

void NeuronikEngine::updateParameters()
{
    // Propagate parameters to all voices
    for (auto& v : voices)
    {
        if (v->getType() == VoiceType::Additive)
            static_cast<Synthesis::AdditiveVoice*>(v.get())->setParams(pendingVoiceParams);
    }

    BaseEngine::updateParameters();
    // La modulacion NO se aplica aqui: la aplica renderVoicesWithControlRate() en la
    // rejilla de control, con el valor de LFO de cada tramo (antes se aplicaba una vez
    // por bloque del host y con el valor del bloque anterior).
}


void NeuronikEngine::getSpectralData(float* destination64) const
{
    bool found = false;
    for (const auto& v : voices)
    {
        if (v->isActive() && v->getType() == VoiceType::Additive)
        {
            auto* av = static_cast<Synthesis::AdditiveVoice*>(v.get());
            auto& partials = av->getResonator().getPartialAmplitudes();
            for (int i = 0; i < 64; ++i) destination64[i] = partials[i];
            found = true;
            break;
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
        if (v->isActive() && v->getType() == VoiceType::Additive)
        {
            auto* av = static_cast<Synthesis::AdditiveVoice*>(v.get());
            amp = av->getAmpEnvelopeLevel();
            filter = av->getFilterEnvelopeLevel();
            return;
        }
    }
    amp = 0.0f;
    filter = 0.0f;
}

void NeuronikEngine::getModulationValues(float* destination, int count) const
{
    if (destination == nullptr || count <= 0) return;
    
    int numToCopy = dsp::jmin(count, (int)lastModulations.size());
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
        if (v->getType() == VoiceType::Additive)
            static_cast<Synthesis::AdditiveVoice*>(v.get())->loadModel(model, slot);
    }
}

void NeuronikEngine::setVoiceParams(const NEURONiK::DSP::Synthesis::AdditiveVoice::Params& p)
{
    pendingVoiceParams = p;
}

// MORPH del pad XY: read-modify-write de pendingVoiceParams. No toca morphZ
// (el eje temporal es parametro propio, morphZ/morphZ2/morphZ3) ni el resto
// de la ficha: solo los dos ejes del pad. Viaja por el mismo canal que
// setVoiceParams, asi que el handoff a las voces es el de siempre.
void NeuronikEngine::setMorph (float morphX, float morphY)
{
    pendingVoiceParams.morphX = morphX;
    pendingVoiceParams.morphY = morphY;
}

// Eje temporal del morph (FASE 10): frame canonico de la capa 0.
void NeuronikEngine::setMorphZ (float morphZ)
{
    pendingVoiceParams.morphZ = morphZ;
}

// FASE 11.4: el VOLUMEN de las capas 1 y 2 (la capa 0 es el fondo, siempre
// al maximo). Mismo canal RT-safe que setMorphZ; el clampeo lo hace el
// resonador al consumirlo.
void NeuronikEngine::setVoiceLayerMorph (float layerGain2, float layerGain3)
{
    pendingVoiceParams.layerGain2 = layerGain2;
    pendingVoiceParams.layerGain3 = layerGain3;
}

void NeuronikEngine::handleMidiEvent(const dsp::MidiMessage& m)
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
            if (v->isActive() && (v->getChannel() == channel))
                v->notePitchBend(bendSemitones);
        }
    }
    else if (m.isAftertouch() || m.isChannelPressure())
    {
        float pressureVal = m.isAftertouch() ? (float)m.getAfterTouchValue() : (float)m.getChannelPressureValue();
        float pressure = pressureVal / 127.0f;
        
        for (auto& v : voices)
        {
            if (v->isActive() && (v->getChannel() == channel))
                v->notePressure(pressure);
        }
    }
    else if (m.isController() && m.getControllerNumber() == 74)
    {
        float timbre = (float)m.getControllerValue() / 127.0f;
        for (auto& v : voices)
        {
            if (v->isActive() && (v->getChannel() == channel))
                v->noteTimbre(timbre);
        }
    }
}


} // namespace NEURONiK::DSP
