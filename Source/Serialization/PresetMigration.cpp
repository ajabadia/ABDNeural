/*
  ==============================================================================

    PresetMigration.cpp

  ==============================================================================
*/

#include "PresetMigration.h"

namespace NEURONiK::Serialization
{
namespace
{
    /** The child type and property names used by AudioProcessorValueTreeState. */
    const juce::Identifier paramType { "PARAM" };
    const juce::Identifier idProperty { "id" };

    /** Level-1 metadata written next to the parameters by PresetManager. */
    const juce::Identifier metadataType { "METADATA" };
}

juce::StringArray currentParameterIds (const juce::AudioProcessor& processor)
{
    juce::StringArray ids;

    for (const auto* parameter : processor.getParameters())
        if (const auto* withId = dynamic_cast<const juce::AudioProcessorParameterWithID*> (parameter))
            ids.add (withId->paramID);

    return ids;
}

int migratePresetState (juce::ValueTree& state, const juce::AudioProcessor& processor)
{
    if (! state.isValid())
        return 0;

    const auto knownIds = currentParameterIds (processor);
    int removed = 0;

    // Backwards so removing a child never invalidates an index still to be visited.
    for (int index = state.getNumChildren(); --index >= 0;)
    {
        const auto child = state.getChild (index);

        if (child.hasType (metadataType))
        {
            state.removeChild (index, nullptr);
            ++removed;
            continue;
        }

        if (child.hasType (paramType)
            && ! knownIds.contains (child.getProperty (idProperty).toString()))
        {
            state.removeChild (index, nullptr);
            ++removed;
        }
    }

    return removed;
}

} // namespace NEURONiK::Serialization
