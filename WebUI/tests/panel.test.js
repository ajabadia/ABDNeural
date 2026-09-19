/**
 * Ensayo del `--selftest` del host contra el DOM REAL del lienzo.
 *
 * El chequeo en C++ conduce `document.querySelector('input[type=range]')` y parsea
 * `document.querySelector('.panel-footer code').textContent` como JSON, esperando
 * unos ids concretos con valor numérico. Esos dos selectores son el contrato
 * público de la página con el host, y aquí quedan pinchados para que un cambio de
 * interfaz no lo rompa en silencio (fallaría dentro de WebView2, minutos después,
 * como un selftest agotado).
 *
 * Con el lienzo único (8.2) el resto del test comprueba que los 70 controles están
 * de verdad en la página, cada uno en su ficha y con el tipo que pide el contrato.
 */

import { afterEach, describe, expect, it, vi } from 'vitest';

import { AUDIO_OWNER } from '../src/audio/policy.js';
import { describeControl } from '../src/contracts/parameters.js';
import { GENERAL_PARAMETER_IDS, KEYS_TAB_SELECTOR, SCREEN_PARAMETER_IDS } from '../src/contracts/screens.js';
import {
  BANDS,
  SECTIONS,
  SECTION_ACTIONS,
  SECTION_PARAMETER_IDS,
  SECTION_VISUALS,
} from '../src/contracts/sections.js';
import { createPanel } from '../src/ui/panel.js';
// La MISMA fabrica que usa la pagina (src/ui/visuals.js): el harness no vuelve a
// decidir que vista monta cada ficha.
import { createVisual } from '../src/ui/visuals.js';
import { contractSummary, defaultNormalizedState } from '../src/contracts/parameters.js';

/** Ids exactos que comprueba el selftest de GENERAL del host. */
const HOST_GENERAL_IDS = [
  'engineType',
  'envAttack', 'envDecay', 'envSustain', 'envRelease',
  'unisonDetune', 'unisonSpread',
  'randomStrength',
  'freezeResonator', 'freezeFilter', 'freezeEnvelopes',
];

// Igual que app.js: controles, acciones y vistas de ficha se resuelven contra su
// catalogo ANTES de llegar al panel, asi que el panel no conoce ids ni etiquetas.
const BANDS_WITH_CONTROLS = BANDS.map((band) => band.map((section) => {
  const controls = section.ids.map(describeControl).filter(Boolean);
  const visualSpec = section.visual ? SECTION_VISUALS[section.visual] : null;

  return {
    ...section,
    controls,
    action: section.action ? SECTION_ACTIONS[section.action] ?? null : null,
    visual: visualSpec
      ? createVisual(
        visualSpec.id,
        visualSpec.parameterIds.map(describeControl).filter(Boolean),
      )
      : null,
  };
}));

function makeState(overrides = {}) {
  return {
    parameters: defaultNormalizedState(SCREEN_PARAMETER_IDS),
    summary: contractSummary(SCREEN_PARAMETER_IDS),
    contractErrors: [],
    changeCount: 0,
    bridgeAvailable: false,
    snapshotVersion: 0,
    ...overrides,
  };
}

function mountPanel(handlers = {}) {
  const panel = createPanel({
    bands: BANDS_WITH_CONTROLS,
    baselineId: 'masterLevel',
    handlers,
  });

  document.body.append(panel.element);

  return panel;
}

afterEach(() => {
  document.body.innerHTML = '';
});

