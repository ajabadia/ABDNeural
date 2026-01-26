/*
  ==============================================================================

    XYPad.h
    Created: 23 Jan 2026
    Description: A 2D pad component for controlling two parameters with real-time feedback.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Main/NEURONiKProcessor.h"

namespace NEURONiK::UI
{

class XYPad : public juce::Component, private juce::Timer
{
public:
    XYPad(NEURONiKProcessor& p, juce::AudioProcessorValueTreeState& vts);
    ~XYPad() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    void setModelNames(const std::array<juce::String, 4>& names);

private:
    void timerCallback() override;
    void updateThumbPosition();

    juce::AudioProcessorValueTreeState& vts;
    NEURONiKProcessor& processor;
    std::array<juce::String, 4> modelNames;

    juce::Point<float> thumbPosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYPad)
};

}
