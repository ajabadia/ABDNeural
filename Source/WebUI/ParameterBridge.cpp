/*
  ==============================================================================

    ParameterBridge.cpp
    Created: 16 Sep 2026
    Description: Implementation of the two way parameter bridge. Kept transport free
                 on purpose: see ParameterBridge.h for the wire format and for why
                 outgoing changes are polled instead of pushed from listeners.

  ==============================================================================
*/

#include "ParameterBridge.h"

#include <cmath>

namespace NEURONiK::WebUI
{

namespace
{
    /** Value below which two normalised values count as the same position.
     *  Tighter than any slider resolution and looser than float noise after a
     *  denormalise/renormalise round trip through a skewed range.
     */
    constexpr float valueEpsilon = 1.0e-6f;

    juce::String gestureFrom (const juce::DynamicObject& message)
    {
        const auto declared = message.getProperty ("gesture").toString();
        return declared.isEmpty() ? juce::String (BridgeGestures::change) : declared;
    }

    bool isKnownGesture (const juce::String& gesture)
    {
        return gesture == BridgeGestures::begin
            || gesture == BridgeGestures::change
            || gesture == BridgeGestures::end;
    }

    /** @brief True when `name` cannot be a preset file name (empty, too long,
     *         separators, or traversal). The wire is untrusted: a page bug or an
     *         injection attempt must end in presetError, not in a file write. */
    bool isUnsafePresetName (const juce::String& name)
    {
        if (name.isEmpty() || name.length() > 100)
            return true;

        return name.containsAnyOf ("/\\:*?\"<>|")
            || name.contains ("..")
            || name.startsWithChar ('.')
            || name.trim() != name;
    }
}

//==============================================================================

ParameterBridge::ParameterBridge (juce::AudioProcessorValueTreeState& stateToBridge)
    : apvts (stateToBridge)
{
    // The mirrored set is the APVTS layout itself, in layout order, so a parameter
    // added to createParameterLayout() shows up here without touching this class.
    for (auto* parameter : apvts.processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            entries.push_back ({ ranged, ranged->getParameterID(), 0.0f, false, false });
    }
}

void ParameterBridge::setSender (Sender newSender)
{
    sender = std::move (newSender);
}

void ParameterBridge::setPresetController (PresetController* newController) noexcept
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());
    presets = newController;
}

void ParameterBridge::setMidiController (MidiController* newController) noexcept
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());
    midi = newController;
}

void ParameterBridge::setModelController (NativeModelController* newController) noexcept
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());
    models = newController;
}

void ParameterBridge::setTelemetryController (TelemetryController* newController) noexcept
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());
    visualization = newController;
}

void ParameterBridge::setRandomizeController (RandomizeController* newController) noexcept
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());
    randomizer = newController;
}

int ParameterBridge::getParameterCount() const noexcept
{
    return static_cast<int> (entries.size());
}

juce::String ParameterBridge::getParameterId (int index) const
{
    if (index < 0 || index >= getParameterCount())
        return {};

    return entries[static_cast<size_t> (index)].id;
}

int ParameterBridge::indexOfParameter (const juce::String& id) const
{
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].id == id)
            return static_cast<int> (i);

    return -1;
}

float ParameterBridge::getNormalisedValue (const juce::String& id) const
{
    const auto index = indexOfParameter (id);

    if (index < 0)
        return 0.0f;

    return entries[static_cast<size_t> (index)].parameter->getValue();
}

//==============================================================================

juce::var ParameterBridge::describe (const Entry& entry) const
{
    const auto normalised = entry.parameter->getValue();

    juce::DynamicObject::Ptr item = new juce::DynamicObject();
    item->setProperty ("id", entry.id);
    item->setProperty ("value", static_cast<double> (normalised));
    item->setProperty ("real", static_cast<double> (entry.parameter->getNormalisableRange()
                                                        .convertFrom0to1 (normalised)));
    item->setProperty ("text", entry.parameter->getText (normalised, 32));

    return juce::var (item.get());
}

