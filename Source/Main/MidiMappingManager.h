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
    
    /** Removes any mapping for this parameter. */
    void clearMapping(const juce::String& paramID);

    /** Returns the CC assigned to a paramID, or -1 if none. */
    int getCCForParam(const juce::String& paramID) const;

    /** Returns the paramID for a given CC, or empty string if none. */
    juce::String getParamForCC(int ccNumber) const;

    /** Returns all mappings. */
    const std::map<int, juce::String>& getMappings() const { return ccToParam; }

    /** Reset to a safe set of defaults. */
    void resetToDefaults();

    /** Persistence: Load/Save from ValueTree. */
    void saveToValueTree(juce::ValueTree& v);
    void loadFromValueTree(const juce::ValueTree& v);

    /** Checks if a CC is in conflict (assigned to multiple, though setMapping prevents this). */
    bool hasConflict(int ccNumber) const;

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::map<int, juce::String> ccToParam;
    std::map<juce::String, int> paramToCC;

    void updateInternalMaps();
};

} // namespace NEURONiK::Main
