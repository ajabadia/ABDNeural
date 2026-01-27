#include "NEURONiKProcessor.h"
#include "NEURONiKEditor.h"
#include "../State/ParameterDefinitions.h"
#include "../DSP/Synthesis/ResonatorVoice.h"
#include "../DSP/Synthesis/ResonatorSound.h"

using namespace NEURONiK::State;

NEURONiKProcessor::NEURONiKProcessor()
    : apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    presetManager = std::make_unique<NEURONiK::Serialization::PresetManager>(apvts);

    keyboardState.addListener(this);

    for (auto& val : spectralDataForUI)
    {
        val.store(0.0f, std::memory_order_relaxed);
    }

    for (auto& name : modelNames)
        name = "EMPTY";

    setupModulatableParameters();

    for (auto& route : modulationMatrix)
    {
        route.source = 0;
        route.destination = 0;
        route.amount = 0.0f;
    }

    for (auto& param : getParameters())
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
        {
            apvts.addParameterListener(p->getParameterID(), this);
        }
    }

    masterLevelSmoother.reset(getSampleRate(), 0.05);

    #define LOAD_PARAM(id) parameterChanged(IDs::id, apvts.getRawParameterValue(IDs::id)->load())
    LOAD_PARAM(morphX);
    LOAD_PARAM(morphY);
    LOAD_PARAM(oscLevel);
    LOAD_PARAM(envAttack);
    LOAD_PARAM(envDecay);
    LOAD_PARAM(envSustain);
    LOAD_PARAM(envRelease);
    LOAD_PARAM(filterCutoff);
    LOAD_PARAM(filterRes);
    LOAD_PARAM(oscInharmonicity);
    LOAD_PARAM(oscRoughness);
    LOAD_PARAM(fxSaturation);
    LOAD_PARAM(fxDelayTime);
    LOAD_PARAM(fxDelayFeedback);
    LOAD_PARAM(resonatorParity);
    LOAD_PARAM(resonatorShift);
    LOAD_PARAM(resonatorRolloff);
    LOAD_PARAM(filterEnvAmount);
    LOAD_PARAM(filterAttack);
    LOAD_PARAM(filterDecay);
    LOAD_PARAM(filterSustain);
    LOAD_PARAM(filterRelease);
    LOAD_PARAM(masterBPM);
    LOAD_PARAM(lfo1Waveform);
    LOAD_PARAM(lfo1RateHz);
    LOAD_PARAM(lfo1SyncMode);
    LOAD_PARAM(lfo1RhythmicDivision);
    LOAD_PARAM(lfo1Depth);
    LOAD_PARAM(lfo2Waveform);
    LOAD_PARAM(lfo2RateHz);
    LOAD_PARAM(lfo2SyncMode);
    LOAD_PARAM(lfo2RhythmicDivision);
    LOAD_PARAM(lfo2Depth);
    LOAD_PARAM(mod1Source);
    LOAD_PARAM(mod1Destination);
    LOAD_PARAM(mod1Amount);
    LOAD_PARAM(mod2Source);
    LOAD_PARAM(mod2Destination);
    LOAD_PARAM(mod2Amount);
    LOAD_PARAM(mod3Source);
    LOAD_PARAM(mod3Destination);
    LOAD_PARAM(mod3Amount);
    LOAD_PARAM(mod4Source);
    LOAD_PARAM(mod4Destination);
    LOAD_PARAM(mod4Amount);
    #undef LOAD_PARAM

    masterLevelSmoother.setCurrentAndTargetValue(apvts.getRawParameterValue(IDs::masterLevel)->load());

    apvts.state.addListener(this);
}

NEURONiKProcessor::~NEURONiKProcessor()
{
    apvts.state.removeListener(this);
    keyboardState.removeListener(this);
    for (auto& param : getParameters())
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
        {
            apvts.removeParameterListener(p->getParameterID(), this);
        }
    }
}

void NEURONiKProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    synth.setCurrentPlaybackSampleRate(sampleRate);
    setupSynth();

    lfo1.setSampleRate(sampleRate);
    lfo2.setSampleRate(sampleRate);
    lfo1.reset();
    lfo2.reset();

    masterLevelSmoother.reset(sampleRate, 0.05);
    delayProcessor.prepare(sampleRate, static_cast<int>(sampleRate * 2.0));
    chorusProcessor.prepare(sampleRate);
    reverbProcessor.prepare(sampleRate);
}

