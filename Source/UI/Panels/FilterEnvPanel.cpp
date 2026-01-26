#include "FilterEnvPanel.h"
#include "../../Main/NEURONiKProcessor.h"
#include "../../State/ParameterDefinitions.h"

namespace NEURONiK::UI {

FilterEnvPanel::FilterEnvPanel(NEURONiKProcessor& p)
    : processor(p), vts(p.getAPVTS())
{
    using namespace NEURONiK::State;

    setupControl(attack,    IDs::envAttack,    "ATTACK");
    setupControl(decay,     IDs::envDecay,     "DECAY");
    setupControl(sustain,   IDs::envSustain,   "SUSTAIN");
    setupControl(release,   IDs::envRelease,   "RELEASE");
    
    setupControl(cutoff,    IDs::filterCutoff, "CUTOFF");
    setupControl(resonance, IDs::filterRes,    "RESONANCE");
}

void FilterEnvPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText)
{
    ctrl.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);
    ctrl.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(ctrl.slider);
    
    ctrl.label.setText(labelText, juce::dontSendNotification);
    ctrl.label.setJustificationType(juce::Justification::centred);
    ctrl.label.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    addAndMakeVisible(ctrl.label);
    
    ctrl.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(vts, paramID, ctrl.slider);
    ctrl.midiLearner = std::make_unique<MidiLearner>(processor, ctrl.slider, paramID);
}

void FilterEnvPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.05f), 0, 0,
                                  juce::Colours::white.withAlpha(0.01f), 0, area.getHeight(), false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(area, 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawRoundedRectangle(area, 10.0f, 1.0f);
}

void FilterEnvPanel::resized()
{
    auto floatArea = getLocalBounds().reduced(20).toFloat();
    auto rowHeight = floatArea.getHeight() / 2.0f;
    
    const int labelHeight = 15;
    const int textBoxHeight = 20;
    
    auto layoutControl = [&](RotaryControl& ctrl, juce::Rectangle<float> bounds) {
        auto intBounds = bounds.toNearestInt();
        ctrl.label.setBounds(intBounds.removeFromTop(labelHeight));
        ctrl.slider.setBounds(intBounds.removeFromTop(intBounds.getHeight() - textBoxHeight));
    };

    auto filterArea = floatArea.removeFromTop(rowHeight);
    auto filterWidth = filterArea.getWidth() / 2.0f;
    layoutControl(cutoff, filterArea.removeFromLeft(filterWidth).reduced(40.0f, 10.0f));
    layoutControl(resonance, filterArea.reduced(40.0f, 10.0f));
    
    auto envArea = floatArea;
    auto envWidth = envArea.getWidth() / 4.0f;
    layoutControl(attack, envArea.removeFromLeft(envWidth).reduced(10.0f, 5.0f));
    layoutControl(decay, envArea.removeFromLeft(envWidth).reduced(10.0f, 5.0f));
    layoutControl(sustain, envArea.removeFromLeft(envWidth).reduced(10.0f, 5.0f));
    layoutControl(release, envArea.reduced(10.0f, 5.0f));
}

} // namespace NEURONiK::UI
