/*
  ==============================================================================

    EnvelopeVisualizer.h
    Created: 26 Jan 2026
    Description: Visualizes an ADSR envelope with real-time feedback. (Inlined)

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ThemeManager.h"

namespace NEURONiK::UI
{

class EnvelopeVisualizer : public juce::Component, private juce::Timer
{
public:
    EnvelopeVisualizer(std::atomic<float>& attack,
                       std::atomic<float>& decay,
                       std::atomic<float>& sustain,
                       std::atomic<float>& release,
                       std::atomic<float>& activeLevel)
        : att(attack), dec(decay), sus(sustain), rel(release), envLevel(activeLevel)
    {
        startTimerHz(30);
    }
                       
    ~EnvelopeVisualizer() override
    {
        stopTimer();
    }

    void timerCallback() override
    {
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        
        // --- Screen Background (Always Visible) ---
        const auto& theme = ThemeManager::getCurrentTheme();
        g.setColour(theme.background.withAlpha(0.9f)); 
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // --- Subtle Screen Glow / Bezier ---
        juce::ColourGradient screenGlow(theme.surface, bounds.getCentreX(), bounds.getCentreY(),
                                      theme.background, bounds.getX(), bounds.getY(), true);
        g.setGradientFill(screenGlow);
        g.fillRoundedRectangle(bounds.reduced(1.0f), 4.0f);

        // --- Background Grid (Always Visible) ---
        g.setColour(theme.accent.withAlpha(0.05f));
        int numLines = 10;
        for (int i = 1; i < numLines; ++i)
        {
            float x = bounds.getX() + (bounds.getWidth() / numLines) * i;
            float y = bounds.getY() + (bounds.getHeight() / numLines) * i;
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
            g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
        }

        // --- ADSR Path Calculation ---
        auto plotArea = bounds.reduced(8.0f);
        auto bottom = plotArea.getBottom();
        auto top = plotArea.getY();
        auto left = plotArea.getX();
        auto width = plotArea.getWidth();
        auto height = plotArea.getHeight();

        float a = att.load(std::memory_order_relaxed);
        float d = dec.load(std::memory_order_relaxed);
        float s = sus.load(std::memory_order_relaxed);
        float r = rel.load(std::memory_order_relaxed);

        float totalTime = a + d + r + 0.5f;
        if (totalTime < 0.001f) totalTime = 1.0f;

        float wA = (a / totalTime) * width;
        float wD = (d / totalTime) * width;
        float wS = (0.5f / totalTime) * width;
        float wR = (r / totalTime) * width;

        juce::Point<float> pStart(left, bottom);
        juce::Point<float> pPeak(left + wA, top);
        juce::Point<float> pSustainStart(left + wA + wD, bottom - (s * height));
        juce::Point<float> pSustainEnd(left + wA + wD + wS, bottom - (s * height));
        juce::Point<float> pEnd(left + wA + wD + wS + wR, bottom);

        juce::Path envPath;
        envPath.startNewSubPath(pStart);
        envPath.lineTo(pPeak);
        envPath.lineTo(pSustainStart);
        envPath.lineTo(pSustainEnd);
        envPath.lineTo(pEnd);

        // Static path (base)
        g.setColour(theme.accent.withAlpha(0.2f));
        g.strokePath(envPath, juce::PathStrokeType(1.5f));

        // Real-time feedback
        float currentLevel = envLevel.load(std::memory_order_relaxed);
        
        if (currentLevel > 0.001f)
        {
            g.setColour(theme.accent.withAlpha(0.4f + currentLevel * 0.5f));
            g.strokePath(envPath, juce::PathStrokeType(2.5f + currentLevel * 1.0f));

            juce::Path fillPath = envPath;
            fillPath.lineTo(pEnd.x, bottom);
            fillPath.lineTo(pStart.x, bottom);
            fillPath.closeSubPath();
            
            g.setGradientFill(juce::ColourGradient(theme.accent.withAlpha(0.25f * currentLevel), 
                                                  plotArea.getCentreX(), bottom, 
                                                  theme.accent.withAlpha(0.0f), 
                                                  plotArea.getCentreX(), top, false));
            g.fillPath(fillPath);
            
            // Neon Glow around the path
            g.setColour(theme.accent.withAlpha(0.15f * currentLevel));
            g.strokePath(envPath, juce::PathStrokeType(6.0f));
        }
        
        // Scanlines overlay (Screen effect)
        g.setColour(theme.background.withAlpha(0.1f));
        for (int y = static_cast<int>(bounds.getY()); y < bounds.getBottom(); y += 2)
            g.drawHorizontalLine(y, bounds.getX(), bounds.getRight());

    }

    void resized() override
    {
    }

private:
    std::atomic<float>& att;
    std::atomic<float>& dec;
    std::atomic<float>& sus;
    std::atomic<float>& rel;
    std::atomic<float>& envLevel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeVisualizer)
};

} // namespace NEURONiK::UI
