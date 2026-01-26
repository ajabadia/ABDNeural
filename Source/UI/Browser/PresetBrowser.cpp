/*
  ==============================================================================

    PresetBrowser.cpp
    Created: 25 Jan 2026
    Description: Implementation of the Advanced Preset Browser.

  ==============================================================================
*/

#include "PresetBrowser.h"

namespace NEURONiK::UI::Browser
{

// ============================================================================
// Internal List Models
// ============================================================================

class BankListModel : public juce::ListBoxModel
{
public:
    BankListModel(juce::Array<juce::File>& _banks, std::function<void(int)> _callback)
        : banks(_banks), onSelectionCtx(_callback) {}

    int getNumRows() override { return banks.size(); }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll(juce::Colour(0xFF004444)); // Cyan/Dark background

        g.setColour(rowIsSelected ? juce::Colours::cyan : juce::Colours::grey);
        g.setFont(juce::Font(juce::FontOptions(15.0f).withStyle(rowIsSelected ? "Bold" : "Plain")));
        
        g.drawText(banks[rowNumber].getFileName(), 5, 0, width - 5, height, juce::Justification::centredLeft);
    }

    void selectedRowsChanged(int lastRowSelected) override
    {
        if (onSelectionCtx) onSelectionCtx(lastRowSelected);
    }

private:
    juce::Array<juce::File>& banks;
    std::function<void(int)> onSelectionCtx;
};

class PresetListModel : public juce::ListBoxModel
{
public:
    PresetListModel(juce::Array<juce::File>& _files, std::function<void(int)> _callback)
        : files(_files), onSelectionCtx(_callback) {}

    int getNumRows() override { return files.size(); }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll(juce::Colour(0xFF005555));

        g.setColour(rowIsSelected ? juce::Colours::white : juce::Colours::lightgrey);
        g.setFont(14.0f);
        
        juce::String name = files[rowNumber].getFileNameWithoutExtension();
        g.drawText(name, 10, 0, width - 10, height, juce::Justification::centredLeft);
    }

    void selectedRowsChanged(int lastRowSelected) override
    {
        if (onSelectionCtx) onSelectionCtx(lastRowSelected);
    }

    void listBoxItemClicked(int /*row*/, const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            // Optional: Right click menu
        }
    }

private:
    juce::Array<juce::File>& files;
    std::function<void(int)> onSelectionCtx;
};

// ============================================================================
// PresetBrowser Implementation
// ============================================================================

PresetBrowser::PresetBrowser(NEURONiKProcessor& p)
    : processor(p)
{
    rootDirectory = processor.getPresetManager().getPresetsDirectory();
    
    // --- Bank List Setup ---
    bankModel = std::make_unique<BankListModel>(banks, [this](int idx) { loadPresetsForBank(idx); });
    bankList.setModel(bankModel.get());
    bankList.setRowHeight(30);
    bankList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xFF111111));
    bankList.setOutlineThickness(1);
    addAndMakeVisible(bankList);

    addAndMakeVisible(addBankButton);
    addBankButton.onClick = [this] {
        auto* alert = new juce::AlertWindow("New Bank", "Enter name for new folder:", juce::AlertWindow::NoIcon, this);
        alert->addTextEditor("bankName", "");
        alert->addButton("Create", 1);
        alert->addButton("Cancel", 0);
        alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert](int res) {
            if (res == 1) {
                auto name = alert->getTextEditorContents("bankName");
                if (name.isNotEmpty()) {
                    rootDirectory.getChildFile(name).createDirectory();
                    scanBanks();
                }
            }
        }), true);
    };

    // --- Preset List Setup ---
    presetModel = std::make_unique<PresetListModel>(presetFiles, [this](int idx) { onPresetSelected(idx); });
    presetList.setModel(presetModel.get());
    presetList.setRowHeight(25);
    presetList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xFF181818));
    presetList.setOutlineThickness(1);
    addAndMakeVisible(presetList);

    // --- Search Setup ---
    addAndMakeVisible(searchBox);
    searchBox.setTextToShowWhenEmpty("Filter presets...", juce::Colours::grey);
    searchBox.onTextChange = [this] { filterPresets(searchBox.getText()); };
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF222222));

    // --- Info Area ---
    addAndMakeVisible(titleInfo);
    titleInfo.setColour(juce::Label::textColourId, juce::Colours::cyan);
    titleInfo.setFont(juce::Font(juce::FontOptions(18.0f).withStyle("Bold")));
    titleInfo.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(metadataLabel);
    metadataLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    metadataLabel.setJustificationType(juce::Justification::topLeft);
    metadataLabel.setText("No preset selected", juce::dontSendNotification);

    addAndMakeVisible(loadButton);
    loadButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF006666));
    loadButton.onClick = [this] {
        int idx = presetList.getSelectedRow();
        if (idx >= 0 && idx < presetFiles.size())
        {
            processor.getPresetManager().loadPresetFromFile(presetFiles[idx]);
        }
    };

    addAndMakeVisible(saveButton);
    saveButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF005555));
    saveButton.onClick = [this] {
        int bankIdx = bankList.getSelectedRow();
        if (bankIdx >= 0 && bankIdx < banks.size())
        {
            juce::File bankDir = banks[bankIdx];
            if (bankDir.isDirectory())
            {
               auto* alert = new juce::AlertWindow("Save Preset", "Enter name for preset in '" + bankDir.getFileName() + "':", juce::AlertWindow::NoIcon, this);
                alert->addTextEditor("presetName", processor.getPresetManager().getCurrentPreset());
                alert->addButton("Save", 1);
                alert->addButton("Cancel", 0);
                alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert, bankDir](int res) {
                    if (res == 1) {
                        auto name = alert->getTextEditorContents("presetName");
                        if (name.isNotEmpty()) {
                            auto file = bankDir.getChildFile(name + ".neuronikpreset");
                            processor.getPresetManager().savePresetToFile(file);
                            refresh(); // Reload lists
                        }
                    }
                }), true);
            }
        }
        else
        {
             juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Save Error", "Please select a Bank (Folder) to save into.");
        }
    };

    addAndMakeVisible(deleteButton);
    deleteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF882222));
    deleteButton.onClick = [this] {
        int idx = presetList.getSelectedRow();
        if (idx >= 0 && idx < presetFiles.size())
        {
            juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::QuestionIcon)
                .withTitle("Delete")
                .withMessage("Are you sure?")
                .withAssociatedComponent(this),
                juce::ModalCallbackFunction::create([this, idx](int res) {
                    if (res != 0) { // In showAsync with Ok/Cancel, Ok is usually 1
                        presetFiles[idx].deleteFile();
                        loadPresetsForBank(bankList.getSelectedRow());
                    }
                }));
        }
    };

    refresh();
}