void NEURONiKProcessor::setupSynth()
{
    juce::ScopedLock lock(processLock);
    synth.clearVoices();
    
    // Ensure we don't exceed a safe limit for this architecture
    currentPolyphony = juce::jlimit(1, 16, currentPolyphony);

    for (int i = 0; i < currentPolyphony; ++i)
        synth.addVoice(new NEURONiK::DSP::Synthesis::ResonatorVoice(this));

    synth.clearSounds();
    synth.addSound(new NEURONiK::DSP::Synthesis::ResonatorSound());
}

void NEURONiKProcessor::setPolyphony(int numVoices)
{
    if (currentPolyphony != numVoices)
    {
        currentPolyphony = numVoices;
        setupSynth();
    }
}

void NEURONiKProcessor::releaseResources() {}

void NEURONiKProcessor::handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity)
{
    juce::MidiMessage msg = juce::MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity);
    const juce::ScopedLock lock(midiBufferLock);
    uiMidiBuffer.addEvent(msg, 0);
}

void NEURONiKProcessor::handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity)
{
    juce::MidiMessage msg = juce::MidiMessage::noteOff(midiChannel, midiNoteNumber, velocity);
    const juce::ScopedLock lock(midiBufferLock);
    uiMidiBuffer.addEvent(msg, 0);
}

void NEURONiKProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID.startsWith("lfo") || parameterID.startsWith("mod"))
    {
        modulationNeedsUpdating = true;
    }
    else
    {
        if (parameterID == IDs::morphX) voiceParams.morphX = newValue;
        else if (parameterID == IDs::morphY) voiceParams.morphY = newValue;
        else if (parameterID == IDs::oscLevel) voiceParams.oscLevel = newValue;
        else if (parameterID == IDs::envAttack) voiceParams.attack = newValue;
        else if (parameterID == IDs::envDecay) voiceParams.decay = newValue;
        else if (parameterID == IDs::envSustain) voiceParams.sustain = newValue;
        else if (parameterID == IDs::envRelease) voiceParams.release = newValue;
        else if (parameterID == IDs::filterCutoff) voiceParams.filterCutoff = newValue;
        else if (parameterID == IDs::filterRes) voiceParams.filterRes = newValue;
        else if (parameterID == IDs::oscInharmonicity) voiceParams.inharmonicity = newValue;
        else if (parameterID == IDs::oscRoughness) voiceParams.roughness = newValue;
        else if (parameterID == IDs::resonatorParity) voiceParams.resonatorParity = newValue;
        else if (parameterID == IDs::resonatorShift) voiceParams.resonatorShift = newValue;
        else if (parameterID == IDs::resonatorRolloff) voiceParams.resonatorRollOff = newValue;
        else if (parameterID == IDs::filterEnvAmount) voiceParams.fEnvAmount = newValue;
        else if (parameterID == IDs::filterAttack) voiceParams.fAttack = newValue;
        else if (parameterID == IDs::filterDecay) voiceParams.fDecay = newValue;
        else if (parameterID == IDs::filterSustain) voiceParams.fSustain = newValue;
        else if (parameterID == IDs::filterRelease) voiceParams.fRelease = newValue;
        else if (parameterID == IDs::fxSaturation) saturationProcessor.setAmount(newValue);
        else if (parameterID == IDs::fxDelayTime) delayProcessor.setParameters(newValue, apvts.getRawParameterValue(IDs::fxDelayFeedback)->load());
        else if (parameterID == IDs::fxDelayFeedback) delayProcessor.setParameters(apvts.getRawParameterValue(IDs::fxDelayTime)->load(), newValue);
        else if (parameterID == IDs::masterLevel) masterLevelSmoother.setTargetValue(newValue);
        else if (parameterID == IDs::masterBPM) {
            lfo1.setTempoBPM(newValue);
            lfo2.setTempoBPM(newValue);
        }
        else if (parameterID.startsWith("fx")) {
            // Most FX are updated in updateVoiceParameters loop, 
            // but we ensure the flag is set.
            parametersNeedUpdating = true;
        }
        else if (parameterID.startsWith("lfo")) modulationNeedsUpdating = true;

        parametersNeedUpdating = true;
    }
}

