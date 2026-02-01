/*
  ==============================================================================

    XYPad.cpp
    Created: 23 Jan 2026
    Description: Implementation of the 2D pad component.

  ==============================================================================
*/

#include "XYPad.h"
#include "ThemeManager.h"
#include "../DSP/IVisualizationSource.h"
#include "../State/ParameterDefinitions.h"

namespace NEURONiK::UI
{

using namespace NEURONiK::State; // Include the parameter IDs namespace

XYPad::XYPad(NEURONiK::DSP::IVisualizationSource& s, juce::AudioProcessorValueTreeState& v)
    : vts(v), source(s)
{
    for (auto& n : modelNames) n = "EMPTY";
    updateThumbPosition(); // This will be removed or changed later, but for now it's still called.
    startTimerHz(30);
}

XYPad::~XYPad()
{
    stopTimer();
}

void XYPad::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    const auto& theme = ThemeManager::getCurrentTheme();
    g.setColour(theme.background.withAlpha(0.5f));
    g.fillRoundedRectangle(bounds, 8.0f);

    // Grid lines
    g.setColour(theme.text.withAlpha(0.1f));
    g.drawHorizontalLine((int)bounds.getCentreY(), bounds.getX(), bounds.getRight());
    g.drawVerticalLine((int)bounds.getCentreX(), bounds.getY(), bounds.getBottom());

    // Border
    g.setColour(theme.text.withAlpha(0.2f));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    // Thumb (Modulated Position)
    g.setColour(theme.accent);
    g.fillEllipse(thumbPosition.x - 8, thumbPosition.y - 8, 16, 16);
    
    // Ghost Ring (Base Parameter Position) - Optional cool effect
    float baseX = vts.getRawParameterValue(IDs::morphX)->load() * bounds.getWidth();
    float baseY = (1.0f - vts.getRawParameterValue(IDs::morphY)->load()) * bounds.getHeight();
    if (std::hypot(baseX - thumbPosition.x, baseY - thumbPosition.y) > 2.0f)
    {
        g.setColour(theme.text.withAlpha(0.3f));
        g.drawEllipse(baseX - 6, baseY - 6, 12, 12, 1.0f);
        g.drawLine(baseX, baseY, thumbPosition.x, thumbPosition.y, 1.0f);
    }
    
    g.setColour(theme.background.withAlpha(0.5f));
    g.drawEllipse(thumbPosition.x - 8, thumbPosition.y - 8, 16, 16, 1.0f);

    // Labels
    g.setColour(theme.text.withAlpha(0.5f));
    g.setFont(10.0f);
    float textPad = 30.0f; // Shift text inward to accommodate buttons
    g.drawText(modelNames[0], static_cast<int>(textPad), 5, 100, 15, juce::Justification::topLeft);
    g.drawText(modelNames[1], static_cast<int>(bounds.getRight() - 100 - textPad), 5, 100, 15, juce::Justification::topRight);
    g.drawText(modelNames[2], static_cast<int>(textPad), static_cast<int>(bounds.getBottom() - 20), 100, 15, juce::Justification::bottomLeft);
    g.drawText(modelNames[3], static_cast<int>(bounds.getRight() - 100 - textPad), static_cast<int>(bounds.getBottom() - 20), 100, 15, juce::Justification::bottomRight);
}

void XYPad::resized()
{
    updateThumbPosition();
}

void XYPad::mouseDown(const juce::MouseEvent& event)
{
    if (auto* xParam = vts.getParameter(IDs::morphX)) xParam->beginChangeGesture();
    if (auto* yParam = vts.getParameter(IDs::morphY)) yParam->beginChangeGesture();

    mouseDrag(event);
}

void XYPad::mouseDrag(const juce::MouseEvent& event)
{
    auto area = getLocalBounds().toFloat();
    float x = juce::jlimit(0.0f, 1.0f, (float)event.x / area.getWidth());
    float y = juce::jlimit(0.0f, 1.0f, 1.0f - ((float)event.y / area.getHeight()));

    if (auto* xParam = vts.getParameter(IDs::morphX))
        xParam->setValueNotifyingHost(x);

    if (auto* yParam = vts.getParameter(IDs::morphY))
        yParam->setValueNotifyingHost(y);

    // Update thumb position immediately for smooth feedback
    thumbPosition.setXY(x * area.getWidth(), (1.0f - y) * area.getHeight());
    repaint();
}

void XYPad::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    if (auto* xParam = vts.getParameter(IDs::morphX)) xParam->endChangeGesture();
    if (auto* yParam = vts.getParameter(IDs::morphY)) yParam->endChangeGesture();
}

void XYPad::timerCallback()
{
    float mx = 0.5f, my = 0.5f;
    source.getMorphCoordinatesForUI(mx, my);
    
    // Only update from source if we're not currently dragging to avoid jitter
    if (!juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
    {
        auto area = getLocalBounds().toFloat();
        float newX = mx * area.getWidth();
        float newY = (1.0f - my) * area.getHeight();

        if (std::hypot(newX - thumbPosition.x, newY - thumbPosition.y) > 0.5f)
        {
            thumbPosition.setXY(newX, newY);
            repaint();
        }
    }
}

void XYPad::updateThumbPosition()
{
    float mx = 0.5f, my = 0.5f;
    source.getMorphCoordinatesForUI(mx, my);
    auto area = getLocalBounds().toFloat();
    thumbPosition.setXY(mx * area.getWidth(), (1.0f - my) * area.getHeight());
}

void XYPad::setModelNames(const std::array<juce::String, 4>& names)
{
    modelNames = names;
    repaint();
}

} // namespace NEURONiK::UI
