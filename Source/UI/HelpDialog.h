/*
  ==============================================================================

    HelpDialog.h
    Created: 30 Jan 2026
    Description: Reusable help dialog component for displaying formatted text.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace NEURONiK::UI {

/**
 * A reusable dialog window for displaying help text with proper formatting.
 * Follows DRY principle - single component for all help dialogs.
 */
class HelpDialog
{
public:
    /**
     * Static helper to show a help dialog modally.
     */
    static void show(const juce::String& title, const juce::String& content)
    {
        // Create a simple text editor component for the content
        auto* textEditor = new juce::TextEditor();
        textEditor->setMultiLine(true, false);
        textEditor->setReadOnly(true);
        textEditor->setScrollbarsShown(true);
        textEditor->setText(content);
        textEditor->setFont(13.0f);
        textEditor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF0A0A0A));
        textEditor->setColour(juce::TextEditor::textColourId, juce::Colours::white.withAlpha(0.9f));
        textEditor->setColour(juce::TextEditor::outlineColourId, juce::Colours::cyan.withAlpha(0.3f));
        textEditor->setSize(600, 500);
        
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(textEditor);
        options.dialogTitle = title;
        options.dialogBackgroundColour = juce::Colour(0xFF0A0A0A);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = false;
        options.resizable = true;
        options.useBottomRightCornerResizer = true;
        
        options.launchAsync();
    }
};

} // namespace NEURONiK::UI
