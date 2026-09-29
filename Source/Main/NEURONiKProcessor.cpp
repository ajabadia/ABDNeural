#include "NEURONiKProcessor.h"

#include "../DSP/CoreModules/RhythmicDivision.h"
#include "NEURONiKEditor.h"
// Presets y modelos de fabrica (banco CZ101) embebidos:
// installFactoryModels()/installFactoryPresets() los escriben a
// Documents/NEURONiK/{Models,Presets}/ en el arranque.
#include <NeuronikFactoryModels.h>
#include "../State/ParameterDefinitions.h"
#include "../State/ModMatrixFromState.h"
#include "../DSP/CoreModules/NeuronikEngine.h"
#include "../DSP/CoreModules/NeurotikEngine.h"
#include "../DSP/Synthesis/AdditiveVoice.h"
#include "../DSP/Synthesis/NeurotikVoice.h"
#include "../DSP/Runtime/JuceMidiAdapter.h"

using namespace NEURONiK::State;

namespace
{
/**
 * Modelos de fabrica (banco CZ101): el binario lleva los seis .neuronikmodel
 * generados por la sonda del ModelMaker desde los WAV reales del banco. Los
 * presets de fabrica referencian los ficheros por modelPath<slot>, asi que el
 * plugin escribe el material en Documents/NEURONiK/Models/ en el arranque y
 * UNICAMENTE si falta (nunca regraba: el usuario puede sustituirlos por sus
 * propios modelos y el plugin los respeta).
 */
void installFactoryModels()
{
    const auto modelsDir = NEURONiKProcessor::factoryModelsDirectory();
    modelsDir.createDirectory();

    // Regla de simbolos JUCE (patron BridgeSelftest.h): punto -> `_`,
    // guiones fuera; el tamano viaja en la constante <simbolo>Size.
    struct FactoryModel
    {
        const char* name;
        const char* data;
        int size;
    };

    const FactoryModel factory[] = {
        { "CZ-BASS1.neuronikmodel",
          NeuronikFactoryModels::CZBASS1_neuronikmodel,
          NeuronikFactoryModels::CZBASS1_neuronikmodelSize },
        { "CZ-HAMOG.neuronikmodel",
          NeuronikFactoryModels::CZHAMOG_neuronikmodel,
          NeuronikFactoryModels::CZHAMOG_neuronikmodelSize },
        { "CZ-PAD1.neuronikmodel",
          NeuronikFactoryModels::CZPAD1_neuronikmodel,
          NeuronikFactoryModels::CZPAD1_neuronikmodelSize },
        { "CZ-SWEP1.neuronikmodel",
          NeuronikFactoryModels::CZSWEP1_neuronikmodel,
          NeuronikFactoryModels::CZSWEP1_neuronikmodelSize },
        { "CZ-RRISE-temporal.neuronikmodel",
          NeuronikFactoryModels::CZRRISEtemporal_neuronikmodel,
          NeuronikFactoryModels::CZRRISEtemporal_neuronikmodelSize },
        { "CZ-BASS1-temporal.neuronikmodel",
          NeuronikFactoryModels::CZBASS1temporal_neuronikmodel,
          NeuronikFactoryModels::CZBASS1temporal_neuronikmodelSize },
    };

    for (const auto& model : factory)
    {
        const auto target = modelsDir.getChildFile (model.name);
        if (target.existsAsFile())
            continue; // del usuario o de una instalacion anterior: no se toca

        juce::FileOutputStream out (target);
        if (out.openedOk())
        {
            out.write (model.data, (size_t) model.size);
            out.flush();
        }
    }
}

/**
 * Presets de fabrica (banco CZ101): los siete presets generados OFFLINE por
 * Tests/FactoryPresetGenerator.cpp viajan embebidos con el marcador
 * {{FACTORY_MODELS}} en modelPath0 — un preset guarda rutas ABSOLUTAS y el
 * binario es comun a todas las maquinas, asi que el marcador se sustituye por
 * el directorio real de modelos al instalar. Uno de ellos es el BANCO en un
 * solo preset (CZ101-BANK): los cuatro patches TONALES en las cuatro ranuras,
 * asi que el pad XY morfa el banco entero (abre en la esquina A y el pad
 * recorre B/C/D). FLAT en Presets/ (getAllPresets
 * no es recursivo: en una subcarpeta no aparecerian en el navegador) y solo
 * si falta: nunca se regraba lo que el usuario ya tiene.
 */
void installFactoryPresets()
{
    const auto base = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                          .getChildFile ("NEURONiK");
    const auto modelsDir = base.getChildFile ("Models");
    const auto presetsDir = base.getChildFile ("Presets");
    presetsDir.createDirectory();

    struct FactoryPreset
    {
        const char* name;
        const char* data;
        int size;
    };

    const FactoryPreset factory[] = {
        { "CZ-BASS1.neuronikpreset",
          NeuronikFactoryModels::CZBASS1_neuronikpreset,
          NeuronikFactoryModels::CZBASS1_neuronikpresetSize },
        { "CZ-HAMOG.neuronikpreset",
          NeuronikFactoryModels::CZHAMOG_neuronikpreset,
          NeuronikFactoryModels::CZHAMOG_neuronikpresetSize },
        { "CZ-PAD1.neuronikpreset",
          NeuronikFactoryModels::CZPAD1_neuronikpreset,
          NeuronikFactoryModels::CZPAD1_neuronikpresetSize },
        { "CZ-SWEP1.neuronikpreset",
          NeuronikFactoryModels::CZSWEP1_neuronikpreset,
          NeuronikFactoryModels::CZSWEP1_neuronikpresetSize },
        { "CZ-RRISE-temporal.neuronikpreset",
          NeuronikFactoryModels::CZRRISEtemporal_neuronikpreset,
          NeuronikFactoryModels::CZRRISEtemporal_neuronikpresetSize },
        { "CZ-BASS1-temporal.neuronikpreset",
          NeuronikFactoryModels::CZBASS1temporal_neuronikpreset,
          NeuronikFactoryModels::CZBASS1temporal_neuronikpresetSize },
        { "CZ101-BANK.neuronikpreset",
          NeuronikFactoryModels::CZ101BANK_neuronikpreset,
          NeuronikFactoryModels::CZ101BANK_neuronikpresetSize },
    };

    for (const auto& preset : factory)
    {
        const auto target = presetsDir.getChildFile (preset.name);
        if (target.existsAsFile())
            continue; // del usuario o de una instalacion anterior: no se toca

        const auto xml = juce::String::fromUTF8 (preset.data, preset.size)
                             .replace ("{{FACTORY_MODELS}}/", modelsDir.getFullPathName()
                                       + juce::File::getSeparatorString());
        target.replaceWithText (xml);
    }
}
} // namespace

