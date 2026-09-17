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
    });

    transportRef.current = transport;
    setBridgeAvailable(transport.available);

    if (transport.available) {
      transport.announcePageLoaded();
      transport.sendRequestState();
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

  return {
    controls,
    parameters,
    changeCount,
    bridgeAvailable,
    snapshotVersion,
    contractErrors,
    summary,
    pushParameter,
    handleChange,
    handleGesture,
  };
}
