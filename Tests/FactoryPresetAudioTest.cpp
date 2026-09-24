/*
  ==============================================================================

    FactoryPresetAudioTest.cpp
    Created: 24 Sep 2026
    Description: Los presets de fabrica del banco CZ101 SUENAN A LA FUENTE.

                 Cadena completa, midiendo con el mismo harness DFT que la
                 sonda del ModelMaker (ventana Hann de 8192 en el tramo de
                 RMS maximo, top-8 parciales con amp >= 0.05, interpolacion
                 parabolica sub-bin, MEDIANA de cents <= 35):

                   1. INSTALACION — el constructor del processor escribe los
                      seis modelos y los seis presets embebidos en
                      Documents/NEURONiK/{Models,Presets} (solo si faltan):
                      los instalados no conservan el marcador y apuntan a
                      modelos que existen (path nativo, sin separadores
                      mezclados).
                   2. PRESET -> MOTOR — por cada preset EMBEBIDO (bytes del
                      binario, no el disco del usuario: staging en temp),
                      marcador {{FACTORY_MODELS}}/ sustituido, carga con el
                      PresetManager real (replaceState -> reloadModels ->
                      FIFO) y render de una nota a 48 kHz con el engine real.
                   3. RENDER vs MODELO (contrato): los picos del audio contra
                      la rejilla exacta del Resonator
                      ((k*nota*shift*k^inharmonicidad) + offset) * gridRatio
                      del frame muestreado en z=0, con los parametros que el
                      preset trae.
                   4. FUENTE vs MODELO (contrato si hay WAVs): los picos del
                      WAV real del banco (../ABDCZ101/DOCS/patches, que CMake
                      inyecta como NEURONiK_CZ101_WAV_DIR si existe) contra la
                      rejilla anclada en frameSpanHz, con la f0 re-detectada
                      como guardia de deriva. Material con barrido de pitch
                      (CZ-RRISE) se exime ANTES del contrato como en la
                      sonda: un modelo estatico no describe un barrido; la
                      seccion temporal si.

                 El contrato de parciales pide validated >= min(3, fuertes):
                 el frame 0 de CZ-RRISE-temporal tiene UN solo parcial con
                 amp >= 0.05 (el material de arranque del barrido).

                 NEURONiK_DEBUG_RENDER=1 imprime el RMS cada 4 bloques
                 (diagnostico de la envolvente de la senal renderizada).

  ==============================================================================
*/

#include "../Source/Main/NEURONiKProcessor.h"
#include "../Source/Serialization/PresetManager.h"
#include "../Source/State/ParameterDefinitions.h"
#include "../Source/Common/SpectralModel.h"
#include "../Source/DSP/FrameSampler.h"
#include "../Source/DSP/DspMidiMessage.h"
#include "../Source/ModelMaker/Analysis/SpectralAnalyzer.h"

