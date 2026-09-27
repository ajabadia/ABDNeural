/*
  ==============================================================================

    BaseEngine.cpp
    Created: 30 Jan 2026

  ==============================================================================
*/

#include "DspCore.h"
#include "BaseEngine.h"

#include "CoreModules/RhythmicDivision.h"

namespace NEURONiK::DSP {

BaseEngine::BaseEngine()
{
    // Capacidad FIJA (32 = kMaxVoices): la reserva perezosa hace push_back, y el
    // hilo de audio ya indexa `voices`. Con la capacidad reservada de una vez, crecer
    // NUNCA reasigna el buffer: como mucho el audio ve una voz de mas, nunca un
    // puntero que se movio debajo.
    voices.reserve ((size_t) kMaxVoices);
}

void BaseEngine::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentSamplesPerBlock = samplesPerBlock;
    controlCarry = 0;

    saturation.prepare(sampleRate);
    delay.prepare(sampleRate, static_cast<int>(sampleRate * 2.0)); // 2s max
    chorus.prepare(sampleRate);
    reverb.prepare(sampleRate);
    
    lfo1.setSampleRate(sampleRate);
    lfo2.setSampleRate(sampleRate);
    
    masterLevelSmoother.reset(sampleRate, 0.05);
    
    for (auto& voice : voices)
    {
        if (voice) voice->prepare(sampleRate, samplesPerBlock);
    }

    // A partir de aqui las voces nuevas se preparan al crearse (ensureVoices).
    voicesPrepared = true;
    ensureVoices (activeVoiceLimit.load());
}

void BaseEngine::updateParameters()
{
    currentGlobalParams = pendingGlobalParams;
    
    saturation.setDrive(currentGlobalParams.saturationAmt);
    delay.setParameters(currentGlobalParams.delayTime, currentGlobalParams.delayFB);
    chorus.setParameters(currentGlobalParams.chorusRate,
                         currentGlobalParams.chorusDepth,
                         currentGlobalParams.chorusMix);
    reverb.setParameters(currentGlobalParams.reverbSize,
                         currentGlobalParams.reverbDamping,
                         currentGlobalParams.reverbWidth,
                         currentGlobalParams.reverbMix);
    
    masterLevelSmoother.setTargetValue(currentGlobalParams.masterLevel);

    // Tempo sync: the fields existed in GlobalParams but were never applied, so the
    // LFOs always ran free and the SYNC controls did nothing.
    const double bpm = currentGlobalParams.bpm;

    auto applyLfo = [bpm] (Core::LFO& lfo, const GlobalParams::LFOParams& p)
    {
        lfo.setWaveform (static_cast<Core::LFO::Waveform> (dsp::jlimit (0, 5, p.waveform)));
        lfo.setRate (p.rateHz);
        lfo.setDepth (p.depth);
        lfo.setSyncMode (p.syncMode == 0 ? Core::LFO::SyncMode::Free
                                         : Core::LFO::SyncMode::TempoSync);
        lfo.setTempoBPM (bpm);
        lfo.setRhythmicDivision (Core::quarterNotesForDivision (p.rhythmicDivision));
    };

    applyLfo (lfo1, currentGlobalParams.lfo1);
    applyLfo (lfo2, currentGlobalParams.lfo2);
    
    for (auto& voice : voices)
    {
        if (voice) voice->updateParameters();
    }
}

void BaseEngine::reset()
{
    saturation.resetState();
    delay.reset();
    chorus.reset();
    reverb.reset();

    lfo1.reset();
    lfo2.reset();
    controlCarry = 0;   // el reset tambien reinicia la rejilla de control
    
    for (auto& voice : voices)
    {
        if (voice) voice->reset();
    }
}

void BaseEngine::handleMidiMessage(const dsp::MidiMessage& msg)
{
    handleMidiEvent(msg);
}

float BaseEngine::getLfoValue(int index) const
{
    return (index == 0) ? lfo1Value.load() : lfo2Value.load();
}

void BaseEngine::getModulationValues(float* destination, int count) const
{
    if (destination == nullptr || count <= 0) return;
    
    // Default modulation values if not overridden
    for (int i = 0; i < count; ++i) destination[i] = 0.0f;
}

int BaseEngine::getNumActiveVoices() const
{
    int active = 0;
    for (auto& voice : voices)
        if (voice && voice->isActive()) active++;
    return active;
}

