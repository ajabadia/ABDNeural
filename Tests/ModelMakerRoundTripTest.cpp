/*
  ==============================================================================

    ModelMakerRoundTripTest.cpp
    Created: 20 Sep 2026
    Description: El flujo COMPLETO del ModelMaker reproducido de punta a punta
                 con analisis REAL — el flujo que la GUI conduce a mano,
                 conducido aqui por codigo:

                   Audio real (sintesis matematica de una nota con 64 armonicos
                   de amplitud descendente) -> detectPitch (HPS del ModelMaker)
                   -> analyze(root) -> EXPORT en el MISMO dialecto JSON que
                   escribe la GUI -> processor.loadModel(file, slot) en las
                   cuatro ranuras A-D -> verificacion de tres niveles: lo que
                   hay EN LA RANURA (estado del procesador, lo que la ficha
                   A-D muestra), la TABLA del motor con una nota sonando (lo
                   que SUENA de verdad) y el modelsState del puente (lo que la
                   pagina web ensenaria).

                 Lo que este test NO cubre: pulsar LOAD AUDIO -> ANALYZE ->
                 EXPORT con raton en la GUI del ModelMaker (la logica es
                 exactamente la que aqui se invoca; el dialogo de fichero y
                 los widgets no son automatizables sin humano delante).

  ==============================================================================
*/

#include "../Source/ModelMaker/Analysis/SpectralAnalyzer.h"
#include "../Source/Common/SpectralModel.h"
#include "../Source/DSP/FrameSampler.h"
#include "../Source/Serialization/PresetManager.h"
#include "../Source/Common/SpectralModelWriter.h"
#include "../Source/Main/NEURONiKProcessor.h"
#include "../Source/State/ParameterDefinitions.h"
#include "../Source/WebUI/BridgeAdapters.h"
#include "../Source/WebUI/ParameterBridge.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>

#include <array>
#include <cmath>
#include <iostream>

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

    /** @brief El setter de parametros de la casa: unidades reales -> normalizado. */
    void setParameter (NEURONiKProcessor& processor, const juce::String& id, float real)
    {
        if (auto* parameter = processor.getAPVTS().getParameter (id))
            parameter->setValueNotifyingHost (parameter->getNormalisableRange().convertTo0to1 (real));
    }

    /**
     * @brief Renderiza bloques y devuelve el RMS del ULTIMO (0..~1).
     *
     * Con una nota en el primer bloque, un RMS sano dice que el modelo suena
     * de verdad — el A/B a oido del ModelMaker, aqui como numero.
     */
    float render (NEURONiKProcessor& processor, int numBlocks, int blockSize, int noteToPlay)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        double sum = 0.0;
        int samples = 0;

        for (int block = 0; block < numBlocks; ++block)
        {
            juce::MidiBuffer midi;

            if (noteToPlay >= 0 && block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, noteToPlay, 0.9f), 0);

            buffer.clear();
            processor.processBlock (buffer, midi);

            if (block == numBlocks - 1)
            {
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                    {
                        const auto value = (double) buffer.getSample (channel, i);
                        sum += value * value;
                        ++samples;
                    }
            }
        }

        return (float) std::sqrt (sum / (double) juce::jmax (1, samples));
    }

    /** @brief La tabla de parciales del motor, por donde la publica el procesador. */
    std::array<float, 64> readPartialTable (NEURONiKProcessor& processor)
    {
        std::array<float, 64> table {};

        for (int i = 0; i < 64; ++i)
            table[(size_t) i] = processor.spectralDataForUI[(size_t) i].load();

        return table;
    }

    /**
     * @brief El "sample" del flujo LOAD AUDIO, sintetizado: una nota con los
     *        64 armonicos que el analizador va a buscar, con decaimiento 1/n
     *        (el de un instrumento acustico: la fundamental manda y cada
     *        armonico es audible). Con A4 (440 Hz) caben ~75 periodos en la
     *        ventana FFT del analizador (8192 muestras = 170 ms), lo que una
     *        grabacion real de esa nota SIEMPRE tiene y una nota grave NO.
     */
    juce::AudioBuffer<float> generateHarmonicTone (double sampleRate, float f0, int numSamples)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const double t = (double) i / sampleRate;
            double sample = 0.0;

            for (int n = 1; n <= 64; ++n)
            {
                const auto decay = 1.0 / (double) n;          // 1/n: espectro de instrumento real
                sample += decay * std::sin (2.0 * juce::MathConstants<double>::pi * f0 * n * t);
            }

            const auto value = (float) (0.6 * sample / 3.0);  // /~pi^2/2: normalizacion aproximada de 1/n
            left[i] = value;
            right[i] = value;
        }

        return buffer;
    }

    /**
     * @brief El dialecto .neuronikmodel de PRODUCCION. Desde la FASE 11.1 el
     *        escritor es uno solo (Common::writeModelToFile, en
     *        Source/Common/SpectralModelWriter.h) y lo usan la GUI, este test y
     *        la sonda RealWav. Antes este helper replicaba a mano lo que
     *        exportModel() escribia: con las capas (v2.1) eso ya era la tercera
     *        copia de un formato que crece, y probar una copia no prueba el
     *        fichero que sale de la herramienta.
     */
    bool writeModelJson (const juce::File& file, const NEURONiK::Common::SpectralModel& model,
                         const juce::String& name)
    {
        return Common::writeModelToFile (file, model, name,
                                        "Created with NEURONiK Model Maker");
    }

    /** @brief Igualdad de modelos para el chequeo ranura-vs-analisis. */
    bool modelsEqual (const NEURONiK::Common::SpectralModel& a,
                      const std::array<float, 64>& amps, const std::array<float, 64>& offs,
                      float epsilon)
    {
        for (int i = 0; i < 64; ++i)
        {
            if (std::abs (a.amplitudes[(size_t) i] - amps[(size_t) i]) > epsilon)
                return false;

            if (std::abs (a.frequencyOffsets[(size_t) i] - offs[(size_t) i]) > epsilon)
                return false;
        }

        return true;
    }

} // namespace

