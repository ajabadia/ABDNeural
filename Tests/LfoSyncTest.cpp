/*
  ==============================================================================

    LfoSyncTest.cpp
    Created: 16 Sep 2026
    Description: Pins the tempo sync contract that the WebUI and the delay rely
                 on:

                   1. the shared division table matches the choice parameters;
                   2. the LFO really changes period with tempo and division;
                   3. Free mode still obeys rateHz, including after a sync mode
                      change, and the upper tempo limit covers the parameter
                      range (20-400 BPM).

  ==============================================================================
*/

#include "../Source/DSP/CoreModules/LFO.h"
#include "../Source/DSP/CoreModules/RhythmicDivision.h"

// Los headers del DSP ya no arrastran juce_core (Fase 1, paso 6/6): este test usa
// juce::String en sus ayudantes, asi que lo incluye el mismo.
#include <juce_core/juce_core.h>

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
    constexpr double sampleRate = 48000.0;

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

    void checkNear (double actual, double expected, double tolerance, const juce::String& description)
    {
        const double difference = std::abs (actual - expected);

        check (difference <= tolerance,
               description + " (expected " + juce::String (expected, 1) + ", got "
                   + juce::String (actual, 1) + ")");
    }

    /** @brief Average distance, in samples, between rising zero crossings. */
    double measurePeriod (NEURONiK::DSP::Core::LFO& lfo, int numSamples)
    {
        std::vector<int> crossings;
        float previous = 0.0f;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float current = lfo.processSample();

            if (previous <= 0.0f && current > 0.0f)
                crossings.push_back (sample);

            previous = current;
        }

        // The first interval can be partial: either the LFO starts at phase 0, or a
        // preceding mode change left it mid cycle. Discard it and require two full
        // intervals afterwards, so the average is a stable period.
        if (crossings.size() < 4)
            return 0.0;

        double total = 0.0;
        int intervals = 0;

        for (size_t i = 1; i + 1 < crossings.size(); ++i)
        {
            total += crossings[i + 1] - crossings[i];
            ++intervals;
        }

        return intervals > 0 ? total / intervals : 0.0;
    }

    void configure (NEURONiK::DSP::Core::LFO& lfo)
    {
        lfo.setSampleRate (sampleRate);
        lfo.setWaveform (NEURONiK::DSP::Core::LFO::Waveform::Sine);
        lfo.setDepth (1.0f);
        lfo.reset();
    }

    constexpr int oneSecond = static_cast<int> (sampleRate);

    /** Window long enough for at least three cycles of the slowest case tested. */
    constexpr int slowWindow = 4 * oneSecond;

    // Note lengths at the tempos below, in samples:
    //   1/4 @ 120 BPM = 24000    1/8 @ 120 BPM = 12000    1/4t @ 120 BPM = 16000
    //   1/4 @ 400 BPM =  7200    1/1 @ 400 BPM = 28800
    constexpr double quarterAt120 = 24000.0;
    constexpr double eighthAt120 = 12000.0;
    constexpr double quarterTripletAt120 = 16000.0;
    constexpr double quarterAt400 = 7200.0;
    constexpr double wholeAt400 = 28800.0;
}

