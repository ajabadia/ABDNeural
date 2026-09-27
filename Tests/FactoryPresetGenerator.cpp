/*
  ==============================================================================

    FactoryPresetGenerator.cpp
    Created: 24 Sep 2026
    Description: OFFLINE generator for the NEURONiK factory presets (CZ101 bank).

                 Every factory preset is the REAL APVTS layout at its defaults
                 (createLayoutApvts — the full processor is never built) with
                 `modelPath<slot>` pointing at the .neuronikmodel files the
                 ModelMaker probe extracted from the real CZ101 WAVs: one preset
                 per embedded model (slot A) plus ONE BANK PRESET that fills the
                 four slots with the four tonal patches, so the XY pad morphs
                 the whole bank (morphX/morphY stay at their defaults = 0: it
                 opens on slot A and the pad walks B/C/D). The rest of the
                 parameters stay at their layout default: the factory sound IS
                 the default engine pointed at real models.

                 The path travels with the {{FACTORY_MODELS}} marker because
                 modelPath<slot> is an ABSOLUTE path (per machine) and the
                 preset ships embedded in the binary: installFactoryPresets()
                 swaps the marker for Documents/NEURONiK/Models when the
                 plugin installs the preset at startup (only if missing —
                 never rewrites what the user already has).

                 The XML is serialized exactly like the plugin does
                 (copyState().createXml()) but the root tag is re-labelled to
                 `Parameters`: the light layout APVTS carries the contract type
                 (NEURONiKContractProbe) while the REAL processor's state root
                 is `Parameters` — a factory preset must be the byte the plugin
                 itself would write.

                 Output: Assets/Presets/<model-basename>.neuronikpreset —
                 checked in and embedded via the NEURONiK_FactoryModels
                 binary-data target. Regenerate with:

                     cmake --build <builddir> --target NEURONiK_FactoryPresetGenerator
                     <builddir>/NEURONiK_FactoryPresetGenerator [outputDir]

                 No add_test on purpose: it rewrites repository files (a
                 generator tool, like ModelMakerRealWavProbe, not a test).

  ==============================================================================
*/

#include "../Source/Serialization/PresetManager.h"
#include "../Source/State/ParameterDefinitions.h"
#include "../Source/State/ParameterDescriptors.h"

#include <iostream>

namespace
{
    using namespace NEURONiK;

    /** The marker installFactoryPresets() replaces with the real models dir. */
    constexpr const char* kModelsMarker = "{{FACTORY_MODELS}}";

    /** The root tag of a preset saved by the REAL processor (NEURONiKProcessor
        constructs its APVTS with "Parameters"; the light one uses the contract
        type, so the generator re-labels the XML after createXml). */
    constexpr const char* kProductionRootTag = "Parameters";

    struct FactoryPreset
    {
        const char* presetName;  // preset basename -> <name>.neuronikpreset
        const char* models[4];   // slot A..D, fichero dentro de Assets/Models
                                 // (nullptr = ranura vacia, como hoy)
    };

