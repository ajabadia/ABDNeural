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
#include "../Main/ModulationTargets.h"
#include "ThemeManager.h"
#include "MidiLearner.h"

// Forward declaration to break circular dependency
class NEURONiKProcessor;

namespace NEURONiK::UI {

/**
 * A specialized slider that holds a reference to a modulation target index.
 */
class ModulatedSlider : public juce::Slider
{
public:
    ModulatedSlider() = default;
    
    void setModulationTarget(::NEURONiK::ModulationTarget target, ::NEURONiKProcessor* proc);
    std::atomic<float>* getModulationAtomic() const;

private:
    ::NEURONiK::ModulationTarget modTarget = ::NEURONiK::ModulationTarget::Count;
    ::NEURONiKProcessor* processor = nullptr;
};

/**
 * A shared LookAndFeel for all knobs that supports real-time modulation display.
 */
class SharedKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SharedKnobLookAndFeel()
    {
        const auto& theme = ThemeManager::getCurrentTheme();
        setColour(juce::Slider::thumbColourId, theme.knobPointer);
        setColour(juce::Slider::rotarySliderFillColourId, theme.accent.withAlpha(0.3f));
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
        g.setColour(ThemeManager::getCurrentTheme().background.withAlpha(0.3f));
        g.drawEllipse(bounds, 3.0f);

        // 2. Knob Body
        auto knobArea = bounds.reduced(4.0f);
        const auto& theme = ThemeManager::getCurrentTheme();
        juce::ColourGradient knobGrad(theme.text.withAlpha(0.15f), centreX, knobArea.getY(),
                                     theme.background.withAlpha(0.4f), centreX, knobArea.getBottom(), false);
        g.setGradientFill(knobGrad);
        g.fillEllipse(knobArea);
        
        g.setColour(theme.text.withAlpha(0.1f));
        g.drawEllipse(knobArea, 1.0f);

        // 3. Modulation Arc (Read from specialized slider)
        std::atomic<float>* modAtom = nullptr;
        if (auto* modSlider = dynamic_cast<ModulatedSlider*>(&slider))
            modAtom = modSlider->getModulationAtomic();

        if (modAtom != nullptr)
        {
            auto range = slider.getRange();
            float currentMod = modAtom->load(std::memory_order_relaxed);
            float normMod = (float)((currentMod - range.getStart()) / range.getLength());
            auto modAngle = rotaryStartAngle + normMod * rotaryRange;

            juce::Path modPath;
            modPath.addArc(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), rotaryStartAngle, modAngle, true);
            
            const auto& currentTheme = ThemeManager::getCurrentTheme();
            g.setColour(currentTheme.modulationRing.withAlpha(0.4f));
            g.strokePath(modPath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            g.setColour(currentTheme.modulationRing);
            g.strokePath(modPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 4. Pointer
        float pointerAlpha = slider.isEnabled() ? 0.8f : 0.2f;
        juce::Path p;
        p.addRectangle(-1.0f, -knobArea.getWidth() * 0.5f, 2.0f, knobArea.getWidth() * 0.3f);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(ThemeManager::getCurrentTheme().knobPointer.withAlpha(pointerAlpha));
        g.fillPath(p);

        // 5. Disabled Overlay
        if (!slider.isEnabled())
        {
            g.setColour(ThemeManager::getCurrentTheme().background.withAlpha(0.6f));
            g.fillEllipse(bounds);
        }
    }
};

/**
 * A LookAndFeel for vertical faders/sliders.
 */
class VerticalSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VerticalSliderLookAndFeel()
    {
        const auto& theme = ThemeManager::getCurrentTheme();
        setColour(juce::Slider::thumbColourId, theme.knobPointer);
        setColour(juce::Slider::trackColourId, theme.accent.withAlpha(0.5f));
        setColour(juce::Slider::backgroundColourId, theme.background.withAlpha(0.3f));
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        juce::ignoreUnused(minSliderPos, maxSliderPos);
        auto trackWidth = 4.0f;
        auto thumbHeight = 16.0f;
        auto thumbWidth = (float)width * 0.8f;
        
        juce::Point<float> startPoint((float)x + (float)width * 0.5f, (float)height + (float)y - 5.0f);
        juce::Point<float> endPoint(startPoint.x, (float)y + 5.0f);
        