void NEURONiKProcessor::updateVoiceParameters(int numSamples)
{
    auto lfo1Value = lfo1.processBlock(numSamples);
    auto lfo2Value = lfo2.processBlock(numSamples);

    auto pbValue = (pitchBendValue.load(std::memory_order_relaxed) - 0.5f) * 2.0f; // Bipolar
    auto mwValue = modWheelValue.load(std::memory_order_relaxed); // Unipolar

    auto modulatedParams = voiceParams;

    float modulatedSaturation = apvts.getRawParameterValue(IDs::fxSaturation)->load();
    float modulatedDelayTime = apvts.getRawParameterValue(IDs::fxDelayTime)->load();
    float modulatedDelayFeedback = apvts.getRawParameterValue(IDs::fxDelayFeedback)->load();
    float modulatedParity = apvts.getRawParameterValue(IDs::resonatorParity)->load();
    float modulatedShift = apvts.getRawParameterValue(IDs::resonatorShift)->load();
    float modulatedRollOff = apvts.getRawParameterValue(IDs::resonatorRolloff)->load();

    for (const auto& route : modulationMatrix)
    {
        if (route.source == 0 || route.destination == 0) continue;

        float modSourceValue = 0.0f;
        switch(route.source)
        {
            case 1: modSourceValue = lfo1Value; break;
            case 2: modSourceValue = lfo2Value; break;
            case 3: modSourceValue = pbValue; break;
            case 4: modSourceValue = mwValue; break;
            default: break;
        }

        float modAmount = route.amount * modSourceValue;
        const juce::String& destParamID = modulatableParameters[route.destination - 1];
        
        if (auto* param = apvts.getParameter(destParamID))
        {
            float currentValue = param->getValue();
            float newValue = juce::jlimit(0.0f, 1.0f, currentValue + modAmount);
            float realValue = apvts.getParameterRange(destParamID).convertFrom0to1(newValue);

            if (destParamID == IDs::oscLevel) modulatedParams.oscLevel = realValue;
            else if (destParamID == IDs::oscInharmonicity) modulatedParams.inharmonicity = realValue;
            else if (destParamID == IDs::oscRoughness) modulatedParams.roughness = realValue;
            else if (destParamID == IDs::morphX) modulatedParams.morphX = realValue;
            else if (destParamID == IDs::morphY) modulatedParams.morphY = realValue;
            else if (destParamID == IDs::envAttack) modulatedParams.attack = realValue;
            else if (destParamID == IDs::envDecay) modulatedParams.decay = realValue;
            else if (destParamID == IDs::envSustain) modulatedParams.sustain = realValue;
            else if (destParamID == IDs::envRelease) modulatedParams.release = realValue;
            else if (destParamID == IDs::filterCutoff) modulatedParams.filterCutoff = realValue;
            else if (destParamID == IDs::filterRes) modulatedParams.filterRes = realValue;
            else if (destParamID == IDs::filterEnvAmount) modulatedParams.fEnvAmount = realValue;
            else if (destParamID == IDs::filterAttack) modulatedParams.fAttack = realValue;
            else if (destParamID == IDs::filterDecay) modulatedParams.fDecay = realValue;
            else if (destParamID == IDs::filterSustain) modulatedParams.fSustain = realValue;
            else if (destParamID == IDs::filterRelease) modulatedParams.fRelease = realValue;
            else if (destParamID == IDs::fxSaturation) modulatedSaturation = realValue;
            else if (destParamID == IDs::fxDelayTime) modulatedDelayTime = realValue;
            else if (destParamID == IDs::fxDelayFeedback) modulatedDelayFeedback = realValue;
            else if (destParamID == IDs::resonatorParity) modulatedParity = realValue;
            else if (destParamID == IDs::resonatorShift) modulatedShift = realValue;
            else if (destParamID == IDs::resonatorRolloff) modulatedRollOff = realValue;
        }
    }

    modulatedParams.resonatorParity = modulatedParity;
    modulatedParams.resonatorShift = modulatedShift;
    modulatedParams.resonatorRollOff = modulatedRollOff;
    
    // Update visualization atoms
    uiMorphX.store(modulatedParams.morphX, std::memory_order_relaxed);
    uiMorphY.store(modulatedParams.morphY, std::memory_order_relaxed);
    uiInharmonicity.store(modulatedParams.inharmonicity, std::memory_order_relaxed);
    uiRoughness.store(modulatedParams.roughness, std::memory_order_relaxed);
    uiCutoff.store(modulatedParams.filterCutoff, std::memory_order_relaxed);
    uiResonance.store(modulatedParams.filterRes, std::memory_order_relaxed);
    
    uiParity.store(modulatedParams.resonatorParity, std::memory_order_relaxed);
    uiShift.store(modulatedParams.resonatorShift, std::memory_order_relaxed);
    uiRollOff.store(modulatedParams.resonatorRollOff, std::memory_order_relaxed);
    
    uiAttack.store(modulatedParams.attack, std::memory_order_relaxed);
    uiDecay.store(modulatedParams.decay, std::memory_order_relaxed);
    uiSustain.store(modulatedParams.sustain, std::memory_order_relaxed);
    uiRelease.store(modulatedParams.release, std::memory_order_relaxed);

    uiFAttack.store(modulatedParams.fAttack, std::memory_order_relaxed);
    uiFDecay.store(modulatedParams.fDecay, std::memory_order_relaxed);
    uiFSustain.store(modulatedParams.fSustain, std::memory_order_relaxed);
    uiFRelease.store(modulatedParams.fRelease, std::memory_order_relaxed);
    uiFEnvAmount.store(modulatedParams.fEnvAmount, std::memory_order_relaxed);

    uiSaturation.store(modulatedSaturation, std::memory_order_relaxed);
    uiDelayTime.store(modulatedDelayTime, std::memory_order_relaxed);
    uiDelayFB.store(modulatedDelayFeedback, std::memory_order_relaxed);

    // --- Delay Sync Logic ---
    bool delaySyncEnabled = apvts.getRawParameterValue(IDs::fxDelaySync)->load() > 0.5f;
    if (delaySyncEnabled)
    {
        float bpm = apvts.getRawParameterValue(IDs::masterBPM)->load();
        if (auto* ph = getPlayHead())
        {
            auto pos = ph->getPosition();
            if (pos.hasValue() && pos->getBpm().hasValue())
                bpm = static_cast<float>(*pos->getBpm());
        }

        int divIdx = static_cast<int>(apvts.getRawParameterValue(IDs::fxDelayDivision)->load());
        float divisions[] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 2.0f/3.0f, 1.0f/3.0f, 0.5f/3.0f };
        float quarterNoteDir = 60.0f / bpm;
        modulatedDelayTime = quarterNoteDir * divisions[juce::jlimit(0, 8, divIdx)];
    }

    saturationProcessor.setAmount(modulatedSaturation);
    delayProcessor.setParameters(modulatedDelayTime, modulatedDelayFeedback);

    chorusProcessor.setParameters(apvts.getRawParameterValue(IDs::fxChorusRate)->load(), 
                                  apvts.getRawParameterValue(IDs::fxChorusDepth)->load(), 
                                  apvts.getRawParameterValue(IDs::fxChorusMix)->load());
                                  
    reverbProcessor.setParameters(apvts.getRawParameterValue(IDs::fxReverbSize)->load(), 
                                  apvts.getRawParameterValue(IDs::fxReverbDamping)->load(), 
                                  apvts.getRawParameterValue(IDs::fxReverbWidth)->load(), 
                                  apvts.getRawParameterValue(IDs::fxReverbMix)->load());

    float maxEnvLevel = 0.0f;
    float maxFEnvLevel = 0.0f;
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<NEURONiK::DSP::Synthesis::ResonatorVoice*>(synth.getVoice(i)))
        {
            voice->updateParameters(modulatedParams);
            
            // Track max envelopes for visualization
            float env = voice->getCurrentEnvelopeLevel();
            if (env > maxEnvLevel) maxEnvLevel = env;

            float fEnv = voice->getCurrentFilterEnvelopeLevel();
            if (fEnv > maxFEnvLevel) maxFEnvLevel = fEnv;
        }
    }
    uiEnvelope.store(maxEnvLevel, std::memory_order_relaxed);
    uiFEnvelope.store(maxFEnvLevel, std::memory_order_relaxed);
    parametersNeedUpdating = false;
}

