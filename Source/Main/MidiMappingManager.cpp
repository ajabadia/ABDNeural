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
    resetToDefaults();
}

void MidiMappingManager::setMapping(const juce::String& paramID, int ccNumber)
{
    if (paramID.isEmpty()) return;

    // 1. Remove this param from any previous CC
    if (paramToCC.count(paramID))
    {
        int oldCC = paramToCC[paramID];
        ccToParam.erase(oldCC);
    }

    // 2. Remove this CC from any previous param (Conflict Resolution)
    if (ccNumber >= 0 && ccNumber <= 127)
    {
        if (ccToParam.count(ccNumber))
        {
            juce::String oldParam = ccToParam[ccNumber];
            paramToCC.erase(oldParam);
        }

        ccToParam[ccNumber] = paramID;
        paramToCC[paramID] = ccNumber;
    }
    else
    {
        paramToCC.erase(paramID);
    }
}

void MidiMappingManager::clearMapping(const juce::String& paramID)
{
    if (paramToCC.count(paramID))
    {
        int cc = paramToCC[paramID];
        ccToParam.erase(cc);
        paramToCC.erase(paramID);
    }
}

int MidiMappingManager::getCCForParam(const juce::String& paramID) const
{
    if (paramToCC.count(paramID))
        return paramToCC.at(paramID);
    return -1;
}

juce::String MidiMappingManager::getParamForCC(int ccNumber) const
{
    if (ccToParam.count(ccNumber))
        return ccToParam.at(ccNumber);
    return "";
}

void MidiMappingManager::resetToDefaults()
{
    ccToParam.clear();
    paramToCC.clear();

    namespace P = NEURONiK::State::IDs;

    // Standard mappings
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
}

void MidiMappingManager::saveToValueTree(juce::ValueTree& v)
{
    juce::ValueTree midiNode("MIDIMAPPINGS");
    for (auto const& [cc, param] : ccToParam)
    {
        juce::ValueTree m("MAP");
        m.setProperty("cc", cc, nullptr);
        m.setProperty("param", param, nullptr);
        midiNode.appendChild(m, nullptr);
    }
    v.getOrCreateChildWithName("MIDIMAPPINGS", nullptr) = midiNode;
}

void MidiMappingManager::loadFromValueTree(const juce::ValueTree& v)
{
    auto midiNode = v.getChildWithName("MIDIMAPPINGS");
    if (midiNode.isValid())
    {
        ccToParam.clear();
        paramToCC.clear();
        for (int i = 0; i < midiNode.getNumChildren(); ++i)
        {
            auto m = midiNode.getChild(i);
            int cc = m.getProperty("cc");
            juce::String param = m.getProperty("param");
            setMapping(param, cc);
        }
    }
}

bool MidiMappingManager::hasConflict(int ccNumber) const
{
    // Our setMapping logic prevents multi-assignment to same CC,
    // so a conflict only exists if we were to allow multi-assignment.
    // For now, this just returns false as we resolve it by swapping.
    return false;
}

} // namespace NEURONiK::Main
