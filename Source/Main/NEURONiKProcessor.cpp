#include "NEURONiKProcessor.h"
#include "NEURONiKEditor.h"
#include "../State/ParameterDefinitions.h"
#include "../DSP/CoreModules/NeuronikEngine.h"
#include "../DSP/CoreModules/NeurotikEngine.h"
#include "../DSP/Synthesis/AdditiveVoice.h"
#include "../DSP/Synthesis/NeurotikVoice.h"

using namespace NEURONiK::State;

NEURONiKProcessor::NEURONiKProcessor()
    : apvts(*this, nullptr, "Parameters", createParameterLayout()),
      midiFifo(1024),
      commandFifo(32)
{
    presetManager = std::make_unique<NEURONiK::Serialization::PresetManager>(apvts);
    midiMappingManager = std::make_unique<NEURONiK::Main::MidiMappingManager>(apvts);
    int initialEngineType = (int)apvts.getRawParameterValue(IDs::engineType)->load();
    if (initialEngineType == 0)
        engine = std::make_unique<NEURONiK::DSP::NeuronikEngine>();
    else
        engine = std::make_unique<NEURONiK::DSP::NeurotikEngine>();

    keyboardState.addListener(this);

    for (auto& val : spectralDataForUI)
        val.store(0.0f, std::memory_order_relaxed);

    for (auto& name : modelNames)
        name = "EMPTY";

    for (auto& modVal : modulationValues)
        modVal.store(0.0f, std::memory_order_relaxed);

    for (auto& param : getParameters())
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            apvts.addParameterListener(p->getParameterID(), this);
    }

    #define LOAD_PARAM(id) parameterChanged(IDs::id, apvts.getRawParameterValue(IDs::id)->load())
    LOAD_PARAM(engineType);
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

    apvts.state.addListener(this);
}

NEURONiKProcessor::~NEURONiKProcessor()
{
    apvts.state.removeListener(this);
    keyboardState.removeListener(this);
    for (auto& param : getParameters())
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            apvts.removeParameterListener(p->getParameterID(), this);
    }
}

void NEURONiKProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    if (engine) engine->prepare(sampleRate, samplesPerBlock);
    keyboardState.reset();
}

void NEURONiKProcessor::setPolyphony(int numVoices)
{
    int newLimit = juce::jlimit(1, 32, numVoices);
    currentPolyphony.store(newLimit);
    if (engine) engine->setPolyphony(newLimit);
}

int NEURONiKProcessor::getPolyphony() const { return currentPolyphony.load(); }
NEURONiKProcessor::EditorSettings& NEURONiKProcessor::getEditorSettings() { return editorSettings; }
void NEURONiKProcessor::releaseResources() {}

void NEURONiKProcessor::handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity)
{
    int start1, block1, start2, block2;
    midiFifo.prepareToWrite(1, start1, block1, start2, block2);
    if (block1 > 0)
        midiQueue[start1] = { juce::MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity), 0 };
    else if (block2 > 0)
        midiQueue[start2] = { juce::MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity), 0 };
    
    midiFifo.finishedWrite(block1 + block2);
}

void NEURONiKProcessor::handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity)
{
    int start1, block1, start2, block2;
    midiFifo.prepareToWrite(1, start1, block1, start2, block2);
    if (block1 > 0)
        midiQueue[start1] = { juce::MidiMessage::noteOff(midiChannel, midiNoteNumber, velocity), 0 };
    else if (block2 > 0)
        midiQueue[start2] = { juce::MidiMessage::noteOff(midiChannel, midiNoteNumber, velocity), 0 };
        
    midiFifo.finishedWrite(block1 + block2);
}

void NEURONiKProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == IDs::engineType)
    {
        int type = static_cast<int>(newValue);
        
        // This is safe to run on UI thread (called by APVTS)
        // Swap engine
        std::unique_ptr<NEURONiK::DSP::ISynthesisEngine> newEngine;
        if (type == 0)
            newEngine = std::make_unique<NEURONiK::DSP::NeuronikEngine>();
        else
            newEngine = std::make_unique<NEURONiK::DSP::NeurotikEngine>();

        // Prepare new engine
        newEngine->prepare(getSampleRate(), getBlockSize());
        newEngine->setPolyphony(currentPolyphony.load());
        
        // Load models into new engine
        for (int i = 0; i < 4; ++i)
        {
            juce::String path = apvts.state.getProperty("modelPath" + juce::String(i)).toString();
            if (path.isNotEmpty() && path != "EMPTY")
            {
                auto model = NEURONiK::Serialization::PresetManager::loadModelFromFile(juce::File(path));
                if (model.isValid) newEngine->loadModel(model, i);
            }
        }

        // Swap (Atomic unique_ptr swap is not atomic, so we need a lock if processBlock runs)
        {
            const juce::ScopedLock engineLock(getCallbackLock());
            engine = std::move(newEngine);
        }
    }
}

void NEURONiKProcessor::synchronizeEngineParameters()
{
    if (!engine) return;

    if (auto* nEngine = dynamic_cast<NEURONiK::DSP::NeuronikEngine*>(engine.get()))
    {
        ::NEURONiK::DSP::Synthesis::AdditiveVoice::Params vParams;
        vParams.oscLevel = apvts.getRawParameterValue(IDs::oscLevel)->load();
        vParams.attack = apvts.getRawParameterValue(IDs::envAttack)->load() * 1000.0f;
        vParams.decay = apvts.getRawParameterValue(IDs::envDecay)->load() * 1000.0f;
        vParams.sustain = apvts.getRawParameterValue(IDs::envSustain)->load();
        vParams.release = apvts.getRawParameterValue(IDs::envRelease)->load() * 1000.0f;
        vParams.filterCutoff = apvts.getRawParameterValue(IDs::filterCutoff)->load();
        vParams.filterRes = apvts.getRawParameterValue(IDs::filterRes)->load();
        vParams.fEnvAmount = apvts.getRawParameterValue(IDs::filterEnvAmount)->load();
        vParams.fAttack = apvts.getRawParameterValue(IDs::filterAttack)->load() * 1000.0f;
        vParams.fDecay = apvts.getRawParameterValue(IDs::filterDecay)->load() * 1000.0f;
        vParams.fSustain = apvts.getRawParameterValue(IDs::filterSustain)->load();
        vParams.fRelease = apvts.getRawParameterValue(IDs::filterRelease)->load() * 1000.0f;
        vParams.morphX = apvts.getRawParameterValue(IDs::morphX)->load();
        vParams.morphY = apvts.getRawParameterValue(IDs::morphY)->load();
        vParams.inharmonicity = apvts.getRawParameterValue(IDs::oscInharmonicity)->load();
        vParams.roughness = apvts.getRawParameterValue(IDs::oscRoughness)->load();
        vParams.resonatorParity = apvts.getRawParameterValue(IDs::resonatorParity)->load();
        vParams.resonatorShift = apvts.getRawParameterValue(IDs::resonatorShift)->load();
        vParams.resonatorRollOff = apvts.getRawParameterValue(IDs::resonatorRolloff)->load();
        vParams.unisonDetune = apvts.getRawParameterValue(IDs::unisonDetune)->load();
        vParams.unisonSpread = apvts.getRawParameterValue(IDs::unisonSpread)->load();
        
        nEngine->setVoiceParams(vParams);

        ::NEURONiK::DSP::GlobalParams gParams;
        gParams.masterLevel = apvts.getRawParameterValue(IDs::masterLevel)->load();
        gParams.saturationAmt = apvts.getRawParameterValue(IDs::fxSaturation)->load();
        gParams.delayTime = apvts.getRawParameterValue(IDs::fxDelayTime)->load();
        gParams.delayFB = apvts.getRawParameterValue(IDs::fxDelayFeedback)->load();
        gParams.chorusMix = apvts.getRawParameterValue(IDs::fxChorusMix)->load();
        gParams.reverbMix = apvts.getRawParameterValue(IDs::fxReverbMix)->load();
        gParams.lfo1.waveform = (int)apvts.getRawParameterValue(IDs::lfo1Waveform)->load();
        gParams.lfo1.rateHz = apvts.getRawParameterValue(IDs::lfo1RateHz)->load();
        gParams.lfo1.depth = apvts.getRawParameterValue(IDs::lfo1Depth)->load();
        gParams.lfo2.waveform = (int)apvts.getRawParameterValue(IDs::lfo2Waveform)->load();
        gParams.lfo2.rateHz = apvts.getRawParameterValue(IDs::lfo2RateHz)->load();
        gParams.lfo2.depth = apvts.getRawParameterValue(IDs::lfo2Depth)->load();

        for (int i = 0; i < 4; ++i)
        {
            juce::String prefix = "mod" + juce::String(i+1);
            gParams.modMatrix[i].source = (int)apvts.getRawParameterValue(prefix + "Source")->load();
            gParams.modMatrix[i].destination = (int)apvts.getRawParameterValue(prefix + "Destination")->load();
            gParams.modMatrix[i].amount = apvts.getRawParameterValue(prefix + "Amount")->load();
        }
        nEngine->setGlobalParams(gParams);
    }
    else if (auto* ntEngine = dynamic_cast<NEURONiK::DSP::NeurotikEngine*>(engine.get()))
    {
        ::NEURONiK::DSP::Synthesis::NeurotikVoice::Params ntParams;
        ntParams.level = apvts.getRawParameterValue(IDs::oscLevel)->load();
        ntParams.attack = apvts.getRawParameterValue(IDs::envAttack)->load() * 1000.0f;
        ntParams.decay = apvts.getRawParameterValue(IDs::envDecay)->load() * 1000.0f;
        ntParams.sustain = apvts.getRawParameterValue(IDs::envSustain)->load();
        ntParams.release = apvts.getRawParameterValue(IDs::envRelease)->load() * 1000.0f;
        ntParams.morphX = apvts.getRawParameterValue(IDs::morphX)->load();
        ntParams.morphY = apvts.getRawParameterValue(IDs::morphY)->load();
        ntParams.excitationNoise = apvts.getRawParameterValue(IDs::oscExciteNoise)->load();
        ntParams.excitationColor = apvts.getRawParameterValue(IDs::excitationColor)->load();
        ntParams.impulseMix = apvts.getRawParameterValue(IDs::impulseMix)->load();
        ntParams.resonatorResonance = apvts.getRawParameterValue(IDs::resonatorRes)->load();
        ntParams.unisonDetune = apvts.getRawParameterValue(IDs::unisonDetune)->load();
        ntParams.unisonSpread = apvts.getRawParameterValue(IDs::unisonSpread)->load();
        
        ntEngine->setVoiceParams(ntParams);

        // Global Params for Neurotik (Unified)
        ::NEURONiK::DSP::GlobalParams gParams;
        gParams.masterLevel = apvts.getRawParameterValue(IDs::masterLevel)->load();
        gParams.saturationAmt = apvts.getRawParameterValue(IDs::fxSaturation)->load();
        gParams.delayTime = apvts.getRawParameterValue(IDs::fxDelayTime)->load();
        gParams.delayFB = apvts.getRawParameterValue(IDs::fxDelayFeedback)->load();
        gParams.chorusMix = apvts.getRawParameterValue(IDs::fxChorusMix)->load();
        gParams.reverbMix = apvts.getRawParameterValue(IDs::fxReverbMix)->load();
        gParams.lfo1.waveform = (int)apvts.getRawParameterValue(IDs::lfo1Waveform)->load();
        gParams.lfo1.rateHz = apvts.getRawParameterValue(IDs::lfo1RateHz)->load();
        gParams.lfo1.depth = apvts.getRawParameterValue(IDs::lfo1Depth)->load();
        gParams.lfo2.waveform = (int)apvts.getRawParameterValue(IDs::lfo2Waveform)->load();
        gParams.lfo2.rateHz = apvts.getRawParameterValue(IDs::lfo2RateHz)->load();
        gParams.lfo2.depth = apvts.getRawParameterValue(IDs::lfo2Depth)->load();

        for (int i = 0; i < 4; ++i)
        {
            juce::String prefix = "mod" + juce::String(i+1);
            gParams.modMatrix[i].source = (int)apvts.getRawParameterValue(prefix + "Source")->load();
            gParams.modMatrix[i].destination = (int)apvts.getRawParameterValue(prefix + "Destination")->load();
            gParams.modMatrix[i].amount = apvts.getRawParameterValue(prefix + "Amount")->load();
        }
        ntEngine->setGlobalParams(gParams);
    }
}