juce::var ParameterBridge::buildSnapshotVar() const
{
    juce::Array<juce::var> parameters;

    for (const auto& entry : entries)
        parameters.add (describe (entry));

    juce::DynamicObject::Ptr message = new juce::DynamicObject();
    message->setProperty ("action", BridgeActions::syncAllParams);
    message->setProperty ("version", snapshotVersion + 1);
    message->setProperty ("parameterCount", getParameterCount());
    message->setProperty ("parameters", juce::var (parameters));

    return juce::var (message.get());
}

void ParameterBridge::send (const juce::var& message, bool isSnapshot)
{
    // Counters describe traffic actually handed to the transport, so a bridge with
    // no web view yet reads as idle rather than as "sent into the void".
    if (sender == nullptr)
        return;

    if (isSnapshot)
        ++stats.snapshotsSent;
    else
        ++stats.changesSent;

    sender (message);
}

void ParameterBridge::sendFullSnapshot()
{
    auto snapshot = buildSnapshotVar();
    ++snapshotVersion;

    send (snapshot, true);

    // The page now knows every value: anything the native side changes from here on
    // is a delta, and nothing is re-sent on the next poll.
    for (auto& entry : entries)
    {
        entry.lastReported = entry.parameter->getValue();
        entry.reported = true;
    }

    // Models are not APVTS parameters: the page's params effect cannot see them.
    // They piggyback on every snapshot (additive v1 message; no backend, no send).
    sendModelsState();
}

int ParameterBridge::publishPendingChanges()
{
    int sent = 0;

    for (auto& entry : entries)
    {
        const auto current = entry.parameter->getValue();

        if (entry.reported && std::abs (current - entry.lastReported) <= valueEpsilon)
            continue;

        entry.lastReported = current;
        entry.reported = true;

        juce::DynamicObject::Ptr message = new juce::DynamicObject();
        message->setProperty ("action", BridgeActions::parameterChanged);
        message->setProperty ("id", entry.id);
        message->setProperty ("value", static_cast<double> (current));
        message->setProperty ("real", static_cast<double> (entry.parameter->getNormalisableRange()
                                                                .convertFrom0to1 (current)));
        message->setProperty ("text", entry.parameter->getText (current, 32));

        send (juce::var (message.get()), false);
        ++sent;
    }

    return sent;
}

int ParameterBridge::closeOpenGestures()
{
    int closed = 0;

    for (auto& entry : entries)
    {
        if (! entry.gestureOpen)
            continue;

        entry.parameter->endChangeGesture();
        entry.gestureOpen = false;
        ++stats.gesturesClosed;
        ++closed;
    }

    return closed;
}

void ParameterBridge::sendModelsState()
{
    if (models == nullptr)
        return;

    const int numSlots = models->getNumModelSlots();
    if (numSlots <= 0)
        return;

    // Flat array of { slot, name, isValid, amplitudes[64], frequencyOffsets[64] }.
    juce::Array<juce::var> slots;

    for (int slot = 0; slot < numSlots; ++slot)
    {
        std::array<float, 64> amplitudes;
        std::array<float, 64> frequencyOffsets;
        bool isValid = false;

        models->getCurrentModel (slot, amplitudes, frequencyOffsets, isValid);

        juce::Array<juce::var> amps;
        juce::Array<juce::var> freqs;

        for (int i = 0; i < 64; ++i)
        {
            amps.add (static_cast<double> (amplitudes[(size_t) i]));
            freqs.add (static_cast<double> (frequencyOffsets[(size_t) i]));
        }

        juce::DynamicObject::Ptr entry = new juce::DynamicObject();
        entry->setProperty ("slot", slot);
        // The display name travels so the page shows the SAME slot list the plugin
        // has: it cannot read the model directory (and the native panel drew these
        // names at the XY pad corners, so they were never a parameter either).
        entry->setProperty ("name", models->getModelName (slot));
        entry->setProperty ("isValid", isValid);
        entry->setProperty ("amplitudes", juce::var (amps));
        entry->setProperty ("frequencyOffsets", juce::var (freqs));
        slots.add (juce::var (entry.get()));
    }

    juce::DynamicObject::Ptr message = new juce::DynamicObject();
    message->setProperty ("action", BridgeActions::modelsState);
    message->setProperty ("slots", juce::var (slots));

    ++stats.modelsSent;
    send (juce::var (message.get()), false);
}

