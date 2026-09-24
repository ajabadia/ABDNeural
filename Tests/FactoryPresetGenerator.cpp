/*
  ==============================================================================

    FactoryPresetGenerator.cpp
    Created: 24 Sep 2026
    Description: OFFLINE generator for the NEURONiK factory presets (CZ101 bank).

                 Every factory preset is the REAL APVTS layout at its defaults
                 (createLayoutApvts — the full processor is never built) with
                 `modelPath0` pointing at one of the six .neuronikmodel files
                 the ModelMaker probe extracted from the real CZ101 WAVs. The
                 rest of the parameters stay at their layout default: the
                 factory sound IS the default engine pointed at a real model.

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
        const char* modelFile;   // filename inside Assets/Models (embedded too)
        const char* presetName;  // preset basename -> <name>.neuronikpreset
    };

    // One preset per embedded model, named after it so browser entry, preset
    // file and slot model line up 1:1 (all six sort together under "CZ").
    const FactoryPreset kFactoryPresets[] = {
        { "CZ-BASS1.neuronikmodel",          "CZ-BASS1" },
        { "CZ-HAMOG.neuronikmodel",          "CZ-HAMOG" },
        { "CZ-PAD1.neuronikmodel",           "CZ-PAD1" },
        { "CZ-SWEP1.neuronikmodel",          "CZ-SWEP1" },
        { "CZ-RRISE-temporal.neuronikmodel", "CZ-RRISE-temporal" },
        { "CZ-BASS1-temporal.neuronikmodel", "CZ-BASS1-temporal" },
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
        // Slot A carries the model; slots B-D stay unset (absent property ==
        // no model) and morphX/Y default to 0, so A is what sounds.
        apvts.state.setProperty ("modelPath0",
                                 juce::String (kModelsMarker) + "/" + factory.modelFile,
                                 nullptr);

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
        const bool carriesMarker = back != nullptr
                                   && back->hasTagName (kProductionRootTag)
                                   && back->hasAttribute ("modelPath0")
                                   && back->getStringAttribute ("modelPath0").contains (kModelsMarker);
        if (! carriesMarker)
        {
            std::cerr << "[gen] FALLO: " << file.getFileName()
                      << " no quedo con raiz <Parameters> y modelPath0 marcado\n";
            return 1;
        }

        std::cout << "  [ok]   " << file.getFullPathName()
                  << "  (modelPath0 = " << back->getStringAttribute ("modelPath0") << ")\n";
    }

    std::cout << "[gen] RESULT: OK ("
              << (int) (sizeof (kFactoryPresets) / sizeof (kFactoryPresets[0]))
              << " presets -> " << outDir.getFullPathName() << ")\n";
    return 0;
}