        // 1. Track Background
        juce::Path backgroundTrack;
        backgroundTrack.startNewSubPath(startPoint);
        backgroundTrack.lineTo(endPoint);
        g.setColour(slider.findColour(juce::Slider::backgroundColourId));
        g.strokePath(backgroundTrack, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

        // 2. Value Track
        juce::Point<float> kStart = startPoint;
        juce::Point<float> kEnd = { startPoint.x, sliderPos };
        
        if (slider.isVertical())
        {
             kEnd = { startPoint.x, sliderPos };
        }
        
        juce::Path valueTrack;
        valueTrack.startNewSubPath(kStart);
        valueTrack.lineTo(kEnd);
        g.setColour(slider.findColour(juce::Slider::trackColourId));
        g.strokePath(valueTrack, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
        
        // 3. Mod visualization (if applicable)
        if (auto* modSlider = dynamic_cast<const ModulatedSlider*>(&slider))
        {
             if (auto* modAtom = modSlider->getModulationAtomic())
             {
                 float modVal = modAtom->load(std::memory_order_relaxed);
                 if (std::abs(modVal) > 0.001f)
                 {
                      float sliderLen = slider.isVertical() ? (float)height : (float)width;
                      float targetPos = sliderPos - (modVal * sliderLen);
                      
                      // Clamp
                      float minLimit = (float)y;
                      float maxLimit = (float)y + (float)height;
                      if (!slider.isVertical())
                      {
                          minLimit = (float)x;
                          maxLimit = (float)x + (float)width;
                      }
                      targetPos = juce::jlimit(minLimit, maxLimit, targetPos);

                      juce::Path modPath;
                      if (slider.isVertical())
                      {
                          modPath.startNewSubPath(kEnd.x + 6.0f, kEnd.y); 
                          modPath.lineTo(kEnd.x + 6.0f, targetPos);
                      }
                      else
                      {
                          modPath.startNewSubPath(kEnd.x, kEnd.y + 6.0f);
                          modPath.lineTo(targetPos, kEnd.y + 6.0f);
                      }
                      
                      const auto& theme = ThemeManager::getCurrentTheme();
                      g.setColour(theme.accent.withAlpha(0.8f));
                      g.strokePath(modPath, { 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
                 }
             }
        }

        // 4. Thumb
        auto thumbRect = juce::Rectangle<float>(static_cast<float>(width), thumbHeight).withCentre(kEnd);
        thumbRect = thumbRect.withWidth(thumbWidth); // Adjust width
        
        const auto& theme = ThemeManager::getCurrentTheme();
        g.setColour(theme.surface.withAlpha(0.9f));
        g.fillRoundedRectangle(thumbRect, 2.0f);
        
        g.setColour(theme.accent);
        g.drawRoundedRectangle(thumbRect, 2.0f, 1.0f);
        
        // Thumb grip lines
        g.setColour(theme.text.withAlpha(0.3f));
        g.drawLine(thumbRect.getX() + 4.0f, thumbRect.getCentreY(), thumbRect.getRight() - 4.0f, thumbRect.getCentreY(), 1.0f);
    }
};

/**
 * Common structure for a UI control, keeping all pieces together.
 */
struct RotaryControl {
    ModulatedSlider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::unique_ptr<MidiLearner> midiLearner;
};

/**
 * Common structure for a Vertical Slider control.
 */
struct VerticalSliderControl {
    ModulatedSlider slider;
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
                                  class ::NEURONiKProcessor& processor,
                                  juce::LookAndFeel& lnf,
                                  ::NEURONiK::ModulationTarget modTarget = ::NEURONiK::ModulationTarget::Count)
    {
        ctrl.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);
        ctrl.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        ctrl.slider.setLookAndFeel(&lnf);
        
        // Pass modulation target to specialized slider
        if (modTarget != ::NEURONiK::ModulationTarget::Count)
            ctrl.slider.setModulationTarget(modTarget, &processor);
            
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

    static void setupVerticalSlider(juce::Component& parent,
                                    VerticalSliderControl& ctrl,
                                    const juce::String& paramID,
                                    const juce::String& labelText,
                                    juce::AudioProcessorValueTreeState& vts,
                                    class ::NEURONiKProcessor& processor,
                                    juce::LookAndFeel& lnf,
                                    ::NEURONiK::ModulationTarget modTarget = ::NEURONiK::ModulationTarget::Count)
    {
        ctrl.slider.setSliderStyle(juce::Slider::LinearVertical);
        ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);
        ctrl.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        ctrl.slider.setLookAndFeel(&lnf);
        
        if (modTarget != ::NEURONiK::ModulationTarget::Count)
            ctrl.slider.setModulationTarget(modTarget, &processor);
            
        parent.addAndMakeVisible(ctrl.slider);

        ctrl.label.setText(labelText, juce::dontSendNotification);
        ctrl.label.setJustificationType(juce::Justification::centred);
        ctrl.label.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
        parent.addAndMakeVisible(ctrl.label);

        ctrl.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(vts, paramID, ctrl.slider);
        
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
        titleLabel.setColour(juce::Label::textColourId, ThemeManager::getCurrentTheme().accent.withAlpha(0.7f));
        addAndMakeVisible(titleLabel);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        
        // Background Glass effect
        const auto& theme = ThemeManager::getCurrentTheme();
        juce::ColourGradient glassGrad(theme.text.withAlpha(0.06f), 0, 0,
                                      theme.text.withAlpha(0.01f), 0, area.getHeight(), false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(area, 8.0f);
        
        // Border
        g.setColour(theme.text.withAlpha(0.1f));
        g.drawRoundedRectangle(area, 8.0f, 1.0f);

        // Header line if title exists
        if (title.isNotEmpty())
        {
            auto lineY = 22.0f;
            g.setColour(ThemeManager::getCurrentTheme().accent.withAlpha(0.2f));
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

/**
 * A standardized "Neural Glass" button for NEURONiK.
 */
class CustomButton : public juce::TextButton
{
public:
    CustomButton(const juce::String& name = "") : juce::TextButton(name) 
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto area = getLocalBounds().toFloat();
        auto cornerSize = 4.0f;

        // 1. Background (Matte Dark Glass)
        const auto& theme = ThemeManager::getCurrentTheme();
        g.setColour(theme.surface.withAlpha(0.8f));
        g.fillRoundedRectangle(area, cornerSize);

        // 2. Dynamic Border
        float borderAlpha = shouldDrawButtonAsDown ? 0.9f : (shouldDrawButtonAsHighlighted ? 0.6f : 0.3f);
        g.setColour(theme.accent.withAlpha(borderAlpha));
        g.drawRoundedRectangle(area.reduced(0.5f), cornerSize, 1.0f);

        // 3. Inner Glow on press
        if (shouldDrawButtonAsDown)
        {
            g.setColour(theme.accent.withAlpha(0.1f));
            g.fillRoundedRectangle(area.reduced(1.0f), cornerSize);
        }

        // 4. Text / Label
        g.setColour(shouldDrawButtonAsDown ? theme.text : theme.accent.withAlpha(0.8f));
        g.setFont(juce::Font(juce::FontOptions(area.getHeight() * 0.5f).withStyle("Bold")));
        g.drawText(getButtonText(), area, juce::Justification::centred, false);
    }
};

/**
 * A modular LED indicator component with a glow effect.
 */
class LedIndicator : public juce::Component
{
public:
    LedIndicator(juce::Colour color = juce::Colour()) : ledColor(color) 
    {
        if (ledColor == juce::Colour())
            ledColor = ThemeManager::getCurrentTheme().accent;
    }

    void setValue(float newValue) 
    { 
        if (value != newValue)
        {
            value = newValue; 
            repaint(); 
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced(1.0f);
        auto centre = area.getCentre();
        auto radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f;

        // 1. Off/Dim background
        g.setColour(ledColor.withAlpha(0.1f));
        g.fillEllipse(area);

        if (value > 0.001f)
        {
            float alpha = juce::jlimit(0.0f, 1.0f, value);
            
            // 2. Glow
            juce::ColourGradient glow(ledColor.withAlpha(0.4f * alpha), centre.x, centre.y,
                                     ledColor.withAlpha(0.0f), centre.x + radius * 2.0f, centre.y, true);
            g.setGradientFill(glow);
            g.fillEllipse(area.expanded(2.5f));

            // 3. Core
            g.setColour(ledColor.withAlpha(0.8f * alpha));
            g.fillEllipse(area.reduced(1.5f));
            
            // 4. Center Shine
            g.setColour(ThemeManager::getCurrentTheme().text.withAlpha(0.4f * alpha));
            g.fillEllipse(juce::Rectangle<float>(radius * 0.4f, radius * 0.4f).withCentre(centre.translated(-radius*0.25f, -radius*0.25f)));
        }

        // Border
        g.setColour(ThemeManager::getCurrentTheme().text.withAlpha(0.15f));
        g.drawEllipse(area, 0.8f);
    }

private:
    juce::Colour ledColor;
    float value = 0.0f;
};

} // namespace NEURONiK::UI
