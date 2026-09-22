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

/**
 * @brief Inserts the ENV 1/ENV 2 modulation routes a pre-ENV preset lacks.
 * @details Before envelopes became modulation sources, the VCA envelope and the
 *          filter envelope were hard-wired: the DSP treats "no ENV route" as
 *          routing depth 1.0 (sentinel in AdditiveVoice/NeurotikVoice), so the
 *          two routes this inserts (ENV 1 -> Osc Level, ENV 2 -> Filter Cutoff,
 *          both amount 1.0) only make the existing wiring VISIBLE and editable
 *          in the matrix. An empty route (source 0 and destination 0) is fair
 *          game: slot 1 is preferred, then 2. Without a free slot nothing is
 *          inserted and the preset keeps sounding identical (sentinel 1.0).
 * @returns the number of routes inserted (0..2).
 */
int insertEnvModRoutes (juce::ValueTree& state);

} // namespace NEURONiK::Serialization
