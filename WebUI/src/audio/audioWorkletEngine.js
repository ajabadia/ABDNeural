/**
 * Page-side controller for the NEURONiK AudioWorklet (web audio path).
 *
 * PORTADO de `WebPilot/lib/audioWorkletEngine.js` SIN cambios de comportamiento,
 * SALVO una cosa: la política de audio. Dentro de un host JUCE (el editor del
 * plugin o la bancada del piloto) este motor **no arranca** — el audio lo pone
 * el motor nativo, y dos motores sonando no es un caso soportado. La regla vive
 * en `./policy.js` y se consulta aquí, en el único punto de entrada.
 *
 * Owns the AudioContext lifecycle and the worklet node, and translates the
 * page's parameter/MIDI state into worklet messages. The contract -> field
 * mapping lives in ../wasm/audioParams.js; this module is transport only.
 *
 * Design notes:
 *  - The .wasm binary crosses into the worklet via processorOptions (structured
 *    clone of an ArrayBuffer). AudioWorkletGlobalScope has no fetch to the page
 *    world, so processorOptions is the only channel that works from a static
 *    export served under a hashed subdirectory.
 *  - All page APIs are null-safe when AudioWorklet is unavailable (older
 *    WebView2 runtimes): the page keeps working bridge-only.
 */

import { gpFieldsFromState } from '../wasm/audioParams.js';
import {
  AUDIO_STATUS_BLOCKED,
  workletAllowed,
  workletRefusalReason,
} from './policy.js';

const WORKLET_URL = 'worklet/neuronik-worklet.js';
const WASM_URL = 'worklet/neuronik_dsp.wasm';

/** Values reported in the startup log line and the page badge. */
export const audioEngineState = {
  status: 'idle', // idle | loading | ready | error | unsupported | blocked
  error: null,
  sampleRate: 0,
  voices: 0,
};

let listeners = [];

function notify() {
  for (const listener of listeners) listener (audioEngineState);
}

/** Subscribe; fires immediately with the current state, then on every change. */
export function onAudioEngineChange(listener) {
  listener (audioEngineState);
  listeners.push (listener);
}

/**
 * Modulation contribution for the Morph Z destination (worklet meter feed).
 * Standalone only: inside the plugin the native telemetry frame carries it
 * (frame.modulation[28]) and the worklet never starts.
 */
const morphZModListeners = [];

export function onWorkletMorphZ(listener) {
  morphZModListeners.push (listener);
}

/**
 * MORPH state (pad XY + eje temporal) last pushed to the worklet. The page
 * keeps it so an engine switch can re-apply the pad position: the switch
 * rebuilds the DSP and the new engine wakes at the struct defaults (0.5,
 * 0.5, 0). Standalone only — inside a host the APVTS is the truth.
 */
const morphState = { x: 0.5, y: 0.5, z: 0.0, z2: 0.0, z3: 0.0 };

/**
 * Active voice count from the worklet meter (0..8, engine polyphony).
 * Standalone only: inside the plugin the count lives in the native engine and
 * the page has no control over it — there the indicator just stays at 0.
 * Fires at the meter cadence (~21 ms) whenever the meter carries the number.
 */
const voicesListeners = [];

export function onWorkletVoices(listener) {
  voicesListeners.push (listener);
}

/**
 * Envelope levels from the worklet meter ([amp, filter], 0..1): the SAME feed
 * the native bridge sends as telemetry `envelopes` — the WebUI's ADSR needles
 * live in the browser too (SOUND ON). Subscribers fire at the meter cadence
 * (~21 ms); only when the meter actually carries the pair.
 */
const envelopeLevelListeners = [];

export function onWorkletEnvelopeLevels(listener) {
  envelopeLevelListeners.push (listener);
}

let context = null;
let node = null;

/**
 * Start the engine: AudioContext + worklet node + initial parameter sync.
 * Resolves with the state; never throws (errors land in audioEngineState).
 *
 * Inside a JUCE host this is a NO-OP that reports `blocked`: the plugin already
 * owns the audio and the page is only a remote control for it.
 */
export async function startAudioEngine() {
  // The policy comes before everything else: inside a host there is nothing to
  // start, not even to check the runtime for.
  if (!workletAllowed())
    return blocked (workletRefusalReason());

  if (audioEngineState.status === 'ready') return audioEngineState;
  if (audioEngineState.status === 'loading') return null; // already starting
  if (typeof window === 'undefined' || !window.AudioWorkletNode)
    return unsupported ('AudioWorkletNode no disponible en este runtime');

  audioEngineState.status = 'loading';
  audioEngineState.error = null;
  notify();

  try {
    // Relative URLs on purpose: the page is served from file:// (embedded
    // snapshot) and from the static export alike.
    const workletUrl = new URL (WORKLET_URL, document.baseURI).href;
    const wasmUrl = new URL (WASM_URL, document.baseURI).href;

    context = new AudioContext ({ latencyHint: 'interactive' });

    // processorOptions clones the buffer, so the page keeps an intact copy
    // for retries (no detach surprises).
    const wasmBinary = await (await fetch (wasmUrl)).arrayBuffer();

    await context.audioWorklet.addModule (workletUrl);
    node = new AudioWorkletNode (context, 'neuronik-processor', {
      numberOfInputs: 0,
      numberOfOutputs: 1,
      outputChannelCount: [2],
      processorOptions: {
        wasmBinary,
        sampleRate: context.sampleRate,
      },
    });

    // Connect BEFORE waiting for ready: Chrome does not dispatch worklet
    // port messages until the node is pulled by the render graph, so waiting
    // first deadlocks (ready never arrives) even though the worklet is live.
    // Without a user gesture the context stays suspended and the pull is inert.
    node.connect (context.destination);

    const ready = await waitForReady (node, 8000);
    if (!ready.ok)
      throw new Error (ready.error ?? 'el worklet no reporto ready');
    // Con un gesto real (click en SOUND ON) esto arranca el reloj de audio;
    // sin gesto queda pending y el contexto sigue suspended (inocuo).
    await context.resume();

    audioEngineState.status = 'ready';
    audioEngineState.sampleRate = context.sampleRate;
    notify();

    return audioEngineState;
  } catch (error) {
    audioEngineState.status = 'error';
    audioEngineState.error = String (error?.message ?? error);
    notify();
    await teardownAudioEngine();
    return audioEngineState;
  }
}