void ParameterBridge::sendTelemetry()
{
    if (visualization == nullptr)
        return;

    // --- leer el frame del backend (todos los valores 0..1) ----------------
    std::array<float, 64> spec;
    visualization->getSpectralFrame (spec.data());

    float envAmp = 0.0f, envFilter = 0.0f;
    visualization->getEnvelopeLevels (envAmp, envFilter);

    const int numTargets = visualization->getModulationTargetCount();
    std::vector<float> mod ((size_t) std::max (0, numTargets), 0.0f);
    for (int i = 0; i < numTargets; ++i)
        mod[(size_t) i] = visualization->getModulationValue (i);

    float morphX = 0.0f, morphY = 0.0f;
    visualization->getMorphCoordinates (morphX, morphY);
    const float lfo1 = visualization->getLfoValue (0);
    const float lfo2 = visualization->getLfoValue (1);

    // --- diff contra el ultimo frame ENVIADO (epsilon ~1/255) --------------
    constexpr float epsilon = 1.0f / 255.0f;
    auto moved = [epsilon] (float a, float b) { return std::abs (a - b) > epsilon; };

    bool anyMoved = ! telemetryHasLastFrame;
    if (telemetryHasLastFrame)
    {
        anyMoved = moved (envAmp, lastEnvAmp) || moved (envFilter, lastEnvFilter)
                   || moved (morphX, lastMorphX) || moved (morphY, lastMorphY)
                   || moved (lfo1, lastLfo1) || moved (lfo2, lastLfo2);
        if (! anyMoved && mod.size() != lastMod.size())
            anyMoved = true; // el numero de targets cambio: el frame importa
        if (! anyMoved)
            for (size_t i = 0; i < mod.size(); ++i)
                if (moved (mod[i], lastMod[i])) { anyMoved = true; break; }
        if (! anyMoved)
            for (size_t i = 0; i < spec.size(); ++i)
                if (moved (spec[i], lastSpec[i])) { anyMoved = true; break; }
    }
    if (! anyMoved)
        return; // sintetizador quieto: no hay mensaje

    // --- recordar y emitir --------------------------------------------------
    lastSpec = spec;
    lastEnvAmp = envAmp;
    lastEnvFilter = envFilter;
    lastMorphX = morphX;
    lastMorphY = morphY;
    lastLfo1 = lfo1;
    lastLfo2 = lfo2;
    lastMod = mod;
    telemetryHasLastFrame = true;

    juce::Array<juce::var> specVar;
    for (float s : spec)
        specVar.add ((double) s);

    juce::Array<juce::var> envVar;
    envVar.add ((double) envAmp);
    envVar.add ((double) envFilter);

    juce::Array<juce::var> lfoVar;
    lfoVar.add ((double) lfo1);
    lfoVar.add ((double) lfo2);

    juce::Array<juce::var> modVar;
    for (float m : mod)
        modVar.add ((double) m);

    juce::Array<juce::var> morphVar;
    morphVar.add ((double) morphX);
    morphVar.add ((double) morphY);

    juce::DynamicObject::Ptr message = new juce::DynamicObject();
    message->setProperty ("action", BridgeActions::telemetry);
    message->setProperty ("seq", (int) ++telemetrySeq);
    message->setProperty ("spectral", juce::var (specVar));
    message->setProperty ("envelopes", juce::var (envVar));
    message->setProperty ("lfos", juce::var (lfoVar));
    message->setProperty ("modulation", juce::var (modVar));
    message->setProperty ("morph", juce::var (morphVar));

    ++stats.telemetrySent;
    send (juce::var (message.get()), false);
}
void ParameterBridge::sendModelError (int slot, const juce::String& detail)
{
    juce::DynamicObject::Ptr message = new juce::DynamicObject();
    message->setProperty ("action", BridgeActions::modelError);
    message->setProperty ("slot", slot);
    message->setProperty ("detail", detail);

    ++stats.modelErrors;
    send (juce::var (message.get()), false);
}

