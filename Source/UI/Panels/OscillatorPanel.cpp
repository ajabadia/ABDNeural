#include "OscillatorPanel.h"
#include "../../Main/NEURONiKProcessor.h"
#include "../../State/ParameterDefinitions.h"

namespace NEURONiK::UI {

OscillatorPanel::OscillatorPanel(NEURONiKProcessor& p)
    : processor(p),
      xyPad(p.getAPVTS()),
      loadA("A"), loadB("B"), loadC("C"), loadD("D")
{
    using namespace NEURONiK::State;
    addAndMakeVisible(xyPad);

    setupControl(inharmonicity, IDs::oscInharmonicity, "INHARMONICITY");
    setupControl(roughness,    IDs::oscRoughness,    "ROUGHNESS");

    addAndMakeVisible(loadA);
    loadA.addListener(this);
    addAndMakeVisible(loadB);
    loadB.addListener(this);
    addAndMakeVisible(loadC);
    loadC.addListener(this);
    addAndMakeVisible(loadD);
    loadD.addListener(this);

    for (auto& name : modelNames)
        name = "EMPTY";
}

void OscillatorPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText)
{
    auto& apvts = processor.getAPVTS();
    ctrl.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);
    ctrl.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(ctrl.slider);

    ctrl.label.setText(labelText, juce::dontSendNotification);
    ctrl.label.setJustificationType(juce::Justification::centred);
    ctrl.label.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    addAndMakeVisible(ctrl.label);

    ctrl.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, ctrl.slider);
    ctrl.midiLearner = std::make_unique<MidiLearner>(processor, ctrl.slider, paramID);
}

void OscillatorPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    
    juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.05f), 0, 0,
                                  juce::Colours::white.withAlpha(0.01f), 0, area.getHeight(), false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(area, 10.0f);
    
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawRoundedRectangle(area, 10.0f, 1.0f);

    g.setColour(juce::Colours::lightgrey);
    g.setFont(12.0f);
    auto padBounds = xyPad.getBounds();
    g.drawText(modelNames[0], padBounds.getX(), padBounds.getY(), 100, 20, juce::Justification::topLeft, true);
    g.drawText(modelNames[1], padBounds.getRight() - 100, padBounds.getY(), 100, 20, juce::Justification::topRight, true);
    g.drawText(modelNames[2], padBounds.getX(), padBounds.getBottom() - 20, 100, 20, juce::Justification::bottomLeft, true);
    g.drawText(modelNames[3], padBounds.getRight() - 100, padBounds.getBottom() - 20, 100, 20, juce::Justification::bottomRight, true);
}

void OscillatorPanel::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto padArea = area.removeFromLeft(static_cast<int>(area.getWidth() * 0.7f));
    xyPad.setBounds(padArea);

    int buttonSize = 25;
    loadA.setBounds(padArea.getX(), padArea.getY(), buttonSize, buttonSize);
    loadB.setBounds(padArea.getRight() - buttonSize, padArea.getY(), buttonSize, buttonSize);
    loadC.setBounds(padArea.getX(), padArea.getBottom() - buttonSize, buttonSize, buttonSize);
    loadD.setBounds(padArea.getRight() - buttonSize, padArea.getBottom() - buttonSize, buttonSize, buttonSize);

    auto controlsArea = area.reduced(10, 0);
    auto knobHeight = controlsArea.getHeight() / 2;

    auto layoutControl = [&](RotaryControl& ctrl, juce::Rectangle<int> bounds) {
        ctrl.label.setBounds(bounds.removeFromTop(15));
        ctrl.slider.setBounds(bounds.reduced(0, 10));
    };

    layoutControl(inharmonicity, controlsArea.removeFromTop(knobHeight));
    layoutControl(roughness, controlsArea);
}

void OscillatorPanel::buttonClicked(juce::Button* button)
{
    int slot = -1;
    if (button == &loadA) slot = 0;
    else if (button == &loadB) slot = 1;
    else if (button == &loadC) slot = 2;
    else if (button == &loadD) slot = 3;

    if (slot != -1)
    {
        fileChooser = std::make_unique<juce::FileChooser>("Load a .neuronikmodel file...",
                                                            juce::File(),
                                                            "*.neuronikmodel");

        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                               [this, slot](const juce::FileChooser& fc)
        {
            if (fc.getResults().size() > 0)
            {
                auto file = fc.getResult();
                processor.loadModel(file, slot);
                setModelName(slot, file.getFileNameWithoutExtension());
            }
        });
    }
}

void OscillatorPanel::setModelName(int slot, const juce::String& name)
{
    if (slot >= 0 && slot < 4)
    {
        modelNames[slot] = name;
        repaint();
    }
}

} // namespace NEURONiK::UI
