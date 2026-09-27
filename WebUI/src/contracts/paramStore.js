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
  MODEL_SLOT_COUNT,
  displayableModelName,
  emptyLocalModelSlot,
  emptyLocalModels,
  parseModelText,
  readModelText,
} from '../audio/localModels.js';
import {
  forgetCachedModelText,
  readCachedModelTexts,
  saveCachedModelText,
} from '../audio/localModelCache.js';
import {
  PILOT_PARAMETER_IDS,
  contractSummary,
  defaultNormalizedState,
  describePilotControls,
  getDescriptor,
  toNormalized,
  validateNormalizedState,
} from './parameters.js';

/**
 * MODO LOCAL: la ruta de la matriz que hace BAILAR el anillo del pad sin host.
 *
 * Fuera del plugin el motor WASM nace con la matriz en el default del contrato
 * (dos rutas ENV y los slots 3/4 en Off), asi que `_neuronikGetMod(morphZ)` vale
 * 0 y el anillo se queda quieto: el pad mueve morphZ a mano, pero nada lo
 * MODULA. Esta ruta es la que el usuario veria si hubiera girado el LFO 2 a
 * MORPH Z en la MATRIZ, y la deja puesta de fabrica en local para que el arco
 * gire desde el primer SOUND ON.
 *
 * Slot 3 (no 1 ni 2): los dos primeros ya llevan las rutas de las envolventes
 * (ENV 1 -> Osc Level y ENV 2 -> Filter Cutoff), y pisarlas en local cambiaria el
 * sonido del legado. Solo se siembra si el slot sigue VIRGEN.
 *
 * LFO 2 a 1 Hz con profundidad 1 es el default del contrato: la ruta es lo unico
 * que falta para el arco. El nombre de la fuente y del destino son LABELS: se
 * resuelven contra la tabla del contrato (choices), nunca con un indice escrito
 * a mano, para que crecer la tabla no re-apunte la ruta.
 */