/**
 * LOAD MODEL: validate the slot on the native side (the wire is untrusted) and
 * hand the request to the backend, which owns the file dialog. The answer is
 * ASYNCHRONOUS and comes back through sendModelsState()/sendModelError(): this
 * method cannot know yet whether a file was chosen.
 */
void ParameterBridge::handleLoadModel (const juce::DynamicObject& message)
{
    const auto slotProperty = message.getProperty ("slot");
    const bool numeric = slotProperty.isDouble() || slotProperty.isInt() || slotProperty.isInt64();

    if (! numeric)
    {
        sendModelError (-1, "loadModel without a numeric slot");
        return;
    }

    const auto requested = static_cast<double> (slotProperty);
    const auto slot = static_cast<int> (requested);

    if (models == nullptr)
    {
        sendModelError (slot, "no model backend installed");
        return;
    }

    // Fractional slots are rejected explicitly instead of truncated: a page that
    // sends 1.5 meant something this side does not offer, and silently loading
    // into slot 1 would look like it worked.
    if (static_cast<double> (slot) != requested)
    {
        sendModelError (slot, "slot must be an integer: " + juce::String (requested, 3));
        return;
    }

    if (slot < 0 || slot >= models->getNumModelSlots())
    {
        sendModelError (slot, "slot out of range: " + juce::String (slot));
        return;
    }

    ++stats.modelLoads;
    models->loadModel (slot);
}

//==============================================================================
// Preset management (additive wire messages; see the header doc).

void ParameterBridge::sendPresetList()
{
    juce::DynamicObject::Ptr message = new juce::DynamicObject();
    message->setProperty ("action", BridgeActions::presetList);

    juce::Array<juce::var> names;
    const auto current = presets->getCurrentPreset();

    for (const auto& name : presets->listPresets())
        names.add (name);

    message->setProperty ("presets", juce::var (names));
    message->setProperty ("current", current);

    send (juce::var (message.get()), false);
}

void ParameterBridge::sendPresetError (const char* operation, const juce::String& detail)
{
    juce::DynamicObject::Ptr message = new juce::DynamicObject();
    message->setProperty ("action", BridgeActions::presetError);
    message->setProperty ("operation", juce::String (operation));
    message->setProperty ("detail", detail);

    ++stats.presetErrors;
    send (juce::var (message.get()), false);
}

void ParameterBridge::handleListPresets()
{
    if (presets == nullptr)
    {
        sendPresetError (BridgeActions::listPresets, "no preset backend installed");
        return;
    }

    sendPresetList();
}

void ParameterBridge::handleLoadPreset (const juce::DynamicObject& message)
{
    const auto name = message.getProperty ("name").toString();

    if (isUnsafePresetName (name))
    {
        sendPresetError (BridgeActions::loadPreset, "rejected preset name: " + name);
        return;
    }

    if (presets == nullptr)
    {
        sendPresetError (BridgeActions::loadPreset, "no preset backend installed");
        return;
    }

    if (! presets->loadPreset (name))
    {
        sendPresetError (BridgeActions::loadPreset, "preset not found: " + name);
        return;
    }

    ++stats.presetsLoaded;

    // The load rewrote the whole state: the page must resynchronise everything,
    // and its open drags belong to a state that no longer exists.
    closeOpenGestures();
    sendFullSnapshot();
    sendPresetList();
    sendModelsState();
}