int main()
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr float f0 = 440.0f;                  // A4: ~75 periodos dentro de la ventana FFT del analizador
    constexpr int numSamples = (int) sampleRate;  // 1 segundo: el "sample" del flujo LOAD AUDIO
    const char* slotLetter = "ABCD";

    std::cout << "ModelMaker round trip (audio real -> analisis -> export -> ranuras A-D)\n";

    // --- 1. El "sample" del flujo LOAD AUDIO --------------------------------------
    std::cout << "\nAudio\n";
    const auto source = generateHarmonicTone (sampleRate, f0, numSamples);
    check (source.getNumSamples() == numSamples && source.getNumChannels() == 2,
           "el sample sintetizado esta listo (1 s, estereo)");

    // --- 2. La herramienta: el analizador DEL MODELMAKER ---------------------------
    //       (el mismo codigo que la GUI conduce: detectPitch -> analyze)
    std::cout << "\nAnalyzer\n";
    NEURONiK::ModelMaker::Analysis::SpectralAnalyzer analyzer;

    const auto detected = analyzer.detectPitch (source, sampleRate);
    check (std::abs (detected - f0) < 10.0f,
           "detectPitch (HPS) encuentra la fundamental: " + juce::String (detected, 2) + " Hz (esperados ~220)");

    const auto model = analyzer.analyze (source, sampleRate, detected);
    check (model.amplitudes[0] > 0.5f && model.amplitudes[0] <= 1.0f + 1.0e-4f,
           "analyze produce la tabla normalizada (fundamental dominante: "
               + juce::String (model.amplitudes[0], 3) + ")");

    // --- 3. EXPORT: el mismo dialecto JSON que la GUI escribe ----------------------
    std::cout << "\nExport\n";
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("neuronik-modelmaker-roundtrip");
    dir.deleteRecursively();
    dir.createDirectory();

    std::array<juce::File, 4> modelFiles;

    for (int slot = 0; slot < 4; ++slot)
    {
        // Un modelo por ranura (mismo analisis, nombre por fichero): el flujo
        // real carga un .neuronikmodel en cada ranura A-D.
        const auto name = juce::String ("Harmonic Sweep 440Hz ") + slotLetter[slot];
        modelFiles[(size_t) slot] = dir.getChildFile (name + ".neuronikmodel");
        check (writeModelJson (modelFiles[(size_t) slot], model, name),
               "export " + juce::String::charToString (static_cast<juce::juce_wchar> (slotLetter[slot])) + ": " + modelFiles[(size_t) slot].getFileName());
    }

    // El fichero es JSON legible y trae la tabla del analisis.
    {
        const auto parsed = juce::JSON::parse (modelFiles[0]);
        const auto* obj = parsed.getDynamicObject();
        bool jsonOk = obj != nullptr;

        if (jsonOk)
        {
            const auto* amps = obj->getProperty ("amplitudes").getArray();
            jsonOk = amps != nullptr && amps->size() == 64
                         && std::abs ((double) amps->getUnchecked (0) - (double) model.amplitudes[0]) < 1.0e-4;
        }

        check (jsonOk, "el .neuronikmodel exportado se relee como JSON con las 64 amplitudes");
    }

    // --- 4. CARGA en las ranuras A-D del plugin ------------------------------------
    std::cout << "\nSlots\n";
    {
        NEURONiKProcessor audio;

        // La tasa la fija el HOST (setRateAndBufferSizeDetails), como en la seccion 6
        // de ModelSlotTest: sin ella un motor nuevo se prepara con 0 y el primer
        // bloque con nota divide entre cero.
        audio.setRateAndBufferSizeDetails (sampleRate, blockSize);
        audio.prepareToPlay (sampleRate, blockSize);

        // Estado conocido: motor aditivo, sin filtro, envolvente sin rampas largas
        // (los mismos defaults de la seccion "SUENA" de ModelSlotTest).
        setParameter (audio, State::IDs::engineType, 0.0f);
        setParameter (audio, State::IDs::filterCutoff, 20000.0f);
        setParameter (audio, State::IDs::envAttack, 0.001f);
        setParameter (audio, State::IDs::envSustain, 1.0f);
        setParameter (audio, State::IDs::resonatorRolloff, 1.0f);
        setParameter (audio, State::IDs::resonatorParity, 0.25f);
        setParameter (audio, State::IDs::oscLevel, 1.0f);
        setParameter (audio, State::IDs::masterLevel, 0.8f);

        for (int slot = 0; slot < 4; ++slot)
            check (audio.loadModel (modelFiles[(size_t) slot], slot),
                   juce::String ("ranura ") + slotLetter[slot] + " acepta el modelo real ("
                       + modelFiles[(size_t) slot].getFileName() + ")");

        // Lo que hay EN LA RANURA es el modelo del fichero (lo que la ficha A-D
        // y el pad muestran; lo que el snapshot publica).
        std::array<float, 64> amps {}, offs {};
        bool valid = false;
        audio.getCurrentModel (0, amps, offs, valid);

        check (valid, "la ranura A queda VALIDA tras la carga");
        check (modelsEqual (model, amps, offs, 1.0e-3f),
               "el contenido de la ranura A coincide con el modelo analizado (<= 1e-3)");

        const auto& names = audio.getModelNames();
        bool namesOk = true;

        for (int slot = 0; slot < 4; ++slot)
            if (names[(size_t) slot] != modelFiles[(size_t) slot].getFileNameWithoutExtension())
                namesOk = false;

        check (namesOk, "las cuatro ranuras llevan el nombre del fichero (la ficha A-D lo ensena)");

        // --- 5. SUENA: la tabla del motor con una nota sonando ----------------------
        //       (el A/B a oido del ModelMaker, aqui como numero: RMS + tabla)
        std::cout << "Engine\n";

        const auto rms = render (audio, 8, blockSize, 69);   // A4 = MIDI 69 = la f0 del sample
        check (rms > 0.05f, "con la nota hay AUDIO de verdad (RMS " + juce::String (rms, 4) + ")");

        const auto table = readPartialTable (audio);

        // La tabla del resonador es el modelo morfado RE-ESCALADO: divide cada
        // parcial entre la SUMA de los 64 (Resonator.cpp: partialAmplitudes[i] =
        // tempAmps[i] * invNorm). Con un modelo REAL de banda ancha (a diferencia
        // de los de un parcial del ModelSlotTest, que salen 1.0) la tabla muestra
        // CUOTAS, no amplitudes: lo contractual es la FORMA.
        double tableSum = 0.0;
        float tableMax = 0.0f;
        int tableArgMax = 0;

        for (int i = 0; i < 64; ++i)
        {
            tableSum += table[(size_t) i];

            if (table[(size_t) i] > tableMax)
            {
                tableMax = table[(size_t) i];
                tableArgMax = i;
            }
        }

        check (tableArgMax == 0,
               "el maximo de la tabla es la fundamental (parcial 1), como en el modelo");
        check (std::abs (tableSum - 1.0) < 0.05,
               "la tabla esta normalizada por suma (suma = " + juce::String ((float) tableSum, 3) + ")");

        // Paridad conocida (parity=0.25 -> impares x1.5, pares x0.5): la RAZON entre
        // dos impares de la tabla tiene que salir de la razon del modelo analizado.
        const auto oddRatioTable = table[0] / juce::jmax (1.0e-6f, table[2]);
        const auto oddRatioModel = model.amplitudes[0] / juce::jmax (1.0e-6f, model.amplitudes[2]);
        check (std::abs (oddRatioTable - oddRatioModel) < 0.25 * oddRatioModel,
               "la forma de la tabla sale del modelo (razon parciales 1:3, "
                   + juce::String (oddRatioTable, 3) + " vs " + juce::String (oddRatioModel, 3) + ")");

        // --- 6. EL PUENTE: lo que la pagina web ensenaria --------------------------
        std::cout << "Bridge\n";
        WebUI::EngineModelsAdapter adapter (
            audio,
            [] (int) {},
            [] (int, const juce::String&) {});

        std::array<float, 64> bridgeAmps {}, bridgeOffs {};
        bool bridgeValid = false;
        adapter.getCurrentModel (0, bridgeAmps, bridgeOffs, bridgeValid);
        check (bridgeValid && modelsEqual (model, bridgeAmps, bridgeOffs, 1.0e-3f),
               "el adaptador del puente publica el modelo real (modelsState)");

        const auto bridgeName = adapter.getModelName (0);
        check (bridgeName == modelFiles[0].getFileNameWithoutExtension(),
               "el nombre que viajaria a la ficha A-D es el del fichero (" + bridgeName + ")");
    }

    // --- 7. FASE 10.1: modelo temporal — contrato v2 de punta a punta ----------
    //       El modelo de 3 frames del plan 3: f0 -> f1 CRUZA el borde del
    //       armonico (+0.45*span -> -0.45*span, camino corto del sampler), y
    //       f2 lleva un offset patologico para el clamp del escritor.
    std::cout << "\nTemporal (v2)\n";
    {
        Common::SpectralModel tri;
        tri.isValid = true;
        tri.frameCount = 3;
        tri.frameSpanHz = f0;

        for (int i = 0; i < 64; ++i)
        {
            tri.amplitudes[(size_t) i] = 1.0f;
            tri.frequencyOffsets[(size_t) i] = 0.45f * f0;    // +198 Hz
            tri.extraAmps[0][(size_t) i] = 0.0f;
            tri.extraOffsets[0][(size_t) i] = -0.45f * f0;    // -198 Hz
            tri.extraAmps[1][(size_t) i] = 0.0f;
            tri.extraOffsets[1][(size_t) i] = -0.45f * f0;
        }

        tri.extraOffsets[1][5] = 3.5f * f0;                   // patologico: clamp

        const auto triFile = dir.getChildFile ("Temporal 3F.neuronikmodel");
        check (writeModelJson (triFile, tri, "Temporal 3F"), "export v2 del modelo de 3 frames");

        // El JSON v2 lleva format, los frames y el span.
        {
            const auto parsed = juce::JSON::parse (triFile);
            const auto* obj = parsed.getDynamicObject();
            const auto* frames = obj != nullptr ? obj->getProperty ("frames").getArray() : nullptr;
            check (obj != nullptr && (int) obj->getProperty ("format") == 2
                       && frames != nullptr && frames->size() == 3,
                   "el JSON v2 lleva format=2 y los 3 frames");
            check (obj != nullptr
                       && std::abs ((double) obj->getProperty ("frameSpanHz") - (double) f0) < 1.0e-4,
                   "el JSON v2 lleva frameSpanHz (la f0 del analisis)");
        }

        const auto loaded = Serialization::PresetManager::loadModelFromFile (triFile);
        check (loaded.isValid && loaded.frameCount == 3,
               "el lector recupera los 3 frames (frameCount=3)");
        check (std::abs (loaded.frameSpanHz - f0) < 1.0e-4, "el lector recupera frameSpanHz");

        bool exact = true;

        for (int i = 0; i < 64 && exact; ++i)
            exact = std::abs (loaded.amplitudes[(size_t) i] - 1.0f) < 1.0e-6
                        && std::abs (loaded.frequencyOffsets[(size_t) i] - 0.45f * f0) < 1.0e-6
                        && std::abs (loaded.extraAmps[0][(size_t) i]) < 1.0e-6
                        && std::abs (loaded.extraOffsets[0][(size_t) i] + 0.45f * f0) < 1.0e-6
                        && std::abs (loaded.extraAmps[1][(size_t) i]) < 1.0e-6;

        check (exact, "roundtrip exacto de amplitudes y offsets frame a frame");
        check (std::abs (loaded.extraOffsets[1][5] - 0.5f * f0) < 1.0e-4,
               "el offset patologico sale clampado a media banda (0.5*span)");

        // sampleFrame sobre el modelo CARGADO: extremos bit-exactos (copia del
        // frame almacenado, sin redondeo de lerp) y camino corto entre frames.
        {
            Common::SpectralModel out;
            Common::sampleFrame (loaded, 0.0f, out);
            bool extremesOk = std::abs (out.amplitudes[0] - 1.0f) < 1.0e-6
                                  && std::abs (out.frequencyOffsets[0] - 0.45f * f0) < 1.0e-6;
            Common::sampleFrame (loaded, 1.0f, out);
            extremesOk = extremesOk
                                  && std::abs (out.amplitudes[0]) < 1.0e-6
                                  && std::abs (out.frequencyOffsets[0] + 0.45f * f0) < 1.0e-6;
            check (extremesOk, "sampleFrame z=0 y z=1 devuelven los frames extremos (copia bit-exacta)");

            Common::sampleFrame (loaded, 0.25f, out);
            // f0(+198) -> f1(-198): el delta -396 envuelve por el span (440) a
            // +44; z=0.25 => 198 + 44*0.5 = 220 = el armonico siguiente a medias.
            // Sin camino corto daria 0.0: un salto de 220 Hz en la direccion
            // equivocada.
            check (std::abs (out.amplitudes[0] - 0.5f) < 1.0e-6
                       && std::abs (out.frequencyOffsets[0] - 0.5f * f0) < 1.0e-3,
                   "sampleFrame z=0.25 interpola por camino corto (cruce de borde: +220 Hz)");
        }

        // Compat hacia atras: un "plugin v1" ignora "frames" y lee la raiz.
        {
            const auto parsed = juce::JSON::parse (triFile);
            const auto* obj = parsed.getDynamicObject();
            const auto* amps = obj != nullptr ? obj->getProperty ("amplitudes").getArray() : nullptr;
            check (amps != nullptr && amps->size() == 64
                       && std::abs ((double) amps->getUnchecked (0) - 1.0) < 1.0e-6,
                   "un lector v1 sigue leyendo el fichero v2 (frame 0 viaja en la raiz)");
        }

        // Estatico con el escritor v2 SIEMPRE: frames con UN elemento y el
        // lector devuelve el modelo estatico de siempre (bit-igual al analisis).
        {
            Common::SpectralModel stat;
            stat.isValid = true;
            stat.frameSpanHz = f0;

            for (int i = 0; i < 64; ++i)
            {
                stat.amplitudes[(size_t) i] = model.amplitudes[(size_t) i];
                stat.frequencyOffsets[(size_t) i] = model.frequencyOffsets[(size_t) i];
            }

            const auto statFile = dir.getChildFile ("Static v2.neuronikmodel");
            check (writeModelJson (statFile, stat, "Static v2"), "export v2 del modelo estatico");

            const auto parsed = juce::JSON::parse (statFile);
            const auto* obj = parsed.getDynamicObject();
            const auto* frames = obj != nullptr ? obj->getProperty ("frames").getArray() : nullptr;
            check (frames != nullptr && frames->size() == 1,
                   "el escritor v2 emite frames con UN elemento cuando es estatico");

            const auto back = Serialization::PresetManager::loadModelFromFile (statFile);
            check (back.isValid && back.frameCount == 1
                       && modelsEqual (model, back.amplitudes, back.frequencyOffsets, 1.0e-4f),
                   "el modelo estatico v2 se lee como el estatico de siempre");
        }

        // La ranura A acepta el modelo temporal: el FIFO transporta el struct
        // COMPLETO (frames incluidos) hasta el motor.
        {
            NEURONiKProcessor audio;
            audio.setRateAndBufferSizeDetails (sampleRate, blockSize);
            audio.prepareToPlay (sampleRate, blockSize);

            check (audio.loadModel (triFile, 0),
                   "la ranura A acepta el modelo temporal (struct completo por el FIFO)");

            std::array<float, 64> slotAmps {}, slotOffs {};
            bool slotValid = false;
            audio.getCurrentModel (0, slotAmps, slotOffs, slotValid);
            check (slotValid && std::abs (slotAmps[0] - 1.0f) < 1.0e-3,
                   "el frame 0 del modelo temporal es visible en la ranura");
        }
    }

    // --- 8. FASE 10.4: el analizador MULTIFRAME REAL de punta a punta ---------
    //       El mismo camino de la GUI con el combo FRAMES: bloques de seno con
    //       inharmonicidad escalonada -> analyzeTemporal -> export v2 -> lector
    //       -> el ENGINE recibe los N frames (el FIFO transporta el struct
    //       completo; la ranura expone el frame 0, el motor consume todos).
    std::cout << "\nAnalyzer temporal (v2 multiframe real)\n";
    {
        constexpr int nBlocks = 3;
        const float steps[nBlocks] = { 0.0f, 9.0f, -9.0f };
        constexpr int fftSize = 8192; // SpectralAnalyzer::fftSize

        juce::AudioBuffer<float> temporal (1, nBlocks * fftSize);
        for (int b = 0; b < nBlocks; ++b)
            for (int i = 0; i < fftSize; ++i)
            {
                // FASE 10.6: armonicos estables a n*(f0+d) — la raiz por
                // frame (HPS) captura d y el frame se mide contra SU rejilla.
                double s = 0.0;
                for (int n = 1; n <= 6; ++n)
                {
                    const double w = 2.0 * juce::MathConstants<double>::pi
                                    * (double) ((f0 + steps[b]) * n) / sampleRate;
                    s += std::sin (w * (double) i) / (double) n;
                }
                temporal.setSample (0, b * fftSize + i, (float) (0.6 * s / 2.45));
            }

        const auto model = analyzer.analyzeTemporal (temporal, sampleRate, f0, nBlocks);
        check (model.frameCount == nBlocks && model.isValid,
               "analyzeTemporal produce 3 frames validos del audio sintetico");

        // Export v2 con el dialecto de la GUI (mismo escritor del paso 7).
        const auto temporalFile = dir.getChildFile ("Analyzer Temporal.neuronikmodel");
        check (writeModelJson (temporalFile, model, "Analyzer Temporal"),
               "export v2 del modelo temporal del analizador real");

        // Roundtrip: el lector recupera TODOS los frames con sus offsets.
        const auto back = Serialization::PresetManager::loadModelFromFile (temporalFile);
        check (back.isValid && back.frameCount == nBlocks,
               "el lector recupera los 3 frames del analisis real");

        // FASE 10.6: frame 0 (canonico) conserva el offset del bloque contra
        // la f0 global; frames 1+ se miden contra SU rejilla: offset ~0 y
        // f0Frame = f0+d. El ciclo export/recarga debe preservar ambas cosas.
        bool offsetsOk = true;
        for (int fr = 0; fr < nBlocks; ++fr)
        {
            const float got = fr == 0 ? back.frequencyOffsets[(size_t) 0]
                                      : back.offsetAt (fr, 0);
            const float want = (fr == 0) ? steps[fr] : 0.0f;
            offsetsOk &= std::abs (got - want) < 2.5f; // precision HPS (~0.3 bin)
        }
        check (offsetsOk,
               "los offsets de los frames sobreviven el ciclo (emparejador real, rejilla propia)");

        // FASE 10.6: las raices por frame sobreviven el ciclo (export v2 ->
        // recarga) y el frame interpolado z=0.5 lleva el ratio del tramo.
        {
            bool f0Ok = true;
            for (int fr = 1; fr < back.frameCount; ++fr)
                f0Ok &= std::abs (back.f0At (fr) - model.f0At (fr)) < 1.0e-4f
                        && back.f0At (fr) > 0.0f;
            check (f0Ok, "las f0 por frame sobreviven el ciclo (extraF0 viaja en el v2)");

            Common::SpectralModel mid;
            Common::sampleFrame (back, 0.5f, mid);
            const float wantF0 = 0.5f * (back.f0At (0) + back.f0At (1));
            const bool midOk = mid.frameF0 > 0.0f
                               && std::abs (mid.frameF0 - wantF0)
                                      < 0.05f * juce::jmax (wantF0, 1.0f);
            check (midOk, "sampleFrame(z=0.5) interpola la raiz (ratio de rejilla presente)");
        }

        // El ENGINE real: la ranura acepta el modelo temporal del analizador.
        {
            NEURONiKProcessor audio;
            audio.setRateAndBufferSizeDetails (sampleRate, blockSize);
            audio.prepareToPlay (sampleRate, blockSize);

            check (audio.loadModel (temporalFile, 0),
                   "el engine acepta el modelo temporal del analizador real");

            std::array<float, 64> slotAmps {}, slotOffs {};
            bool slotValid = false;
            audio.getCurrentModel (0, slotAmps, slotOffs, slotValid);
            check (slotValid && std::abs (slotAmps[0] - model.amplitudes[0]) < 1.0e-3,
                   "la ranura expone el frame 0 del analisis temporal real");
        }

        // Y el sampler consume el modelo recargado (z=0.5 = frame 1).
        Common::SpectralModel frame;
        Common::sampleFrame (back, 0.5f, frame);
        check (std::abs (frame.frequencyOffsets[(size_t) 0] - back.offsetAt (1, 0)) < 1.0e-6f,
               "sampleFrame(z=0.5) entrega el frame 1 del modelo recargado");
    }


    // --- 9. FASE 11.1: CAPAS (formato v2.1) ---------------------------------
    //       El escritor y el lector de PRODUCCION (Common::writeModelToFile +
    //       PresetManager::loadModelFromFile) con un modelo de DOS capas: la
    //       capa 0 es el analisis real (que sigue viajando en la raiz para que
    //       un lector v2 antiguo suene esa capa) y la capa 1 es una voz
    //       sintetica con sus frames, su peso temporal y su f0 propios.
    std::cout << "\nCapas (v2.1)\n";
    {
        Common::SpectralModel layered = model;              // capa 0 = analisis real
        layered.setLayerCount (2);
        layered.setLayerNameAt (0, "drone");
        layered.setLayerWeightAt (0, 1.0f);
        layered.setLayerNameAt (1, "lead");
        layered.setLayerWeightAt (1, 0.6f);
        layered.setNumFramesOf (1, 3);

        for (int f = 0; f < 3; ++f)
        {
            float* amps = layered.ampsOf (1, f);
            float* offsets = layered.offsetsOf (1, f);

            for (int i = 0; i < 64; ++i)
            {
                // Un armonico que MIGRA (6 -> 8): la firma del material RRISE,
                // aqui como dato para que la capa no sea una copia de la 0.
                amps[(size_t) i] = (i == 6 + f) ? 0.9f : 0.01f * (float) (i + 1);
                offsets[(size_t) i] = 0.1f * (float) f;
            }

            layered.setF0At (1, f, f0 * (1.0f + 0.05f * (float) f));
            layered.setFrameWeightAt (1, f, 1.0f - 0.2f * (float) f);
        }

        const auto layeredFile = dir.getChildFile ("Two Layers.neuronikmodel");
        check (Common::writeModelToFile (layeredFile, layered, "Two Layers",
                                         "Created with NEURONiK Model Maker"),
               "export v2.1 del modelo de 2 capas");

        // El dialecto: v2.1 sube a format=2.1 y lleva el bloque de capas.
        {
            const auto parsed = juce::JSON::parse (layeredFile);
            const auto* obj = parsed.getDynamicObject();
            const auto* layers = obj != nullptr ? obj->getProperty ("layers").getDynamicObject() : nullptr;
            const auto* layerArray = layers != nullptr ? layers->getProperty ("layers").getArray() : nullptr;

            check (obj != nullptr && std::abs ((double) obj->getProperty ("format") - 2.1) < 1.0e-9,
                   "el JSON v2.1 lleva format=2.1");
            check (layers != nullptr && (int) layers->getProperty ("layerCount") == 2
                       && layerArray != nullptr && layerArray->size() == 2,
                   "el JSON v2.1 lleva layerCount=2 y las dos capas");
            check (layerArray != nullptr && layerArray->size() == 2
                       && std::abs ((double) (*layerArray)[1].getDynamicObject()
                                        ->getProperty ("weight") - 0.6) < 1.0e-6,
                   "cada capa del JSON lleva su name/weight/frames");
        }

        const auto back = Serialization::PresetManager::loadModelFromFile (layeredFile);
        check (back.isValid && back.layerCount == 2 && back.isLayered(),
               "el lector recupera las 2 capas (layerCount=2)");
        check (back.layerNameAt (0) == "drone" && back.layerNameAt (1) == "lead",
               "los nombres de capa viajan en el fichero");
        check (std::abs (back.layerWeightAt (1) - 0.6f) < 1.0e-6f
                   && std::abs (back.layerWeightAt (0) - 1.0f) < 1.0e-6f,
               "las mezclas estaticas de capa viajan");
        check (back.numFramesOf (1) == 3 && back.numFramesOf (0) == layered.numFramesOf (0)
                   && back.frameCount == layered.frameCount,
               "los frames por capa viajan (y la raiz conserva los suyos)");

        bool layerOk = true;

        for (int f = 0; f < 3; ++f)
            for (int i = 0; i < 64; ++i)
                layerOk = layerOk
                          && std::abs (back.ampAt (1, f, i) - layered.ampAt (1, f, i)) < 1.0e-6f
                          && std::abs (back.offsetAt (1, f, i) - layered.offsetAt (1, f, i)) < 1.0e-6f;

        check (layerOk, "los frames de la capa 1 vuelven bit a bit (mismo clamp de banda)");

        check (std::abs (back.frameWeightAt (1, 0) - 1.0f) < 1.0e-6f
                   && std::abs (back.frameWeightAt (1, 2) - 0.6f) < 1.0e-6f,
               "los pesos temporales por frame de la capa viajan");
        check (std::abs (back.f0At (1, 1) - layered.f0At (1, 1)) < 1.0e-4f
                   && back.f0At (1, 1) > 0.0f,
               "la raiz por frame de la capa viaja (frameF0 del v2)");

        // La capa 0 sale en la RAIZ: un lector v2 antiguo (y el puente WASM de
        // 128 floats) suena esa capa en vez de no entender el fichero.
        check (std::abs (back.amplitudes[0] - layered.amplitudes[0]) < 1.0e-4f
                   && back.frameCount == layered.frameCount,
               "la capa 0 sigue siendo la raiz del fichero (compat v2/v1)");

        // El ENGINE real: un modelo de capas no rompe la ranura. El motor
        // sumara las capas en la Fase 11.3; hoy suena la capa 0, igual que el
        // v1 WASM, y por eso el fichero es compatible sin tocar nada.
        {
            NEURONiKProcessor audio;
            audio.setRateAndBufferSizeDetails (sampleRate, blockSize);
            audio.prepareToPlay (sampleRate, blockSize);

            check (audio.loadModel (layeredFile, 0),
                   "el engine acepta el modelo de 2 capas");

            std::array<float, 64> slotAmps {}, slotOffs {};
            bool slotValid = false;
            audio.getCurrentModel (0, slotAmps, slotOffs, slotValid);
            check (slotValid && std::abs (slotAmps[0] - layered.amplitudes[0]) < 1.0e-3f,
                   "la ranura expone la capa 0 (lo que suena sin motor de capas)");
        }

        // Un fichero con MAS capas de las que caben se TRUNCA a kMaxLayers, no
        // se rechaza: misma politica que con mas frames de los que caben.
        {
            auto parsed = juce::JSON::parse (layeredFile);
            auto* obj = parsed.getDynamicObject();
            auto layersVar = obj != nullptr ? obj->getProperty ("layers") : juce::var();
            auto* layers = layersVar.getDynamicObject();
            auto* layerArray = layers != nullptr ? layers->getProperty ("layers").getArray() : nullptr;

            if (layerArray != nullptr && layerArray->size() == 2)
            {
                layerArray->add ((*layerArray)[1]);
                layerArray->add ((*layerArray)[1]);
                layerArray->add ((*layerArray)[1]);
                layers->setProperty ("layerCount", 5);

                const auto bigFile = dir.getChildFile ("Five Layers.neuronikmodel");
                bigFile.replaceWithText (juce::JSON::toString (parsed));

                const auto big = Serialization::PresetManager::loadModelFromFile (bigFile);
                check (big.isValid && big.layerCount == Common::SpectralModel::kMaxLayers,
                       "un fichero con 5 capas se trunca a kMaxLayers ("
                           + juce::String (Common::SpectralModel::kMaxLayers) + ")");
            }
            else
            {
                check (false, "no se pudo reescribir el JSON de capas para el caso de truncado");
            }
        }
    }

    // --- 10. FASE 11.1: el v2 PURO no cambia ---------------------------------
    //       Sin capas el fichero no lleva "layers" y todo el camino (lector,
    //       motor y puente WASM de 128 floats) es exactamente el de siempre:
    //       es la condicion de la paridad A-E en el mismo commit que el v2.1.
    std::cout << "\nSin capas (v2 puro)\n";
    {
        const auto plainFile = dir.getChildFile ("Plain v2.neuronikmodel");
        check (writeModelJson (plainFile, model, "Plain v2"), "export v2 del analisis real");

        const auto parsed = juce::JSON::parse (plainFile);
        const auto* obj = parsed.getDynamicObject();
        check (obj != nullptr && obj->getProperty ("layers").isVoid(),
               "el v2 puro NO trae bloque de capas");
        check (obj != nullptr && obj->getProperty ("gridFixed").isVoid(),
               "el v2 puro NO declara rejilla fija (el modo es opcional)");
        check (obj != nullptr && (int) obj->getProperty ("format") == 2
                   && ! obj->getProperty ("frameSpanHz").isVoid(),
               "el v2 puro conserva format=2 y frameSpanHz");

        const auto back = Serialization::PresetManager::loadModelFromFile (plainFile);
        check (back.isValid && back.layerCount == 1 && ! back.isLayered(),
               "el v2 puro carga con layerCount = 1 (el camino de siempre)");
        check (back.frameCount == model.frameCount
                   && modelsEqual (back, model.amplitudes, model.frequencyOffsets, 1.0e-4f),
               "el v2 puro recupera el modelo del analisis (frame a frame)");

        // El puente WASM construye un SpectralModel NUEVO y le copia 128
        // floats: con layerCount = 1 por defecto suena la capa 0 — el clamp del
        // v1, que es lo que mantiene la paridad nativa/WASM y el bloque por
        // slot en ~33 KB.
        const Common::SpectralModel fresh {};
        check (fresh.layerCount == 1 && ! fresh.isLayered() && fresh.numFramesOf (0) == 1
                   && fresh.frameWeightAt (0, 0) == 1.0f,
               "un modelo por defecto tiene 1 capa, 1 frame y peso 1 (el clamp del puente WASM)");
    }


    // --- 11. REJILLA FIJA (modo declarado, 2026-09-25) ----------------------
    //       El modo del ModelMaker: la f0 la fijo el usuario y el analisis no
    //       siguio el pitch. El fichero lo declara con "gridFixed": true — sin
    //       subir "format" (es ortogonal a las capas) y sin tocar el JSON de los
    //       modelos que no lo declaran.
    std::cout << "\nRejilla fija\n";
    {
        auto fixed = model;
        fixed.gridFixed = true;

        const auto fixedFile = dir.getChildFile ("Fixed grid.neuronikmodel");
        check (writeModelJson (fixedFile, fixed, "Fixed grid"),
               "export del modelo con rejilla fija");

        const auto parsed = juce::JSON::parse (fixedFile);
        const auto* obj = parsed.getDynamicObject();
        check (obj != nullptr && (bool) obj->getProperty ("gridFixed"),
               "el fichero DECLARA gridFixed");
        check (obj != nullptr && (int) obj->getProperty ("format") == 2
                   && obj->getProperty ("layers").isVoid(),
               "gridFixed NO sube la version del formato (sigue siendo v2)");

        const auto back = Serialization::PresetManager::loadModelFromFile (fixedFile);
        check (back.isValid && back.gridFixed,
               "el lector de produccion recupera la rejilla fija");
        check (back.frameCount == fixed.frameCount
                   && modelsEqual (back, fixed.amplitudes, fixed.frequencyOffsets, 1.0e-4f),
               "y el modelo entero (frames y amplitudes) sigue intacto");

        // Y al reves: sin el modo, el fichero de siempre no cambia NI EN EL
        // TEXTO (es la condicion para que los assets versionados sigan siendo
        // byte a byte los mismos).
        const auto plainFile = dir.getChildFile ("No fixed grid.neuronikmodel");
        check (writeModelJson (plainFile, model, "No fixed grid"), "export sin el modo");
        check (! plainFile.loadFileAsString().contains ("gridFixed"),
               "sin el modo la clave no aparece ni en el texto del fichero");
        check (! Serialization::PresetManager::loadModelFromFile (plainFile).gridFixed,
               "y el lector lo deja en false (el modelo de siempre)");
    }

    // --- 12. OFFSETS TRANSPONIBLES (modo declarado, 2026-09-25) -------------
    //       Los offsets se declaran RATIOS medidos contra la rejilla de analisis
    //       y el motor los escala por base/f0: la inharmonicidad se transpone
    //       con el teclado. El fichero lo declara con "offsetsTranspose": true —
    //       sin subir "format" (es ortogonal a las capas) y sin tocar el JSON de
    //       los modelos que no lo declaran.
    std::cout << "\nOffsets transpuestos\n";
    {
        auto transposed = model;
        transposed.offsetsTranspose = true;

        const auto transFile = dir.getChildFile ("Transposed offsets.neuronikmodel");
        check (writeModelJson (transFile, transposed, "Transposed offsets"),
               "export del modelo con offsets transpuestos");

        const auto parsed = juce::JSON::parse (transFile);
        const auto* obj = parsed.getDynamicObject();
        check (obj != nullptr && (bool) obj->getProperty ("offsetsTranspose"),
               "el fichero DECLARA offsetsTranspose");
        check (obj != nullptr && (int) obj->getProperty ("format") == 2
                   && obj->getProperty ("layers").isVoid()
                   && obj->getProperty ("gridFixed").isVoid(),
               "offsetsTranspose NO sube la version ni arrastra otras claves");

        const auto back = Serialization::PresetManager::loadModelFromFile (transFile);
        check (back.isValid && back.offsetsTranspose,
               "el lector de produccion recupera el modo");
        check (back.frameCount == transposed.frameCount
                   && modelsEqual (back, transposed.amplitudes, transposed.frequencyOffsets, 1.0e-4f),
               "y el modelo entero sigue intacto");

        // El motor lee el SNAPSHOT, no el modelo fuente: si el modo y su rejilla
        // de referencia no sobreviven al sampler, el escalado no ocurre.
        Common::SpectralModel snap;
        Common::sampleFrame (back, 0.0f, snap);
        check (snap.offsetsTranspose && snap.offsetRootHz == back.frameSpanHz,
               "el modo y su rejilla viajan al snapshot que lee el motor");

        const auto plainFile = dir.getChildFile ("No transposed offsets.neuronikmodel");
        check (writeModelJson (plainFile, model, "No transposed offsets"), "export sin el modo");
        check (! plainFile.loadFileAsString().contains ("offsetsTranspose"),
               "sin el modo la clave no aparece ni en el texto del fichero");
        check (! Serialization::PresetManager::loadModelFromFile (plainFile).offsetsTranspose,
               "y el lector lo deja en false (el modelo de siempre)");
    }

    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.") << '\n';
    return failures == 0 ? 0 : 1;
}