void NEURONiKProcessor::updateModulation()
{
    auto getDivision = [](int choice) -> float {
        switch (choice) {
            case 0: return 0.25f; // 1/1
            case 1: return 0.5f;  // 1/2
            case 2: return 1.0f;  // 1/4
            case 3: return 2.0f;  // 1/8
            case 4: return 4.0f;  // 1/16
            case 5: return 8.0f;  // 1/32
            case 6: return 1.5f;  // 1/4t (3 cycles in 2 beats)
            case 7: return 3.0f;  // 1/8t (3 cycles in 1 beat)
            case 8: return 6.0f;  // 1/16t (3 cycles in 0.5 beat)
            default: return 1.0f;
        }
    };

    lfo1.setWaveform(static_cast<NEURONiK::DSP::Core::LFO::Waveform>((int)apvts.getRawParameterValue(IDs::lfo1Waveform)->load()));
    lfo1.setRate(apvts.getRawParameterValue(IDs::lfo1RateHz)->load());
    lfo1.setSyncMode(static_cast<NEURONiK::DSP::Core::LFO::SyncMode>((int)apvts.getRawParameterValue(IDs::lfo1SyncMode)->load()));
    lfo1.setRhythmicDivision(getDivision((int)apvts.getRawParameterValue(IDs::lfo1RhythmicDivision)->load()));
    lfo1.setDepth(apvts.getRawParameterValue(IDs::lfo1Depth)->load());

    lfo2.setWaveform(static_cast<NEURONiK::DSP::Core::LFO::Waveform>((int)apvts.getRawParameterValue(IDs::lfo2Waveform)->load()));
    lfo2.setRate(apvts.getRawParameterValue(IDs::lfo2RateHz)->load());
    lfo2.setSyncMode(static_cast<NEURONiK::DSP::Core::LFO::SyncMode>((int)apvts.getRawParameterValue(IDs::lfo2SyncMode)->load()));
    lfo2.setRhythmicDivision(getDivision((int)apvts.getRawParameterValue(IDs::lfo2RhythmicDivision)->load()));
    lfo2.setDepth(apvts.getRawParameterValue(IDs::lfo2Depth)->load());

    for (int i = 0; i < 4; ++i)
    {
        modulationMatrix[i].source = (int)apvts.getRawParameterValue("mod" + juce::String(i+1) + "Source")->load();
        modulationMatrix[i].destination = (int)apvts.getRawParameterValue("mod" + juce::String(i+1) + "Destination")->load();
        modulationMatrix[i].amount = apvts.getRawParameterValue("mod" + juce::String(i+1) + "Amount")->load();
    }

    modulationNeedsUpdating = false;
}

