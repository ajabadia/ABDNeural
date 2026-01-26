#include "ParameterPanel.h"
#include "../State/ParameterDefinitions.h"

namespace NEURONiK::UI {

using namespace NEURONiK::State;

ParameterPanel::ParameterPanel(NEURONiKProcessor& p)
    : processor(p), vts(p.getAPVTS()), presetPanel(p)
{
    addAndMakeVisible(presetPanel);

    setupControl(attack,    IDs::envAttack,   "ATTACK");
    setupControl(decay,     IDs::envDecay,    "DECAY");
    setupControl(sustain,   IDs::envSustain,  "SUSTAIN");
    setupControl(release,   IDs::envRelease,  "RELEASE");
    setupControl(cutoff,    IDs::filterCutoff, "BRILLANCE");
    setupControl(resonance, IDs::masterLevel,  "VOLUME");

    addAndMakeVisible(randomizeButton);
    randomizeButton.onClick = [this] { randomizeParameters(); };

    titleLabel.setFont(juce::Font(juce::FontOptions(28.0f).withStyle("Bold")));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setFont(juce::Font(juce::FontOptions(14.0f).withStyle("Italic")));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    subtitleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(subtitleLabel);
}

void ParameterPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText)
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

void ParameterPanel::randomizeParameters()
{
    auto& random = juce::Random::getSystemRandom();

    auto randomizeParam = [&](const juce::String& id, float minVal, float maxVal) {
        if (auto* param = vts.getParameter(id))
        {
            float range = maxVal - minVal;
            float randomVal = minVal + random.nextFloat() * range;
            param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1(randomVal));
        }
    };

    randomizeParam(IDs::morphX, 0.0f, 1.0f);
    randomizeParam(IDs::morphY, 0.0f, 1.0f);
    randomizeParam(IDs::oscInharmonicity, 0.0f, 0.5f);
    randomizeParam(IDs::oscRoughness, 0.0f, 0.4f);
    randomizeParam(IDs::filterCutoff, 200.0f, 8000.0f);
    randomizeParam(IDs::filterRes, 0.0f, 0.6f);
    randomizeParam(IDs::envAttack, 0.001f, 0.5f);
    randomizeParam(IDs::envDecay, 0.1f, 1.0f);
    randomizeParam(IDs::envSustain, 0.2f, 0.8f);
    randomizeParam(IDs::envRelease, 0.1f, 2.0f);
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

    presetPanel.setBounds(area.removeFromTop(40));

    auto titleArea = area.removeFromBottom(40);
    titleLabel.setBounds(titleArea.removeFromLeft(200));
    subtitleLabel.setBounds(titleArea);

    auto controlsArea = area.reduced(10, 0);

    const int numRows = 2;
    const int numCols = 4;
    auto rowHeight = controlsArea.getHeight() / numRows;
    auto colWidth = controlsArea.getWidth() / numCols;

    auto layoutControl = [&](RotaryControl& ctrl, int row, int col) {
        auto bounds = controlsArea.withY(controlsArea.getY() + row * rowHeight)
                                .withX(controlsArea.getX() + col * colWidth)
                                .withHeight(rowHeight)
                                .withWidth(colWidth);

        ctrl.label.setBounds(bounds.removeFromTop(15));
        ctrl.slider.setBounds(bounds.reduced(5));
    };

    layoutControl(attack, 0, 0);
    layoutControl(decay, 0, 1);
    layoutControl(sustain, 0, 2);
    layoutControl(release, 0, 3);

    layoutControl(cutoff, 1, 0);
    layoutControl(resonance, 1, 1);

    randomizeButton.setBounds(controlsArea.withY(controlsArea.getY() + rowHeight)
                                        .withX(controlsArea.getX() + 3 * colWidth)
                                        .withHeight(rowHeight)
                                        .withWidth(colWidth).reduced(15));
}

} // namespace NEURONiK::UI
