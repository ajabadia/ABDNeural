#include "ParameterPanel.h"
#include "../State/ParameterDefinitions.h"

namespace Nexus::UI {

ParameterPanel::ParameterPanel(juce::AudioProcessorValueTreeState& vtsIn)
    : vts(vtsIn)
{
    using namespace Nexus::State;

    setupControl(rollOff,   IDs::resonatorRolloff, "HARMONIC TENSION");
    setupControl(cutoff,    IDs::filterCutoff,     "BRILLANCE");
    setupControl(resonance, IDs::masterLevel,      "VOLUME");
    
    setupControl(attack,    IDs::envAttack,   "ATTACK");
    setupControl(decay,     IDs::envDecay,    "DECAY");
    setupControl(sustain,   IDs::envSustain,  "SUSTAIN");
    setupControl(release,   IDs::envRelease,  "RELEASE");
}

void ParameterPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText)
{
    ctrl.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);
    ctrl.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(ctrl.slider);
    
    ctrl.label.setText(labelText, juce::dontSendNotification);
    ctrl.label.setJustificationType(juce::Justification::centred);
    ctrl.label.setFont(juce::Font(12.0f).boldened());
    addAndMakeVisible(ctrl.label);
    
    ctrl.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(vts, paramID, ctrl.slider);
}

void ParameterPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    
    juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.08f), 0, 0,
                                  juce::Colours::white.withAlpha(0.02f), 0, area.getHeight(), false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(area, 10.0f);
    
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(area, 10.0f, 1.2f);
    
    g.setColour(juce::Colours::cyan.withAlpha(0.03f));
    g.fillRoundedRectangle(area.reduced(1.0f), 10.0f);
}

void ParameterPanel::resized()
{
    auto area = getLocalBounds().reduced(15);
    auto floatArea = area.toFloat();

    auto topRow = floatArea.removeFromTop(floatArea.getHeight() / 2.0f);
    auto bottomRow = floatArea;
    
    const int labelHeight = 15;
    const int textBoxHeight = 20;

    auto layoutControl = [&](RotaryControl& ctrl, juce::Rectangle<float> bounds) {
        auto intBounds = bounds.toNearestInt();
        ctrl.label.setBounds(intBounds.removeFromTop(labelHeight));
        ctrl.slider.setBounds(intBounds.removeFromTop(intBounds.getHeight() - textBoxHeight));
    };

    auto envArea = topRow.removeFromLeft(topRow.getWidth() * 0.65f);
    auto envWidth = envArea.getWidth() / 4.0f;
    
    layoutControl(attack, envArea.removeFromLeft(envWidth).reduced(5.0f));
    layoutControl(decay, envArea.removeFromLeft(envWidth).reduced(5.0f));
    layoutControl(sustain, envArea.removeFromLeft(envWidth).reduced(5.0f));
    layoutControl(release, envArea.reduced(5.0f));
    
    auto filterWidth = topRow.getWidth() / 2.0f;
    layoutControl(cutoff, topRow.removeFromLeft(filterWidth).reduced(5.0f));
    layoutControl(resonance, topRow.reduced(5.0f));
    
    auto resArea = bottomRow.removeFromLeft(bottomRow.getWidth() * 0.22f);
    layoutControl(rollOff, resArea.reduced(5.0f));
}

} // namespace Nexus::UI