describe('panel / contrato del selftest del host', () => {
  it('el PRIMER input[type=range] del documento es masterLevel', () => {
    const panel = mountPanel();
    const state = makeState();

    panel.paint(state);

    const slider = document.querySelector('input[type=range]');

    expect(slider).not.toBeNull();
    expect(slider.id).toBe('masterLevel');
    expect(slider.dataset.parameterId).toBe('masterLevel');
    // El cable lleva normalizado 0..1 y el fader muestra exactamente eso
    // (masterLevel es 0..1 sin skew, así que las dos escalas coinciden).
    expect(Number(slider.value)).toBeCloseTo(state.parameters.masterLevel, 5);
  });

  it('el fader va ANTES que las ruedas del teclado (el host lee el primero)', () => {
    mountPanel();

    // El teclado compartido monta sus ruedas (range) dentro de #keys-root DESPUÉS
    // del panel; aquí se simula ese montaje para fijar el orden del documento.
    const wheel = document.createElement('input');
    wheel.type = 'range';
    wheel.className = 'kbd-wheel-slider';
    document.querySelector('#keys-root').append(wheel);

    const ranges = [...document.querySelectorAll('input[type=range]')];

    expect(ranges).toHaveLength(2);
    expect(ranges[0].dataset.parameterId).toBe('masterLevel');
  });

  it('pinta un snapshot nativo sobre el fader (NATIVO -> JS)', () => {
    const panel = mountPanel();
    const state = makeState();

    panel.paint({ ...state, parameters: { ...state.parameters, masterLevel: 0.25 } });

    expect(Number(document.querySelector('input[type=range]').value)).toBeCloseTo(0.25, 5);
    expect(document.querySelector('[data-parameter-readout="masterLevel"]').textContent)
      .toContain('25%');
  });

  it('el <code> del pie es JSON con todos los ids de GENERAL como números', () => {
    const panel = mountPanel();
    panel.paint(makeState());

    const code = document.querySelector('.panel-footer code');
    expect(code).not.toBeNull();

    const parsed = JSON.parse(code.textContent);

    for (const id of HOST_GENERAL_IDS) {
      expect(parsed, `falta "${id}" en el JSON del pie`).toHaveProperty(id);
      expect(typeof parsed[id], `"${id}" no es numérico`).toBe('number');
    }

    expect(HOST_GENERAL_IDS).toEqual(GENERAL_PARAMETER_IDS);
  });

  it('un edit nativo de envAttack llega al pie como 0.5 normalizado', () => {
    const panel = mountPanel();
    const state = makeState();

    panel.paint({ ...state, parameters: { ...state.parameters, envAttack: 0.5 } });

    expect(JSON.parse(document.querySelector('.panel-footer code').textContent).envAttack)
      .toBeCloseTo(0.5, 6);
  });

  it('informa del estado del puente y de los errores de contrato', () => {
    const panel = mountPanel();

    panel.paint(makeState({ bridgeAvailable: true, snapshotVersion: 4, changeCount: 7 }));
    expect(document.querySelector('.status').textContent).toContain('snapshot #4');
    expect(document.querySelector('.panel-footer span').textContent).toContain('updates: 7');

    panel.paint(makeState({ contractErrors: ['"masterLevel" out of normalised range: 1.5'] }));
    expect(document.querySelector('.panel-footer span').textContent).toContain('contract errors');
  });
});

