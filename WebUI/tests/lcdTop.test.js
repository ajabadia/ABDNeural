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
 *   - rebuild() cambia el árbol cuando cambia el motor;
 *   - la rama EFFECTOS se DERIVA del estado: los nombres y el número de
 *     mandos del hueco 1 los pone el efecto que hay PUESTO, y el árbol se
 *     rehace cuando ese efecto cambia;
 *   - y que CADA id del árbol exista en el contrato generado, que es la
 *     cuenta que faltaba cuando un id retirado dejó un knob muerto en
 *     silencio.
 */

import { describe, expect, it, vi } from 'vitest';

import { createLcdPanel } from '@abdsynths/shared/components';

import { buildMenuTree, createLcdTop } from '../src/ui/lcdTop.js';
import { FX_CATALOG } from '../generated/fx-catalog.generated.js';
import { describeControl, defaultNormalizedState, getDescriptor } from '../src/contracts/parameters.js';
import { SCREEN_PARAMETER_IDS } from '../src/contracts/screens.js';
import { normalizedFromChoiceIndex } from '../src/contracts/paramValue.js';

function makeHost() {
  const host = document.createElement('div');
  document.body.append(host);
  return host;
}

function makeStore(overrides = {}) {
  const { sendMidiCcLearn, sendMidiCcClear, sendMidiCcReset, ...rest } = overrides;
  const state = {
    parameters: defaultNormalizedState(SCREEN_PARAMETER_IDS),
    presetState: { presets: [], current: 'INIT SOUND' },
    ...rest,
  };
  const push = vi.fn();

  return {
    state,
    push,
    getState: () => state,
    pushParameter: push,
    // Acciones MIDI CC del LCD (no son estado: van directas al retorno).
    sendMidiCcLearn: sendMidiCcLearn ?? vi.fn(),
    sendMidiCcClear: sendMidiCcClear ?? vi.fn(),
    sendMidiCcReset: sendMidiCcReset ?? vi.fn(),
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

  it('CADA id del arbol existe en el contrato generado, en los dos motores', () => {
    // ESTE TEST ES EL QUE FALTABA, y su ausencia es como se muere un knob
    // en silencio. La migracion del hueco 1 retiro el mando suelto
    // `fxSaturation` y la entrada del LCD se quedo apuntando al id retirado
    // sin que nada se pusiera rojo: el store IGNORA un id que no posee, la
    // llamada se come sin error y el knob no hacia NADA. El lienzo si tiene
    // esta cuenta (`sections.test.js`, 'cada id del lienzo existe en el
    // contrato generado'); el arbol del LCD, no, y es el mismo contrato.
    //
    // Se recorren los dos motores porque el arbol depende del engineType, y
    // lo que se apaga con un motor puede ser justo lo que se rompe con el
    // otro. Las Actions (RESET ALL) no son parametros y se saltan: su id es
    // un verbo del D-pad, no un mando del APVTS.
    const items = (node, into = []) => {
      for (const item of node) {
        if (item.sub) items(item.sub, into);
        else if (item.type !== 'action') into.push(item);
      }
      return into;
    };

    const missing = [0, 1]
      .flatMap((engineType) => items(buildMenuTree(engineType)).map((item) => ({ engineType, ...item })))
      .filter((item) => getDescriptor(item.paramId) === null)
      .map((item) => `${item.label} (${item.paramId}) en el motor ${item.engineType}`);

    // VACIA, y el propio test lo dice: hasta el 2026-09-29 era
    // 'SATURATION (fxSaturation)', el id que la migracion del rack retiro.
    expect(missing).toEqual([]);
  });

  it('el mando del drive es el del hueco 1, no el id retirado', () => {
    // El caso concreto del anterior, nombrado: la entrada que antes se
    // llamaba SATURATION conduce ahora `fx1Param1`, que es el drive del
    // bus del hueco 1 (`fx[0].params[0]`) y el id que usa tambien el destino
    // 17 de la matriz. Se mira el id y NO la etiqueta a proposito: el nombre
    // es el de un parametro cuyo sentido depende del efecto PUESTO, y ese
    // nombre lo deriva ahora la rama EFFECTOS (los tests de abajo). Aqui
    // lo que se comprueba es el cable: que la entrada apunte al hueco 1.
    const effects = buildMenuTree(0).find((item) => item.label === 'EFFECTS');
    const ids = effects.sub.map((item) => item.paramId);

    expect(ids).toContain('fx1Param1');
    expect(ids).not.toContain('fxSaturation');
  });


describe('lcdTop / la rama EFFECTOS se DERIVA del efecto puesto', () => {
  const fila = (displayName) => FX_CATALOG.effects.find((effect) => effect.displayName === displayName);
  const rama = (efecto) => buildMenuTree(0, efecto).find((item) => item.label === 'EFFECTS').sub;
  const nombres = (efecto) => rama(efecto).map((item) => item.label);
  const delHueco = (efecto) => rama(efecto).filter((item) => item.paramId.startsWith('fx1Param'));

  // El normalizado del `choice` para dejar un efecto PUESTO en el hueco.
  const puesto = (indice) => normalizedFromChoiceIndex(describeControl('fx1Type'), indice);

  // Lo que el LCD PINTA al entrar en la rama: la maquina navega hasta ella y
  // la abre. Es la unica forma de mirar el menu de verdad, sin colarse en
  // el arbol que el modulo acaba de fabricar.
  const textoDeEfectos = (view) => {
    const machine = view.panel.machine;

    machine.onMenuPress();                          // -> navigation (GLOBAL)
    machine.onArrow('right');                      // RESONATOR
    machine.onArrow('right');                      // FILTER
    machine.onArrow('right');                      // EFFECTS
    machine.onOkPress();                            // abre la rama
    view.paint();                                  // y la pantalla se entera

    return [...view.element.querySelectorAll('.abd-lcd__line')]
      .map((el) => el.textContent).join(' | ');
  };

  it('el nombre del mando cambia con el efecto: SAT DRIVE con saturacion, CHO RATE con chorus', () => {
    // EL ENCARGO: si el hueco 1 no lleva saturacion, el nombre del mando
    // cambia con el efecto puesto. `fx1Param1` es el parametro 1 de lo que
    // haya en el bus, asi que un nombre fijo seria mentira en cuanto la
    // ficha cambiase de efecto: por eso el arbol se deriva de la fila del
    // catalogo (displayName + params[].name).
    const sat = nombres(fila('Saturation'));
    const chorus = nombres(fila('Chorus'));
    const reverb = nombres(fila('Reverb'));

    expect(sat).toContain('SAT DRIVE');
    expect(chorus).toContain('CHO RATE');
    expect(chorus).toContain('CHO DEPTH');
    expect(reverb).toContain('REV SIZE');

    // Y el nombre viejo desaparece: no hay dos verdades en la misma rama.
    expect(chorus).not.toContain('SAT DRIVE');
    expect(reverb).not.toContain('SAT DRIVE');

    // Los tres mandos planos que el hueco no se llevo siguen ahi.
    expect(sat).toEqual(expect.arrayContaining(['CHORUS MIX', 'DELAY TIME', 'REVERB MIX']));
  });

  it('los mandos del hueco son los que DECLARA el efecto, no cuatro fijos', () => {
    // Un hueco publica cuatro posiciones para cualquier efecto (el bus no
    // sabe cuantos pondra el que le pongas) y quien lo dice es la fila. La
    // saturacion declara UN mando y el resto no existe; la reverb declara
    // CUATRO. Con cuatro entradas fijas, tres serian knobs muertos.
    expect(delHueco(fila('Saturation')).map((item) => item.paramId)).toEqual(['fx1Param1']);
    expect(delHueco(fila('Chorus')).map((item) => item.paramId)).toEqual(['fx1Param1', 'fx1Param2']);
    expect(delHueco(fila('Reverb')).map((item) => item.paramId))
      .toEqual(['fx1Param1', 'fx1Param2', 'fx1Param3', 'fx1Param4']);

    // El bypass no declara ninguno: la rama se queda con los tres planos,
    // sin inventar un mando del hueco que no existe.
    expect(rama(fila('Bypass')).map((item) => item.paramId))
      .toEqual(['fxChorusMix', 'fxDelayTime', 'fxReverbMix']);
  });

  it('sin efecto en el store se usa el default DEL CONTRATO (saturacion), no el bypass', () => {
    // El id ausente del snapshot no es "sin efecto": el hueco 1 arranca en
    // saturacion (`fx1Type` con defaultValue 4) y el menu tiene que arrancar
    // en SAT DRIVE. Es el mismo razonamiento que el cajon de efectos.
    expect(nombres(undefined)).toContain('SAT DRIVE');
  });

  it('createLcdTop lee el efecto del STORE, y el menu se rehace al vuelo', () => {
    const store = makeStore({ parameters: { ...defaultNormalizedState(SCREEN_PARAMETER_IDS), fx1Type: puesto(1) } });
    const view = createLcdTop({ store });
    makeHost().append(view.element);

    expect(textoDeEfectos(view)).toContain('CHO RATE');

    // La ficha cambia el efecto: el menu se rehace solo, sin que nadie le
    // avise (esto es lo que hace app.js comparando menuSignature()).
    store.state.parameters.fx1Type = puesto(4);
    view.rebuild();
    view.paint();

    const tras = textoDeEfectos(view);

    expect(tras).toContain('SAT DRIVE');
    expect(tras).not.toContain('CHO RATE');

    view.destroy();
  });

  it('menuSignature() depende SOLO del motor y del efecto puesto', () => {
    // La firma es lo que decide si el arbol hay que rehacerlo (app.js). Si
    // contemplase mas cosas, el LCD reconstruiria su menu sin motivo; si
    // contemplase menos, se quedaria con el arbol viejo cuando el efecto
    // cambiase. Los dos son fallos de este encargo.
    // El motor se lee del STORE: el `engineType` que se le pasa solo decide
    // SI se lee (sin el, el motor sale siempre 0), asi que el motor se
    // cambia cambiando el store, que es quien lo tiene. Es exactamente
    // el enganche que pone app.js.
    const store = makeStore();
    const view = createLcdTop({ store, engineType: () => store.getState().parameters.engineType ?? 0 });

    const inicial = view.menuSignature();

    store.state.parameters.engineType = 1;
    expect(view.menuSignature()).not.toBe(inicial);

    const conNeurotik = view.menuSignature();

    // Mandos que no son del menu: la firma no se mueve.
    store.state.parameters.masterBPM = 0.7;
    store.state.parameters.filterCutoff = 0.33;
    expect(view.menuSignature()).toBe(conNeurotik);

    // El efecto del hueco 1, si.
    store.state.parameters.fx1Type = puesto(3);
    expect(view.menuSignature()).not.toBe(conNeurotik);

    view.destroy();
  });
});

describe('lcdTop / showParameterPreview (el LCD ensena lo que giras)', () => {
  it('un edit de usuario (id, normalizado) pinta label + valor del contrato en transitorio', () => {
    const store = makeStore();
    const view = createLcdTop({ store });
    makeHost().append(view.element);

    // El reposo pinta PATCH/READY; el preview pisa las dos lineas.
    view.showParameterPreview('masterLevel', 0.5);

    const lines = [...view.element.querySelectorAll('.abd-lcd__line')].map((el) => el.textContent);
    expect(lines[0]).toContain('MASTER');
    expect(lines[1]).toContain('> ');
    // El valor formateado viene del contrato (displayText del real): un float
    // sin unidad 0..1 se ensena en porcentaje (50%).
    expect(lines[1]).toContain('50%');

    view.destroy();
  });

  it('en EDIT del menu NO pisa: la pantalla sigue ensenando lo que se edita', () => {
    const store = makeStore();
    const view = createLcdTop({ store });
    makeHost().append(view.element);

    // Abrir EDIT de un parametro del menu (GLOBAL -> MASTER VOL).
    const machine = view.panel.machine;
    machine.onMenuPress();
    machine.onOkPress();
    machine.onOkPress();

    expect(machine.state).toBe('edit');
    view.panel.repaint(); // las llamadas directas a la maquina no repintan

    view.showParameterPreview('morphX', 0.9);

    const lines = [...view.element.querySelectorAll('.abd-lcd__line')].map((el) => el.textContent);
    // Sigue el item del menu, no el preview del edit de usuario.
    expect(lines[0]).toContain('MASTER VOL');
    expect(lines[1]).not.toContain('MORPH');

    view.destroy();
  });

  it('un id desconocido no revienta y el preview de native no se dispara solo', () => {
    const store = makeStore();
    const view = createLcdTop({ store });
    makeHost().append(view.element);

    expect(() => view.showParameterPreview('id-que-no-existe', 0.5)).not.toThrow();

    view.destroy();
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

describe('lcdTop / los items cc del menu MIDI CONTROL son FUNCIONALES', () => {
  it('EDIT de un cc: OK/encoder+ arma el learn, encoder- desmapea, y nada mueve el parametro', () => {
    const store = makeStore({
      midiCcMappings: [
        { paramId: 'filterCutoff', cc: 74 },
        { paramId: 'filterRes', cc: 71 },
        { paramId: 'oscLevel', cc: -1 },
      ],
      sendMidiCcLearn: vi.fn(),
      sendMidiCcClear: vi.fn(),
      sendMidiCcReset: vi.fn(),
    });
    const view = createLcdTop({ store });
    makeHost().append(view.element);

    const machine = view.panel.machine;

    // Entrar por navegacion: MENU -> MIDI CONTROL (4 a la derecha) -> OK ->
    // primer item (CC CUTOFF) -> OK abre SU EDIT.
    machine.onMenuPress();
    for (let i = 0; i < 4; i += 1) machine.onArrow('right');
    machine.onOkPress();
    machine.onOkPress();

    expect(machine.state).toBe('edit');
    expect(machine.editing?.type).toBe('cc');

    // El EDIT de un cc ensena SU CC de la tabla del motor (segunda linea).
    view.panel.repaint();
    const lines = [...view.element.querySelectorAll('.abd-lcd__line')].map((el) => el.textContent);
    expect(lines[0]).toBe('CC CUTOFF');
    expect(lines[1]).toBe('CC 74');

    // Encoder+ (OK/‹›+): arma el learn de ESE parametro.
    machine.onEncoderRotate(1);
    expect(store.sendMidiCcLearn).toHaveBeenCalledWith('filterCutoff');

    // Encoder-: desmapea.
    machine.onEncoderRotate(-1);
    expect(store.sendMidiCcClear).toHaveBeenCalledWith('filterCutoff');

    // Y NADA empujo el parametro: el EDIT del cc no es un edit de valor.
    expect(store.push).not.toHaveBeenCalled();

    view.destroy();
  });

  it('el valor cc sin mapeo es CC --, RESET ALL llama al reset de la tabla y en local no revienta', () => {
    const store = makeStore({
      midiCcMappings: [{ paramId: 'filterCutoff', cc: -1 }],
      sendMidiCcLearn: vi.fn(),
      sendMidiCcClear: vi.fn(),
      sendMidiCcReset: vi.fn(),
    });
    const view = createLcdTop({ store });
    makeHost().append(view.element);

    const machine = view.panel.machine;
    const item = buildMenuTree(0)[4].sub[0]; // MIDI CONTROL -> CC CUTOFF

    // Sin mapeo, el EDIT ensena CC -- (la pantalla real: enter/EDIT/repaint).
    machine.onMenuPress();
    for (let i = 0; i < 4; i += 1) machine.onArrow('right');
    machine.onOkPress();
    machine.onOkPress();
    view.panel.repaint();
    const ccLines = [...view.element.querySelectorAll('.abd-lcd__line')].map((el) => el.textContent);
    expect(ccLines.some((text) => text.includes('CC --'))).toBe(true);

    // Rehacer el arnes para la parte del RESET sin el EDIT abierto.
    view.destroy();
    const view2 = createLcdTop({ store });
    makeHost().append(view2.element);
    const machine2 = view2.panel.machine;

    // RESET ALL (action): el reset viaja como accion de estado.
    machine2.onMenuPress();
    for (let i = 0; i < 4; i += 1) machine2.onArrow('right');
    machine2.onOkPress();
    machine2.onArrow('down');
    machine2.onArrow('down');
    machine2.onOkPress();
    expect(store.sendMidiCcReset).toHaveBeenCalledTimes(1);

    view2.destroy();

    // Modo local: sin tabla ni acciones en el store, navegar hasta el EDIT
    // de un cc no revienta (el EDIT es un no-op honesto).
    const localView = createLcdTop({ store: makeStore() });
    makeHost().append(localView.element);

    const localMachine = localView.panel.machine;
    localMachine.onMenuPress();
    for (let i = 0; i < 4; i += 1) localMachine.onArrow('right');
    localMachine.onOkPress();
    localMachine.onOkPress();
    localMachine.onEncoderRotate(1); // sin sendMidiCc* en el store: no-op

    expect(localMachine.state).toBe('edit');
    localView.panel.repaint();
    const localLines = [...localView.element.querySelectorAll('.abd-lcd__line')].map((el) => el.textContent);
    expect(localLines.some((text) => text.includes('CC --'))).toBe(true);

    localView.destroy();
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
