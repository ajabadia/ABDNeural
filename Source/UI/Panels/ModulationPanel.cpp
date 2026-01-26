/*
  ==============================================================================

    ModulationPanel.cpp
    Created: 28 Jan 2026
    Description: Implementation of the UI component for LFOs and the modulation matrix.

  ==============================================================================
*/

#include "ModulationPanel.h"
#include "../../Main/NEURONiKProcessor.h"
#include "../../State/ParameterDefinitions.h"

namespace NEURONiK::UI {

using namespace NEURONiK::State;

ModulationPanel::ModulationPanel(NEURONiKProcessor& p)
    : processor(p), vts(p.getAPVTS())
{
    for (int i = 0; i < 2; ++i)
        setupLfoControls(i);
    for (int i = 0; i < 4; ++i)
        setupModSlotControls(i);

    addAndMakeVisible(sourceTitle);
    addAndMakeVisible(destTitle);
    addAndMakeVisible(amountTitle);
    
    juce::Font titleFont(juce::FontOptions(12.0f).withStyle("Bold"));
    sourceTitle.setFont(titleFont);
    destTitle.setFont(titleFont);
    amountTitle.setFont(titleFont);
    sourceTitle.setColour(juce::Label::textColourId, juce::Colours::cyan.withAlpha(0.6f));
    destTitle.setColour(juce::Label::textColourId, juce::Colours::cyan.withAlpha(0.6f));
    amountTitle.setColour(juce::Label::textColourId, juce::Colours::cyan.withAlpha(0.6f));
}

void ModulationPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::transparentBlack);
}

void ModulationPanel::resized()
{
    auto area = getLocalBounds().reduced(10);
    
    // Header for LFOs
    auto lfoArea = area.removeFromTop(static_cast<int>(area.getHeight() * 0.45f));
    auto lfo1Area = lfoArea.removeFromLeft(lfoArea.getWidth() / 2).reduced(5);
    auto lfo2Area = lfoArea.reduced(5);
 
    auto layoutRotary = [&](ControlElement& ctrl, juce::Rectangle<int> bounds) {
        ctrl.label.setBounds(bounds.removeFromTop(15));
        ctrl.slider.setBounds(bounds);
    };
 
    auto layoutLfo = [&](LfoUiElements& lfo, juce::Rectangle<int> bounds) {
        auto comboHeight = 24;
        auto comboArea = bounds.removeFromTop(comboHeight);
        lfo.waveform.setBounds(comboArea.removeFromLeft(comboArea.getWidth() / 3).reduced(2, 0));
        lfo.syncMode.setBounds(comboArea.removeFromLeft(comboArea.getWidth() / 2).reduced(2, 0));
        lfo.division.setBounds(comboArea.reduced(2, 0));
 
        bounds.removeFromTop(5);
        auto knobWidth = bounds.getWidth() / 2;
        layoutRotary(lfo.rate, bounds.removeFromLeft(knobWidth));
        layoutRotary(lfo.depth, bounds.removeFromLeft(knobWidth));
    };
    
    layoutLfo(lfos[0], lfo1Area);
    layoutLfo(lfos[1], lfo2Area);
 
    // Header for Mod Matrix
    area.removeFromTop(15);
    auto modArea = area;
    
    // Column titles
    auto titleArea = modArea.removeFromTop(20);
    sourceTitle.setBounds(titleArea.removeFromLeft(140).reduced(2));
    destTitle.setBounds(titleArea.removeFromLeft(140).reduced(2));
    amountTitle.setBounds(titleArea.reduced(2));
    
    for (int i = 0; i < 4; ++i)
    {
        auto slotArea = modArea.removeFromTop(modArea.getHeight() / (4 - i)).reduced(0, 2);
        modSlots[i].source.setBounds(slotArea.removeFromLeft(140).reduced(2));
        modSlots[i].destination.setBounds(slotArea.removeFromLeft(140).reduced(2));
        modSlots[i].amount.slider.setBounds(slotArea.reduced(5, 2));
    }
}

void ModulationPanel::setupRotaryControl(ControlElement& ctrl, const juce::String& paramID, const juce::String& labelText)
{
    ctrl.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 50, 18);
    addAndMakeVisible(ctrl.slider);
    ctrl.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(vts, paramID, ctrl.slider);
 
    ctrl.label.setText(labelText, juce::dontSendNotification);
    ctrl.label.setJustificationType(juce::Justification::centred);
    ctrl.label.setFont(juce::Font(juce::FontOptions(11.0f)));
    addAndMakeVisible(ctrl.label);
 
    ctrl.midiLearner = std::make_unique<MidiLearner>(processor, ctrl.slider, paramID);
}

void ModulationPanel::setupAmountControl(ControlElement& ctrl, const juce::String& paramID)
{
    ctrl.slider.setSliderStyle(juce::Slider::LinearHorizontal);
    ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxRight, true, 45, 18);
    ctrl.slider.setColour(juce::Slider::thumbColourId, juce::Colours::orange);
    ctrl.slider.setColour(juce::Slider::trackColourId, juce::Colours::orange.withAlpha(0.3f));
    addAndMakeVisible(ctrl.slider);
    
    ctrl.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(vts, paramID, ctrl.slider);
    ctrl.midiLearner = std::make_unique<MidiLearner>(processor, ctrl.slider, paramID);
}

void ModulationPanel::setupLfoControls(int lfoIndex)
{
    auto& lfo = lfos[lfoIndex];
    auto lfoId = juce::String(lfoIndex + 1);

    lfo.waveform.addItemList({ "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random S&H" }, 1);
    lfo.syncMode.addItemList({ "Free", "Tempo Sync" }, 1);
    lfo.division.addItemList({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4t", "1/8t", "1/16t" }, 1);

    addAndMakeVisible(lfo.waveform);
    addAndMakeVisible(lfo.syncMode);
    addAndMakeVisible(lfo.division);

    lfo.waveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(vts, IDs::lfo1Waveform.replace("1", lfoId), lfo.waveform);
    lfo.syncModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(vts, IDs::lfo1SyncMode.replace("1", lfoId), lfo.syncMode);
    lfo.divisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(vts, IDs::lfo1RhythmicDivision.replace("1", lfoId), lfo.division);

    setupRotaryControl(lfo.rate, IDs::lfo1RateHz.replace("1", lfoId), "Rate");
    setupRotaryControl(lfo.depth, IDs::lfo1Depth.replace("1", lfoId), "Depth");
}

void ModulationPanel::setupModSlotControls(int slotIndex)
{
    auto& slot = modSlots[slotIndex];
    auto slotId = juce::String(slotIndex + 1);

    slot.source.addItemList({ "Off", "LFO 1", "LFO 2", "Pitch Bend", "Mod Wheel" }, 1);
    slot.destination.addItemList({ "Off", "Osc Level", "Inharmonicity", "Roughness", "Morph X", "Morph Y", "Attack", "Decay", "Sustain", "Release", "Filter Cutoff", "Filter Res", "Saturation", "Delay Time", "Delay FB" }, 1);

    addAndMakeVisible(slot.source);
    addAndMakeVisible(slot.destination);

    slot.sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(vts, IDs::mod1Source.replace("1", slotId), slot.source);
    slot.destinationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(vts, IDs::mod1Destination.replace("1", slotId), slot.destination);
 
    setupAmountControl(slot.amount, IDs::mod1Amount.replace("1", slotId));
}

} // namespace NEURONiK::UI
