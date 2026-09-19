#include "ParameterPanel.h"
#include "ThemeManager.h"
#include "../Main/NEURONiKProcessor.h"
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

    // El `EnvelopeVisualizer` (y su miembro adsrVisualizer) se retiro el 2026-09-19:
    // se construia en CADA apertura del editor y nunca se anadia al panel ("removed
    // from General per user request (no space)"), asi que era una asignacion muerta.
    // La curva ADSR vive ahora en la WebUI (ficha FILTRO & ENVOLVENTE).
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

    // RANDOM: la logica es COMPARTIDA (State/ParameterRandomizer), no un metodo
    // privado de este panel. Antes vivia aqui y tenia un `jmap` que mezclaba
    // unidades reales con normalizadas (ver ParameterRandomizerTest).
    randomizeButton.onClick = [this]
    {
        NEURONiK::State::applyRandomize (vts, NEURONiK::State::readRandomizeStrength (vts), random);
    };
    
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

    // Layout Unison (Row 2) - detune and spread only. The ENABLE UNISON toggle
    // was removed: nothing in the engine ever read it, so it promised a sound
    // change it did not make. Detune at 0 is what switches the unison off.
    {
        auto c = unisonBox.getContentArea();
        auto knobW = c.getWidth() / 2;
        
        layoutRotary(unisonDetune, c.removeFromLeft(knobW).reduced(5));
        layoutRotary(unisonSpread, c.reduced(5));
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
