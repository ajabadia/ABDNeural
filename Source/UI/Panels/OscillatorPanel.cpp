#include "OscillatorPanel.h"
#include "../../Main/NEURONiKProcessor.h"
#include "../../State/ParameterDefinitions.h"

namespace NEURONiK::UI {

OscillatorPanel::OscillatorPanel(NEURONiKProcessor& p)
    : processor(p),
      xyPad(p, p.getAPVTS()),
      loadA("A"), loadB("B"), loadC("C"), loadD("D")
{
    using namespace NEURONiK::State;
    addAndMakeVisible(xyPad);

    setupControl(inharmonicity, IDs::oscInharmonicity, "INHARMONICITY", &processor.uiInharmonicity);
    setupControl(roughness,    IDs::oscRoughness,     "ROUGHNESS",     &processor.uiRoughness);
    setupControl(parity,       IDs::resonatorParity,  "PARITY",        &processor.uiParity); 
    setupControl(shift,        IDs::resonatorShift,   "SHIFT",         &processor.uiShift);
    setupControl(rollOff,      IDs::resonatorRolloff, "ROLL-OFF",      &processor.uiRollOff);

    addAndMakeVisible(loadA);
    loadA.addListener(this);
    addAndMakeVisible(loadB);
    loadB.addListener(this);
    addAndMakeVisible(loadC);
    loadC.addListener(this);
    addAndMakeVisible(loadD);
    loadD.addListener(this);

    for (auto& loadBtn : { &loadA, &loadB, &loadC, &loadD })
    {
        loadBtn->setAlpha(0.7f);
        loadBtn->setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey.withAlpha(0.3f));
    }

    modelNames = processor.getModelNames();
    xyPad.setModelNames(modelNames);
    
    startTimerHz(10);
}

    // Internal LookAndFeel class for modulation
    class ModulatedKnobLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        ModulatedKnobLookAndFeel(std::atomic<float>* v) : val(v) {}
        
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                              const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
        {
            auto radius = (float)juce::jmin(width / 2, height / 2) - 4.0f;
            auto centreX = (float)x + (float)width * 0.5f;
            auto centreY = (float)y + (float)height * 0.5f;
            auto rx = centreX - radius;
            auto ry = centreY - radius;
            auto rw = radius * 2.0f;
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            // Base track
            g.setColour(juce::Colours::darkgrey);
            g.drawEllipse(rx, ry, rw, rw, 4.0f);

            // Value arc
            juce::Path p;
            p.addArc(centreX, centreY, radius, radius, rotaryStartAngle, angle, true);
            g.setColour(juce::Colour(0xFF005555)); // Base Teal
            g.strokePath(p, juce::PathStrokeType(4.0f));
            
            // Pointer
            juce::Path ptr;
            ptr.addRectangle(-2.0f, -radius, 4.0f, 6.0f);
            ptr.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
            g.setColour(juce::Colours::white);
            g.fillPath(ptr);

            // Modulation Arc (Ghost Ring)
            if (val != nullptr)
            {
                // We assume val is 0..1 normalized. But we need this rotary's range
                auto range = slider.getRange();
                float currentMod = val->load(std::memory_order_relaxed);
                // Convert real value -> 0..1
                float normMod = (float)((currentMod - range.getStart()) / range.getLength());
                auto modAngle = rotaryStartAngle + normMod * (rotaryEndAngle - rotaryStartAngle);

                // Draw thin cyan ring for modulation
                float modRadius = radius + 3.0f;
                juce::Path modP;
                modP.addArc(centreX, centreY, modRadius, modRadius, rotaryStartAngle, modAngle, true);
                g.setColour(juce::Colours::cyan.withAlpha(0.6f));
                g.strokePath(modP, juce::PathStrokeType(2.0f));
            }
        }
    private:
        std::atomic<float>* val;
    };
    
    std::vector<std::unique_ptr<ModulatedKnobLookAndFeel>> lnfs;

void OscillatorPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText, std::atomic<float>* modValue)
{
    auto& apvts = processor.getAPVTS();
    ctrl.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);
    ctrl.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    
    // Apply custom LNF if we have a modulation source
    if (modValue != nullptr)
    {
        ctrl.boundModularValue = modValue;
        auto lnf = std::make_unique<ModulatedKnobLookAndFeel>(modValue);
        ctrl.slider.setLookAndFeel(lnf.get());
        lnfs.push_back(std::move(lnf));
    }
    
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
}

void OscillatorPanel::resized()
{
    auto area = getLocalBounds().reduced(15);
    
    // 1. Calculate Square Pad area
    int padSize = juce::jmin(area.getWidth() - 180, area.getHeight()); // Reserve space for controls
    auto padArea = area.removeFromLeft(padSize).withSizeKeepingCentre(padSize, padSize);
    xyPad.setBounds(padArea);

    // 2. Buttons in corners of the pad
    int buttonOffset = 2; // Slight indent
    int buttonSize = 24;
    loadA.setBounds(padArea.getX() + buttonOffset, padArea.getY() + buttonOffset, buttonSize, buttonSize);
    loadB.setBounds(padArea.getRight() - buttonSize - buttonOffset, padArea.getY() + buttonOffset, buttonSize, buttonSize);
    loadC.setBounds(padArea.getX() + buttonOffset, padArea.getBottom() - buttonSize - buttonOffset, buttonSize, buttonSize);
    loadD.setBounds(padArea.getRight() - buttonSize - buttonOffset, padArea.getBottom() - buttonSize - buttonOffset, buttonSize, buttonSize);

    // 3. Controls in 2-column grid
    auto controlsArea = area.reduced(10, 5);
    int colWidth = controlsArea.getWidth() / 2;
    int rowHeight = controlsArea.getHeight() / 3;

    auto layoutControl = [&](RotaryControl& ctrl, juce::Rectangle<int> bounds) {
        ctrl.label.setBounds(bounds.removeFromTop(15));
        ctrl.slider.setBounds(bounds);
    };

    auto row1 = controlsArea.removeFromTop(rowHeight);
    layoutControl(inharmonicity, row1.removeFromLeft(colWidth).reduced(5));
    layoutControl(roughness,     row1.reduced(5));

    auto row2 = controlsArea.removeFromTop(rowHeight);
    layoutControl(parity, row2.removeFromLeft(colWidth).reduced(5));
    layoutControl(shift,  row2.reduced(5));

    auto row3 = controlsArea;
    // Let's center roll-off in the last row since there's an odd number
    layoutControl(rollOff, row3.withSizeKeepingCentre(colWidth, rowHeight).reduced(5));
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
        xyPad.setModelNames(modelNames);
        repaint();
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
    
    // Trigger repaint of sliders to show animation
    inharmonicity.slider.repaint();
    roughness.slider.repaint();
    parity.slider.repaint();
    shift.slider.repaint();
    rollOff.slider.repaint();
}

} // namespace NEURONiK::UI
