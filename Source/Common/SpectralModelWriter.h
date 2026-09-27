/*
  ==============================================================================

    SpectralModelWriter.h
    Created: 24 Sep 2026
    Description: EL escritor del dialecto .neuronikmodel (v2 / v2.1), compartido
                 por la GUI del Model Maker, los tests y la sonda RealWav.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include "SpectralModel.h"

namespace NEURONiK::Common {

/**
 * @brief El dialecto `.neuronikmodel` que ESCRIBE la herramienta, en un sitio.
 *
 * FASE 11.1: antes habia TRES copias de este JSON (exportModel de la GUI, el
 * test de roundtrip y la sonda RealWav) y las tres tenian que cambiar a la vez
 * con cada campo nuevo. Con las capas, el formato crece: pasa a ser UNA
 * funcion. La GUI pone la POLITICA (nombre, guardia de afinacion, dialogo de
 * fichero), aqui vive el FORMATO. El lector es PresetManager::loadModelFromFile
 * (Serialization) y los dos comparten los nombres de campo de aqui abajo.
 *
 * v2 (capas = 1): raiz amplitudes[64]/frequencyOffsets[64] (frame canonico,
 * layout v1 exacto: un plugin v1 lo lee y suena) + "frames" (UN elemento si el
 * modelo es estatico) + "frameSpanHz" cuando el analisis lo trajo + "format": 2.
 * Los offsets van acotados a media banda (|offset| <= 0.5*gap, gap = frameSpanHz
 * o el armonico unitario n=i+1): el camino corto del sampler (FrameSampler.h)
 * basta con UNA envoltura y los extremos z=0/z=1 se copian bit-exactos.
 *
 * v2.1 (capas > 1): ademas "layers": { "layerCount": N, "layers": [ { "name",
 * "weight", "frames", "frameWeights" } ] } y "format": 2.1.
 *   - Los frames de la RAIZ siguen siendo los de la capa 0: un lector v2
 *     antiguo (y el puente WASM, que cruza 128 floats planos) suena esa capa
 *     en vez de no entender nada.
 *   - "frameWeights" es el peso temporal por frame (0..1); ausente = 1.0.
 *   - "frameF0" por frame es opcional (Fase 10.6). La capa 0 no lo escribe para
 *     su frame 0: su raiz es "frameSpanHz", como en el v2 de siempre.
 *
 * "gridFixed": true (2026-09-25, opcional) declara que la f0 la FIJO el usuario
 * (modo "rejilla fija" del ModelMaker) y que el analizador no siguio el pitch
 * por ventana: los frameF0 son la rejilla declarada, no una trayectoria medida.
 * Ausente = false (el modelo de siempre). No sube "format": es ortogonal a las
 * capas, asi que no rompe a ningun lector y el v2/v2.1 de siempre no cambia.
 *
 * "offsetsTranspose": true (2026-09-25, opcional) declara que los offsets del
 * fichero son RATIOS medidos contra la rejilla de analisis (delta-n/n) y el
 * motor debe escalarlos por base/f0 al renderizar: asi la inharmonicidad se
 * TRANSPONE con el teclado y no cambia de caracter segun la nota. Ausente =
 * false y el offset se suma en Hz tal cual (el modelo de siempre). Ortogonal
 * a las capas, tambien fuera de "layers": la rejilla de referencia es la del
 * fichero ("frameSpanHz").
 *
 * Los ficheros con MAS capas de las que caben (kMaxLayers) no se rechazan: se
 * truncan, igual que un modelo con mas frames de los que caben.
 */

namespace detail
{
    /** @brief Un frame (64 amplitudes + 64 offsets) con el clamp de banda. */
    inline juce::DynamicObject::Ptr frameToJson (const float* amplitudes,
                                                 const float* frequencyOffsets,
                                                 float span, float f0Hz)
    {
        juce::Array<juce::var> amps, offsets;

        for (int i = 0; i < 64; ++i)
        {
            const float gap = span > 0.0f ? span : (float) (i + 1);
            amps.add (amplitudes[i]);
            offsets.add (juce::jlimit (-0.5f * gap, 0.5f * gap, frequencyOffsets[i]));
        }

        juce::DynamicObject::Ptr frameObj = new juce::DynamicObject();
        frameObj->setProperty ("amplitudes", amps);
        frameObj->setProperty ("frequencyOffsets", offsets);

        if (f0Hz > 0.0f)
            frameObj->setProperty ("frameF0", f0Hz);

        return frameObj;
    }
}

