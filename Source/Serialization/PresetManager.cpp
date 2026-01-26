/*
  ==============================================================================

    PresetManager.cpp
    Created: 25 Jan 2026
    Description: Implementation of PresetManager.

  ==============================================================================
*/

#include "PresetManager.h"

namespace NEURONiK::Serialization {

const juce::String PresetManager::presetExtension = ".neuronikpreset";

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts)
    : valueTreeState(apvts), currentPresetName("Init Preset")
{
    // Ensure presets directory exists
    const auto presetsDir = getPresetsDirectory();
    if (!presetsDir.exists())
        presetsDir.createDirectory();
}

void PresetManager::savePreset(const juce::String& presetName)
{
    savePresetInFolder(presetName, "");
}

void PresetManager::savePresetInFolder(const juce::String& presetName, const juce::String& folderName)
{
    auto presetsDir = getPresetsDirectory();
    if (folderName.isNotEmpty())
    {
        presetsDir = presetsDir.getChildFile(folderName);
        if (!presetsDir.exists())
            presetsDir.createDirectory();
    }

    savePresetToFile(presetsDir.getChildFile(presetName + presetExtension));
}

void PresetManager::savePresetToFile(const juce::File& file)
{
    auto xml = valueTreeState.copyState().createXml();
    xml->writeTo(file);
    currentPresetName = file.getFileNameWithoutExtension();
}

void PresetManager::deletePreset(const juce::String& presetName)
{
    const auto presetsDir = getPresetsDirectory();
    // Search recursively for the file to delete
    auto files = presetsDir.findChildFiles(juce::File::findFiles, true, presetName + presetExtension);
    if (files.size() > 0)
        files[0].deleteFile();
}

void PresetManager::loadPreset(const juce::String& presetName)
{
    const auto presetsDir = getPresetsDirectory();
    auto files = presetsDir.findChildFiles(juce::File::findFiles, true, presetName + presetExtension);

    if (files.size() > 0)
    {
        loadPresetFromFile(files[0]);
    }
}

void PresetManager::loadPresetFromFile(const juce::File& file)
{
    if (file.existsAsFile())
    {
        auto xml = juce::parseXML(file);
        if (xml != nullptr)
        {
            valueTreeState.replaceState(juce::ValueTree::fromXml(*xml));
            currentPresetName = file.getFileNameWithoutExtension();
        }
    }
}

int PresetManager::loadNextPreset()
{
    const auto presets = getAllPresets();
    if (presets.isEmpty()) return -1;

    const auto currentIndex = presets.indexOf(currentPresetName);
    const auto nextIndex = (currentIndex + 1) % presets.size();
    loadPreset(presets[nextIndex]);
    return nextIndex;
}

int PresetManager::loadPreviousPreset()
{
    const auto presets = getAllPresets();
    if (presets.isEmpty()) return -1;

    const auto currentIndex = presets.indexOf(currentPresetName);
    const auto prevIndex = (currentIndex + presets.size() - 1) % presets.size();
    loadPreset(presets[prevIndex]);
    return prevIndex;
}

juce::StringArray PresetManager::getAllPresets() const
{
    juce::StringArray presets;
    const auto presetsDir = getPresetsDirectory();
    
    // Scan directory
    auto files = presetsDir.findChildFiles(juce::File::findFiles, false, "*" + presetExtension);
    files.sort();

    for (const auto& file : files)
    {
        presets.add(file.getFileNameWithoutExtension());
    }
    return presets;
}

juce::String PresetManager::getCurrentPreset() const
{
    return currentPresetName;
}

juce::File PresetManager::getPresetsDirectory() const
{
    // For now, save in Documents/NEURONiK/Presets
    auto result = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                  .getChildFile("NEURONiK")
                  .getChildFile("Presets");
    return result;
}

} // namespace NEURONiK::Serialization