    // One preset per embedded model, named after it so browser entry, preset
    // file and slot model line up 1:1 (all six sort together under "CZ"), plus
    // the BANK preset: los cuatro patches TONALES del banco CZ101 en las cuatro
    // esquinas del pad XY (A=BASS1, B=HAMOG, C=PAD1, D=SWEP1). CZ-RRISE no entra
    // en el banco: es un barrido de pitch y el banco es material estatico (su
    // preset temporal ya existe aparte). El pad arranca en la esquina A
    // (morphX/morphY = 0 es el default del layout) y al arrastrarlo recorre los
    // otros tres patches: es el banco entero en un solo preset.
    const FactoryPreset kFactoryPresets[] = {
        { "CZ-BASS1",          { "CZ-BASS1.neuronikmodel", nullptr, nullptr, nullptr } },
        { "CZ-HAMOG",          { "CZ-HAMOG.neuronikmodel", nullptr, nullptr, nullptr } },
        { "CZ-PAD1",           { "CZ-PAD1.neuronikmodel", nullptr, nullptr, nullptr } },
        { "CZ-SWEP1",          { "CZ-SWEP1.neuronikmodel", nullptr, nullptr, nullptr } },
        { "CZ-RRISE-temporal", { "CZ-RRISE-temporal.neuronikmodel", nullptr, nullptr, nullptr } },
        { "CZ-BASS1-temporal", { "CZ-BASS1-temporal.neuronikmodel", nullptr, nullptr, nullptr } },
        { "CZ101-BANK",        { "CZ-BASS1.neuronikmodel", "CZ-HAMOG.neuronikmodel",
                                   "CZ-PAD1.neuronikmodel", "CZ-SWEP1.neuronikmodel" } },
    };
} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto outDir = argc > 1
                            ? juce::File::getCurrentWorkingDirectory()
                                  .getChildFile (juce::String::fromUTF8 (argv[1]))
                            : juce::File::getCurrentWorkingDirectory()
                                  .getChildFile ("Assets")
                                  .getChildFile ("Presets");

    if (const auto result = outDir.createDirectory(); result.failed())
    {
        std::cerr << "[gen] no se pudo crear " << outDir.getFullPathName()
                  << ": " << result.getErrorMessage() << '\n';
        return 1;
    }

    // The real layout in a light APVTS: exactly the parameters the plugin
    // ships, without constructing NEURONiKProcessor (PresetRoundTrip pattern).
    // No PresetManager instance on purpose: constructing one creates the real
    // Documents presets directory as a side effect — a generator must not.
    auto layout = State::createLayoutApvts();
    auto& apvts = *layout.apvts;

    std::cout << "NEURONiK factory preset generator (CZ101 bank)\n";

    for (const auto& factory : kFactoryPresets)
    {
        // Ranuras limpias antes de cada preset: el estado es UNO y se reutiliza,
        // asi que sin esto el banco (que llena A-D) dejaria modelos en los
        // presets siguientes. Slot sin modelo = propiedad AUSENTE (no vacia):
        // es lo que el processor lee como "sin modelo".
        for (int slot = 0; slot < 4; ++slot)
            apvts.state.removeProperty ("modelPath" + juce::String (slot), nullptr);

        for (int slot = 0; slot < 4; ++slot)
        {
            if (const auto* modelFile = factory.models[slot])
                apvts.state.setProperty ("modelPath" + juce::String (slot),
                                         juce::String (kModelsMarker) + "/" + modelFile,
                                         nullptr);
        }

        const auto file = outDir.getChildFile (juce::String (factory.presetName)
                                               + Serialization::PresetManager::presetExtension);

        auto xml = apvts.copyState().createXml();
        if (xml == nullptr)
        {
            std::cerr << "[gen] FALLO: copyState no serialize a XML\n";
            return 1;
        }

        xml->setTagName (kProductionRootTag);

        if (! xml->writeTo (file))
        {
            std::cerr << "[gen] FALLO: no se pudo escribir " << file.getFullPathName() << '\n';
            return 1;
        }

        // Round-trip guard: what landed on disk parses, carries the production
        // root and still holds the models marker (that is what
        // installFactoryPresets() substitutes at install time).
        auto back = juce::parseXML (file);

        // Guardia de round trip por RANURA: lo escrito parsea, lleva la raiz de
        // produccion y cada slot declarado conserva el marcador que
        // installFactoryPresets() sustituye al instalar. Un slot declarado que
        // llegue sin marcador (o un slot extra que se cuele) rompe aqui.
        bool carriesMarker = back != nullptr && back->hasTagName (kProductionRootTag);

        if (carriesMarker)
        {
            for (int slot = 0; slot < 4; ++slot)
            {
                const auto attribute = "modelPath" + juce::String (slot);
                const bool declared = factory.models[slot] != nullptr;
                const bool present = back->hasAttribute (attribute);

                if (present != declared
                        || (declared && ! back->getStringAttribute (attribute).contains (kModelsMarker)))
                {
                    carriesMarker = false;
                    std::cerr << "[gen] FALLO: " << file.getFileName() << ": ranura " << slot
                              << (declared ? " declarada sin marcador" : " sin declarar y presente") << '\n';
                }
            }
        }

        if (! carriesMarker)
        {
            std::cerr << "[gen] FALLO: " << file.getFileName()
                      << " no quedo con raiz <Parameters> y sus modelPath<slot> marcados\n";
            return 1;
        }

        std::cout << "  [ok]   " << file.getFullPathName();

        for (int slot = 0; slot < 4; ++slot)
            if (factory.models[slot] != nullptr)
                std::cout << "\n           modelPath" << slot << " = " << factory.models[slot];

        std::cout << '\n';
    }

    std::cout << "[gen] RESULT: OK ("
              << (int) (sizeof (kFactoryPresets) / sizeof (kFactoryPresets[0]))
              << " presets -> " << outDir.getFullPathName() << ")\n";
    return 0;
}
