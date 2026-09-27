/*
  ==============================================================================

    PresetManager.cpp
    Created: 25 Jan 2026
    Description: Implementation of PresetManager.

  ==============================================================================
*/

#include "PresetManager.h"
#include "PresetMigration.h"

namespace NEURONiK::Serialization {

const juce::String PresetManager::presetExtension = ".neuronikpreset";

namespace
{
    /**
     * FASE 11.1: lee UNA capa del bloque "layers" del formato v2.1 al modelo
     * (frames, nombre, peso estatico y pesos temporales). Devuelve los frames
     * leidos, o 0 si la capa no es valida (sin "frames" o con el primero
     * recortado): esa capa y las siguientes se descartan y el modelo conserva
     * las que si se leyeron — la misma politica que un modelo con mas frames de
     * los que caben.
     *
     * El frameF0 por frame es opcional; sin el, la capa hereda la rejilla comun
     * del fichero (frameSpanHz), que es lo que genera la separacion de capas
     * (todas las capas comparten la f0, ver LAYER_SEPARATION_PLAN §5).
     */
    /**
     * Nombre y mezcla estatica de una capa: lo que viaja fuera de sus frames.
     * Se lee para la capa 0 tambien, porque su nombre y su peso SOLO existen aqui
     * (la raiz del v2 lleva el nombre del MODELO). Los frames de la capa 0 no se
     * leen nunca del bloque: son los de la raiz, que es donde los dejo el v2 y
     * quien manda si el fichero se contradice.
     */
    void readLayerHeader (const juce::var& layerVar, NEURONiK::Common::SpectralModel& model,
                          int layer)
    {
        const auto* layerObj = layerVar.getDynamicObject();
        if (layerObj == nullptr) return;

        model.setLayerNameAt (layer, layerObj->getProperty ("name").toString());

        // El peso estatico ausente vale 1.0, pero 0 es un valor legitimo (capa
        // silenciada): la ausencia se distingue con isVoid(), no con el valor.
        if (! layerObj->getProperty ("weight").isVoid())
            model.setLayerWeightAt (layer, (float) (double) layerObj->getProperty ("weight"));
    }

