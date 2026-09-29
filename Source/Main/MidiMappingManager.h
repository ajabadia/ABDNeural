/*
  ==============================================================================

    MidiMappingManager.h
    Created: 27 Jan 2026
    Description: Manages global MIDI CC to Parameter mappings with conflict detection.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>

namespace NEURONiK::Main {

class MidiMappingManager
{
public:
    MidiMappingManager(juce::AudioProcessorValueTreeState& apvts);
    ~MidiMappingManager() = default;

    /** Sets a mapping. If CC is already used, it unassigns it from previous parameter. */
    void setMapping(const juce::String& paramID, int ccNumber);

    /**
     * RT-safe variant for the AUDIO thread (MIDI learn completing on a real CC
     * message): the parameter travels as its INDEX in getLearnableParams(), so
     * no String is built or copied while rendering — only the atomic slots move.
     */
    void setMappingByIndex(int paramIndex, int ccNumber);

    /**
     * RT-safe inverse lookup for the AUDIO thread: which learnable parameter
     * (by INDEX, -1 if none) answers to this CC. One atomic load, no String.
     */
    int getParamIndexForCC(int ccNumber) const
    {
        if (ccNumber < 0 || ccNumber > 127) return -1;
        return ccToIndex[(size_t) ccNumber].load();
    }
    
    /** Removes any mapping for this parameter. */
    void clearMapping(const juce::String& paramID);

    /** Returns the CC assigned to a paramID, or -1 if none. */
    int getCCForParam(const juce::String& paramID) const;

    /** Returns the paramID for a given CC, or empty string if none. */
    juce::String getParamForCC(int ccNumber) const;

    /** Returns all mappings. (Note: This is now a more expensive view for the UI) */
    std::map<int, juce::String> getMappings() const;

    /** Reset to a safe set of defaults. */
    void resetToDefaults();

    /** Persistence: Load/Save from ValueTree. */
    void saveToValueTree(juce::ValueTree& v);
    void loadFromValueTree(const juce::ValueTree& v);

    /** Checks if a CC is in conflict. */
    bool hasConflict(int ccNumber) const;

    /** @brief Version de la TABLA (no del APVTS), RT-safe.
     *
     *  El learn completado por HARDWARE reescribe la tabla desde el hilo de
     *  audio y el puente solo publicaba `midiCcState` cuando la pagina lo pedia
     *  (o al mandar un snapshot): sin este contador, el menu MIDI CONTROL de la
     *  pagina se quedaba con la tabla vieja despues de un learn de hardware.
     *  Lo lee el poll del editor (hilo de mensajes) para saber si hay algo
     *  nuevo que publicar.
     */
    int getTableVersion() const noexcept { return tableVersion.load (std::memory_order_acquire); }

    /** RT-safe access to the list of learnable parameters. */
    static const juce::StringArray& getLearnableParams();
    static int getParamIndex(const juce::String& paramID);

private:
    juce::AudioProcessorValueTreeState& apvts;
    
    // Real-time safe storage: store the index of the parameter in the modulatable list
    // -1 means no mapping for that CC.
    std::array<std::atomic<int>, 128> ccToIndex;

    // Version de la tabla: la mueve TODO mutador (incluido el learn que
    // completa el bloque de audio). No se compara nada mas que igualdad, asi
    // que el numero de incrementos da igual.
    std::atomic<int> tableVersion { 0 };

    void updateInternalMaps();
};

} // namespace NEURONiK::Main
