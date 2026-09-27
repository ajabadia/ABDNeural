/**
 * Vanilla store: generated contract + bridge transport + gesture protocol.
 *
 * PORTADO de `WebPilot/lib/useParameterControls.js` (el hook React del piloto).
 * El hook era el PEGAMENTO, no la UI: cargaba los view-models del contrato,
 * guardaba el estado normalizado y exponía los handlers que empujan por el
 * bridge con el protocolo de gestos. Esa lógica es la misma en vainilla; lo que
 * desaparece es `useState`/`useMemo`/`useRef` y en su lugar hay un único objeto
 * de estado inmutable, `getState()` y `subscribe()`.
 *
 * Value model: state and wire are ALWAYS parameter-normalised 0..1 (the value
 * the APVTS reports). The shared controls take that same 0..1 as their own
 * value (it drives rotation/position), so no double conversion happens on the
 * way in or out — skew lives in the NormalisableRange mapping, only display
 * formatting needs real units (contracts/paramValue.js).
 *
 * Lifecycle: `start()` connects to the host (pageLoaded + requestState +
 * listPresets), `dispose()` removes every listener. Without a host the store
 * works in LOCAL MODE: `available: false` and every send is a no-op.
 *
 * State actions that the PAGE cannot perform (randomize, model loading) say so in
 * their return value — false in local mode — instead of pretending: the work belongs
 * to the processor (it has the ranges and the freeze flags) or to the host (only it
 * can open a file dialog).
 */

import { createBridgeTransport } from '../bridge/bridgeCore.js';
import { latestTelemetry, onTelemetry, pushTelemetryFrame } from '../bridge/telemetry.js';
import {
  PILOT_PARAMETER_IDS,
  contractSummary,
  defaultNormalizedState,
  describePilotControls,
  validateNormalizedState,
} from './parameters.js';

/**
 * @param {object} [options]
 * @param {string[]} [options.ids]  parameters this store owns (normalised space)
 * @param {object}   [options.scope] global object used for the selftest handles
 *   (`__pilotReady`, `__pilotSendMidi`); injectable so tests need no real window.
 * @returns {object} the store
 */
