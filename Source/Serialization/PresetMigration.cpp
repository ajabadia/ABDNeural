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

    // Fuentes ENV de la matriz (se anadieron al final: 6 = ENV 1, 7 = ENV 2).
    constexpr int env1Source = 6;
    constexpr int env2Source = 7;
    constexpr int oscLevelDestination = 1;
    constexpr int filterCutoffDestination = 10;
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

int insertEnvModRoutes (juce::ValueTree& state)
{
    if (! state.isValid())
        return 0;

    // Los choices se serializan como indice (APVTS guarda el valor
    // desnormalizado; para un AudioParameterChoice es 0..n-1).
    const float env1Route[3] = { (float) env1Source, (float) oscLevelDestination, 1.0f };
    const float env2Route[3] = { (float) env2Source, (float) filterCutoffDestination, 1.0f };
    int inserted = 0;

    for (const auto* route : { &env1Route, &env2Route })
    {
        for (int slot = 1; slot <= 2 && inserted < 2; ++slot)
        {
            auto src = state.getChildWithProperty (idProperty,
                juce::String ("mod") + juce::String (slot) + "Source");
            auto dst = state.getChildWithProperty (idProperty,
                juce::String ("mod") + juce::String (slot) + "Destination");

            if (! src.isValid() || ! dst.isValid())
                continue;

            const bool empty = ((int) src.getProperty ("value", 0) == 0)
                            && ((int) dst.getProperty ("value", 0) == 0);
            if (! empty)
                continue;

            auto amount = state.getChildWithProperty (idProperty,
                juce::String ("mod") + juce::String (slot) + "Amount");
            if (amount.isValid())
                amount.setProperty ("value", (*route)[2], nullptr);

            src.setProperty ("value", (*route)[0], nullptr);
            dst.setProperty ("value", (*route)[1], nullptr);
            ++inserted;
            break; // esta ruta ya tiene asiento; la siguiente busca en el slot 1
        }
    }

    return inserted;
}

} // namespace NEURONiK::Serialization