juce::File NEURONiKProcessor::factoryModelsDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("NEURONiK")
               .getChildFile ("Models");
}

NEURONiKProcessor::NEURONiKProcessor()
    : apvts(*this, nullptr, "Parameters", createParameterLayout()),
      midiFifo(1024),
      commandQueue(std::make_unique<Command[]>(32)),   // Fase 11.1: 32 x ~25 KB = 800 KB fuera de la pila
      commandFifo(32)
{
    presetManager = std::make_unique<NEURONiK::Serialization::PresetManager>(apvts);
    midiMappingManager = std::make_unique<NEURONiK::Main::MidiMappingManager>(apvts);
    installFactoryModels();
    installFactoryPresets();
    int initialEngineType = (int)apvts.getRawParameterValue(IDs::engineType)->load();
    if (initialEngineType == 0)
        engine = std::make_unique<NEURONiK::DSP::NeuronikEngine>();
    else
        engine = std::make_unique<NEURONiK::DSP::NeurotikEngine>();


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

    // OJO: `engineType` NO entra en esta lista aunque `parameterChanged` lo atienda. El
    // motor de arriba ya se ha creado a partir de ESE MISMO valor, y volver a pasar por
    // `parameterChanged` construia un SEGUNDO motor (32 voces, delay, reverb) para
    // prepararlo con `getSampleRate()` = 0 — el plugin nace sin tasa, la pone el host en
    // `prepareToPlay` — y tirar el primero. En Debug ese prepare(0, 0) ademas revienta en
    // el jassert del smoothed value de la saturacion. El motor vivo es el de arriba, y
    // `prepareToPlay` es quien lo deja a punto.
    #define LOAD_PARAM(id) parameterChanged(IDs::id, apvts.getRawParameterValue(IDs::id)->load())
    LOAD_PARAM(morphX);
    LOAD_PARAM(morphY);
    LOAD_PARAM(morphZ);
    LOAD_PARAM(oscLevel);
    LOAD_PARAM(envAttack);
    LOAD_PARAM(envDecay);
    LOAD_PARAM(envSustain);
    LOAD_PARAM(envRelease);
    LOAD_PARAM(filterCutoff);
    LOAD_PARAM(filterRes);
    LOAD_PARAM(oscInharmonicity);
    LOAD_PARAM(oscRoughness);
    LOAD_PARAM(fx1Mix);
    LOAD_PARAM(fx1Param1);
    LOAD_PARAM(fxDelayTime);
    LOAD_PARAM(fxDelayFeedback);
    LOAD_PARAM(resonatorParity);
    LOAD_PARAM(resonatorShift);
    LOAD_PARAM(resonatorRolloff);
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

    // Ensure engine is fully synchronized at startup
    synchronizeEngineParameters();
}

NEURONiKProcessor::~NEURONiKProcessor()
{
    apvts.state.removeListener(this);
    for (auto& param : getParameters())
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            apvts.removeParameterListener(p->getParameterID(), this);
    }
}

void NEURONiKProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    if (engine) engine->prepare(sampleRate, samplesPerBlock);
    synchronizeEngineParameters();
}

