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
 *  34..39 fx[0].{params[0..3], gain, mix}  (el hueco 1; los demas
 *    huecos van detras, y el contrato solo tiene este migrado)
 *
 * Los 34..39 los publica `neuronikModMatrixLayout`, que devuelve EL
 * TRAMO QUE VA DETRAS DE LOS ESCALARES con la numeracion desde cero.
 * Antes publicaba solo los doce de la matriz: los del bus se recogian
 * en el vector y se descartaban, y como se solapan, lo que se veia era
 * un numero que parecia completo. Ese export se usa para pedir la cola
 * sin los escalares delante; para escribir por indice de campo esta
 * tabla se construye con `neuronikGlobalParamsLayout` a secas (abajo,
 * en `allocateBuffers`, esta el porque de no concatenar las dos).
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
// La traduccion indice -> offset + clase vive en un modulo aparte porque el
// puente publica las dos mitades y aqui no hay nada que decidir: lo que se
// escribia con `INT_FIELDS` y `BPM_FIELD` escritas a mano son copias del
// struct, y una copia se queda vieja sin avisar (ver `gpMirror.js`).
import { readGpLayout, writeGpField as writeGpFieldIn } from './gpMirror.js';
// La traduccion de los eventos, la cola que los reparte por bloque y el
// aviso de los campos que el motor no publica viven en modulos aparte, con
// sus tests en vitest. Aqui no se puede probar nada: este fichero se
// registra como `AudioWorkletProcessor` al importarse y no exporta nada.
import { eventFromMessage } from './gpEvents.js';
import { createEventQueue, MAX_EVENTS_PER_BLOCK } from './gpEventsQueue.js';
import { createFieldAudit } from './gpFieldAudit.js';

// Los tipos de evento, el canal y la nota por defecto van en `gpEvents.js`:
// son el ABI del POD y ahora tienen sus tests al lado.

/** ModMatrix destination the page's z-ring visualises (engine enum: 28 = Morph Z). */
const MORPH_Z_DESTINATION = 28;
const EVENT_BYTES = 24; // Runtime::Event: 4xi32 + f32 + i32 (static_assert'd)
// El tope por bloque va en `gpEventsQueue.js`, con la cola que lo aplica.

