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

    // El id de la ranura `slot` (1..4), como lo escribe el layout.
    const auto idOf = [] (int slot, const char* suffix)
    {
        return juce::String ("mod") + juce::String (slot) + suffix;
    };

    // [&] y no [&state]: read usa idOf, que tambien es local de esta funcion.
    const auto read = [&] (int slot, const char* suffix) -> int
    {
        return (int) (double) state.getChildWithProperty (idProperty, idOf (slot, suffix))
                            .getProperty ("value", 0);
    };

    for (const auto* route : { &env1Route, &env2Route })
    {
        const int wantedSource      = (int) (*route)[0];
        const int wantedDestination = (int) (*route)[1];

        // ¿ESTA RUTA YA ESTA? Sin esta pregunta la migracion no es idempotente
        // y anade una copia de ENV 1 en cuanto encuentra dos huecos: un
        // preset recien creado trae mod1=ENV 1 y mod2=ENV 2, y las ranuras 3 y 4
        // vacias, así que abrirlo y migrarlo metia una segunda ENV 1 -> Osc Level
        // sin tocar la original. Ese caso lo tapaba el limite `slot <= 2` de
        // abajo: nunca se llegaba a mirar la 3. PresetRoundTripTest lo llevaba
        // escrito desde el principio ("a preset that already routes ENVs is left
        // alone") y pasaba por el defecto, no por el acierto.
        bool alreadyThere = false;
        for (int slot = 1; slot <= 4 && ! alreadyThere; ++slot)
            alreadyThere = read (slot, "Source") == wantedSource
                        && read (slot, "Destination") == wantedDestination;

        if (alreadyThere)
            continue;

        // LAS CUATRO ranuras, no solo las dos primeras. El limite era
        // `slot <= 2` y su efecto era que un preset con una ruta del usuario en
        // la ranura 1 se migraba a medias: la ENV 1 entraba en la 2 y la ENV 2
        // se perdia, con las ranuras 3 y 4 libremente vacias al lado. El sonido
        // no cambiaba (sin ruta el factor de routing se queda en el sentinela
        // 1.0, que es lo mismo que escribe amount 1.0), pero el preset se
        // abria con media matriz migrada: una envolvente visible en la matriz y
        // la otra no, sin motivo. El commit que la introdujo decia "en la
        // primera ranura libre", y en un preset de cuatro la primera ranura
        // libre es cualquiera de las cuatro.
        // Sin tope: el `inserted < 2` que estaba aqui era codigo muerto.
        for (int slot = 1; slot <= 4; ++slot)
        {
            if (read (slot, "Source") != 0 || read (slot, "Destination") != 0)
                continue;   // la ranura ya la ocupa el usuario

            auto amount = state.getChildWithProperty (idProperty, idOf (slot, "Amount"));
            if (amount.isValid())
                amount.setProperty ("value", (*route)[2], nullptr);

            state.getChildWithProperty (idProperty, idOf (slot, "Source"))
                .setProperty ("value", (*route)[0], nullptr);
            state.getChildWithProperty (idProperty, idOf (slot, "Destination"))
                .setProperty ("value", (*route)[1], nullptr);
            ++inserted;
            break;   // esta ruta ya tiene asiento; la siguiente empieza por la 1
        }
    }

    return inserted;
}

} // namespace NEURONiK::Serialization
