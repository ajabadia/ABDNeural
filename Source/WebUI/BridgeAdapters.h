/**
 * @file BridgeAdapters.h
 * @brief The three adapters that plug the REAL processor into the ParameterBridge.
 *
 * Ticket 8.1: these lived inside `Source/WebPilotHost.cpp` (the pilot bench),
 * which is why only the bench host could talk to the page. The plugin editor has
 * to host the page as well, so they move here and BOTH surfaces use the same
 * three classes instead of one copy each.
 *
 * They are thin by design: the bridge owns the wire, the processor owns the
 * behaviour. Every one of them wraps `NEURONiKProcessor` and nothing else.
 *
 * Header-only on purpose: the classes carry no state beyond a reference to the
 * processor (which outlives the bridge in every host), so they need no .cpp and
 * no new entry in any target's source list.
 */

#pragma once

#include <array>
#include <cmath>

#include "Main/NEURONiKProcessor.h"
#include "Serialization/PresetManager.h"
#include "WebUI/ParameterBridge.h"

namespace NEURONiK::WebUI
{

/**
 * @brief Adapts the plugin's PresetManager to the bridge's PresetController
 *        interface. All PresetManager calls must run on the message thread,
 *        which is exactly where the bridge handles the preset messages.
 */
class PresetManagerAdapter final : public PresetController
{
public:
    explicit PresetManagerAdapter (NEURONiKProcessor& processorToWrap)
        : presetManager (processorToWrap.getPresetManager()) {}

    [[nodiscard]] juce::StringArray listPresets() const override
    {
        return presetManager.getAllPresets();
    }

    bool loadPreset (const juce::String& name) override
    {
        // loadPreset() silently no-ops on a missing file; check first so the
        // bridge can answer presetError instead of reporting success.
        const auto file = presetManager.getPresetsDirectory()
                              .getChildFile (name + NEURONiK::Serialization::PresetManager::presetExtension);

        if (! file.existsAsFile())
            return false;

        presetManager.loadPreset (name);
        return true;
    }

    bool savePreset (const juce::String& name) override
    {
        presetManager.savePreset (name);
        return presetManager.getPresetsDirectory()
                   .getChildFile (name + NEURONiK::Serialization::PresetManager::presetExtension)
                   .existsAsFile();
    }

    [[nodiscard]] juce::String getCurrentPreset() const override
    {
        return presetManager.getCurrentPreset();
    }

private:
    NEURONiK::Serialization::PresetManager& presetManager;
};

/**
 * @brief Adapts the plugin's public MIDI injection API to the bridge's
 *        MidiController interface. inject*() push into the processor's own
 *        lock-free FIFO — the SAME path the native editor keyboard uses —
 *        and processBlock drains that FIFO while a device renders audio.
 */
class MidiInjectionAdapter final : public MidiController
{
public:
    explicit MidiInjectionAdapter (NEURONiKProcessor& processorToWrap)
        : processor (processorToWrap) {}

    void noteOn (int note, float velocity) override
    {
        processor.injectNoteOn (midiChannel, note, velocity);
    }

    void noteOff (int note) override
    {
        processor.injectNoteOff (midiChannel, note, 0.0f);
    }

    void pitchBend (float normalized) override
    {
        processor.injectPitchBend (midiChannel,
                                   juce::jlimit (0, 16383,
                                                 (int) std::lround ((normalized + 1.0f) * 8192.0f)));
    }

    void modWheel (float normalized) override
    {
        processor.injectController (midiChannel, 1 /* CC1 */, (int) std::lround (normalized * 127.0f));
    }

    void allNotesOff() override
    {
        processor.requestAllNotesOff();
    }

private:
    static constexpr int midiChannel = 1;   // page keyboard plays on channel 1
    NEURONiKProcessor& processor;
};

/**
 * @brief Adapts the plugin's spectral model slots to the bridge's
 *        NativeModelController interface.
 *
 * A preset is APVTS state PLUS up to four SpectralModel slots the Resonator
 * morphs between; the models never live in the APVTS, so the page would
 * never learn about them. The adapter serves the file-backed mirror
 * (modelPath<slot>) — the same source an engine switch reloads from — so
 * the WASM path morphs between the SAME partials the plugin renders.
 */
class EngineModelsAdapter final : public NativeModelController
{
public:
    explicit EngineModelsAdapter (NEURONiKProcessor& processorToWrap)
        : processor (processorToWrap) {}

    int getNumModelSlots() const override { return 4; }

    void getCurrentModel (int slot,
                          std::array<float, 64>& amplitudes,
                          std::array<float, 64>& frequencyOffsets,
                          bool& isValid) const override
    {
        processor.getCurrentModel (slot, amplitudes, frequencyOffsets, isValid);
    }

private:
    NEURONiKProcessor& processor;
};

} // namespace NEURONiK::WebUI