void ParameterBridge::handleSavePreset (const juce::DynamicObject& message)
{
    const auto name = message.getProperty ("name").toString();

    if (isUnsafePresetName (name))
    {
        sendPresetError (BridgeActions::savePreset, "rejected preset name: " + name);
        return;
    }

    if (presets == nullptr)
    {
        sendPresetError (BridgeActions::savePreset, "no preset backend installed");
        return;
    }

    if (! presets->savePreset (name))
    {
        sendPresetError (BridgeActions::savePreset, "could not write preset: " + name);
        return;
    }

    ++stats.presetsSaved;
    sendPresetList();
}

//==============================================================================
// MIDI (additive wire messages; see the header doc).

void ParameterBridge::sendMidiNoteState (const std::vector<int>& heldNotes,
                                         float pitchBendNormalized, float modWheelNormalized)
{
    juce::Array<juce::var> held;

    for (const auto note : heldNotes)
        held.add (juce::jlimit (0, 127, note));

    juce::DynamicObject::Ptr message = new juce::DynamicObject();
    message->setProperty ("action", BridgeActions::midiNoteState);
    message->setProperty ("held", juce::var (held));
    message->setProperty ("pitchBend", static_cast<double> (juce::jlimit (-1.0f, 1.0f, pitchBendNormalized)));
    message->setProperty ("modWheel", static_cast<double> (juce::jlimit (0.0f, 1.0f, modWheelNormalized)));

    send (juce::var (message.get()), false);
}

void ParameterBridge::handleMidiAction (const juce::String& action, const juce::DynamicObject& message)
{
    const auto numeric = [&message] (const char* field, bool& ok) -> double
    {
        const auto property = message.getProperty (field);
        ok = property.isDouble() || property.isInt() || property.isInt64();
        return ok ? static_cast<double> (property) : 0.0;
    };

    auto ok = false;

    if (action == BridgeActions::midiNoteOn)
    {
        const auto note = numeric ("note", ok);
        const auto velocity = numeric ("velocity", ok);
        const auto noteInt = static_cast<int> (note);

        if (! ok || static_cast<double> (noteInt) != note || noteInt < 0 || noteInt > 127
            || velocity < 0.0 || velocity > 1.0)
        {
            ++stats.midiRejected;
            return;
        }

        ++stats.midiForwarded;
        if (midi != nullptr)
            midi->noteOn (noteInt, static_cast<float> (velocity));
        return;
    }

    if (action == BridgeActions::midiNoteOff)
    {
        const auto note = numeric ("note", ok);
        const auto noteInt = static_cast<int> (note);

        if (! ok || static_cast<double> (noteInt) != note || noteInt < 0 || noteInt > 127)
        {
            ++stats.midiRejected;
            return;
        }

        ++stats.midiForwarded;
        if (midi != nullptr)
            midi->noteOff (noteInt);
        return;
    }

    if (action == BridgeActions::midiPitchBend)
    {
        const auto value = numeric ("value", ok);

        if (! ok || value < -1.0 || value > 1.0)
        {
            ++stats.midiRejected;
            return;
        }

        ++stats.midiForwarded;
        if (midi != nullptr)
            midi->pitchBend (static_cast<float> (value));
        return;
    }

    if (action == BridgeActions::midiModWheel)
    {
        const auto value = numeric ("value", ok);

        if (! ok || value < 0.0 || value > 1.0)
        {
            ++stats.midiRejected;
            return;
        }

        ++stats.midiForwarded;
        if (midi != nullptr)
            midi->modWheel (static_cast<float> (value));
        return;
    }

    if (action == BridgeActions::midiPanic)
    {
        ++stats.midiForwarded;
        if (midi != nullptr)
            midi->allNotesOff();
        return;
    }

    // Unreachable through handleJsEvent; kept defensive for direct callers.
    ++stats.midiRejected;
}

