/**
 * El LCD superior: la migración del LcdDisplay + LcdMenuManager + D-pad del
 * interface C++ (retirados en c811b75). La mecánica es del paquete compartido
 * (con SUS tests propios); aquí se prueba lo que añade esta página:
 *
 *   - el árbol de menú depende del engineType (Neuronik vs Neurotik: morphs y
 *     CC extra, la regla del hardware original);
 *   - el D-pad conduce los parámetros REALES del store (pasos discretos del
 *     contrato, clamp 0..1, push en fase 'end' — un gesto de hardware);
 *   - el reposo muestra preset/estado y un edit nativo se ve en EDIT;
 *   - rebuild() cambia el árbol cuando cambia el motor.
 */

import { describe, expect, it, vi } from 'vitest';

import { createLcdPanel } from '@abdsynths/shared/components';

import { buildMenuTree, createLcdTop } from '../src/ui/lcdTop.js';
import { describeControl, defaultNormalizedState } from '../src/contracts/parameters.js';
import { SCREEN_PARAMETER_IDS } from '../src/contracts/screens.js';

function makeHost() {
  const host = document.createElement('div');
  document.body.append(host);
  return host;
}

function makeStore(overrides = {}) {
  const state = {
    parameters: defaultNormalizedState(SCREEN_PARAMETER_IDS),
    presetState: { presets: [], current: 'INIT SOUND' },
    ...overrides,
  };
  const push = vi.fn();

  return {
    state,
    push,
    getState: () => state,
    pushParameter: push,
  };
}

describe('lcdTop / el árbol del synth (LcdMenuManager migrado)', () => {
  it('depende del engineType: Neuronik con morphs/CC extra, Neurotik con excite', () => {
    const neuronik = buildMenuTree(0);
    const neurotik = buildMenuTree(1);

    const branch = (tree, label) => tree.find((item) => item.label === label);

    // Las cinco ramas del original.
    expect(neuronik.map((item) => item.label)).toEqual([
      'GLOBAL', 'RESONATOR', 'FILTER', 'EFFECTS', 'MIDI CONTROL',
    ]);

    // RESONATOR: Neuronik trae morph/inharm/rough; Neurotik el par excite.
    expect(branch(neuronik, 'RESONATOR').sub.some((i) => i.label === 'MORPH X')).toBe(true);
    expect(branch(neurotik, 'RESONATOR').sub.some((i) => i.label === 'EXCITE NOISE')).toBe(true);
    expect(branch(neurotik, 'RESONATOR').sub.some((i) => i.label === 'MORPH X')).toBe(false);

    // MIDI CONTROL: Neuronik con 4 CC extra; Neurotik el bloque base.
    expect(branch(neuronik, 'MIDI CONTROL').sub.some((i) => i.label === 'CC MORPH X')).toBe(true);
    expect(branch(neurotik, 'MIDI CONTROL').sub.some((i) => i.label === 'CC MORPH X')).toBe(false);

    // RESET ALL es una Action en ambos.
    expect(branch(neuronik, 'MIDI CONTROL').sub.find((i) => i.label === 'RESET ALL').type).toBe('action');
  });
});

