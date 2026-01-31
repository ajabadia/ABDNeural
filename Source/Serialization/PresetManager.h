/*
  ==============================================================================

    PresetManager.h
    Created: 25 Jan 2026
    Description: Handles saving and loading of presets (.neuronikpreset files).

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Common/SpectralModel.h"

namespace NEURONiK::Serialization {

class PresetManager
{
public:
    static const juce::String presetExtension;

    static Common::SpectralModel loadModelFromFile(const juce::File& file);

    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts);

    void savePreset(const juce::String& presetName);
    void savePresetInFolder(const juce::String& presetName, const juce::String& folderName);
    void savePresetToFile(const juce::File& file);
    void deletePreset(const juce::String& presetName);
    void loadPreset(const juce::String& presetName);
    void loadPresetFromFile(const juce::File& file);
    int loadNextPreset();
    int loadPreviousPreset();

    // --- Bank Support ---
    void saveBank(const juce::File& targetFile, const juce::File& sourceDir);
    void loadBank(const juce::File& bankFile);

    // --- Metadata Handling ---
    void setTagsForPreset(const juce::File& file, const juce::StringArray& tags);
    juce::StringArray getTagsForPreset(const juce::File& file) const;
    juce::StringArray getAllUniqueTags() const;

    juce::StringArray getAllPresets() const;
    juce::String getCurrentPreset() const;

    juce::File getPresetsDirectory() const;

private:
    void valueTreeRedirected(juce::ValueTree& tree);

    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::String currentPresetName;
};

} // namespace NEURONiK::Serialization
