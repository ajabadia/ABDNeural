/**
 * El LCD superior (primera fila del lienzo, bajo la nav-bar): la migración de
 * lo que el interface C++ tenía y la WebUI aún no — LcdDisplay.h (pantalla
 * 16x2 con scroll y preview) + LcdMenuManager.h (menú jerárquico dependiente
 * del engineType) + el D-pad con hold-repeat del Editor — retirados en c811b75
 * y vivos en git. La MECÁNICA es del paquete compartido (lcdPanel: pantalla +
 * máquina + botones MENU/OK/‹›^v con hold-repeat 400/120 ms); los DATOS son de
 * este synth:
 *
 *   - el ÁRBOL replica el LcdMenuManager.h (GLOBAL/RESONATOR/FILTER/EFFECTS/
 *     MIDI CONTROL, con las ramas RESONATOR y MIDI CONTROL dependiendo del
 *     engineType: Neuronik muestra morph/inharm/rough, Neurotik el par excite —
 *     la misma regla del hardware original);
 *   - el REPOSO muestra preset y estado (como updateLcdDefault del Editor:
 *     "PATCH: <nombre>" / "NEURONiK READY");
 *   - onEdit conduce el PARÁMETRO REAL del store (pushParameter, fase 'end':
 *     el D-pad es un gesto de hardware, no un arrastre); los pasos discretos
 *     y el rango salen del contrato generado — el D-pad no inventa nada;
 *   - onAction ejecuta RESET_MIDI del árbol: la tabla de mapeos CC vive en el
 *     MOTOR (MidiMappingManager) y viaja por el bridge — la página la enseña y
 *     la edita, nunca la posee (midiCcState + midiCcLearn/Clear/Reset);
 *   - los items 'cc' del árbol MIDI CONTROL SON FUNCIONALES: su EDIT no mueve
 *     el parámetro, GESTIONA SU CC — OK arma el learn (el próximo CC del motor
 *     gana), ‹› asigna/desasigna el CC a mano y el valor en EDIT muestra el
 *     CC actual ("CC 74" / "CC --" sin mapeo).
 */

import { createLcdPanel } from '@abdsynths/shared/components';
import { describeControl } from '../contracts/parameters.js';
import { choiceIndexFromNormalized, displayText, realFromNormalized } from '../contracts/paramValue.js';
// El CATALOGO del bus, el mismo que lee el cajon de efectos. Es un fichero
// GENERADO y versionado, y traerlo aqui es lo que permite que el nombre del
// mando diga el efecto que hay PUESTO y no el que habia cuando se escribio
// el arbol. Importarlo por el `generated/` y no por el contrato de
// ABDSharedAssets es lo mismo que hace `ui/fxModules.js`: el build tiene que
// funcionar en un clon sin el repositorio hermano al lado.
import { FX_CATALOG } from '../../generated/fx-catalog.generated.js';

/**
 * EL NOMBRE DE LOS MANDOS DEL HUECO 1 VIENE DEL ESTADO, no de una lista escrita
 * =========================================================================
 * `fx1Param1` es "el parametro 1 del efecto PUESTO": el drive de una saturacion,
 * el rate de un chorus, el decay de un Schroeder. Un arbol que lo llamara
 * SATURATION estaria mintiendo en cuanto la ficha cambiara el efecto, y uno que
 * lo llamara FX 1 P1 no diria nada. Los dos eran Menu ESTATICO, que es lo unico
 * que se puede escribir sin mirar el estado, asi que la entrada de la ronda
 * anterior quedo en FX 1 P1 con un comentario explicando por que no podia decir
 * mas. Esta es la parte que hacia falta: el arbol se deriva de la fila del
 * catalogo, y si el hueco no lleva saturacion el nombre cambia con el.
 */
const FX1_TYPE = 'fx1Type';

/** La fila del catalogo que hay puesta en el hueco 1 (0 = bypass). */
function filaDelHuecoUno (normalized) {
  const control = describeControl(FX1_TYPE);
  const index = normalized === undefined
    ? (control?.defaultValue ?? 0)
    : choiceIndexFromNormalized(control, normalized);

  return FX_CATALOG.effects[index] ?? FX_CATALOG.effects[0];
}

/**
 * Tres letras del efecto. El LCD es de 16 caracteres justos y el nombre
 * entero mas el valor no caben ("SATURATION DRIVE" ocupa la pantalla
 * sola); con tres letras sigue siendo inequivoco ("SAT DRIVE", "CHO RATE",
 * "JUN WEAR") porque las tres letras ya dicen cual es.
 */
