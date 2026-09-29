/**
 * NEURONiK AudioWorklet processor — drives the REAL DSP (Fase 5 WASM build)
 * inside the audio thread:
 *
 *   JS events/params -> port messages -> this processor
 *     -> NeuronikWasmBridge (extern "C") -> Runtime::DspEngineFacade -> engine
 *
 * The .wasm binary arrives through processorOptions (structured clone of an
 * ArrayBuffer): the worklet global scope is a SEPARATE world from the page,
 * so globals set on the page are not visible here and AudioWorkletGlobalScope
 * has neither fetch-relative tricks nor importScripts. The glue ES6 module
 * (MODULARIZE + EXPORT_ES6) is imported statically; instantiation goes through
 * the instantiateWasm hook (same technique as Tests/neuronik_wasm_smoke.mjs).
 *
 * Contract knowledge (which parameter id means what) lives on the PAGE side
 * (WebUI/src/wasm/audioParams.js): this processor only understands
 * GlobalParams FIELD INDICES, exactly the order documented in
 * NeuronikWasmBridge.cpp::neuronikGlobalParamsLayout():
 *
 *   0 masterLevel   1 saturationAmt   2 bpm (f64!)
 *   3 delayTime     4 delayFB
 *   5 chorusRate    6 chorusDepth     7 chorusMix
 *   8 reverbSize    9 reverbDamping  10 reverbWidth  11 reverbMix
 *  12 lfo1.waveform 13 lfo1.rateHz   14 lfo1.syncMode
 *  15 lfo1.rhythmicDivision          16 lfo1.depth
 *  17..21 lfo2.* (same order)
 *  22..33 modMatrix[r].{source,destination,amount} r=0..3
 *
 * ADSR (canal `neuronik:voice`, 2026-09-28) — el layout de
 * NeuronikWasmBridge.cpp::neuronikVoiceEnvelopeLayout(). Son VoiceParams, no
 * GlobalParams: hasta este canal los knobs de envolvente de la pagina no
 * llegaban al motor local y las dos envolventes vivian en los defaults de C++.
 *
 *   0 ampAttack 1 ampDecay 2 ampSustain 3 ampRelease
 *   4 filterAttack 5 filterDecay 6 filterSustain 7 filterRelease
 *     (tiempos en MILISEGUNDOS — el contrato los da en segundos, como el
 *      APVTS del plugin, y el multiply por 1000 lo hace la pagina)
 */

import createModule from './neuronik_dsp.js';

const EVENT_TYPE = { NOTE_ON: 0, NOTE_OFF: 1, PITCH_BEND: 2, CHANNEL_PRESSURE: 3 };

/** ModMatrix destination the page's z-ring visualises (engine enum: 28 = Morph Z). */
const MORPH_Z_DESTINATION = 28;
const EVENT_BYTES = 24; // Runtime::Event: 4xi32 + f32 + i32 (static_assert'd)
const MAX_EVENTS_PER_BLOCK = 16;

/** Field 2 (bpm) is a double inside GlobalParams: written through the f64 view. */
const BPM_FIELD = 2;
// Fields whose GlobalParams member is a C++ int (DspTypes.h): written via
// Int32 — an f32 write leaves the IEEE bit pattern (2.0 -> 0x40000000),
// which the engine reads as garbage (route.source -> jlimit clamps it away).
// Order-sensitive with the bridge layout: lfo{1,2}.{waveform,syncMode,
// rhythmicDivision} and modMatrix[r].{source,destination}; amounts are float.
const INT_FIELDS = new Set([12, 14, 15, 17, 19, 20, 22, 23, 25, 26, 28, 29, 31, 32]);
const DEFAULT_BPM = 120.0;

class NeuronikProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();

    const processorOptions = options?.processorOptions ?? {};
    this.wasmBinary = processorOptions.wasmBinary ?? null;
    this.sampleRate = processorOptions.sampleRate || globalThis.sampleRate || 48000;

    // Allocation quantum: worklet render quanta are stable per context; the
    // first process() call allocates for it (128 floor, 2048 cap).
    this.blockCap = Math.max(128, Math.min(globalThis.currentRenderQuantum || 128, 2048));
    this.blockSamples = 0;

    this.ready = false;
    this.initError = null;
    this.blockCounter = 0;

    // Runtime::Event staging (heap side) — filled from the message queue.
    this.pendingEvents = [];

    this.port.onmessage = (event) => this.handleMessage(event.data ?? {});
    this.initialize();
  }

  async initialize() {
    try {
      if (!this.wasmBinary)
        throw new Error('no wasmBinary in processorOptions');
      console.log('[worklet-dbg] wasmBinary bytes=', this.wasmBinary.byteLength);
      const Module = await createModule ({
        // Arrow (not a method shorthand + .bind): a MethodDefinition cannot be
        // a MemberExpression target, so `method(){}.bind(this)` never parses —
        // the worklet module would fail to load. Lexical this = the processor.
        instantiateWasm: (info, receiveInstance) => {
          WebAssembly.instantiate (new Uint8Array (this.wasmBinary), info)
            .then ((out) => receiveInstance (out.instance))
            .catch ((e) => { throw e; });
          return {};
        },
      });

      console.log('[worklet-dbg] modulo creado');
      // El glue de este build no publica HEAPU8/HEAPF32 en el Module; derivarlas
      // desde la memoria exportada (HEAP32 si esta asignado explicitamente).
      if (!Module.HEAPU8) Module.HEAPU8 = new Uint8Array (Module.HEAP32.buffer);
      if (!Module.HEAPF32) Module.HEAPF32 = new Float32Array (Module.HEAP32.buffer);
      this.module = Module;
      this.allocateBuffers();
      console.log('[worklet-dbg] buffers asignados, init sr=', this.sampleRate);
      Module._neuronikInit (this.sampleRate, 128);
      console.log('[worklet-dbg] _neuronikInit OK');

      // Mirror the C++ default state: the facade applies whatever bytes the
      // JS side sends, and zeros would mean masterLevel 0 (silence). The page
      // immediately overwrites these with the generated contract defaults.
      this.applyGpMirror();


      this.ready = true;
      this.port.postMessage({
        type: 'neuronik:ready',
        sampleRate: this.sampleRate,
        globalParamsSize: this.gpSize,
        paramsFieldCount: this.paramsFieldCount,
        voiceEnvelopeSize: this.veSize,
      });
      console.log('[worklet-dbg] initialize COMPLETO (ready posteado)');
    } catch (error) {

      console.log('[worklet-dbg] EXCEPCION:', error?.stack ?? String(error));
      this.initError = String(error?.message ?? error);
      this.port.postMessage({ type: 'neuronik:error', error: this.initError });
    }
  }

  /** One-time heap allocations: I/O blocks, GP mirror + scratch, event buffer. */
  allocateBuffers() {
    const Module = this.module;
    const blockBytes = this.blockCap * 4;

    this.leftPtr = Module._malloc (blockBytes);
    this.rightPtr = Module._malloc (blockBytes);
    this.eventsPtr = Module._malloc (EVENT_BYTES * MAX_EVENTS_PER_BLOCK);

    this.gpSize = Module._neuronikGlobalParamsSize();
    this.gpPtr = Module._malloc (this.gpSize);

    // Layout descriptor: field offsets of the base layout plus the mod matrix
    // appended after it (both queries return their field count when out==0).
    const baseFields = Module._neuronikGlobalParamsLayout (0, 0);
    const modFields = Module._neuronikModMatrixLayout (0, 0);
    this.paramsFieldCount = baseFields + modFields;
    const scratchPtr = Module._malloc (4 * (baseFields + modFields));
    const scratch = Module.HEAP32.subarray (scratchPtr >> 2, (scratchPtr >> 2) + baseFields + modFields);

    const basePtr = Module._malloc (4 * baseFields);
    Module._neuronikGlobalParamsLayout (basePtr, baseFields);
    const base = Module.HEAP32.subarray (basePtr >> 2, (basePtr >> 2) + baseFields);

    const modPtr = Module._malloc (4 * modFields);
    Module._neuronikModMatrixLayout (modPtr, modFields);
    const mod = Module.HEAP32.subarray (modPtr >> 2, (modPtr >> 2) + modFields);

    // Keep byte offsets (not view indices) so f32/f64 views share one buffer.
    this.gpFieldByteOffsets = Array.from (base).concat (Array.from (mod));

    Module._free (basePtr);
    Module._free (modPtr);
    Module._free (scratchPtr);

    this.gpMirror = new ArrayBuffer (this.gpSize);
    this.gpF32 = new Float32Array (this.gpMirror);
    this.gpI32 = new Int32Array (this.gpMirror);

    // 2 floats para los niveles de envolvente: neuronikGetEnvelopeLevels escribe
    // por punteros y el meter los lee cada ~21 ms. SIEMPRE asignados (no como
    // los buffers de modelos, que son perezosos): el meter corre desde el primer
    // bloque, con o sin preset cargado.
    this.envPtr = Module._malloc (8);
    this.envView = Module.HEAPF32.subarray (this.envPtr >> 2, (this.envPtr >> 2) + 2);
    this.gpF64 = new Float64Array (this.gpMirror);

    // ESPEJO DEL ADSR (canal `neuronik:voice`): ocho floats, layout del POD
    // VoiceEnvelopeWire del puente. Es un POD TODO FLOAT (el puente lo static_assert'ea
    // a 32 bytes, sin relleno), asi que no hace falta ni vista f64 ni set de campos
    // enteros como en el espejo de GlobalParams: el offset de layout, dividido por
    // cuatro, ES el indice del Float32Array.
    this.veSize = Module._neuronikVoiceEnvelopeSize();
    this.vePtr = Module._malloc (this.veSize);

    const veFields = Module._neuronikVoiceEnvelopeLayout (0, 0);
    const vePtr = Module._malloc (4 * veFields);
    Module._neuronikVoiceEnvelopeLayout (vePtr, veFields);
    this.veFieldByteOffsets = Array.from (
      Module.HEAP32.subarray (vePtr >> 2, (vePtr >> 2) + veFields));
    Module._free (vePtr);

    this.veMirror = new Float32Array (this.veSize / 4);
    // El espejo arranca VACIO y NO se manda al arrancar: el motor ya nace con
    // los defaults de su struct (10/100/0.7/500 ms en las dos envolventes) y
    // un push de ceros los congelaria en silencio hasta que llegase la pagina.
    // La pagina manda el snapshot completo del contrato en cuanto el worklet
    // esta listo, y `veDirty` es lo que hace legitimo re-aplicarlo tras un
    // cambio de motor (que reconstruye el motor con sus defaults otra vez).
    this.veDirty = false;

    this.leftView = null;
    this.rightView = null;
  }

  /** Copy the JS mirror into the WASM heap and push it to the engine. */
  applyGpMirror() {
    const Module = this.module;
    Module.HEAPU8.set (new Uint8Array (this.gpMirror), this.gpPtr);
    Module._neuronikSetGlobalParams (this.gpPtr, this.gpSize);
  }

  /** Write one ADSR field by layout index and push the whole mirror. */
  applyVeMirror() {
    if (!this.veDirty) return;

    const Module = this.module;
    // El POD son ocho floats contiguos: el heap y el espejo comparten indices.
    Module.HEAPF32.set (this.veMirror, this.vePtr >> 2);
    Module._neuronikSetVoiceEnvelope (this.vePtr, this.veSize);
  }

  /** Write one GlobalParams field by layout index (handles the f64 bpm slot). */
  writeGpField(fieldIndex, value) {
    const byteOffset = this.gpFieldByteOffsets[fieldIndex];
    if (byteOffset === undefined) return;

    if (fieldIndex === BPM_FIELD) {
      const f64Offset = byteOffset / 8;
      if (Number.isInteger (f64Offset)) this.gpF64[f64Offset] = Number (value);
      return;
    }

    const f32Offset = byteOffset / 4;
    if (INT_FIELDS.has (fieldIndex)) {
      // C++ int member: store the integer value, never its f32 bit pattern.
      if (Number.isInteger (f32Offset)) this.gpI32[f32Offset] = Math.round (Number (value));
      return;
    }
    if (Number.isInteger (f32Offset)) this.gpF32[f32Offset] = Number (value);
  }

  /**
   * Heap buffer for spectral model slots (preset timbre data). 128 floats per
   * slot: amplitudes[0..63] then frequencyOffsets[0..63], laid out to match
   * NeuronikWasmBridge.cpp::neuronikLoadModel(). Filled by 'neuronik:models'.
   */
  allocateModelsBuffer() {
    const Module = this.module;
    this.modelFloats = 128;                                   // 64 amps + 64 freqs
    this.modelsPtr = Module._malloc (4 * this.modelFloats);
    this.modelsView = Module.HEAPF32.subarray (
      this.modelsPtr >> 2, (this.modelsPtr >> 2) + this.modelFloats);
    this.engineType = 0;

    // FASE 11.3: scratch de la capa extra (193 floats/frame x 16 frames
    // + 16 pesos = 3104 floats). Se asigna UNA vez y se rellena por slot.
    this.layerFloats = 3 * 64 * 16 + 1 + 16;
    this.layersPtr = Module._malloc (4 * this.layerFloats);
    this.layersView = Module.HEAPF32.subarray (
      this.layersPtr >> 2, (this.layersPtr >> 2) + this.layerFloats);
  }

  handleMessage(message) {
    switch (message.type) {
      case 'neuronik:params': {
        // fields: [[fieldIndex, realValue], ...] (contract mapping on the page)
        if (!this.ready) return;
        for (const [fieldIndex, value] of message.fields ?? [])
          this.writeGpField (fieldIndex, value);
        this.applyGpMirror();
        break;
      }

      case 'neuronik:models': {
        // slots: [{ slot, isValid, amplitudes[64], frequencyOffsets[64] }, ...]
        // (bridge modelsState). Written into one shared heap buffer and fanned
        // out to every voice of the ACTIVE engine — the models hang off the
        // concrete engine, not the facade, hence this.engineType.
        if (!this.ready) return;
        if (!this.modelsView) this.allocateModelsBuffer();

        for (const slotEntry of message.slots ?? []) {
          const slot = slotEntry.slot | 0;
          if (slot < 0 || slot > 3) continue;

          const amps = slotEntry.amplitudes;
          const freqs = slotEntry.frequencyOffsets;
          if (!Array.isArray (amps) || amps.length !== 64) continue;

          this.modelsView.fill (0);
          for (let i = 0; i < 64; ++i) this.modelsView[i] = amps[i];

          const freqsOk = Array.isArray (freqs) && freqs.length === 64;
          for (let i = 0; i < 64; ++i)
            this.modelsView[64 + i] = freqsOk ? freqs[i] : 0;

          // FASE 11.3: si el slot trae capa 1 (bridge modelsState enriquecido
          // o parser local), la serializa y carga el slot COMPLETO en UNA
          // llamada (loadModel reemplaza el struct: en dos llamadas se
          // perderia la raiz). Sin capa, el camino v1 es identico.
          const layers = slotEntry.layers;
          const frames = layers?.frames;

          if (Number (layers?.layerCount) >= 2 && Array.isArray (frames)
              && frames.length > 0 && this.layersView) {
            const floatsPerFrame = 3 * 64 + 1;
            const count = Math.min (frames.length, 16);
            let serializado = true;

            this.layersView.fill (0);

            for (let f = 0; f < count; ++f) {
              const frame = frames[f];
              const base = f * floatsPerFrame;
              const frameAmps = frame?.amplitudes;
              const frameFreqs = frame?.frequencyOffsets;

              if (!Array.isArray (frameAmps) || frameAmps.length !== 64) {
                serializado = false;
                break;
              }

              for (let i = 0; i < 64; ++i) {
                this.layersView[base + i] = frameAmps[i];
                this.layersView[base + 64 + i] = Array.isArray (frameFreqs) && frameFreqs.length === 64 ? frameFreqs[i] : 0;
              }
              this.layersView[base + 128] = Number (frame?.frameF0) || 0;
            }

            const weightsBase = count * floatsPerFrame;
            const fw = layers.frameWeights;
            for (let f = 0; f < count; ++f)
              this.layersView[weightsBase + f] = Number (fw?.[f]) || 1.0;

            if (serializado) {
              this.module._neuronikLoadModelLayers (
                slot, this.engineType, this.modelsPtr, slotEntry.isValid ? 1 : 0,
                this.layersPtr, count,
                Number (layers.weight) || 1.0,
                this.layersPtr + 4 * weightsBase);
              break;
            }
          }

          this.module._neuronikLoadModel (
            slot, this.engineType, this.modelsPtr, slotEntry.isValid ? 1 : 0);
        }
        break;
      }

      case 'neuronik:engine': {
        if (this.ready) {
          this.module._neuronikSetEngine (message.index ?? 0);
          this.engineType = message.index ?? 0;

          // El motor nuevo nace con los defaults de SU struct: hay que devolverle
          // el ADSR que la pagina ya habia enviado, igual que los modelos y el
          // morph. Solo si la pagina lo mando alguna vez (si no, el motor acaba
          // de nacer bien y el reenvio no harian mas que pisar sus defaults).
          this.applyVeMirror();
        }
        break;
 }

      case 'neuronik:morph': {
        // Pad XY + eje temporal (2026-09-26): morphX/morphY/morphZ son
        // VoiceParams — neuronikSetVoiceMorph es el export que los hace
        // llegar al motor (read-modify-write de pendingVoiceParams). El
        // clampeo a [0,1] lo hace el propio export en la frontera.
        if (this.ready) {
          const fin = (value) => (Number.isFinite (value) ? value : 0);
          this.module._neuronikSetVoiceMorph (
            fin (message.x), fin (message.y), fin (message.z));

          // FASE 11.4: el VOLUMEN de las capas 1 y 2 (exports propios;
          // ausentes = sin cambio, el motor conserva lo ultimo).
          if (typeof message.z2 === 'number' || typeof message.z3 === 'number')
            this.module._neuronikSetVoiceLayerMorph (
              Math.min (1, Math.max (0, fin (message.z2))),
              Math.min (1, Math.max (0, fin (message.z3))));
        }
        break;
      }

      case 'neuronik:voice': {
        // ADSR de la voz (los ocho knobs de envolvente de la pagina). Snapshot
        // COMPLETO en cada envio —la pagina manda siempre los ocho—, asi que el
        // espejo se puede re-aplicar entero tras un cambio de motor sin
        // acordarse de que campos habian cambiado.
        // fields: [[fieldIndex, realMs | nivel 0..1], ...] (mapeo en la pagina)
        if (!this.ready) return;

        for (const [fieldIndex, value] of message.fields ?? []) {
          const byteOffset = this.veFieldByteOffsets[fieldIndex];
          if (byteOffset === undefined) continue;
          // POD TODO FLOAT: el indice de layout es el indice del Float32Array.
          const slot = byteOffset / 4;
          if (Number.isInteger (slot)) this.veMirror[slot] = Number (value);
        }

        this.veDirty = true;
        this.applyVeMirror();
        break;
      }

      case 'neuronik:midi': {
        if (!this.ready) return;
        if (message.kind === 'panic') {
          this.module._neuronikAllNotesOff();
        } else {
          this.pendingEvents.push (message);
          if (this.pendingEvents.length > 256) this.pendingEvents.shift();
        }
        break;
      }

      case 'neuronik:panic': {
        if (this.ready) this.module._neuronikAllNotesOff();
        break;
      }

      default:
        break;
    }
  }

  /** Drain the queued messages into a Runtime::Event block (byte layout in
   *  NeuronikWasmBridge.cpp: i32 type, i32 channel, i32 note, i32 value14,
   *  f32 value, i32 sampleOffset). */
  stageEvents() {
    if (this.pendingEvents.length === 0) return 0;

    const Module = this.module;
    const heap32 = Module.HEAP32;
    const heapF32 = Module.HEAPF32;
    const base = this.eventsPtr >> 2;
    const baseF = this.eventsPtr >> 2;
    const count = Math.min (this.pendingEvents.length, MAX_EVENTS_PER_BLOCK);

    for (let i = 0; i < count; ++i) {
      const message = this.pendingEvents[i];
      const record = base + i * 6;
      const recordF = baseF + i * 6;

      let type = EVENT_TYPE.NOTE_ON;
      let channel = 1;
      let note = 0;
      let value14 = 8192;
      let value = 0.0;

      if (message.kind === 'noteOn') {
        type = EVENT_TYPE.NOTE_ON;
        note = message.note ?? 60;
        value = Number (message.velocity ?? 0.8);
        value14 = Math.max (0, Math.min (16383, Math.round (value * 16383)));
      } else if (message.kind === 'noteOff') {
        type = EVENT_TYPE.NOTE_OFF;
        note = message.note ?? 60;
      } else if (message.kind === 'pitchBend') {
        type = EVENT_TYPE.PITCH_BEND;
        value = Math.max (-1, Math.min (1, Number (message.value ?? 0)));
        value14 = Math.max (0, Math.min (16383, Math.round ((value + 1) * 8191.5)));
      } else if (message.kind === 'pressure') {
        type = EVENT_TYPE.CHANNEL_PRESSURE;
        value = Math.max (0, Math.min (1, Number (message.value ?? 0)));
        value14 = Math.round (value * 16383);
      }

      heap32[record] = type;
      heap32[record + 1] = channel;
      heap32[record + 2] = note;
      heap32[record + 3] = value14;
      heapF32[recordF + 4] = value;
      heap32[record + 5] = 0; // sampleOffset: block boundary is fine for UI notes
    }

    this.pendingEvents.splice (0, count);
    return count;
  }

  process(inputs, outputs) {
    const output = outputs[0];
    const left = output[0];
    const right = output[1] ?? output[0];
    const numSamples = left.length;

    if (!this.ready || this.initError) {
      left.fill (0);
      if (right !== left) right.fill (0);
      return true;
    }

    // First block (or a larger quantum than allocated): (re)allocate views.
    if (numSamples > this.blockCap) {
      this.blockCap = numSamples;
      const Module = this.module;
      const blockBytes = numSamples * 4;
      Module._free (this.leftPtr);
      Module._free (this.rightPtr);
      this.leftPtr = Module._malloc (blockBytes);
      this.rightPtr = Module._malloc (blockBytes);
      this.leftView = null;
      this.rightView = null;
    }

    if (!this.leftView || this.blockSamples !== numSamples) {
      this.leftView = this.module.HEAPF32.subarray (this.leftPtr >> 2, (this.leftPtr >> 2) + numSamples);
      this.rightView = this.module.HEAPF32.subarray (this.rightPtr >> 2, (this.rightPtr >> 2) + numSamples);
      this.blockSamples = numSamples;
    }

    const numEvents = this.stageEvents();
    this.module._neuronikProcess (
      this.leftPtr, this.rightPtr, numSamples,
      numEvents > 0 ? this.eventsPtr : 0, numEvents);

    left.set (this.leftView);
    if (right !== left) right.set (this.rightView);

    if (this.blockCounter === 0) console.log('[worklet-dbg] process(): primer bloque ejecutado');
    // Lightweight telemetry for the page (~every 21 ms at 128/48k x 32).
    if ((++this.blockCounter & 31) === 0) {
      // ENV 1/ENV 2: los mismos niveles que el puente nativo manda como
      // telemetry envelopes [amp, filter] — la aguja de las curvas ADSR en modo
      // navegador. out params por punteros al heap del worklet (8 bytes fijos).
      this.module._neuronikGetEnvelopeLevels (this.envPtr, this.envPtr + 4);
      this.port.postMessage ({
        type: 'neuronik:meter',
        voices: this.module._neuronikNumActiveVoices(),
        lfo1: this.module._neuronikGetLfo (0),
        // Destino 28 (morphZ): contribucion CON SIGNO de la matriz — el anillo
        // exterior del pad la suma a la base para pintar la posicion real.
        morphZMod: this.module._neuronikGetMod (MORPH_Z_DESTINATION),
        // frame.envelopes-compatible: [amp, filter] (mismo orden del puente nativo).
        envelopes: [this.envView[0], this.envView[1]],
      });
    }

    return true;
  }
}

registerProcessor('neuronik-processor', NeuronikProcessor);
