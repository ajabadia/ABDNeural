#include "ParameterPanel.h"

namespace Nexus::UI {

ParameterPanel::ParameterPanel(juce::AudioProcessorValueTreeState& vtsIn)
    : vts(vtsIn), visualizer(vtsIn)
{
    setupControl(attack,    "env_attack",        "ATTACK");
    setupControl(decay,     "env_decay",         "DECAY");
    setupControl(sustain,   "env_sustain",       "SUSTAIN");
    setupControl(release,   "env_release",       "RELEASE");
    
    setupControl(cutoff,    "filter_cutoff",     "CUTOFF");
    setupControl(resonance, "filter_res",        "RESONANCE");
    
    setupControl(rollOff,   "resonator_rolloff", "ROLL-OFF");
    
    addAndMakeVisible(visualizer);
}

void ParameterPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText)
{
    ctrl.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20); // true = read-only for cleaner look
    ctrl.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(ctrl.slider);
    
    ctrl.label.setText(labelText, juce::dontSendNotification);
    ctrl.label.setJustificationType(juce::Justification::centred);
    ctrl.label.setFont(juce::Font(12.0f, juce::Font::bold));
    addAndMakeVisible(ctrl.label);
    
    ctrl.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(vts, paramID, ctrl.slider);
}

void ParameterPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    
    // Premium Glass Look
    juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.08f), 0, 0,
                                  juce::Colours::white.withAlpha(0.02f), 0, area.getHeight(), false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(area, 10.0f);
    
    // Subtle border
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(area, 10.0f, 1.2f);
    
    // Cyan accents for "Neural" feel
    g.setColour(juce::Colours::cyan.withAlpha(0.03f));
    g.fillRoundedRectangle(area.reduced(1.0f), 10.0f);
}

void ParameterPanel::resized()
{
    auto area = getLocalBounds().reduced(15);
    auto rowHeight = area.getHeight() / 2;
    
    auto topRow = area.removeFromTop(rowHeight);
    auto bottomRow = area;
    
    // --- Layout Constants ---
    const int labelHeight = 15;
    const int textBoxHeight = 20;

    auto layoutControl = [&](RotaryControl& ctrl, juce::Rectangle<int> bounds) {
        // Label on top
        ctrl.label.setBounds(bounds.removeFromTop(labelHeight));
        // Slider in the middle (main area)
        ctrl.slider.setBounds(bounds.removeFromTop(bounds.getHeight() - textBoxHeight));
    };

    // --- Top Row: Envelope (65%) and Filter (35%) ---
    auto envArea = topRow.removeFromLeft(topRow.getWidth() * 0.65f);
    auto envWidth = envArea.getWidth() / 4;
    
    layoutControl(attack, envArea.removeFromLeft(envWidth).reduced(5));
    layoutControl(decay, envArea.removeFromLeft(envWidth).reduced(5));
    layoutControl(sustain, envArea.removeFromLeft(envWidth).reduced(5));
    layoutControl(release, envArea.reduced(5));
    
    // Filter
    auto filterWidth = topRow.getWidth() / 2;
    layoutControl(cutoff, topRow.removeFromLeft(filterWidth).reduced(5));
    layoutControl(resonance, topRow.reduced(5));
    
    // --- Bottom Row: Resonator + Spectral Visualizer ---
    auto resArea = bottomRow.removeFromLeft(bottomRow.getWidth() * 0.22f);
    layoutControl(rollOff, resArea.reduced(5));
    
    // Spectral Visualizer takes the remaining space
    visualizer.setBounds(bottomRow.reduced(10, 5));
}

} // namespace Nexus::UI