    int readLayer (const juce::var& layerVar, NEURONiK::Common::SpectralModel& model, int layer)
    {
        const auto* layerObj = layerVar.getDynamicObject();
        if (layerObj == nullptr) return 0;

        readLayerHeader (layerVar, model, layer);

        const auto* frames = layerObj->getProperty ("frames").getArray();
        if (frames == nullptr) return 0;

        const auto* weights = layerObj->getProperty ("frameWeights").getArray();
        const int maxFrames = NEURONiK::Common::SpectralModel::kMaxFrames;
        int count = 0;

        for (int f = 0; f < frames->size() && f < maxFrames; ++f)
        {
            const auto* frameObj = (*frames)[f].getDynamicObject();
            if (frameObj == nullptr) break;

            const auto* amps = frameObj->getProperty ("amplitudes").getArray();
            const auto* offsets = frameObj->getProperty ("frequencyOffsets").getArray();

            if (amps == nullptr || offsets == nullptr
                    || amps->size() < 64 || offsets->size() < 64) break;

            float* ampOut = model.ampsOf (layer, f);
            float* offsetOut = model.offsetsOf (layer, f);

            for (int i = 0; i < 64; ++i)
            {
                ampOut[(size_t) i] = (float) (double) (*amps)[i];
                offsetOut[(size_t) i] = (float) (double) (*offsets)[i];
            }

            const auto& frameF0 = frameObj->getProperty ("frameF0");
            model.setF0At (layer, f, frameF0.isVoid() ? model.frameSpanHz
                                                     : (float) (double) frameF0);

            if (weights != nullptr && f < weights->size())
                model.setFrameWeightAt (layer, f, (float) (double) (*weights)[f]);

            ++count;
        }

        return count;
    }
} // namespace

Common::SpectralModel PresetManager::loadModelFromFile(const juce::File& file)
{
    Common::SpectralModel model;
    model.amplitudes.fill(0.0f);
    model.frequencyOffsets.fill(0.0f);

    if (!file.existsAsFile()) return model;

    // 1. JSON — el dialecto que ESCRIBE la propia herramienta
    //    (`Source/ModelMaker/MainComponent.cpp::exportModel`: amplitudes[64] +
    //    frequencyOffsets[64] + name + description). Era el unico que este lector no
    //    entendia, asi que un modelo salido del Model Maker se rechazaba en silencio:
    //    la ranura se quedaba EMPTY y no sonaba, y el plugin parecia roto sin decir
    //    por que. Si algun dia cambia el escritor, cambia AQUI, no en dos sitios.
    // OJO con el temporal: `var` es un puntero con contador, y `getDynamicObject()`
    // devuelve el objeto CRUDO. Encadenar `JSON::parse(texto).getDynamicObject()` deja el
    // puntero apuntando a un objeto ya liberado en cuanto muere el temporal de la
    // sentencia — se lee basura y el modelo se rechaza como si el fichero estuviera mal.
    // El `var` vive en una local hasta que se termina de leer.
    const auto parsedJson = juce::JSON::parse (file.loadFileAsString());
    const auto* modelObject = parsedJson.getDynamicObject();

    if (modelObject != nullptr)
    {
        const auto* amplitudes = modelObject->getProperty ("amplitudes").getArray();
        const auto* offsets = modelObject->getProperty ("frequencyOffsets").getArray();

        // Un modelo recortado no es un modelo: 64 parciales o nada (leer los que
        // hubiera dejaria una ranura a medias que suena a otra cosa).
        if (amplitudes != nullptr && offsets != nullptr
                && amplitudes->size() >= 64 && offsets->size() >= 64)
        {
            for (int i = 0; i < 64; ++i)
            {
                model.amplitudes[static_cast<size_t> (i)] =
                    static_cast<float> (static_cast<double> ((*amplitudes)[i]));
                model.frequencyOffsets[static_cast<size_t> (i)] =
                    static_cast<float> (static_cast<double> ((*offsets)[i]));
            }

            model.isValid = true;

            // FASE 10.1: frameSpanHz (dimension real de la envoltura de camino
            // corto). Opcional: sin el (v1) el sampler usa el fallback
            // armonico unitario, como siempre hizo.
            if (!modelObject->getProperty ("frameSpanHz").isVoid())
                model.frameSpanHz = static_cast<float> (
                    static_cast<double> (modelObject->getProperty ("frameSpanHz")));

            // 2026-09-25: REJILLA FIJA (procedencia del modo declarado del
            // ModelMaker). Opcional: ausente = false (el modelo de siempre), asi
            // que un fichero v1/v2 no se lee distinto por esto.
            if (!modelObject->getProperty ("gridFixed").isVoid())
                model.gridFixed = modelObject->getProperty ("gridFixed");

            // 2026-09-25: OFFSETS TRANSPONIBLES (ratio delta-n/n). Opcional:
            // ausente = false (el modelo de siempre), asi que un fichero
            // v1/v2/v2.1 sin la clave no se lee distinto. La rejilla de
            // referencia es frameSpanHz (ya leida arriba); el sampler la
            // transporta al snapshot y el motor la usa para escalar.
            if (!modelObject->getProperty ("offsetsTranspose").isVoid())
                model.offsetsTranspose = modelObject->getProperty ("offsetsTranspose");

            // FASE 10: frames temporales (formato v2). Opcional: sin "frames"
            // (v1) el modelo queda estatico. Con "frames", el elemento [0] es
            // el canonico ya leido; los demas llenan extraAmps/extraOffsets.
            // Un frame recortado NO invalida el modelo: se trunca frameCount
            // (los frames parciales se ignoran, el sonido de siempre no cambia).
            if (const auto* frames = modelObject->getProperty ("frames").getArray())
            {
                const int maxFrames = Common::SpectralModel::kMaxFrames;
                int count = 0;

                for (int f = 1; f < frames->size() && count < maxFrames - 1; ++f)
                {
                    const auto* frameObj = (*frames)[f].getDynamicObject();
                    if (frameObj == nullptr) break;

                    const auto* fAmps = frameObj->getProperty ("amplitudes").getArray();
                    const auto* fOffs = frameObj->getProperty ("frequencyOffsets").getArray();
                    if (fAmps == nullptr || fOffs == nullptr
                            || fAmps->size() < 64 || fOffs->size() < 64) break;

                    for (int i = 0; i < 64; ++i)
                    {
                        model.extraAmps[(size_t) count][(size_t) i] =
                            static_cast<float> (static_cast<double> ((*fAmps)[i]));
                        model.extraOffsets[(size_t) count][(size_t) i] =
                            static_cast<float> (static_cast<double> ((*fOffs)[i]));
                    }
                    // FASE 10.6: raiz del frame (opcional; 0 = la canonica).
                    model.extraF0[(size_t) count] =
                        static_cast<float> (static_cast<double> (frameObj->getProperty ("frameF0")));
                    ++count;
                }

                model.frameCount = 1 + count;
            }

            // FASE 11.1: CAPAS (formato v2.1). OPCIONAL y aditivo: sin "layers"
            // el modelo es v2 puro (layerCount = 1) y todo lo de arriba es el
            // modelo. El bloque va DESPUES de los frames de la raiz porque esos
            // SON la capa 0: un lector v2 antiguo (y el puente WASM, que cruza
            // 128 floats planos) suena esa capa y ni siquiera ve "layers" — de
            // ahi que el formato sea compatible hacia atras.
            if (const auto* layersBlock = modelObject->getProperty ("layers").getDynamicObject())
            {
                const auto* layerArray = layersBlock->getProperty ("layers").getArray();

                // La capa 0 es la raiz (sus frames no se releen), pero su nombre y
                // su mezcla viven SOLO en este bloque: la raiz del v2 lleva el
                // nombre del MODELO, no el de la capa.
                if (layerArray != nullptr && layerArray->size() > 0)
                    readLayerHeader ((*layerArray)[0], model, 0);

                // Una sola capa no aporta nada nuevo: la raiz ya es esa capa.
                if (layerArray != nullptr && layerArray->size() > 1)
                {
                    // Un fichero con MAS capas de las que caben se trunca, como
                    // un modelo con mas frames de los que caben (no se rechaza).
                    const int declared = (int) layersBlock->getProperty ("layerCount");
                    const int kMaxLayers = Common::SpectralModel::kMaxLayers;
                    int wanted = juce::jmin ((int) layerArray->size(), kMaxLayers);

                    if (declared > 0)
                        wanted = juce::jmin (wanted, declared);

                    int read = 0;

                    for (int l = 1; l < wanted; ++l)   // la capa 0 es la raiz: no se relee
                    {
                        const int layerFrames = readLayer ((*layerArray)[l], model, l);
                        if (layerFrames <= 0) break;   // capa invalida: se para aqui

                        model.setNumFramesOf (l, layerFrames);
                        read = l;
                    }

                    if (read > 0)
                        model.setLayerCount (read + 1);
                }
            }

            return model;
        }
    }

    // 2. XML legacy (`<NEURONIK_MODEL amplitudes="." offsets="."/>`), el dialecto que
    //    este lector esperaba antes de que existiera el Model Maker. Sigue cargando:
    //    hay modelos de esa era en discos y presets.
    auto xml = juce::parseXML(file);
    if (xml != nullptr && xml->hasTagName("NEURONIK_MODEL"))
    {
        juce::String amps = xml->getStringAttribute("amplitudes");
        juce::String freqs = xml->getStringAttribute("offsets");

        juce::StringArray ampList;
        ampList.addTokens(amps, ",", "");
        
        juce::StringArray freqList;
        freqList.addTokens(freqs, ",", "");

        for (int i = 0; i < 64; ++i)
        {
            if (i < ampList.size()) model.amplitudes[i] = ampList[i].getFloatValue();
            if (i < freqList.size()) model.frequencyOffsets[i] = freqList[i].getFloatValue();
        }
        model.isValid = true;
    }

    return model;
}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& apvts)
    : valueTreeState(apvts), currentPresetName("Init Preset")
{
    // Ensure presets directory exists (recursive creation)
    const auto presetsDir = getPresetsDirectory();
    if (!presetsDir.exists())
    {
        auto result = presetsDir.createDirectory();
        if (result.failed())
        {
            // Log error or fallback?
            #if JUCE_DEBUG
            DBG("PresetManager: Failed to create presets directory: " + result.getErrorMessage());
            #endif
        }
    }
}