const LOCAL_MORPH_Z_ROUTE = {
  slot: 3,
  source: 'LFO 2',
  destination: 'Morph Z',
  amount: 1.0,   // -1..1: el barrido entero, la misma profundidad que pinea el E2E ZRING
};

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
    models: null,   // modo local: `emptyLocalModels()` + la memoria local (app.js)
    // Last model load that did NOT happen (bridge modelError), or null. The view
    // shows it instead of letting a click fail in silence.
    modelError: null,
    // EL AVISO de un gesto que SI ocurrio: `{ slot, detail, tone }` o null. Vive
    // al lado de `modelError` porque se pinta en la MISMA linea de la ficha
    // RANURAS (una linea, un mensaje: si hay fallo manda el fallo, ver el paint
    // de ui/modelSlots.js). `tone: 'ok'` es un acierto ('se olvido la ranura') y
    // `tone: 'warn'` un acierto a medias (la sesion la vacio, la memoria del
    // navegador no). No se confunde con un error: vaciar una ranura no es fallar,
    // y `modelError` sigue siendo lo que el host o el parser no pudieron hacer.
    modelNotice: null,
    // Tabla de mapeos MIDI CC (bridge midiCcState): [{ paramId, cc }] con UNA
    // entrada por parametro aprendible (cc = -1 cuando no esta mapeado). La
    // dueña es el motor (MidiMappingManager); la pagina la ensena y la edita
    // con las acciones midiCcLearn/Clear/Reset — en local no hay tabla: null.
    midiCcMappings: null,
    // MODO LOCAL: el camino de carga local esta montado (input de fichero).
    // La ficha RANURAS lo usa para habilitar CARGAR sin host.
    localModelReady: false,
    // EL CONMUTADOR de la ruta local del pad (ficha MATRIZ): si esa ruta esta
    // activa y que LFO la mueve. El valor inicial es la SIEMBRA MISMA
    // (LOCAL_MORPH_Z_ROUTE), leida del contrato y no escrita otra vez: arrancar
    // sin la ruta es una decision del usuario, arrancarla es la de fabrica.
    // `sources` la lista la tabla de fuentes (LFO 1, LFO 2...): la vista la
    // pinta, no la decide.
    localMorphRoute: {
      enabled: true,
      source: LOCAL_MORPH_Z_ROUTE.source,
      sources: [],
    },
  };

  let transport = null;
  let draggingId = null;
  let started = false;

  // Las fuentes del conmutador salen del contrato (la tabla de `mod3Source` ya
  // esta; el store no depende de la ficha que lo pinta). Se resuelven aqui y no
  // en el literal de arriba porque necesitan `state.parameters`.
  state = {
    ...state,
    localMorphRoute: { ...state.localMorphRoute, sources: localRouteSources() },
    contractErrors: validateNormalizedState(ids, state.parameters),
  };

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

  // EDITS DE USUARIO (pub/sub fuera del estado): pushParameter es la UNICA
  // puerta por la que pasa un edit de usuario (celdas, pad, aro, LCD) — los
  // cambios nativos (host, automatizacion) entran por onParameterChanged y
  // NUNCA disparan esto. El LCD superior lo usa para su preview de parametro.
  const userEditListeners = new Set();

  /**
   * Subscribe to USER edits: (id, normalized, phase) por cada pushParameter.
   * @returns {() => void} unsubscribe.
   */
  function onUserEdit(listener) {
    userEditListeners.add(listener);
    return () => userEditListeners.delete(listener);
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

    for (const listener of userEditListeners) listener(id, normalized, phase);
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
    setState({ modelError: null, modelNotice: null });
    transport?.sendLoadModel(slot);
    return true;
  }

  /**
   * MODO LOCAL: el usuario ya eligio el fichero — parsearlo, meterlo en el
   * estado `models` (lo que pinta la ficha RANURAS), dejar que el paint
   * del motor lo lleve al worklet por `neuronik:models` y RECORDARLO para la
   * proxima recarga. El error del parser se pinta en el MISMO sitio que el
   * modelError del host.
   *
   * Se lee el TEXTO crudo y se parsea aparte (no `readModelFile`, que hace los
   * dos pasos de una): el texto es justo lo que guarda la memoria local, asi que
   * una ranura recordada vuelve a cruzar el MISMO parser que un fichero elegido
   * a mano en vez de un objeto derivado que habria que versionar.
   */
  async function loadLocalModel(file, slot) {
    try {
      const text = await readModelText(file);
      const parsed = parseModelText(text);
      const models = Array.isArray(state.models) ? [...state.models] : null;

      if (!models) return false;

      models[slot] = { ...parsed, slot };
      setState({ models, modelError: null, modelNotice: null });
      rememberLocalModel(slot, text);
      return true;
    } catch (error) {
      setState({ modelError: { slot, detail: String(error?.message ?? error) }, modelNotice: null });
      return false;
    }
  }

  /**
   * Memoria local (best-effort): la ranura YA esta cargada, asi que no poder
   * guardarla no deshace la carga — pero se dice en consola (mismo trato que los
   * avisos de catalogo) en vez de fingir que la ranura se recordara.
   */
  function rememberLocalModel(slot, text) {
    if (saveCachedModelText(slot, text)) return true;

    console.warn(`modelo del slot ${slot}: no se pudo recordar (sin localStorage o sin cupo); `
      + 'la ranura volvera vacia al recargar');

    return false;
  }

  /**
   * MODO LOCAL: OLVIDAR una ranura — la deja EMPTY y saca su texto de la memoria
   * del navegador, para que un F5 no la devuelva. El boton vive en la ficha
   * RANURAS y el aviso, en la misma linea de estado que un fallo de carga.
   *
   * Que la verdad del vaciado la tenga el ESTADO y no solo la memoria: se
   * reescribe `models[slot]` con la ranura vacia de FABRICA
   * (`emptyLocalModelSlot`), no se borra la entrada. Asi el motor recibe la
   * entrada EMPTY por el canal `neuronik:models` que ya usa la carga (el
   * worklet llama a `neuronikLoadModel` con `isValid: 0`) y el timbre se
   * descarga en el MISMO gesto, no en el siguiente F5.
   *
   * El AVISO distingue los dos desenlaces que se parecen y no son lo mismo:
   * olvidada de verdad (no sobrevive al F5) y olvidada a medias (la sesion ya la
   * vacio, pero el almacen no la solto y volvera). El segundo es `tone: 'warn'`,
   * no un error: el gesto funciono, solo que el navegador no cooperate.
   *
   * Con host no hace nada: las ranuras son del PRESET (su `modelPath<slot>`) y el
   * motor las volveria a cargar al recargar el proyecto, asi que vaciarlas desde
   * la pagina seria un gesto que no se sostiene. Una ranura vacia tampoco se
   * olvida (no hay nada que olvidar: no se finge un gesto).
   *
   * @returns {boolean} true si vacio la ranura.
   */
  function forgetLocalModel(slot) {
    if (state.bridgeAvailable) return false;
    if (!Number.isInteger(slot) || slot < 0 || slot >= MODEL_SLOT_COUNT) return false;

    const models = Array.isArray(state.models) && state.models.length === MODEL_SLOT_COUNT
      ? [...state.models]
      : null;

    if (!models) return false;
    if (displayableModelName(models[slot]) === null) return false;

    models[slot] = emptyLocalModelSlot(slot);

    const { forgotten } = forgetCachedModelText(slot);

    setState({
      models,
      modelError: null,
      modelNotice: {
        slot,
        tone: forgotten ? 'ok' : 'warn',
        // El `detail` NO lleva la letra de la ranura: el `slot` va al lado y la
        // vista lo pone en el title (mismo trato que el `modelError` del host).
        detail: forgotten
          ? 'olvidada: la ranura vuelve a EMPTY y no sobrevive al F5'
          : 'vaciada en esta sesion, pero el navegador no la deja olvidar: al recargar volvera',
      },
    });

    return true;
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

  /**
   * MODO LOCAL: repuebla las ranuras con los modelos que la pagina RECUERDA.
   *
   * Al recargar, en el navegador no hay `modelsState` (no hay host) ni rutas de
   * preset que rellenen las ranuras: lo unico que sobrevive a un F5 es esta
   * memoria. Se recupera el TEXTO guardado y se parsea con el MISMO lector del
   * input, de modo que una ranura recordada es indistinguible de una recien
   * cargada — el pintado de la ficha ni se entera. Con host no se toca nada:
   * manda `modelsState` (el preset).
   *
   * Una entrada que ya no parsea (almacen corrupto, formato de otra version) se
   * DESCARTA en vez de romper el arranque, y el motivo se pinta con el MISMO
   * `modelError` de siempre. Nada de silencios: una ranura que desaparece sola
   * es peor que un aviso.
   *
   * @returns {number} cuantas ranuras se recuperaron.
   */
  function restoreLocalModels() {
    if (state.bridgeAvailable) return 0;

    const remembered = readCachedModelTexts();
    const slots = Object.keys(remembered).map(Number).sort((a, b) => a - b);

    if (slots.length === 0) return 0;

    const base = Array.isArray(state.models) && state.models.length === MODEL_SLOT_COUNT
      ? [...state.models]
      : emptyLocalModels();

    let restored = 0;
    let failure = null;

    for (const slot of slots) {
      try {
        base[slot] = { ...parseModelText(remembered[slot]), slot };
        restored += 1;
      } catch (error) {
        // El primer fallo es el que se ensena: la ficha tiene UNA linea de estado.
        failure ??= {
          slot,
          detail: `el modelo recordado no se pudo releer (${String(error?.message ?? error)})`,
        };
      }
    }

    setState({ models: base, modelError: failure, modelNotice: null });
    return restored;
  }

  /** Los tres ids de la fila que gobierna el conmutador, o null si el store no los posee. */
  function localRouteIds() {
    const { slot } = LOCAL_MORPH_Z_ROUTE;
    // `routeIds`, no `ids`: el store ya tiene un `ids` (los que posee) y
    // sombrearlo aqui seria pasar un objeto donde se espera su array.
    const routeIds = {
      sourceId: `mod${slot}Source`,
      destinationId: `mod${slot}Destination`,
      amountId: `mod${slot}Amount`,
    };

    // Un id que el store no posee acabaria en contractErrors: no se escribe.
    return Object.values(routeIds).every((id) => id in state.parameters)
      ? routeIds
      : null;
  }

  /**
   * Un label de la tabla de un choice, en NORMALIZADO. -1 si la tabla ya no
   * lleva ese label: no se inventa un indice escrito a mano.
   */
  function choiceNormalizedOf(descriptor, label) {
    const index = descriptor?.choices?.indexOf(label) ?? -1;

    if (index < 0) return -1;

    return descriptor.choices.length > 1 ? index / (descriptor.choices.length - 1) : 0;
  }

  /**
   * Los labels de LFO que la tabla de fuentes ofrece (LFO 1, LFO 2...): la lista
   * del conmutador sale del CONTRATO, no de una constante escrita a mano, asi que
   * un LFO nuevo aparece solo. Vacia si el store no posee la fila.
   */
  function localRouteSources() {
    const routeIds = localRouteIds();

    if (!routeIds) return [];

    const choices = getDescriptor(routeIds.sourceId)?.choices ?? [];

    return choices.filter((label) => /^LFO \d/.test(label));
  }

  /**
   * Los PARAMETROS resultantes de escribir la fila del conmutador: la ruta
   * local del pad, o la fila VIRGEN. VIRGEN son los defaults del PROPIO
   * contrato de esos tres ids, que es exactamente como estaba antes de la
   * siembra (fuente y destino en Off). Devuelve null si no se puede escribir
   * (store sin la fila, o una tabla que ya no lleva el label): quien llama
   * decide el setState, para que un gesto del conmutador se aplique con UNO
   * solo (parametros y estado juntos, sin un frame con la fila cambiada y el
   * conmutador sin su nueva verdad).
   */
  function localRouteParameters({ source, virgin = false }) {
    const routeIds = localRouteIds();

    if (!routeIds) return null;

    const { destination, amount } = LOCAL_MORPH_Z_ROUTE;
    let parameters = state.parameters;

    if (virgin) {
      parameters = {
        ...parameters,
        ...defaultNormalizedState([
          routeIds.sourceId,
          routeIds.destinationId,
          routeIds.amountId,
        ]),
      };
    } else {
      const sourceNormalized = choiceNormalizedOf(getDescriptor(routeIds.sourceId), source);
      const destinationNormalized = choiceNormalizedOf(
        getDescriptor(routeIds.destinationId),
        destination,
      );

      // Tabla que ya no lleva ese label, o que llevaria la ruta a Off (indice 0
      // por contrato): no se inventa un indice y no se deja la fila a medias.
      if (sourceNormalized <= 0 || destinationNormalized <= 0) return null;

      parameters = {
        ...parameters,
        [routeIds.sourceId]: sourceNormalized,
        [routeIds.destinationId]: destinationNormalized,
        [routeIds.amountId]: toNormalized(getDescriptor(routeIds.amountId), amount),
      };
    }

    return parameters;
  }

  /**
   * MODO LOCAL: encamina la ruta por defecto a Morph Z (ver LOCAL_MORPH_Z_ROUTE)
   * para que el anillo del pad baile con el LFO sin host. Solo escribe si el slot
   * sigue VIRGEN (fuente y destino en Off) y el store POSEE los tres ids: con host
   * manda el APVTS, y una ruta que el usuario ya toco no se pisa. Con el
   * conmutador en OFF no siembra: apagar es no sembrar.
   *
   * @returns {boolean} true si sembro la ruta.
   */
  function seedLocalMorphZRoute() {
    if (state.bridgeAvailable) return false;
    if (!state.localMorphRoute.enabled) return false;

    const routeIds = localRouteIds();

    if (!routeIds) return false;

    // La ruta ya tiene dueno: no se toca (ni la del usuario ni una del host).
    if (state.parameters[routeIds.sourceId] !== 0
        || state.parameters[routeIds.destinationId] !== 0)
      return false;

    const parameters = localRouteParameters({ source: state.localMorphRoute.source });

    if (!parameters) return false;

    setState({
      parameters,
      contractErrors: validateNormalizedState(ids, parameters),
    });

    return true;
  }

  /**
   * EL CONMUTADOR de la ficha MATRIZ: activa o desactiva la ruta local del pad y
   * dice QUE LFO la mueve. Es un gesto EXPLICITO del usuario, asi que escribe la
   * fila aunque la hubiera tocado antes (a diferencia de la siembra, que solo
   * pisa slots virgenes). Con OFF la fila vuelve a VIRGEN. Con host no hace
   * nada: la matriz es del APVTS. Un `source` que la tabla de fuentes no lista
   * se rechaza entero, sin dejar la fila a medias.
   *
   * @param {{enabled?: boolean, source?: string}} [next]
   * @returns {boolean} true si escribio la fila.
   */
  function setLocalMorphRoute(next = {}) {
    if (state.bridgeAvailable) return false;

    const enabled = next.enabled === undefined
      ? state.localMorphRoute.enabled
      : Boolean(next.enabled);
    const source = next.source === undefined ? state.localMorphRoute.source : next.source;

    if (enabled && !localRouteSources().includes(source)) return false;

    const parameters = localRouteParameters({ source, virgin: !enabled });

    if (!parameters) return false;

    setState({
      parameters,
      localMorphRoute: { ...state.localMorphRoute, enabled, source },
      contractErrors: validateNormalizedState(ids, parameters),
    });

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

  // ---- MIDI CC (el menu MIDI CONTROL del LCD): acciones de ESTADO sobre la
  // tabla del motor. El host responde con un midiCcState fresco, que repinta
  // el menu; sin host no hay tabla que editar y las acciones son no-op.
  function sendMidiCcLearn(paramId) {
    transport?.sendMidiCcLearn(paramId);
  }

  function sendMidiCcClear(paramId) {
    transport?.sendMidiCcClear(paramId);
  }

  function sendMidiCcReset() {
    transport?.sendMidiCcReset();
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
        setState({
          models: Array.isArray(slots) ? slots : null,
          modelError: null,
          modelNotice: null,
        });
      },

      onModelError(modelError) {
        setState({ modelError });
      },

      onMidiCc(mappings) {
        setState({ midiCcMappings: Array.isArray(mappings) ? mappings : null });
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
    /** Edits de usuario (pushParameter): el preview del LCD vive aqui. */
    onUserEdit,
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
    forgetLocalModel,
    seedLocalModels,
    restoreLocalModels,
    seedLocalMorphZRoute,
    setLocalMorphRoute,
    setLocalModelReady,
    sendMidiNoteOn,
    sendMidiNoteOff,
    sendMidiPitchBend,
    sendMidiModWheel,
    sendMidiPanic,
    sendMidiCcLearn,
    sendMidiCcClear,
    sendMidiCcReset,
    sendMidiMessage,
  };
}
