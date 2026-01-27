#include "ParameterPanel.h"
#include "../State/ParameterDefinitions.h"

namespace NEURONiK::UI {

using namespace NEURONiK::State;

ParameterPanel::ParameterPanel(NEURONiKProcessor& p)
    : processor(p), vts(p.getAPVTS())
{
    using namespace NEURONiK::State;

    addAndMakeVisible(ampEnvBox);
    addAndMakeVisible(globalBox);

    setupControl(attack,    IDs::envAttack,   "ATTACK",  &processor.uiAttack);
    setupControl(decay,     IDs::envDecay,    "DECAY",   &processor.uiDecay);
    setupControl(sustain,   IDs::envSustain,  "SUSTAIN", &processor.uiSustain);
    setupControl(release,   IDs::envRelease,  "RELEASE", &processor.uiRelease);
    
    setupControl(randomStrength, IDs::randomStrength, "STRENGTH");
    setupControl(masterBPM, IDs::masterBPM, "BPM");

    adsrVisualizer = std::make_unique<EnvelopeVisualizer>(
        processor.uiAttack, processor.uiDecay, processor.uiSustain, processor.uiRelease, 
        processor.uiEnvelope
    );
    ampEnvBox.addAndMakeVisible(adsrVisualizer.get());

    globalBox.addAndMakeVisible(freezeResBtn);
    globalBox.addAndMakeVisible(freezeFltBtn);
    globalBox.addAndMakeVisible(freezeEnvBtn);
    globalBox.addAndMakeVisible(randomizeButton);

    freezeResAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(vts, IDs::freezeResonator, freezeResBtn);
    freezeFltAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(vts, IDs::freezeFilter,    freezeFltBtn);
    freezeEnvAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(vts, IDs::freezeEnvelopes, freezeEnvBtn);

    freezeResMidi = std::make_unique<MidiLearner>(processor, freezeResBtn, IDs::freezeResonator);
    freezeFltMidi = std::make_unique<MidiLearner>(processor, freezeFltBtn, IDs::freezeFilter);
    freezeEnvMidi = std::make_unique<MidiLearner>(processor, freezeEnvBtn, IDs::freezeEnvelopes);

    randomizeButton.onClick = [this] { randomizeParameters(); };
    
    startTimerHz(30);
}

ParameterPanel::~ParameterPanel()
{
    stopTimer();
}

void ParameterPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText, std::atomic<float>* modValue)
{
    bool isEnv = paramID.startsWith("env");
    juce::Component& parent = isEnv ? ampEnvBox : globalBox;
    UIUtils::setupRotaryControl(parent, ctrl, paramID, labelText, vts, processor, sharedLNF, modValue);
}


