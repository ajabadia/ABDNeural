/*
  ==============================================================================

    PresetListModels.h
    Created: 26 Jan 2026
    Description: ListBox models for the Preset Browser.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace NEURONiK::UI::Browser
{

class BankListModel : public juce::ListBoxModel
{
public:
    BankListModel(juce::Array<juce::File>& _banks, std::function<void(int)> _callback)
        : banks(_banks), onSelectionCtx(_callback) {}

    int getNumRows() override { return banks.size(); }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll(juce::Colour(0xFF004444)); 

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
    PresetListModel(juce::Array<juce::File>& _files, std::function<void(int)> _callback, std::function<void(int)> _rightClickCallback)
        : files(_files), onSelectionCtx(_callback), onRightClickCtx(_rightClickCallback) {}

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

    void listBoxItemClicked(int row, const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onRightClickCtx)
            onRightClickCtx(row);
    }

private:
    juce::Array<juce::File>& files;
    std::function<void(int)> onSelectionCtx;
    std::function<void(int)> onRightClickCtx;
};

} // namespace NEURONiK::UI::Browser
