#include "LcdDisplay.h"
#include "ThemeManager.h"

namespace NEURONiK::UI
{

LcdDisplay::LcdDisplay()
{
    defaultLines[0] = "NEURONIK";
    defaultLines[1] = "SPECTRAL MORPHING";
    
    currentLines[0] = defaultLines[0];
    currentLines[1] = defaultLines[1];

    startTimer(150); // Refresh for scrolling and timeouts
}

LcdDisplay::~LcdDisplay()
{
    stopTimer();
}

void LcdDisplay::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    
    // --- Plastic Frame ---
    g.setColour(juce::Colour(0xFF151515));
    g.fillRoundedRectangle(area, 4.0f);
    
    // --- Screen Background (Product-Specific Backlight) ---
    auto screenArea = area.reduced(4.0f);
    const auto& theme = ThemeManager::getCurrentTheme();
    g.setColour(theme.lcdBackground.withAlpha(1.0f)); // Background is already alpha-aware in theme
    g.fillRoundedRectangle(screenArea, 2.0f);
    
    // --- Subtle Grid Pattern (Emulate Matrix Pixels) ---
    g.setColour(juce::Colours::black.withAlpha(0.2f));
    const float charW = screenArea.getWidth() / MaxChars;
    const float charH = screenArea.getHeight() / 2.0f;
    
    for (int i = 0; i <= MaxChars; ++i)
        g.drawVerticalLine(static_cast<int>(screenArea.getX() + i * charW), screenArea.getY(), screenArea.getBottom());
    g.drawHorizontalLine(static_cast<int>(screenArea.getY() + charH), screenArea.getX(), screenArea.getRight());

    // --- Text Rendering ---
    // Using a monospaced-style font
    g.setColour(theme.lcdText);
    g.setFont(juce::Font(juce::FontOptions(charH * 0.8f).withStyle("Bold")));

    for (int line = 0; line < 2; ++line)
    {
        auto text = getDisplayString(line);
        auto lineRect = juce::Rectangle<float>(screenArea.getX() + 4.0f, 
                                             screenArea.getY() + (line * charH), 
                                             screenArea.getWidth() - 8.0f, 
                                             charH);
        
        // Draw each char with slight offset to simulate matrix
        g.drawText(text, lineRect, juce::Justification::centredLeft, false);
    }
    
    // Scanline/Glass overlay
    juce::ColourGradient glass(theme.text.withAlpha(0.05f), 0, 0,
                              juce::Colours::transparentBlack, 0, static_cast<float>(getHeight()), false);
    g.setGradientFill(glass);
    g.fillRect(screenArea);
}

void LcdDisplay::resized()
{
}

void LcdDisplay::setLine(int lineIdx, const juce::String& text, bool forceNoScroll)
{
    juce::ignoreUnused(forceNoScroll);
    if (lineIdx < 0 || lineIdx > 1) return;
    
    if (currentLines[lineIdx] != text)
    {
        currentLines[lineIdx] = text;
        scrollOffsets[lineIdx] = 0;
        scrollTimers[lineIdx] = 0;
        repaint();
    }
}

void LcdDisplay::setDefaultText(const juce::String& line1, const juce::String& line2)
{
    defaultLines[0] = line1;
    defaultLines[1] = line2;
    
    if (!isShowingPreview)
    {
        setLine(0, line1);
        setLine(1, line2);
    }
}

void LcdDisplay::showParameterPreview(const juce::String& paramName, const juce::String& value)
{
    isShowingPreview = true;
    previewTimeoutCounter = PreviewDurationTicks;
    
    setLine(0, paramName.toUpperCase());
    setLine(1, "> " + value);
}

void LcdDisplay::timerCallback()
{
    bool needsRepaint = false;

    // Handle Previews
    if (isShowingPreview)
    {
        if (--previewTimeoutCounter <= 0)
        {
            isShowingPreview = false;
            setLine(0, defaultLines[0]);
            setLine(1, defaultLines[1]);
            needsRepaint = true;
        }
    }

    // Handle Scrolling
    for (int i = 0; i < 2; ++i)
    {
        if (currentLines[i].length() > MaxChars)
        {
            updateScroll(i);
            needsRepaint = true;
        }
    }

    if (needsRepaint)
        repaint();
}

void LcdDisplay::updateScroll(int lineIdx)
{
    if (++scrollTimers[lineIdx] > 4) // Delay before starting scroll
    {
        scrollOffsets[lineIdx]++;
        
        // Reset scroll if we reached the end (with some padding)
        if (scrollOffsets[lineIdx] > currentLines[lineIdx].length())
        {
            scrollOffsets[lineIdx] = 0;
            scrollTimers[lineIdx] = -6; // Longer pause at start
        }
    }
}

juce::String LcdDisplay::getDisplayString(int lineIdx) const
{
    juce::String text = currentLines[lineIdx];
    
    if (text.length() <= MaxChars)
        return text;
        
    // Circular scroll logic
    juce::String working = text + "  ---  " + text;
    return working.substring(scrollOffsets[lineIdx], scrollOffsets[lineIdx] + MaxChars);
}

} // namespace NEURONiK::UI
