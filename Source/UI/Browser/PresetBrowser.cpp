#include "PresetBrowser.h"
#include "ThemeManager.h"
#include "PresetListModels.h"

namespace NEURONiK::UI::Browser
{

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
    bankList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xFF222222).withAlpha(0.6f));
    bankList.setColour(juce::ListBox::outlineColourId, juce::Colours::white.withAlpha(0.1f));
    bankList.setOutlineThickness(1);
    addAndMakeVisible(bankList);

    addAndMakeVisible(addBankButton);
    addAndMakeVisible(bankLabel);
    addAndMakeVisible(patchesLabel);

    bankLabel.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    bankLabel.setColour(juce::Label::textColourId, juce::Colours::cyan.withAlpha(0.7f));
    patchesLabel.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    patchesLabel.setColour(juce::Label::textColourId, juce::Colours::cyan.withAlpha(0.7f));
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
    presetModel = std::make_unique<PresetListModel>(presetFiles, 
        [this](int idx) { onPresetSelected(idx); },
        [this](int idx) {
            juce::PopupMenu m;
            m.addItem(1, "Move to Bank...");
            m.showMenuAsync(juce::PopupMenu::Options(), [this, idx](int res) {
                if (res == 1) {
                    juce::PopupMenu banksMenu;
                    for (int i = 0; i < banks.size(); ++i)
                        banksMenu.addItem(100 + i, banks[i].getFileName());
                    
                    banksMenu.showMenuAsync(juce::PopupMenu::Options(), [this, idx](int bankRes) {
                        if (bankRes >= 100) {
                            auto target = banks[bankRes - 100].getChildFile(presetFiles[idx].getFileName());
                            if (presetFiles[idx].moveFileTo(target))
                                refresh();
                        }
                    });
                }
            });
        }
    );
    presetList.setModel(presetModel.get());
    presetList.setRowHeight(28); // Slightly taller for cleaner look
    presetList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xFF252525).withAlpha(0.5f));
    presetList.setColour(juce::ListBox::outlineColourId, juce::Colours::white.withAlpha(0.08f));
    presetList.setOutlineThickness(1);
    addAndMakeVisible(presetList);

    // --- Search Setup ---
    addAndMakeVisible(searchBox);
    searchBox.setTextToShowWhenEmpty("Filter presets...", juce::Colours::grey);
    searchBox.onTextChange = [this] { filterPresets(searchBox.getText()); };
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1A1A1A));
    searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colours::cyan.withAlpha(0.3f));
    searchBox.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::cyan.withAlpha(0.8f));

    addAndMakeVisible(loadBankButton);
    loadBankButton.onClick = [this] {
        processor.getPresetManager().loadBank(juce::File()); // Simplified; would use chooser
        refresh();
    };

    addAndMakeVisible(tagsTitle);
    tagsTitle.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
    tagsTitle.setColour(juce::Label::textColourId, juce::Colours::cyan.withAlpha(0.8f));
    
    addAndMakeVisible(tagsEditor);
    tagsEditor.setTextToShowWhenEmpty("e.g. Bass, Lead...", juce::Colours::grey);
    tagsEditor.onReturnKey = [this] { updateTagsForCurrentSelection(); };
    tagsEditor.onTextChange = [this] { showTagSuggestions(); };
    tagsEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1A1A1A));
    tagsEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::cyan.withAlpha(0.3f));

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

    addAndMakeVisible(loadBankButton);
    loadBankButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF335533));
    loadBankButton.onClick = [this] {
        auto fc = std::make_unique<juce::FileChooser>("Load Bank...", juce::File(), "*.neuronikbank");
        fc->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& chooser) {
            auto file = chooser.getResult();
            if (file.exists()) {
                processor.getPresetManager().loadBank(file);
                refresh();
            }
        });
    };

    addAndMakeVisible(saveBankButton);
    saveBankButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF553333));
    saveBankButton.onClick = [this] {
        int bankIdx = bankList.getSelectedRow();
        if (bankIdx >= 0) {
            auto fc = std::make_unique<juce::FileChooser>("Save Bank...", juce::File(), "*.neuronikbank");
            fc->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, [this, bankIdx](const juce::FileChooser& chooser) {
                auto file = chooser.getResult();
                if (file.getParentDirectory().exists()) // Check generally valid path
                    processor.getPresetManager().saveBank(file, banks[bankIdx]);
            });
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
        auto allFiles = bankDir.findChildFiles(juce::File::findFiles, false, "*.neuronikpreset");
        
        for (const auto& f : allFiles)
        {
            if (currentSearchTerm.isEmpty()) {
                presetFiles.add(f);
                continue;
            }

            // Search in Name
            if (f.getFileNameWithoutExtension().containsIgnoreCase(currentSearchTerm)) {
                presetFiles.add(f);
                continue;
            }

            // Search in Tags
            auto tags = processor.getPresetManager().getTagsForPreset(f);
            for (const auto& tag : tags) {
                if (tag.containsIgnoreCase(currentSearchTerm)) {
                    presetFiles.add(f);
                    break;
                }
            }
        }
        presetFiles.sort();
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
        
        auto tags = processor.getPresetManager().getTagsForPreset(f);
        tagsEditor.setText(tags.joinIntoString(", "), false);
        
        processor.getPresetManager().loadPresetFromFile(f);
    }
}

