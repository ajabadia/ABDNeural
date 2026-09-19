/*
  ==============================================================================

    ModelSlotTest.cpp
    Created: 19 Sep 2026
    Description: Las cuatro ranuras de modelo espectral A-D, con el procesador REAL.

                 Es el test que contesta a "cargar un .neuronikmodel se ve en la
                 pagina y suena", parte por parte:

                   1. FORMATO — un modelo exportado con el ModelMaker tiene que
                      cargar. La herramienta escribe JSON ({amplitudes[64],
                      frequencyOffsets[64], name, description}) y el lector esperaba
                      XML (<NEURONIK_MODEL amplitudes=".." offsets="..">), asi que
                      los ficheros de la propia herramienta NO cargaban: la ranura
                      se quedaba sin nombre y sin sonido. Aqui se pinchan los DOS
                      dialectos y lo que no es un modelo.
                   2. RANURAS — `loadModel(file, slot)` acepta las cuatro y las
                      nombra; un fichero inservible o inexistente devuelve false y NO
                      toca la ranura (el nombre dice lo que hay, que era el defecto
                      del panel nativo).
                   3. PRESET — un preset lleva `modelPath<slot>`, asi que cargarlo
                      tiene que recargar sus modelos: es lo que hace que un preset
                      guardado suene igual al reabrirlo.
                   4. CABLE — con el adaptador REAL del puente, el `modelsState` del
                      snapshot publica los nombres de las cuatro ranuras. OJO: no se
                      llama a `loadModel()` del adaptador a proposito, que ese abre
                      un dialogo del sistema.
                   5. SUENA — con una nota sonando, las esquinas de morph (A=0,0;
                      B=1,0; C=0,1; D=1,1) suenan EXACTAMENTE la ranura que les toca:
                      la tabla de parciales del motor (`getSpectralData`, que es la
                      misma fuente que alimentaba el visualizador nativo y la que el
                      procesador publica en `spectralDataForUI`) tiene el parcial del
                      modelo de esa ranura. Es el contrato que dibujaba el XYPad.
                   6. ANTES DE LA TASA — cambiar de motor con el procesador SIN preparar
                      (lo que puede hacer un host que restaure un preset antes de
                      darnos su sample rate) no puede dejar un motor con divisores a
                      cero. Este test lo cazo como un 0xC0000094 en la seccion 5.

                 Los ficheros de prueba se escriben en un directorio temporal y se
                 borran al terminar: no se toca ni el directorio de presets del
                 usuario ni ningun modelo de verdad.

  ==============================================================================
*/

#include "../Source/Common/SpectralModel.h"
#include "../Source/Main/NEURONiKProcessor.h"
#include "../Source/Serialization/PresetManager.h"
#include "../Source/State/ParameterDefinitions.h"
#include "../Source/WebUI/BridgeAdapters.h"
#include "../Source/WebUI/ParameterBridge.h"

#include <array>
#include <iostream>
#include <vector>

namespace
{
    using namespace NEURONiK;

    int failures = 0;

    void check (bool condition, const juce::String& description)
    {
        if (condition)
        {
            std::cout << "  [ok]   " << description << '\n';
            return;
        }

        std::cout << "  [FAIL] " << description << '\n';
        ++failures;
    }

    bool closeEnough (float a, float b, float tolerance = 1.0e-3f)
    {
        return std::abs (a - b) <= tolerance;
    }

    //==============================================================================
    // Ficheros de prueba
    //==============================================================================

    /**
     * Un modelo como lo exporta el ModelMaker: JSON con `amplitudes` y
     * `frequencyOffsets` (los 64 parciales) mas `name`/`description`. La forma se
     * copia de `MainComponent::exportModel` a proposito: si la herramienta cambia
     * de formato, este test tiene que seguir escribiendo el MISMO que se lee.
     */
    juce::File writeModelMakerFile (const juce::File& directory, const juce::String& name,
                                    int partialIndex)
    {
        const auto file = directory.getChildFile (name + ".neuronikmodel");

        juce::DynamicObject::Ptr modelObject = new juce::DynamicObject();
        juce::Array<juce::var> amplitudes;
        juce::Array<juce::var> frequencyOffsets;

        for (int i = 0; i < 64; ++i)
        {
            amplitudes.add (i == partialIndex ? 1.0f : 0.0f);
            frequencyOffsets.add (0.0f);
        }

        modelObject->setProperty ("amplitudes", amplitudes);
        modelObject->setProperty ("frequencyOffsets", frequencyOffsets);
        modelObject->setProperty ("name", name);
        modelObject->setProperty ("description", "Created with NEURONiK Model Maker");

        file.replaceWithText (juce::JSON::toString (juce::var (modelObject.get())));

        return file;
    }

