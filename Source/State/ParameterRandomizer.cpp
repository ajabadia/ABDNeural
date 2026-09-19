#include "ParameterRandomizer.h"

#include "ParameterDefinitions.h"

#include <cmath>

namespace NEURONiK::State
{

namespace
{
    /** @brief Diferencia por debajo de la cual el sorteo no se considera un cambio. */
    constexpr float changeEpsilon = 1.0e-6f;
}

const std::vector<RandomizeTarget>& getRandomizeTargets()
{
    // La intencion musical es la que tenia `ParameterPanel::randomizeParameters()`,
    // transcrita tal cual (mismos extremos) para que el RANDOMIZE suene igual que
    // antes de mudarse aqui. Lo que cambia es que ahora el test comprueba que cada
    // extremo cabe en el rango real del parametro y que ningun id esta de sobra.
    static const std::vector<RandomizeTarget> targets
    {
        // --- banco espectral, morph y unísono (freezeResonator) ---------------
        { IDs::morphX,            0.00f, 1.00f, FreezeGroup::resonator },
        { IDs::morphY,            0.00f, 1.00f, FreezeGroup::resonator },
        { IDs::oscInharmonicity,  0.00f, 0.40f, FreezeGroup::resonator },
        { IDs::oscRoughness,      0.00f, 0.50f, FreezeGroup::resonator },
        { IDs::resonatorParity,   0.20f, 0.80f, FreezeGroup::resonator },
        { IDs::resonatorShift,    0.80f, 1.30f, FreezeGroup::resonator },
        { IDs::resonatorRolloff,  0.50f, 2.80f, FreezeGroup::resonator },
        { IDs::oscExciteNoise,    0.00f, 0.80f, FreezeGroup::resonator },
        { IDs::excitationColor,   0.20f, 0.70f, FreezeGroup::resonator },
        { IDs::impulseMix,        0.00f, 1.00f, FreezeGroup::resonator },
        // 0.3 en la tabla original: el parametro empieza en 0.5, asi que ese
        // extremo nunca fue alcanzable (el sorteo caia fuera del rango y el
        // `setValueNotifyingHost` lo recortaba). La tabla dice ahora lo que el
        // RANDOMIZE podia hacer de verdad, que es lo que el test exige.
        { IDs::resonatorRes,      0.50f, 0.95f, FreezeGroup::resonator },
        { IDs::unisonDetune,      0.00f, 0.05f, FreezeGroup::resonator },
        { IDs::unisonSpread,      0.20f, 0.80f, FreezeGroup::resonator },

        // --- filtro (freezeFilter) -------------------------------------------
        { IDs::filterCutoff,    200.00f, 8000.0f, FreezeGroup::filter },
        { IDs::filterRes,         0.00f,    0.60f, FreezeGroup::filter },

        // --- FX: congelado de filtro a proposito (no tienen boton propio) -----
        { IDs::fxSaturation,      0.00f, 0.40f, FreezeGroup::filter },
        { IDs::fxChorusMix,       0.00f, 0.50f, FreezeGroup::filter },
        { IDs::fxReverbMix,       0.00f, 0.40f, FreezeGroup::filter },

        // --- envolvente de amplitud y nivel (freezeEnvelopes) ---------------
        { IDs::envAttack,         0.001f, 0.50f, FreezeGroup::envelopes },
        { IDs::envDecay,          0.10f,  1.00f, FreezeGroup::envelopes },
        { IDs::envSustain,        0.20f,  0.80f, FreezeGroup::envelopes },
        { IDs::envRelease,        0.10f,  2.00f, FreezeGroup::envelopes },
        { IDs::oscLevel,          0.30f,  0.80f, FreezeGroup::envelopes },
    };

    return targets;
}

FreezeFlags readFreezeFlags (const juce::AudioProcessorValueTreeState& apvts)
{
    FreezeFlags flags;

    const auto read = [&apvts] (const char* id) -> bool
    {
        if (const auto* value = apvts.getRawParameterValue (id))
            return value->load() > 0.5f;

        return false;   // sin el parametro no se congela nada: mejor sortear que no hacer nada
    };

    flags.resonator = read (IDs::freezeResonator);
    flags.filter    = read (IDs::freezeFilter);
    flags.envelopes = read (IDs::freezeEnvelopes);

    return flags;
}

float readRandomizeStrength (const juce::AudioProcessorValueTreeState& apvts)
{
    if (const auto* value = apvts.getRawParameterValue (IDs::randomStrength))
        return juce::jlimit (0.0f, 1.0f, value->load());

    return 0.0f;   // sin parametro de fuerza el RANDOMIZE no toca nada
}

int applyRandomize (juce::AudioProcessorValueTreeState& apvts, float strength, juce::Random& random)
{
    const auto flags = readFreezeFlags (apvts);
    const auto amount = juce::jlimit (0.0f, 1.0f, strength);

    const auto isFrozen = [&flags] (FreezeGroup group)
    {
        switch (group)
        {
            case FreezeGroup::resonator: return flags.resonator;
            case FreezeGroup::filter:    return flags.filter;
            case FreezeGroup::envelopes: return flags.envelopes;
            case FreezeGroup::none:
            default:                     return false;
        }
    };

    int moved = 0;

    for (const auto& target : getRandomizeTargets())
    {
        if (isFrozen (target.freeze))
            continue;

        auto* parameter = apvts.getParameter (target.id);

        if (parameter == nullptr)
            continue;   // la tabla y el layout los cruza ParameterRandomizerTest

        const auto& range = parameter->getNormalisableRange();

        // La intencion se RECORTA al rango del parametro: un rango del contrato
        // que cambie no puede sacar el RANDOMIZE de su propio parametro.
        const auto low  = juce::jlimit (range.start, range.end, target.minValue);
        const auto high = juce::jlimit (range.start, range.end, target.maxValue);

        if (high <= low)
            continue;

        const auto currentReal = range.convertFrom0to1 (parameter->getValue());
        const auto desiredReal = range.snapToLegalValue (low + random.nextFloat() * (high - low));

        // ESTA es la linea que estaba mal: la mezcla va en unidades REALES (a
        // mitad de fuerza el parametro queda a mitad de camino entre lo que sonaba
        // y lo que ha salido en el sorteo) y se convierte a normalizado UNA vez.
        const auto nextReal = amount >= 1.0f ? desiredReal
                                             : juce::jmap (amount, currentReal, desiredReal);

        // Redondeo del propio parametro (intervalos discretos incluidos) antes de
        // comparar, para que un "cambio" que el parametro va a deshacer no cuente.
        const auto settledReal = range.snapToLegalValue (nextReal);
        const auto next = juce::jlimit (0.0f, 1.0f, range.convertTo0to1 (settledReal));

        if (std::abs (next - parameter->getValue()) <= changeEpsilon)
            continue;   // a fuerza 0 no se mueve nada, y no se notifica

        parameter->setValueNotifyingHost (next);
        ++moved;
    }

    return moved;
}

} // namespace NEURONiK::State