describe('panel / lienzo único', () => {
  it('pinta una ficha por sección, en las bandas que dice el reparto', () => {
    mountPanel();

    expect(document.querySelectorAll('.card')).toHaveLength(SECTIONS.length);
    expect(document.querySelectorAll('.band')).toHaveLength(BANDS.length);

    for (const section of SECTIONS) {
      const card = document.querySelector(`[data-section-id="${section.id}"]`);

      expect(card, `falta la ficha "${section.id}"`).not.toBeNull();
      expect(card.querySelector('.card__heading h2').textContent).toBe(section.title);
    }
  });

  it('los 70 parámetros tienen su celda, exactamente una vez', () => {
    mountPanel();

    const ids = [...document.querySelectorAll('[data-parameter-id]')]
      .map((cell) => cell.dataset.parameterId);

    expect(ids).toHaveLength(70);
    expect(new Set(ids).size).toBe(70);
    expect(ids.sort()).toEqual([...SECTION_PARAMETER_IDS].sort());
  });

  it('cada ficha lleva los controles que declara el reparto', () => {
    mountPanel();

    for (const section of SECTIONS) {
      // Una ficha de cajon (matriz) pinta sus celdas en el cajon, no en el lienzo:
      // el reparto es el mismo, cambia donde vive la celda.
      const scope = section.drawer
        ? `#drawer-${section.id} [data-parameter-id]`
        : `[data-section-id="${section.id}"] [data-parameter-id]`;
      const ids = [...document.querySelectorAll(scope)].map((cell) => cell.dataset.parameterId);

      // En el cajon el orden es el de las rutas (fuente, destino, cantidad), que es
      // el mismo de `ids`; el distintivo de cada ruta no lleva `data-parameter-id`.
      expect(ids, `ficha "${section.id}"`).toEqual(section.ids);
    }
  });

  it('usa la familia compartida: 46 floats, 5 toggles y 19 desplegables', () => {
    mountPanel();

    const countOf = (selector) => document.querySelectorAll(selector).length;

    // 46 floats menos masterLevel, que es el fader nativo del host: es el ÚNICO
    // control que no sale de la familia compartida (contrato de 8.1 paso 2c).
    expect(countOf('.cell--knob')).toBe(45);
    expect(countOf('.cell--baseline')).toBe(1);
    expect(countOf('.cell--toggle')).toBe(5);
    // Los desplegables son el `Select` COMPARTIDO (clase de la familia), no un
    // <select> hecho a mano: si alguien vuelve a construirlo inline, esto cae.
    expect(countOf('.cell--choice .abd-select__field')).toBe(19);
    expect(countOf('.cell--knob') + countOf('.cell--baseline')
      + countOf('.cell--toggle') + countOf('.cell--choice')).toBe(70);
  });

  it('el gating por motor se reevalua con cada snapshot sin reescribir el valor', () => {
    const panel = mountPanel();
    const state = makeState();
    const optionFor = (label) => [...document.querySelectorAll('#control-mod1Destination option')]
      .find((option) => option.textContent === label);

    panel.paint(state);   // engineType arranca en NEURONiK (indice 0)

    expect(optionFor('Excite Noise').disabled).toBe(true);
    expect(optionFor('Osc Level').disabled).toBe(false);

    // El host restaura un preset del OTRO motor (Neurotik) apuntando a un destino
    // exclusivo de NEURONiK (indice 10, "Filter Cutoff"): la opcion se marca como no
    // disponible, pero el valor guardado se conserva. El panel nativo lo pasaba a
    // Off desde un timer: esa era la corrupcion silenciosa que se retiro.
    panel.paint({
      ...state,
      parameters: { ...state.parameters, engineType: 1, mod1Destination: 10 / 27 },
    });

    expect(optionFor('Excite Noise').disabled).toBe(false);   // ahora le toca a Neurotik
    expect(optionFor('Filter Cutoff').disabled).toBe(true);
    expect(optionFor('Odd/Even Bal').disabled).toBe(true);
    expect(document.querySelector('#control-mod1Destination').value).toBe('10');
    expect(document.querySelector('#control-mod1Destination')
      .closest('.abd-select').dataset.divergent).toBe('true');
  });

  it('sin el motor en el snapshot el gating no se aplica (no se inventa el activo)', () => {
    const panel = mountPanel();
    const { engineType, ...withoutEngine } = defaultNormalizedState(SCREEN_PARAMETER_IDS);

    panel.paint(makeState({ parameters: withoutEngine }));

    const disabled = [...document.querySelectorAll('#control-mod1Destination option')]
      .filter((option) => option.disabled);

    expect(disabled).toHaveLength(0);
  });

  it('un edit de una celda sale al handler en normalizado (JS -> NATIVO)', () => {
    const onChange = vi.fn();
    mountPanel({ onChange });

    const control = document.querySelector('[data-parameter-id="freezeResonator"] .abd-toggle');
    control.click();

    expect(onChange).toHaveBeenCalledWith('freezeResonator', 1);
  });

  it('cada knob publica su dial accesible (role=slider), listo para arrastre y teclado', () => {
    mountPanel();

    const dials = document.querySelectorAll('.cell--knob .abd-knob__dial[role="slider"]');

    expect(dials).toHaveLength(45);

    for (const dial of dials) expect(dial.tabIndex).toBe(0);
  });
});

describe('panel / franja de interpretación', () => {
  it('publica el anclaje data-tab="keys" que el host pulsa', () => {
    mountPanel();

    expect(document.querySelector(KEYS_TAB_SELECTOR)).not.toBeNull();
  });

  it('pliega y despliega la franja sin desmontar el teclado', () => {
    const panel = mountPanel();
    const keysRoot = document.querySelector('#keys-root');

    expect(panel.isKeysCollapsed()).toBe(false);
    expect(keysRoot).not.toBeNull();

    document.querySelector(KEYS_TAB_SELECTOR).click();
    expect(panel.isKeysCollapsed()).toBe(true);
    // Sigue en el DOM: el host lee la rueda de modulación ahí dentro.
    expect(document.querySelector('#keys-root')).not.toBeNull();

    document.querySelector(KEYS_TAB_SELECTOR).click();
    expect(panel.isKeysCollapsed()).toBe(false);
  });

  it('el botón de PANIC llama al handler', () => {
    const onPanic = vi.fn();
    mountPanel({ onPanic });

    document.querySelector('.keys-panic').click();

    expect(onPanic).toHaveBeenCalledTimes(1);
  });
});

