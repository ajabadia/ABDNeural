#include "OscillatorPanel.h"
#include "../../State/ParameterDefinitions.h"

namespace Nexus::UI {

OscillatorPanel::OscillatorPanel(juce::AudioProcessorValueTreeState& vtsIn)
    : vts(vtsIn), visualizer(vtsIn), xyPad(vtsIn)
{
    addAndMakeVisible(visualizer);
    addAndMakeVisible(xyPad);
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
}

void OscillatorPanel::resized()
{
    auto floatArea = getLocalBounds().reduced(20).toFloat();
    
    auto vizArea = floatArea.removeFromLeft(floatArea.getWidth() * 0.65f);
    visualizer.setBounds(vizArea.reduced(5.0f).toNearestInt());

    auto padArea = floatArea;
    xyPad.setBounds(padArea.reduced(20.0f).toNearestInt());
}

} // namespace Nexus::UI