void ParameterPanel::randomizeParameters()
{
    auto& random = juce::Random::getSystemRandom();

    auto randomizeParam = [&](const juce::String& id, float minVal, float maxVal) {
        if (auto* param = vts.getParameter(id))
        {
            // Check freeze flags based on parameter ID
            bool isFrozen = false;
            if (id == IDs::morphX || id == IDs::morphY || id == IDs::oscInharmonicity || id == IDs::oscRoughness ||
                id == IDs::resonatorParity || id == IDs::resonatorShift || id == IDs::resonatorRolloff)
            {
                isFrozen = vts.getRawParameterValue(IDs::freezeResonator)->load() > 0.5f;
            }
            else if (id == IDs::filterCutoff || id == IDs::filterRes)
            {
                isFrozen = vts.getRawParameterValue(IDs::freezeFilter)->load() > 0.5f;
            }
            else if (id.startsWith("env") || id == IDs::oscLevel)
            {
                isFrozen = vts.getRawParameterValue(IDs::freezeEnvelopes)->load() > 0.5f;
            }
            else if (id.startsWith("fx"))
            {
                 // Use Filter freeze for FX as well since we don't have a dedicated button
                 isFrozen = vts.getRawParameterValue(IDs::freezeFilter)->load() > 0.5f;
            }

            if (isFrozen) return;

            float strength = vts.getRawParameterValue(IDs::randomStrength)->load();
            float currentValue = vts.getParameterRange(id).convertFrom0to1(param->getValue());
            
            float fullRandomVal = minVal + random.nextFloat() * (maxVal - minVal);
            float random0to1 = vts.getParameterRange(id).convertTo0to1(fullRandomVal);
            
            float newVal0to1 = juce::jmap(strength, currentValue, random0to1);
            param->setValueNotifyingHost(newVal0to1);
        }
    };

    randomizeParam(IDs::morphX, 0.0f, 1.0f);
    randomizeParam(IDs::morphY, 0.0f, 1.0f);
    
    // Core Resonator Parameters
    randomizeParam(IDs::oscInharmonicity, 0.0f, 0.4f);
    randomizeParam(IDs::oscRoughness,     0.0f, 0.5f);
    
    // Advanced Spectral Controls
    randomizeParam(IDs::resonatorParity,  0.2f, 0.8f); 
    randomizeParam(IDs::resonatorShift,   0.8f, 1.3f);
    randomizeParam(IDs::resonatorRolloff, 0.5f, 2.8f);

    randomizeParam(IDs::filterCutoff, 200.0f, 8000.0f);
    randomizeParam(IDs::filterRes, 0.0f, 0.6f);
    randomizeParam(IDs::envAttack, 0.001f, 0.5f);
    randomizeParam(IDs::envDecay, 0.1f, 1.0f);
    randomizeParam(IDs::envSustain, 0.2f, 0.8f);
    randomizeParam(IDs::envRelease, 0.1f, 2.0f);
    
    // Add missing params
    randomizeParam(IDs::oscLevel, 0.3f, 0.8f);
    
    // Mild FX Randomization
    randomizeParam(IDs::fxSaturation, 0.0f, 0.4f);
    randomizeParam(IDs::fxChorusMix, 0.0f, 0.5f);
    randomizeParam(IDs::fxReverbMix, 0.0f, 0.4f);
}

void ParameterPanel::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void ParameterPanel::resized()
{
    auto area = getLocalBounds().reduced(5);
    
    auto leftArea = area.removeFromLeft(area.getWidth() * 0.7f).reduced(3);
    auto rightArea = area.reduced(3);

    ampEnvBox.setBounds(leftArea);
    globalBox.setBounds(rightArea);

    // Layout Amp Env
    {
        auto c = ampEnvBox.getContentArea();
        auto vizH = c.getHeight() * 0.5f;
        adsrVisualizer->setBounds(c.removeFromTop(vizH).reduced(10));
        
        auto knobArea = c;
        auto knobW = knobArea.getWidth() / 4;
        auto layoutRotary = [&](RotaryControl& ctrl, juce::Rectangle<int> bounds) {
            ctrl.label.setBounds(bounds.removeFromTop(15));
            ctrl.slider.setBounds(bounds);
        };

        layoutRotary(attack, knobArea.removeFromLeft(knobW).reduced(5));
        layoutRotary(decay, knobArea.removeFromLeft(knobW).reduced(5));
        layoutRotary(sustain, knobArea.removeFromLeft(knobW).reduced(5));
        layoutRotary(release, knobArea.reduced(5));
    }

    // Layout Global
    {
        auto c = globalBox.getContentArea();
        auto knobH = c.getHeight() / 4;
        
        auto layoutRotary = [&](RotaryControl& ctrl, juce::Rectangle<int> bounds) {
            ctrl.label.setBounds(bounds.removeFromTop(15));
            ctrl.slider.setBounds(bounds);
        };

        layoutRotary(masterBPM, c.removeFromTop(knobH).reduced(15, 2));
        layoutRotary(randomStrength, c.removeFromTop(knobH).reduced(15, 2));
        
        auto btnH = 24;
        randomizeButton.setBounds(c.removeFromTop(btnH + 10).reduced(10, 5));
        
        auto toggleH = c.getHeight() / 3;
        freezeResBtn.setBounds(c.removeFromTop(toggleH).reduced(5));
        freezeFltBtn.setBounds(c.removeFromTop(toggleH).reduced(5));
        freezeEnvBtn.setBounds(c.reduced(5));
    }
}

void ParameterPanel::timerCallback()
{
    attack.slider.repaint();
    decay.slider.repaint();
    sustain.slider.repaint();
    release.slider.repaint();
    masterBPM.slider.repaint();
    randomStrength.slider.repaint();
}

} // namespace NEURONiK::UI
