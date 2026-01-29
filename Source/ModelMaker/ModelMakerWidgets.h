/*
  ==============================================================================
    ModelMakerWidgets.h
  ==============================================================================
*/

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace NEURONiK {
namespace ModelMaker {

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
        float scale = area.getHeight() / 200.0f; // Guessing a base height for scale
        if (scale < 0.5f) scale = 0.5f;

        juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.06f), 0, 0,
                                      juce::Colours::white.withAlpha(0.01f), 0, area.getHeight(), false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(area, 8.0f * scale);
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawRoundedRectangle(area, 8.0f * scale, 1.0f * scale);

        if (title.isNotEmpty())
        {
            float lineY = 22.0f * (area.getHeight() / (area.getHeight() > 0 ? area.getHeight() : 1.0f)); 
            // Better: use fixed margin for title area
            lineY = juce::jmin(30.0f, area.getHeight() * 0.15f);
            g.setColour(juce::Colours::cyan.withAlpha(0.2f));
            g.drawLine(8.0f * scale, lineY, area.getWidth() - 8.0f * scale, lineY, 1.0f * scale);
            
            titleLabel.setFont(juce::Font(juce::FontOptions(juce::jmax(10.0f, lineY * 0.6f)).withStyle("Bold")));
        }
    }

    void resized() override
    {
        if (title.isNotEmpty())
        {
            float titleH = juce::jmin(30.0f, getHeight() * 0.15f);
            titleLabel.setBounds(10, 2, getWidth() - 20, (int)titleH - 4);
        }
    }

private:
    juce::String title;
    juce::Label titleLabel;
};

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
        g.setColour(juce::Colour(0xFF151515).withAlpha(0.8f));
        g.fillRoundedRectangle(area, cornerSize);
        float borderAlpha = shouldDrawButtonAsDown ? 0.9f : (shouldDrawButtonAsHighlighted ? 0.6f : 0.3f);
        g.setColour(juce::Colours::cyan.withAlpha(borderAlpha));
        g.drawRoundedRectangle(area.reduced(0.5f), cornerSize, 1.0f);
        if (shouldDrawButtonAsDown)
        {
            g.setColour(juce::Colours::cyan.withAlpha(0.1f));
            g.fillRoundedRectangle(area.reduced(1.0f), cornerSize);
        }
        g.setColour(shouldDrawButtonAsDown ? juce::Colours::white : juce::Colours::cyan.withAlpha(0.8f));
        g.setFont(juce::Font(juce::FontOptions(area.getHeight() * 0.4f).withStyle("Bold")));
        g.drawText(getButtonText(), area, juce::Justification::centred, false);
    }
};

} // namespace ModelMaker
} // namespace NEURONiK