const tresLetras = (nombre) =>
  String(nombre ?? '').trim().split(/\s+/)[0].slice(0, 3).toUpperCase();

/**
 * Los mandos que DECLARA el efecto puesto, con SU nombre.
 *
 * CUANTOS SON, LOS DICE EL CATALOGO y no el hueco: un hueco publica cuatro
 * posiciones y una fila declara las que usa (la saturacion declara UNA y las
 * otras tres quedan sin nombre porque no hacen nada). Es la misma regla que
 * aplica el cajon, y por eso el menu no puede llevar un numero fijo de entradas.
 */
function mandosDelHueco (efecto) {
  const declarados = efecto?.params ?? [];

  return declarados.map((param, index) => ({
    label: `${tresLetras(efecto.displayName)} ${String(param.name).toUpperCase()}`,
    paramId: `fx1Param${index + 1}`,
  }));
}

/** Los tres mandos planos que el hueco NO se llevo, y que siguen vivos. */
const MANDOS_FLATOS = [
  { label: 'CHORUS MIX', paramId: 'fxChorusMix' },
  { label: 'DELAY TIME', paramId: 'fxDelayTime' },
  { label: 'REVERB MIX', paramId: 'fxReverbMix' },
];

/** Rama RESONATOR según engineType (0 = Neuronik con morph, 1 = Neurotik). */
const RESONATOR_NEURONIK = [
  { label: 'UNISON DETUNE', paramId: 'unisonDetune' },
  { label: 'MORPH X', paramId: 'morphX' },
  { label: 'MORPH Y', paramId: 'morphY' },
  { label: 'INHARMONICITY', paramId: 'oscInharmonicity' },
  { label: 'ROUGHNESS', paramId: 'oscRoughness' },
  { label: 'ODD/EVEN BAL', paramId: 'resonatorParity' },
  { label: 'SPECTRAL SHIFT', paramId: 'resonatorShift' },
  { label: 'HARM ROLLOFF', paramId: 'resonatorRolloff' },
];

const RESONATOR_NEUROTIK = [
  { label: 'UNISON DETUNE', paramId: 'unisonDetune' },
  { label: 'EXCITE NOISE', paramId: 'oscExciteNoise' },
  { label: 'EXCITE COLOR', paramId: 'excitationColor' },
  { label: 'IMPULSE MIX', paramId: 'impulseMix' },
  { label: 'RES BANK RES', paramId: 'resonatorRes' },
];

const MIDI_CC_NEURONIK = [
  { label: 'CC CUTOFF', paramId: 'filterCutoff', type: 'cc' },
  { label: 'CC RESON', paramId: 'filterRes', type: 'cc' },
  { label: 'CC OSC LVL', paramId: 'oscLevel', type: 'cc' },
  { label: 'CC ATTACK', paramId: 'envAttack', type: 'cc' },
  { label: 'CC MORPH X', paramId: 'morphX', type: 'cc' },
  { label: 'CC MORPH Y', paramId: 'morphY', type: 'cc' },
  { label: 'CC INHARM', paramId: 'oscInharmonicity', type: 'cc' },
  { label: 'CC ROUGH', paramId: 'oscRoughness', type: 'cc' },
  { label: 'RESET ALL', paramId: 'RESET_MIDI', type: 'action' },
];

const MIDI_CC_BASE = [
  { label: 'CC CUTOFF', paramId: 'filterCutoff', type: 'cc' },
  { label: 'CC RESON', paramId: 'filterRes', type: 'cc' },
  { label: 'CC OSC LVL', paramId: 'oscLevel', type: 'cc' },
  { label: 'CC ATTACK', paramId: 'envAttack', type: 'cc' },
  { label: 'RESET ALL', paramId: 'RESET_MIDI', type: 'action' },
];

/**
 * El árbol del synth, dependiente del engineType — la regla del LcdMenuManager
 * original, ahora contra el contrato de esta página.
 */
