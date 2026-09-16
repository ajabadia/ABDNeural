/*
  ==============================================================================

    PresetMigration.h
    Created: 16 Sep 2026
    Description: Keeps presets loadable across parameter retirements.

                 Presets are stored as a dump of the APVTS state, so a parameter
                 removed from the layout simply becomes an inert child in old
                 files. Loading already tolerates that, but re-saving writes the
                 child back, so the dead ID would travel forward forever. This
                 migration drops what the plugin no longer knows about.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace NEURONiK::Serialization
{

/**
 * @brief Removes children of a preset state tree that the plugin no longer defines.
 * @details Only `<PARAM id="...">` children are inspected; a child is removed when
 *          its `id` is not one of the processor's current parameters. The metadata
 *          child written by PresetManager is also dropped, because it belongs to
 *          the preset file and not to the plugin state: leaving it in would make a
 *          re-save write a second copy of it.
 * @returns the number of children removed.
 */
int migratePresetState (juce::ValueTree& state, const juce::AudioProcessor& processor);

/** @brief Ids the given processor currently exposes, for diagnostics and tests. */
juce::StringArray currentParameterIds (const juce::AudioProcessor& processor);

} // namespace NEURONiK::Serialization