void PresetBrowser::updateTagsForCurrentSelection()
{
    int idx = presetList.getSelectedRow();
    if (idx >= 0 && idx < presetFiles.size())
    {
        juce::StringArray tags;
        tags.addTokens(tagsEditor.getText(), ",", "\"");
        tags.trim();
        tags.removeEmptyStrings();
        processor.getPresetManager().setTagsForPreset(presetFiles[idx], tags);
    }
}

void PresetBrowser::showTagSuggestions()
{
    juce::String text = tagsEditor.getText();
    if (text.isEmpty()) return;

    // Get the last token being typed
    juce::StringArray tokens;
    tokens.addTokens(text, ",", "\"");
    if (tokens.size() == 0) return;
    juce::String lastToken = tokens[tokens.size() - 1].trim();
    if (lastToken.length() < 2) return;

    auto allTags = processor.getPresetManager().getAllUniqueTags();
    juce::PopupMenu m;
    int count = 0;
    for (const auto& tag : allTags)
    {
        if (tag.startsWithIgnoreCase(lastToken) && tag != lastToken)
        {
            m.addItem(++count, tag);
            if (count > 10) break;
        }
    }

    if (count > 0)
    {
       m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&tagsEditor), [this, tokens](int res) {
           if (res > 0) {
               auto allTags = processor.getPresetManager().getAllUniqueTags();
               juce::String selected = allTags[res - 1];
               
               auto newTokens = tokens;
               newTokens.remove(newTokens.size() - 1);
               newTokens.add(selected);
               tagsEditor.setText(newTokens.joinIntoString(", ") + ", ", true);
               updateTagsForCurrentSelection();
           }
       });
    }
}

void PresetBrowser::refresh()
{
    scanBanks();
}

void PresetBrowser::paint(juce::Graphics& g)
{
    auto backgroundColor = juce::Colour(0xFF1A1A1A);
    g.fillAll(backgroundColor); 
    
    auto area = getLocalBounds();
    int colWidth = area.getWidth() / 3;
    
    // Column backgrounds with subtle neural glow
    const auto& theme = ThemeManager::getCurrentTheme();
    g.setColour(theme.background.withAlpha(0.25f));
    g.fillRect(area.removeFromLeft(colWidth)); 
    
    // Separators updated to theme accent with glow
    juce::Colour separatorColor = theme.accent.withAlpha(0.2f);
    g.setColour(separatorColor);
    g.drawVerticalLine(colWidth, 0.0f, (float)getHeight());
    g.drawVerticalLine(colWidth * 2, 0.0f, (float)getHeight());
    
    // Row separator updated to theme accent
    g.drawHorizontalLine(40, 0.0f, (float)getWidth());

    // Decorative glow at the top
    juce::ColourGradient topGlow(theme.accent.withAlpha(0.05f), 0, 0,
                               juce::Colours::transparentBlack, 0, 40, false);
    g.setGradientFill(topGlow);
    g.fillRect(getLocalBounds().removeFromTop(40));
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
        bankLabel.setBounds(bankArea.removeFromTop(20));
        bankList.setBounds(bankArea.reduced(2));
    }
    
    area.removeFromLeft(8);
    
    // Column 2: Presets + Search
    auto presetArea = area.removeFromLeft(colWidth);
    {
        searchBox.setBounds(presetArea.removeFromBottom(25).reduced(2));
        patchesLabel.setBounds(presetArea.removeFromTop(20));
        presetList.setBounds(presetArea.reduced(2));
    }
    
    area.removeFromLeft(8);

    // Column 3: Info
    auto infoArea = area;
    titleInfo.setBounds(infoArea.removeFromTop(30));
    metadataLabel.setBounds(infoArea.removeFromTop(120).reduced(10));
    
    auto tagsArea = infoArea.removeFromTop(60).reduced(10, 0);
    tagsTitle.setBounds(tagsArea.removeFromTop(15));
    tagsEditor.setBounds(tagsArea.removeFromTop(25).reduced(2, 0));

    auto buttonArea = infoArea.removeFromBottom(120).reduced(10, 0); // Reduced height for tighter packing
    
    // Grid Setup
    int numCols = 2;
    int colW = buttonArea.getWidth() / numCols;
    int rowH = 35;
    
    // Row 1
    auto row1 = buttonArea.removeFromTop(rowH);
    loadButton.setBounds(row1.removeFromLeft(colW).reduced(2));
    saveButton.setBounds(row1.reduced(2));
    
    buttonArea.removeFromTop(5); // Spacer

    // Row 2
    auto row2 = buttonArea.removeFromTop(rowH);
    loadBankButton.setBounds(row2.removeFromLeft(colW).reduced(2));
    saveBankButton.setBounds(row2.reduced(2));
    
    buttonArea.removeFromTop(5); // Spacer

    // Row 3 (Delete takes full width)
    deleteButton.setBounds(buttonArea.removeFromTop(rowH).reduced(2));
}

} // namespace NEURONiK::UI::Browser