#include <NeuronikFactoryModels.h>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using NEURONiK::Common::SpectralModel;

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

    /** El marcador que el generador pone en modelPath0 (seguido de la barra
        del propio marcador) y que installFactoryPresets() sustituye por el
        directorio real con el separador nativo. */
    constexpr const char* kModelsMarkerSlash = "{{FACTORY_MODELS}}/";
    constexpr const char* kModelsMarker      = "{{FACTORY_MODELS}}";

    constexpr int   kWindow           = 8192;
    constexpr float kMedianLimitCents = 35.0f;

    struct FactoryEntry
    {
        const char* presetName;   // nombre del preset = basename del modelo
        const char* modelFile;    // fichero embebido en Assets/Models
        const char* sourceWav;    // WAV real del banco del que salio
        const char* modelData;
        int modelSize;
        const char* presetData;
        int presetSize;
    };

    // Simbolos JUCE: punto -> `_`, guiones fuera (misma regla que el resto
    // del codigo embebido; BridgeSelftest.h es el patron). CZ-RRISE deja
    // DOS R: CZ + RRISE = CZRRISE.
    const FactoryEntry kFactory[] = {
        { "CZ-BASS1", "CZ-BASS1.neuronikmodel", "CZ-BASS1.wav",
          NeuronikFactoryModels::CZBASS1_neuronikmodel,
          NeuronikFactoryModels::CZBASS1_neuronikmodelSize,
          NeuronikFactoryModels::CZBASS1_neuronikpreset,
          NeuronikFactoryModels::CZBASS1_neuronikpresetSize },
        { "CZ-HAMOG", "CZ-HAMOG.neuronikmodel", "CZ-HAMOG.wav",
          NeuronikFactoryModels::CZHAMOG_neuronikmodel,
          NeuronikFactoryModels::CZHAMOG_neuronikmodelSize,
          NeuronikFactoryModels::CZHAMOG_neuronikpreset,
          NeuronikFactoryModels::CZHAMOG_neuronikpresetSize },
        { "CZ-PAD1", "CZ-PAD1.neuronikmodel", "CZ-PAD1.wav",
          NeuronikFactoryModels::CZPAD1_neuronikmodel,
          NeuronikFactoryModels::CZPAD1_neuronikmodelSize,
          NeuronikFactoryModels::CZPAD1_neuronikpreset,
          NeuronikFactoryModels::CZPAD1_neuronikpresetSize },
        { "CZ-SWEP1", "CZ-SWEP1.neuronikmodel", "CZ-SWEP1.wav",
          NeuronikFactoryModels::CZSWEP1_neuronikmodel,
          NeuronikFactoryModels::CZSWEP1_neuronikmodelSize,
          NeuronikFactoryModels::CZSWEP1_neuronikpreset,
          NeuronikFactoryModels::CZSWEP1_neuronikpresetSize },
        { "CZ-RRISE-temporal", "CZ-RRISE-temporal.neuronikmodel", "CZ-RRISE.wav",
          NeuronikFactoryModels::CZRRISEtemporal_neuronikmodel,
          NeuronikFactoryModels::CZRRISEtemporal_neuronikmodelSize,
          NeuronikFactoryModels::CZRRISEtemporal_neuronikpreset,
          NeuronikFactoryModels::CZRRISEtemporal_neuronikpresetSize },
        { "CZ-BASS1-temporal", "CZ-BASS1-temporal.neuronikmodel", "CZ-BASS1.wav",
          NeuronikFactoryModels::CZBASS1temporal_neuronikmodel,
          NeuronikFactoryModels::CZBASS1temporal_neuronikmodelSize,
          NeuronikFactoryModels::CZBASS1temporal_neuronikpreset,
          NeuronikFactoryModels::CZBASS1temporal_neuronikpresetSize },
    };

    juce::File documentsSub (const char* sub)
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("NEURONiK")
            .getChildFile (sub);
    }

    /** Sustituye el marcador por un directorio con el SEPARADOR NATIVO: el
        path que viaja en el XML es el mismo que juce::File(fullPath) resuelve
        (sin `Models/fichero` con barra mezclada). */
    juce::String unmarkPreset (const char* data, int size, const juce::File& dir)
    {
        return juce::String::fromUTF8 (data, size)
            .replace (kModelsMarkerSlash,
                      dir.getFullPathName() + juce::File::getSeparatorString());
    }

    //==========================================================================
    // Harness DFT — copiado a proposito de ModelMakerRealWavProbe: este test
    // es autocontenido como el resto de la suite.
    //==========================================================================

    float windowedBinMag (const float* seg, int n, double bin)
    {
        const double step = juce::MathConstants<double>::twoPi * bin / (double) n;
        double re = 0.0, im = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double w = 0.5 * (1.0 - std::cos (juce::MathConstants<double>::twoPi * i / (double) (n - 1)));
            const double v = (double) seg[i] * w;
            re += v * std::cos (step * i);
            im -= v * std::sin (step * i);
        }
        return (float) std::sqrt (re * re + im * im);
    }

    struct GridCheck
    {
        int validated = 0;
        int strongCount = 0;   // parciales del modelo con amp >= 0.05 (top-8)
        float medianCents = 0.0f;
        float maxCents = 0.0f;
    };

    /** Parciales del modelo contra el espectro: centro_k = rejilla del
        Resonator con la base dada (nota renderizada o f0 de la fuente). */
    GridCheck gridCheck (const juce::AudioBuffer<float>& audio, double sampleRate,
                         const SpectralModel& model, double base,
                         float shift, float inharm, float gridRatio,
                         const char* tag)
    {
        GridCheck out;

        const float* ch = audio.getReadPointer (0);
        const int total = audio.getNumSamples();
        if (total < kWindow)
        {
            std::printf ("[%s]     audio demasiado corto (%d < %d)\n", tag, total, kWindow);
            return out;
        }

        // 1. tramo de RMS maximo (ventana de 8192, avance de 1024)
        int bestStart = 0;
        double bestRms = -1.0;
        for (int start = 0; start + kWindow <= total; start += 1024)
        {
            double acc = 0.0;
            for (int i = 0; i < kWindow; i += 4)
                acc += (double) ch[start + i] * ch[start + i];
            if (! std::isfinite (acc)) { acc = -1.0; } // la explosion satura la busqueda
            if (acc > bestRms) { bestRms = acc; bestStart = start; }
        }
        const float* seg = ch + bestStart;

        // 2. top-8 parciales del modelo con amp >= 0.05, pico real en su banda
        int order[64];
        for (int i = 0; i < 64; ++i) order[i] = i;
        std::sort (order, order + 64, [&model] (int a, int b)
                   { return model.amplitudes[(size_t) a] > model.amplitudes[(size_t) b]; });

        const double binHz = (double) sampleRate / (double) kWindow;
        std::vector<float> centsAll;

        for (int idx = 0; idx < 64 && (int) centsAll.size() < 8; ++idx)
        {
            const int k = order[idx] + 1;
            const float amp = model.amplitudes[(size_t) (k - 1)];
            if (amp < 0.05f) break;
            ++out.strongCount;

            const double stretched = std::exp (std::log ((double) k) * (1.0 + (double) inharm * 0.5));
            const double center = ((base * stretched * (double) shift)
                                   + (double) model.frequencyOffsets[(size_t) (k - 1)])
                                  * (double) gridRatio;
            const double halfBand = 0.5 * (double) shift * base;

            int lo = (int) std::floor ((center - halfBand) / binHz);
            int hi = (int) std::ceil  ((center + halfBand) / binHz);
            if (lo < 1) lo = 1;
            if (hi > kWindow / 2 - 1) hi = kWindow / 2 - 1;
            if (hi <= lo) continue;

            int peakBin = lo;
            float peakMag = -1.0f;
            for (int b = lo; b <= hi; ++b)
            {
                const float mag = windowedBinMag (seg, kWindow, (double) b);
                if (mag > peakMag) { peakMag = mag; peakBin = b; }
            }

            // Interpolacion parabola SOLO si el pico quedo dentro de la banda
            // (en el borde la parabola se dispara y el "medido" escapa de la
            // banda: pasa cuando la senal no tiene energia ahi).
            double measured = (double) peakBin * binHz;
            if (peakBin > lo && peakBin < hi)
            {
                const float yl = windowedBinMag (seg, kWindow, (double) (peakBin - 1));
                const float y0 = windowedBinMag (seg, kWindow, (double) peakBin);
                const float yr = windowedBinMag (seg, kWindow, (double) (peakBin + 1));
                const float denom = yl - 2.0f * y0 + yr;
                const float delta = std::abs (denom) > 1.0e-9f ? 0.5f * (yl - yr) / denom : 0.0f;
                const float clamped = juce::jlimit (-0.5f, 0.5f, delta);
                measured = ((double) peakBin + (double) clamped) * binHz;
            }

            const double cents = 1200.0 * std::log2 (measured / center);
            centsAll.push_back ((float) cents);
            out.maxCents = juce::jmax (out.maxCents, (float) std::abs (cents));
            ++out.validated;

            std::printf ("[%s]     parcial %2d: rejilla %.1f Hz vs medida %.1f Hz -> %+.1f cents (amp %.2f)\n",
                         tag, k, center, measured, cents, (double) amp);
        }

        if (centsAll.empty())
            return out;

        std::sort (centsAll.begin(), centsAll.end());
        const int n = (int) centsAll.size();
        out.medianCents = (n % 2 == 1) ? centsAll[(size_t) (n / 2)]
                                       : 0.5f * (centsAll[(size_t) (n / 2 - 1)] + centsAll[(size_t) (n / 2)]);
        return out;
    }

    /** Material con barrido de pitch: cabeza vs cola (mismo criterio que la
        sonda; el modelo estatico no es aplicable, lo dice y se continua). */
    bool isPitchSweep (const juce::AudioBuffer<float>& audio, double sampleRate)
    {
        constexpr int nw = kWindow;
        if (audio.getNumSamples() < 2 * nw)
            return false;

        juce::AudioBuffer<float> head (1, nw);
        juce::AudioBuffer<float> tail (1, nw);
        head.copyFrom (0, 0, audio, 0, 0, nw);
        tail.copyFrom (0, 0, audio, 0, audio.getNumSamples() - nw, nw);

        ModelMaker::Analysis::SpectralAnalyzer analyzer;
        const float f0Head = analyzer.detectPitch (head, sampleRate);
        const float f0Tail = analyzer.detectPitch (tail, sampleRate);

        return f0Head > 20.0f && f0Tail > 20.0f
               && juce::jmax (f0Head, f0Tail) > 1.5f * juce::jmin (f0Head, f0Tail);
    }

    float bufferRms (const juce::AudioBuffer<float>& buffer)
    {
        double sum = 0.0;
        int samples = 0;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const double v = (double) buffer.getSample (c, i);
                sum += v * v;
                ++samples;
            }
        return samples > 0 ? (float) std::sqrt (sum / samples) : 0.0f;
    }
} // namespace