// El bpm (unico double del espejo) y el resto de escalares: lo default que
// el motor ya trae de su struct y que la pagina no tiene que mandar.
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
    // La cola reparte los eventos por bloque (`gpEventsQueue.js`) y el
    // registro cuenta los campos que el motor no publica
    // (`gpFieldAudit.js`). Los dos son modulos con tests; lo que se queda
    // aqui es el volcado al heap, que necesita el `Module`.
    this.eventQueue = createEventQueue ({
      write: (index, event) => this.writeEventRecord (index, event),
    });
    this.fieldAudit = createFieldAudit (() => this.paramsFieldCount);

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
        // La firma del layout de ESTE binario, para que la pagina la
        // compare con la suya al arrancar y diga si el `.wasm` que
        // cargo es el que ella espera. Va en el ready y no en un
        // aviso aparte: es un dato del arranque, no un evento.
        layoutFingerprint: this.gpLayout.fingerprint,
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

    // UNA sola tabla de translation: la que publica
    // `neuronikGlobalParamsLayout`, que es el layout COMPLETO con los
    // indices que usa la pagina (0..21 escalares, 22..33 matriz, 34.. el
    // bus). Los offsets se guardan en BYTES, no como indice de vista, para
    // que las vistas f32 y f64 compartan un solo buffer.
    //
    // Y POR QUE NO SE LE CONCATENA `neuronikModMatrixLayout`: ese export
    // publica el MISMO tramo -matriz y bus- pero RENUMERADO desde cero, y
    // antes de que el puente publicase el tramo entero solo eran doce
    // campos (la matriz), de modo que anadirlos si tenia sentido. Metido
    // detras de un layout que ya los trae, el concat duplica la matriz y el
    // bus: el indice 58 apuntaria al primer campo de la matriz otra vez, y
    // `paramsFieldCount` diria 94 cuando el layout tiene 58. Se conserva el
    // export para quien pida la cola sin los escalares delante, pero quien
    // escribe por indice de campo usa esta tabla y solo esta.
    // Offset Y clase, los dos del puente y de la misma tabla.
    this.gpLayout = readGpLayout (Module);
    this.paramsFieldCount = this.gpLayout.fieldCount;
    this.reportedMissingCount = 0;

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

  /** Campo pedido por la pagina que el layout del motor NO publica. */
  noteMissingField(fieldIndex) {
    this.fieldAudit.note (fieldIndex);
  }

  /**
   * Avisa de los campos que el motor no publica, UNA vez por push y solo
   * si ha aparecido alguno nuevo. La cuenta y la regla estan en
   * `gpFieldAudit.js`; aqui solo se decide a quien se le avisa.
   */
  flushMissingFields() {
    const report = this.fieldAudit.takeReport ();
    if (report === null) return;

    console.warn ('[worklet] el espejo pide', report.missingCount,
      'campos que este motor no publica (de', report.fieldCount, '):', report.fields,
      '- o el .wasm de public/worklet es anterior al layout del puente,'
      + ' o la pagina escribe un campo que ya no existe');
    this.port.postMessage ({
      type: 'neuronik:layout',
      paramsFieldCount: report.fieldCount,
      missingFields: report.fields,
    });
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

  /**
   * Un campo del espejo, por indice de campo. De que vista se escribe lo
   * decide el puente (`gpMirror.js`), no este fichero.
   */
  writeGpField(fieldIndex, value) {
    const escrito = writeGpFieldIn (this.gpLayout, fieldIndex, value, {
      f32: this.gpF32,
      i32: this.gpI32,
      f64: this.gpF64,
    });

    // Un campo que el LAYOUT no publica no tiene donde escribir, y hasta
    // aqui se lo comia en silencio: con el .wasm viejo, que solo publicaba
    // 34 campos, los seis del bus no llegaban al motor y ningun knob se
    // quejaba. Ahora se cuentan y se avisa.
    if (!escrito) this.noteMissingField (fieldIndex);
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
        // UNA vez por push, no uno por campo (arriba, en
        // `flushMissingFields`, esta el porque).
        this.flushMissingFields ();
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
          this.eventQueue.push (message);
          // Tope de seguridad: si la pagina mandara sin parar (un bucle
          // mal escrito) la cola creceria sin fin en el hilo de audio, que
          // es donde mas caro sale. 256 eventos son mas de seis segundos
          // de entrada, y lo que se tira es lo mas viejo.
          while (this.eventQueue.length > 256) this.eventQueue.popOldest ();
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
  /**
   * Un registro del POD Event en el heap. Es la mitad de `stageEvents` que
   * NO se puede probar sin wasm: el reparto y la traduccion del mensaje
   * estan en `gpEventsQueue.js` y `gpEvents.js`.
   *
   * `value` es float y los otros cinco campos son enteros, asi que el
   * cuarto se escribe por la vista de float. Con una sola vista, el motor
   * leeria el patron de bits de IEEE donde espera un entero.
   */
  writeEventRecord (index, event) {
    const heap32 = this.module.HEAP32;
    const heapF32 = this.module.HEAPF32;
    const record = (this.eventsPtr >> 2) + index * 6;

    heap32[record + 0] = event.type;
    heap32[record + 1] = event.channel;
    heap32[record + 2] = event.note;
    heap32[record + 3] = event.value14;
    heapF32[record + 4] = event.value;
    heap32[record + 5] = event.sampleOffset;
  }

  /**
   * Un bloque: reparte los eventos pendientes y devuelve cuantos salieron.
   */
  stageEvents() {
    if (this.eventQueue.length === 0) return 0;

    return this.eventQueue.flush (eventFromMessage);
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
