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
    // Load existing tags if file exists to preserve them
    auto tags = getTagsForPreset(file);
    
    auto xml = valueTreeState.copyState().createXml();
    if (tags.size() > 0)
    {
        auto* metadata = xml->createNewChildElement("METADATA");
        metadata->setAttribute("tags", tags.joinIntoString(","));
    }
    
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

void PresetManager::saveBank(const juce::File& targetFile, const juce::File& sourceDir)
{
    if (!sourceDir.isDirectory()) return;
    
    // JUCE ZipFile requires a stream
    if (targetFile.existsAsFile()) targetFile.deleteFile();
    
    juce::FileOutputStream fos(targetFile);
    if (fos.openedOk())
    {
        juce::ZipFile::Builder builder;
        juce::Array<juce::File> filesToAdd;
        sourceDir.findChildFiles(filesToAdd, juce::File::findFiles, true, "*" + presetExtension);
        
        for (const auto& f : filesToAdd)
        {
            // Relative path inside zip
            auto relPath = f.getRelativePathFrom(sourceDir);
            builder.addFile(f, 9, relPath);
        }
        
        builder.writeToStream(fos, nullptr);
    }
}

void PresetManager::loadBank(const juce::File& bankFile)
{
    if (!bankFile.existsAsFile()) return;
    
    juce::ZipFile zip(bankFile);
    auto targetDir = getPresetsDirectory().getChildFile(bankFile.getFileNameWithoutExtension());
    if (!targetDir.exists()) targetDir.createDirectory();
    
    zip.uncompressEntry(0, targetDir); // Unpack all? JUCE's ZipFile doesn't have "unpack all" in one call easily
    // We iterate entries
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        zip.uncompressEntry(i, targetDir);
    }
}

void PresetManager::setTagsForPreset(const juce::File& file, const juce::StringArray& tags)
{
    if (!file.existsAsFile()) return;
    
    auto xml = juce::parseXML(file);
    if (xml != nullptr)
    {
        auto* metadata = xml->getChildByName("METADATA");
        if (metadata == nullptr) metadata = xml->createNewChildElement("METADATA");
        
        metadata->setAttribute("tags", tags.joinIntoString(","));
        xml->writeTo(file);
    }
}

juce::StringArray PresetManager::getTagsForPreset(const juce::File& file) const
{
    if (!file.existsAsFile()) return {};
    
    auto xml = juce::parseXML(file);
    if (xml != nullptr)
    {
        if (auto* metadata = xml->getChildByName("METADATA"))
        {
            juce::String tagsCsv = metadata->getStringAttribute("tags");
            juce::StringArray tags;
            tags.addTokens(tagsCsv, ",", "\"");
            tags.trim();
            tags.removeEmptyStrings();
            return tags;
        }
    }
    return {};
}

juce::StringArray PresetManager::getAllUniqueTags() const
{
    juce::StringArray uniqueTags;
    auto presetsDir = getPresetsDirectory();
    auto files = presetsDir.findChildFiles(juce::File::findFiles, true, "*" + presetExtension);
    
    for (const auto& f : files)
    {
        auto tags = getTagsForPreset(f);
        for (const auto& tag : tags)
        {
            if (!uniqueTags.contains(tag, true)) // Case-insensitive check
                uniqueTags.add(tag);
        }
    }
    
    uniqueTags.sort(true);
    return uniqueTags;
}

} // namespace NEURONiK::Serialization