void NEURONiKProcessor::enterMidiLearnMode(const juce::String& paramID)
{
    midiLearnActive.store(true);
    parameterToLearn = paramID;
}

void NEURONiKProcessor::clearMidiLearnForParameter(const juce::String& paramID)
{
    midiMappingManager->clearMapping(paramID);
}

void NEURONiKProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    
    // Run pending commands (e.g. Model Loading)
    processCommands();

    // Safe MIDI injection from UI thread (Lock-Free)
    int blockSize1, blockSize2, startIndex1, startIndex2;
    midiFifo.prepareToRead(1024, startIndex1, blockSize1, startIndex2, blockSize2);

    if (blockSize1 > 0)
    {
        for (int i = 0; i < blockSize1; ++i)
            midiMessages.addEvent(midiQueue[startIndex1 + i].message, midiQueue[startIndex1 + i].sampleOffset);
    }

    if (blockSize2 > 0)
    {
        for (int i = 0; i < blockSize2; ++i)
            midiMessages.addEvent(midiQueue[startIndex2 + i].message, midiQueue[startIndex2 + i].sampleOffset);
    }

    midiFifo.finishedRead(blockSize1 + blockSize2);
    
    synchronizeEngineParameters();
    if (engine != nullptr)
    {
        engine->renderNextBlock(buffer, midiMessages);
        float partials[64];
        engine->getSpectralData(partials);
        for (int i = 0; i < 64; ++i)
            spectralDataForUI[i].store(partials[i], std::memory_order_relaxed);
        lfo1ValueForUI.store(engine->getLfoValue(0), std::memory_order_relaxed);
        lfo2ValueForUI.store(engine->getLfoValue(1), std::memory_order_relaxed);
        
        float ampLevel = 0.0f, filterLevel = 0.0f;
        engine->getEnvelopeLevels(ampLevel, filterLevel);
        uiEnvelope.store(ampLevel, std::memory_order_relaxed);
        uiFEnvelope.store(filterLevel, std::memory_order_relaxed);
        
        // --- Safe Modulation Indexing Update ---
        float mods[static_cast<int>(NEURONiK::ModulationTarget::Count)];
        engine->getModulationValues(mods, static_cast<int>(NEURONiK::ModulationTarget::Count));
        for (int i = 0; i < static_cast<int>(NEURONiK::ModulationTarget::Count); ++i)
        {
            modulationValues[i].store(mods[i], std::memory_order_relaxed);
        }
        
        // Update filter envelope parameters for visualization
        uiFAttack.store(apvts.getRawParameterValue(IDs::filterAttack)->load(), std::memory_order_relaxed);
        uiFDecay.store(apvts.getRawParameterValue(IDs::filterDecay)->load(), std::memory_order_relaxed);
        uiFSustain.store(apvts.getRawParameterValue(IDs::filterSustain)->load(), std::memory_order_relaxed);
        uiFRelease.store(apvts.getRawParameterValue(IDs::filterRelease)->load(), std::memory_order_relaxed);
        
        // Update XY Pad parameters for visualization
        uiMorphX.store(apvts.getRawParameterValue(IDs::morphX)->load(), std::memory_order_relaxed);
        uiMorphY.store(apvts.getRawParameterValue(IDs::morphY)->load(), std::memory_order_relaxed);
    }
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
            dst[sample] = static_cast<double>(src[sample]);
    }
}

