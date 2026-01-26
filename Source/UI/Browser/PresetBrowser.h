/*
  ==============================================================================

    PresetBrowser.h
    Created: 25 Jan 2026
    Description: Advanced 3-column preset browser.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Main/NEURONiKProcessor.h"

namespace NEURONiK::UI::Browser
{

// Forward declaration of internal models
class BankListModel;
class PresetListModel;

class PresetBrowser : public juce::Component
{
public:
    explicit PresetBrowser(NEURONiKProcessor& p);
    ~PresetBrowser() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void refresh();

private:
    NEURONiKProcessor& processor;
    juce::File rootDirectory;

    // --- Data ---
    juce::Array<juce::File> banks;
    juce::Array<juce::File> presetFiles;

    // --- Models ---
    std::unique_ptr<BankListModel> bankModel;
    std::unique_ptr<PresetListModel> presetModel;

    // --- UI ---
    juce::ListBox bankList;
    juce::ListBox presetList;
    juce::TextButton addBankButton { "+" };
    
    // Search
    juce::TextEditor searchBox;
    
    // Info Panel
    juce::Label infoLabel;
    juce::TextButton loadButton { "LOAD PRESET" };
    juce::TextButton saveButton { "SAVE AS..." };
    juce::TextButton deleteButton { "DELETE" };
    
    // Bank Buttons
    juce::TextButton loadBankButton { "LOAD BANK" };
    juce::TextButton saveBankButton { "SAVE BANK" };

    juce::Label titleInfo { "Preset Info", "Selection Details" };
    juce::Label metadataLabel;
    
    // Tag Editor
    juce::Label tagsTitle { "tags", "TAGS:" };
    juce::TextEditor tagsEditor;
    juce::StringArray tagSuggestions;

    // --- Logic ---
    void scanBanks();
    void loadPresetsForBank(int bankIndex);
    void connectCallbacks();
    void onPresetSelected(int index);
    void filterPresets(const juce::String& filterText);
    void updateTagsForCurrentSelection();
    void showTagSuggestions();

    juce::String currentSearchTerm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};

} // namespace NEURONiK::UI::Browser