function waitForReady(workletNode, timeoutMs) {
  return new Promise ((resolve) => {
    const timer = setTimeout (() => {
      workletNode.port.removeEventListener ('message', onMessage);
      resolve ({ ok: false, error: 'timeout esperando neuronik:ready' });
    }, timeoutMs);

    function onMessage(event) {
      const type = event.data?.type;

      if (type === 'neuronik:ready') {
        clearTimeout (timer);
        // Kept attached: this listener is also the page consumer of
        // 'neuronik:meter' (voices + morphZMod fan-out). Removing it here
        // silences telemetry right after the handshake.
        resolve ({ ok: true });
      } else if (type === 'neuronik:error') {
        clearTimeout (timer);
        workletNode.port.removeEventListener ('message', onMessage);
        resolve ({ ok: false, error: event.data.error });
      } else if (type === 'neuronik:meter') {
        audioEngineState.voices = event.data.voices;
        if (typeof event.data.voices === 'number')
          for (const listener of voicesListeners) listener (event.data.voices);
        if (typeof event.data.morphZMod === 'number')
          for (const listener of morphZModListeners) listener (event.data.morphZMod);

        if (Array.isArray (event.data.envelopes) && event.data.envelopes.length === 2)
          for (const listener of envelopeLevelListeners) listener (event.data.envelopes);
      }
    }

    // MessagePort semantics: addEventListener alone never delivers queued
    // messages — the port must be started (start() or an onmessage setter).
    workletNode.port.start ();
    workletNode.port.addEventListener ('message', onMessage);
  });
}

function unsupported(reason) {
  audioEngineState.status = 'unsupported';
  audioEngineState.error = reason;
  notify();
  return audioEngineState;
}

/** Refused by policy: a host owns the audio. Not an error — an answer. */
function blocked(reason) {
  audioEngineState.status = AUDIO_STATUS_BLOCKED;
  audioEngineState.error = reason;
  notify();
  return audioEngineState;
}

/**
 * Push the pad's morph (morphX, morphY, morphZ real units 0..1) to the
 * worklet — `neuronikSetVoiceMorph`, the export that makes the pad reach
 * the WASM engine at all (morph is VoiceParams, not GlobalParams). The
 * page remembers the value for the engine-switch re-apply.
 *
 * FASE 11.4: z2/z3 son el VOLUMEN de las capas 1 y 2 (neuronikSetVoiceLayerMorph).
 * undefined = sin cambio: el worklet no toca el export y el motor conserva.
 */
export function pushMorphToWorklet(x, y, z = 0.0, z2, z3) {
  morphState.x = x;
  morphState.y = y;
  morphState.z = z;
  if (z2 !== undefined) morphState.z2 = z2;
  if (z3 !== undefined) morphState.z3 = z3;

  if (!node) return false;

  const message = { type: 'neuronik:morph', x, y, z };
  if (z2 !== undefined) message.z2 = z2;
  if (z3 !== undefined) message.z3 = z3;
  node.port.postMessage (message);
  return true;
}

/** Morph values last pushed (for the engine-switch re-apply). */
export function getWorkletMorph() {
  return { ...morphState };
}

/** Push a full parameter snapshot (id -> normalised) to the worklet. */
export function pushParamsToWorklet(parameters) {
  if (!node) return false;

  node.port.postMessage ({
    type: 'neuronik:params',
    fields: gpFieldsFromState (parameters),
  });
  return true;
}

/** Switch engine (0 = NEURONiK, 1 = Neurotik) in the worklet. */
export function pushEngineToWorklet(index) {
  if (!node) return false;
  node.port.postMessage ({ type: 'neuronik:engine', index });
  return true;
}

/**
 * Push the spectral model slots (preset timbre data) to the worklet.
 * `slots` mirrors the bridge's modelsState: [{ slot, isValid, amplitudes[64],
 * frequencyOffsets[64] }, ...]. Models hang off the concrete engine, so the
 * worklet re-applies them after every engine switch.
 */
export function pushModelsToWorklet(slots) {
  if (!node) return false;
  node.port.postMessage ({ type: 'neuronik:models', slots });
  return true;
}

/** Forward one MIDI event (same shapes the bridge senders emit). */
export function pushMidiToWorklet(message) {
  if (!node) return false;
  node.port.postMessage ({ type: 'neuronik:midi', ...message });
  return true;
}

export function panicWorklet() {
  if (!node) return false;
  node.port.postMessage ({ type: 'neuronik:panic' });
  return true;
}

export function isAudioEngineReady() {
  return audioEngineState.status === 'ready';
}

/** Teardown (error recovery, page unload). */
export async function teardownAudioEngine() {
  try {
    node?.disconnect();
    node = null;
    await context?.close();
    context = null;
  } catch {
    // teardown is best-effort
  }
  audioEngineState.status = 'idle';
  audioEngineState.voices = 0;
  notify();
}