juce::AudioProcessorEditor* NEURONiKProcessor::createEditor() { return new NEURONiKEditor(*this); }
bool NEURONiKProcessor::hasEditor() const { return true; }
const juce::String NEURONiKProcessor::getName() const { return JucePlugin_Name; }

void NEURONiKProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    midiMappingManager->saveToValueTree(state);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr) copyXmlToBinary(*xml, destData);
}

void NEURONiKProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml(*xmlState);
        apvts.replaceState(tree);
        midiMappingManager->loadFromValueTree(tree);
        reloadModels();
    }
}

void NEURONiKProcessor::loadModel(const juce::File& file, int slot)
{
    if (slot < 0 || slot >= 4 || !file.exists()) return;
    auto model = NEURONiK::Serialization::PresetManager::loadModelFromFile(file);
    if (model.isValid)
    {
        // Safe Lock-Free Queue
        int start1, block1, start2, block2;
        commandFifo.prepareToWrite(1, start1, block1, start2, block2);
        
        if (block1 > 0)
        {
            auto& cmd = commandQueue[start1];
            cmd.type = EngineCommand::LoadModel;
            cmd.slot = slot;
            cmd.modelData = model; // Copy POD
            commandFifo.finishedWrite(1);
        }
        else if (block2 > 0)
        {
            auto& cmd = commandQueue[start2];
            cmd.type = EngineCommand::LoadModel;
            cmd.slot = slot;
            cmd.modelData = model; // Copy POD
            commandFifo.finishedWrite(1);
        }

        modelNames[slot] = file.getFileNameWithoutExtension();
        if (apvts.state.isValid())
            apvts.state.setProperty("modelPath" + juce::String(slot), file.getFullPathName(), nullptr);
        if (auto* editor = dynamic_cast<NEURONiKEditor*>(getActiveEditor()))
            editor->updateModelNames();
    }
}