void NEURONiKProcessor::enterMidiLearnMode(const juce::String& paramID)
{
    midiLearnActive = true;
    parameterToLearn = paramID;
}

void NEURONiKProcessor::clearMidiLearnForParameter(const juce::String& paramID)
{
    for (auto it = midiCCMap.begin(); it != midiCCMap.end(); ++it)
    {
        if (it->second == paramID)
        {
            midiCCMap.erase(it);
            break;
        }
    }
}

void NEURONiKProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ScopedLock lock(processLock); // Protect against voice changes
    buffer.clear();

    { // Scoped lock for MIDI buffer access
        const juce::ScopedLock midiLock(midiBufferLock);
        if (!uiMidiBuffer.isEmpty())
        {
            midiMessages.addEvents(uiMidiBuffer, 0, -1, 0);
            uiMidiBuffer.clear();
        }
    }

    auto selectedChannel = apvts.getRawParameterValue(IDs::midiChannel)->load();

    juce::MidiBuffer processedMidi;
    int selectedChInt = juce::roundToInt(selectedChannel);

    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        if (selectedChInt == 0 || message.getChannel() == selectedChInt)
        {
            processedMidi.addEvent(message, metadata.samplePosition);
        }
    }

    if (modulationNeedsUpdating.load())
        updateModulation();

    bool hostTempoFound = false;
    if (auto* currentPlayHead = getPlayHead())
    {
        if (auto positionInfo = currentPlayHead->getPosition())
        {
            if (positionInfo->getBpm())
            {
                lfo1.setTempoBPM(*positionInfo->getBpm());
                lfo2.setTempoBPM(*positionInfo->getBpm());
                hostTempoFound = true;
            }
        }
    }

    if (!hostTempoFound)
    {
        float internalBPM = apvts.getRawParameterValue(IDs::masterBPM)->load();
        lfo1.setTempoBPM(internalBPM);
        lfo2.setTempoBPM(internalBPM);
    }

    for (const auto metadata : processedMidi)
    {
        auto message = metadata.getMessage();
        if (message.isController())
        {
            int controllerNumber = message.getControllerNumber();

            if (midiLearnActive.load())
            {
                midiCCMap[controllerNumber] = parameterToLearn;
                midiLearnActive = false;
                parameterToLearn = "";
            }
            else if (midiCCMap.count(controllerNumber))
            {
                apvts.getParameter(midiCCMap[controllerNumber])->setValueNotifyingHost(message.getControllerValue() / 127.0f);
            }
            else
            {
                switch (controllerNumber)
                {
                    case 1: // Mod Wheel
                        modWheelValue.store(message.getControllerValue() / 127.0f, std::memory_order_relaxed);
                        break;
                    case 7: // Main Volume
                        apvts.getParameter(IDs::masterLevel)->setValueNotifyingHost(message.getControllerValue() / 127.0f);
                        break;
                }
            }
        }
        else if (message.isPitchWheel())
        {
            pitchBendValue.store(message.getPitchWheelValue() / 16383.0f, std::memory_order_relaxed);
            synth.handlePitchWheel(message.getChannel(), message.getPitchWheelValue());
        }
    }

    updateVoiceParameters(buffer.getNumSamples());

    bool thruEnabled = apvts.getRawParameterValue(IDs::midiThru)->load() > 0.5f;
    if (thruEnabled)
        midiMessages.addEvents(processedMidi, 0, buffer.getNumSamples(), 0);

    synth.renderNextBlock(buffer, processedMidi, 0, buffer.getNumSamples());

    saturationProcessor.processBlock(buffer);
    chorusProcessor.processBlock(buffer);
    delayProcessor.processBlock(buffer);
    reverbProcessor.processBlock(buffer);

    masterLevelSmoother.applyGain(buffer, buffer.getNumSamples());
}

void NEURONiKProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> floatBuffer(buffer.getNumChannels(), buffer.getNumSamples());
    processBlock(floatBuffer, midi);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const float* src = floatBuffer.getReadPointer(channel);
        double* dst = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            dst[sample] = static_cast<double>(src[sample]);
        }
    }
}

juce::AudioProcessorEditor* NEURONiKProcessor::createEditor()
{
    return new NEURONiKEditor(*this);
}

bool NEURONiKProcessor::hasEditor() const
{
    return true;
}

const juce::String NEURONiKProcessor::getName() const
{
    return JucePlugin_Name;
}

void NEURONiKProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    auto* modelsNode = new juce::XmlElement("MODELS");
    for (int i = 0; i < 4; ++i)
        modelsNode->setAttribute("model" + juce::String(i), modelNames[i]);
    xml->addChildElement(modelsNode);

    copyXmlToBinary(*xml, destData);
}

void NEURONiKProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        if (auto* modelsNode = xmlState->getChildByName("MODELS"))
        {
            for (int i = 0; i < 4; ++i)
                modelNames[i] = modelsNode->getStringAttribute("model" + juce::String(i), "EMPTY");
        }

        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        reloadModels();
        parametersNeedUpdating = true;
        modulationNeedsUpdating = true;

        if (auto* editor = dynamic_cast<NEURONiKEditor*>(getActiveEditor()))
            editor->updateModelNames();
    }
}