/**
 * @brief El modelo como JSON v2 (una capa) o v2.1 (capas).
 *
 * Separado de writeModelToFile para poder comprobarlo sin disco.
 */
inline juce::var modelToJson (const SpectralModel& model,
                              const juce::String& name,
                              const juce::String& description)
{
    const float span = model.frameSpanHz;

    juce::DynamicObject::Ptr modelObj = new juce::DynamicObject();

    // La raiz y frames[0] salen del MISMO frame: son identicos siempre.
    const float* rootAmps = model.ampsOf (0);
    const float* rootOffsets = model.offsetsOf (0);

    juce::Array<juce::var> amps, offsets;

    for (int i = 0; i < 64; ++i)
    {
        const float gap = span > 0.0f ? span : (float) (i + 1);
        amps.add (rootAmps[i]);
        offsets.add (juce::jlimit (-0.5f * gap, 0.5f * gap, rootOffsets[i]));
    }

    modelObj->setProperty ("amplitudes", amps);
    modelObj->setProperty ("frequencyOffsets", offsets);

    juce::Array<juce::var> frames;
    frames.add (detail::frameToJson (rootAmps, rootOffsets, span, 0.0f).get());

    for (int f = 1; f < model.frameCount; ++f)
        frames.add (detail::frameToJson (model.ampsOf (f), model.offsetsOf (f),
                                         span, model.f0At (f)).get());

    // El v2 puro mantiene "format": 2 ENTERO (su JSON no cambia ni un byte);
    // el 2.1, con capas, es el unico que sube de version.
    modelObj->setProperty ("format", model.isLayered() ? juce::var (2.1) : juce::var (2));
    modelObj->setProperty ("frames", frames);

    if (span > 0.0f)
        modelObj->setProperty ("frameSpanHz", span);

    // 2026-09-25: REJILLA FIJA — SOLO cuando esta declarada (misma regla que el
    // bloque de capas: el JSON de los modelos de siempre no cambia ni un byte).
    if (model.gridFixed)
        modelObj->setProperty ("gridFixed", true);

    // 2026-09-25: OFFSETS TRANSPONIBLES — SOLO cuando el modo esta declarado
    // (misma regla que gridFixed: el JSON de los modelos de siempre no cambia
    // ni un byte). La rejilla de referencia es "frameSpanHz", ya presente.
    if (model.offsetsTranspose)
        modelObj->setProperty ("offsetsTranspose", true);

    // FASE 11.1: el bloque de capas SOLO cuando hay mas de una. El v2 puro no
    // cambia ni un byte, asi que los lectores v2 (y el v1 de la raiz) siguen
    // leyendo exactamente lo de siempre.
    if (model.isLayered())
    {
        juce::Array<juce::var> layerArray;

        for (int l = 0; l < model.layerCount; ++l)
        {
            const int numFrames = model.numFramesOf (l);

            juce::Array<juce::var> layerFrames;

            for (int f = 0; f < numFrames; ++f)
            {
                // La capa 0 no declara frameF0 en su frame 0: su raiz es la del
                // fichero (frameSpanHz), como en el bloque v2 de arriba.
                const float f0Hz = (l == 0 && f == 0) ? 0.0f : model.f0At (l, f);
                layerFrames.add (detail::frameToJson (model.ampsOf (l, f), model.offsetsOf (l, f),
                                                      span, f0Hz).get());
            }

            juce::Array<juce::var> weights;

            for (int f = 0; f < numFrames; ++f)
                weights.add (model.frameWeightAt (l, f));

            juce::DynamicObject::Ptr layerObj = new juce::DynamicObject();
            layerObj->setProperty ("name", model.layerNameAt (l));
            layerObj->setProperty ("weight", model.layerWeightAt (l));
            layerObj->setProperty ("frames", layerFrames);
            layerObj->setProperty ("frameWeights", weights);
            layerArray.add (layerObj.get());
        }

        juce::DynamicObject::Ptr layersObj = new juce::DynamicObject();
        layersObj->setProperty ("layerCount", model.layerCount);
        layersObj->setProperty ("layers", layerArray);
        modelObj->setProperty ("layers", layersObj.get());
    }

    modelObj->setProperty ("name", name);
    modelObj->setProperty ("description", description);

    return juce::var (modelObj.get());
}

/** @brief Escribe el modelo en `file` (el .neuronikmodel del Model Maker). */
inline bool writeModelToFile (const juce::File& file, const SpectralModel& model,
                              const juce::String& name, const juce::String& description)
{
    return file.replaceWithText (juce::JSON::toString (modelToJson (model, name, description)));
}

} // namespace NEURONiK::Common
