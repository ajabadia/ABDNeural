/*
  ==============================================================================

    MidiLearner.cpp
    Created: 30 Jan 2026
    Description: Implementation of the MidiLearner component.

  ==============================================================================
*/

#include "MidiLearner.h"

namespace NEURONiK::UI
{

MidiLearner::MidiLearner(NEURONiKProcessor& p, juce::Component& targetComponent, const juce::String& paramID)
    : processor(p), target(targetComponent), parameterId(paramID)
{
    target.addMouseListener(this, true);
}

MidiLearner::~MidiLearner()
{
    target.removeMouseListener(this);
}

void MidiLearner::mouseUp(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        juce::PopupMenu menu;
        menu.addItem("MIDI Learn", [this]() {
            processor.enterMidiLearnMode(parameterId);
        });
        menu.addItem("Clear MIDI Learn", [this]() {
            processor.clearMidiLearnForParameter(parameterId);
        });
        menu.showMenuAsync(juce::PopupMenu::Options());
    }
}

} // namespace NEURONiK::UI
