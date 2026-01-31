#include "ParameterPanel.h"
#include "ThemeManager.h"
#include "../State/ParameterDefinitions.h"

namespace NEURONiK::UI {

using ::NEURONiK::ModulationTarget;

using namespace NEURONiK::State;

ParameterPanel::ParameterPanel(NEURONiKProcessor& p)
    : processor(p), vts(p.getAPVTS())
{
    using namespace NEURONiK::State;

    addAndMakeVisible(ampEnvBox);
    addAndMakeVisible(unisonBox);
    addAndMakeVisible(globalSettingsBox);
    addAndMakeVisible(globalBox);

    unisonBox.addAndMakeVisible(unisonEnabled);
    unisonEnabledAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(vts, IDs::unisonEnabled, unisonEnabled);

    globalBox.addAndMakeVisible(titleLabel);
    globalBox.addAndMakeVisible(versionLabel);

    const auto& theme = ThemeManager::getCurrentTheme();
    titleLabel.setFont(juce::Font(juce::FontOptions(32.0f).withStyle("Bold")));
    titleLabel.setColour(juce::Label::textColourId, theme.accent);
    titleLabel.setJustificationType(juce::Justification::centred);

    versionLabel.setColour(juce::Label::textColourId, theme.text.withAlpha(0.6f));
    versionLabel.setJustificationType(juce::Justification::centred);

    setupControl(attack,    IDs::envAttack,   "ATTACK",  ModulationTarget::AmpAttack);
    setupControl(decay,     IDs::envDecay,    "DECAY",   ModulationTarget::AmpDecay);
    setupControl(sustain,   IDs::envSustain,  "SUSTAIN", ModulationTarget::AmpSustain);
    setupControl(release,   IDs::envRelease,  "RELEASE", ModulationTarget::AmpRelease);
    
    setupControl(unisonDetune, IDs::unisonDetune, "DETUNE", ModulationTarget::UnisonDetune);
    setupControl(unisonSpread, IDs::unisonSpread, "SPREAD", ModulationTarget::Count);
    setupControl(masterLevel, IDs::masterLevel, "VOLUME", ModulationTarget::MasterLevel);
    setupControl(randomStrength, IDs::randomStrength, "STRENGTH", ModulationTarget::Count);

    adsrVisualizer = std::make_unique<EnvelopeVisualizer>(
        processor.uiAttack, processor.uiDecay, processor.uiSustain, processor.uiRelease, 
        processor.uiEnvelope
    );
    // Visualizer removed from General per user request (no space)
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
    engineMidi    = std::make_unique<MidiLearner>(processor, engineSelector,IDs::engineType);
    globalBox.addAndMakeVisible(engineSelector);
    engineSelector.addItem("Engine: NEURONiK", 1);
    engineSelector.addItem("Engine: Neurotik", 2);
    engineAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(vts, IDs::engineType, engineSelector);

    randomizeButton.onClick = [this] { randomizeParameters(); };
    
    startTimerHz(30);
}

ParameterPanel::~ParameterPanel()
{
    stopTimer();
}

void ParameterPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText, ::NEURONiK::ModulationTarget modTarget)
{
    bool isEnv = paramID.startsWith("env");
    bool isUnison = paramID.contains("unison");
    bool isSettings = paramID == IDs::masterLevel || paramID == IDs::randomStrength;
    
    juce::Component& parent = isEnv ? ampEnvBox : (isUnison ? unisonBox : (isSettings ? globalSettingsBox : globalBox));
    UIUtils::setupRotaryControl(parent, ctrl, paramID, labelText, vts, processor, sharedLNF, modTarget);
}

