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
import { MOD_DESTINATIONS } from '../generated/parameters.generated.js';

/**
 * El ultimo indice de destino (la normalizada del APVTS es indice/ultimo):
 * sale del CONTRATO generado, no de una cuenta a mano. Cuando la tabla crecio
 * (Morph Z 2/3, FASE 11.3) los valores escritos 10/27 dejaron de apuntar a
 * "Filter Cutoff" y estos dos tests fallaban por un pin, no por un fallo.
 */
const LAST_DESTINATION = MOD_DESTINATIONS.length - 1;
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
import { onTelemetry as telemetryOnTelemetry, pushTelemetryFrame, resetTelemetry } from '../src/bridge/telemetry.js';

// El cajon de MODELOS delega la carga de ranuras aqui (la vista se crea UNA
// vez; el handler es el del montaje vivo, como en app.js).
let drawerOnLoad = null;
// Emisores de telemetria de las vistas del arnés (canal propio de la prueba,
// como el del lienzo en su test). Es una LISTA: el canal real (store.onTelemetry)
// es pub/sub con varios suscriptores — el espectral de MODELOS y las curvas de
// ENVOLVENTES cuelgan del mismo; una variable simple pisaria al anterior.
const harnessTelemetryNotifies = [];
const emitTelemetry = (frame) => { for (const notify of harnessTelemetryNotifies) notify(frame); };
// Captura del opener de rutas (env-curves / mod-summary), como app.js.
let harnessOpenRoute = null;

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
  // La vista de DETALLE del cajon (MODELOS: espectral + ranuras) se resuelve
  // igual que la del lienzo: mismos handlers vacios, mismo catalogo.
  const drawerSpec = section.drawer?.visual ? SECTION_VISUALS[section.drawer.visual] : null;
  // Igual que app.js: los view-models extra de las vistas (rutas de la matriz
  // para el lienzo Y el cajon de ENVOLVENTES) y los ids de la ficha para su
  // cajon.
  const routeControls = visualSpec?.id === 'envelope-curves' || drawerSpec?.id === 'envelope-blocks'
    ? SECTIONS.find((candidate) => candidate.id === 'modMatrix').ids
      .map(describeControl).filter(Boolean)
    : [];

  return {
    ...section,
    controls,
    action: section.action ? SECTION_ACTIONS[section.action] ?? null : null,
    drawerVisual: drawerSpec
      ? createVisual(
        drawerSpec.id,
        drawerSpec.parameterIds.map(describeControl).filter(Boolean),
        {
          onLoad: (slot) => drawerOnLoad?.(slot),
          ids: section.ids,
          routeControls,
          // El cajón de ENVOLVENTES pinta la aguja del MISMO canal que el
          // lienzo (app.js le pasa store.onTelemetry): la prueba expone el
          // emisor para el test de integración.
          onTelemetry: (notify) => { harnessTelemetryNotifies.push(notify); return () => {}; },
        },
      )
      : null,
    visual: visualSpec
      ? createVisual(
        visualSpec.id,
        visualSpec.parameterIds.map(describeControl).filter(Boolean),
        {
          routeControls,
          // El opener de rutas (env-curves y mod-summary): app.js lo conecta
          // tarde; la prueba lo captura para los tests del gesto.
          onOpenRoute: (slot) => harnessOpenRoute?.(slot),
        },
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
  drawerOnLoad = handlers.onLoad ?? null;

  const panel = createPanel({
    bands: BANDS_WITH_CONTROLS,
    baselineId: 'masterLevel',
    handlers,
    // El canal REAL de telemetría (pushTelemetryFrame notifica aquí, igual que
    // store.onTelemetry en producción) + la lista del arnés para los tests que
    // emiten a mano.
    onTelemetry: (notify) => {
      harnessTelemetryNotifies.push(notify);
      const stop = telemetryOnTelemetry(notify);
      return () => { const i = harnessTelemetryNotifies.indexOf(notify); if (i >= 0) harnessTelemetryNotifies.splice(i, 1); stop(); };
    },
  });

  document.body.append(panel.element);

  return panel;
}

afterEach(() => {
  document.body.innerHTML = '';
});

describe('panel / fila del LCD superior', () => {
  it('sin lcdSlot la fila NO existe (los gap del grid cuentan por pista)', () => {
    const panel = mountPanel();

    expect(panel.element.querySelector('.lcd-row')).toBeNull();
    expect(panel.element.classList.contains('panel--has-lcd')).toBe(false);
  });

  it('con lcdSlot el LCD vive DEBAJO del titulo y ENCIMA de las bandas', () => {
    const lcd = document.createElement('div');
    lcd.className = 'lcd-top';

    const panel = createPanel({ bands: BANDS_WITH_CONTROLS, baselineId: 'masterLevel', lcdSlot: lcd });
    document.body.append(panel.element);

    const row = panel.element.querySelector('.lcd-row');
    expect(row).not.toBeNull();
    expect(row.firstElementChild).toBe(lcd);
    expect(panel.element.classList.contains('panel--has-lcd')).toBe(true);

    // Orden vertical del mueble: cabecera -> LCD -> lienzo (las bandas).
    const order = [...panel.element.children].map((child) => child.className);
    expect(order.indexOf('panel-header')).toBeLessThan(order.indexOf('lcd-row'));
    expect(order.indexOf('lcd-row')).toBeLessThan(order.indexOf('canvas'));
  });
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

  it('los 71 parámetros tienen su celda, exactamente una vez', () => {
    mountPanel();

    const ids = [...document.querySelectorAll('[data-parameter-id]')]
      .map((cell) => cell.dataset.parameterId);

    expect(ids).toHaveLength(71);
    expect(new Set(ids).size).toBe(71);
    expect(ids.sort()).toEqual([...SECTION_PARAMETER_IDS].sort());
  });

  it('cada ficha lleva los controles que declara el reparto', () => {
    mountPanel();

    for (const section of SECTIONS) {
      // Una ficha de cajon pinta sus celdas en el cajon, no en el lienzo: el
      // reparto es el mismo, cambia donde vive la celda. El cajon SIN grupos
      // (GLOBAL & MASTER) excluye ademas el control base: masterLevel lo pinta
      // buildCard en la ficha (es el range nativo que consulta el host).
      // Con FRONTAL (caja LFO) las celdas viven en los DOS sitios: las del
      // frontal en la ficha del lienzo y el resto en el cajon. El orden de la
      // union es el del reparto (frontal primero, detalle despues).
      const frontal = section.drawer?.frontal ?? null;
      const scope = frontal
        ? `[data-section-id="${section.id}"] [data-parameter-id], #drawer-${section.id} [data-parameter-id]`
        : section.drawer
          ? `#drawer-${section.id} [data-parameter-id]`
          : `[data-section-id="${section.id}"] [data-parameter-id]`;
      // Orden del DOM: los nodos del cajon van antes que los de la ficha.
      const expected = section.drawer?.groups
        ? section.ids
        : [
            ...section.ids.filter((id) => !frontal?.includes(id) && id !== 'masterLevel'),
            ...(frontal ?? []),
          ];
      const ids = [...document.querySelectorAll(scope)].map((cell) => cell.dataset.parameterId);

      // En el cajon el orden es el de las rutas (fuente, destino, cantidad), que es
      // el mismo de `ids`; el distintivo de cada ruta no lleva `data-parameter-id`.
      expect(ids, `ficha "${section.id}"`).toEqual(expected);
    }
  });

  it('usa la familia compartida: 47 floats, 5 toggles y 19 desplegables', () => {
    mountPanel();

    const countOf = (selector) => document.querySelectorAll(selector).length;

    // 47 floats menos masterLevel, que es el fader nativo del host: es el ÚNICO
    // control que no sale de la familia compartida (contrato de 8.1 paso 2c).
    expect(countOf('.cell--knob')).toBe(46);
    expect(countOf('.cell--baseline')).toBe(1);
    expect(countOf('.cell--toggle')).toBe(5);
    // 19 choices: 13 desplegables + 5 segmentados (motor, syncs, ondas LED) +
    // 1 NumberBox (midiChannel; masterBPM cuenta como knob en el recuento de
    // celdas). Familia COMPARTIDA: si alguien construye uno inline, esto cae.
    expect(countOf('.cell--choice .abd-select__field')).toBe(13);   // 16: dos ondas (LED) + midiChannel (NumberBox)
    expect(countOf('.cell--choice .abd-segmented__group')).toBe(5);   // 3 del lienzo (motor, syncs) + las dos ondas LED de los cajones
    expect(countOf('.cell--knob') + countOf('.cell--baseline')
      + countOf('.cell--toggle') + countOf('.cell--choice')).toBe(71);
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
      parameters: { ...state.parameters, engineType: 1, mod1Destination: 10 / LAST_DESTINATION },
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

    expect(dials).toHaveLength(45);   // 46 menos masterBPM (NumberBox)

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

  it('el medidor de VOCES enciende un LED por voz activa y se esconde en silencio', () => {
    const panel = mountPanel();
    const meter = document.querySelector('.voice-meter');
    const leds = () => [...meter.querySelectorAll('.voice-meter__led')];

    // Monta 8 leds (la polifonía del host) y nace oculto (sin meter).
    expect(leds()).toHaveLength(8);
    expect(meter.hidden).toBe(true);

    panel.setVoiceMeter(3);
    expect(meter.hidden).toBe(false);
    expect(meter.getAttribute('aria-label')).toBe('3 voces activas');
    expect(leds().filter((led) => led.dataset.active === 'true').map((led) => led.dataset.led))
      .toEqual(['1', '2', '3']);

    // Una sola voz: singular en el aria-label.
    panel.setVoiceMeter(1);
    expect(meter.getAttribute('aria-label')).toBe('1 voz activa');
    expect(leds().filter((led) => led.dataset.active === 'true').map((led) => led.dataset.led))
      .toEqual(['1']);

    // Silencio: se esconde (no se queda encendido congelado).
    panel.setVoiceMeter(0);
    expect(meter.hidden).toBe(true);

    // Basura (no numérico): cuenta como 0, no revienta el canal del meter.
    expect(() => panel.setVoiceMeter(undefined)).not.toThrow();
    expect(meter.hidden).toBe(true);
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

  it('el distintivo del cajon es VIVO: cuenta las rutas ASIGNADAS, no el literal', () => {
    const panel = mountPanel();
    const drawer = panel.drawers.get('modMatrix');
    const badge = () => drawer.header.querySelector('.drawer__badge').textContent;

    // Los defaults del contrato asignan DOS fuentes (ENV 1 y ENV 2) y dejan Off las
    // otras dos rutas: el distintivo lo dice, donde el literal '4 RUTAS' mentia.
    const state = makeState();

    panel.paint(state);
    expect(badge()).toBe('2/4 RUTAS');

    // Una fuente a Off (indice 0): una ruta asignada menos.
    panel.paint({ ...state, parameters: { ...state.parameters, mod1Source: 0 } });
    expect(badge()).toBe('1/4 RUTAS');

    // Y con una tercera puesta (LFO 1, indice 1 de 8 -> 1/7) vuelve a subir.
    panel.paint({
      ...state,
      parameters: { ...state.parameters, mod1Source: 0, mod3Source: 1 / 7 },
    });
    expect(badge()).toBe('2/4 RUTAS');
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
    // El default del contrato ya SON rutas: ENV 1 -> Osc Level (cableado ENV
    // 1/2 por matriz) y la primera fila del resumen lo refleja.
    expect(summary.querySelector('.mod-summary__source').textContent).toBe('ENV 1');
    expect(summary.querySelector('.mod-summary__destination').textContent).toBe('Osc Level');

    // Ahora con una ruta: LFO 1 -> Filter Cutoff. Los ids salen del propio resumen.
    panel.paint({
      ...state,
      parameters: {
        ...state.parameters,
        mod1Source: 1 / 7,        // "LFO 1" de 8 opciones (Off, LFO 1/2, PB, MW, AT, ENV 1/2)
        mod1Destination: 10 / LAST_DESTINATION, // "Filter Cutoff"
      },
    });

    const first = document.querySelector('.mod-summary__row[data-slot="1"]');

    expect(first.querySelector('.mod-summary__source').textContent).toBe('LFO 1');
    expect(first.querySelector('.mod-summary__destination').textContent).toBe('Filter Cutoff');
  });
});

describe('panel / vista de la ficha ENVOLVENTES (dos curvas + rutas)', () => {
  it('se monta SIN celdas de parametro: las curvas viven en el cuerpo de la ficha', () => {
    mountPanel();

    const card = document.querySelector('.card[data-section-id="envelopes"]');
    const curves = card.querySelectorAll('[data-visual="amp-envelope"], [data-visual="filter-envelope"]');

    expect(curves).toHaveLength(2);
    for (const curve of curves) {
      expect(curve.querySelector('svg')).not.toBeNull();
      expect(curve.classList.contains('cell')).toBe(false);
    }

    // La ficha es de CAJON: sus ocho ADSR no empujan el lienzo.
    expect(card.querySelectorAll('[data-parameter-id]')).toHaveLength(0);

    // Tres vistas con cuerpo propio en el lienzo: las dos curvas de
    // ENVOLVENTES (una ficha, un visual compuesto) y el pad de MODELOS.
    // El resumen de la matriz es otra vista (`.mod-summary`), no un `card__visual`.
    expect(document.querySelectorAll('.card__body .card__visual')).toHaveLength(2);
    expect(document.querySelectorAll('.mod-summary')).toHaveLength(1);

    // Y el lienzo sigue teniendo 70 celdas de PARAMETRO (las ocho ADSR están
    // en el cajon, que las cuenta igual).
    expect(document.querySelectorAll('.cell')).toHaveLength(SECTION_PARAMETER_IDS.length);
  });

  it('pinta bajo cada curva las rutas de la matriz de SU fuente', () => {
    const panel = mountPanel();
    const state = makeState();

    panel.paint(state);

    // Defaults del contrato: ENV 1 -> Osc Level (100%) y ENV 2 -> Filter
    // Cutoff (100%): cada columna pinta SU ruta, el resto de slots no.
    const ampColumn = document.querySelector('.env-curves__column[data-envelope="env"]');
    const filterColumn = document.querySelector('.env-curves__column[data-envelope="filter"]');

    expect(ampColumn.querySelectorAll('.env-route:not(.env-route--empty)')).toHaveLength(1);
    expect(filterColumn.querySelectorAll('.env-route:not(.env-route--empty)')).toHaveLength(1);
    expect(ampColumn.querySelector('.env-route__destination').textContent).toBe('Osc Level');
    expect(filterColumn.querySelector('.env-route__destination').textContent).toBe('Filter Cutoff');
    // El amount se lee como PROFUNDIDAD: porcentaje con signo (+100%).
    expect(ampColumn.querySelector('.env-route__amount').textContent).toBe('+100%');

    // La matriz manda: mod1 pasa a LFO 1 -> Filter Cutoff y mod3 a ENV 1 ->
    // Delay Time. La columna AMP pierde su ruta del slot 1 y la gana del 3.
    panel.paint({
      ...state,
      parameters: {
        ...state.parameters,
        mod1Source: 1 / 7,          // "LFO 1" de 8 opciones
        mod1Destination: 10 / LAST_DESTINATION, // "Filter Cutoff"
        mod3Source: 6 / 7,          // "ENV 1"
        mod3Destination: 18 / LAST_DESTINATION, // "Delay Time"
        // amount simétrico -1..1: normalizado 0.75 == real +0.5 (+50%).
        mod3Amount: 0.75,
      },
    });

    expect(ampColumn.querySelectorAll('.env-route:not(.env-route--empty)')).toHaveLength(1);
    expect(ampColumn.querySelector('.env-route__destination').textContent).toBe('Delay Time');
    expect(ampColumn.querySelector('.env-route__amount').textContent).toBe('+50%');

    // La columna FILTER ya no tiene ruta (su ENV 2 -> Filter Cutoff del slot 2
    // sigue, pero el slot 1 ahora es de LFO 1): la ENV 2 pierde el suyo solo si
    // se reasigna; aqui lo que cambia es el slot 1.
    expect(filterColumn.querySelectorAll('.env-route:not(.env-route--empty)')).toHaveLength(1);
  });

  it('sin rutas para una envolvente lo DICE (no pinta un hueco mudo)', () => {
    const panel = mountPanel();
    const state = makeState();

    // Todas las fuentes ENV se apagan: ninguna columna tiene ruta.
    panel.paint({
      ...state,
      parameters: {
        ...state.parameters,
        mod1Source: 0,
        mod2Source: 0,
        mod3Source: 0,
        mod4Source: 0,
      },
    });

    for (const column of document.querySelectorAll('.env-curves__column'))
      expect(column.querySelector('.env-route--empty')).not.toBeNull();
  });

  it('cada ruta es un BOTON con su slot; pulsarla abre la MATRIZ resaltando SU fila', () => {
    const panel = mountPanel();

    // El opener es cable de app.js: la vista pide, el panel ejecuta.
    BANDS_WITH_CONTROLS
      .flat()
      .find((section) => section.visual?.element?.classList?.contains('env-curves'))
      .visual.setRouteOpener((slot) => panel.openDrawerRoute('modMatrix', slot));

    panel.paint(makeState());

    const ampRoute = document.querySelector('.env-curves__column[data-envelope="env"] .env-route');

    expect(ampRoute.dataset.slot).toBe('1');
    expect(ampRoute.tagName).toBe('BUTTON');
    expect(panel.drawers.get('modMatrix').isOpen()).toBe(false);

    ampRoute.click();

    const drawer = panel.drawers.get('modMatrix');
    expect(drawer.isOpen()).toBe(true);

    // La RUTA 1 del cajon queda resaltada (y SOLO ella); las otras tres no.
    const highlighted = [...drawer.body.querySelectorAll('.drawer-slot')]
      .filter((row) => row.dataset.slotHighlight === 'true');

    expect(highlighted).toHaveLength(1);
    expect(highlighted[0].dataset.slot).toBe('1');

    // El gesto es genérico: otro slot resalta otra fila.
    expect(panel.openDrawerRoute('modMatrix', 3)).toBe(true);
    const nowHighlighted = [...drawer.body.querySelectorAll('.drawer-slot')]
      .filter((row) => row.dataset.slotHighlight === 'true');
    expect(nowHighlighted.map((row) => row.dataset.slot)).toEqual(['3']);

    // Y un cajón que no existe es un no-op honesto.
    expect(panel.openDrawerRoute('no-existe', 1)).toBe(false);
  });

  it('la franja de estado de GLOBAL (tempo/MIDI/aleatorio) pinta el snapshot y abre el cajón', () => {
    const panel = mountPanel();
    const state = makeState();

    panel.paint(state);

    const strip = document.querySelector('.global-strip');
    const chip = (key) => strip.querySelector(`.global-strip__chip[data-key="${key}"] .global-strip__value`);

    expect(strip).not.toBeNull();

    // Defaults del contrato: 120 BPM, Omni, sin Thru, fuerza 70%.
    expect(chip('tempo').textContent).toBe('120');
    expect(chip('midi').textContent).toBe('Omni');
    expect(chip('random').textContent).toBe('70%');

    // El snapshot manda: BPM 132.5, canal 3, Thru ON, fuerza 80%.
    const bpm = describeControl('masterBPM');
    const midi = describeControl('midiChannel');
    const thru = describeControl('midiThru');
    const strength = describeControl('randomStrength');
    const channel3 = 3 / (midi.options.length - 1); // índice 3 = "3" (0 = Omni)

    panel.paint({
      ...state,
      parameters: {
        ...state.parameters,
        masterBPM: (132.5 - bpm.min) / (bpm.max - bpm.min),
        midiChannel: channel3,
        midiThru: 1,
        randomStrength: 0.8,
      },
    });
    // El formateo del contrato es el de las celdas (redondeo al entero).
    expect(chip('tempo').textContent).toBe('132');
    expect(chip('midi').textContent).toContain('3');
    expect(chip('midi').textContent).toContain('THRU');
    expect(chip('random').textContent).toBe('80%');

    // La franja es clicable: abre el cajón de GLOBAL (patrón resumen de matriz).
    expect(panel.drawers.get('globalFull').isOpen()).toBe(false);
    strip.click();
    expect(panel.drawers.get('globalFull').isOpen()).toBe(true);
  });

  it('la BARRA de nivel del cajón de la MATRIZ vive SOLO en filas con fuente ENV (frame al instante)', () => {
    const panel = mountPanel();
    const state = makeState();
    const barOf = (slot) => document
      .querySelector(`.drawer-slot[data-slot="${slot}"] .drawer-slot__env-level`);

    // El contrato por defecto deja ENV 1 en la ruta 1 y ENV 2 en la ruta 2.
    panel.paint(state);

    expect(barOf('1').dataset.live).toBe('true');
    expect(barOf('2').dataset.live).toBe('true');
    expect(barOf('3').dataset.live).toBe('false');
    expect(barOf('4').dataset.live).toBe('false');

    // Sin frames todavía (level 0): la barra existe pero vacía.
    const emptyLevel = barOf('1').style.getPropertyValue('--env-level');
    expect(Number(emptyLevel)).toBe(0);

    // Un frame REPAINTA el nivel SIN paint intermedio (el canal es su dueño):
    // cada fila pinta SU envolvente del par [amp, filter]. El canal exige el
    // frame completo (spectral de 64, contrato del puente).
    pushTelemetryFrame({ spectral: new Array(64).fill(0), envelopes: [0.25, 0.9] });
    expect(Number(barOf('1').style.getPropertyValue('--env-level'))).toBeCloseTo(0.25, 5);
    expect(Number(barOf('2').style.getPropertyValue('--env-level'))).toBeCloseTo(0.9, 5);

    // La fuente manda: ruta 1 pasa a LFO 1 (1/7), la ruta 2 se apaga y ENV 2
    // se muda a la ruta 4 (la matriz permite duplicar: la mudanza es apagar
    // el slot viejo, no moverlo).
    panel.paint({
      ...state,
      parameters: { ...state.parameters, mod1Source: 1 / 7, mod2Source: 0, mod4Source: 1.0 },
    });
    expect(barOf('1').dataset.live).toBe('false');
    expect(barOf('2').dataset.live).toBe('false');
    expect(barOf('4').dataset.live).toBe('true');
    // ENV 2 sigue en el índice 1 del par: su barra pinta el ÚLTIMO nivel recibido.
    expect(Number(barOf('4').style.getPropertyValue('--env-level'))).toBeCloseTo(0.9, 5);

    resetTelemetry();
  });

  it('las filas del RESUMEN de la matriz son botones: clic abre el cajón en SU slot', () => {
    const panel = mountPanel();
    const state = makeState();
    const summaryView = BANDS_WITH_CONTROLS
      .flat()
      .find((section) => section.visual?.element?.classList?.contains('mod-summary'))
      .visual;

    // El opener es cable de app.js (tarde): la vista pide, el panel ejecuta.
    summaryView.setRouteOpener((slot) => panel.openDrawerRoute('modMatrix', slot));
    panel.paint(state);

    const rowOf = (slot) => document.querySelector(`.mod-summary__row[data-slot="${slot}"]`);

    // Son BOTONES (semántica + teclado gratis) con su tooltip.
    expect(rowOf('2').tagName).toBe('BUTTON');
    expect(rowOf('2').title).toContain('RUTA 2');
    expect(panel.drawers.get('modMatrix').isOpen()).toBe(false);

    rowOf('2').click();

    const drawer = panel.drawers.get('modMatrix');
    expect(drawer.isOpen()).toBe(true);
    expect([...drawer.body.querySelectorAll('.drawer-slot')]
      .filter((row) => row.dataset.slotHighlight === 'true').map((row) => row.dataset.slot))
      .toEqual(['2']);

    // Otra fila, otro slot (el número sale de la propia fila, no de una lista).
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    expect(drawer.isOpen()).toBe(false);
    rowOf('4').click();
    expect([...drawer.body.querySelectorAll('.drawer-slot')]
      .filter((row) => row.dataset.slotHighlight === 'true').map((row) => row.dataset.slot))
      .toEqual(['4']);

    // El opener tardío: un paint no lo desconecta.
    panel.paint(state);
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    rowOf('1').click();
    expect(drawer.isOpen()).toBe(true);
  });

  it('el opener llega TARDE (setRouteOpener) y un paint no lo desconecta', () => {
    const panel = mountPanel();
    const state = makeState();
    const view = BANDS_WITH_CONTROLS
      .flat()
      .find((section) => section.visual?.element?.classList?.contains('env-curves'))
      .visual;

    panel.paint(state);

    // Orden real: la vista nace sin opener y app.js lo conecta despues.
    view.setRouteOpener(null);

    let requested = 0;
    view.setRouteOpener((slot) => { requested = slot; });

    document.querySelector('.env-curves__column[data-envelope="env"] .env-route').click();
    expect(requested).toBe(1);

    // Un repintado (preset, edit) NO desconecta el wiring ni rompe el botón.
    panel.paint(state);
    document.querySelector('.env-curves__column[data-envelope="filter"] .env-route').click();
    expect(requested).toBe(2);
  });

  it('el resalte es un GESTO: muere al cerrar el cajon, no hereda al reabrir', () => {
    const panel = mountPanel();
    const drawer = panel.drawers.get('modMatrix');
    const route1 = drawer.body.querySelector('.drawer-slot[data-slot="1"]');

    panel.openDrawerRoute('modMatrix', 1);
    expect(route1.dataset.slotHighlight).toBe('true');

    // ESC (o el botón de cierre) limpia: al reabrir por su EDIT la fila no
    // hereda un borde de un gesto viejo.
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    expect(drawer.isOpen()).toBe(false);
    expect(route1.dataset.slotHighlight).toBe('false');

    document.querySelector('[data-drawer-trigger="modMatrix"]').click();
    expect(drawer.isOpen()).toBe(true);
    expect(route1.dataset.slotHighlight).toBe('false');
  });

  it('IR A LA RUTA con retorno: cerrar la matriz REABRE el cajón de ENVOLVENTES', () => {
    const panel = mountPanel();
    const state = makeState();
    const envBlocksView = BANDS_WITH_CONTROLS
      .flat()
      .find((section) => section.drawerVisual?.element?.classList?.contains('env-blocks'))
      .drawerVisual;

    // El opener del cajón pide la vuelta (así lo cablea app.js).
    envBlocksView.setRouteOpener((slot) =>
      panel.openDrawerRoute('modMatrix', slot, { returnTo: 'envelopes' }));
    panel.paint(state);

    const envelopes = panel.drawers.get('envelopes');
    const matrix = panel.drawers.get('modMatrix');
    const gotoEnv = () => document.querySelector('.env-block[data-envelope="env"] .env-block__goto');

    // El usuario está en el cajón de ENVOLVENTES y salta a la matriz.
    document.querySelector('[data-drawer-trigger="envelopes"]').click();
    expect(envelopes.isOpen()).toBe(true);

    gotoEnv().click();
    expect(matrix.isOpen()).toBe(true);
    expect(envelopes.isOpen()).toBe(false);   // el origen cede el sitio

    // Cierre de usuario (ESC/✕/velo): vuelve a ENVOLVENTES, sin resalte heredado.
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    expect(matrix.isOpen()).toBe(false);
    expect(envelopes.isOpen()).toBe(true);
    expect([...matrix.body.querySelectorAll('.drawer-slot')]
      .every((row) => row.dataset.slotHighlight === 'false')).toBe(true);
  });

  it('sin returnTo (ruta del lienzo): cerrar la matriz NO abre ENVOLVENTES', () => {
    const panel = mountPanel();
    const state = makeState();
    const view = BANDS_WITH_CONTROLS
      .flat()
      .find((section) => section.visual?.element?.classList?.contains('env-curves'))
      .visual;

    // El opener del LIENZO no pide retorno: el usuario nunca abrió ENVOLVENTES.
    view.setRouteOpener((slot) => panel.openDrawerRoute('modMatrix', slot));
    panel.paint(state);

    document.querySelector('.env-curves__column[data-envelope="env"] .env-route').click();
    expect(panel.drawers.get('modMatrix').isOpen()).toBe(true);

    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    expect(panel.drawers.get('modMatrix').isOpen()).toBe(false);
    expect(panel.drawers.get('envelopes').isOpen()).toBe(false);
  });

  it('el retorno se consume UNA vez: reabrir y cerrar ENVOLVENTES no revive la matriz', () => {
    const panel = mountPanel();
    const state = makeState();
    const envBlocksView = BANDS_WITH_CONTROLS
      .flat()
      .find((section) => section.drawerVisual?.element?.classList?.contains('env-blocks'))
      .drawerVisual;

    envBlocksView.setRouteOpener((slot) =>
      panel.openDrawerRoute('modMatrix', slot, { returnTo: 'envelopes' }));
    panel.paint(state);

    const envelopes = panel.drawers.get('envelopes');
    const matrix = panel.drawers.get('modMatrix');

    document.querySelector('[data-drawer-trigger="envelopes"]').click();
    document.querySelector('.env-block[data-envelope="env"] .env-block__goto').click();
    expect(matrix.isOpen()).toBe(true);

    // Primera vuelta: la consume.
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    expect(envelopes.isOpen()).toBe(true);

    // Y cerrar ENVOLVENTES ya es un cierre normal: no rebota a la matriz.
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    expect(envelopes.isOpen()).toBe(false);
    expect(matrix.isOpen()).toBe(false);
  });

  it('un salto nuevo sustituye al pendiente: el retorno viejo muere con el gesto', () => {
    const panel = mountPanel();
    const state = makeState();
    const envBlocksView = BANDS_WITH_CONTROLS
      .flat()
      .find((section) => section.drawerVisual?.element?.classList?.contains('env-blocks'))
      .drawerVisual;

    envBlocksView.setRouteOpener((slot) =>
      panel.openDrawerRoute('modMatrix', slot, { returnTo: 'envelopes' }));
    panel.paint(state);

    const envelopes = panel.drawers.get('envelopes');
    const matrix = panel.drawers.get('modMatrix');

    document.querySelector('[data-drawer-trigger="envelopes"]').click();
    document.querySelector('.env-block[data-envelope="env"] .env-block__goto').click();
    expect(matrix.isOpen()).toBe(true);

    // Otro gesto SIN retorno (p. ej. una ruta del lienzo) sustituye al pendiente.
    panel.openDrawerRoute('modMatrix', 2);

    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    expect(matrix.isOpen()).toBe(false);
    expect(envelopes.isOpen()).toBe(false);
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

  it('el CAJON de ENVOLVENTES: un bloque por ADSR, su curva encima de sus knobs', () => {
    const panel = mountPanel();
    const drawer = panel.drawers.get('envelopes');

    expect(drawer.isOpen()).toBe(false);

    const blocks = drawer.body.querySelectorAll('.env-block');
    expect(blocks).toHaveLength(2);

    // El bloque 1 lleva las cuatro de la ENV 1; el 2, las del filtro.
    const knobIds = (block) => [...block.querySelectorAll('[data-parameter-id]')]
      .map((el) => el.dataset.parameterId);

    expect(knobIds(blocks[0]))
      .toEqual(['envAttack', 'envDecay', 'envSustain', 'envRelease']);
    expect(knobIds(blocks[1]))
      .toEqual(['filterAttack', 'filterDecay', 'filterSustain', 'filterRelease']);

    // Cada bloque lleva SU curva encima (dataset de cajón).
    expect(blocks[0].querySelector('[data-visual="drawer-envelope"]')).not.toBeNull();
    expect(blocks[1].querySelector('[data-visual="drawer-envelope"]')).not.toBeNull();

    // Las celdas del cajon cuentan para el total: una vez cada id.
    expect(document.querySelectorAll('.cell')).toHaveLength(SECTION_PARAMETER_IDS.length);
    for (const id of ['envAttack', 'filterRelease'])
      expect(document.querySelectorAll(`[data-parameter-id="${id}"]`)).toHaveLength(1);
  });

  it('el EDIT de ENVOLVENTES abre SU cajon', () => {
    const panel = mountPanel();

    document.querySelector('[data-drawer-trigger="envelopes"]').click();
    expect(panel.drawers.get('envelopes').isOpen()).toBe(true);
  });

  it('IR A LA RUTA: deshabilitado sin ruta, y con ruta abre la MATRIZ en SU slot', () => {
    const panel = mountPanel();
    const state = makeState();
    const envBlocksView = BANDS_WITH_CONTROLS
      .flat()
      .find((section) => section.drawerVisual?.element?.classList?.contains('env-blocks'))
      .drawerVisual;

    // El opener es cable de app.js (tarde): la vista pide, el panel ejecuta.
    envBlocksView.setRouteOpener((slot) => panel.openDrawerRoute('modMatrix', slot));

    panel.paint(state);

    const gotoFor = (env) => document.querySelector(`.env-block[data-envelope="${env}"] .env-block__goto`);

    // Defaults del contrato: las dos envolventes tienen ruta -> habilitados.
    expect(gotoFor('env').disabled).toBe(false);
    expect(gotoFor('filter').disabled).toBe(false);
    expect(gotoFor('env').textContent).toBe('IR A LA RUTA');

    gotoFor('env').click();

    const drawer = panel.drawers.get('modMatrix');
    expect(drawer.isOpen()).toBe(true);
    const highlighted = [...drawer.body.querySelectorAll('.drawer-slot')]
      .filter((row) => row.dataset.slotHighlight === 'true');
    expect(highlighted.map((row) => row.dataset.slot)).toEqual(['1']);

    // La ENV 2 apunta al slot 2 (su ruta vive ahi), no al 1.
    gotoFor('filter').click();
    const nowHighlighted = [...drawer.body.querySelectorAll('.drawer-slot')]
      .filter((row) => row.dataset.slotHighlight === 'true');
    expect(nowHighlighted.map((row) => row.dataset.slot)).toEqual(['2']);

    // El paint reevalua: si la matriz deja de tener ruta para ENV 2, su boton
    // se apaga (la verdad la dice el estado, no el primer render).
    panel.paint({
      ...state,
      parameters: { ...state.parameters, mod2Source: 0 },
    });
    expect(gotoFor('filter').disabled).toBe(true);
  });

  it('IR A LA RUTA resuelve el slot EN EL CLIC (no el que había al pintar)', () => {
    const panel = mountPanel();
    const state = makeState();
    const envBlocksView = BANDS_WITH_CONTROLS
      .flat()
      .find((section) => section.drawerVisual?.element?.classList?.contains('env-blocks'))
      .drawerVisual;

    envBlocksView.setRouteOpener((slot) => panel.openDrawerRoute('modMatrix', slot));

    // El boton se pinta con la ruta de ENV 1 en el slot 1...
    panel.paint(state);
    expect(document.querySelector('.env-block[data-envelope="env"] .env-block__goto').disabled).toBe(false);

    // ...la matriz cambia (la ENV 1 se muda al slot 3)... y el cajon de
    // ENVOLVENTES NO se repinta (esta cerrado, el usuario no lo ve): el paint
    // del lienzo si corrio, pero supongamos el peor caso: el clic llega sin
    // paint intermedio. El boton resuelve contra lastParameters, que es del
    // ULTIMO paint; aqui forzamos ese caso apagando el snapshot viejo y
    // comprobando que el clic no inventa un slot.
    envBlocksView.paint({ ...state.parameters, mod1Source: 0 });

    document.querySelector('.env-block[data-envelope="env"] .env-block__goto').click();

    // Sin ruta ya: el click no lleva a ninguna parte (boton deshabilitado
    // ademas, pero un click sintetico lo atraviesa).
    expect(panel.drawers.get('modMatrix').isOpen()).toBe(false);
  });

  it('la AGUJA de nivel vive del canal de telemetria (envelopes=[amp, filter])', () => {
    // El harness del panel NO conecta el canal (como en modo local sin frames):
    // la aguja nace oculta en las dos curvas.
    mountPanel();

    for (const visual of document.querySelectorAll('[data-visual="amp-envelope"], [data-visual="filter-envelope"]'))
      expect(visual.querySelector('.envelope-curve__level').dataset.visible).toBe('false');
  });

  it('con frames de telemetria la aguja de CADA curva sigue SU nivel del frame', () => {
    // fabrica = la misma que app.js: canal propio de la prueba, como hace
    // xyPad.test.js con el pad.
    let emit = null;
    const curve = createVisual('envelope-curves', [
      ...['envAttack', 'envDecay', 'envSustain', 'envRelease',
        'filterAttack', 'filterDecay', 'filterSustain', 'filterRelease'].map(describeControl),
    ], {
      routeControls: SECTIONS.find((section) => section.id === 'modMatrix')
        .ids.map(describeControl),
      onTelemetry: (notify) => { emit = notify; return () => {}; },
    });

    document.body.append(curve.element);

    const needle = (env) => curve.element
      .querySelector(`.env-curves__column[data-envelope="${env}"] .envelope-curve__level`);

    // frame.envelopes es [amp, filter] en ESE orden (contrato del puente).
    emit({ envelopes: [0.25, 0.9] });

    expect(needle('env').dataset.visible).toBe('true');
    expect(needle('filter').dataset.visible).toBe('true');
    const yOf = (env) => Number(/M0,([\d.]+) L/.exec(needle(env).getAttribute('d'))[1]);
    const yAmp = yOf('env');
    const yFilter = yOf('filter');
    // y crece hacia abajo: el filtro (0.9) pinta MAS ARRIBA (y menor).
    expect(yFilter).toBeLessThan(yAmp);

    // Sin array (frame degenerado): silencio, no excepcion.
    emit({ envelopes: [] });
    expect(needle('env').dataset.visible).toBe('false');
    expect(needle('filter').dataset.visible).toBe('false');

    curve.destroy();
  });

  it('la AGUJA del CAJÓN vive del mismo canal: cada bloque pinta SU nivel', () => {
    // El harness conecta la vista del cajón al canal de la prueba (como app.js
    // le pasa store.onTelemetry).
    mountPanel();

    const needle = (env) => document
      .querySelector(`.env-block[data-envelope="${env}"] .envelope-curve__level`);
    expect(needle('env')).not.toBeNull();
    expect(needle('filter')).not.toBeNull();

    // Nace oculta (sin frames): el cajón cerrado no miente.
    expect(needle('env').dataset.visible).toBe('false');
    expect(needle('filter').dataset.visible).toBe('false');

    // frame.envelopes es [amp, filter] en ESE orden (contrato del puente).
    emitTelemetry({ envelopes: [0.25, 0.9] });

    expect(needle('env').dataset.visible).toBe('true');
    expect(needle('filter').dataset.visible).toBe('true');
    const yOf = (env) => Number(/M0,([\d.]+) L/.exec(needle(env).getAttribute('d'))[1]);
    // y crece hacia abajo: el filtro (0.9) pinta MAS ARRIBA (y menor).
    expect(yOf('filter')).toBeLessThan(yOf('env'));

    // Frame degenerado: silencio, no excepción.
    emitTelemetry({ envelopes: [] });
    expect(needle('env').dataset.visible).toBe('false');
    expect(needle('filter').dataset.visible).toBe('false');
  });
});

describe('panel / la ficha MODELOS (pad en el lienzo, detalle en el cajon)', () => {
  it('el pad vive en su ficha del lienzo, sin contar como celda de parametro', () => {
    mountPanel();

    const card = document.querySelector('.card[data-section-id="models"]');
    const view = card.querySelector('[data-visual="model-xy"]');

    expect(view).not.toBeNull();
    // No es una celda: el lienzo sigue teniendo 70 celdas de PARAMETRO.
    expect(document.querySelectorAll('.cell')).toHaveLength(SECTION_PARAMETER_IDS.length);
  });

  it('las ranuras viven en el CAJON de la ficha (mismo DOM, mismas variables)', () => {
    const panel = mountPanel();
    const view = () => panel.drawers.get('models').body.querySelector('[data-visual="model-slots"]');

    // El detalle esta montado UNA vez en el cajon: siempre en el documento (el
    // selftest del host y la suite cuentan celdas aunque el cajon este cerrado).
    expect(panel.drawers.has('models')).toBe(true);
    expect(view().querySelectorAll('.model-slots__row')).toHaveLength(4);
    expect(document.querySelectorAll('.cell')).toHaveLength(SECTION_PARAMETER_IDS.length);

    // El trigger de la ficha abre SU cajon.
    document.querySelector('[data-drawer-trigger="models"]').click();
    expect(panel.drawers.get('models').isOpen()).toBe(true);
  });

  it('recibe el ESTADO del store, no solo los parametros (vive fuera del APVTS)', () => {
    const panel = mountPanel();
    const view = () => panel.drawers.get('models').body.querySelector('[data-visual="model-slots"]');

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

    const card = document.querySelector('.card[data-section-id="globalFull"]');
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
    // Los EDITAR de cajon comparten clase con RANDOM; el sujeto es la accion.
    const button = document.querySelector('[data-action="randomize"]');

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

describe('panel / caja LFO (frontal + cajon)', () => {
  it('el frontal vive en el lienzo, el resto en el cajon, y no hay duplicados', () => {
    const panel = mountPanel();

    const card = document.querySelector('.card[data-section-id="lfo"]');
    const frontalIds = ['lfo1RateHz', 'lfo1Depth', 'lfo2RateHz', 'lfo2Depth'];

    // Los cuatro del frontal estan en la rejilla de la ficha (lienzo)...
    for (const id of frontalIds)
      expect(card.querySelector(`[data-parameter-id="${id}"]`)).not.toBeNull();

    // ...una sola vez en todo el documento (el cajon no recibe copia).
    for (const id of frontalIds)
      expect(document.querySelectorAll(`[data-parameter-id="${id}"]`)).toHaveLength(1);

    // El cajon lleva los seis del detalle (forma, sync, division x2).
    const drawerBody = panel.drawers.get('lfo').body;
    const drawerIds = [...drawerBody.querySelectorAll('[data-parameter-id]')].map((el) => el.dataset.parameterId);
    expect(drawerIds).toEqual(['lfo1Waveform', 'lfo1SyncMode', 'lfo1RhythmicDivision', 'lfo2Waveform', 'lfo2SyncMode', 'lfo2RhythmicDivision']);

    // Y el lienzo sigue teniendo exactamente 70 celdas de parametro.
    expect(document.querySelectorAll('.cell')).toHaveLength(SECTION_PARAMETER_IDS.length);
  });

  it('el trigger EDIT de la ficha abre SU cajon', () => {
    const panel = mountPanel();

    document.querySelector('[data-drawer-trigger="lfo"]').click();
    expect(panel.drawers.get('lfo').isOpen()).toBe(true);
  });
});
describe('panel / recorrido E2E del cajon GLOBAL & MASTER (EDITAR -> drawer -> Freeze)', () => {
  it('el EDIT de la ficha abre su cajon, el Freeze se conmuta alli y sobrevive al cierre', () => {
    const onChange = vi.fn();
    const panel = mountPanel({ onChange });
    const drawer = panel.drawers.get('globalFull');

    // 0. Estado inicial: cajon cerrado y las celdas YA en el documento (el
    //    contenido NO se reconstruye al abrir: contrato del cajon compartido).
    expect(drawer.isOpen()).toBe(false);
    const freezeCell = drawer.body.querySelector('[data-parameter-id="freezeResonator"]');
    expect(freezeCell).not.toBeNull();

    // 1. El EDIT (lapiz) de la ficha abre SU cajon.
    document.querySelector('[data-drawer-trigger="globalFull"]').click();
    expect(drawer.isOpen()).toBe(true);
    expect(drawer.element.classList.contains('drawer--open')).toBe(true);
    expect(drawer.element.getAttribute('aria-hidden')).toBe('false');
    // El velo acompana al cajon abierto (clase del mueble compartido).
    expect(drawer.backdrop.classList.contains('drawer-backdrop--visible')).toBe(true);

    // 2. El cajon apila TODOS los ids menos el control base (8 celdas n1..n8);
    //    masterLevel solo existe en la ficha (contrato 8.1 paso 2c).
    expect(drawer.body.querySelectorAll('.drawer-slot')).toHaveLength(8);
    expect(drawer.body.querySelector('[data-parameter-id="masterLevel"]')).toBeNull();

    // 3. Freeze Resonator: Off -> On con un click real en el toggle compartido.
    const freezeButton = freezeCell.querySelector('.abd-toggle');
    expect(freezeButton.getAttribute('aria-pressed')).toBe('false');

    freezeButton.click();
    expect(onChange).toHaveBeenCalledWith('freezeResonator', 1);
    expect(freezeButton.getAttribute('aria-pressed')).toBe('true');

    // 4. Los otros dos congelados siguen Off y se conmutan a la vez.
    const filterButton = drawer.body
      .querySelector('[data-parameter-id="freezeFilter"] .abd-toggle');
    expect(filterButton.getAttribute('aria-pressed')).toBe('false');

    filterButton.click();
    expect(onChange).toHaveBeenLastCalledWith('freezeFilter', 1);
    // El primero no se despierta por el segundo (independencia de toggles).
    expect(freezeButton.getAttribute('aria-pressed')).toBe('true');

    // 5. ESC cierra el cajon; el DOM persiste y al reabrir el Freeze sigue On.
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    expect(drawer.isOpen()).toBe(false);
    expect(freezeButton.getAttribute('aria-pressed')).toBe('true');

    document.querySelector('[data-drawer-trigger="globalFull"]').click();
    expect(drawer.isOpen()).toBe(true);
    expect(drawer.body.querySelector('[data-parameter-id="freezeResonator"] .abd-toggle')
      .getAttribute('aria-pressed')).toBe('true');
  });

  it('el paint del procesador (round-trip) enciende el toggle SIN notificar', () => {
    const onChange = vi.fn();
    const panel = mountPanel({ onChange });
    const state = makeState();

    panel.paint(state);

    const button = panel.drawers.get('globalFull').body
      .querySelector('[data-parameter-id="freezeResonator"] .abd-toggle');
    expect(button.getAttribute('aria-pressed')).toBe('false');

    // El procesador confirma el congelado: setValue silencioso (sin onChange).
    panel.paint({ ...state, parameters: { ...state.parameters, freezeResonator: 1 } });
    expect(button.getAttribute('aria-pressed')).toBe('true');
    expect(onChange).not.toHaveBeenCalled();
  });
});
describe('panel / recorrido E2E de los cuatro cajones con EDIT', () => {
  // El reparto del lienzo DECLARA estos cajones; si se anade uno nuevo, su
  // recorrido de usuario (EDIT -> cajon -> editar) tiene que vivir aqui.
  // DISENO 9.x: ENVOLVENTES entra como quinto (sus ocho ADSR viven en el cajon,
  // un bloque por envolvente).
  const EDITABLE_DRAWERS = ['lfo', 'envelopes', 'models', 'modMatrix', 'globalFull'];

  it('inventario: exactamente los cuatro cajones con trigger EDIT', () => {
    mountPanel();

    const triggers = [...document.querySelectorAll('[data-drawer-trigger]')]
      .map((el) => el.dataset.drawerTrigger);

    expect(triggers).toEqual(EDITABLE_DRAWERS);
    // El badge del trigger es siempre EDIT (lapiz compartido): las fichas
    // respiran; lo que cambia es el aria-label completo.
    for (const id of EDITABLE_DRAWERS) {
      const trigger = document.querySelector(`[data-drawer-trigger="${id}"]`);
      expect(trigger.textContent).toContain('EDIT');
      expect(trigger.getAttribute('aria-label')).toBeTruthy();
    }
  });

  it('MODELOS: EDIT abre el detalle, la ranura cargada se habilita y CARGAR pide la carga', () => {
    const onLoad = vi.fn();
    const panel = mountPanel({ onLoad });
    const drawer = panel.drawers.get('models');

    expect(drawer.isOpen()).toBe(false);
    // En el lienzo SOLO el pad XY; el detalle (espectral + ranuras) es del cajon.
    expect(document.querySelectorAll('[data-section-id="models"] .model-slots__row'))
      .toHaveLength(0);

    document.querySelector('[data-drawer-trigger="models"]').click();
    expect(drawer.isOpen()).toBe(true);

    const body = drawer.body;
    expect(body.querySelectorAll('.model-slots__row')).toHaveLength(4);

    // Sin puente: CARGAR muerto. Con puente + ranura cargada: habilitado y pide.
    const load0 = body.querySelector('[data-slot="0"] .model-slots__load');
    panel.paint(makeState({ bridgeAvailable: false }));
    expect(load0.disabled).toBe(true);

    panel.paint(makeState({
      bridgeAvailable: true,
      models: [{ slot: 0, name: 'Campana', isValid: true, amplitudes: [], frequencyOffsets: [] }],
    }));
    expect(load0.disabled).toBe(false);
    load0.click();
    expect(onLoad).toHaveBeenCalledWith(0);

    // ESC cierra y el estado de las ranuras no se toca (DOM estable).
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    expect(drawer.isOpen()).toBe(false);
    expect(body.querySelector('[data-slot="0"] .model-slots__name').textContent).toBe('Campana');
  });

  it('LFO: el cajon recibe SOLO forma/sync/division (el frontal no se replica) y se edita', () => {
    const onChange = vi.fn();
    const panel = mountPanel({ onChange });
    const drawer = panel.drawers.get('lfo');

    document.querySelector('[data-drawer-trigger="lfo"]').click();
    expect(drawer.isOpen()).toBe(true);

    const ids = [...drawer.body.querySelectorAll('[data-parameter-id]')]
      .map((el) => el.dataset.parameterId);

    // Los cuatro del frontal viven en el LIENZO (un control, un nodo DOM).
    for (const frontal of ['lfo1RateHz', 'lfo1Depth', 'lfo2RateHz', 'lfo2Depth']) {
      expect(ids).not.toContain(frontal);
      expect(document.querySelector(`[data-section-id="lfo"] [data-parameter-id="${frontal}"]`))
        .not.toBeNull();
    }
    // En el cajon: las ondas (Segmented LED) y sync + division de cada LFO.
    expect(ids).toEqual([
      'lfo1Waveform', 'lfo1SyncMode', 'lfo1RhythmicDivision',
      'lfo2Waveform', 'lfo2SyncMode', 'lfo2RhythmicDivision',
    ]);

    // Edicion real: onda 0 -> 2 (Saw Up), 2/5 normalizado en el contrato.
    const wave = drawer.body
      .querySelector('[data-parameter-id="lfo1Waveform"] .abd-segmented');
    expect(wave.classList.contains('abd-segmented--led')).toBe(true);
    expect(wave.querySelectorAll('button')).toHaveLength(6);

    [...wave.querySelectorAll('button')][2].click();
    expect(onChange).toHaveBeenCalledWith('lfo1Waveform', 2 / 5);
  });

  it('MATRIZ: cuatro rutas, edicion fuente->destino y cantidad por teclado del knob', () => {
    const onChange = vi.fn();
    const panel = mountPanel({ onChange });
    const drawer = panel.drawers.get('modMatrix');

    document.querySelector('[data-drawer-trigger="modMatrix"]').click();
    expect(drawer.isOpen()).toBe(true);

    // Una fila RUTA n por ruta, con sus tres campos EN ORDEN del contrato.
    const rows = [...drawer.body.querySelectorAll('.drawer-slot')];
    expect(rows).toHaveLength(4);
    const ids = rows.flatMap((row) =>
      [...row.querySelectorAll('[data-parameter-id]')].map((el) => el.dataset.parameterId));

    expect(ids).toEqual([
      'mod1Source', 'mod1Destination', 'mod1Amount',
      'mod2Source', 'mod2Destination', 'mod2Amount',
      'mod3Source', 'mod3Destination', 'mod3Amount',
      'mod4Source', 'mod4Destination', 'mod4Amount',
    ]);
    // La ficha del lienzo queda sin celdas (solo resumen + EDIT).
    expect(document.querySelectorAll('[data-section-id="modMatrix"] [data-parameter-id]'))
      .toHaveLength(0);

    // Edicion 1: fuente de la RUTA 2 a LFO 2 (indice 2 -> 2/7).
    const select = drawer.body
      .querySelector('[data-slot="2"] [data-parameter-id="mod2Source"] select');
    select.value = '2';
    select.dispatchEvent(new Event('change'));
    expect(onChange).toHaveBeenCalledWith('mod2Source', 2 / 7);

    // Edicion 2: cantidad de la RUTA 1 con el knob (teclado, el gesto accesible).
    const dial = drawer.body
      .querySelector('[data-slot="1"] [data-parameter-id="mod1Amount"] .abd-knob__dial');
    dial.focus();
    dial.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowUp', bubbles: true }));
    const fired = onChange.mock.calls.filter(([id]) => id === 'mod1Amount');
    expect(fired.length).toBeGreaterThan(0);
    const [id, value] = fired.at(-1);
    expect(id).toBe('mod1Amount');
    expect(value).toBeGreaterThan(0);
    expect(value).toBeLessThanOrEqual(1);
  });

  it('GLOBAL: los tres congelados viven en el cajon y conmutan de forma independiente', () => {
    const onChange = vi.fn();
    const panel = mountPanel({ onChange });
    const drawer = panel.drawers.get('globalFull');

    document.querySelector('[data-drawer-trigger="globalFull"]').click();
    expect(drawer.isOpen()).toBe(true);

    const body = drawer.body;
    expect(body.querySelectorAll('.drawer-slot')).toHaveLength(8);
    const toggles = ['freezeResonator', 'freezeFilter', 'freezeEnvelopes']
      .map((id) => body.querySelector(`[data-parameter-id="${id}"] .abd-toggle`));

    expect(toggles.every((b) => b.getAttribute('aria-pressed') === 'false')).toBe(true);
    toggles[0].click();
    toggles[2].click();
    expect(onChange).toHaveBeenCalledWith('freezeResonator', 1);
    expect(onChange).toHaveBeenCalledWith('freezeEnvelopes', 1);
    // El del medio no se despierta (aislamiento de estado).
    expect(toggles[1].getAttribute('aria-pressed')).toBe('false');
  });

  it('aislamiento: un cajon abierto no deja el turno abierto al cerrarse otro', () => {
    const panel = mountPanel();
    const models = panel.drawers.get('models');
    const global = panel.drawers.get('globalFull');

    document.querySelector('[data-drawer-trigger="models"]').click();
    expect(models.isOpen()).toBe(true);

    document.querySelector('[data-drawer-trigger="globalFull"]').click();
    expect(global.isOpen()).toBe(true);

    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    // ESC cierra AMBOS (listener global) o solo el activo, pero no queda ninguno
    // abierto sin foco: el estado de vista no puede divergir del DOM.
    const open = EDITABLE_DRAWERS
      .filter((id) => panel.drawers.get(id).isOpen());
    expect(open.length).toBeLessThanOrEqual(1);
  });
});