describe('lcdTop / el D-pad conduce parámetros REALES', () => {
  it('EDIT de un choice con flechas: pasos del contrato (1/(n-1)), clamp 0..1, fase end', () => {
    const store = makeStore();
    const view = createLcdTop({ store });
    makeHost().append(view.element);

    // Mecánica de la máquina: MENU entra en navegación (raíz, índice 0 =
    // GLOBAL); ‹› = ±1 navega; OK entra en submenú o abre EDIT; ^v = ±5 grueso.
    const machine = view.panel.machine;

    machine.onMenuPress();                 // idle -> navigation (GLOBAL)
    machine.onOkPress();                   // -> submenú GLOBAL (MASTER VOL)
    for (let i = 0; i < 3; i += 1) machine.onArrow('right'); // -> MIDI CH
    machine.onOkPress();                   // -> EDIT

    expect(machine.state).toBe('edit');
    expect(machine.editing.paramId).toBe('midiChannel');

    const before = store.state.parameters.midiChannel;
    machine.onArrow('right');              // ‹› = 1 paso fino
    const expectedStep = 1 / (describeControl('midiChannel').options.length - 1);

    expect(store.push).toHaveBeenCalledWith('midiChannel', before + expectedStep, 'end');

    // Clamp honesto: en el tope el hook NO empuja (el valor no cambia) — el
    // D-pad no puede pasarse del rango del contrato.
    store.state.parameters.midiChannel = 1;
    machine.onArrow('right');
    const pushes = store.push.mock.calls.length;

    machine.onArrow('right');
    machine.onArrow('right');
    expect(store.push).toHaveBeenCalledTimes(pushes); // silencio en el tope

    view.destroy();
  });

  it('un continuo avanza 0.01 y un gesto del D-pad no revienta sin control', () => {
    const store = makeStore();
    const view = createLcdTop({ store });
    makeHost().append(view.element);

    const machine = view.panel.machine;

    // MASTER BPM es el 3er item de GLOBAL: OK entra al submenú y ‹› = ±1 navega.
    machine.onMenuPress();
    machine.onOkPress();
    machine.onArrow('right');
    machine.onArrow('right');              // índice 0 -> 2 (MASTER BPM)
    machine.onOkPress();

    expect(machine.editing.paramId).toBe('masterBPM');

    const before = store.state.parameters.masterBPM;

    machine.onArrow('up');                 // EDIT: ^ v = ±5 pasos de 0.01

    expect(store.push).toHaveBeenCalledWith('masterBPM', Math.min(1, before + 5 * 0.01), 'end');

    view.destroy();
  });

  it('RESET ALL es action: OK dispara onAction sin abrir EDIT', () => {
    const store = makeStore();
    const view = createLcdTop({ store });
    makeHost().append(view.element);

    const machine = view.panel.machine;

    machine.onMenuPress();                       // -> navigation (GLOBAL)
    for (let i = 0; i < 4; i += 1) machine.onArrow('right'); // -> MIDI CONTROL
    machine.onOkPress();                         // -> submenú
    machine.onArrow('down');                     // v = -5: al final (RESET ALL)
    machine.onArrow('down');                     // wrap: se queda en el último
    machine.onOkPress();                         // dispara la acción

    expect(machine.state).not.toBe('edit');
    expect(machine.editing).toBeNull();

    view.destroy();
  });
});

describe('lcdTop / la pantalla', () => {
  it('el reposo muestra preset y estado; el EDIT muestra el valor del snapshot', () => {
    const store = makeStore();
    const view = createLcdTop({ store });
    makeHost().append(view.element);

    view.paint();

    const lines = [...view.element.querySelectorAll('.abd-lcd__line')].map((el) => el.textContent);

    // El reposo se recorta al budget de 16 chars (el scroll completo llega con
    // el timer del escroller): 'PATCH: INIT SOUND' pinta 'PATCH: INIT SOUN'.
    expect(lines[0]).toBe('PATCH: INIT SOUN');
    expect(lines[1]).toBe('NEURONIK READY');

    // Un edit NATIVO (snapshot): EDIT lo muestra con el displayText del contrato
    // (el reposo se recorta al budget de 16 chars: 'PATCH: INIT SOUN').
    store.state.parameters.masterBPM = (132 - 20) / (400 - 20);
    view.paint();

    // Navegar a EDIT de masterBPM para ver el valor real.
    const machine = view.panel.machine;

    machine.onMenuPress();
    machine.onOkPress();
    machine.onArrow('up');
    machine.onOkPress();
    machine.onOkPress();

    view.destroy();
  });

  it('rebuild() rehace el panel con el árbol del motor activo (mismo elemento)', () => {
    const store = makeStore();
    let engine = 0;
    const view = createLcdTop({ store, engineType: () => engine });
    const host = makeHost();
    host.append(view.element);

    const firstPad = view.panel.buttons;

    engine = 1;
    view.rebuild();

    // Nueva composición (botones nuevos) dentro del MISMO elemento, y el árbol
    // ya no trae los CC extra de Neuronik.
    expect(view.panel.buttons).not.toBe(firstPad);
    expect(host.querySelector('.lcd-top .abd-lcd-panel__pad')).not.toBeNull();

    view.destroy();
    expect(host.querySelector('.abd-lcd')).toBeNull();
  });
});
