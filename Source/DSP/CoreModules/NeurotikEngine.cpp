/*
  ==============================================================================

    NeurotikEngine.cpp
    Created: 30 Jan 2026
    Description: Implementation of the Neurotik engine.

  ==============================================================================
*/

#include "DspCore.h"
#include "NeurotikEngine.h"
#include "../Synthesis/NeurotikVoice.h"
#include "../DspSafety.h"

namespace NEURONiK::DSP {

NeurotikEngine::NeurotikEngine()
{
    // Reserva PEREZOSA: nace con OCHO voces (su limite por defecto), no 32.
    activeVoiceLimit.store(8);
    ensureVoices (activeVoiceLimit.load());
}

std::unique_ptr<IVoice> NeurotikEngine::createVoice(int index)
{
    // El indice es la semilla del ruido de excitacion: sin el, el unison sonaria
    // con ruido identico en todas las voces (artefacto audible).
    return std::make_unique<Synthesis::NeurotikVoice>(index);
}

void NeurotikEngine::prepare(double sampleRate, int samplesPerBlock)
{
    BaseEngine::prepare(sampleRate, samplesPerBlock);
}

void NeurotikEngine::renderNextBlock(dsp::AudioBuffer<float>& buffer, dsp::MidiBuffer& midiMessages)
{
    // 1. Parametros del bloque (config de LFO, FX y params de voz)
    updateParameters();

    // 2. Process MIDI events
    processMidiBuffer(midiMessages);

    buffer.clear();

    // 3. Voces + modulacion, en tramos de tasa de control fija: el LFO se lee y la
    //    matriz se aplica cada kControlBlockSize muestras, no cada bloque del host.
    renderVoicesWithControlRate(buffer);

    // 4. Global FX
    applyGlobalFX(buffer);
}

void NeurotikEngine::updateParameters()
{
    // Propagate parameters to all voices
    for (auto& v : voices)
    {
        if (v->getType() == VoiceType::Neurotik)
            static_cast<Synthesis::NeurotikVoice*>(v.get())->setParams(pendingVoiceParams);
    }

    BaseEngine::updateParameters();
    // La modulacion NO se aplica aqui: la aplica renderVoicesWithControlRate() en la
    // rejilla de control, con el valor de LFO de cada tramo (antes se aplicaba una vez
    // por bloque del host y con el valor del bloque anterior).
}

void NeurotikEngine::applyModulation()
{
    // Las fuentes ENV (6/7) se resuelven per-voz dentro de los casos
    // per-note: aqui valen 0 y nunca entran por el camino LFO.
    float sources[8] = { 
        0.0f,                   // Off
        lfo1Value.load(),       // LFO 1
        lfo2Value.load(),       // LFO 2
        0.0f,                   // PB
        0.0f,                   // MW
        0.0f,                   // AT
        0.0f,                   // ENV 1: per-voz (VCA)
        0.0f                    // ENV 2: per-voz (ADSR del filtro)
    };

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
        
        // Dest logic. Casos PER-NOTE (1, 10): LFOs suman como siempre; rutas
        // ENV entregan la suma de amounts a la voz (reemplazo, ver arriba).
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
            // Destinos 12-16 ("Filter Env Amt" 12 = suma al factor de routing;
            // 13-16 = ADSR del filtro): solo responden a ENV 2. El motor
            // resonador no tiene filtro, pero IVoice es el contrato comun y los
            // campos existen (el destino queda sin efecto audible aqui).
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
            // El drive del hueco 1, que desde 2026-09-29 tiene bus propio. El
            // indice vaparecido con el destino 17 de la tabla de NEURONiK
            // (ModRule::Kind::globalFxAdd, hueco 0, mando 0).
            case 17: currentGlobalParams.fx[0].params[0] += rawMod; break;
            // El 18 y el 19 son los dos primeros mandos del hueco 3 (el retardo),
            // igual que el destino 17 es el primero del hueco 1: los mandos
            // planos que se modulaban antes ya no los mira nadie.
            case 18: currentGlobalParams.fx[2].params[0] += rawMod; break;
            case 19: currentGlobalParams.fx[2].params[1] += rawMod; break;
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

void NeurotikEngine::getEnvelopeLevels(float& amp, float& filter) const
{
    for (const auto& v : voices)
    {
        if (v->isActive() && v->getType() == VoiceType::Neurotik)
        {
            auto* nv = static_cast<Synthesis::NeurotikVoice*>(v.get());
            amp = nv->getAmpEnvelopeLevel();
            filter = 0.0f;
            return;
        }
    }
    amp = 0.0f;
    filter = 0.0f;
}

void NeurotikEngine::getModulationValues(float* destination, int count) const
{
    if (destination == nullptr || count <= 0) return;
    
    int numToCopy = dsp::jmin(count, (int)lastModulations.size());
    for (int i = 0; i < numToCopy; ++i)
        destination[i] = lastModulations[i];
        
    // Fill remaining with 0
    for (int i = numToCopy; i < count; ++i)
        destination[i] = 0.0f;
}

void NeurotikEngine::getSpectralData(float* destination64) const
{
    bool found = false;
    for (const auto& v : voices)
    {
        if (v->isActive() && v->getType() == VoiceType::Neurotik)
        {
            auto* nv = static_cast<Synthesis::NeurotikVoice*>(v.get());
            const auto& partials = nv->getPartialAmplitudes();
            for (int i = 0; i < 64; ++i) destination64[i] = partials[i];
            found = true;
            break;
        }
    }
    
    if (!found)
    {
        std::fill(destination64, destination64 + 64, 0.0f);
    }
}

void NeurotikEngine::loadModel(const NEURONiK::Common::SpectralModel& model, int slot)
{
    for (auto& v : voices)
    {
        if (v->getType() == VoiceType::Neurotik)
            static_cast<Synthesis::NeurotikVoice*>(v.get())->loadModel(model, slot);
    }
}

void NeurotikEngine::setVoiceParams(const NEURONiK::DSP::Synthesis::NeurotikVoice::Params& p)
{
    pendingVoiceParams = p;
}

// MORPH del pad XY (mismo contrato que NeuronikEngine::setMorph).
void NeurotikEngine::setMorph (float morphX, float morphY)
{
    pendingVoiceParams.morphX = morphX;
    pendingVoiceParams.morphY = morphY;
}

// Eje temporal del morph (FASE 10).
void NeurotikEngine::setMorphZ (float morphZ)
{
    pendingVoiceParams.morphZ = morphZ;
}

// FASE 11.4: el VOLUMEN de las capas 1 y 2 (la capa 0 es el fondo, siempre
// al maximo). Mismo canal RT-safe que setMorphZ.
void NeurotikEngine::setVoiceLayerMorph (float layerGain2, float layerGain3)
{
    pendingVoiceParams.layerGain2 = layerGain2;
    pendingVoiceParams.layerGain3 = layerGain3;
}

// ADSR del canal del worklet (los knobs de envolvente de la pagina, que no
// tienen APVTS). Solo la ENV de AMP: la voz neurotik no tiene envolvente de
// filtro, asi que los cuatro tramos 'f*' no tienen destino aqui. Mismo
// read-modify-write RT-safe que los tres de arriba: el morph no se pisa.
void NeurotikEngine::setVoiceEnvelope (float attack, float decay, float sustain, float release,
                                       float fAttack, float fDecay, float fSustain, float fRelease)
{
    (void) fAttack; (void) fDecay; (void) fSustain; (void) fRelease;

    pendingVoiceParams.attack  = attack;
    pendingVoiceParams.decay   = decay;
    pendingVoiceParams.sustain = sustain;
    pendingVoiceParams.release = release;
}

void NeurotikEngine::handleMidiEvent(const dsp::MidiMessage& m)
{
    int channel = m.getChannel();

    if (m.isNoteOn())
    {
        int limit = activeVoiceLimit.load();
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
            {
                v->noteOff(m.getFloatVelocity(), true);
            }
        }
    }
    else if (m.isPitchWheel())
    {
        float bendSemitones = ((float)m.getPitchWheelValue() - 8192.0f) / 8192.0f * 48.0f;
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