void BaseEngine::setPolyphony(int numVoices)
{
    const int limit = dsp::jlimit(1, kMaxVoices, numVoices);
    const int oldSize = (int) voices.size();

    if (limit > oldSize)
    {
        // Crecer ANTES de publicar el limite: el hilo de audio indexa
        // `voices[i]` para i < activeVoiceLimit, asi que la invariante
        // `size() >= limit` tiene que estar puesta cuando el limite nuevo
        // se vea. La reserva la hace el hilo de mensajes (o el que prepara
        // el motor), nunca el de audio.
        ensureVoices (limit);
        activeVoiceLimit.store(limit);
        return;
    }

    if (limit < oldSize)
    {
        // BAJAR: devuelve solo las OCiosas. Las que aun suenan (isActive)
        // no se tocan; las demas se liberan. La CAPACIDAD en caliente
        // (reserve 32) se mantiene, solo baja size(). Ver reclaimIdleVoices.
        // Publicar el limite primero no rompe la invariante size()>=limit
        // en este camino (limit < oldSize), y el audio ve el nuevo limite
        // antes que la compactacion — bajo el cerrojo del procesador.
        activeVoiceLimit.store(limit);
        reclaimIdleVoices (limit);
        return;
    }

    // limit == oldSize: solo mueve el limite (subir/bajar dentro de lo
    // ya reservado sin re-asignar, o mismo valor: no hay memoria que
    // devolver porque ya cabe, solo baja el limite.
    activeVoiceLimit.store(limit);
}

void BaseEngine::ensureVoices(int count)
{
    const int target = dsp::jlimit(1, kMaxVoices, count);

    while ((int) voices.size() < target)
    {
        const int index = (int) voices.size();

        voices.push_back (createVoice (index));

        if (voicesPrepared && voices.back() != nullptr)
            voices.back()->prepare (currentSampleRate, currentSamplesPerBlock);
    }
}

void BaseEngine::reclaimIdleVoices (int newLimit) noexcept
{
    const int currentSize = (int) voices.size();
    if (newLimit >= currentSize)
        return;

    int numActive = 0;
    for (auto& v : voices)
        if (v && v->isActive())
            ++numActive;

    if (numActive >= currentSize)
        return; // todas suenan: no hay nada ocioso que devolver

    // Compacta las que suenan al frente: [0, numActive) activas,
    // [numActive, size) ociosas. Asi el trim de la cola solo toca
    // ociosas y nunca corta una cola.
    int write = 0;
    for (int read = 0; read < currentSize; ++read)
    {
        if (voices[(size_t) read] && voices[(size_t) read]->isActive())
        {
            if (read != write)
                std::swap (voices[(size_t) read], voices[(size_t) write]);
            ++write;
        }
    }

    const int targetSize = std::max (newLimit, numActive);
    while ((int) voices.size() > targetSize)
    {
        auto& tail = voices.back();
        if (tail && tail->isActive())
            break; // por si la particion no dejo cola ociosa limpia
        voices.pop_back();
    }
    // Capacidad (reserve 32) intacta: volver a subir solo pushea.
}

void BaseEngine::allNotesOff()
{
    // Release rather than reset: keeps the envelope tail, so no click.
    for (auto& voice : voices)
        if (voice != nullptr && voice->isActive())
            voice->noteOff (0.0f, true);
}

void BaseEngine::processMidiBuffer(dsp::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        handleMidiEvent(metadata.getMessage());
    }
}

void BaseEngine::renderVoicesWithControlRate(dsp::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    int offset = 0;

    while (offset < numSamples)
    {
        // Se completa el tramo de control en curso antes de abrir otro: la rejilla es
        // de kControlBlockSize muestras DE AUDIO, no de "bloques del host", asi que un
        // host de 96 muestras (o de 1024) no la desplaza.
        const int chunk = dsp::jmin(kControlBlockSize - controlCarry, numSamples - offset);

        // 1. LFOs: el valor se lee una vez por tramo y se mantiene en todo el tramo.
        lfo1Value.store(lfo1.processBlock(chunk));
        lfo2Value.store(lfo2.processBlock(chunk));

        // 2. Matriz de modulacion con el valor recien leido.
        applyModulation();

        // 3. Voces: configuran su resonator con ese snapshot y suman en el buffer.
        for (auto& voice : voices)
        {
            if (voice != nullptr && voice->isActive())
                voice->renderNextBlock(buffer, offset, chunk);
        }

        offset += chunk;
        controlCarry = (controlCarry + chunk) % kControlBlockSize;
    }
}

void BaseEngine::applyGlobalFX(dsp::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();

    // Los LFOs NO se leen aqui: los consume renderVoicesWithControlRate() en la rejilla
    // de control, junto con la modulacion (antes se leian una vez por bloque del host y
    // la matriz de modulacion heredaba el tamano de bloque).

    // Global Effects
    saturation.processBlock(buffer);
    chorus.processBlock(buffer);
    delay.processBlock(buffer);
    reverb.processBlock(buffer);

    // 3. Output Level
    masterLevelSmoother.applyGain(buffer, numSamples);
}

} // namespace NEURONiK::DSP
