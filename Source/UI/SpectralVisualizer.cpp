#include "SpectralVisualizer.h"
#include <cmath>

namespace Nexus::UI {

SpectralVisualizer::SpectralVisualizer(juce::AudioProcessorValueTreeState& vts)
    : vts_(vts)
{
    harmonicProfile_.fill(0.0f);
    startTimerHz(30); 
}

SpectralVisualizer::~SpectralVisualizer()
{
    stopTimer();
}

void SpectralVisualizer::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced(2.0f);
    
    // Glass Background
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillRoundedRectangle(area, 6.0f);
    
    // Inner Glow/Border
    g.setColour(juce::Colours::cyan.withAlpha(0.1f));
    g.drawRoundedRectangle(area, 6.0f, 1.0f);
    
    // Grid Lines (Subtle)
    g.setColour(juce::Colours::white.withAlpha(0.03f));
    for (int i = 1; i < 4; ++i)
    {
        float y = area.getY() + area.getHeight() * (i / 4.0f);
        g.drawHorizontalLine(static_cast<int>(y), area.getX(), area.getRight());
    }

    // Bars
    const float barGap = 1.0f;
    const float totalWidth = area.getWidth();
    const float barWidth = (totalWidth - (63.0f * barGap)) / 64.0f;
    
    for (int i = 0; i < 64; ++i)
    {
        float val = harmonicProfile_[i];
        if (val < 0.001f) continue;
        
        // Clamp for visualization
        val = std::min(val, 1.0f);
        
        float barHeight = area.getHeight() * val;
        auto barX = area.getX() + i * (barWidth + barGap);
        
        auto barArea = juce::Rectangle<float>(
            barX,
            area.getBottom() - barHeight,
            barWidth,
            barHeight
        );
        
        // Gradient from Cyan to Magenta for a neural/vibrant look
        juce::Colour topColor = juce::Colours::cyan.interpolatedWith(juce::Colours::magenta, static_cast<float>(i) / 64.0f);
        
        juce::ColourGradient grad(topColor, barArea.getTopLeft(),
                                 topColor.withAlpha(0.1f), barArea.getBottomLeft(), false);
        g.setGradientFill(grad);
        g.fillRect(barArea);
        
        // Top cap glow
        g.setColour(topColor.withAlpha(0.8f));
        g.fillRect(barArea.withHeight(1.5f));
    }
}

void SpectralVisualizer::resized()
{
}

void SpectralVisualizer::timerCallback()
{
    updateProfile();
    repaint();
}

void SpectralVisualizer::updateProfile()
{
    auto* rollOffPtr = vts_.getRawParameterValue("resonator_rolloff");
    if (!rollOffPtr) return;
    
    float rollOff = rollOffPtr->load();
    
    float firstAmp = 1.0f; // Fundamental is always 1.0 before normalization
    harmonicProfile_[0] = firstAmp;
    
    float total = firstAmp;
    for (int i = 1; i < 64; ++i)
    {
        harmonicProfile_[i] = 1.0f / std::pow(static_cast<float>(i + 1), rollOff);
        total += harmonicProfile_[i];
    }
    
    // Normalization for visual scale
    // We want the fundamental to be near the top but not clipping
    float scale = 0.8f; 
    for (int i = 0; i < 64; ++i)
    {
        harmonicProfile_[i] *= scale;
    }
}

} // namespace Nexus::UI
