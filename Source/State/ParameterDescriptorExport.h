/*
  ==============================================================================

    ParameterDescriptorExport.h
    Created: 16 Sep 2026
    Description: Serialises the parameter descriptor contract into artifacts that
                 non-C++ consumers (the Next.js pilot today, the final WebUI
                 later) can import.

                 Three artifacts are produced from the same in-memory table:
                   - parameters.generated.json  (data snapshot / tooling)
                   - parameters.generated.js    (runtime module for the WebUI)
                   - parameters.generated.d.ts  (types for TypeScript consumers)

                 Generation is deterministic: no timestamps, so the regression
                 suite can compare on-disk artifacts against a fresh build and
                 fail when the contract and the committed files drift apart.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

#include "ParameterDescriptors.h"

namespace NEURONiK::State
{

/** @brief Container of every generated artifact and its relative file name. */
struct ParameterArtifacts
{
    juce::String json;
    juce::String javaScript;
    juce::String typeScript;

    static constexpr const char* jsonFileName = "parameters.generated.json";
    static constexpr const char* javaScriptFileName = "parameters.generated.js";
    static constexpr const char* typeScriptFileName = "parameters.generated.d.ts";
};

/** @brief Render all artifacts from the current descriptor table. */
ParameterArtifacts buildParameterArtifacts();

/** @brief Render only the JSON snapshot. */
juce::String buildParameterArtifactsJson();

/** @brief Render only the ES module consumed by the WebUI. */
juce::String buildParameterArtifactsJavaScript();

/** @brief Render only the TypeScript declarations. */
juce::String buildParameterArtifactsTypeScript();

/**
 * @brief Write all artifacts into a directory, creating it when needed.
 * @returns false and fills errorMessage when the directory cannot be written.
 */
bool writeParameterArtifacts (const juce::File& directory, juce::String& errorMessage);

} // namespace NEURONiK::State