void NEURONiKProcessor::processCommands()
{
    int start1, block1, start2, block2;
    commandFifo.prepareToRead(32, start1, block1, start2, block2);

    auto processCmdBlock = [this](int start, int block) {
        for (int i = 0; i < block; ++i)
        {
            const auto& cmd = commandQueue[start + i];
            if (cmd.type == EngineCommand::LoadModel)
            {
                if (engine) engine->loadModel(cmd.modelData, cmd.slot);
            }
        }
    };

    if (block1 > 0) processCmdBlock(start1, block1);
    if (block2 > 0) processCmdBlock(start2, block2);

    commandFifo.finishedRead(block1 + block2);
}

void NEURONiKProcessor::reloadModels()
{
    for (int i = 0; i < 4; ++i)
    {
        juce::String path = apvts.state.getProperty("modelPath" + juce::String(i)).toString();
        if (path.isNotEmpty() && path != "EMPTY")
        {
            juce::File file(path);
            if (file.existsAsFile()) loadModel(file, i);
        }
    }
}

void NEURONiKProcessor::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) {}
void NEURONiKProcessor::valueTreeRedirected(juce::ValueTree& tree) { tree.addListener(this); reloadModels(); }

void NEURONiKProcessor::copyPatchToClipboard()
{
    auto xml = apvts.copyState().createXml();
    if (xml != nullptr) juce::SystemClipboard::copyTextToClipboard(xml->toString());
}

void NEURONiKProcessor::pastePatchFromClipboard()
{
    auto xmlString = juce::SystemClipboard::getTextFromClipboard();
    auto xml = juce::parseXML(xmlString);
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NEURONiKProcessor(); }