void ParameterPanel::setupControl(VerticalSliderControl& ctrl, const juce::String& paramID, const juce::String& labelText, ::NEURONiK::ModulationTarget modTarget)
{
    // Add directly to the panel as it has its own dedicated area
    UIUtils::setupVerticalSlider(*this, ctrl, paramID, labelText, vts, processor, verticalLNF, modTarget);
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
                id == IDs::resonatorParity || id == IDs::resonatorShift || id == IDs::resonatorRolloff ||
                id == IDs::oscExciteNoise || id == IDs::excitationColor || id == IDs::impulseMix || id == IDs::resonatorRes ||
                id == IDs::unisonDetune || id == IDs::unisonSpread)
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

    // Neurotik Specific (Phase 30.5)
    randomizeParam(IDs::oscExciteNoise,  0.0f, 0.8f);
    randomizeParam(IDs::excitationColor, 0.2f, 0.7f);
    randomizeParam(IDs::impulseMix,       0.0f, 1.0f);
    randomizeParam(IDs::resonatorRes,    0.3f, 0.95f);

    // Mild Unison Randomization
    randomizeParam(IDs::unisonDetune, 0.0f, 0.05f);
    randomizeParam(IDs::unisonSpread, 0.2f, 0.8f);
}

void ParameterPanel::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void ParameterPanel::resized()
{
    auto area = getLocalBounds().reduced(5);
    
    // Dashboard/Global on the LEFT (25%), Controls in CENTER (65%), Volume on RIGHT (10%)
    auto leftArea = area.removeFromLeft(static_cast<int>(area.getWidth() * 0.28f)).reduced(3);
    auto volumeArea = area.removeFromRight(60).reduced(3); // Vertical slider
    auto centerArea = area.reduced(3);

    globalBox.setBounds(leftArea);

    // Center: 2 rows + 1 column for strength
    auto strengthCol = centerArea.removeFromRight(static_cast<int>(centerArea.getWidth() * 0.25f)).reduced(0, 3);
    auto rowsArea = centerArea;
    
    auto rowH = rowsArea.getHeight() / 2;
    ampEnvBox.setBounds(rowsArea.removeFromTop(rowH).reduced(0, 3));
    unisonBox.setBounds(rowsArea.reduced(0, 3));
    globalSettingsBox.setBounds(strengthCol);

    auto layoutRotary = [&](RotaryControl& ctrl, juce::Rectangle<int> bounds) {
        ctrl.label.setBounds(bounds.removeFromTop(15));
        ctrl.slider.setBounds(bounds);
    };

    // Layout Amp Env (Row 1) - 4 knobs in 3 columns
    {
        auto c = ampEnvBox.getContentArea();
        auto knobW = c.getWidth() / 4;

        layoutRotary(attack, c.removeFromLeft(static_cast<int>(knobW)).reduced(5));
        layoutRotary(decay, c.removeFromLeft(static_cast<int>(knobW)).reduced(5));
        layoutRotary(sustain, c.removeFromLeft(static_cast<int>(knobW)).reduced(5));
        layoutRotary(release, c.reduced(5));
    }

    // Layout Unison (Row 2) - 2 knobs + toggle in 3 columns
    {
        auto c = unisonBox.getContentArea();
        
        auto toggleArea = c.removeFromLeft(static_cast<int>(c.getWidth() * 0.25f)).reduced(5);
        unisonEnabled.setBounds(toggleArea);

        auto knobArea = c;
        auto knobW = knobArea.getWidth() / 2;
        
        layoutRotary(unisonDetune, knobArea.removeFromLeft(knobW).reduced(5));
        layoutRotary(unisonSpread, knobArea.reduced(5));
    }

    // Layout Random Strength (Column 4)
    {
        auto c = globalSettingsBox.getContentArea();
        layoutRotary(randomStrength, c.reduced(5));
    }

    // Layout Master Volume (Right side - vertical slider)
    // TODO: Implement vertical slider component
    // For now, use rotary control
    {
        masterLevel.label.setBounds(volumeArea.removeFromTop(20));
        masterLevel.slider.setBounds(volumeArea);
    }

    // Layout Dashboard/Global (Left Column)
    {
        auto c = globalBox.getContentArea();
        
        titleLabel.setBounds(c.removeFromTop(45));
        versionLabel.setBounds(c.removeFromTop(15));

        c.removeFromTop(10); // padding

        auto btnH = 26;
        engineSelector.setBounds(c.removeFromTop(btnH + 5).reduced(10, 2));
        randomizeButton.setBounds(c.removeFromTop(btnH + 5).reduced(10, 2));
        
        c.removeFromTop(10); // padding

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
    masterLevel.slider.repaint();
    randomStrength.slider.repaint();
    unisonDetune.slider.repaint();
    unisonSpread.slider.repaint();
}

} // namespace NEURONiK::UI