void NEURONiKProcessor::setPolyphony(int numVoices)
{
    const int newLimit = juce::jlimit(1, 32, numVoices);
    currentPolyphony.store(newLimit);

    if (engine)
    {
        // Bajo el mismo cerrojo que el cambio de motor: subir RESERVA voces (heap) y
        // bajar devuelve solo las OCIOSAS (!isActive, ver BaseEngine::reclaimIdleVoices).
        // Las que aun sueltan cola no se desalojan; la capacidad en caliente (reserve 32)
        // queda intacta, solo baja size(). La reserva no puede ocurrir mientras el hilo
        // de audio recorre la lista. El editor llama a esto desde el hilo de mensajes.
        const juce::ScopedLock engineLock(getCallbackLock());
        engine->setPolyphony(newLimit);
    }
}

int NEURONiKProcessor::getPolyphony() const { return currentPolyphony.load(); }
void NEURONiKProcessor::releaseResources() {}

void NEURONiKProcessor::injectNoteOn(int midiChannel, int midiNoteNumber, float velocity)
{
    int start1, block1, start2, block2;
    midiFifo.prepareToWrite(1, start1, block1, start2, block2);
    if (block1 > 0)
        midiQueue[start1] = { juce::MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity), 0 };
    else if (block2 > 0)
        midiQueue[start2] = { juce::MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity), 0 };
    
    midiFifo.finishedWrite(block1 + block2);
}

void NEURONiKProcessor::injectNoteOff(int midiChannel, int midiNoteNumber, float velocity)
{
    int start1, block1, start2, block2;
    midiFifo.prepareToWrite(1, start1, block1, start2, block2);
    if (block1 > 0)
        midiQueue[start1] = { juce::MidiMessage::noteOff(midiChannel, midiNoteNumber, velocity), 0 };
    else if (block2 > 0)
        midiQueue[start2] = { juce::MidiMessage::noteOff(midiChannel, midiNoteNumber, velocity), 0 };
        
    midiFifo.finishedWrite(block1 + block2);
}

void NEURONiKProcessor::injectPitchBend(int midiChannel, int position14bit)
{
    int start1, block1, start2, block2;
    midiFifo.prepareToWrite(1, start1, block1, start2, block2);
    if (block1 > 0)
        midiQueue[start1] = { juce::MidiMessage::pitchWheel(midiChannel,
                                  juce::jlimit(0, 16383, position14bit)), 0 };
    else if (block2 > 0)
        midiQueue[start2] = { juce::MidiMessage::pitchWheel(midiChannel,
                                  juce::jlimit(0, 16383, position14bit)), 0 };

    midiFifo.finishedWrite(block1 + block2);
}

void NEURONiKProcessor::injectController(int midiChannel, int controllerNumber, int value)
{
    // Track mod-wheel level for UI feedback loops (atomic, audio-thread safe).
    if (controllerNumber == 1)
        externalModWheel.store(juce::jlimit(0, 127, value) / 127.0f, std::memory_order_relaxed);

    int start1, block1, start2, block2;
    midiFifo.prepareToWrite(1, start1, block1, start2, block2);
    if (block1 > 0)
        midiQueue[start1] = { juce::MidiMessage::controllerEvent(midiChannel,
                                  controllerNumber, juce::jlimit(0, 127, value)), 0 };
    else if (block2 > 0)
        midiQueue[start2] = { juce::MidiMessage::controllerEvent(midiChannel,
                                  controllerNumber, juce::jlimit(0, 127, value)), 0 };

    midiFifo.finishedWrite(block1 + block2);
}

void NEURONiKProcessor::requestAllNotesOff()
{
    allNotesOffRequested.store(true, std::memory_order_relaxed);
}

std::vector<int> NEURONiKProcessor::getHeldNotes() const
{
    std::vector<int> notes;
    notes.reserve (16);

    for (int word = 0; word < 4; ++word)
    {
        auto bits = heldNotesMask[static_cast<size_t> (word)].load (std::memory_order_relaxed);

        // Plain shift scan: no C++20 <bit> (the pilot builds as C++17) and no
        // compiler intrinsics — 128 iterations worst case is nothing for a poll.
        for (int bit = 0; bits != 0u; ++bit, bits >>= 1)
            if ((bits & 1u) != 0u)
                notes.push_back ((word << 5) + bit);
    }

    return notes;
}

void NEURONiKProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Switching the input channel would leave notes from the previous channel
    // hanging, since their note-offs arrive on a channel we now ignore.
    if (parameterID == IDs::midiChannel)
        allNotesOffRequested.store(true, std::memory_order_relaxed);

    // --- El TIPO del hueco 1 (2026-09-29) ----------------------------------
    // `FxSlot::setType` CREA y DESTRUYE la instancia del efecto, asi que no
    // puede correr en el hilo de audio (que es donde `updateFromGlobalParams`
    // empuja los mandos). Este es el sitio: el hilo de mensajes, que es donde
    // ya se cambia el motor entero unas lineas mas abajo.
    //
    // Y HAY UNA DEUDA IGUAL QUE LA DEL INTERCAMBIO DE MOTOR, que se anota aqui
    // para no venderla como cosa hecha: este `engine` se puede estar usando en
    // `processBlock` mientras aqui se toca. No se ha empeorado (el intercambio
    // de motor ya lo hacia, y con el puntero entero), pero el `setType` de un
    // hueco encima es un caso mas. Arreglarlo de verdad es un mutex o una
    // suspension de audio que cubra AMBAS cosas, y eso es un cambio aparte.
    if (parameterID == IDs::fx1Type && engine != nullptr)
    {
        NEURONiK::DSP::GlobalParams bus;
        fillGlobalParams (bus);
        engine->setFxSlotType (0, static_cast<int> (newValue), bus.fx[0]);
        return;
    }

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

        // Prepare new engine, PERO solo si el host ya nos ha dicho su tasa.
        //
        // `getSampleRate()` NO es la tasa de la sesion hasta que el host la fija
        // (`setRateAndBufferSizeDetails`, que llama prepareToPlay): antes vale 0. Preparar
        // un motor con 0 lo deja con divisores a cero y el primer bloque con nota revienta
        // (0xC0000094, lo cazo ModelSlotTest). Sin tasa todavia, el motor se queda sin
        // preparar y lo prepara `prepareToPlay`, que es por donde pasa el motor inicial:
        // un host siempre llama a prepare antes de que haya audio que procesar.
        const auto hostSampleRate = getSampleRate();

        if (hostSampleRate > 0.0)
            newEngine->prepare(hostSampleRate, getBlockSize());
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

    if (engine->getType() == NEURONiK::DSP::ISynthesisEngine::Type::Neuronik)
    {
        auto* nEngine = static_cast<NEURONiK::DSP::NeuronikEngine*>(engine.get());
        ::NEURONiK::DSP::Synthesis::AdditiveVoice::Params vParams;
        vParams.oscLevel = apvts.getRawParameterValue(IDs::oscLevel)->load();
        vParams.attack = apvts.getRawParameterValue(IDs::envAttack)->load() * 1000.0f;
        vParams.decay = apvts.getRawParameterValue(IDs::envDecay)->load() * 1000.0f;
        vParams.sustain = apvts.getRawParameterValue(IDs::envSustain)->load();
        vParams.release = apvts.getRawParameterValue(IDs::envRelease)->load() * 1000.0f;
        vParams.filterCutoff = apvts.getRawParameterValue(IDs::filterCutoff)->load();
        vParams.filterRes = apvts.getRawParameterValue(IDs::filterRes)->load();
        vParams.fAttack = apvts.getRawParameterValue(IDs::filterAttack)->load() * 1000.0f;
        vParams.fDecay = apvts.getRawParameterValue(IDs::filterDecay)->load() * 1000.0f;
        vParams.fSustain = apvts.getRawParameterValue(IDs::filterSustain)->load();
        vParams.fRelease = apvts.getRawParameterValue(IDs::filterRelease)->load() * 1000.0f;
        vParams.morphX = apvts.getRawParameterValue(IDs::morphX)->load();
        vParams.morphY = apvts.getRawParameterValue(IDs::morphY)->load();
        vParams.morphZ = apvts.getRawParameterValue(IDs::morphZ)->load();
        vParams.morphZ2 = apvts.getRawParameterValue(IDs::morphZ2)->load();
        vParams.morphZ3 = apvts.getRawParameterValue(IDs::morphZ3)->load();
        vParams.inharmonicity = apvts.getRawParameterValue(IDs::oscInharmonicity)->load();
        vParams.roughness = apvts.getRawParameterValue(IDs::oscRoughness)->load();
        vParams.resonatorParity = apvts.getRawParameterValue(IDs::resonatorParity)->load();
        vParams.resonatorShift = apvts.getRawParameterValue(IDs::resonatorShift)->load();
        vParams.resonatorRollOff = apvts.getRawParameterValue(IDs::resonatorRolloff)->load();
        vParams.unisonDetune = apvts.getRawParameterValue(IDs::unisonDetune)->load();
        vParams.unisonSpread = apvts.getRawParameterValue(IDs::unisonSpread)->load();
        
        nEngine->setVoiceParams(vParams);

        ::NEURONiK::DSP::GlobalParams gParams;
        fillGlobalParams(gParams);
        nEngine->setGlobalParams(gParams);
    }
    else if (engine->getType() == NEURONiK::DSP::ISynthesisEngine::Type::Neurotik)
    {
        auto* ntEngine = static_cast<NEURONiK::DSP::NeurotikEngine*>(engine.get());
        ::NEURONiK::DSP::Synthesis::NeurotikVoice::Params ntParams;
        ntParams.level = apvts.getRawParameterValue(IDs::oscLevel)->load();
        ntParams.attack = apvts.getRawParameterValue(IDs::envAttack)->load() * 1000.0f;
        ntParams.decay = apvts.getRawParameterValue(IDs::envDecay)->load() * 1000.0f;
        ntParams.sustain = apvts.getRawParameterValue(IDs::envSustain)->load();
        ntParams.release = apvts.getRawParameterValue(IDs::envRelease)->load() * 1000.0f;
        ntParams.morphX = apvts.getRawParameterValue(IDs::morphX)->load();
        ntParams.morphY = apvts.getRawParameterValue(IDs::morphY)->load();
        ntParams.morphZ = apvts.getRawParameterValue(IDs::morphZ)->load();
        ntParams.morphZ2 = apvts.getRawParameterValue(IDs::morphZ2)->load();
        ntParams.morphZ3 = apvts.getRawParameterValue(IDs::morphZ3)->load();
        ntParams.excitationNoise = apvts.getRawParameterValue(IDs::oscExciteNoise)->load();
        ntParams.excitationColor = apvts.getRawParameterValue(IDs::excitationColor)->load();
        ntParams.impulseMix = apvts.getRawParameterValue(IDs::impulseMix)->load();
        ntParams.bowExcite = apvts.getRawParameterValue(IDs::oscExciteBow)->load();
        ntParams.resonatorResonance = apvts.getRawParameterValue(IDs::resonatorRes)->load();
        ntParams.unisonDetune = apvts.getRawParameterValue(IDs::unisonDetune)->load();
        ntParams.unisonSpread = apvts.getRawParameterValue(IDs::unisonSpread)->load();
        
        ntEngine->setVoiceParams(ntParams);

        ::NEURONiK::DSP::GlobalParams gParams;
        fillGlobalParams(gParams);
        ntEngine->setGlobalParams(gParams);
    }
}

