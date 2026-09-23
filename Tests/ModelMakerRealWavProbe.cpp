/*
  ==============================================================================

    ModelMakerRealWavProbe.cpp
    Created: 23 Sep 2026
    Description: Sonda CLI de producción del ModelMaker — el ModelMaker es GUI
                 pura (sin CLI), así que esta sonda enlaza los MISMOS ficheros
                 de producción (SpectralAnalyzer.cpp, PresetManager.cpp) y
                 conduce el flujo real WAV -> detectPitch -> analyze ->
                 serialización v2 (dialecto exacto de exportModel()) ->
                 PresetManager::loadModelFromFile -> sampleFrame, contra WAVs
                 reales de notas de sintetizador.

  ==============================================================================
*/

#include "../Source/ModelMaker/Analysis/SpectralAnalyzer.h"
#include "../Source/Common/SpectralModel.h"
#include "../Source/DSP/FrameSampler.h"
#include "../Source/Serialization/PresetManager.h"
#include "../Source/Main/NEURONiKProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdio>

using NEURONiK::Common::SpectralModel;

namespace
{
void fail (const juce::String& what)
{
    std::printf ("[probe] FAIL: %s\n", what.toRawUTF8());
    std::exit (1);
}

/** Serialización v2 EXACTA de exportModel() (MainComponent.cpp) — mismo
    dialecto, clamps incluidos: el ciclo WAV->v2->load es el de producción. */
juce::String serializeV2 (const SpectralModel& m, const juce::String& name)
{
    const float span = m.frameSpanHz;

    juce::DynamicObject::Ptr modelObj = new juce::DynamicObject();

    juce::Array<juce::var> amps, offs;
    for (int i = 0; i < 64; ++i)
    {
        const float gap = span > 0.0f ? span : (float) (i + 1);
        amps.add (m.amplitudes[i]);
        offs.add (juce::jlimit (-0.5f * gap, 0.5f * gap, m.frequencyOffsets[i]));
    }
    modelObj->setProperty ("amplitudes", amps);
    modelObj->setProperty ("frequencyOffsets", offs);
    modelObj->setProperty ("name", name);
    modelObj->setProperty ("description", "Created with NEURONiK Model Maker");

    juce::Array<juce::var> frames;
    juce::DynamicObject::Ptr frame0 = new juce::DynamicObject();
    frame0->setProperty ("amplitudes", amps);
    frame0->setProperty ("frequencyOffsets", offs);
    frames.add (frame0.get());

    // FASE 10.4: los frames extra acompanan cuando el modelo es temporal.
    for (int fr = 1; fr < m.frameCount; ++fr)
    {
        juce::Array<juce::var> fa, fo;
        for (int i = 0; i < 64; ++i)
        {
            const float gap = span > 0.0f ? span : (float) (i + 1);
            fa.add (m.ampAt (fr, i));
            fo.add (juce::jlimit (-0.5f * gap, 0.5f * gap, m.offsetAt (fr, i)));
        }
        juce::DynamicObject::Ptr fo_ = new juce::DynamicObject();
        fo_->setProperty ("amplitudes", fa);
        fo_->setProperty ("frequencyOffsets", fo);
        frames.add (fo_.get());
    }

    modelObj->setProperty ("format", 2);
    modelObj->setProperty ("frames", frames);
    if (span > 0.0f)
        modelObj->setProperty ("frameSpanHz", span);

    return juce::JSON::toString (juce::var (modelObj));
}

int argmax (const SpectralModel& m)
{
    int k = 0;
    for (int i = 1; i < 64; ++i)
        if (m.amplitudes[i] > m.amplitudes[k]) k = i;
    return k;
}

float centroid (const SpectralModel& m)
{
    double num = 0.0, den = 0.0;
    for (int i = 0; i < 64; ++i)
    {
        const double n = m.amplitudes[i];
        num += n * (i + 1);
        den += n;
    }
    return den > 0.0 ? (float) (num / den) : 0.0f;
}

void probeWav (const juce::File& wav, const juce::File& outDir,
               juce::AudioFormatManager& fmts)
{
    std::printf ("[probe] ---- %s\n", wav.getFileName().toRawUTF8());

    // 1. Lectura real del WAV (juce_audio_formats, como la GUI: LOAD AUDIO)
    std::unique_ptr<juce::AudioFormatReader> r (fmts.createReaderFor (wav));
    if (r == nullptr) fail ("no hay reader para " + wav.getFullPathName());
    juce::AudioBuffer<float> audio ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (&audio, 0, (int) r->lengthInSamples, 0, true, true);
    const double sr = r->sampleRate;

    // 2. El flujo exacto de la GUI: detectPitch -> analyze
    NEURONiK::ModelMaker::Analysis::SpectralAnalyzer analyzer;
    const float f0 = analyzer.detectPitch (audio, sr);
    const SpectralModel model = analyzer.analyze (audio, sr, f0);

    if (f0 <= 20.0f || f0 > 5000.0f) fail ("detectPitch fuera de rango audible");
    if (model.frameCount != 1)       fail ("el analizador monoframe debe dar frameCount=1");
    if (model.frameSpanHz != f0)     fail ("frameSpanHz debe ser la f0 del analisis");

    // Normalización y tonalidad (un modelo a-ruido tiene la energía repartida)
    float sum = 0.0f, top = 0.0f;
    int nonzero = 0;
    for (int i = 0; i < 64; ++i)
    {
        sum += model.amplitudes[i];
        top  = juce::jmax (top, model.amplitudes[i]);
        if (model.amplitudes[i] > 1.0e-3f) ++nonzero;
    }
    if (std::abs (top - 1.0f) > 1.0e-3f) fail ("el pico no esta normalizado a 1");
    if (nonzero < 3)                     fail ("menos de 3 parciales activos: modelo vacio");

    std::printf ("[probe]   f0=%.1f Hz  centroid=parcial %.1f  parciales activos=%d/64  top=%.3f\n",
                 (double) f0, (double) centroid (model), nonzero, (double) top);

    // 3. Export v2 de producción + recarga con el lector REAL
    const auto json = serializeV2 (model, wav.getFileNameWithoutExtension());
    const auto out = outDir.getChildFile (wav.getFileNameWithoutExtension() + ".neuronikmodel");
    if (! out.replaceWithText (json)) fail ("no se pudo escribir el modelo");
    std::printf ("[probe]   export v2: %s (%d bytes)\n",
                 out.getFullPathName().toRawUTF8(), (int) out.getSize());

    const SpectralModel back = NEURONiK::Serialization::PresetManager::loadModelFromFile (out);
    if (! back.isValid)            fail ("el lector de produccion rechazo el modelo");
    if (back.frameCount != 1)      fail ("frames perdidos en el ciclo");
    if (back.frameSpanHz <= 0.0f)  fail ("el lector no vio frameSpanHz");
    if (std::abs (back.frameSpanHz - f0) > 1.0e-4f) fail ("frameSpanHz perdido en el ciclo");

    // 4. El sampler sobre el modelo RECARGADO (lo que hara la voz en el motor)
    for (const float z : { 0.0f, 0.5f, 1.0f })
    {
        SpectralModel frame;
        NEURONiK::Common::sampleFrame (back, z, frame);
        if (std::abs (frame.amplitudes[argmax (frame)] - model.amplitudes[argmax (model)]) > 1.0e-4f)
            fail (juce::String ("sampleFrame z=") + juce::String (z, 2) + " mudo el dominante");
    }

    // 5. E2E con el ENGINE real: el modelo generado entra por la via de
    // produccion (loadModel por FIFO) y la ranura refleja el analisis.
    {
        NEURONiKProcessor audio;
        audio.setRateAndBufferSizeDetails (sr, 512);
        audio.prepareToPlay (sr, 512);

        if (! audio.loadModel (out, 0)) fail ("el engine rechazo el modelo generado");

        std::array<float, 64> slotAmps {}, slotOffs {};
        bool slotValid = false;
        audio.getCurrentModel (0, slotAmps, slotOffs, slotValid);
        if (! slotValid) fail ("la ranura no marco el modelo como valido");

        const int kModel = argmax (model);
        if (std::abs (slotAmps[(size_t) kModel] - model.amplitudes[(size_t) kModel]) > 1.0e-3f)
            fail ("la tabla de la ranura no refleja el analisis");

        std::printf ("[probe]   engine: ranura A valida, parcial %d (=dominante) a %.3f\n",
                     kModel + 1, (double) slotAmps[(size_t) kModel]);
    }

    // 6. FASE 10.4: analisis TEMPORAL del WAV real — 4 frames, export v2 con
    //    los 4 frames y recarga. Con material real los frames deben diferir
    //    (evolucion temporal) y todos con pico valido.
    {
        const SpectralModel temp = analyzer.analyzeTemporal (audio, sr, f0, 4);
        if (temp.frameCount != 4) fail ("analyzeTemporal no produjo 4 frames");

        float minTop = 1.0f, maxAmp0 = 0.0f;
        for (int fr = 0; fr < 4; ++fr)
        {
            float top = 0.0f;
            for (int i = 0; i < 64; ++i) top = juce::jmax (top, temp.ampAt (fr, i));
            minTop = juce::jmin (minTop, top);
            maxAmp0 = juce::jmax (maxAmp0, temp.ampAt (fr, 0));
        }
        if (minTop < 0.05f) fail ("un frame temporal esta vacio");

        const auto tjson = serializeV2 (temp, wav.getFileNameWithoutExtension());
        const auto tout = outDir.getChildFile (wav.getFileNameWithoutExtension() + "-temporal.neuronikmodel");
        if (! tout.replaceWithText (tjson)) fail ("no se pudo escribir el modelo temporal");
        const SpectralModel tback = NEURONiK::Serialization::PresetManager::loadModelFromFile (tout);
        if (! tback.isValid || tback.frameCount != 4) fail ("frames perdidos en el ciclo temporal");

        std::printf ("[probe]   temporal: 4 frames v2 (%d bytes), pico min=%.3f, dominante max=%.3f\n",
                     (int) tjson.length(), (double) minTop, (double) maxAmp0);
    }

    std::printf ("[probe]   OK: ciclo completo WAV->modelo->recarga->sampleFrame->engine\n");
}

} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc < 2)
        fail ("uso: ModelMakerRealWavProbe <wav> [wav...]");

    juce::AudioFormatManager fmts;
    fmts.registerBasicFormats();

    const auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile ("build-reference/probe-models");
    outDir.createDirectory();

    for (int i = 1; i < argc; ++i)
        probeWav (juce::File (juce::String::fromUTF8 (argv[i])), outDir, fmts);

    std::printf ("[probe] RESULT: OK (%d wav(s) analizados -> %s)\n", argc - 1,
                 outDir.getFullPathName().toRawUTF8());
    return 0;
}
