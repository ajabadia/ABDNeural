#include "FXPanel.h"
#include "../../Main/NEURONiKProcessor.h"
#include "../../State/ParameterDefinitions.h"

namespace NEURONiK::UI {

FXPanel::FXPanel(NEURONiKProcessor& p)
    : processor(p), vts(p.getAPVTS())
{
    using namespace NEURONiK::State;

    setupControl(saturation,    IDs::fxSaturation,     "DRIVE");
    
    // Delay
    setupControl(delayTime,     IDs::fxDelayTime,      "TIME");
    setupControl(delayFeedback, IDs::fxDelayFeedback,  "FEEDBACK");
    setupChoice(delaySync,      IDs::fxDelaySync,      "SYNC");
    setupChoice(delayDivision,  IDs::fxDelayDivision,  "DIV");

    // Chorus
    setupControl(chorusRate,    IDs::fxChorusRate,     "CH RATE");
    setupControl(chorusDepth,   IDs::fxChorusDepth,    "DEPTH");
    setupControl(chorusMix,     IDs::fxChorusMix,      "MIX");

    // Reverb
    setupControl(reverbSize,    IDs::fxReverbSize,     "ROOM");
    setupControl(reverbDamping, IDs::fxReverbDamping,  "DAMP");
    setupControl(reverbWidth,   IDs::fxReverbWidth,    "WIDTH");
    setupControl(reverbMix,     IDs::fxReverbMix,      "REVERB");

    setupControl(masterLevel,   IDs::masterLevel,      "MASTER");
}

void FXPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText)
{
    ctrl.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    ctrl.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);
    ctrl.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(ctrl.slider);
    
    ctrl.label.setText(labelText, juce::dontSendNotification);
    ctrl.label.setJustificationType(juce::Justification::centred);
    ctrl.label.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    addAndMakeVisible(ctrl.label);
    
    ctrl.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(vts, paramID, ctrl.slider);
    ctrl.midiLearner = std::make_unique<MidiLearner>(processor, ctrl.slider, paramID);
}

void FXPanel::setupChoice(ChoiceControl& ctrl, const juce::String& paramID, const juce::String& labelText)
{
    ctrl.label.setText(labelText, juce::dontSendNotification);
    ctrl.label.setJustificationType(juce::Justification::centred);
    ctrl.label.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    addAndMakeVisible(ctrl.label);

    auto* param = vts.getParameter(paramID);
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(param))
        ctrl.comboBox.addItemList(choice->choices, 1);

    addAndMakeVisible(ctrl.comboBox);
    ctrl.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(vts, paramID, ctrl.comboBox);
}

void FXPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.05f), 0, 0,
                                  juce::Colours::white.withAlpha(0.01f), 0, area.getHeight(), false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(area, 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawRoundedRectangle(area, 10.0f, 1.0f);
}

void FXPanel::resized()
{
    auto area = getLocalBounds().reduced(15);
    auto masterArea = area.removeFromRight(80);
    
    // Layout Master
    masterLevel.label.setBounds(masterArea.removeFromTop(20));
    masterLevel.slider.setBounds(masterArea.removeFromTop(masterArea.getHeight() - 20));

    auto colWidth = area.getWidth() / 4;

    // Helper to layout vertical stack
    auto layoutStack = [&](juce::Rectangle<int> col, std::vector<juce::Component*> comps) {
        int h = col.getHeight() / static_cast<int>(comps.size());
        for (auto* c : comps) {
            c->setBounds(col.removeFromTop(h).reduced(5, 2));
        }
    };

    // Col 1: Saturation
    auto satCol = area.removeFromLeft(colWidth);
    saturation.label.setBounds(satCol.removeFromTop(20));
    saturation.slider.setBounds(satCol.reduced(10, 20));

    // Col 2: Delay
    auto delayCol = area.removeFromLeft(colWidth);
    {
        auto top = delayCol.removeFromTop(delayCol.getHeight() / 2);
        auto bot = delayCol;
        
        auto topLeft = top.removeFromLeft(top.getWidth() / 2);
        delaySync.label.setBounds(topLeft.removeFromTop(15));
        delaySync.comboBox.setBounds(topLeft.reduced(2));
        
        delayDivision.label.setBounds(top.removeFromTop(15));
        delayDivision.comboBox.setBounds(top.reduced(2));
        
        auto botLeft = bot.removeFromLeft(bot.getWidth() / 2);
        delayTime.label.setBounds(botLeft.removeFromTop(15));
        delayTime.slider.setBounds(botLeft);
        
        delayFeedback.label.setBounds(bot.removeFromTop(15));
        delayFeedback.slider.setBounds(bot);
    }

    // Col 3: Chorus
    auto chorusCol = area.removeFromLeft(colWidth);
    chorusRate.label.setBounds(chorusCol.removeFromTop(15));
    chorusRate.slider.setBounds(chorusCol.removeFromTop(chorusCol.getHeight() / 3));
    chorusDepth.label.setBounds(chorusCol.removeFromTop(15));
    chorusDepth.slider.setBounds(chorusCol.removeFromTop(chorusCol.getHeight() / 2));
    chorusMix.label.setBounds(chorusCol.removeFromTop(15));
    chorusMix.slider.setBounds(chorusCol);

    // Col 4: Reverb
    auto reverbCol = area;
    {
        auto r1 = reverbCol.removeFromTop(reverbCol.getHeight() / 2);
        auto r2 = reverbCol;
        
        auto r11 = r1.removeFromLeft(r1.getWidth() / 2);
        reverbSize.label.setBounds(r11.removeFromTop(15));
        reverbSize.slider.setBounds(r11);
        reverbDamping.label.setBounds(r1.removeFromTop(15));
        reverbDamping.slider.setBounds(r1);
        
        auto r21 = r2.removeFromLeft(r2.getWidth() / 2);
        reverbWidth.label.setBounds(r21.removeFromTop(15));
        reverbWidth.slider.setBounds(r21);
        reverbMix.label.setBounds(r2.removeFromTop(15));
        reverbMix.slider.setBounds(r2);
    }
}

} // namespace NEURONiK::UI