void NEURONiKProcessor::fillGlobalParams(NEURONiK::DSP::GlobalParams& gParams)
{
    gParams.masterLevel = apvts.getRawParameterValue(IDs::masterLevel)->load();
    gParams.bpm = apvts.getRawParameterValue(IDs::masterBPM)->load();

    // El BUS del hueco 1 (2026-09-29). Los mandos van NORMALIZADOS porque el
    // hueco habla normalizado (`FxSlot::setParameter`), que es donde esta el
    // sesgo de la fila. Los huecos 2, 3 y 4 los mapean los mandos planos de mas
    // abajo, y se migran igual que este uno.
    gParams.fx[0].mix  = apvts.getRawParameterValue(IDs::fx1Mix)->load();
    gParams.fx[0].gain = apvts.getRawParameterValue(IDs::fx1Gain)->load();
    gParams.fx[0].params[0] = apvts.getRawParameterValue(IDs::fx1Param1)->load();
    gParams.fx[0].params[1] = apvts.getRawParameterValue(IDs::fx1Param2)->load();
    gParams.fx[0].params[2] = apvts.getRawParameterValue(IDs::fx1Param3)->load();
    gParams.fx[0].params[3] = apvts.getRawParameterValue(IDs::fx1Param4)->load();

    // Effects: the full parameter set is forwarded, not just the mixes, so the
    // rate/depth and size/damping/width controls actually reach the DSP.
    gParams.chorusRate = apvts.getRawParameterValue(IDs::fxChorusRate)->load();
    gParams.chorusDepth = apvts.getRawParameterValue(IDs::fxChorusDepth)->load();
    gParams.chorusMix = apvts.getRawParameterValue(IDs::fxChorusMix)->load();

    gParams.reverbSize = apvts.getRawParameterValue(IDs::fxReverbSize)->load();
    gParams.reverbDamping = apvts.getRawParameterValue(IDs::fxReverbDamping)->load();
    gParams.reverbWidth = apvts.getRawParameterValue(IDs::fxReverbWidth)->load();
    gParams.reverbMix = apvts.getRawParameterValue(IDs::fxReverbMix)->load();

    // Delay time: seconds in Free mode, note length in Tempo Sync.
    const int delayDivision = juce::roundToInt(apvts.getRawParameterValue(IDs::fxDelayDivision)->load());
    const bool delaySynced = juce::roundToInt(apvts.getRawParameterValue(IDs::fxDelaySync)->load()) == 1;
    gParams.delayTime = delaySynced
        ? static_cast<float>(NEURONiK::DSP::Core::secondsForDivision(delayDivision, gParams.bpm))
        : apvts.getRawParameterValue(IDs::fxDelayTime)->load();
    gParams.delayFB = apvts.getRawParameterValue(IDs::fxDelayFeedback)->load();

    gParams.lfo1.waveform = (int)apvts.getRawParameterValue(IDs::lfo1Waveform)->load();
    gParams.lfo1.rateHz = apvts.getRawParameterValue(IDs::lfo1RateHz)->load();
    gParams.lfo1.depth = apvts.getRawParameterValue(IDs::lfo1Depth)->load();
    gParams.lfo1.syncMode = juce::roundToInt(apvts.getRawParameterValue(IDs::lfo1SyncMode)->load());
    gParams.lfo1.rhythmicDivision = juce::roundToInt(apvts.getRawParameterValue(IDs::lfo1RhythmicDivision)->load());

    gParams.lfo2.waveform = (int)apvts.getRawParameterValue(IDs::lfo2Waveform)->load();
    gParams.lfo2.rateHz = apvts.getRawParameterValue(IDs::lfo2RateHz)->load();
    gParams.lfo2.depth = apvts.getRawParameterValue(IDs::lfo2Depth)->load();
    gParams.lfo2.syncMode = juce::roundToInt(apvts.getRawParameterValue(IDs::lfo2SyncMode)->load());
    gParams.lfo2.rhythmicDivision = juce::roundToInt(apvts.getRawParameterValue(IDs::lfo2RhythmicDivision)->load());

    // LA MATRIZ, con la traduccion compartida (State/ModMatrixFromState.h). El
    // nombre de cada campo y el orden de lectura viven alli, no aqui: asi el
    // rename de un id rompe el producto y el test a la vez, en vez de romper
    // solo la carga de presets en silencio (getRawParameterValue devuelve
    // nullptr para un id que ya no existe y el preset parece vacio).
    //
    // Lo que SI es de aqui es de donde se lee: los atomicos vivos del APVTS, no
    // apvts.state, que va con retardo. Por eso la traduccion es una plantilla
    // sobre un lector y no una funcion que reciba el estado.
    const auto modMatrix = readModMatrix (
        [this] (int slot, ModField field)
        {
            const auto* value = apvts.getRawParameterValue (modMatrixParameterId (slot, field));
            return value != nullptr ? value->load() : 0.0f;
        });

    for (int i = 0; i < kModMatrixSlots; ++i)
        gParams.modMatrix[i] = modMatrix[i];
}

