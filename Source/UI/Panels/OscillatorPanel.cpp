#include "OscillatorPanel.h"
#include "../ThemeManager.h"
#include "../../Main/NEURONiKProcessor.h"
#include "../../State/ParameterDefinitions.h"

namespace NEURONiK::UI {
 
using namespace NEURONiK::State;
using ::NEURONiK::ModulationTarget;

OscillatorPanel::OscillatorPanel(NEURONiKProcessor& p)
    : processor(p),
      xyPad(p, p.getAPVTS()),
      loadA("A"), loadB("B"), loadC("C"), loadD("D"),
      modelBox("MODEL"), engineBox("ENGINE")
{
    addAndMakeVisible(modelBox);
    addAndMakeVisible(engineBox);

    modelBox.addAndMakeVisible(xyPad);

    setupControl(inharmonicity, IDs::oscInharmonicity, "INHARMONICITY", ModulationTarget::Inharmonicity);
    setupControl(roughness,    IDs::oscRoughness,     "ROUGHNESS",     ModulationTarget::Roughness);
    setupControl(parity,       IDs::resonatorParity,  "PARITY",        ModulationTarget::Parity); 
    setupControl(shift,        IDs::resonatorShift,   "SHIFT",         ModulationTarget::Shift);
    setupControl(rollOff,      IDs::resonatorRolloff, "ROLL-OFF",      ModulationTarget::ResonatorRolloff);

    // Neurotik
    setupControl(exciteNoise,  IDs::oscExciteNoise,   "EXCITE NOISE", ModulationTarget::ExciteNoise);
    setupControl(exciteColor,  IDs::excitationColor,  "EXCITE COLOR", ModulationTarget::ExciteColor);
    setupControl(impulseMix,   IDs::impulseMix,       "IMPULSE MIX",  ModulationTarget::ImpulseMix);
    setupControl(resonatorRes, IDs::resonatorRes,     " RESONANCE",   ModulationTarget::ResonatorResonance);


    modelBox.addAndMakeVisible(loadA);
    loadA.addListener(this);
    modelBox.addAndMakeVisible(loadB);
    loadB.addListener(this);
    modelBox.addAndMakeVisible(loadC);
    loadC.addListener(this);
    modelBox.addAndMakeVisible(loadD);
    loadD.addListener(this);

    for (auto& loadBtn : { &loadA, &loadB, &loadC, &loadD })
    {
        loadBtn->setAlpha(0.7f);
        loadBtn->setColour(juce::TextButton::buttonColourId, ThemeManager::getCurrentTheme().surface.withAlpha(0.3f));
    }

    modelNames = processor.getModelNames();
    xyPad.setModelNames(modelNames);
    
    startTimerHz(10);
}

void OscillatorPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText, ::NEURONiK::ModulationTarget modTarget)
{
    UIUtils::setupRotaryControl(engineBox, ctrl, paramID, labelText, processor.getAPVTS(), processor, sharedLNF, modTarget);
}

void OscillatorPanel::setModelName(int slot, const juce::String& name)
{
    if (slot >= 0 && slot < 4)
    {
        modelNames[slot] = name;
        xyPad.setModelNames(modelNames);
        repaint();
    }
}

