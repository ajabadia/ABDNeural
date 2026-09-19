/**
 * @file BridgeAdapters.h
 * @brief The three adapters that plug the REAL processor into the ParameterBridge.
 *
 * Ticket 8.1: these lived inside `Source/WebPilotHost.cpp` (la bancada WebView2),
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
#include <functional>
#include <memory>

// juce::FileChooser (model slots): the adapter owns the ONLY dialog in the whole
// bridge surface, and it is a GUI class, which is why this header is included by
// GUI hosts (the plugin editor and the pilot bench) and by nothing headless.
#include <juce_gui_basics/juce_gui_basics.h>

#include "Main/NEURONiKProcessor.h"
#include "Serialization/PresetManager.h"
#include "State/ParameterRandomizer.h"
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
 * @brief Adapta el RANDOMIZE del instrumento al RandomizeController del puente.
 *
 * La logica NO vive aqui: vive en `State/ParameterRandomizer`, que es donde puede
 * probarse sin procesador ni UI (fue un metodo privado del panel nativo hasta que
 * se retiro el panel, y ahi se quedo un `jmap` mezclando unidades reales con
 * normalizadas que rompia el sorteo de todo lo que no fuera 0..1).
 *
 * La fuerza sale del parametro `randomStrength`, asi que la pagina no puede pedir
 * una fuerza que el instrumento no tenga — y el mismo boton sirve con el motor
 * WASM o con el nativo, porque el estado es el mismo APVTS.
 */
class RandomizerAdapter final : public RandomizeController
{
public:
    explicit RandomizerAdapter (NEURONiKProcessor& processorToWrap)
        : apvts (processorToWrap.getAPVTS()),
          // Semilla del reloj, no `getSystemRandom()`: la regla del proyecto es no
          // depender de entropia de sistema (ver WASM, fase 7A).
          random (static_cast<juce::int64> (juce::Time::getMillisecondCounter())) {}

    int randomize() override
    {
        return NEURONiK::State::applyRandomize (apvts,
                                                NEURONiK::State::readRandomizeStrength (apvts),
                                                random);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::Random random;
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
 *
 * It also owns the ONE out-of-band half of the model messages: `loadModel` asks
 * the OPERATING SYSTEM for a file (the dialog the native panel opened from its
 * loadA..loadD buttons) and hands it to the processor. The page cannot do that
 * itself: it has no file system, and the model list must stay the engine's (a
 * preset carries modelPath<slot>, not a copy of the partials).
 *
 * The dialog is ASYNCHRONOUS, so the answer travels back through the two
 * callbacks the host injects (the hosts wire them to ParameterBridge's
 * sendModelsState/sendModelError, which is why the bridge, not the adapter, owns
 * the wire). Lifetime: the chooser is a member here, and juce::FileChooser drops
 * a pending async callback when it is destroyed (its destructor clears
 * asyncCallback and the dialog's own completion cannot run once the shared impl
 * is being torn down), so a host closed mid-dialog never answers into a dead
 * bridge. The other half of that guarantee is member order in every host: the
 * adapters are declared AFTER the bridge, so they are destroyed BEFORE it.
 */
class EngineModelsAdapter final : public NativeModelController
{
public:
    /** @brief Called once the processor ACCEPTED the file (slot already loaded). */
    using LoadedCallback = std::function<void (int slot)>;
    /** @brief Called when nothing was loaded: cancelled, unusable, out of range. */
    using ErrorCallback = std::function<void (int slot, const juce::String& detail)>;

    EngineModelsAdapter (NEURONiKProcessor& processorToWrap,
                         LoadedCallback onLoaded,
                         ErrorCallback onError)
        : processor (processorToWrap),
          loadedCallback (std::move (onLoaded)),
          errorCallback (std::move (onError)) {}

    int getNumModelSlots() const override { return numSlots; }

    void getCurrentModel (int slot,
                          std::array<float, 64>& amplitudes,
                          std::array<float, 64>& frequencyOffsets,
                          bool& isValid) const override
    {
        processor.getCurrentModel (slot, amplitudes, frequencyOffsets, isValid);
    }

    juce::String getModelName (int slot) const override
    {
        if (slot < 0 || slot >= numSlots)
            return {};

        return processor.getModelNames()[static_cast<size_t> (slot)];
    }

    void loadModel (int slot) override
    {
        // The bridge validates this too; a direct caller must not be able to
        // index the model array out of bounds.
        if (slot < 0 || slot >= numSlots)
        {
            reportError (slot, "slot out of range: " + juce::String (slot));
            return;
        }

        // Starting directory `juce::File()`: whatever the platform considers
        // sensible. The plugin hard-codes no paths (project rule), and the user's
        // last choice is remembered by the OS dialog, not by us.
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load a .neuronikmodel into slot " + slotLabel (slot),
            juce::File(),
            "*.neuronikmodel");

        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                      | juce::FileBrowserComponent::canSelectFiles,
                                  [this, slot] (const juce::FileChooser& chooser)
                                  {
                                      const auto file = chooser.getResult();

                                      if (file == juce::File() || ! file.existsAsFile())
                                      {
                                          reportError (slot, "no file chosen");
                                          return;
                                      }

                                      if (! processor.loadModel (file, slot))
                                      {
                                          reportError (slot, "not a usable .neuronikmodel: "
                                                                 + file.getFileName());
                                          return;
                                      }

                                      if (loadedCallback != nullptr)
                                          loadedCallback (slot);
                                  });
    }

private:
    void reportError (int slot, const juce::String& detail)
    {
        if (errorCallback != nullptr)
            errorCallback (slot, detail);
    }

    static juce::String slotLabel (int slot)
    {
        return juce::String::charToString (static_cast<juce::juce_wchar> ('A' + slot));
    }

    static constexpr int numSlots = 4;   //!< mirrors processor.getModelNames()

    NEURONiKProcessor& processor;
    LoadedCallback loadedCallback;
    ErrorCallback errorCallback;

    /** The dialog in flight; a member so it outlives launchAsync (see the class doc). */
    std::unique_ptr<juce::FileChooser> fileChooser;
};

} // namespace NEURONiK::WebUI
