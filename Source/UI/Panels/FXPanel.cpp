#include "FXPanel.h"
#include "../../Main/NEURONiKProcessor.h"
#include "../../State/ParameterDefinitions.h"

namespace NEURONiK::UI {

FXPanel::FXPanel(NEURONiKProcessor& p)
    : processor(p), vts(p.getAPVTS())
{
    using namespace NEURONiK::State;

    setupControl(saturation,    IDs::fxSaturation,     "SATURATION");
    setupControl(delayTime,     IDs::fxDelayTime,     "DELAY TIME");
    setupControl(delayFeedback, IDs::fxDelayFeedback, "FEEDBACK");
    setupControl(masterLevel,   IDs::masterLevel,      "MASTER LEVEL");
}

void FXPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText)
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

void FXPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.05f), 0, 0,
                                  juce::Colours::white.withAlpha(0.01f), 0, area.getHeight(), false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(area, 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawRoundedRectangle(area, 10.0f, 1.0f);
}

void FXPanel::resized()
{
    auto floatArea = getLocalBounds().reduced(20).toFloat();
    auto ctrlWidth = floatArea.getWidth() / 4.0f;
    
    const int labelHeight = 15;
    const int textBoxHeight = 20;
    
    auto layoutControl = [&](RotaryControl& ctrl, juce::Rectangle<float> bounds) {
        auto intBounds = bounds.toNearestInt();
        ctrl.label.setBounds(intBounds.removeFromTop(labelHeight));
        ctrl.slider.setBounds(intBounds.removeFromTop(intBounds.getHeight() - textBoxHeight));
    };

    layoutControl(saturation,    floatArea.removeFromLeft(ctrlWidth).reduced(15.0f, 40.0f));
    layoutControl(delayTime,     floatArea.removeFromLeft(ctrlWidth).reduced(15.0f, 40.0f));
    layoutControl(delayFeedback, floatArea.removeFromLeft(ctrlWidth).reduced(15.0f, 40.0f));
    layoutControl(masterLevel,   floatArea.reduced(15.0f, 40.0f));
}

} // namespace NEURONiK::UI
