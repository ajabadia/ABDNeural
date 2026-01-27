#include "FXPanel.h"
#include "../../Main/NEURONiKProcessor.h"
#include "../../State/ParameterDefinitions.h"

namespace NEURONiK::UI {

using namespace NEURONiK::State;

FXPanel::FXPanel(NEURONiKProcessor& p)
    : processor(p), vts(p.getAPVTS())
{
    using namespace NEURONiK::State;

    addAndMakeVisible(saturationBox);
    addAndMakeVisible(delayBox);
    addAndMakeVisible(chorusBox);
    addAndMakeVisible(reverbBox);
    addAndMakeVisible(masterBox);

    setupControl(saturation,    IDs::fxSaturation,     "DRIVE", saturationBox, &processor.uiSaturation);
    
    // Delay
    setupControl(delayTime,     IDs::fxDelayTime,      "TIME", delayBox, &processor.uiDelayTime);
    setupControl(delayFeedback, IDs::fxDelayFeedback,  "FEEDBACK", delayBox, &processor.uiDelayFB);
    setupChoice(delaySync,      IDs::fxDelaySync,      "SYNC", delayBox);
    setupChoice(delayDivision,  IDs::fxDelayDivision,  "DIV", delayBox);

    // Chorus
    setupControl(chorusRate,    IDs::fxChorusRate,     "RATE", chorusBox);
    setupControl(chorusDepth,   IDs::fxChorusDepth,    "DEPTH", chorusBox);
    setupControl(chorusMix,     IDs::fxChorusMix,      "MIX", chorusBox);

    // Reverb
    setupControl(reverbSize,    IDs::fxReverbSize,     "ROOM", reverbBox);
    setupControl(reverbDamping, IDs::fxReverbDamping,  "DAMP", reverbBox);
    setupControl(reverbWidth,   IDs::fxReverbWidth,    "WIDTH", reverbBox);
    setupControl(reverbMix,     IDs::fxReverbMix,      "MIX", reverbBox);

    setupControl(masterLevel,   IDs::masterLevel,      "LEVEL", masterBox);

    saturationBox.addAndMakeVisible(saturationLed);
    delayBox.addAndMakeVisible(delayLed);
    chorusBox.addAndMakeVisible(chorusLed);
    reverbBox.addAndMakeVisible(reverbLed);

    startTimer(50); // 20fps for LEDs
}

void FXPanel::setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText, juce::Component& parent, std::atomic<float>* modValue)
{
    UIUtils::setupRotaryControl(parent, ctrl, paramID, labelText, vts, processor, sharedLNF, modValue);
}

void FXPanel::setupChoice(ChoiceControl& ctrl, const juce::String& paramID, const juce::String& labelText, juce::Component& parent)
{
    ctrl.label.setText(labelText, juce::dontSendNotification);
    ctrl.label.setJustificationType(juce::Justification::centred);
    ctrl.label.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Bold")));
    parent.addAndMakeVisible(ctrl.label);

    auto* param = vts.getParameter(paramID);
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(param))
        ctrl.comboBox.addItemList(choice->choices, 1);

    parent.addAndMakeVisible(ctrl.comboBox);
    ctrl.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(vts, paramID, ctrl.comboBox);
}

void FXPanel::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void FXPanel::timerCallback()
{
    saturationLed.setValue(vts.getRawParameterValue(IDs::fxSaturation)->load());
    delayLed.setValue(vts.getRawParameterValue(IDs::fxDelayFeedback)->load() * 1.05f); // Boost slightly for visibility
    chorusLed.setValue(vts.getRawParameterValue(IDs::fxChorusMix)->load());
    reverbLed.setValue(vts.getRawParameterValue(IDs::fxReverbMix)->load());
}

void FXPanel::resized()
{
    auto area = getLocalBounds().reduced(10);
    
    auto colWidth = area.getWidth() / 5;
    
    saturationBox.setBounds(area.removeFromLeft(colWidth).reduced(3));
    delayBox.setBounds(area.removeFromLeft(colWidth).reduced(3));
    chorusBox.setBounds(area.removeFromLeft(colWidth).reduced(3));
    reverbBox.setBounds(area.removeFromLeft(colWidth).reduced(3));
    masterBox.setBounds(area.reduced(3));

    auto layoutRotary = [&](RotaryControl& ctrl, juce::Rectangle<int> bounds) {
        ctrl.label.setBounds(bounds.removeFromTop(15));
        ctrl.slider.setBounds(bounds);
    };

    auto layoutLed = [&](LedIndicator& led, juce::Component& box) {
        led.setBounds(box.getWidth() - 20, 6, 12, 12);
    };

    layoutLed(saturationLed, saturationBox);
    layoutLed(delayLed, delayBox);
    layoutLed(chorusLed, chorusBox);
    layoutLed(reverbLed, reverbBox);

    // Saturation Content
    {
        auto c = saturationBox.getContentArea();
        // Increased knob size to match delay/reverb (120 height instead of 80)
        layoutRotary(saturation, c.withSizeKeepingCentre(c.getWidth(), 120));
    }

    // Delay Content
    {
        auto c = delayBox.getContentArea();
        auto top = c.removeFromTop(c.getHeight() / 2);
        
        // Vertical distribution for Sync and Div as requested
        auto syncArea = top.removeFromTop(top.getHeight() / 2);
        delaySync.label.setBounds(syncArea.removeFromTop(12));
        delaySync.comboBox.setBounds(syncArea.withSizeKeepingCentre(syncArea.getWidth(), 22).reduced(10, 0));
        
        delayDivision.label.setBounds(top.removeFromTop(12));
        delayDivision.comboBox.setBounds(top.withSizeKeepingCentre(top.getWidth(), 22).reduced(10, 0));
        
        auto bot = c;
        auto knobW = bot.getWidth() / 2;
        layoutRotary(delayTime, bot.removeFromLeft(knobW));
        layoutRotary(delayFeedback, bot);
    }

    // Chorus Content
    {
        auto c = chorusBox.getContentArea();
        auto h = c.getHeight() / 3;
        layoutRotary(chorusRate, c.removeFromTop(h));
        layoutRotary(chorusDepth, c.removeFromTop(h));
        layoutRotary(chorusMix, c);
    }

    // Reverb Content
    {
        auto c = reverbBox.getContentArea();
        auto top = c.removeFromTop(c.getHeight() / 2);
        layoutRotary(reverbSize, top.removeFromLeft(top.getWidth() / 2));
        layoutRotary(reverbDamping, top);
        layoutRotary(reverbWidth, c.removeFromLeft(c.getWidth() / 2));
        layoutRotary(reverbMix, c);
    }

    // Master Content
    {
        auto c = masterBox.getContentArea();
        layoutRotary(masterLevel, c.withSizeKeepingCentre(c.getWidth(), 80));
    }
}

} // namespace NEURONiK::UI
