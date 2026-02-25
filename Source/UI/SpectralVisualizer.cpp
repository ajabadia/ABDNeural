#include "SpectralVisualizer.h"
#include "ThemeManager.h"
#include "../DSP/IVisualizationSource.h"
#include <cmath>

namespace NEURONiK::UI {

// Constructor takes the visualization source interface
SpectralVisualizer::SpectralVisualizer(NEURONiK::DSP::IVisualizationSource& s)
    : source(s)
{
    harmonicProfile_.fill(0.0f);
    startTimerHz(30); // Refresh rate for the visualizer
}

SpectralVisualizer::~SpectralVisualizer()
{
    stopTimer();
}

void SpectralVisualizer::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced(2.0f);

    // Glass Background
    const auto& theme = ThemeManager::getCurrentTheme();
    g.setColour(theme.background.withAlpha(0.3f));
    g.fillRoundedRectangle(area, 6.0f);

    // Inner Glow/Border
    g.setColour(theme.accent.withAlpha(0.1f));
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    // Grid Lines (Subtle)
    g.setColour(theme.text.withAlpha(0.03f));
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

        // Apply a power curve (gamma) for a more logarithmic-like scale
        val = std::pow(val, 0.5f);
        val = std::min(val, 1.0f);

        float barHeight = area.getHeight() * val;
        auto barX = area.getX() + i * (barWidth + barGap);

        auto barArea = juce::Rectangle<float>(
            barX,
            area.getBottom() - barHeight,
            barWidth,
            barHeight
        );

        // Gradient from Accent to Magenta for a vibrant look
        juce::Colour topColor = theme.accent.interpolatedWith(juce::Colours::magenta, static_cast<float>(i) / 64.0f);

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
    // This gets called at the rate specified in startTimerHz()
    updateProfile();
    repaint(); // This will trigger a call to paint()
}

void SpectralVisualizer::updateProfile()
{
    // Read the real-time spectral data from the source (e.g. Processor)
    source.getSpectralDataForUI(harmonicProfile_.data());
}

} // namespace NEURONiK::UI