describe('panel / propiedad del audio (policy)', () => {
  it('dentro de un host es una LECTURA: no hay SOUND ON que pulsar', () => {
    const panel = mountPanel();

    panel.paintAudio({ owner: AUDIO_OWNER.NATIVE });

    const button = document.querySelector('.audio-start');

    expect(button.hidden).toBe(true);
    expect(document.querySelector('.audio-mode__label').textContent).toContain('nativo');
  });

  it('en modo local ofrece SOUND ON e informa del motor', () => {
    const onStartSound = vi.fn();
    const panel = mountPanel({ onStartSound });

    panel.paintAudio({ owner: AUDIO_OWNER.WORKLET });

    const button = document.querySelector('.audio-start');

    expect(button.hidden).toBe(false);
    button.click();
    expect(onStartSound).toHaveBeenCalledTimes(1);

    panel.paintAudio({ owner: AUDIO_OWNER.WORKLET, status: 'ready', sampleRate: 48000 });
    expect(document.querySelector('.audio-mode__detail').textContent).toContain('48.0 kHz');
    expect(button.hidden).toBe(true);

    panel.paintAudio({ owner: AUDIO_OWNER.WORKLET, status: 'error', error: 'sin device' });
    expect(button.hidden).toBe(false);
    expect(button.textContent).toBe('REINTENTAR');
    expect(document.querySelector('.audio-mode__detail').textContent).toContain('sin device');
  });
});

describe('panel / ficha de cajon (matriz de modulacion)', () => {
  it('las 12 celdas de la matriz viven en el cajon, no en la rejilla', () => {
    mountPanel();

    const matrixIds = SECTIONS.find((section) => section.id === 'modMatrix').ids;
    const inDrawer = [...document.querySelectorAll('#drawer-modMatrix [data-parameter-id]')]
      .map((cell) => cell.dataset.parameterId);

    expect(inDrawer).toEqual(matrixIds);
    // Y en el lienzo NO queda ninguna: la ficha solo lleva el resumen y el boton.
    expect(document.querySelectorAll('[data-section-id="modMatrix"] [data-parameter-id]'))
      .toHaveLength(0);
    // Cuatro rutas, una fila cada una, con sus tres campos.
    expect(document.querySelectorAll('#drawer-modMatrix .drawer-slot')).toHaveLength(4);
    expect(document.querySelector('#drawer-modMatrix .drawer-slot__badge').textContent).toBe('RUTA 1');
  });

  it('el boton de la ficha abre el cajon y ESC lo cierra (estado de vista)', () => {
    const panel = mountPanel();
    const drawer = panel.drawers.get('modMatrix');

    expect(drawer.isOpen()).toBe(false);

    document.querySelector('[data-drawer-trigger="modMatrix"]').click();
    expect(drawer.isOpen()).toBe(true);
    expect(drawer.body.querySelector('[data-parameter-id="mod1Destination"]')).not.toBeNull();

    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    expect(drawer.isOpen()).toBe(false);
  });

  it('el resumen del lienzo se pinta con el mismo snapshot que las celdas', () => {
    const panel = mountPanel();
    const state = makeState();

    panel.paint(state);

    const summary = document.querySelector('.mod-summary');

    expect(summary).not.toBeNull();
    expect(summary.querySelectorAll('.mod-summary__row')).toHaveLength(4);
    // Sin rutas configuradas, fuente y destino son "Off" (el default del contrato).
    expect(summary.querySelector('.mod-summary__source').textContent).toBe('Off');
    expect(summary.querySelector('.mod-summary__destination').textContent).toBe('Off');

    // Ahora con una ruta: LFO 1 -> Filter Cutoff. Los ids salen del propio resumen.
    panel.paint({
      ...state,
      parameters: {
        ...state.parameters,
        mod1Source: 1 / 5,       // "LFO 1" de 6 opciones
        mod1Destination: 10 / 27, // "Filter Cutoff"
      },
    });

    const first = document.querySelector('.mod-summary__row[data-slot="1"]');

    expect(first.querySelector('.mod-summary__source').textContent).toBe('LFO 1');
    expect(first.querySelector('.mod-summary__destination').textContent).toBe('Filter Cutoff');
  });
});

