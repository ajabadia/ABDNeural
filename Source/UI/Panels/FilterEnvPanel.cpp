#include "FilterEnvPanel.h"
#include "../../Main/NEURONiKProcessor.h"
#include "../../State/ParameterDefinitions.h"

namespace NEURONiK::UI {

using namespace NEURONiK::State;

FilterEnvPanel::FilterEnvPanel(NEURONiKProcessor& p)
    : processor(p), vts(p.getAPVTS())
{

    addAndMakeVisible(filterBox);
    addAndMakeVisible(fEnvBox);

    setupControl(cutoff,    IDs::filterCutoff,    "CUTOFF",    &processor.uiCutoff);
    setupControl(resonance, IDs::filterRes,       "RESONANCE", &processor.uiResonance);
    setupControl(envAmount, IDs::filterEnvAmount, "ENV AMT",   &processor.uiFEnvAmount);
    
    setupControl(fAttack,   IDs::filterAttack,    "ATTACK",    &processor.uiFAttack);
    setupControl(fDecay,    IDs::filterDecay,     "DECAY",     &processor.uiFDecay);
    setupControl(fSustain,  IDs::filterSustain,   "SUSTAIN",   &processor.uiFSustain);
    setupControl(fRelease,  IDs::filterRelease,   "RELEASE",   &processor.uiFRelease);
    
    // Create Visualizer passing the atoms it needs for shape and active level
    fEnvVisualizer = std::make_unique<EnvelopeVisualizer>(
        processor.uiFAttack, processor.uiFDecay, processor.uiFSustain, processor.uiFRelease, 
        processor.uiFEnvelope // This tracks the actual voice filter envelope level
    );
    fEnvBox.addAndMakeVisible(fEnvVisualizer.get());
    
    startTimerHz(30);
}

void FilterEnvPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText, std::atomic<float>* modValue)
{
    if (paramID == IDs::filterCutoff || paramID == IDs::filterRes || paramID == IDs::filterEnvAmount)
    {
        UIUtils::setupRotaryControl(filterBox, ctrl, paramID, labelText, vts, processor, sharedLNF, modValue);
    }
    else
    {
        UIUtils::setupRotaryControl(fEnvBox, ctrl, paramID, labelText, vts, processor, sharedLNF, modValue);
    }
}

void FilterEnvPanel::timerCallback()
{
    cutoff.slider.repaint();
    resonance.slider.repaint();
    envAmount.slider.repaint();
    fAttack.slider.repaint();
    fDecay.slider.repaint();
    fSustain.slider.repaint();
    fRelease.slider.repaint();
}

void FilterEnvPanel::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void FilterEnvPanel::resized()
{
    auto area = getLocalBounds().reduced(5);
    
    auto leftArea = area.removeFromLeft(area.getWidth() * 0.4f).reduced(3);
    auto rightArea = area.reduced(3);

    filterBox.setBounds(leftArea);
    fEnvBox.setBounds(rightArea);

    // Layout Filter Buttons
    {
        auto c = filterBox.getContentArea();
        auto knobH = c.getHeight() / 3;
        
        auto layoutKnobOrControl = [&](RotaryControl& ctrl, juce::Rectangle<int> bounds) {
            ctrl.label.setBounds(bounds.removeFromTop(15));
            ctrl.slider.setBounds(bounds);
        };

        layoutKnobOrControl(cutoff, c.removeFromTop(knobH).reduced(20, 5));
        layoutKnobOrControl(resonance, c.removeFromTop(knobH).reduced(20, 5));
        layoutKnobOrControl(envAmount, c.reduced(20, 5));
    }

    // Layout Env Buttons
    {
        auto c = fEnvBox.getContentArea();
        auto vizHeight = c.getHeight() * 0.45f;
        fEnvVisualizer->setBounds(c.removeFromTop(vizHeight).reduced(10));
        
        auto knobArea = c;
        auto knobW = knobArea.getWidth() / 4;
        
        auto layoutKnobOrControl = [&](RotaryControl& ctrl, juce::Rectangle<int> bounds) {
            ctrl.label.setBounds(bounds.removeFromTop(15));
            ctrl.slider.setBounds(bounds);
        };

        layoutKnobOrControl(fAttack, knobArea.removeFromLeft(knobW).reduced(5));
        layoutKnobOrControl(fDecay, knobArea.removeFromLeft(knobW).reduced(5));
        layoutKnobOrControl(fSustain, knobArea.removeFromLeft(knobW).reduced(5));
        layoutKnobOrControl(fRelease, knobArea.reduced(5));
    }
}

} // namespace NEURONiK::UI