int main()
{
    using namespace NEURONiK::DSP::Core;

    std::cout << "NEURONiK tempo sync contract\n";

    // --- 1. Shared division table ------------------------------------------------
    std::cout << "\nDivision table\n";

    check (numRhythmicDivisions == 9, "the table has one entry per choice");
    check (quarterNotesForDivision (0) == 4.0f, "1/1 is a whole note (4 quarters)");
    check (quarterNotesForDivision (2) == 1.0f, "1/4 is one quarter note");
    check (quarterNotesForDivision (5) == 0.125f, "1/32 is an eighth of a quarter");
    checkNear (quarterNotesForDivision (6), 2.0 / 3.0, 1.0e-6, "1/4t is two thirds of a quarter");
    checkNear (quarterNotesForDivision (8), 1.0 / 6.0, 1.0e-6, "1/16t is a sixth of a quarter");
    check (quarterNotesForDivision (-1) == 1.0f, "a negative index falls back to 1/4");
    check (quarterNotesForDivision (99) == 1.0f, "an out-of-range index falls back to 1/4");

    // The table is float, so derived seconds are compared with float tolerance.
    checkNear (secondsForDivision (2, 120.0), 0.5, 1.0e-6, "1/4 at 120 BPM is half a second");
    checkNear (secondsForDivision (3, 120.0), 0.25, 1.0e-6, "1/8 at 120 BPM is a quarter second");
    checkNear (secondsForDivision (6, 120.0), 1.0 / 3.0, 1.0e-6,
               "1/4t at 120 BPM lasts two thirds of a beat");
    checkNear (secondsForDivision (2, 0.0), 0.5, 1.0e-6, "an invalid tempo falls back to 120 BPM");

    // --- 2. Tempo sync -----------------------------------------------------------
    std::cout << "\nTempo-synced LFO\n";

    LFO lfo;
    configure (lfo);
    lfo.setSyncMode (LFO::SyncMode::TempoSync);
    lfo.setTempoBPM (120.0);
    lfo.setRhythmicDivision (quarterNotesForDivision (2));
    checkNear (measurePeriod (lfo, 3 * oneSecond), quarterAt120, 200.0, "1/4 at 120 BPM is 0.5 s");

    configure (lfo);
    lfo.setSyncMode (LFO::SyncMode::TempoSync);
    lfo.setTempoBPM (120.0);
    lfo.setRhythmicDivision (quarterNotesForDivision (3));
    checkNear (measurePeriod (lfo, 3 * oneSecond), eighthAt120, 200.0,
               "1/8 at 120 BPM is half a beat, not two beats");

    configure (lfo);
    lfo.setSyncMode (LFO::SyncMode::TempoSync);
    lfo.setTempoBPM (120.0);
    lfo.setRhythmicDivision (quarterNotesForDivision (6));
    checkNear (measurePeriod (lfo, 3 * oneSecond), quarterTripletAt120, 200.0,
               "1/4t at 120 BPM is two thirds of a beat");

    configure (lfo);
    lfo.setSyncMode (LFO::SyncMode::TempoSync);
    lfo.setTempoBPM (400.0);
    lfo.setRhythmicDivision (quarterNotesForDivision (2));
    checkNear (measurePeriod (lfo, oneSecond), quarterAt400, 100.0,
               "400 BPM is honoured, not clamped to 300");

    // --- 3. Free mode stays in charge -------------------------------------------
    std::cout << "\nFree mode\n";

    configure (lfo);
    lfo.setRate (1.0f);
    lfo.setSyncMode (LFO::SyncMode::Free);
    checkNear (measurePeriod (lfo, slowWindow), 48000.0, 200.0, "free mode obeys rateHz");

    configure (lfo);
    lfo.setRate (1.0f);
    lfo.setSyncMode (LFO::SyncMode::TempoSync);
    lfo.setTempoBPM (400.0);
    lfo.setRhythmicDivision (quarterNotesForDivision (0));
    checkNear (measurePeriod (lfo, 3 * oneSecond), wholeAt400, 400.0,
               "a whole note at 400 BPM lasts 0.6 s");

    lfo.setSyncMode (LFO::SyncMode::Free);
    checkNear (measurePeriod (lfo, slowWindow), 48000.0, 200.0,
               "returning to Free restores rateHz regardless of tempo");

    // --- 4. Depth must not affect timing ----------------------------------------
    std::cout << "\nDepth independence\n";

    configure (lfo);
    lfo.setSyncMode (LFO::SyncMode::TempoSync);
    lfo.setTempoBPM (120.0);
    lfo.setRhythmicDivision (quarterNotesForDivision (2));
    lfo.setDepth (0.25f);
    checkNear (measurePeriod (lfo, 3 * oneSecond), quarterAt120, 200.0,
               "depth changes amplitude, not period");

    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.") << '\n';
    return failures == 0 ? 0 : 1;
}