void PresetManager::savePreset(const juce::String& presetName)
{
    savePresetInFolder(presetName, "");
}

void PresetManager::savePresetInFolder(const juce::String& presetName, const juce::String& folderName)
{
    auto presetsDir = getPresetsDirectory();
    if (folderName.isNotEmpty())
    {
        presetsDir = presetsDir.getChildFile(folderName);
        if (!presetsDir.exists())
            presetsDir.createDirectory();
    }

    savePresetToFile(presetsDir.getChildFile(presetName + presetExtension));
}

void PresetManager::savePresetToFile(const juce::File& file)
{
    // Load existing tags if file exists to preserve them
    auto tags = getTagsForPreset(file);
    
    auto xml = valueTreeState.copyState().createXml();
    if (tags.size() > 0)
    {
        auto* metadata = xml->createNewChildElement("METADATA");
        metadata->setAttribute("tags", tags.joinIntoString(","));
    }
    
    auto result = xml->writeTo(file);
    if (!result)
    {
        #if JUCE_DEBUG
        DBG("PresetManager: Failed to write XML to file: " + file.getFullPathName());
        #endif
    }
    currentPresetName = file.getFileNameWithoutExtension();
    sendChangeMessage();
}

void PresetManager::deletePreset(const juce::String& presetName)
{
    const auto presetsDir = getPresetsDirectory();
    // Search recursively for the file to delete
    auto files = presetsDir.findChildFiles(juce::File::findFiles, true, presetName + presetExtension);
    if (files.size() > 0) {
        files[0].deleteFile();
        sendChangeMessage();
    }
}

