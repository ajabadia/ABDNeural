#pragma once

// Central definition of Modulation Targets aligned with ParameterDefinitions.h
// IDs must match index in ParameterDefinitions::getModDestinations()

namespace NEURONiK {

enum class ModulationTarget
{
    Off = 0,
    OscLevel = 1,
    Inharmonicity = 2,
    Roughness = 3,
    MorphX = 4,
    MorphY = 5,
    AmpAttack = 6,
    AmpDecay = 7,
    AmpSustain = 8,
    AmpRelease = 9,
    FilterCutoff = 10,
    FilterRes = 11,
    FilterEnvAmount = 12,
    FilterAttack = 13,
    FilterDecay = 14,
    FilterSustain = 15,
    FilterRelease = 16,
    FxSaturation = 17,
    FxDelayTime = 18,
    FxDelayFeedback = 19,
    ResonatorParity = 20, // Odd/Even Bal
    ResonatorShift = 21,  // Spectral Shift
    ResonatorRolloff = 22, // Harm Roll-off
    ExciteNoise = 23,
    ExciteColor = 24,
    ImpulseMix = 25,
    ResonatorResonance = 26,
    UnisonDetune = 27,

    // Aliases for compatibility with existing UI code
    Level = OscLevel,
    MasterLevel = OscLevel,
    Parity = ResonatorParity,
    Shift = ResonatorShift,
    Unison = UnisonDetune, 
    UnisonSpread = UnisonDetune, // Mapped to Same slot logic
    
    Count = 64
};

inline constexpr size_t ModulationTargetCount = static_cast<size_t>(ModulationTarget::Count);

} // namespace NEURONiK
