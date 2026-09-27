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
 *   - onAction ejecuta RESET_MIDI del árbol (hoy es un no-op honesto: el
 *     manager de mapeos MIDI CC no ha viajado aún a la WebUI);
 *   - los items 'cc' existen en el árbol por fidelidad con el original, pero su
 *     aprendizaje vive en el host: se muestran, no se editan aquí.
 */

import { createLcdPanel } from '@abdsynths/shared/components';
import { describeControl } from '../contracts/parameters.js';
import { displayText, realFromNormalized } from '../contracts/paramValue.js';

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
export function buildMenuTree(engineType = 0) {
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
    { label: 'EFFECTS', sub: [
      { label: 'SATURATION', paramId: 'fxSaturation' },
      { label: 'CHORUS MIX', paramId: 'fxChorusMix' },
      { label: 'DELAY TIME', paramId: 'fxDelayTime' },
      { label: 'REVERB MIX', paramId: 'fxReverbMix' },
    ] },
    { label: 'MIDI CONTROL', sub: isNeuronik ? MIDI_CC_NEURONIK : MIDI_CC_BASE },
  ];
}

/**
 * @param {object} options
 * @param {object} options.store  el store de la página (getState/pushParameter):
 *   las mismas manos que el panel — el LCD conduce los parámetros REALES.
 * @param {() => number} [options.engineType]  engineType ACTUAL (el árbol
 *   depende de él); sin ella, Neuronik.
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

  hooksRef.onEdit = (paramId, dir) => {
    const control = controlOf(paramId);

    if (!control) return;

    const current = parameters()[paramId] ?? 0;
    const next = Math.min(1, Math.max(0, current + stepOf(control) * dir));

    if (next !== current) store.pushParameter(paramId, next, 'end');
  };

  hooksRef.onAction = (item) => {
    // RESET_MIDI vive en el host (el manager de mapeos CC no ha viajado a la
    // WebUI): la petición sale por el canal MIDI, el host decide.
    if (item?.paramId === 'RESET_MIDI') store.sendMidiMessage?.({ kind: 'resetMappings' });
  };

  hooksRef.onPreview = (item) => {
    const control = controlOf(item?.paramId);

    if (!control) return;

    const value = parameters()[item.paramId] ?? 0;

    panel.screen.preview(0, item.label.toUpperCase(), {});
    panel.screen.preview(1, `> ${displayText(control, realFromNormalized(control, value))}`, {});
  };

  // Reposo del hardware original (updateLcdDefault): preset + estado.
  idleRef.idle = () => {
    const preset = store.getState().presetState?.current ?? '';
    const isNeuronik = engineNow() === 0;

    return [
      preset ? `PATCH: ${preset.toUpperCase()}` : 'NEURONiK',
      isNeuronik ? 'NEURONIK READY' : 'NEUROTIK READY',
    ];
  };

  // El valor en EDIT: el snapshot manda (un edit nativo se ve en el LCD).
  editValueRef.editValue = (item) => {
    const control = controlOf(item?.paramId);

    if (!control) return '';

    return displayText(control, realFromNormalized(control, parameters()[control.id] ?? 0));
  };

  let panel = createLcdPanel(element, {
    menu: buildMenuTree(engineNow()),
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
     * El árbol depende del engineType: un cambio de motor reconstruye el menú.
     * La máquina es pura y el panel no expone swap de árbol: recrear la
     * composición en el MISMO elemento (barata: pantalla + 6 botones) con los
     * mismos closures por referencia.
     */
    rebuild() {
      panel.destroy();
      element.textContent = '';

      panel = createLcdPanel(element, {
        menu: buildMenuTree(engineNow()),
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
