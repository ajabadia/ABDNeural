/*
  ==============================================================================

    CustomUIComponents.h
    Created: 26 Jan 2026
    Description: Shared UI components and LookAndFeel for NEURONiK.
                 Follows DRY principle for knobs and modulation visualization.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdint>
#include "MidiLearner.h"

namespace NEURONiK::UI {

/**
 * A shared LookAndFeel for all knobs that supports real-time modulation display.
 * It looks for a "modValue" property in the slider to draw the arc.
 */
class SharedKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SharedKnobLookAndFeel()
    {
        setColour(juce::Slider::thumbColourId, juce::Colours::white);
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF005555));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
    {
        auto area = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(6.0f);
        auto diam = juce::jmin(area.getWidth(), area.getHeight());
        auto bounds = juce::Rectangle<float>(diam, diam).withCentre(area.getCentre());
        
        auto centreX = bounds.getCentreX();
        auto centreY = bounds.getCentreY();
        auto rotaryRange = rotaryEndAngle - rotaryStartAngle;
        auto angle = rotaryStartAngle + sliderPos * rotaryRange;

        // 1. Background Track
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.drawEllipse(bounds, 3.0f);

        // 2. Knob Body
        auto knobArea = bounds.reduced(4.0f);
        juce::ColourGradient knobGrad(juce::Colours::white.withAlpha(0.15f), centreX, knobArea.getY(),
                                     juce::Colours::black.withAlpha(0.4f), centreX, knobArea.getBottom(), false);
        g.setGradientFill(knobGrad);
        g.fillEllipse(knobArea);
        
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawEllipse(knobArea, 1.0f);

        // 3. Modulation Arc (Read from property as a pointer handle)
        auto* modAtom = reinterpret_cast<std::atomic<float>*>(static_cast<std::intptr_t>((juce::int64)slider.getProperties()["modValue"]));
        if (modAtom != nullptr)
        {
            auto range = slider.getRange();
            float currentMod = modAtom->load(std::memory_order_relaxed);
            float normMod = (float)((currentMod - range.getStart()) / range.getLength());
            auto modAngle = rotaryStartAngle + normMod * rotaryRange;

            juce::Path modPath;
            modPath.addArc(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), rotaryStartAngle, modAngle, true);
            
            g.setColour(juce::Colours::cyan.withAlpha(0.4f));
            g.strokePath(modPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            g.setColour(juce::Colours::cyan);
            g.strokePath(modPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 4. Pointer
        juce::Path p;
        p.addRectangle(-1.0f, -knobArea.getWidth() * 0.5f, 2.0f, knobArea.getWidth() * 0.3f);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.fillPath(p);
    }
};

/**
 * Common structure for a UI control, keeping all pieces together.
 */
struct RotaryControl {
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::unique_ptr<MidiLearner> midiLearner;
};

/**
 * Utility class for configuring controls without repeating boilerplate code.
 */
class UIUtils {
public:
    static void setupRotaryControl(juce::Component& parent,
                                  RotaryControl& ctrl, 
                                  const juce::String& paramID, 
                                  const juce::String& labelText,
                                  juce::AudioProcessorValueTreeState& vts,
                                  class NEURONiKProcessor& processor,
                                  juce::LookAndFeel& lnf,
                                  std::atomic<float>* modValue = nullptr)
    {
        ctrl.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);
        ctrl.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        ctrl.slider.setLookAndFeel(&lnf);
        // Store modulation source in property (using intptr_t for a safe pointer handle)
        if (modValue != nullptr)
            ctrl.slider.getProperties().set("modValue", static_cast<juce::int64>(reinterpret_cast<std::intptr_t>(modValue)));
            
        parent.addAndMakeVisible(ctrl.slider);

        ctrl.label.setText(labelText, juce::dontSendNotification);
        ctrl.label.setJustificationType(juce::Justification::centred);
        ctrl.label.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
        parent.addAndMakeVisible(ctrl.label);

        ctrl.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(vts, paramID, ctrl.slider);
        
        // 2-decimal precision (Override attachment/parameter text)
        ctrl.slider.textFromValueFunction = [](double v) { 
            return juce::String(v, 2); 
        };

        ctrl.midiLearner = std::make_unique<MidiLearner>(processor, ctrl.slider, paramID);
    }
};

/**
 * A reusable "Glass" container for grouping components.
 */
class GlassBox : public juce::Component
{
public:
    GlassBox(const juce::String& name = "") : title(name) 
    {
        titleLabel.setText(title, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        titleLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::cyan.withAlpha(0.7f));
        addAndMakeVisible(titleLabel);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        
        // Background Glass effect
        juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.06f), 0, 0,
                                      juce::Colours::white.withAlpha(0.01f), 0, area.getHeight(), false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(area, 8.0f);
        
        // Border
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawRoundedRectangle(area, 8.0f, 1.0f);

        // Header line if title exists
        if (title.isNotEmpty())
        {
            auto lineY = 22.0f;
            g.setColour(juce::Colours::cyan.withAlpha(0.2f));
            g.drawLine(8.0f, lineY, area.getWidth() - 8.0f, lineY, 1.0f);
        }
    }

    void resized() override
    {
        if (title.isNotEmpty())
            titleLabel.setBounds(10, 2, getWidth() - 20, 18);
    }

    juce::Rectangle<int> getContentArea() 
    { 
        return getLocalBounds().reduced(8).withTrimmedTop(title.isNotEmpty() ? 20 : 0); 
    }

private:
    juce::String title;
    juce::Label titleLabel;
};

} // namespace NEURONiK::UI
