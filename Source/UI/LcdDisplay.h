#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace NEURONiK::UI
{

/**
 * A specialized component that emulates a 16x2 character LCD display.
 * Includes support for automatic scrolling, parameter previews, and custom styling.
 */
class LcdDisplay : public juce::Component,
                  private juce::Timer
{
public:
    LcdDisplay();
    ~LcdDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Sets the text for a specific line (0 or 1). 
        If the text is > 16 chars, it will automatically scroll. */
    void setLine(int lineIdx, const juce::String& text, bool forceNoScroll = false);

    /** Temporarily shows a parameter name and value. 
        Returns to normal state after a timeout. */
    void showParameterPreview(const juce::String& paramName, const juce::String& value);

    /** Sets the default background text (usually Preset Name and Bank) */
    void setDefaultText(const juce::String& line1, const juce::String& line2);

private:
    void timerCallback() override;
    juce::String getDisplayString(int lineIdx) const;

    juce::String defaultLines[2];
    juce::String currentLines[2];
    
    int scrollOffsets[2] = { 0, 0 };
    int scrollTimers[2] = { 0, 0 };
    
    bool isShowingPreview = false;
    int previewTimeoutCounter = 0;

    static constexpr int MaxChars = 16;
    static constexpr int PreviewDurationTicks = 15; // @150ms = ~2.2s

    void updateScroll(int lineIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LcdDisplay)
};

} // namespace NEURONiK::UI