void PresetBrowser::filterPresets(const juce::String& filterText)
{
    currentSearchTerm = filterText;
    loadPresetsForBank(bankList.getSelectedRow());
}

PresetBrowser::~PresetBrowser()
{
    bankList.setModel(nullptr);
    presetList.setModel(nullptr);
}

void PresetBrowser::scanBanks()
{
    banks.clear();
    
    // First, add a "ALL" virtual bank if we wanted to search everywhere, 
    // but for now let's just do folders.
    banks.add(rootDirectory);

    juce::RangedDirectoryIterator iter(rootDirectory, false, "*", juce::File::findDirectories);
    for (const auto& entry : iter)
    {
        banks.add(entry.getFile());
    }

    bankList.updateContent();
    bankList.selectRow(0);
}

void PresetBrowser::loadPresetsForBank(int bankIndex)
{
    presetFiles.clear();
    
    if (bankIndex >= 0 && bankIndex < banks.size())
    {
        juce::File bankDir = banks[bankIndex];
        auto pattern = currentSearchTerm.isEmpty() ? "*.neuronikpreset" : "*" + currentSearchTerm + "*.neuronikpreset";
        
        auto files = bankDir.findChildFiles(juce::File::findFiles, false, pattern);
        files.sort();
        
        for (const auto& f : files)
            presetFiles.add(f);
    }
    
    presetList.updateContent();
}

void PresetBrowser::onPresetSelected(int index)
{
    if (index >= 0 && index < presetFiles.size())
    {
        juce::File f = presetFiles[index];
        
        juce::String info;
        info << "NAME: " << f.getFileNameWithoutExtension().toUpperCase() << "\n\n";
        info << "LOCATION: " << f.getParentDirectory().getFileName() << "\n";
        info << "SIZE: " << juce::String(f.getSize() / 1024.0, 1) << " KB\n";
        info << "MODIFIED: " << f.getLastModificationTime().toString(true, true) << "\n";
        
        metadataLabel.setText(info, juce::dontSendNotification);
        
        // Instant Preview
        processor.getPresetManager().loadPresetFromFile(f);
    }
}

void PresetBrowser::refresh()
{
    scanBanks();
}

void PresetBrowser::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF141414));
    
    auto area = getLocalBounds();
    int colWidth = area.getWidth() / 3;
    
    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.fillRect(area.removeFromLeft(colWidth)); 
    
    g.setColour(juce::Colours::cyan.withAlpha(0.1f));
    g.drawVerticalLine(colWidth, 0.0f, (float)getHeight());
    g.drawVerticalLine(colWidth * 2, 0.0f, (float)getHeight());
}

void PresetBrowser::resized()
{
    auto area = getLocalBounds().reduced(8);
    int colWidth = area.getWidth() / 3;
    
    // Column 1: Banks
    auto bankArea = area.removeFromLeft(colWidth);
    {
        auto labelArea = bankArea.removeFromTop(20);
        addBankButton.setBounds(labelArea.removeFromRight(25).reduced(2));
        
        juce::Label* lbl = new juce::Label("l1", "CATEGORY / BANK");
        lbl->setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
        lbl->setColour(juce::Label::textColourId, juce::Colours::cyan.withAlpha(0.7f));
        addAndMakeVisible(lbl);
        lbl->setBounds(bankArea.removeFromTop(20));
        bankList.setBounds(bankArea.reduced(2));
    }
    
    area.removeFromLeft(8);
    
    // Column 2: Presets + Search
    auto presetArea = area.removeFromLeft(colWidth);
    {
        searchBox.setBounds(presetArea.removeFromBottom(25).reduced(2));
        
        juce::Label* lbl = new juce::Label("l2", "PATCHES");
        lbl->setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
        lbl->setColour(juce::Label::textColourId, juce::Colours::cyan.withAlpha(0.7f));
        addAndMakeVisible(lbl);
        lbl->setBounds(presetArea.removeFromTop(20));
        presetList.setBounds(presetArea.reduced(2));
    }
    
    area.removeFromLeft(8);

    // Column 3: Info
    auto infoArea = area;
    titleInfo.setBounds(infoArea.removeFromTop(30));
    metadataLabel.setBounds(infoArea.removeFromTop(200).reduced(10));
    
    auto buttonArea = infoArea.removeFromBottom(130).reduced(10, 0); // Increased height
    loadButton.setBounds(buttonArea.removeFromTop(35).reduced(2));
    saveButton.setBounds(buttonArea.removeFromTop(35).reduced(2));
    buttonArea.removeFromTop(5);
    deleteButton.setBounds(buttonArea.removeFromTop(30).reduced(10, 2));
}

} // namespace NEURONiK::UI::Browser
