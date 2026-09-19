/*
  ==============================================================================

    ParameterExportTool.cpp
    Created: 16 Sep 2026
    Description: Regenerates the parameter descriptor artifacts consumed by the
                 WebUI. Usage:

                   NEURONiK_ParameterExport [outputDirectory]

                 Default output directory is WebUI/generated (the contract moved
                 there with the pilot retirement, ticket 8.4), resolved relative
                 to the current working directory when the argument is not
                 absolute.

  ==============================================================================
*/

#include "../Source/State/ParameterDescriptorExport.h"

#include <juce_events/juce_events.h>

#include <iostream>

int main (int argc, char* argv[])
{
    // Reading descriptors materialises an APVTS, which needs a message manager.
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::String targetArgument = argc > 1 ? juce::String (argv[1])
                                                 : juce::String ("WebUI/generated");

    const auto directory = juce::File::isAbsolutePath (targetArgument)
                               ? juce::File (targetArgument)
                               : juce::File::getCurrentWorkingDirectory().getChildFile (targetArgument);

    juce::String error;
    if (! NEURONiK::State::writeParameterArtifacts (directory, error))
    {
        std::cerr << "NEURONiK parameter export failed: " << error << '\n';
        return 1;
    }

    const auto descriptorCount = NEURONiK::State::getParameterDescriptors().size();
    const auto unrouted = NEURONiK::State::getUnroutedParameterIds();

    std::cout << "NEURONiK parameter export\n";
    std::cout << "  output directory : " << directory.getFullPathName() << '\n';
    std::cout << "  parameters       : " << descriptorCount << '\n';
    // Empty is the expected state since 2026-09-19 (no phantom IDs); print it as such
    // instead of leaving a blank that reads like a failed query.
    std::cout << "  unrouted ids     : "
              << (unrouted.isEmpty() ? juce::String ("(none)")
                                     : unrouted.joinIntoString (", ")) << '\n';
    std::cout << "  files            : "
              << NEURONiK::State::ParameterArtifacts::jsonFileName << ", "
              << NEURONiK::State::ParameterArtifacts::javaScriptFileName << ", "
              << NEURONiK::State::ParameterArtifacts::typeScriptFileName << '\n';

    return 0;
}
