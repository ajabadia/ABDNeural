#include "ParameterPanel.h"
#include "../State/ParameterDefinitions.h"

namespace NEURONiK::UI {

using namespace NEURONiK::State;

ParameterPanel::ParameterPanel(NEURONiKProcessor& p)
    : processor(p), vts(p.getAPVTS()), presetPanel(p)
{
    addAndMakeVisible(presetPanel);

    setupControl(attack,    IDs::envAttack,   "ATTACK");
    setupControl(decay,     IDs::envDecay,    "DECAY");
    setupControl(sustain,   IDs::envSustain,  "SUSTAIN");
    setupControl(release,   IDs::envRelease,  "RELEASE");
    
    // Link Filter controls to visualization atoms
    setupControl(cutoff,    IDs::filterCutoff, "BRILLANCE", &processor.uiCutoff);
    setupControl(resonance, IDs::masterLevel,  "VOLUME", &processor.uiResonance);
    
    setupControl(randomStrength, IDs::randomStrength, "RND STRENGTH");

    addAndMakeVisible(freezeResBtn);
    addAndMakeVisible(freezeFltBtn);
    addAndMakeVisible(freezeEnvBtn);

    freezeResAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(vts, IDs::freezeResonator, freezeResBtn);
    freezeFltAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(vts, IDs::freezeFilter,    freezeFltBtn);
    freezeEnvAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(vts, IDs::freezeEnvelopes, freezeEnvBtn);

    freezeResMidi = std::make_unique<MidiLearner>(processor, freezeResBtn, IDs::freezeResonator);
    freezeFltMidi = std::make_unique<MidiLearner>(processor, freezeFltBtn, IDs::freezeFilter);
    freezeEnvMidi = std::make_unique<MidiLearner>(processor, freezeEnvBtn, IDs::freezeEnvelopes);

    addAndMakeVisible(randomizeButton);
    randomizeButton.onClick = [this] { randomizeParameters(); };

    titleLabel.setFont(juce::Font(juce::FontOptions(28.0f).withStyle("Bold")));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setFont(juce::Font(juce::FontOptions(14.0f).withStyle("Italic")));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    subtitleLabel.setJustificationType(juce::Justification::centredLeft);
    subtitleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(subtitleLabel);
    
    // Timer for animation
    startTimerHz(30);
}

// Internal LookAndFeel for ParameterPanel (Duplicated from OscillatorPanel for simplicity, 
// in a real refactor this should be in a common header)
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
        g.setColour(juce::Colour(0xFF005555)); 
        g.strokePath(p, juce::PathStrokeType(4.0f));
        
        // Pointer
        juce::Path ptr;
        ptr.addRectangle(-2.0f, -radius, 4.0f, 6.0f);
        ptr.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(juce::Colours::white);
        g.fillPath(ptr);

        // Modulation Arc
        if (val != nullptr)
        {
            auto range = slider.getRange();
            float currentMod = val->load(std::memory_order_relaxed);
            // Volume is special, it's not 0-1 param usually, but masterLevel is 0-1 in our definition. Cutoff is 20-20k
            
            float normMod = 0.0f;
            // Handle skew/range mapping if needed, simplified here assuming direct mapping or 0-1 for normalized visualization
            // For Cutoff which is 20-20000, we need to be careful if uiCutoff stores Hz or 0-1
            // In Processor we stored real value. So we map.
            normMod = (float)((currentMod - range.getStart()) / range.getLength());

            auto modAngle = rotaryStartAngle + normMod * (rotaryEndAngle - rotaryStartAngle);

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

void ParameterPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText, std::atomic<float>* modValue)
{
    ctrl.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);
    ctrl.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    
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
    
    ctrl.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(vts, paramID, ctrl.slider);
    ctrl.midiLearner = std::make_unique<MidiLearner>(processor, ctrl.slider, paramID);
}

void ParameterPanel::timerCallback()
{
    cutoff.slider.repaint();
    resonance.slider.repaint();
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
            else if (id.startsWith("env"))
            {
                isFrozen = vts.getRawParameterValue(IDs::freezeEnvelopes)->load() > 0.5f;
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
}

void ParameterPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    
    juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.08f), 0, 0,
                                  juce::Colours::white.withAlpha(0.02f), 0, area.getHeight(), false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(area, 10.0f);
    
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(area, 10.0f, 1.2f);
    
    g.setColour(juce::Colours::cyan.withAlpha(0.03f));
    g.fillRoundedRectangle(area.reduced(1.0f), 10.0f);
}

void ParameterPanel::resized()
{
    auto area = getLocalBounds().reduced(15);

    presetPanel.setBounds(area.removeFromTop(40));

    auto titleArea = area.removeFromBottom(40);
    titleLabel.setBounds(titleArea.removeFromLeft(200));
    subtitleLabel.setBounds(titleArea);

    auto controlsArea = area.reduced(10, 0);

    const int numRows = 2;
    const int numCols = 4;
    auto rowHeight = controlsArea.getHeight() / numRows;
    auto colWidth = controlsArea.getWidth() / numCols;

    auto layoutControl = [&](RotaryControl& ctrl, int row, int col) {
        auto bounds = controlsArea.withY(controlsArea.getY() + row * rowHeight)
                                .withX(controlsArea.getX() + col * colWidth)
                                .withHeight(rowHeight)
                                .withWidth(colWidth);

        ctrl.label.setBounds(bounds.removeFromTop(15));
        ctrl.slider.setBounds(bounds.reduced(5));
    };

    layoutControl(attack, 0, 0);
    layoutControl(decay, 0, 1);
    layoutControl(sustain, 0, 2);
    layoutControl(release, 0, 3);

    layoutControl(cutoff, 1, 0);
    layoutControl(resonance, 1, 1);
    layoutControl(randomStrength, 1, 2);

    auto btnArea = controlsArea.withY(controlsArea.getY() + rowHeight)
                               .withX(controlsArea.getX() + 3 * colWidth)
                               .withHeight(rowHeight)
                               .withWidth(colWidth);
    
    randomizeButton.setBounds(btnArea.removeFromTop(static_cast<int>(rowHeight * 0.6f)).reduced(5));
    
    auto freezeArea = btnArea;
    int fH = freezeArea.getHeight() / 3;
    freezeResBtn.setBounds(freezeArea.removeFromTop(fH));
    freezeFltBtn.setBounds(freezeArea.removeFromTop(fH));
    freezeEnvBtn.setBounds(freezeArea);
}

} // namespace NEURONiK::UI