void PresetManager::loadPreset(const juce::String& presetName)
{
    const auto presetsDir = getPresetsDirectory();
    auto files = presetsDir.findChildFiles(juce::File::findFiles, true, presetName + presetExtension);

    if (files.size() > 0)
    {
        loadPresetFromFile(files[0]);
    }
}

void PresetManager::loadPresetFromFile(const juce::File& file)
{
    if (file.existsAsFile())
    {
        auto xml = juce::parseXML(file);
        if (xml != nullptr)
        {
            auto state = juce::ValueTree::fromXml(*xml);

            // Presets saved by older builds may carry parameters that no longer
            // exist (e.g. harmMix). Dropping them here keeps the state honest and
            // stops a re-save from writing the dead ids back to the file.
            migratePresetState(state, valueTreeState.processor);

            // Los presets previos a ENV1/ENV2 cableaban las envolventes dentro de
            // la voz; hacer visible ese cableado en la matriz no cambia el sonido
            // ("no route" == depth 1.0 en el DSP).
            insertEnvModRoutes(state);

            valueTreeState.replaceState(state);
            currentPresetName = file.getFileNameWithoutExtension();
            sendChangeMessage();
        }
    }
}

int PresetManager::loadNextPreset()
{
    const auto presets = getAllPresets();
    if (presets.isEmpty()) return -1;

    const auto currentIndex = presets.indexOf(currentPresetName);
    const auto nextIndex = (currentIndex + 1) % presets.size();
    loadPreset(presets[nextIndex]);
    return nextIndex;
}

int PresetManager::loadPreviousPreset()
{
    const auto presets = getAllPresets();
    if (presets.isEmpty()) return -1;

    const auto currentIndex = presets.indexOf(currentPresetName);
    const auto prevIndex = (currentIndex + presets.size() - 1) % presets.size();
    loadPreset(presets[prevIndex]);
    return prevIndex;
}

