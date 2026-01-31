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

    /** Returns all mappings. (Note: This is now a more expensive view for the UI) */
    std::map<int, juce::String> getMappings() const;

    /** Reset to a safe set of defaults. */
    void resetToDefaults();

    /** Persistence: Load/Save from ValueTree. */
    void saveToValueTree(juce::ValueTree& v);
    void loadFromValueTree(const juce::ValueTree& v);

    /** Checks if a CC is in conflict. */
    bool hasConflict(int ccNumber) const;

    /** RT-safe access to the list of learnable parameters. */
    static const juce::StringArray& getLearnableParams();
    static int getParamIndex(const juce::String& paramID);

private:
    juce::AudioProcessorValueTreeState& apvts;
    
    // Real-time safe storage: store the index of the parameter in the modulatable list
    // -1 means no mapping for that CC.
    std::array<std::atomic<int>, 128> ccToIndex;

    void updateInternalMaps();
};

} // namespace NEURONiK::Main