    /** El dialecto XML que el lector esperaba (`amplitudes`/`offsets` en CSV). */
    juce::File writeLegacyXmlFile (const juce::File& directory, const juce::String& name,
                                   int partialIndex)
    {
        const auto file = directory.getChildFile (name + ".neuronikmodel");

        juce::XmlElement xml ("NEURONIK_MODEL");
        juce::StringArray amplitudes;
        juce::StringArray offsets;

        for (int i = 0; i < 64; ++i)
        {
            amplitudes.add (i == partialIndex ? "1" : "0");
            offsets.add ("0");
        }

        xml.setAttribute ("amplitudes", amplitudes.joinIntoString (","));
        xml.setAttribute ("offsets", offsets.joinIntoString (","));

        file.replaceWithText (xml.toString());

        return file;
    }

    /** Un fichero con extension de modelo que no es un modelo. */
    juce::File writeBogusFile (const juce::File& directory)
    {
        const auto file = directory.getChildFile ("no-es-un-modelo.neuronikmodel");

        file.replaceWithText ("<?xml version=\"1.0\"?><PARAMS id=\"algo\" value=\"1\"/>");

        return file;
    }

    //==============================================================================
    // El procesador
    //==============================================================================

    void setParameter (NEURONiKProcessor& processor, const juce::String& id, float real)
    {
        if (auto* parameter = processor.getAPVTS().getParameter (id))
            parameter->setValueNotifyingHost (parameter->getNormalisableRange().convertTo0to1 (real));
    }

    /** Un bloque de audio con (o sin) una nota, que es lo que mueve el motor. */
    void render (NEURONiKProcessor& processor, int numBlocks, int blockSize, int noteToPlay)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);

        for (int block = 0; block < numBlocks; ++block)
        {
            juce::MidiBuffer midi;

            if (noteToPlay >= 0 && block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, noteToPlay, 0.9f), 0);

            buffer.clear();
            processor.processBlock (buffer, midi);
        }
    }

    /** La tabla de parciales del motor, por donde la publica el procesador. */
    std::array<float, 64> readPartialTable (NEURONiKProcessor& processor)
    {
        std::array<float, 64> table {};

        for (int i = 0; i < 64; ++i)
            table[(size_t) i] = processor.spectralDataForUI[(size_t) i].load();

        return table;
    }

    float bufferRms (const juce::AudioBuffer<float>& buffer)
    {
        double sum = 0.0;
        int samples = 0;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto value = static_cast<double> (buffer.getSample (channel, i));
                sum += value * value;
                ++samples;
            }

        return samples > 0 ? static_cast<float> (std::sqrt (sum / samples)) : 0.0f;
    }
}