export function buildMenuTree(engineType = 0, efecto = filaDelHuecoUno()) {
  const isNeuronik = engineType === 0;

  return [
    { label: 'GLOBAL', sub: [
      { label: 'MASTER VOL', paramId: 'masterLevel' },
      { label: 'ENGINE SELECT', paramId: 'engineType' },
      { label: 'MASTER BPM', paramId: 'masterBPM' },
      { label: 'MIDI CH', paramId: 'midiChannel' },
      { label: 'VEL CURVE', paramId: 'velocityCurve' },
    ] },
    { label: 'RESONATOR', sub: isNeuronik ? RESONATOR_NEURONIK : RESONATOR_NEUROTIK },
    { label: 'FILTER', sub: [
      { label: 'CUTOFF', paramId: 'filterCutoff' },
      { label: 'RESONANCE', paramId: 'filterRes' },
      // "ENV AMOUNT" era filterEnvAmount (retirado 2026-09-26): la profundidad
      // de la ruta ENV 2 -> Filter Cutoff es el amount de la MATRIZ.
    ] },
    // EFFECTOS: los mandos del hueco 1 con el nombre del efecto PUESTO (arriba,
    // `mandosDelHueco`), y debajo los tres mandos planos que el hueco no se
    // llevo y que siguen vivos en el layout.
    //
    // NOTA PARA QUIEN AÑADA UN ID AQUI: los del hueco se COMPONEN
    // (`fx1Param${n}`), asi que el escaner de ids por fuente no los ve. Los
    // comprueba `contractIds.test.js`, que recorre el arbol de verdad.
    { label: 'EFFECTS', sub: [...mandosDelHueco(efecto), ...MANDOS_FLATOS] },
    { label: 'MIDI CONTROL', sub: isNeuronik ? MIDI_CC_NEURONIK : MIDI_CC_BASE },
  ];
}

/**
 * @param {object} options
 * @param {object} options.store  el store de la página (getState/pushParameter):
 *   las mismas manos que el panel — el LCD conduce los parámetros REALES.
 * @param {() => number} [options.engineType]  engineType ACTUAL (el árbol
 *   depende de él); sin ella, Neuronik. El EFECTO del hueco 1 no se pasa:
 *   se lee del propio store, que es quien lo tiene.
 * @returns {{ element: HTMLElement, panel: object, paint: Function, rebuild:
 *   Function, destroy: Function }}
 */
