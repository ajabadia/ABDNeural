/*
  ==============================================================================

    MidiMappingManager.cpp
    Created: 27 Jan 2026

  ==============================================================================
*/

#include "MidiMappingManager.h"
#include "../State/ParameterDefinitions.h"

namespace NEURONiK::Main {

MidiMappingManager::MidiMappingManager(juce::AudioProcessorValueTreeState& vts) 
    : apvts(vts)
{
    getLearnableParams(); // Force initialization outside audio thread
    for (auto& slot : ccToIndex) slot.store(-1);
    resetToDefaults();
}

// RT-safe parameter list
const juce::StringArray& MidiMappingManager::getLearnableParams()
{
    static juce::StringArray params;
    if (params.size() == 0)
    {
        namespace P = NEURONiK::State::IDs;
        params.add(P::filterCutoff); params.add(P::filterRes);
        params.add(P::oscLevel); params.add(P::envAttack);
        params.add(P::envRelease); params.add(P::morphX);
        params.add(P::morphY); params.add(P::oscInharmonicity);
        params.add(P::oscRoughness); params.add(P::resonatorParity);
        params.add(P::resonatorShift); params.add(P::resonatorRolloff);
        params.add(P::filterEnvAmount); params.add(P::fxSaturation);
        params.add(P::fxChorusMix); params.add(P::fxDelayTime);
        params.add(P::fxReverbMix); params.add(P::fxDelayFeedback);
        params.add(P::oscExciteNoise); params.add(P::excitationColor);
        params.add(P::impulseMix); params.add(P::resonatorRes);
        params.add(P::masterLevel);
    }
    return params;
}

int MidiMappingManager::getParamIndex(const juce::String& paramID)
{
    const auto& params = getLearnableParams();
    for (int i = 0; i < params.size(); ++i)
        if (params[i] == paramID) return i;
    return -1;
}

void MidiMappingManager::setMapping(const juce::String& paramID, int ccNumber)
{
    int paramIdx = getParamIndex(paramID);
    if (paramIdx == -1) return;
    if (ccNumber < 0 || ccNumber > 127) return;

    // 1. Clear any existing mapping for this parameter in other CCs
    for (int i = 0; i < 128; ++i)
    {
        if (ccToIndex[i].load() == paramIdx)
            ccToIndex[i].store(-1);
    }

    // 2. Clear any existing mapping for this CC (Conflict resolution)
    ccToIndex[ccNumber].store(paramIdx);
}

void MidiMappingManager::clearMapping(const juce::String& paramID)
{
    int idx = getParamIndex(paramID);
    if (idx == -1) return;

    for (int i = 0; i < 128; ++i)
        if (ccToIndex[i].load() == idx)
            ccToIndex[i].store(-1);
}

int MidiMappingManager::getCCForParam(const juce::String& paramID) const
{
    int idx = getParamIndex(paramID);
    if (idx == -1) return -1;

    for (int i = 0; i < 128; ++i)
        if (ccToIndex[i].load() == idx)
            return i;

    return -1;
}

juce::String MidiMappingManager::getParamForCC(int ccNumber) const
{
    if (ccNumber >= 0 && ccNumber <= 127)
    {
        int idx = ccToIndex[ccNumber].load();
        if (idx >= 0 && idx < (int)getLearnableParams().size())
            return getLearnableParams()[idx];
    }
    return {};
}

std::map<int, juce::String> MidiMappingManager::getMappings() const
{
    std::map<int, juce::String> m;
    const auto& params = getLearnableParams();
    for (int i = 0; i < 128; ++i)
    {
        int idx = ccToIndex[i].load();
        if (idx >= 0 && idx < (int)params.size())
            m[i] = params[idx];
    }
    return m;
}

void MidiMappingManager::resetToDefaults()
{
    for (auto& slot : ccToIndex) slot.store(-1);

    namespace P = NEURONiK::State::IDs;
    setMapping(P::filterCutoff, 74);
    setMapping(P::filterRes, 71);
    setMapping(P::oscLevel, 7);
    setMapping(P::envAttack, 73);
    setMapping(P::envRelease, 72);
    setMapping(P::morphX, 12);
    setMapping(P::morphY, 13);
    setMapping(P::oscInharmonicity, 14);
    setMapping(P::oscRoughness, 15);
    setMapping(P::resonatorParity, 16);
    setMapping(P::resonatorShift, 17);
    setMapping(P::resonatorRolloff, 18);
    setMapping(P::filterEnvAmount, 79);
    setMapping(P::fxSaturation, 91);
    setMapping(P::fxChorusMix, 93);
    setMapping(P::fxDelayTime, 94);
    setMapping(P::fxReverbMix, 95);

    // Neurotik Specific
    setMapping(P::oscExciteNoise, 20);
    setMapping(P::excitationColor, 21);
    setMapping(P::impulseMix, 22);
    setMapping(P::resonatorRes, 23);
}

void MidiMappingManager::saveToValueTree(juce::ValueTree& v)
{
    juce::ValueTree midiNode("MIDIMAPPINGS");
    const auto& params = getLearnableParams();

    for (int i = 0; i < 128; ++i)
    {
        int idx = ccToIndex[i].load();
        if (idx >= 0 && idx < (int)params.size())
        {
            juce::ValueTree m("MAP");
            m.setProperty("cc", i, nullptr);
            m.setProperty("param", params[idx], nullptr);
            midiNode.appendChild(m, nullptr);
        }
    }
    v.getOrCreateChildWithName("MIDIMAPPINGS", nullptr) = midiNode;
}

void MidiMappingManager::loadFromValueTree(const juce::ValueTree& v)
{
    auto midiNode = v.getChildWithName("MIDIMAPPINGS");
    if (midiNode.isValid())
    {
        for (auto& slot : ccToIndex) slot.store(-1);

        for (int i = 0; i < midiNode.getNumChildren(); ++i)
        {
            auto m = midiNode.getChild(i);
            int cc = m.getProperty("cc");
            juce::String paramID = m.getProperty("param");
            setMapping(paramID, cc);
        }
    }
}

bool MidiMappingManager::hasConflict(int /*ccNumber*/) const
{
    return false;
}

} // namespace NEURONiK::Main