//==============================================================================

void ParameterBridge::handleJsEvent (const juce::var& message)
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());

    const auto* object = message.getDynamicObject();

    if (object == nullptr)
    {
        ++stats.rejectedMessages;
        return;
    }

    const auto action = object->getProperty ("action").toString();

    if (action == BridgeActions::parameterChanged)
    {
        applyParameterChange (*object);
        return;
    }

    if (action == BridgeActions::requestState)
    {
        sendFullSnapshot();
        return;
    }

    if (action == BridgeActions::randomize)
    {
        handleRandomize();
        return;
    }

    if (action == BridgeActions::listPresets)
    {
        handleListPresets();
        return;
    }

    if (action == BridgeActions::loadPreset)
    {
        handleLoadPreset (*object);
        return;
    }

    if (action == BridgeActions::savePreset)
    {
        handleSavePreset (*object);
        return;
    }

    if (action == BridgeActions::loadModel)
    {
        handleLoadModel (*object);
        return;
    }

    if (action == BridgeActions::midiNoteOn
     || action == BridgeActions::midiNoteOff
     || action == BridgeActions::midiPitchBend
     || action == BridgeActions::midiModWheel
     || action == BridgeActions::midiPanic)
    {
        handleMidiAction (action, *object);
        return;
    }

    ++stats.rejectedMessages;
}

/**
 * RANDOMIZE: no fields, no answer. The backend writes the APVTS with
 * setValueNotifyingHost(), so the page learns about every moved parameter through
 * the normal `publishPendingChanges()` path — no extra message to keep in sync.
 * Without a backend it still counts as accepted, exactly like MIDI in silent mode.
 */
void ParameterBridge::handleRandomize()
{
    ++stats.randomized;

    if (randomizer == nullptr)
        return;

    stats.randomizedParameters += randomizer->randomize();
}

void ParameterBridge::applyParameterChange (const juce::DynamicObject& message)
{
    const auto id = message.getProperty ("id").toString();

    if (id.isEmpty())
    {
        ++stats.rejectedMessages;
        return;
    }

    const auto index = indexOfParameter (id);

    if (index < 0)
    {
        // Unknown ids are ordinary while the two sides are developed together or
        // when an old page talks to a new plugin; they are counted, not fatal.
        ++stats.unknownIds;
        return;
    }

    const auto valueProperty = message.getProperty ("value");

    if (! valueProperty.isDouble() && ! valueProperty.isInt() && ! valueProperty.isInt64())
    {
        ++stats.rejectedMessages;
        return;
    }

    const auto gesture = gestureFrom (message);

    if (! isKnownGesture (gesture))
    {
        ++stats.rejectedMessages;
        return;
    }

    auto& entry = entries[static_cast<size_t> (index)];

    if (gesture == BridgeGestures::begin && ! entry.gestureOpen)
    {
        entry.parameter->beginChangeGesture();
        entry.gestureOpen = true;
        ++stats.gesturesOpened;
    }

    const auto requested = juce::jlimit (0.0f, 1.0f, static_cast<float> (static_cast<double> (valueProperty)));

    if (std::abs (entry.parameter->getValue() - requested) > valueEpsilon)
    {
        entry.parameter->setValueNotifyingHost (requested);
        ++stats.appliedFromJs;
    }
    else
    {
        ++stats.idleChanges;
    }

    // Whatever the APVTS ended up storing (it may snap or re-map the value) is what
    // the page already shows, so it is never echoed back to the sender.
    entry.lastReported = entry.parameter->getValue();
    entry.reported = true;

    if (gesture == BridgeGestures::end && entry.gestureOpen)
    {
        entry.parameter->endChangeGesture();
        entry.gestureOpen = false;
        ++stats.gesturesClosed;
    }
}

} // namespace NEURONiK::WebUI
