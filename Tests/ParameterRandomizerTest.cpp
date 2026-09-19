/*
  ==============================================================================

    ParameterRandomizerTest.cpp
    Created: 19 Sep 2026
    Description: Regression suite for the RANDOMIZE now that it lives in
                 `State/ParameterRandomizer` instead of inside the native panel.

                 What it pins, in order of importance:

                 1. THE BUG THIS MODULE FIXES. `ParameterPanel::randomizeParameters()`
                    converted the current value to REAL units, the drawn value to
                    NORMALISED and averaged the two with `jmap`. For any parameter
                    whose real range is not 0..1 (cutoff in Hz, times, BPM...) the
                    result fell outside 0..1 and `setValueNotifyingHost` slammed it
                    to the parameter's maximum. Here the average is checked to be
                    linear in REAL units, and the strength-1 draw has to land
                    inside the intent window (200..8000 Hz for the cutoff, well
                    below its 20..20000 real range).
                 2. The table cannot drift from the layout: every id exists, no id
                    repeats, every id is routed (not `notRouted`) and every intent
                    window fits INSIDE the parameter's own range, so the randomiser
                    can never ask for a value the parameter would reject.
                 3. The freeze flags protect what they promise, and strength 0
                    changes nothing (not even a notification).

                 Deterministic by construction: the generator is injected, so
                 every expectation is exact (same seed, same result).

  ==============================================================================
*/