describe('panel / vista de la ficha (curva ADSR)', () => {
  it('se monta en FILTRO & ENVOLVENTE sin contar como celda de parametro', () => {
    mountPanel();

    const card = document.querySelector('.card[data-section-id="filterEnv"]');
    const curve = card.querySelector('[data-visual="amp-envelope"]');

    expect(curve).not.toBeNull();
    expect(curve.querySelector('svg')).not.toBeNull();
    expect(curve.classList.contains('cell')).toBe(false);

    // El lienzo sigue teniendo 70 celdas de PARAMETRO, la curva no es una mas
    expect(document.querySelectorAll('.cell')).toHaveLength(SECTION_PARAMETER_IDS.length);
    // Una sola curva ADSR. El resumen de la matriz es otra vista (`.mod-summary`),
    // no un `card__visual`: así que este recuento sigue diciendo lo que dice.
    expect(document.querySelectorAll('.card__body .card__visual')).toHaveLength(1);
    expect(document.querySelectorAll('.mod-summary')).toHaveLength(1);
  });

  it('se repinta con el snapshot: otro ataque, otro trazo', () => {
    const panel = mountPanel();
    const line = () => document
      .querySelector('[data-visual="amp-envelope"] .envelope-curve__line')
      .getAttribute('d');

    panel.paint(makeState());
    const before = line();

    const parameters = defaultNormalizedState(SCREEN_PARAMETER_IDS);
    parameters.envAttack = 0.99;      // ataque casi al maximo

    panel.paint(makeState({ parameters }));

    expect(line()).not.toBe(before);
  });
});

describe('panel / vista de la ficha (ranuras de modelo A–D)', () => {
  it('se monta en su ficha sin contar como celda de parametro', () => {
    mountPanel();

    const card = document.querySelector('.card[data-section-id="models"]');
    const view = card.querySelector('[data-visual="model-slots"]');

    expect(view).not.toBeNull();
    expect(view.querySelectorAll('.model-slots__row')).toHaveLength(4);
    // No es una celda: el lienzo sigue teniendo 70 celdas de PARAMETRO.
    expect(document.querySelectorAll('.cell')).toHaveLength(SECTION_PARAMETER_IDS.length);
  });

  it('recibe el ESTADO del store, no solo los parametros (vive fuera del APVTS)', () => {
    const panel = mountPanel();
    const view = () => document.querySelector('[data-visual="model-slots"]');

    // Sin host: no hay a quien pedir la carga, asi que los botones no se pueden pulsar.
    panel.paint(makeState());

    expect([...view().querySelectorAll('.model-slots__load')].every((button) => button.disabled))
      .toBe(true);

    // Con host y slots del puente: el nombre que trae el MOTOR se pinta, y el boton
    // de esa ranura se habilita.
    panel.paint(makeState({
      bridgeAvailable: true,
      models: [{ slot: 0, name: 'Campana', isValid: true, amplitudes: [], frequencyOffsets: [] }],
    }));

    expect(view().querySelector('[data-slot="0"] .model-slots__name').textContent).toBe('Campana');
    expect(view().querySelector('[data-slot="0"] .model-slots__load').disabled).toBe(false);
    expect(view().querySelector('.model-slots__status').textContent).toBe('1/4 cargados');
  });
});

describe('panel / acciones de ficha (RANDOM)', () => {
  it('vive en la cabecera de GLOBAL & MASTER y NO ocupa celda', () => {
    mountPanel();

    const card = document.querySelector('.card[data-section-id="global"]');
    const button = card.querySelector('.card__action');

    expect(button).not.toBeNull();
    expect(button.dataset.action).toBe('randomize');
    expect(button.textContent).toBe('RANDOM');
    expect(button.title).toBeTruthy();

    // Sigue habiendo exactamente 70 celdas de parametro en el lienzo
    expect(document.querySelectorAll('.cell')).toHaveLength(SECTION_PARAMETER_IDS.length);
  });

  it('sin host esta deshabilitado; con host, pide la accion al store', () => {
    const onAction = vi.fn();
    const panel = mountPanel({ onAction });
    const button = document.querySelector('.card__action');

    panel.paint(makeState({ bridgeAvailable: false }));
    expect(button.disabled).toBe(true);

    button.click();
    expect(onAction).not.toHaveBeenCalled();   // un boton deshabilitado no dispara

    panel.paint(makeState({ bridgeAvailable: true }));
    expect(button.disabled).toBe(false);

    button.click();
    expect(onAction).toHaveBeenCalledWith('randomize');
  });
});
