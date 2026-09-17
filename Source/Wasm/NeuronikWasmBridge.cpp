/*
  ==============================================================================

    NeuronikWasmBridge.cpp
    Created: 17 Sep 2026
    Description: extern "C" bridge exposing the synthesis engine to JS
                 (AudioWorklet) through the Phase-1 boundary
                 (Runtime::DspEngineFacade + Runtime::Event + GlobalParams).

    ABI conventions follow the ABDMS2000 pattern (flat C exports,
    EMSCRIPTEN_KEEPALIVE, JS owns the heap buffers via _malloc), but the
    engine behind it is the REAL DSP — no port, so native and WASM render
    the same deterministic output.

    Event layout: Runtime::Event is standard-layout, 24 bytes:
      [0]i32 type  [1]i32 channel  [2]i32 note  [3]i32 value14
      [4]f32 value [5]i32 sampleOffset
    GlobalParams: JS never hardcodes offsets — call globalParamsLayout()
    once and write fields by the returned offsets.

  ==============================================================================
*/

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WASM_EXPORT
#endif

#include "../DSP/Runtime/DspEngineFacade.h"
#include "../DSP/Runtime/DspEvent.h"
#include "../DSP/DspTypes.h"
#include "../DSP/CoreModules/NeuronikEngine.h"
#include "../DSP/CoreModules/NeurotikEngine.h"

#include <cstddef>
#include <cstring>
#include <memory>

namespace
{
    // Static asserts: the JS side depends on these exact sizes.
    static_assert(std::is_standard_layout<NEURONiK::DSP::Runtime::Event>::value,
                  "Runtime::Event must cross the WASM boundary as raw bytes");
    static_assert(sizeof(NEURONiK::DSP::Runtime::Event) == 24,
                  "Unexpected Runtime::Event layout (JS ABI is 24 bytes)");

    struct EngineInstance
    {
        std::unique_ptr<NEURONiK::DSP::ISynthesisEngine> engine;
        std::unique_ptr<NEURONiK::DSP::Runtime::DspEngineFacade> facade;
        double sampleRate = 48000.0;
        int blockSize = 128;
    };

    EngineInstance& instance()
    {
        static EngineInstance inst;
        return inst;
    }

    void rebuildEngine (int engineType)
    {
        auto& inst = instance();
        if (engineType == 1)
            inst.engine = std::make_unique<NEURONiK::DSP::NeurotikEngine>();
        else
            inst.engine = std::make_unique<NEURONiK::DSP::NeuronikEngine>();

        inst.facade = std::make_unique<NEURONiK::DSP::Runtime::DspEngineFacade> (*inst.engine);
        inst.facade->prepare (inst.sampleRate, inst.blockSize);
    }
}