#include "../Source/State/ParameterDescriptors.h"
#include "../Source/State/ParameterRandomizer.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& description)
    {
        if (condition)
        {
            std::cout << "  [ok]   " << description << '\n';
            return;
        }

        std::cout << "  [FAIL] " << description << '\n';
        ++failures;
    }

    using NEURONiK::State::FreezeGroup;
    using NEURONiK::State::RandomizeTarget;

    juce::RangedAudioParameter* parameterFor (juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        return apvts.getParameter (id);
    }

    /** @brief Real (denormalised) value of a parameter, or NaN when unknown. */
    float realValue (juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        if (auto* parameter = parameterFor (apvts, id))
            return parameter->getNormalisableRange().convertFrom0to1 (parameter->getValue());

        return std::nanf ("");
    }

    /** @brief Write a REAL value into a parameter, the way the APVTS expects. */
    void setRealValue (juce::AudioProcessorValueTreeState& apvts, const char* id, float real)
    {
        if (auto* parameter = parameterFor (apvts, id))
            parameter->setValueNotifyingHost (parameter->getNormalisableRange().convertTo0to1 (real));
    }

    /** @brief Every normalised value in layout order, for "nothing moved" checks. */
    std::vector<float> snapshot (juce::AudioProcessorValueTreeState& apvts)
    {
        std::vector<float> values;

        for (auto* parameter : apvts.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                values.push_back (ranged->getValue());

        return values;
    }

    void testTableMatchesLayout (juce::AudioProcessorValueTreeState& apvts)
    {
        std::cout << "\n[tabla] la intencion y el layout no se separan\n";

        juce::StringArray seen;
        bool allExist = true;
        bool allInsideOwnRange = true;
        bool allOrdered = true;
        bool allRouted = true;

        for (const auto& target : NEURONiK::State::getRandomizeTargets())
        {
            if (seen.contains (target.id))
                check (false, juce::String ("id repetido en la tabla: ") + target.id);

            seen.add (target.id);

            auto* parameter = parameterFor (apvts, target.id);

            if (parameter == nullptr)
            {
                allExist = false;
                std::cout << "         id que no esta en el layout: " << target.id << '\n';
                continue;
            }

            const auto& range = parameter->getNormalisableRange();

            if (target.minValue < range.start || target.maxValue > range.end)
            {
                allInsideOwnRange = false;
                std::cout << "         " << target.id << ": intencion [" << target.minValue << ", "
                          << target.maxValue << "] fuera del rango real [" << range.start << ", "
                          << range.end << "]\n";
            }

            if (target.maxValue <= target.minValue)
            {
                allOrdered = false;
                std::cout << "         " << target.id << ": intencion invertida o vacia\n";
            }

            if (NEURONiK::State::dspStatusFor (target.id) == NEURONiK::State::ParameterDspStatus::notRouted)
            {
                allRouted = false;
                std::cout << "         " << target.id << " no lo lee nadie (notRouted)\n";
            }
        }

        check (allExist, "todos los ids de la tabla existen en el layout");
        check (allInsideOwnRange, "todas las intenciones caben dentro del rango de su parametro");
        check (allOrdered, "ninguna intencion esta invertida");
        check (allRouted, "ningun parametro de la tabla es notRouted");
        check (seen.size() >= 20, "la tabla cubre el timbre (" + juce::String (seen.size()) + " parametros)");
    }

    void testStrengthZeroIsANoOp (juce::AudioProcessorValueTreeState& apvts)
    {
        std::cout << "\n[fuerza 0] no mueve nada, ni siquiera notifica\n";

        juce::Random random (11);
        const auto before = snapshot (apvts);
        const auto moved = NEURONiK::State::applyRandomize (apvts, 0.0f, random);
        const auto after = snapshot (apvts);

        check (moved == 0, "applyRandomize(0) informa de 0 cambios");
        check (before == after, "ningun valor del APVTS cambio");
    }

    void testStrengthOneLandsInsideTheIntent (juce::AudioProcessorValueTreeState& apvts)
    {
        std::cout << "\n[fuerza 1] cada sorteo cae dentro de su intencion\n";

        bool allInside = true;

        for (int seed = 1; seed <= 8; ++seed)
        {
            juce::Random random (seed);
            NEURONiK::State::applyRandomize (apvts, 1.0f, random);

            for (const auto& target : NEURONiK::State::getRandomizeTargets())
            {
                auto* parameter = parameterFor (apvts, target.id);

                if (parameter == nullptr)
                    continue;

                const auto& range = parameter->getNormalisableRange();
                const auto low  = juce::jlimit (range.start, range.end, target.minValue);
                const auto high = juce::jlimit (range.start, range.end, target.maxValue);
                const auto real = realValue (apvts, target.id);

                // Tolerancia: el intervalo del parametro (un param discreto redondea
                // a su rejilla, que puede quedar en el borde de la ventana).
                const auto tolerance = juce::jmax (1.0e-4f, range.interval);

                if (real < low - tolerance || real > high + tolerance)
                {
                    allInside = false;
                    std::cout << "         semilla " << seed << ": " << target.id << " = " << real
                              << " fuera de [" << low << ", " << high << "]\n";
                }
            }
        }

        check (allInside, "8 semillas x " + juce::String (NEURONiK::State::getRandomizeTargets().size())
                              + " parametros: todos dentro de su ventana");
    }

    /**
     * El bug del panel nativo, en una comprobacion exacta: la mezcla tiene que ser
     * lineal en unidades REALES. Se sortea el valor objetivo a fuerza 1 (misma
     * semilla), se vuelve a poner el parametro en su valor de partida y se repite a
     * fuerza 0.5: el resultado tiene que ser el punto medio de los DOS REALES.
     */
    void testBlendIsLinearInRealUnits (juce::AudioProcessorValueTreeState& apvts)
    {
        std::cout << "\n[mezcla] la fuerza promedia en unidades reales (el bug que se arregla)\n";

        const char* id = "filterCutoff";
        const float start = 1000.0f;   // dentro de 20..20000, lejos del maximo

        setRealValue (apvts, id, start);

        juce::Random drawSeed (7);
        NEURONiK::State::applyRandomize (apvts, 1.0f, drawSeed);
        const auto drawn = realValue (apvts, id);

        setRealValue (apvts, id, start);

        juce::Random sameSeed (7);
        NEURONiK::State::applyRandomize (apvts, 0.5f, sameSeed);
        const auto halfway = realValue (apvts, id);

        auto* parameter = parameterFor (apvts, id);
        const auto& range = parameter->getNormalisableRange();
        const auto expected = 0.5f * (start + drawn);
        const auto tolerance = juce::jmax (1.0f, range.interval);

        std::cout << "         partida " << start << " -> sorteo " << drawn
                  << " -> mitad " << halfway << " (esperado " << expected << ")\n";

        check (drawn >= 200.0f && drawn <= 8000.0f,
               "el sorteo a fuerza 1 cae en la ventana del cutoff (200..8000 Hz)");
        check (drawn < range.end,
               "el sorteo NO se clava en el maximo del parametro (" + juce::String (range.end) + " Hz)");
        check (std::abs (halfway - expected) <= tolerance,
               "la mitad es el punto medio en Hz (" + juce::String (expected) + ")");
    }

    void testFreezeFlagsProtectTheirGroup (juce::AudioProcessorValueTreeState& apvts)
    {
        std::cout << "\n[congelados] cada flag protege su grupo\n";

        const auto freeze = [&apvts] (const char* id, float value)
        {
            setRealValue (apvts, id, value);
        };

        freeze ("freezeResonator", 1.0f);
        freeze ("freezeFilter", 0.0f);
        freeze ("freezeEnvelopes", 1.0f);
        setRealValue (apvts, "morphX", 0.42f);
        setRealValue (apvts, "envAttack", 0.123f);

        const auto cutoffBefore = realValue (apvts, "filterCutoff");

        juce::Random random (3);
        NEURONiK::State::applyRandomize (apvts, 1.0f, random);

        check (std::abs (realValue (apvts, "morphX") - 0.42f) < 1.0e-5f,
               "freezeResonator deja morphX donde estaba");
        check (std::abs (realValue (apvts, "envAttack") - 0.123f) < 1.0e-5f,
               "freezeEnvelopes deja envAttack donde estaba");

        const auto cutoffAfter = realValue (apvts, "filterCutoff");

        check (std::abs (cutoffAfter - cutoffBefore) > 1.0e-3f,
               "el filtro (sin congelar) SI se mueve: " + juce::String (cutoffBefore) + " -> "
                   + juce::String (cutoffAfter));

        // Y con los tres puestos no se toca nada.
        freeze ("freezeResonator", 1.0f);
        freeze ("freezeFilter", 1.0f);
        freeze ("freezeEnvelopes", 1.0f);

        const auto before = snapshot (apvts);
        juce::Random another (5);
        const auto moved = NEURONiK::State::applyRandomize (apvts, 1.0f, another);

        check (moved == 0 && before == snapshot (apvts), "con los tres congelados no se mueve nada");
    }

    void testDeterministicForTheSameSeed (juce::AudioProcessorValueTreeState& apvts)
    {
        std::cout << "\n[determinismo] misma semilla, mismo resultado\n";

        juce::Random first (99);
        const auto movedFirst = NEURONiK::State::applyRandomize (apvts, 0.7f, first);
        const auto stateFirst = snapshot (apvts);

        juce::Random second (99);
        const auto movedSecond = NEURONiK::State::applyRandomize (apvts, 0.7f, second);

        check (movedFirst == movedSecond, "el numero de cambios coincide");
        check (stateFirst == snapshot (apvts), "el estado final coincide");
    }
}

int main()
{
    // The layout APVTS starts a timer while it exists, so it needs a message manager.
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    auto layout = NEURONiK::State::createLayoutApvts();
    auto& apvts = *layout.apvts;

    std::cout << "ParameterRandomizerTest\n";
    std::cout << "  parametros en el layout : " << apvts.processor.getParameters().size() << '\n';
    std::cout << "  parametros en la tabla  : "
              << NEURONiK::State::getRandomizeTargets().size() << '\n';

    testTableMatchesLayout (apvts);
    testStrengthZeroIsANoOp (apvts);
    testStrengthOneLandsInsideTheIntent (apvts);
    testBlendIsLinearInRealUnits (apvts);
    testFreezeFlagsProtectTheirGroup (apvts);
    testDeterministicForTheSameSeed (apvts);

    std::cout << '\n' << (failures == 0 ? "PASS" : "FAIL") << ": " << failures << " fallo(s)\n";

    return failures == 0 ? 0 : 1;
}
