#include "FilterEnvPanel.h"
#include "../../Main/NEURONiKProcessor.h"
#include "../../State/ParameterDefinitions.h"

namespace NEURONiK::UI {

FilterEnvPanel::FilterEnvPanel(NEURONiKProcessor& p)
    : processor(p), vts(p.getAPVTS())
{
    using namespace NEURONiK::State;

    setupControl(attack,    IDs::envAttack,    "ATTACK",   &processor.uiAttack);
    setupControl(decay,     IDs::envDecay,     "DECAY",    &processor.uiDecay);
    setupControl(sustain,   IDs::envSustain,   "SUSTAIN",  &processor.uiSustain);
    setupControl(release,   IDs::envRelease,   "RELEASE",  &processor.uiRelease);
    
    setupControl(cutoff,    IDs::filterCutoff, "CUTOFF",   &processor.uiCutoff);
    setupControl(resonance, IDs::filterRes,    "RESONANCE",&processor.uiResonance);
    
    // Create Visualizer passing the atoms it needs for shape and active level
    envVisualizer = std::make_unique<EnvelopeVisualizer>(
        processor.uiAttack, processor.uiDecay, processor.uiSustain, processor.uiRelease, 
        processor.uiEnvelope // This tracks the actual voice envelope level
    );
    addAndMakeVisible(envVisualizer.get());
    
    startTimerHz(30);
}

// Internal LookAndFeel for FilterEnvPanel (Duplicated from OscillatorPanel/ParameterPanel)
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
            // Assuming normalization matches.
            float normMod = (float)((currentMod - range.getStart()) / range.getLength());

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

void FilterEnvPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText, std::atomic<float>* modValue)
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

void FilterEnvPanel::timerCallback()
{
    attack.slider.repaint();
    decay.slider.repaint();
    sustain.slider.repaint();
    release.slider.repaint();
    cutoff.slider.repaint();
    resonance.slider.repaint();
}

void FilterEnvPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.05f), 0, 0,
                                  juce::Colours::white.withAlpha(0.01f), 0, area.getHeight(), false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(area, 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawRoundedRectangle(area, 10.0f, 1.0f);
}

void FilterEnvPanel::resized()
{
    auto floatArea = getLocalBounds().reduced(20).toFloat();
    // Layout: Filter controls on top left, Envelope controls on bottom
    // Visualizer in the middle/right of top row? or just centered?
    
    // Let's do:
    // Top Row: Filter Controls (Left) -- Visualizer (Right)
    // Bottom Row: ADSR Controls
    
    auto rowHeight = floatArea.getHeight() / 2.0f;
    
    const int labelHeight = 15;
    const int textBoxHeight = 20;
    
    auto layoutControl = [&](RotaryControl& ctrl, juce::Rectangle<float> bounds) {
        auto intBounds = bounds.toNearestInt();
        ctrl.label.setBounds(intBounds.removeFromTop(labelHeight));
        ctrl.slider.setBounds(intBounds.removeFromTop(intBounds.getHeight() - textBoxHeight));
    };

    // New Layout Logic for Top Row
    auto topRow = floatArea.removeFromTop(rowHeight);
    
    // Visualizer gets right 40%
    auto vizArea = topRow.removeFromRight(topRow.getWidth() * 0.4f).reduced(10.0f);
    envVisualizer->setBounds(vizArea.toNearestInt());
    
    // Filter knobs get the rest (left 60%)
    auto filterWidth = topRow.getWidth() / 2.0f;
    layoutControl(cutoff, topRow.removeFromLeft(filterWidth).reduced(20.0f, 5.0f));
    layoutControl(resonance, topRow.reduced(20.0f, 5.0f));
    
    auto envArea = floatArea;
    auto envWidth = envArea.getWidth() / 4.0f;
    layoutControl(attack, envArea.removeFromLeft(envWidth).reduced(10.0f, 5.0f));
    layoutControl(decay, envArea.removeFromLeft(envWidth).reduced(10.0f, 5.0f));
    layoutControl(sustain, envArea.removeFromLeft(envWidth).reduced(10.0f, 5.0f));
    layoutControl(release, envArea.reduced(10.0f, 5.0f));
}

} // namespace NEURONiK::UI