juce::StringArray PresetManager::getAllPresets() const
{
    juce::StringArray presets;
    const auto presetsDir = getPresetsDirectory();
    
    // Scan directory
    auto files = presetsDir.findChildFiles(juce::File::findFiles, false, "*" + presetExtension);
    files.sort();

    for (const auto& file : files)
    {
        presets.add(file.getFileNameWithoutExtension());
    }
    return presets;
}

juce::String PresetManager::getCurrentPreset() const
{
    return currentPresetName;
}

juce::File PresetManager::getPresetsDirectory() const
{
    // For now, save in Documents/NEURONiK/Presets
    auto result = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                  .getChildFile("NEURONiK")
                  .getChildFile("Presets");
    return result;
}

void PresetManager::saveBank(const juce::File& targetFile, const juce::File& sourceDir)
{
    if (!sourceDir.isDirectory()) return;
    
    // JUCE ZipFile requires a stream
    if (targetFile.existsAsFile()) targetFile.deleteFile();
    
    juce::FileOutputStream fos(targetFile);
    if (fos.openedOk())
    {
        juce::ZipFile::Builder builder;
        juce::Array<juce::File> filesToAdd;
        sourceDir.findChildFiles(filesToAdd, juce::File::findFiles, true, "*" + presetExtension);
        
        for (const auto& f : filesToAdd)
        {
            // Relative path inside zip
            auto relPath = f.getRelativePathFrom(sourceDir);
            builder.addFile(f, 9, relPath);
        }
        
        builder.writeToStream(fos, nullptr);
    }
}

void PresetManager::loadBank(const juce::File& bankFile)
{
    if (!bankFile.existsAsFile()) return;
    
    juce::ZipFile zip(bankFile);
    auto targetDir = getPresetsDirectory().getChildFile(bankFile.getFileNameWithoutExtension());
    if (!targetDir.exists()) targetDir.createDirectory();
    
    zip.uncompressEntry(0, targetDir); // Unpack all? JUCE's ZipFile doesn't have "unpack all" in one call easily
    // We iterate entries
    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        zip.uncompressEntry(i, targetDir);
    }
    sendChangeMessage();
}

void PresetManager::setTagsForPreset(const juce::File& file, const juce::StringArray& tags)
{
    if (!file.existsAsFile()) return;
    
    auto xml = juce::parseXML(file);
    if (xml != nullptr)
    {
        auto* metadata = xml->getChildByName("METADATA");
        if (metadata == nullptr) metadata = xml->createNewChildElement("METADATA");
        
        metadata->setAttribute("tags", tags.joinIntoString(","));
        xml->writeTo(file);
    }
}

juce::StringArray PresetManager::getTagsForPreset(const juce::File& file) const
{
    if (!file.existsAsFile()) return {};
    
    auto xml = juce::parseXML(file);
    if (xml != nullptr)
    {
        if (auto* metadata = xml->getChildByName("METADATA"))
        {
            juce::String tagsCsv = metadata->getStringAttribute("tags");
            juce::StringArray tags;
            tags.addTokens(tagsCsv, ",", "\"");
            tags.trim();
            tags.removeEmptyStrings();
            return tags;
        }
    }
    return {};
}

juce::StringArray PresetManager::getAllUniqueTags() const
{
    juce::StringArray uniqueTags;
    auto presetsDir = getPresetsDirectory();
    auto files = presetsDir.findChildFiles(juce::File::findFiles, true, "*" + presetExtension);
    
    for (const auto& f : files)
    {
        auto tags = getTagsForPreset(f);
        for (const auto& tag : tags)
        {
            if (!uniqueTags.contains(tag, true)) // Case-insensitive check
                uniqueTags.add(tag);
        }
    }
    
    uniqueTags.sort(true);
    return uniqueTags;
}

} // namespace NEURONiK::Serialization
