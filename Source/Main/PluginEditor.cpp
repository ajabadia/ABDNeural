#include "PluginEditor.h"

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(p), processor(p)
{
    setSize(400, 300);
}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("NEXUS Synthesizer", getLocalBounds(), juce::Justification::centred, 1);
}

void PluginEditor::resized()
{
}
