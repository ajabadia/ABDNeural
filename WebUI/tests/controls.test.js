/**
 * Celdas de control sobre la familia compartida: el tipo sale del contrato y los
 * valores van y vienen en las dos escalas (estado normalizado, control real).
 *
 * Lo que se pincha aquí es lo que puede romper el puente sin que se note:
 *
 *   - el tipo elegido por descriptor (float -> knob, bool -> toggle, choice ->
 *     desplegable), con el recuento real del contrato;
 *   - que un edit de USUARIO sale como onChange con el valor NORMALIZADO 0..1
 *     (el store y el cable no entienden unidades reales);
 *   - que un valor que llega del host NO dispara onChange (si lo hiciera, cada
 *     snapshot nativo rebotaría al plugin como un edit del usuario).
 */

import { afterEach, describe, expect, it, vi } from 'vitest';

import { describeControl } from '../src/contracts/parameters.js';
import { PARAMETERS } from '../src/contracts/parameters.js';
import { KINDS, createParameterControl, kindForControl } from '../src/ui/controls.js';

const build = (id, handlers = {}) => createParameterControl(describeControl(id), handlers);

afterEach(() => {
  document.body.innerHTML = '';
});

describe('controls / tipo por descriptor', () => {
  it('reparte los 70 en monos de la familia compartida', () => {
    const kinds = PARAMETERS.map((descriptor) => kindForControl(descriptor));

    expect(kinds.filter((kind) => kind === KINDS.knob)).toHaveLength(46);
    expect(kinds.filter((kind) => kind === KINDS.toggle)).toHaveLength(5);
    expect(kinds.filter((kind) => kind === KINDS.choice)).toHaveLength(19);
  });

  it('un float es un knob compartido con su dial', () => {
    const control = build('filterCutoff');

    expect(control.kind).toBe(KINDS.knob);
    expect(control.element.querySelector('.abd-knob__dial')).not.toBeNull();
  });

  it('un bool es un Toggle compartido (LED) y un choice un desplegable', () => {
    expect(build('unisonEnabled').element.querySelector('.abd-toggle')).not.toBeNull();
    expect(build('mod1Destination').element.querySelector('select')).not.toBeNull();
  });

  it('el desplegable lleva las opciones del contrato, en orden', () => {
    const descriptor = describeControl('lfo1Waveform');
    const select = build('lfo1Waveform').element.querySelector('select');

    expect([...select.options].map((option) => option.textContent)).toEqual(descriptor.options);
  });

  it('el desplegable es el Select COMPARTIDO, con su label asociada', () => {
    const control = build('mod1Destination');
    const field = control.element.querySelector('select');

    expect(control.element.querySelector('.abd-select__field')).toBe(field);
    expect(control.element.querySelector('.abd-select--labelled')).not.toBeNull();
    expect(field.id).toBe('control-mod1Destination');
    expect(control.element.querySelector('label').htmlFor).toBe(field.id);
  });

  it('cada elección sale con el ÍNDICE del control y el contrato la normaliza', () => {
    const onChange = vi.fn();
    const control = build('lfo1Waveform', { onChange });

    document.body.append(control.element);

    const field = control.element.querySelector('select');
    field.value = '2';   // el modelo del control es el índice, no 0..1
    field.dispatchEvent(new Event('change'));

    expect(onChange).toHaveBeenCalledWith('lfo1Waveform', 2 / 5);
  });

  it('marca los parámetros que el motor no consume', () => {
    const divergent = PARAMETERS.find((descriptor) => descriptor.dspStatus !== 'implemented');

    expect(divergent, 'el contrato ya no declara divergencias: revisar este test').toBeDefined();
    expect(build(divergent.id).element.classList.contains('cell--divergent')).toBe(true);
  });
});

describe('controls / valores en las dos escalas', () => {
  it('un snapshot normalizado llega al control y a su lectura, sin disparar onChange', () => {
    const onChange = vi.fn();
    const control = build('masterBPM', { onChange });

    document.body.append(control.element);
    control.setNormalized(0.5);

    // masterBPM es un rango real: se lee en unidades reales, no en 0..1.
    expect(control.element.querySelector('.cell__readout').textContent).toMatch(/[0-9]/);
    expect(onChange).not.toHaveBeenCalled();
  });

  it('un click en el Toggle sale NORMALIZADO 0..1', () => {
    const onChange = vi.fn();
    const control = build('freezeFilter', { onChange });

    document.body.append(control.element);
    control.element.querySelector('.abd-toggle').click();

    expect(onChange).toHaveBeenCalledWith('freezeFilter', 1);
  });

  it('elegir una opción sale normalizado, en la codificación del APVTS', () => {
    const onChange = vi.fn();
    const control = build('lfo1Waveform', { onChange });

    document.body.append(control.element);

    const select = control.element.querySelector('select');
    select.value = '3';   // opción 3 de 6 -> 3/5
    select.dispatchEvent(new Event('change'));

    expect(onChange).toHaveBeenCalledWith('lfo1Waveform', 3 / 5);
  });

  it('el desplegable se reposiciona desde el estado normalizado', () => {
    const control = build('midiChannel', {});

    document.body.append(control.element);
    control.setNormalized(1);   // último canal: índice 16 de 17

    expect(control.element.querySelector('select').value).toBe('16');
  });
});