void OscillatorPanel::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void OscillatorPanel::resized()
{
    auto area = getLocalBounds().reduced(5);
    
    auto leftArea = area.removeFromLeft(static_cast<int>(area.getWidth() * 0.45f)).reduced(3);
    auto rightArea = area.reduced(3);
    modelBox.setBounds(leftArea);
    engineBox.setBounds(rightArea);

    // Layout Model Box (XYPad)
    {
        auto c = modelBox.getContentArea();
        int padSize = juce::jmin(c.getWidth(), c.getHeight());
        auto padArea = c.withSizeKeepingCentre(padSize, padSize);
        xyPad.setBounds(padArea);

        int buttonOffset = 2;
        int buttonSize = 22;
        loadA.setBounds(padArea.getX() + buttonOffset, padArea.getY() + buttonOffset, buttonSize, buttonSize);
        loadB.setBounds(padArea.getRight() - buttonSize - buttonOffset, padArea.getY() + buttonOffset, buttonSize, buttonSize);
        loadC.setBounds(padArea.getX() + buttonOffset, padArea.getBottom() - buttonSize - buttonOffset, buttonSize, buttonSize);
        loadD.setBounds(padArea.getRight() - buttonSize - buttonOffset, padArea.getBottom() - buttonSize - buttonOffset, buttonSize, buttonSize);
    }

    // Layout Engine Box (3x3 grid)
    {
        auto c = engineBox.getContentArea();
        auto rowH = c.getHeight() / 3;
        auto colW = c.getWidth() / 3;

        auto layoutRotary = [&](RotaryControl& ctrl, juce::Rectangle<int> bounds) {
            ctrl.label.setBounds(bounds.removeFromTop(15));
            ctrl.slider.setBounds(bounds);
        };

        // Row 1
        auto r1 = c.removeFromTop(rowH);
        layoutRotary(inharmonicity, r1.removeFromLeft(colW).reduced(5));
        layoutRotary(roughness, r1.removeFromLeft(colW).reduced(5));
        layoutRotary(rollOff, r1.reduced(5));

        // Row 2
        auto r2 = c.removeFromTop(rowH);
        layoutRotary(parity, r2.removeFromLeft(colW).reduced(5));
        layoutRotary(shift, r2.removeFromLeft(colW).reduced(5));
        layoutRotary(resonatorRes, r2.reduced(5));

        // Row 3
        auto r3 = c;
        layoutRotary(exciteNoise, r3.removeFromLeft(colW).reduced(5));
        layoutRotary(exciteColor, r3.removeFromLeft(colW).reduced(5));
        layoutRotary(impulseMix, r3.reduced(5));
    }

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

void OscillatorPanel::timerCallback()
{
    auto latestNames = processor.getModelNames();
    bool changed = false;
    for (int i = 0; i < 4; ++i)
    {
        if (latestNames[i] != modelNames[i])
        {
            modelNames[i] = latestNames[i];
            changed = true;
        }
    }
    
    if (changed)
    {
        xyPad.setModelNames(modelNames);
        repaint();
    }
    
    // Trigger repaint of sliders for modulation animation
    for (auto* ctrl : { &inharmonicity, &roughness, &parity, &shift, &rollOff, 
                        &exciteNoise, &exciteColor, &impulseMix, &resonatorRes })
    {
        ctrl->slider.repaint();
    }

    // Visibility logic
    auto* engineParam = processor.getAPVTS().getRawParameterValue(IDs::engineType);
    if (engineParam != nullptr)
    {
        int engineType = static_cast<int>(engineParam->load());
        bool isNeuronik = (engineType == 0);
        
        inharmonicity.slider.setEnabled(isNeuronik);
        roughness.slider.setEnabled(isNeuronik);
        parity.slider.setEnabled(isNeuronik);
        shift.slider.setEnabled(isNeuronik);
        rollOff.slider.setEnabled(isNeuronik);
        
        exciteNoise.slider.setEnabled(!isNeuronik);
        exciteColor.slider.setEnabled(!isNeuronik);
        impulseMix.slider.setEnabled(!isNeuronik);
        resonatorRes.slider.setEnabled(!isNeuronik);

        inharmonicity.label.setEnabled(isNeuronik);
        roughness.label.setEnabled(isNeuronik);
        parity.label.setEnabled(isNeuronik);
        shift.label.setEnabled(isNeuronik);
        rollOff.label.setEnabled(isNeuronik);
        
        exciteNoise.label.setEnabled(!isNeuronik);
        exciteColor.label.setEnabled(!isNeuronik);
        impulseMix.label.setEnabled(!isNeuronik);
        resonatorRes.label.setEnabled(!isNeuronik);
    }
}

} // namespace NEURONiK::UI