int main()
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const bool debugRender = std::getenv ("NEURONIK_DEBUG_RENDER") != nullptr;

    std::cout << "NEURONiK factory presets CZ101 — verificacion acustica\n";

    //==========================================================================
    // 1. INSTALACION — el arranque del processor escribe el material embebido
    //    (solo si falta) y los presets instalados salen con paths reales.
    //==========================================================================
    std::cout << "\nInstalacion de fabrica en Documents\n";

    {
        NEURONiKProcessor installer; // ctor -> installFactoryModels() + installFactoryPresets()

        const auto modelsDir  = documentsSub ("Models");
        const auto presetsDir = documentsSub ("Presets");

        for (const auto& entry : kFactory)
        {
            const auto modelFile = modelsDir.getChildFile (entry.modelFile);
            check (modelFile.existsAsFile(),
                   "modelo instalado: Models/" + juce::String (entry.modelFile));

            const auto model = Serialization::PresetManager::loadModelFromFile (modelFile);
            check (model.isValid && model.frameSpanHz > 20.0f && model.frameSpanHz < 5000.0f,
                   "modelo legible con f0 en rango: " + juce::String (entry.modelFile)
                       + " (" + juce::String (model.frameSpanHz, 1) + " Hz)");

            const auto presetFile = presetsDir.getChildFile (juce::String (entry.presetName)
                                                             + Serialization::PresetManager::presetExtension);
            check (presetFile.existsAsFile(),
                   "preset instalado: Presets/" + juce::String (entry.presetName));

            if (presetFile.existsAsFile())
            {
                const auto text = presetFile.loadFileAsString();
                check (! text.contains (kModelsMarker),
                       "el preset instalado no conserva el marcador: " + juce::String (entry.presetName));
                check (text.contains (modelFile.getFullPathName()),
                       "el preset instalado apunta a su modelo real: " + juce::String (entry.presetName));
            }
        }
    }

    //==========================================================================
    // 2. STAGING — los bytes EMBEBIDOS (los que viajan en el binario; el disco
    //    de Documents puede llevar ediciones del usuario) a un temp limpio.
    //==========================================================================
    const auto stage        = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("NEURONiK_FactoryAudio");
    stage.deleteRecursively();
    const auto stageModels  = stage.getChildFile ("Models");
    const auto stagePresets = stage.getChildFile ("Presets");
    stageModels.createDirectory();
    stagePresets.createDirectory();

    for (const auto& entry : kFactory)
    {
        const auto modelFile = stageModels.getChildFile (entry.modelFile);
        if (! modelFile.replaceWithData (entry.modelData, (size_t) entry.modelSize))
        {
            check (false, "no se pudo stagiar el modelo " + juce::String (entry.modelFile));
            return 1;
        }

        const auto presetFile = stagePresets.getChildFile (juce::String (entry.presetName)
                                                           + Serialization::PresetManager::presetExtension);
        presetFile.replaceWithText (unmarkPreset (entry.presetData, entry.presetSize, stageModels));
    }

    juce::AudioFormatManager fmts;
    fmts.registerBasicFormats();

    //==========================================================================
    // 3/4. POR CADA PRESET — carga real, render y los dos cheats de cents.
    //==========================================================================
    for (const auto& entry : kFactory)
    {
        std::cout << "\nPreset " << entry.presetName << '\n';

        const auto stagedModel  = stageModels.getChildFile (entry.modelFile);
        const auto stagedPreset = stagePresets.getChildFile (juce::String (entry.presetName)
                                                             + Serialization::PresetManager::presetExtension);

        NEURONiKProcessor audio;
        audio.setRateAndBufferSizeDetails (48000.0, 512);
        audio.prepareToPlay (48000.0, 512);

        audio.getPresetManager().loadPresetFromFile (stagedPreset);

        // El preset llevo su modelo y la ranura A lo refleja.
        const auto loadedPath = audio.getAPVTS().state.getProperty ("modelPath0").toString();
        check (loadedPath == stagedModel.getFullPathName(),
               "el preset cargo su modelPath0 (ranura A)");
        check (audio.getModelNames()[0] == entry.presetName,
               "la ranura A publica el modelo: " + audio.getModelNames()[0]);

        const auto model = Serialization::PresetManager::loadModelFromFile (stagedModel);
        check (model.isValid, "el modelo del preset es valido");
        if (! model.isValid)
            continue;

        // El frame que el motor sintetiza en reposo (morphZ = 0).
        SpectralModel frame;
        Common::sampleFrame (model, 0.0f, frame);
        const float gridRatio = (frame.frameF0 > 0.0f && frame.f0At (0) > 0.0f)
                                    ? frame.frameF0 / frame.f0At (0)
                                    : 1.0f;

        // Los parametros del preset tal como el Resonator los lee (los defaults
        // de fabrica son neutros, pero el contrato se mide con lo que hay).
        const float shift  = audio.getAPVTS().getRawParameterValue (State::IDs::resonatorShift)->load();
        const float inharm = audio.getAPVTS().getRawParameterValue (State::IDs::oscInharmonicity)->load();

        const double f0 = (double) model.frameSpanHz;
        check (f0 > 20.0 && f0 < 5000.0, "f0 del modelo en rango (" + juce::String (f0, 1) + " Hz)");

        // Nota mas cercana a la f0 del analisis; la voz usa EL MISMO puerto de
        // nota que aqui (dsp::MidiMessage), asi que base == baseFrequency.
        const int note = juce::jlimit (0, 127,
            (int) std::round (69.0 + 12.0 * std::log2 (f0 / 440.0)));
        const double base = (double) dsp::MidiMessage::getMidiNoteInHertz (note);
        std::cout << "  nota " << note << " (" << juce::String (base, 2)
                  << " Hz) vs f0 de la fuente " << juce::String (f0, 2) << " Hz\n";

        // Render: 64 bloques de 512 con la nota pulsada en el primero
        // (0.68 s: ataque + sostenimiento, la ventana de 8192 cabe de sobra).
        constexpr int kBlock = 512, kBlocks = 64;
        juce::AudioBuffer<float> capture (1, kBlock * kBlocks);
        juce::AudioBuffer<float> blockBuf (2, kBlock);

        for (int b = 0; b < kBlocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);

            blockBuf.clear();
            audio.processBlock (blockBuf, midi);
            capture.copyFrom (0, b * kBlock, blockBuf, 0, 0, kBlock);

            if (debugRender && (b % 4 == 0))
                std::printf ("    [dbg] bloque %2d RMS %g\n", b, (double) bufferRms (blockBuf));
        }

        check (bufferRms (capture) > 1.0e-4f && std::isfinite (bufferRms (capture)),
               "el preset renderiza audio finito (RMS " + juce::String (bufferRms (capture), 5) + ")");

        // --- 3. RENDER vs MODELO (contrato, siempre) -----------------------
        std::cout << "  render vs modelo\n";
        const GridCheck render = gridCheck (capture, 48000.0, frame, base,
                                            shift, inharm, gridRatio, "render");
        const int required = juce::jmin (3, render.strongCount);
        std::printf ("  [render] acustica: %d/%d parciales fuertes, mediana %+.1f cents, max %.1f cents\n",
                     render.validated, render.strongCount,
                     (double) render.medianCents, (double) render.maxCents);
        check (render.strongCount >= 1, "el modelo tiene al menos un parcial fuerte (amp >= 0.05)");
        check (render.validated >= required,
               "render: valida al menos " + juce::String (required) + " parciales");
        check (std::abs (render.medianCents) <= kMedianLimitCents,
               "render vs modelo: mediana " + juce::String (render.medianCents, 1)
                   + " cents <= " + juce::String (kMedianLimitCents, 0));

        // --- 4. FUENTE vs MODELO (contrato si hay WAVs) ---------------------
        #ifdef NEURONiK_CZ101_WAV_DIR
        {
            const auto wav = juce::File (juce::String (NEURONiK_CZ101_WAV_DIR))
                                 .getChildFile (entry.sourceWav);

            if (! wav.existsAsFile())
            {
                std::cout << "  [skip] falta el WAV de la fuente: " << wav.getFullPathName() << '\n';
            }
            else
            {
                std::unique_ptr<juce::AudioFormatReader> reader (fmts.createReaderFor (wav));
                if (reader == nullptr)
                {
                    check (false, "no hay reader para " + wav.getFileName());
                }
                else
                {
                    juce::AudioBuffer<float> source ((int) reader->numChannels,
                                                     (int) reader->lengthInSamples);
                    reader->read (&source, 0, (int) reader->lengthInSamples, 0, true, true);
                    const double sourceSr = reader->sampleRate;

                    if (isPitchSweep (source, sourceSr))
                    {
                        // Mismo eximento que la sonda: material con barrido
                        // (CZ-RRISE) — un modelo estatico no lo describe; lo
                        // reproduce la seccion temporal con morphZ. El
                        // contrato fuente Y la guardia de deriva saltan
                        // enteros: con un barrido no hay UNA f0 que comparar
                        // contra la del modelo.
                        std::cout << "  [warn] barrido de pitch detectado: la comparacion"
                                     " estatica contra la fuente no es aplicable\n";
                        continue;
                    }

                    // Guardia de deriva: si el WAV cambio desde que la sonda
                    // genero el modelo, la f0 re-detectada (semilla HPS +
                    // ajuste LS sobre todas las ventanas) no coincide.
                    ModelMaker::Analysis::SpectralAnalyzer analyzer;
                    const float detected = analyzer.detectPitch (source, sourceSr);
                    check (std::abs (detected - model.frameSpanHz) <= 0.01f * model.frameSpanHz,
                           "la f0 del WAV sigue siendo la del modelo ("
                               + juce::String (detected, 2) + " vs "
                               + juce::String (model.frameSpanHz, 2) + " Hz)");

                    std::cout << "  fuente vs modelo (WAV " << wav.getFileName() << ")\n";
                    const GridCheck src = gridCheck (source, sourceSr, frame, f0,
                                                     shift, inharm, gridRatio, "fuente");
                    const int srcRequired = juce::jmin (3, src.strongCount);
                    std::printf ("  [fuente] acustica: %d/%d parciales fuertes, mediana %+.1f cents, max %.1f cents\n",
                                 src.validated, src.strongCount,
                                 (double) src.medianCents, (double) src.maxCents);
                    check (src.validated >= srcRequired,
                           "fuente: valida al menos " + juce::String (srcRequired) + " parciales");
                    check (std::abs (src.medianCents) <= kMedianLimitCents,
                           "fuente vs modelo: mediana " + juce::String (src.medianCents, 1)
                               + " cents <= " + juce::String (kMedianLimitCents, 0));
                }
            }
        }
        #else
        std::cout << "  [skip] sin NEURONiK_CZ101_WAV_DIR: el chequeo contra la fuente se omite\n";
        #endif
    }

    stage.deleteRecursively();

    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.") << '\n';
    return failures == 0 ? 0 : 1;
}
