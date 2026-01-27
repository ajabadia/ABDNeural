#include "ModulationPanel.h"
#include "../../Main/NEURONiKProcessor.h"
#include "../../State/ParameterDefinitions.h"

namespace NEURONiK::UI {

using namespace NEURONiK::State;

ModulationPanel::ModulationPanel(NEURONiKProcessor& p)
    : processor(p), vts(p.getAPVTS())
{
    addAndMakeVisible(lfo1Box);
    addAndMakeVisible(lfo2Box);
    addAndMakeVisible(matrixBox);

    for (int i = 0; i < 2; ++i)
        setupLfoControls(i);
    for (int i = 0; i < 4; ++i)
        setupModSlotControls(i);

    matrixBox.addAndMakeVisible(sourceTitle);
    matrixBox.addAndMakeVisible(destTitle);
    matrixBox.addAndMakeVisible(amountTitle);
    
    juce::Font titleFont(juce::FontOptions(11.0f).withStyle("Bold"));
    sourceTitle.setFont(titleFont);
    destTitle.setFont(titleFont);
    amountTitle.setFont(titleFont);
    sourceTitle.setColour(juce::Label::textColourId, juce::Colours::cyan.withAlpha(0.6f));
    destTitle.setColour(juce::Label::textColourId, juce::Colours::cyan.withAlpha(0.6f));
    amountTitle.setColour(juce::Label::textColourId, juce::Colours::cyan.withAlpha(0.6f));
}

void ModulationPanel::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
    // Background handled by glass boxes
}

void ModulationPanel::resized()
{
    auto area = getLocalBounds().reduced(5);
    
    // Header for LFOs
    auto lfoArea = area.removeFromTop(static_cast<int>(area.getHeight() * 0.45f));
    auto lfo1Bounds = lfoArea.removeFromLeft(lfoArea.getWidth() / 2).reduced(3);
    auto lfo2Bounds = lfoArea.reduced(3);

    lfo1Box.setBounds(lfo1Bounds);
    lfo2Box.setBounds(lfo2Bounds);

    auto layoutLfo = [&](LfoUiElements& lfo, juce::Rectangle<int> bounds) {
        auto comboHeight = 24;
        auto comboArea = bounds.removeFromTop(comboHeight);
        lfo.waveform.setBounds(comboArea.removeFromLeft(comboArea.getWidth() / 3).reduced(2, 0));
        lfo.syncMode.setBounds(comboArea.removeFromLeft(comboArea.getWidth() / 2).reduced(2, 0));
        lfo.division.setBounds(comboArea.reduced(2, 0));
 
        bounds.removeFromTop(5);
        auto knobWidth = bounds.getWidth() / 2;
        
        auto lArea = bounds.removeFromLeft(knobWidth);
        lfo.rate.label.setBounds(lArea.removeFromTop(15));
        lfo.rate.slider.setBounds(lArea);
        
        auto rArea = bounds;
        lfo.depth.label.setBounds(rArea.removeFromTop(15));
        lfo.depth.slider.setBounds(rArea);
    };
    
    layoutLfo(lfos[0], lfo1Box.getContentArea());
    layoutLfo(lfos[1], lfo2Box.getContentArea());
 
    // Header for Mod Matrix
    area.removeFromTop(5);
    matrixBox.setBounds(area.reduced(3));
    auto modArea = matrixBox.getContentArea();
    
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

void ModulationPanel::setupAmountControl(RotaryControl& ctrl, const juce::String& paramID)
{
    ctrl.slider.setSliderStyle(juce::Slider::LinearHorizontal);
    ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxRight, true, 45, 18);
    ctrl.slider.setColour(juce::Slider::thumbColourId, juce::Colours::orange);
    ctrl.slider.setColour(juce::Slider::trackColourId, juce::Colours::orange.withAlpha(0.3f));
    matrixBox.addAndMakeVisible(ctrl.slider);
    
    ctrl.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(vts, paramID, ctrl.slider);
    ctrl.midiLearner = std::make_unique<MidiLearner>(processor, ctrl.slider, paramID);
}

void ModulationPanel::setupLfoControls(int lfoIndex)
{
    auto& lfo = lfos[lfoIndex];
    auto lfoId = juce::String(lfoIndex + 1);
    auto& box = (lfoIndex == 0) ? lfo1Box : lfo2Box;

    lfo.waveform.addItemList({ "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random S&H" }, 1);
    lfo.syncMode.addItemList({ "Free", "Tempo Sync" }, 1);
    lfo.division.addItemList({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4t", "1/8t", "1/16t" }, 1);

    box.addAndMakeVisible(lfo.waveform);
    box.addAndMakeVisible(lfo.syncMode);
    box.addAndMakeVisible(lfo.division);

    lfo.waveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(vts, juce::String(IDs::lfo1Waveform).replace("1", lfoId), lfo.waveform);
    lfo.syncModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(vts, juce::String(IDs::lfo1SyncMode).replace("1", lfoId), lfo.syncMode);
    lfo.divisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(vts, juce::String(IDs::lfo1RhythmicDivision).replace("1", lfoId), lfo.division);

    UIUtils::setupRotaryControl(box, lfo.rate, juce::String(IDs::lfo1RateHz).replace("1", lfoId), "Speed", vts, processor, sharedLNF);
    UIUtils::setupRotaryControl(box, lfo.depth, juce::String(IDs::lfo1Depth).replace("1", lfoId), "Depth", vts, processor, sharedLNF);
}

void ModulationPanel::setupModSlotControls(int slotIndex)
{
    auto& slot = modSlots[slotIndex];
    auto slotId = juce::String(slotIndex + 1);

    slot.source.addItemList({ "Off", "LFO 1", "LFO 2", "Pitch Bend", "Mod Wheel" }, 1);
    slot.destination.addItemList({ 
        "Off", "Osc Level", "Inharmonicity", "Roughness", "Morph X", "Morph Y", 
        "Amp Attack", "Amp Decay", "Amp Sustain", "Amp Release", 
        "Filter Cutoff", "Filter Res", "Filter Env Amt",
        "Flt Attack", "Flt Decay", "Flt Sustain", "Flt Release",
        "Saturation", "Delay Time", "Delay FB" }, 1);

    matrixBox.addAndMakeVisible(slot.source);
    matrixBox.addAndMakeVisible(slot.destination);

    slot.sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(vts, juce::String(IDs::mod1Source).replace("1", slotId), slot.source);
    slot.destinationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(vts, juce::String(IDs::mod1Destination).replace("1", slotId), slot.destination);
  
    setupAmountControl(slot.amount, juce::String(IDs::mod1Amount).replace("1", slotId));
}

} // namespace NEURONiK::UI