void NEURONiKProcessor::enterMidiLearnMode(const juce::String& paramID)
{
    midiLearnActive.store(true);
    parameterToLearn = paramID;
    // El indice es lo unico que el hilo de audio puede leer sin carrera: el
    // proximo CC mapea AQUI (el consumidor esta en el barrido de CC).
    parameterToLearnIndex.store (NEURONiK::Main::MidiMappingManager::getParamIndex (paramID),
                                 std::memory_order_release);
}

void NEURONiKProcessor::clearMidiLearnForParameter(const juce::String& paramID)
{
    midiMappingManager->clearMapping(paramID);
}

void NEURONiKProcessor::applyPendingCcChanges()
{
    // HILO DE MENSAJES (lo llama el timer del editor): los CC que el bloque
    // mapeo contra la tabla esperan aqui. setValueNotifyingHost avisa al host,
    // repinta la pagina por parameterChanged y es la misma puerta que un
    // control nativo - un CC deja de ser un dato muerto.
    int start1, block1, start2, block2;
    ccValueFifo.prepareToRead (ccQueueSize, start1, block1, start2, block2);

    auto drainBlock = [this] (int start, int block)
    {
        for (int i = 0; i < block; ++i)
        {
            const auto& pending = ccValueQueue[(size_t) (start + i)];
            const auto& params = NEURONiK::Main::MidiMappingManager::getLearnableParams();

            if (pending.paramIndex < 0 || pending.paramIndex >= (int) params.size())
                continue;

            if (auto* parameter = apvts.getParameter (params[(size_t) pending.paramIndex]))
                parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, pending.normalized));
        }
    };

    if (block1 > 0) drainBlock (start1, block1);
    if (block2 > 0) drainBlock (start2, block2);

    ccValueFifo.finishedRead (block1 + block2);
}

void NEURONiKProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    
    // Run pending commands (e.g. Model Loading)
    processCommands();

    // Filter host MIDI by channel before anything else.
    // The on-screen keyboard is injected further down on purpose: it should always
    // sound, whatever channel this instance is listening to.
    NEURONiK::Midi::filterMidiBuffer (
        midiMessages,
        NEURONiK::Midi::channelFromChoiceIndex (
            juce::roundToInt (apvts.getRawParameterValue(IDs::midiChannel)->load())),
        channelFilteredMidi);

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

    // MIDI CC -> parameter (el eslabon que la tabla nunca tuvo): se barre el
    // bloque COMPLETO, ANTES del filtro de canal — el learn de hardware no
    // deberia depender del canal que escucha el instrumento. Los mapeados se
    // encolan normalizados y el hilo de mensajes los aplica con
    // setValueNotifyingHost (applyPendingCcChanges, lo llama el editor).
    for (const auto metadata : midiMessages)
    {
        const auto& message = metadata.getMessage();

        if (! message.isController())
            continue;

        const auto cc = message.getControllerNumber();
        const auto ccValue = (float) message.getControllerValue() / 127.0f;

        // LEARN ARMADO (el gesto que el MidiLearner nativo empuja y que la
        // pagina empuja por el bridge): el proximo CC gana y la tabla queda
        // aplicandose de ahi en adelante.
        const auto learnIndex = parameterToLearnIndex.load (std::memory_order_acquire);

        if (learnIndex >= 0)
        {
            midiMappingManager->setMappingByIndex (learnIndex, cc);
            parameterToLearnIndex.store (-1, std::memory_order_release);
            midiLearnActive.store (false, std::memory_order_relaxed);
            continue; // este CC asigna, no toca el parametro todavia
        }

        const auto paramIndex = midiMappingManager->getParamIndexForCC (cc);

        if (paramIndex < 0)
            continue;

        int w1, b1, w2, b2;
        ccValueFifo.prepareToWrite (1, w1, b1, w2, b2);

        if (b1 > 0)
            ccValueQueue[(size_t) w1] = { paramIndex, ccValue };

        ccValueFifo.finishedWrite (b1 + b2);
    }

    // Shape note-on velocities after the on-screen keyboard injection on purpose:
    // the curve is part of the instrument's response, not of one input port.
    // Linear reports no change and touches nothing, so nothing is added to the
    // audio thread when the parameter is at its default.
    NEURONiK::Midi::shapeMidiVelocities (
        midiMessages,
        juce::roundToInt (apvts.getRawParameterValue(IDs::velocityCurve)->load()),
        channelFilteredMidi);
    
    synchronizeEngineParameters();

    if (allNotesOffRequested.exchange(false, std::memory_order_relaxed) && engine != nullptr)
    {
        engine->allNotesOff();

        // The UI mirrors this mask on the page keyboard: a panic must not leave
        // stale highlights behind.
        for (auto& word : heldNotesMask)
            word.store (0u, std::memory_order_relaxed);
    }

    if (engine != nullptr)
    {
        // Frontera dsp::AudioBuffer <-> juce::AudioBuffer (motor sin JUCE):
        // vista zero-copy de los mismos canales (patron de
        // DspEngineFacade::process) — el motor escribe directamente en la
        // memoria del buffer del host.
        dsp::AudioBuffer<float> dspBufferView (buffer.getArrayOfWritePointers(),
                                               buffer.getNumChannels(),
                                               buffer.getNumSamples());

        // Frontera MIDI juce::MidiBuffer -> dsp::MidiBuffer (motor sin JUCE):
        // copia los bytes crudos del subconjunto que el motor interpreta, en
        // el mismo orden y con las mismas posiciones. engineMidiBuffer se
        // reutiliza entre bloques, asi que no asigna en el hilo de audio.
        NEURONiK::DSP::Runtime::copyToDspMidiBuffer (midiMessages, engineMidiBuffer);
        engine->renderNextBlock(dspBufferView, engineMidiBuffer);

        // External MIDI view for UI feedback (the WebUI page keyboard): fold this
        // block's note on/off into the 128-bit held mask. Relax order: the mask
        // is advisory (page key highlights), never synchronisation.
        for (const auto metadata : midiMessages)
        {
            const auto& message = metadata.getMessage();
            if (! message.isNoteOnOrOff())
                continue;

            const auto note = juce::jlimit (0, 127, message.getNoteNumber());
            const auto word = static_cast<size_t> (note >> 5);
            const auto bit = 1u << (note & 31);

            if (message.isNoteOn())
                heldNotesMask[word].fetch_or (bit, std::memory_order_relaxed);
            else
                heldNotesMask[word].fetch_and (~bit, std::memory_order_relaxed);
        }

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
        // El medidor de voces del panel (plugin): el MISMO dato que el meter
        // del worklet ensena en local, por el canal de la UI.
        uiVoiceCount.store(engine->getNumActiveVoices(), std::memory_order_relaxed);
        
        // --- Safe Modulation Indexing Update ---
        float mods[static_cast<int>(NEURONiK::ModulationTarget::Count)];
        engine->getModulationValues(mods, static_cast<int>(NEURONiK::ModulationTarget::Count));
        for (int i = 0; i < static_cast<int>(NEURONiK::ModulationTarget::Count); ++i)
        {
            modulationValues[i].store(mods[i], std::memory_order_relaxed);
        }
        
        // APVTS-derived UI telemetry (safe from any thread — tambien lo llama el
        // timer de la bancada WebView2).
        refreshUiTelemetryFromApvts();
    }

    // MIDI Thru. The engine always receives the input; the toggle only decides
    // whether it is echoed back to the host. Before, the buffer was passed through
    // unconditionally, so the (default off) switch did nothing.
    if (apvts.getRawParameterValue(IDs::midiThru)->load() < 0.5f)
        midiMessages.clear();
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
        synchronizeEngineParameters();
    }
}

bool NEURONiKProcessor::loadModel(const juce::File& file, int slot)
{
    // Failure is an ABSOLUTE return, not a fall-through: the WebUI answers modelError
    // when this is false, so "did anything change" has to be answerable here.
    if (slot < 0 || slot >= 4 || !file.exists())
        return false;
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
        // Note: Editor should listen to property changes instead of being called directly

        return true;
    }

    return false;
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

bool NEURONiKProcessor::getCurrentModel(int slot,
                                        std::array<float, 64>& amplitudes,
                                        std::array<float, 64>& frequencyOffsets,
                                        bool& isValid) const
{
    if (slot < 0 || slot >= 4)
        return false;

    const juce::String path = apvts.state.getProperty("modelPath" + juce::String(slot)).toString();

    if (path.isNotEmpty() && path != "EMPTY")
    {
        auto model = NEURONiK::Serialization::PresetManager::loadModelFromFile(juce::File(path));

        if (model.isValid)
        {
            amplitudes = model.amplitudes;
            frequencyOffsets = model.frequencyOffsets;
            isValid = true;
            return true;
        }
    }

    amplitudes.fill(0.0f);
    frequencyOffsets.fill(0.0f);
    isValid = false;
    return true;
}