export function createParameterStore({ ids = PILOT_PARAMETER_IDS, scope = globalThis } = {}) {
  const controls = describePilotControls(ids);
  const summary = contractSummary(ids);
  const listeners = new Set();

  let state = {
    controls,
    summary,
    parameters: defaultNormalizedState(ids),
    contractErrors: [],
    changeCount: 0,
    bridgeAvailable: false,
    snapshotVersion: 0,
    presetState: { presets: [], current: '' },
    presetError: null,
    midiState: { held: [], pitchBend: 0, modWheel: 0 },
    // Spectral model slots (bridge modelsState): [{ slot, name, isValid,
    // amplitudes[64], frequencyOffsets[64] }, ...] — preset timbre data outside
    // the APVTS. `name` is what the page shows next to A..D (it cannot read the
    // model directory).
    models: null,   // modo local: `emptyLocalModels()` en el arranque (app.js)
    // Last model load that did NOT happen (bridge modelError), or null. The view
    // shows it instead of letting a click fail in silence.
    modelError: null,
    // MODO LOCAL: el camino de carga local esta montado (input de fichero).
    // La ficha RANURAS lo usa para habilitar CARGAR sin host.
    localModelReady: false,
  };

  let transport = null;
  let draggingId = null;
  let started = false;

  state = { ...state, contractErrors: validateNormalizedState(ids, state.parameters) };

  const setState = (patch) => {
    state = { ...state, ...patch };

    for (const listener of listeners) listener(state);
  };

  /** Latest state, for callbacks that were created once. */
  const getState = () => state;

  /**
   * Observe the store. The listener is called immediately with the current
   * state (so a renderer never has to seed itself) and on every change.
   * @returns {() => void} unsubscribe
   */
  function subscribe(listener) {
    listeners.add(listener);
    listener(state);

    return () => listeners.delete(listener);
  }

  /**
   * Push one normalised edit to state + bridge. `phase` follows the bridge
   * protocol: 'begin' on drag start, 'change' while dragging, 'end' when a
   * value lands without a drag (click, select, keyboard step).
   */
  function pushParameter(id, normalized, phase = 'change') {
    const parameters = { ...state.parameters, [id]: normalized };

    setState({
      parameters,
      changeCount: state.changeCount + 1,
      contractErrors: validateNormalizedState(ids, parameters),
    });

    transport?.sendParameterChange(id, normalized, phase);
  }

  /** onChange handler for a parameter-bound control (receives normalised). */
  function handleChange(id, normalized) {
    const phase = draggingId === id ? 'change' : 'end';

    pushParameter(id, normalized, phase);
  }

  /** onGesture handler: opens/closes the drag window for the change phase. */
  function handleGesture(id, phase) {
    if (phase === 'begin') {
      draggingId = id;
      transport?.sendParameterChange(id, state.parameters[id] ?? 0, 'begin');
    } else {
      draggingId = null;
    }
  }

  /** Preset management over the bridge; answers update presetState/presetError. */
  function listPresets() {
    transport?.sendListPresets();
  }

  function loadPreset(name) {
    transport?.sendLoadPreset(name);
  }

  function savePreset(name) {
    transport?.sendSavePreset(name);
  }

  // ---- Acciones de estado (el backend las ejecuta sobre el APVTS) -----------

  /**
   * RANDOM: sortea el timbre en el procesador. En modo local (sin host) no hay
   * APVTS que sortear, asi que no se inventa un estado distinto al del plugin:
   * devuelve false y la pagina no finge que ha pasado algo.
   */
  function randomize() {
    if (!state.bridgeAvailable) return false;

    transport?.sendRandomize();
    return true;
  }

  /**
   * Carga un modelo en la ranura `slot` (0..3 = A..D). El dialogo lo abre el HOST
   * (la pagina no tiene sistema de ficheros), asi que esto solo PIDE: la respuesta
   * llega por el cable como `models` nuevo o como `modelError`. En modo local no hay
   * a quien pedirselo, asi que no se finge una carga: devuelve false.
   */
  function loadModel(slot, { requestLocalFile = null } = {}) {
    if (!state.bridgeAvailable) {
      // MODO LOCAL: la pagina SI puede pedir un fichero (input oculto de
      // app.js). Sin handler no se finge nada: devuelve false como siempre.
      if (typeof requestLocalFile === 'function' && slot >= 0 && slot <= 3) {
        requestLocalFile(slot);
        return true;
      }
      return false;
    }

    // El error anterior es de OTRO intento: abrir el dialogo lo deja obsoleto, y
    // dejar ahi el mensaje viejo mientras el usuario elige un fichero miente.
    setState({ modelError: null });
    transport?.sendLoadModel(slot);
    return true;
  }

  /**
   * MODO LOCAL: el usuario ya eligio el fichero — parsear, meterlo en el
   * estado `models` (lo que pinta la ficha RANURAS) y dejar que el paint
   * del motor lo lleve al worklet por `neuronik:models`. El error del
   * parser se pinta en el MISMO sitio que el modelError del host.
   */
  async function loadLocalModel(file, slot) {
    const { readModelFile } = await import('../audio/localModels.js');

    try {
      const parsed = await readModelFile(file);
      const models = Array.isArray(state.models) ? [...state.models] : null;

      if (!models) return false;

      models[slot] = { ...parsed, slot };
      setState({ models, modelError: null });
      return true;
    } catch (error) {
      setState({ modelError: { slot, detail: String(error?.message ?? error) } });
      return false;
    }
  }

  /**
   * MODO LOCAL: declara que el camino de carga local esta montado (app.js
   * lo llama cuando el input de fichero existe en el DOM).
   */
  function setLocalModelReady(ready) {
    if (state.bridgeAvailable || state.localModelReady === ready) return;
    setState({ localModelReady: ready === true });
  }

  /**
   * MODO LOCAL: siembra las ranuras que el host llenaria (solo si no hay
   * bridge: con host, modelsState manda y esto no se llama).
   */
  function seedLocalModels(slots) {
    if (state.bridgeAvailable || !Array.isArray(slots)) return false;
    setState({ models: slots });
    return true;
  }

  // ---- MIDI (page keyboard/wheels -> plugin; see bridge/bridgeCore.js) -------

  function sendMidiNoteOn(note, velocity) {
    transport?.sendMidiNoteOn(note, velocity);
  }

  function sendMidiNoteOff(note) {
    transport?.sendMidiNoteOff(note);
  }

  function sendMidiPitchBend(value) {
    transport?.sendMidiPitchBend(value);
  }

  function sendMidiModWheel(value) {
    transport?.sendMidiModWheel(value);
  }

  function sendMidiPanic() {
    transport?.sendMidiPanic();
  }

  /** Route one selftest message to the store's OWN MIDI path. */
  function sendMidiMessage(message) {
    if (!message || typeof message.action !== 'string') return;

    switch (message.action) {
      case 'midiNoteOn': sendMidiNoteOn(message.note, message.velocity); break;
      case 'midiNoteOff': sendMidiNoteOff(message.note); break;
      case 'midiPitchBend': sendMidiPitchBend(message.value); break;
      case 'midiModWheel': sendMidiModWheel(message.value); break;
      case 'midiPanic': sendMidiPanic(); break;
      default: break;
    }
  }

  /** Connect to the host (or fall into local mode) and announce the page. */
  function start() {
    if (started) return;
    started = true;

    // Marker the WebView2 host times the real panel startup against.
    if (scope) scope.__pilotReady = true;

    transport = createBridgeTransport({
      onSnapshot(entries) {
        const parameters = { ...state.parameters };

        for (const entry of entries)
          if (entry.id in parameters) parameters[entry.id] = entry.value;

        setState({
          parameters,
          contractErrors: validateNormalizedState(ids, parameters),
          snapshotVersion: state.snapshotVersion + 1,
        });
      },

      onParameterChanged(id, value) {
        if (!(id in state.parameters)) return;

        const parameters = { ...state.parameters, [id]: value };

        setState({ parameters, contractErrors: validateNormalizedState(ids, parameters) });
      },

      onPresetList(presetState) {
        setState({ presetState, presetError: null });
      },

      onPresetError(presetError) {
        setState({ presetError });
      },

      onMidiState(midiState) {
        setState({ midiState });
      },

      onModels(slots) {
        // Los slots frescos limpian el ultimo error: el host acaba de responder con
        // lo que hay en el motor, que es lo que la vista pinta.
        setState({ models: Array.isArray(slots) ? slots : null, modelError: null });
      },

      onModelError(modelError) {
        setState({ modelError });
      },

      onTelemetry(frame) {
        // Los frames NO son estado de la app (llegan a ~15 Hz): van al bufer de
        // src/bridge/telemetry.js, que reparte solo a los suscriptores visuales.
        pushTelemetryFrame(frame);
      },
    });

    setState({ bridgeAvailable: transport.available });

    if (transport.available) {
      transport.announcePageLoaded();
      transport.sendRequestState();
      transport.sendListPresets();
    }

    // Selftest handle: lets the WebView2 host drive the page's OWN MIDI path
    // (the same functions the keyboard callbacks call). No-op in local mode.
    if (scope) scope.__pilotSendMidi = sendMidiMessage;
  }

  /** Disconnect: no listeners left behind, no stale selftest handles. */
  function dispose() {
    transport?.dispose();
    transport = null;
    draggingId = null;
    started = false;

    if (scope) {
      scope.__pilotReady = false;
      delete scope.__pilotSendMidi;
    }

    setState({ bridgeAvailable: false });
  }

  return {
    ids,
    getState,
    subscribe,
    /** Telemetria en tiempo real (fuera del ciclo setState): ver bridge/telemetry.js. */
    onTelemetry,
    /** Ultimo frame recibido, o null antes del primero (visuales que montan tarde). */
    latestTelemetry,
    start,
    dispose,
    pushParameter,
    handleChange,
    handleGesture,
    listPresets,
    loadPreset,
    savePreset,
    randomize,
    loadModel,
    loadLocalModel,
    seedLocalModels,
    setLocalModelReady,
    sendMidiNoteOn,
    sendMidiNoteOff,
    sendMidiPitchBend,
    sendMidiModWheel,
    sendMidiPanic,
    sendMidiMessage,
  };
}
