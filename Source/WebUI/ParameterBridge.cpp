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

    // Flat array of { slot, isValid, amplitudes[64], frequencyOffsets[64] }.
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