int main()
{
    // Sin bufer a proposito: si el test muere a mitad (este camino toca el motor de
    // audio de verdad, con sus divisiones), un `std::cout` con bufer se lleva por
    // delante TODO lo comprobado hasta el fallo y no queda ni rastro de donde fue.
    std::cout << std::unitbuf;
    std::cout << "Model slots A-D (el procesador real)\n";

    const auto testDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                   .getChildFile ("neuronik-model-slots-test");

    testDirectory.deleteRecursively();
    testDirectory.createDirectory();

    // Un parcial distinto por ranura, y todos IMPARES (indices pares: 0, 2, 4, 6),
    // porque el balance par/impar (`resonatorParity`) apaga los pares cuando vale 0.
    const std::array<int, 4> partialOf { 0, 2, 4, 6 };

    const auto fileA = writeModelMakerFile (testDirectory, "model-a", partialOf[0]);
    const auto fileB = writeModelMakerFile (testDirectory, "model-b", partialOf[1]);
    const auto fileC = writeModelMakerFile (testDirectory, "model-c", partialOf[2]);
    const auto fileD = writeModelMakerFile (testDirectory, "model-d", partialOf[3]);
    const auto legacyFile = writeLegacyXmlFile (testDirectory, "model-xml", 10);
    const auto bogusFile = writeBogusFile (testDirectory);
    const auto missingFile = testDirectory.getChildFile ("no-existe.neuronikmodel");

    const std::array<juce::File, 4> modelFiles { fileA, fileB, fileC, fileD };
    const std::array<juce::String, 4> modelNames { "model-a", "model-b", "model-c", "model-d" };

    //==============================================================================
    // 1. FORMATO
    //==============================================================================

    std::cout << "\nFormato del fichero\n";

    {
        const auto jsonModel = Serialization::PresetManager::loadModelFromFile (fileA);

        check (jsonModel.isValid,
               "el .neuronikmodel que exporta el ModelMaker (JSON) se carga");
        check (jsonModel.isValid
                   && closeEnough (jsonModel.amplitudes[(size_t) partialOf[0]], 1.0f)
                   && closeEnough (jsonModel.amplitudes[1], 0.0f),
               "y trae sus 64 parciales (el declarado a 1, el resto a 0)");

        const auto xmlModel = Serialization::PresetManager::loadModelFromFile (legacyFile);

        check (xmlModel.isValid
                   && closeEnough (xmlModel.amplitudes[10], 1.0f),
               "el dialecto XML (<NEURONIK_MODEL amplitudes=\"..\" offsets=\"..\"/>) sigue cargando");

        check (! Serialization::PresetManager::loadModelFromFile (bogusFile).isValid,
               "un fichero que no es un modelo se rechaza");
        check (! Serialization::PresetManager::loadModelFromFile (missingFile).isValid,
               "un fichero que no existe se rechaza");
    }

    //==============================================================================
    // 2. RANURAS
    //==============================================================================

    std::cout << "\nLas cuatro ranuras\n";

    NEURONiKProcessor processor;

    {
        bool allAccepted = true;

        for (int slot = 0; slot < 4; ++slot)
            allAccepted = processor.loadModel (modelFiles[(size_t) slot], slot) && allAccepted;

        check (allAccepted, "loadModel acepta las cuatro ranuras (0 = A ... 3 = D)");

        bool namesOk = true;

        for (int slot = 0; slot < 4; ++slot)
            namesOk = namesOk && processor.getModelNames()[(size_t) slot] == modelNames[(size_t) slot];

        check (namesOk, "y cada ranura queda con el nombre del fichero cargado");

        check (! processor.loadModel (bogusFile, 1),
               "un fichero inservible devuelve false en vez de callarse");
        check (processor.getModelNames()[1] == modelNames[1],
               "y NO renombra la ranura: el nombre dice lo que el motor tiene");
        check (! processor.loadModel (missingFile, 1), "un fichero inexistente tambien devuelve false");
        check (! processor.loadModel (fileA, 4) && ! processor.loadModel (fileA, -1),
               "una ranura fuera de rango devuelve false");

        // El espejo del cable: lo que publica `modelsState` sale de aqui.
        std::array<float, 64> amplitudes {};
        std::array<float, 64> frequencyOffsets {};
        auto isValid = false;

        check (processor.getCurrentModel (2, amplitudes, frequencyOffsets, isValid)
                   && isValid
                   && closeEnough (amplitudes[(size_t) partialOf[2]], 1.0f),
               "getCurrentModel (el espejo del cable) lee la ranura desde su fichero");
        check (processor.getCurrentModel (9, amplitudes, frequencyOffsets, isValid) == false,
               "y una ranura fuera de rango no inventa un modelo");
    }

    //==============================================================================
    // 3. PRESET (modelPath<slot>)
    //==============================================================================

    std::cout << "\nUn preset lleva sus modelos\n";

    {
        NEURONiKProcessor fresh;
        auto state = fresh.getAPVTS().copyState();

        for (int slot = 0; slot < 4; ++slot)
            state.setProperty ("modelPath" + juce::String (slot),
                               modelFiles[(size_t) slot].getFullPathName(), nullptr);

        const auto presetFile = testDirectory.getChildFile ("con-modelos.neuronikpreset");
        auto xml = state.createXml();

        check (xml != nullptr && xml->writeTo (presetFile),
               "el preset de prueba se escribe con las cuatro rutas (modelPath0..3)");

        fresh.getPresetManager().loadPresetFromFile (presetFile);

        bool reloaded = true;

        for (int slot = 0; slot < 4; ++slot)
            reloaded = reloaded && fresh.getModelNames()[(size_t) slot] == modelNames[(size_t) slot];

        check (reloaded, "cargarlo RECARGA los modelos que referencia (mismo nombre por ranura)");
    }

    //==============================================================================
    // 4. EL CABLE (adaptador real)
    //==============================================================================

    std::cout << "\nEl cable\n";

    {
        WebUI::ParameterBridge bridge (processor.getAPVTS());
        std::vector<juce::var> sent;

        bridge.setSender ([&sent] (const juce::var& message) { sent.push_back (message); });

        auto loadedSlots = 0;
        juce::String lastError;

        WebUI::EngineModelsAdapter adapter (
            processor,
            [&loadedSlots] (int) { ++loadedSlots; },
            [&lastError] (int, const juce::String& detail) { lastError = detail; });

        bridge.setModelController (&adapter);
        bridge.sendFullSnapshot();

        juce::var modelsMessage;

        for (const auto& message : sent)
            if (const auto* object = message.getDynamicObject();
                object != nullptr && object->getProperty ("action").toString() == WebUI::BridgeActions::modelsState)
                modelsMessage = message;

        const auto* slots = modelsMessage.getDynamicObject() != nullptr
                                ? modelsMessage.getDynamicObject()->getProperty ("slots").getArray()
                                : nullptr;

        check (slots != nullptr && slots->size() == 4,
               "el snapshot publica un modelsState con las cuatro ranuras");

        bool namesOnWire = slots != nullptr && slots->size() == 4;

        for (int slot = 0; slot < 4 && namesOnWire; ++slot)
        {
            const auto* entry = (*slots)[slot].getDynamicObject();

            namesOnWire = entry != nullptr
                              && entry->getProperty ("name").toString() == modelNames[(size_t) slot]
                              && entry->getProperty ("isValid").isBool()
                              && static_cast<bool> (entry->getProperty ("isValid"));
        }

        check (namesOnWire, "con el nombre y la validez de cada una (lo que pinta la pagina)");
        check (adapter.getModelName (0) == modelNames[0] && adapter.getModelName (9).isEmpty()
                   && adapter.getModelName (-1).isEmpty(),
               "el adaptador nombra sus ranuras y no inventa las de fuera");
        check (loadedSlots == 0 && lastError.isEmpty(),
               "cargar modelos NO ha abierto ningun dialogo (el test no llama a loadModel del adaptador)");
    }

    //==============================================================================
    // 5. SUENA: cada esquina de morph suena su ranura
    //==============================================================================

    std::cout << "\nSuenan (parciales de cada ranura)\n";

    {
        NEURONiKProcessor audio;
        constexpr int blockSize = 512;
        constexpr int note = 60;

        // La tasa y el tamano del bloque los fija el HOST, no `prepareToPlay` (que es
        // solo una llamada virtual): `setRateAndBufferSizeDetails` es lo que hace que
        // `getSampleRate()` no sea 0. Importa aqui porque cambiar de motor construye un
        // motor nuevo y lo prepara EL con esos getters: sin esta linea, el motor nuevo
        // se prepara con 0 y el primer bloque con nota divide entre cero (lo cazo este
        // test en 0xC0000094 antes de que la guarda del constructor se comiera el caso).
        audio.setRateAndBufferSizeDetails (48000.0, blockSize);
        audio.prepareToPlay (48000.0, blockSize);

        // Estado conocido, independiente de los defaults del dia que se escribio
        // este test: motor aditivo, sin filtro que recorte, envolvente sin rampas
        // largas, roll-off neutro y paridad que deja sonar los armonicos IMPARES
        // (que son los de las cuatro ranuras de prueba).
        setParameter (audio, State::IDs::engineType, 0.0f);
        setParameter (audio, State::IDs::filterCutoff, 20000.0f);
        setParameter (audio, State::IDs::envAttack, 0.001f);
        setParameter (audio, State::IDs::envSustain, 1.0f);
        setParameter (audio, State::IDs::resonatorRolloff, 1.0f);
        setParameter (audio, State::IDs::resonatorParity, 0.25f);
        setParameter (audio, State::IDs::oscLevel, 1.0f);
        setParameter (audio, State::IDs::masterLevel, 0.8f);

        for (int slot = 0; slot < 4; ++slot)
            audio.loadModel (modelFiles[(size_t) slot], slot);

        // Una nota sonando (la tabla de parciales es la de una VOZ ACTIVA) y un
        // bloque de margen para que el FIFO de comandos llegue al motor.
        render (audio, 4, blockSize, note);

        // Esquinas: A (0,0), B (1,0), C (0,1), D (1,1) — el reparto que dibujaba el
        // XYPad nativo. Cada esquina tiene que sonar SU parcial.
        const std::array<std::array<float, 2>, 4> morphCorner { { { 0.0f, 0.0f }, { 1.0f, 0.0f },
                                                                 { 0.0f, 1.0f }, { 1.0f, 1.0f } } };
        const std::array<const char*, 4> slotLabel { "A (0,0)", "B (1,0)", "C (0,1)", "D (1,1)" };

        for (int slot = 0; slot < 4; ++slot)
        {
            setParameter (audio, State::IDs::morphX, morphCorner[(size_t) slot][0]);
            setParameter (audio, State::IDs::morphY, morphCorner[(size_t) slot][1]);

            // La rampa de morph es de 20 ms: 8 bloques (85 ms) de sobra.
            render (audio, 8, blockSize, -1);

            const auto table = readPartialTable (audio);
            const auto ownPartial = table[(size_t) partialOf[(size_t) slot]];

            auto neighboursAreSilent = true;

            for (int other = 0; other < 4; ++other)
                if (other != slot && table[(size_t) partialOf[(size_t) other]] > 0.05f)
                    neighboursAreSilent = false;

            check (ownPartial > 0.9f && neighboursAreSilent,
                   juce::String ("morph ") + slotLabel[(size_t) slot] + ": suena el parcial del modelo de la ranura "
                       + juce::String (static_cast<char> ('A' + slot))
                       + " (" + juce::String (ownPartial, 3) + ") y ninguno de los otros");
        }

        // Y que ademas haya audio de verdad, no solo una tabla bonita.
        setParameter (audio, State::IDs::morphX, 0.0f);
        setParameter (audio, State::IDs::morphY, 0.0f);
        render (audio, 8, blockSize, -1);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer silence;
        buffer.clear();
        audio.processBlock (buffer, silence);

        check (bufferRms (buffer) > 1.0e-4f,
               "y la nota suena (RMS " + juce::String (bufferRms (buffer), 5) + ")");
    }

    //==============================================================================
    // 6. ANTES DE LA TASA: cambiar de motor sin host preparado no puede dividir entre cero
    //    (el crash 0xC0000094 que este test cazo en la seccion 5, ya con la guarda puesta)
    //==============================================================================

    std::cout << "\nCambiar de motor antes de que el host fije la tasa\n";

    {
        NEURONiKProcessor preHost;

        check (preHost.getSampleRate() <= 0.0,
               "el procesador nace SIN tasa: la fija el host (`setRateAndBufferSizeDetails`)");

        // Un preset restaurado antes de que el host prepare puede mover `engineType`.
        // Con la tasa a 0, el motor nuevo se construia preparado con 0 (divisores a cero).
        setParameter (preHost, State::IDs::engineType, 0.0f);

        preHost.setRateAndBufferSizeDetails (48000.0, 512);
        preHost.prepareToPlay (48000.0, 512);
        setParameter (preHost, State::IDs::masterLevel, 0.8f);

        render (preHost, 8, 512, 60);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer silence;
        buffer.clear();
        preHost.processBlock (buffer, silence);

        check (bufferRms (buffer) > 1.0e-4f,
               "y el motor preparado despues suena (RMS " + juce::String (bufferRms (buffer), 5)
                   + ") en vez de reventar");
    }

    testDirectory.deleteRecursively();

    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.") << '\n';
    return failures == 0 ? 0 : 1;
}