bool NEURONiKProcessor::clearModelSlot(int slot)
{
    // Rango primero y sin excusas: el mismo criterio que loadModel, y con el
    // nombre en "EMPTY" porque es el que la pagina ya trata como ranura vacia
    // (localModels.js: name === '' || name === EMPTY_SLOT_NAME -> null).
    if (slot < 0 || slot >= 4)
        return false;

    // La MISMA cola que loadModel. El modelo por defecto (todo ceros, frameCount
    // 1) no lo valida nadie a proposito: Resonator::loadModel lo guarda tal cual,
    // y una ranura de ceros es una ranura sin timbre, no una con basura de pila
    // (SpectralModel inicializa sus arrays en el constructor).
    int start1, block1, start2, block2;
    commandFifo.prepareToWrite(1, start1, block1, start2, block2);

    const auto write = [this, slot] (int start)
    {
        auto& cmd = commandQueue[start];
        cmd.type = EngineCommand::LoadModel;
        cmd.slot = slot;
        cmd.modelData = NEURONiK::Common::SpectralModel();
        commandFifo.finishedWrite(1);
    };

    if (block1 > 0)
        write(start1);
    else if (block2 > 0)
        write(start2);
    // Si la cola esta llena el comando se cae (y se dice: la ranura queda como
    // estaba) en vez de mentir con un nombre EMPTY que el motor no tiene.

    modelNames[static_cast<size_t> (slot)] = "EMPTY";
    if (apvts.state.isValid())
        apvts.state.setProperty("modelPath" + juce::String(slot), "EMPTY", nullptr);

    return true;
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

void NEURONiKProcessor::refreshUiTelemetryFromApvts() noexcept
{
    // Envelope parameters for the visualizers.
    uiAttack.store(apvts.getRawParameterValue(IDs::envAttack)->load(), std::memory_order_relaxed);
    uiDecay.store(apvts.getRawParameterValue(IDs::envDecay)->load(), std::memory_order_relaxed);
    uiSustain.store(apvts.getRawParameterValue(IDs::envSustain)->load(), std::memory_order_relaxed);
    uiRelease.store(apvts.getRawParameterValue(IDs::envRelease)->load(), std::memory_order_relaxed);

    uiFAttack.store(apvts.getRawParameterValue(IDs::filterAttack)->load(), std::memory_order_relaxed);
    uiFDecay.store(apvts.getRawParameterValue(IDs::filterDecay)->load(), std::memory_order_relaxed);
    uiFSustain.store(apvts.getRawParameterValue(IDs::filterSustain)->load(), std::memory_order_relaxed);
    uiFRelease.store(apvts.getRawParameterValue(IDs::filterRelease)->load(), std::memory_order_relaxed);

    // XY Pad coordinates.
    uiMorphX.store(apvts.getRawParameterValue(IDs::morphX)->load(), std::memory_order_relaxed);
    uiMorphY.store(apvts.getRawParameterValue(IDs::morphY)->load(), std::memory_order_relaxed);
}

void NEURONiKProcessor::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) {}
void NEURONiKProcessor::valueTreeRedirected(juce::ValueTree& tree) { tree.addListener(this); reloadModels(); }

void NEURONiKProcessor::getSpectralDataForUI(float* destination64) const noexcept
{
    for (int i = 0; i < 64; ++i)
        destination64[i] = spectralDataForUI[i].load(std::memory_order_relaxed);
}

void NEURONiKProcessor::getEnvelopeLevelsForUI(float& amp, float& filter) const noexcept
{
    amp = uiEnvelope.load(std::memory_order_relaxed);
    filter = uiFEnvelope.load(std::memory_order_relaxed);
}

int NEURONiKProcessor::getVoiceCountForUI() const noexcept
{
    return uiVoiceCount.load(std::memory_order_relaxed);
}

float NEURONiKProcessor::getLfoValueForUI(int lfoIndex) const noexcept
{
    if (lfoIndex == 0) return lfo1ValueForUI.load(std::memory_order_relaxed);
    if (lfoIndex == 1) return lfo2ValueForUI.load(std::memory_order_relaxed);
    return 0.0f;
}

float NEURONiKProcessor::getModulationValueForUI(int targetIndex) const noexcept
{
    if (targetIndex >= 0 && targetIndex < static_cast<int>(NEURONiK::ModulationTarget::Count))
        return modulationValues[targetIndex].load(std::memory_order_relaxed);
    return 0.0f;
}

void NEURONiKProcessor::getMorphCoordinatesForUI(float& x, float& y) const noexcept
{
    x = uiMorphX.load(std::memory_order_relaxed);
    y = uiMorphY.load(std::memory_order_relaxed);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NEURONiKProcessor(); }