void NEURONiKProcessor::setupModulatableParameters()
{
    modulatableParameters.clear();
    modulatableParameters.push_back(IDs::oscLevel);
    modulatableParameters.push_back(IDs::oscInharmonicity);
    modulatableParameters.push_back(IDs::oscRoughness);
    modulatableParameters.push_back(IDs::morphX);
    modulatableParameters.push_back(IDs::morphY);
    modulatableParameters.push_back(IDs::envAttack);
    modulatableParameters.push_back(IDs::envDecay);
    modulatableParameters.push_back(IDs::envSustain);
    modulatableParameters.push_back(IDs::envRelease);
    modulatableParameters.push_back(IDs::filterCutoff);
    modulatableParameters.push_back(IDs::filterRes);
    modulatableParameters.push_back(IDs::filterEnvAmount);
    modulatableParameters.push_back(IDs::filterAttack);
    modulatableParameters.push_back(IDs::filterDecay);
    modulatableParameters.push_back(IDs::filterSustain);
    modulatableParameters.push_back(IDs::filterRelease);
    modulatableParameters.push_back(IDs::fxSaturation);
    modulatableParameters.push_back(IDs::fxDelayTime);
    modulatableParameters.push_back(IDs::fxDelayFeedback);
    modulatableParameters.push_back(IDs::resonatorParity);
    modulatableParameters.push_back(IDs::resonatorShift);
    modulatableParameters.push_back(IDs::resonatorRolloff);
}

int NEURONiKProcessor::getParameterIndex(const juce::String& paramID) const
{
    for (int i = 0; i < modulatableParameters.size(); ++i)
        if (modulatableParameters[i] == paramID)
            return i + 1;
    return 0;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NEURONiKProcessor();
}

void NEURONiKProcessor::loadModel(const juce::File& file, int slot)
{
    if (slot < 0 || slot >= 4) return;

    juce::var jsonData;
    juce::Result result = juce::JSON::parse(file.loadFileAsString(), jsonData);

    if (result.wasOk())
    {
        if (auto* object = jsonData.getDynamicObject())
        {
            NEURONiK::DSP::Core::SpectralModel model;
            if (auto* amps = object->getProperty("amplitudes").getArray())
            {
                for (int i = 0; i < 64; ++i)
                    model.amplitudes[i] = amps->getUnchecked(i);
            }
            if (auto* freqs = object->getProperty("frequencyOffsets").getArray())
            {
                for (int i = 0; i < 64; ++i)
                    model.frequencyOffsets[i] = freqs->getUnchecked(i);
            }

            for (int i = 0; i < synth.getNumVoices(); ++i)
            {
                if (auto* voice = dynamic_cast<NEURONiK::DSP::Synthesis::ResonatorVoice*>(synth.getVoice(i)))
                {
                    voice->loadModel(model, slot);
                }
            }

            modelNames[slot] = file.getFileNameWithoutExtension();
            
            // Save path in APVTS state for persistence
            if (apvts.state.isValid())
            {
                apvts.state.setProperty("modelPath" + juce::String(slot), file.getFullPathName(), nullptr);
            }

            if (auto* editor = dynamic_cast<NEURONiKEditor*>(getActiveEditor()))
                editor->updateModelNames();
        }
    }
}

void NEURONiKProcessor::reloadModels()
{
    for (int i = 0; i < 4; ++i)
    {
        juce::String path = apvts.state.getProperty("modelPath" + juce::String(i)).toString();
        if (path.isNotEmpty() && path != "EMPTY")
        {
            juce::File file(path);
            if (file.existsAsFile())
                loadModel(file, i);
        }
    }
}

void NEURONiKProcessor::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
    juce::ignoreUnused(tree, property);
    // If a model path changed (e.g. from a past or undo/redo), we might want to reload
    // but loadModel already sets the property, so we must avoid recursion
}

void NEURONiKProcessor::valueTreeRedirected(juce::ValueTree& tree)
{
    // This is called when replaceState is used. Re-attach listener!
    tree.addListener(this);
    reloadModels();
}

void NEURONiKProcessor::copyPatchToClipboard()
{
    auto xml = apvts.copyState().createXml();
    if (xml != nullptr)
        juce::SystemClipboard::copyTextToClipboard(xml->toString());
}

void NEURONiKProcessor::pastePatchFromClipboard()
{
    auto xmlString = juce::SystemClipboard::getTextFromClipboard();
    auto xml = juce::parseXML(xmlString);
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        // valueTreeRedirected will handle re-attachment and reloadModels()
    }
}
