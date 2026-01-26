/*
  ==============================================================================

    XYPad.cpp
    Created: 23 Jan 2026
    Description: Implementation of the 2D pad component.

  ==============================================================================
*/

#include "XYPad.h"
#include "../State/ParameterDefinitions.h"

namespace NEURONiK::UI
{

using namespace NEURONiK::State; // Include the parameter IDs namespace

XYPad::XYPad(juce::AudioProcessorValueTreeState& vtsIn)
    : vts(vtsIn)
{
    // We need to fetch the parameters manually, not using attachments for this custom component
    updateThumbPosition();
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
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(bounds, 8.0f);

    // Grid lines
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawHorizontalLine((int)bounds.getCentreY(), bounds.getX(), bounds.getRight());
    g.drawVerticalLine((int)bounds.getCentreX(), bounds.getY(), bounds.getBottom());

    // Border
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    // Thumb
    g.setColour(juce::Colours::cyan);
    g.fillEllipse(thumbPosition.x - 8, thumbPosition.y - 8, 16, 16);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawEllipse(thumbPosition.x - 8, thumbPosition.y - 8, 16, 16, 1.0f);
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
    auto bounds = getLocalBounds();
    auto x = (float)event.getPosition().x / bounds.getWidth();
    auto y = 1.0f - ((float)event.getPosition().y / bounds.getHeight());

    if (auto* xParam = vts.getParameter(IDs::morphX))
        xParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, x));

    if (auto* yParam = vts.getParameter(IDs::morphY))
        yParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, y));
}

void XYPad::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event); // FIX: Silence the 'unused parameter' warning
    if (auto* xParam = vts.getParameter(IDs::morphX)) xParam->endChangeGesture();
    if (auto* yParam = vts.getParameter(IDs::morphY)) yParam->endChangeGesture();
}

void XYPad::timerCallback()
{
    updateThumbPosition();
}

void XYPad::updateThumbPosition()
{
    auto bounds = getLocalBounds();
    auto newX = vts.getRawParameterValue(IDs::morphX)->load() * bounds.getWidth();
    auto newY = (1.0f - vts.getRawParameterValue(IDs::morphY)->load()) * bounds.getHeight();

    juce::Point<float> newThumbPosition(newX, newY);

    if (thumbPosition.getDistanceFrom(newThumbPosition) > 0.5f)
    {
        thumbPosition = newThumbPosition;
        repaint();
    }
}

} // namespace NEURONiK::UI
