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
        auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(4, 2);

        if (rowIsSelected)
        {
            // Selected background: subtle cyan gradient
            juce::ColourGradient grad(juce::Colour(0xFF004444).withAlpha(0.6f), bounds.getTopLeft().toFloat(),
                                     juce::Colour(0xFF002222).withAlpha(0.4f), bounds.getBottomLeft().toFloat(), false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
            
            g.setColour(juce::Colours::cyan.withAlpha(0.4f));
            g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 1.0f);
        }

        g.setColour(rowIsSelected ? juce::Colours::cyan : juce::Colours::white.withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions(14.0f).withStyle(rowIsSelected ? "Bold" : "Plain")));
        
        g.drawText(banks[rowNumber].getFileName().toUpperCase(), 12, 0, width - 20, height, juce::Justification::centredLeft);
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
        auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(6, 1);

        if (rowIsSelected)
        {
            g.setColour(juce::Colours::cyan.withAlpha(0.15f));
            g.fillRoundedRectangle(bounds.toFloat(), 3.0f);
            
            g.setColour(juce::Colours::cyan.withAlpha(0.3f));
            g.drawRoundedRectangle(bounds.toFloat(), 3.0f, 1.0f);
        }
        else
        {
            // Subtle separator
            g.setColour(juce::Colours::white.withAlpha(0.03f));
            g.drawHorizontalLine(height - 1, 10.0f, (float)width - 10.0f);
        }

        g.setColour(rowIsSelected ? juce::Colours::white : juce::Colours::white.withAlpha(0.8f));
        g.setFont(juce::Font(juce::FontOptions(13.0f).withStyle(rowIsSelected ? "Bold" : "Plain")));
        
        juce::String name = files[rowNumber].getFileNameWithoutExtension();
        g.drawText(name, 15, 0, width - 20, height, juce::Justification::centredLeft);
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