extern "C" {

WASM_EXPORT void neuronikInit (double sampleRate, int blockSize)
{
    auto& inst = instance();
    inst.sampleRate = sampleRate > 1000.0 ? sampleRate : 48000.0;
    inst.blockSize  = blockSize > 0 ? blockSize : 128;

    if (inst.engine == nullptr)
        rebuildEngine (0);
    else
        inst.facade->prepare (inst.sampleRate, inst.blockSize);
}

WASM_EXPORT void neuronikSetEngine (int engineType)
{
    rebuildEngine (engineType == 1 ? 1 : 0);
}

/**
 * Renders one block. outL/outR are Float32Array-backed heap pointers owned by
 * JS (allocate once via _malloc, reuse every block). events is a pointer to
 * numEvents consecutive 24-byte Runtime::Event records (may be null).
 */
WASM_EXPORT void neuronikProcess (float* outL, float* outR, int numSamples,
                                  const NEURONiK::DSP::Runtime::Event* events,
                                  int numEvents)
{
    auto& inst = instance();
    if (inst.facade == nullptr)
        return;

    inst.facade->process (outL, outR, numSamples, events, numEvents);
}

WASM_EXPORT void neuronikSetGlobalParams (const void* pod, int byteSize)
{
    if (pod == nullptr || byteSize != (int) sizeof (NEURONiK::DSP::GlobalParams))
        return;

    NEURONiK::DSP::GlobalParams params;
    std::memcpy (&params, pod, sizeof (params));
    instance().facade->setGlobalParams (params);
}

/** Byte size JS must allocate for the GlobalParams mirror. */
WASM_EXPORT int neuronikGlobalParamsSize()
{
    return (int) sizeof (NEURONiK::DSP::GlobalParams);
}

/**
 * Layout descriptor so JS writes the GlobalParams mirror without hardcoded
 * offsets: outOffsets points at an i32 array of numFields entries where the
 * bridge stores offsetof() of every field, in the documented field order
 * (masterLevel, saturationAmt, bpm, delayTime, delayFB, chorusRate,
 * chorusDepth, chorusMix, reverbSize, reverbDamping, reverbWidth, reverbMix,
 * lfo1.waveform, lfo1.rateHz, lfo1.syncMode, lfo1.rhythmicDivision,
 * lfo1.depth, lfo2.<same five>, modMatrix[r].{source,destination,amount} r=0..3).
 * Returns the number of fields written.
 */
WASM_EXPORT int neuronikGlobalParamsLayout (int* outOffsets, int maxFields)
{
    using GP = NEURONiK::DSP::GlobalParams;

    const std::size_t offsets[] = {
        offsetof (GP, masterLevel), offsetof (GP, saturationAmt), offsetof (GP, bpm),
        offsetof (GP, delayTime),   offsetof (GP, delayFB),
        offsetof (GP, chorusRate),  offsetof (GP, chorusDepth),   offsetof (GP, chorusMix),
        offsetof (GP, reverbSize),  offsetof (GP, reverbDamping),
        offsetof (GP, reverbWidth), offsetof (GP, reverbMix),
        offsetof (GP, lfo1.waveform), offsetof (GP, lfo1.rateHz),
        offsetof (GP, lfo1.syncMode), offsetof (GP, lfo1.rhythmicDivision),
        offsetof (GP, lfo1.depth),
        offsetof (GP, lfo2.waveform), offsetof (GP, lfo2.rateHz),
        offsetof (GP, lfo2.syncMode), offsetof (GP, lfo2.rhythmicDivision),
        offsetof (GP, lfo2.depth),
    };

    const int count = (int) (sizeof (offsets) / sizeof (offsets[0]));
    if (outOffsets != nullptr)
    {
        const int n = count < maxFields ? count : maxFields;
        for (int i = 0; i < n; ++i)
            outOffsets[i] = (int) offsets[i];
        return n;
    }
    return count;
}

/** modMatrix field offsets appended after the base layout (returns count). */
WASM_EXPORT int neuronikModMatrixLayout (int* outOffsets, int maxFields)
{
    using GP = NEURONiK::DSP::GlobalParams;

    const std::size_t offsets[] = {
        offsetof (GP, modMatrix[0].source),     offsetof (GP, modMatrix[0].destination), offsetof (GP, modMatrix[0].amount),
        offsetof (GP, modMatrix[1].source),     offsetof (GP, modMatrix[1].destination), offsetof (GP, modMatrix[1].amount),
        offsetof (GP, modMatrix[2].source),     offsetof (GP, modMatrix[2].destination), offsetof (GP, modMatrix[2].amount),
        offsetof (GP, modMatrix[3].source),     offsetof (GP, modMatrix[3].destination), offsetof (GP, modMatrix[3].amount),
    };

    const int count = (int) (sizeof (offsets) / sizeof (offsets[0]));
    if (outOffsets != nullptr)
    {
        const int n = count < maxFields ? count : maxFields;
        for (int i = 0; i < n; ++i)
            outOffsets[i] = (int) offsets[i];
        return n;
    }
    return count;
}

WASM_EXPORT void neuronikAllNotesOff()
{
    auto& inst = instance();
    if (inst.facade != nullptr)
        inst.facade->allNotesOff();
}

WASM_EXPORT int neuronikNumActiveVoices()
{
    auto& inst = instance();
    return inst.facade != nullptr ? inst.facade->getNumActiveVoices() : 0;
}

/** Visualization feed for the UI (atomic reads inside the engine). */
WASM_EXPORT float neuronikGetLfo (int index)
{
    auto& inst = instance();
    return inst.facade != nullptr ? inst.engine->getLfoValue (index) : 0.0f;
}

} // extern "C"