export function createLcdTop({ store, engineType = null }) {
  const element = document.createElement('div');
  element.className = 'lcd-top';

  const parameters = () => store.getState().parameters;
  const controlOf = (paramId) => describeControl(paramId);
  const engineNow = () => (typeof engineType === 'function'
    ? Math.round(parameters().engineType ?? 0)
    : 0);
  // El efecto PUESTO en el hueco 1, para que los nombres del menu vayan con el.
  // Del estado y no de un argumento: la ficha es quien lo cambia, y el menu
  // tiene que enterarse sin que nadie le avise.
  const efectoNow = () => filaDelHuecoUno(parameters()[FX1_TYPE]);

  // Paso del D-pad por parámetro: la regla del original (discretos = 1/(n-1),
  // continuos 0.01 — el hold-repeat del panel da la aceleración).
  const stepOf = (control) => {
    if (control.kind === 'choice' || control.kind === 'bool') {
      return 1 / Math.max(control.options.length - 1, 1);
    }
    return 0.01;
  };

  // Los hooks y las líneas de reposo van por REFERENCIA: rebuild() recrea el
  // panel con el árbol nuevo del engineType reusando los mismos closures.
  const hooksRef = {};
  const idleRef = {};
  const editValueRef = {};

  hooksRef.onEdit = (paramId, dir, state) => {
    // EDIT de un item 'cc' (MIDI CONTROL): no mueve el parametro — GESTIONA SU
    // CC. El item editando viaja en el estado de la maquina (machine.editing).
    // La tabla vive en el motor: con host, sendMidiCc* + midiCcState de vuelta;
    // en local no hay tabla (midiCcMappings null) y el EDIT es honesto: nada.
    if (state?.editing?.type === 'cc') {
      const item = state.editing;

      if (dir > 0) store.sendMidiCcLearn?.(item.paramId);      // OK/encoder+: armar learn
      else if (dir < 0) store.sendMidiCcClear?.(item.paramId); // encoder-: desmapear

      return;
    }

    const control = controlOf(paramId);

    if (!control) return;

    const current = parameters()[paramId] ?? 0;
    const next = Math.min(1, Math.max(0, current + stepOf(control) * dir));

    if (next !== current) store.pushParameter(paramId, next, 'end');
  };

  hooksRef.onAction = (item) => {
    // RESET_MIDI: la tabla es del MOTOR — el reset viaja como acción y la
    // respuesta (midiCcState fresco) repinta el menú. En local, no-op honesto.
    if (item?.paramId === 'RESET_MIDI') store.sendMidiCcReset?.();
  };

  hooksRef.onPreview = (item) => {
    const control = controlOf(item?.paramId);

    if (!control) return;

    const value = parameters()[item.paramId] ?? 0;

    panel.screen.preview(0, item.label.toUpperCase(), {});
    panel.screen.preview(1, `> ${displayText(control, realFromNormalized(control, value))}`, {});
  };

  /**
   * PREVIEW de un parametro en la pantalla (el nombre del ID real, no un item
   * del menu): lo llama el cable de EDITS DE USUARIO (store.onUserEdit) para
   * que girar CUALQUIER control — celda, pad, aro, knob de cajon — muestre su
   * valor en el LCD, como el encoder de hardware del original. Transitorio:
   * la pantalla vuelve sola al reposo (previewMs del paquete).
   */
  function showParameterPreview (paramId, normalized) {
    const control = controlOf(paramId);

    if (!control) return;

    const value = normalized ?? parameters()[paramId] ?? 0;

    // En EDIT del menu el LCD esta contando otra cosa (el valor del item que
    // se edita): no se le pisa — el reposo/preview de parametro vuelve al
    // cerrar la edicion.
    if (panel.machine.state === 'edit') return;

    panel.screen.preview(0, control.label.toUpperCase(), {});
    panel.screen.preview(1, `> ${displayText(control, realFromNormalized(control, value))}`, {});
  }

  // Reposo del hardware original (updateLcdDefault): preset + estado.
  idleRef.idle = () => {
    const preset = store.getState().presetState?.current ?? '';
    const isNeuronik = engineNow() === 0;

    return [
      preset ? `PATCH: ${preset.toUpperCase()}` : 'NEURONiK',
      isNeuronik ? 'NEURONIK READY' : 'NEUROTIK READY',
    ];
  };

  // El valor en EDIT: el snapshot manda (un edit nativo se ve en el LCD). Los
  // items 'cc' muestran SU CC de la tabla del motor ("CC 74", "CC --" sin mapeo).
  editValueRef.editValue = (item) => {
    if (item?.type === 'cc' && item.paramId !== 'RESET_MIDI') {
      const entry = Array.isArray(store.getState().midiCcMappings)
        ? store.getState().midiCcMappings.find((m) => m?.paramId === item.paramId)
        : null;

      return entry && entry.cc >= 0 ? `CC ${entry.cc}` : 'CC --';
    }

    const control = controlOf(item?.paramId);

    if (!control) return '';

    return displayText(control, realFromNormalized(control, parameters()[control.id] ?? 0));
  };

  let panel = createLcdPanel(element, {
    menu: buildMenuTree(engineNow(), efectoNow()),
    hooks: hooksRef,
    lines: 2,
    widthChars: 16,
    idle: () => idleRef.idle(),
    editValue: (item) => editValueRef.editValue(item),
  });

  return {
    element,
    panel,

    /**
     * Preview transitorio de UN parametro (label + valor formateado del
     * contrato). El cable de edits de usuario (app.js: store.onUserEdit) lo
     * llama; tambien es usable a mano (p.ej. un gesto propio de una vista).
     */
    showParameterPreview,

    /**
     * FIRMA DEL ÁRBOL: todo lo de lo que depende el menú, en una
     * cadena. El árbol se deriva del motor y del efecto del hueco 1, así
     * que eso es lo único que hay que mirar para saber si el menú sigue
     * siendo el mismo.
     *
     * POR QUÉ UNA CADENA y no un árbol: comparar menús es comparar
     * objetos recién hechos, que nunca son los mismos, y el que llama
     * (app.js) no tiene que saber CÓMO se deriva el nombre de un mando. Le
     * basta con guardar esto y reconstruir cuando cambie.
     */
    menuSignature: () => `${engineNow()}#${efectoNow().id}`,
    /**
     * El árbol depende del engineType Y del efecto puesto en el hueco 1: un
     * cambio de cualquiera de los dos reconstruye el menú.
     * La máquina es pura y el panel no expone swap de árbol: recrear la
     * composición en el MISMO elemento (barata: pantalla + 6 botones) con los
     * mismos closures por referencia.
     */
    rebuild() {
      panel.destroy();
      element.textContent = '';

      panel = createLcdPanel(element, {
        menu: buildMenuTree(engineNow(), efectoNow()),
        hooks: hooksRef,
        lines: 2,
        widthChars: 16,
        idle: () => idleRef.idle(),
        editValue: (item) => editValueRef.editValue(item),
      });

      this.panel = panel;
    },

    paint() {
      // Repinta la pantalla con idle()/editValue() actuales: un snapshot nuevo
      // (preset cargado, edit nativo) se refleja aquí.
      panel.repaint();
    },

    destroy() {
      panel.destroy();
      element.textContent = '';
    },
  };
}