describe('controls / gating por motor', () => {
  // engineType tiene dos opciones: su normalizado 0 es NEURONiK y el 1, Neurotik.
  const NEURONIK = 0;
  const NEUROTIK = 1;

  const mount = (id, handlers = {}) => {
    const control = build(id, handlers);

    document.body.append(control.element);
    return control;
  };

  const optionFor = (control, label) => [...control.element.querySelectorAll('option')]
    .find((option) => option.textContent === label);

  it('el contrato dice el motor de cada opción y quién elige el motor', () => {
    const destination = describeControl('mod1Destination');

    expect(destination.optionEngines).toHaveLength(destination.options.length);
    expect(destination.engineParameter).toBe('engineType');
    expect(describeControl('engineType').optionEngines).toEqual(['neuronik', 'neurotik']);
  });

  it('un destino que el motor activo no consume queda deshabilitado, con su motivo', () => {
    const control = mount('mod1Destination');

    control.setEngine(NEURONIK);

    expect(optionFor(control, 'Excite Noise').disabled).toBe(true);   // Neurotik-only
    expect(optionFor(control, 'Odd/Even Bal').disabled).toBe(false);  // Neuronik-only
    expect(optionFor(control, 'Osc Level').disabled).toBe(false);     // de los dos

    // La nota explica el porqué, y el nombre del motor sale del CONTRATO.
    expect(optionFor(control, 'Excite Noise').title).toContain('Neurotik');

    control.setEngine(NEUROTIK);

    expect(optionFor(control, 'Excite Noise').disabled).toBe(false);
    expect(optionFor(control, 'Odd/Even Bal').disabled).toBe(true);
    expect(optionFor(control, 'Filter Cutoff').disabled).toBe(true);
    expect(optionFor(control, 'Odd/Even Bal').title).toContain('NEURONiK');
  });

  it('el valor NO se reescribe: se marca como divergente', () => {
    const onChange = vi.fn();
    const control = mount('mod1Destination', { onChange });

    control.setNormalized(23 / 27);   // "Excite Noise", del otro motor
    control.setEngine(NEURONIK);

    const field = control.element.querySelector('select');

    expect(field.value).toBe('23');   // el estado del host se conserva
    expect(field.closest('.abd-select').dataset.divergent).toBe('true');
    expect(onChange).not.toHaveBeenCalled();

    control.setEngine(NEUROTIK);       // el destino vuelve a valer
    expect(field.closest('.abd-select').dataset.divergent).toBe('false');
  });

  it('reevaluar el motor no dispara onChange (es estado, no un edit)', () => {
    const onChange = vi.fn();
    const control = mount('mod1Destination', { onChange });

    control.setEngine(NEURONIK);
    control.setEngine(NEUROTIK);
    control.setEngine(NEURONIK);

    expect(onChange).not.toHaveBeenCalled();
  });

  it('una lista sin dependencia del motor no recibe gating', () => {
    for (const id of ['mod1Source', 'mod1Amount', 'lfo1Waveform', 'midiChannel']) {
      const control = build(id);

      expect(control.engineParameter, id).toBe('');
      expect(control.setEngine, id).toBeNull();
    }
  });

  it('el selector de motor no se autodeshabilita: sus dos segmentos siguen vivos', () => {
    const control = mount('engineType');

    // No declara engineParameter (nadie lo gobierna), así que no entra en el gating.
    expect(control.engineParameter).toBe('');
    expect([...control.element.querySelectorAll('.abd-segmented__segment')]
      .every((segment) => !segment.disabled))
      .toBe(true);
  });
});

describe('controls / presentación segmented (listas de dos)', () => {
  // engineType tiene dos opciones: su normalizado 0 es NEURONiK y el 1, Neurotik.
  const mount = (id, handlers = {}) => {
    const control = createParameterControl(describeControl(id), handlers);

    document.body.append(control.element);
    return control;
  };

  it('las listas del set SEGMENTED se montan como selector segmentado', () => {
    for (const id of ['engineType', 'lfo1SyncMode', 'lfo2SyncMode']) {
      const control = mount(id);
      const segments = control.element.querySelectorAll('.abd-segmented__segment');

      expect(segments.length, id).toBe(2);
      expect(control.element.querySelector('select'), id).toBeNull();
    }
  });

  it('las listas largas y las gateadas siguen siendo Select', () => {
    // lfo1Waveform (6 opciones) y mod1Destination (28, gateada): desplegable.
    expect(mount('lfo1Waveform').element.querySelector('select')).not.toBeNull();
    expect(mount('mod1Destination').element.querySelector('select')).not.toBeNull();
  });

  it('un pick de usuario sale normalizado, en la codificación del APVTS', () => {
    const onChange = vi.fn();
    const control = mount('engineType', { onChange });

    const [, neurotik] = control.element.querySelectorAll('.abd-segmented__segment');
    neurotik.click();

    expect(onChange).toHaveBeenCalledWith('engineType', 1);
  });

  it('un snapshot normalizado activa su segmento sin disparar onChange', () => {
    const onChange = vi.fn();
    const control = mount('lfo1SyncMode', { onChange });

    control.setNormalized(1);   // Tempo Sync: índice 1 de 2

    const [, tempoSync] = control.element.querySelectorAll('.abd-segmented__segment');
    expect(tempoSync.classList.contains('is-active')).toBe(true);
    expect(onChange).not.toHaveBeenCalled();
  });

  it('el segmentado es el Segmented COMPARTIDO, con su label asociada', () => {
    const control = mount('engineType');

    expect(control.element.querySelector('.abd-segmented__group')).not.toBeNull();
    expect(control.element.querySelector('.abd-segmented--labelled')).not.toBeNull();
  });
});
