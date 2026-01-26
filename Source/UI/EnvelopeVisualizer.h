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
        auto bounds = getLocalBounds().toFloat().reduced(5.0f);
        auto bottom = bounds.getBottom();
        auto top = bounds.getY();
        auto left = bounds.getX();
        auto width = bounds.getWidth();
        auto height = bounds.getHeight();

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

        g.setColour(juce::Colours::darkgrey.withAlpha(0.3f));
        g.strokePath(envPath, juce::PathStrokeType(2.0f));

        float currentLevel = envLevel.load(std::memory_order_relaxed);
        
        if (currentLevel > 0.001f)
        {
            g.setColour(juce::Colours::cyan.withAlpha(0.2f + currentLevel * 0.6f));
            g.strokePath(envPath, juce::PathStrokeType(2.0f + currentLevel * 1.5f));

            juce::Path fillPath = envPath;
            fillPath.lineTo(pEnd.x, bottom);
            fillPath.lineTo(pStart.x, bottom);
            fillPath.closeSubPath();
            
            g.setGradientFill(juce::ColourGradient(juce::Colours::cyan.withAlpha(0.3f * currentLevel), 
                                                  bounds.getCentreX(), bottom, 
                                                  juce::Colours::cyan.withAlpha(0.0f), 
                                                  bounds.getCentreX(), top, false));
            g.fillPath(fillPath);
        }
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
