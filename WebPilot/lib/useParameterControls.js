'use client';

/**
 * React glue: generated contract + shared control family + bridge transport.
 *
 * Given a list of parameter IDs, this hook loads their view-models from the
 * generated contract, holds the normalised state, and exposes edit/gesture
 * handlers that push over the bridge with the gesture protocol. The page keeps
 * working in local mode when the bridge is unavailable (no window.__JUCE__).
 *
 * Value model: state and wire are ALWAYS parameter-normalised 0..1 (the value
 * the APVTS reports). The shared controls take that same 0..1 as their own
 * value (it drives rotation/position), so no double conversion happens on the
 * way in or out — skew lives in the NormalisableRange mapping, only display
 * formatting needs real units (lib/paramValue.js).
 */

import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import { createBridgeTransport } from './bridge.js';
import {
  PILOT_PARAMETER_IDS,
  contractSummary,
  defaultNormalizedState,
  describePilotControls,
  validateNormalizedState,
} from './parameters.js';

export function useParameterControls(ids = PILOT_PARAMETER_IDS) {
  // Contract view-models, in declaration order. The contract is generated at
  // build time, so `ids` is the only dependency.
  const controls = useMemo(() => describePilotControls(ids), [ids]);

  // Normalised state, seeded in the hook's own state space (0..1 for every
  // kind — NOT real units; see defaultNormalizedState in parameters.js).
  const [parameters, setParameters] = useState(() => defaultNormalizedState(ids));
  const [changeCount, setChangeCount] = useState(0);
  const [bridgeAvailable, setBridgeAvailable] = useState(false);
  const [snapshotVersion, setSnapshotVersion] = useState(0);
  const [presetState, setPresetState] = useState({ presets: [], current: '' });
  const [presetError, setPresetError] = useState(null);
  const [midiState, setMidiState] = useState({ held: [], pitchBend: 0, modWheel: 0 });
  // Spectral model slots (bridge modelsState): [{ slot, isValid, amplitudes[64],
  // frequencyOffsets[64] }, ...] — preset timbre data outside the APVTS.
  const [models, setModels] = useState(null);

  const transportRef = useRef(null);
  const draggingIdRef = useRef(null);

  // Latest state for callbacks created once, below.
  const parametersRef = useRef(parameters);
  parametersRef.current = parameters;

  const contractErrors = useMemo(
    () => validateNormalizedState(ids, parameters),
    [ids, parameters],
  );
  const summary = useMemo(() => contractSummary(ids), [ids]);

  useEffect(() => {
    // Marker the WebView2 host times the real panel startup against.
    window.__pilotReady = true;

    const transport = createBridgeTransport({
      onSnapshot(entries) {
        setParameters((current) => {
          const next = { ...current };

          for (const entry of entries)
            if (entry.id in next) next[entry.id] = entry.value;

          return next;
        });
        setSnapshotVersion((version) => version + 1);
      },

      onParameterChanged(id, value) {
        setParameters((current) =>
          id in current ? { ...current, [id]: value } : current);
      },

      onPresetList({ presets, current }) {
        setPresetState({ presets, current });
        setPresetError(null);
      },

      onPresetError(error) {
        setPresetError(error);
      },

      onMidiState(state) {
        setMidiState(state);
      },

      onModels(slots) {
        setModels(Array.isArray(slots) ? slots : null);
      },
    });

    transportRef.current = transport;
    setBridgeAvailable(transport.available);

    if (transport.available) {
      transport.announcePageLoaded();
      transport.sendRequestState();
      transport.sendListPresets();
    }

    return () => {
      window.__pilotReady = false;
      transport.dispose();
      transportRef.current = null;
    };
  }, []);

  /**
   * Push one normalised edit to state + bridge. `phase` follows the bridge
   * protocol: 'begin' on drag start, 'change' while dragging, 'end' when a
   * value lands without a drag (click, select, keyboard step).
   */
  const pushParameter = useCallback((id, normalized, phase = 'change') => {
    setParameters((current) => ({ ...current, [id]: normalized }));
    setChangeCount((count) => count + 1);

    if (transportRef.current?.available)
      transportRef.current.sendParameterChange(id, normalized, phase);
  }, []);

  /** onChange handler for a parameter-bound control (receives normalised). */
  const handleChange = useCallback((id, normalized) => {
    const phase = draggingIdRef.current === id ? 'change' : 'end';

    pushParameter(id, normalized, phase);
  }, [pushParameter]);

  /** onGesture handler: opens/closes the drag window for the change phase. */
  const handleGesture = useCallback((id, phase) => {
    if (phase === 'begin') {
      draggingIdRef.current = id;

      if (transportRef.current?.available)
        transportRef.current.sendParameterChange(
          id, parametersRef.current[id] ?? 0, 'begin');
    } else {
      draggingIdRef.current = null;
    }
  }, []);

  /** Preset management over the bridge; answers update presetState/presetError. */
  const listPresets = useCallback(() => {
    transportRef.current?.sendListPresets();
  }, []);

  const loadPreset = useCallback((name) => {
    transportRef.current?.sendLoadPreset(name);
  }, []);

  const savePreset = useCallback((name) => {
    transportRef.current?.sendSavePreset(name);
  }, []);

  // ---- MIDI (page keyboard/wheels -> plugin; see bridge.js) ------------------
  const sendMidiNoteOn = useCallback((note, velocity) => {
    transportRef.current?.sendMidiNoteOn(note, velocity);
  }, []);

  const sendMidiNoteOff = useCallback((note) => {
    transportRef.current?.sendMidiNoteOff(note);
  }, []);

  const sendMidiPitchBend = useCallback((value) => {
    transportRef.current?.sendMidiPitchBend(value);
  }, []);

  const sendMidiModWheel = useCallback((value) => {
    transportRef.current?.sendMidiModWheel(value);
  }, []);

  const sendMidiPanic = useCallback(() => {
    transportRef.current?.sendMidiPanic();
  }, []);

  useEffect(() => {
    // Selftest handle: lets the WebView2 host drive the page's OWN MIDI path
    // (the same functions the keyboard callbacks call). No-op in local mode.
    window.__pilotSendMidi = (message) => {
      if (!message || typeof message.action !== 'string') return;
      if (message.action === 'midiNoteOn') sendMidiNoteOn(message.note, message.velocity);
      else if (message.action === 'midiNoteOff') sendMidiNoteOff(message.note);
      else if (message.action === 'midiPitchBend') sendMidiPitchBend(message.value);
      else if (message.action === 'midiModWheel') sendMidiModWheel(message.value);
      else if (message.action === 'midiPanic') sendMidiPanic();
    };

    return () => {
      delete window.__pilotSendMidi;
    };
  }, [sendMidiNoteOn, sendMidiNoteOff, sendMidiPitchBend, sendMidiModWheel, sendMidiPanic]);

  return {
    controls,
    parameters,
    changeCount,
    bridgeAvailable,
    snapshotVersion,
    contractErrors,
    summary,
    presetState,
    presetError,
    midiState,
    models,
    pushParameter,
    handleChange,
    handleGesture,
    listPresets,
    loadPreset,
    savePreset,
    sendMidiNoteOn,
    sendMidiNoteOff,
    sendMidiPitchBend,
    sendMidiModWheel,
    sendMidiPanic,
  };
}
