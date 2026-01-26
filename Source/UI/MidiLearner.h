/*
  ==============================================================================

    MidiLearner.h
    Created: 30 Jan 2026
    Description: A component that adds MIDI learn functionality to any other component.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Main/NEURONiKProcessor.h"

namespace NEURONiK::UI
{

class MidiLearner : public juce::Component
{
public:
    MidiLearner(NEURONiKProcessor& p, juce::Component& targetComponent, const juce::String& paramID);
    ~MidiLearner() override;

    void mouseUp(const juce::MouseEvent& event) override;

private:
    NEURONiKProcessor& processor;
    juce::Component& target;
    juce::String parameterId;
};

} // namespace NEURONiK::UI
