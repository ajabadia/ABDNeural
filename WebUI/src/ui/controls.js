/**
 * Una celda de control de parámetro, hecha con la familia COMPARTIDA del suite
 * (`@abdsynths/shared/components`), la misma que documenta el barrel:
 *
 *   import { Knob, Slider, Toggle } from '@abdsynths/shared/components';
 *
 * El tipo sale del contrato generado, no de una tabla a mano:
 *
 *   - `float`  -> Knob    (46 de los 70)
 *   - `bool`   -> Toggle  (5: los congelados, unísono y MIDI Thru)
 *   - `choice` -> Select  (19: motor, sync, divisiones, canales, fuente/destino)
 *
 * Los cuatro son de la familia COMPARTIDA: un desplegable de 28 destinos no se
 * puede pedir a un knob, así que ABDSharedAssets ganó `components/select.js` (la
 * lista sigue siendo un `<select>` nativo por teclado/lectores de pantalla/WebView2;
 * el control aporta el contrato de la familia, el modelo de valor por ÍNDICE y las
 * opciones deshabilitables que necesita el gating por motor).
 *
 * Contrato de valores: el estado y el puente van en NORMALIZADO 0..1, el control
 * muestra y edita UNIDADES REALES. Toda la conversión pasa por paramValue.js, que
 * usa la misma matemática de NormalisableRange que el APVTS, así que una celda no
 * puede emitir un valor que el plugin rechace.
 *
 * Contrato de gestos (el mismo camino que ya usa el fader base, que es el que el
 * selftest del host ejercita por los dos lados):
 *   - arrastre de knob: onDragStart -> 'begin', onChange -> handleChange, onDragEnd -> 'end';
 *   - toggle o desplegable: un solo handleChange (el store lo cierra como 'end').
 */

import { Knob, Select, Toggle } from '@abdsynths/shared/components';

import { describeControl } from '../contracts/parameters.js';
import { GEOMETRY } from '../contracts/sections.js';
import {
  choiceIndexFromNormalized,
  displayText,
  normalizedFromChoiceIndex,
  realFromNormalized,
} from '../contracts/paramValue.js';

export const KINDS = {
  knob: 'knob',
  toggle: 'toggle',
  choice: 'choice',
};

/** Tipo de control que pide un descriptor del contrato. */
export function kindForControl(control) {
  if (control.kind === 'bool') return KINDS.toggle;
  if (control.kind === 'choice') return KINDS.choice;

  return KINDS.knob;
}

/**
 * Construye la celda de un parámetro.
 *
 * @param {object} control  view-model del contrato (describeControl)
 * @param {object} handlers
 * @param {(id: string, normalized: number) => void} handlers.onChange
 * @param {(id: string, phase: 'begin'|'end') => void} [handlers.onGesture]
 * @returns {{ element: HTMLElement, kind: string, setNormalized: Function, destroy: Function }}
 */
export function createParameterControl(control, handlers = {}) {
  const kind = kindForControl(control);

  const element = document.createElement('div');
  element.className = `cell cell--${kind}`;
  element.dataset.parameterId = control.id;
  element.dataset.controlKind = kind;

  // El contrato dice qué parámetros NO consume el motor (dspStatus != implemented).
  // Se marcan en vez de esconderse: el panel nativo también los muestra.
  if (control.dspStatus !== 'implemented') {
    element.classList.add('cell--divergent');
    element.title = control.dspNote || 'El motor no consume este parámetro (ver contrato)';
  }

  const api = kind === KINDS.knob
    ? buildKnob(control, element, handlers)
    : kind === KINDS.toggle
      ? buildToggle(control, element, handlers)
      : buildChoice(control, element, handlers);

  return {
    element,
    kind,
    setNormalized: api.setNormalized,
    destroy: api.destroy,
    // Gating por motor (solo lo llevan las listas que dependen de él, ver
    // buildChoice): el panel lo usa para reevaluar la lista con cada snapshot.
    engineParameter: api.engineParameter ?? '',
    setEngine: api.setEngine ?? null,
  };
}

/** float -> Knob compartido + lectura en unidades reales debajo. */
function buildKnob(control, element, handlers) {
  const readout = document.createElement('span');
  readout.className = 'cell__readout';

  const knob = new Knob(element, {
    size: GEOMETRY.knob,
    label: control.label,
    value: 0,
    format: (normalized) => displayText(control, realFromNormalized(control, normalized)),
    onChange: (normalized) => handlers.onChange?.(control.id, normalized),
    onDragStart: () => handlers.onGesture?.(control.id, 'begin'),
    onDragEnd: () => handlers.onGesture?.(control.id, 'end'),
  });

  element.append(readout);

  return {
    setNormalized(normalized) {
      knob.setValue(normalized);
      readout.textContent = displayText(control, realFromNormalized(control, normalized));
    },
    destroy() {
      knob.destroy();
      readout.remove();
    },
  };
}

/** choice de dos estados (bool) -> Toggle compartido. */
function buildToggle(control, element, handlers) {
  const toggle = new Toggle(element, {
    label: control.label,
    value: false,
    onChange: (isOn) => handlers.onChange?.(control.id, isOn ? 1 : 0),
  });

  return {
    setNormalized(normalized) {
      toggle.setValue(normalized >= 0.5);
    },
    destroy() {
      toggle.destroy();
    },
  };
}

/**
 * choice -> Select compartido, con las opciones del contrato y el gating por motor.
 *
 * El gating sale ENTERO del contrato: `optionEngines` dice qué motor consume cada
 * opción y `engineParameter` qué parámetro elige el motor. Aquí no se adivina
 * ninguno de los dos: sin contrato la lista se ofrece completa (y un test lo pilla,
 * en vez de que la página invente un motor activo).
 */
function buildChoice(control, element, handlers) {
  const engineControl = control.engineParameter ? describeControl(control.engineParameter) : null;
  const engineFor = (index) => control.optionEngines[index] ?? 'both';

  const select = new Select(element, {
    // El id no es decorativo: el propio control lo usa para el <label for>, y es
    // lo que hace que la lista sea alcanzable por su nombre.
    id: `control-${control.id}`,
    label: control.label,
    // La nota viaja en la opción (title): explica el PORQUÉ de lo que no se puede
    // elegir, en vez de dejar una entrada muerta sin explicación.
    options: control.options.map((label, index) => {
      const engine = engineFor(index);

      return {
        label,
        note: engine === 'both' ? '' : `Requiere el motor ${engineLabel(engineControl, engine)}`,
      };
    }),
    value: 0,
    onChange: (index) =>
      handlers.onChange?.(control.id, normalizedFromChoiceIndex(control, index)),
  });

  /**
   * Recalcula la disponibilidad desde el valor NORMALIZADO del parámetro que elige
   * el motor. Nunca reescribe el valor: una selección que no vale para el motor
   * activo se queda y el Select la marca ([data-divergent]), porque cambiarla sería
   * corromper estado del host sin que nadie lo pida (era el bug del panel nativo,
   * que la pasaba a Off desde un timer de 100 ms).
   *
   * Es `null` cuando la lista no depende del motor (la mayoría): un no-op invitaría
   * a llamarlo en listas que lo ignoran.
   */
  const setEngine = engineControl
    ? (engineNormalized) => {
      const index = choiceIndexFromNormalized(engineControl, engineNormalized);
      const active = engineControl.optionEngines[index];

      if (active === undefined) {
        // El contrato dejó de decir qué motor activa cada opción: mejor ofrecer
        // todo que esconder opciones por un dato que falta.
        console.warn(`"${engineControl.id}" no declara optionEngines: gating desactivado`);
        select.setDisabled([]);
        return;
      }

      select.setDisabled((entry, optionIndex) => {
        const engine = engineFor(optionIndex);

        return engine !== 'both' && engine !== active;
      });
    }
    : null;

  return {
    engineParameter: engineControl ? engineControl.id : '',
    setEngine,

    setNormalized(normalized) {
      select.setValue(choiceIndexFromNormalized(control, normalized));
    },

    // El control solo elige; la index<->normalizado es del contrato del parámetro
    // (lleva su propio skew), así que aquí no se duplica ni un decimal.
    destroy() {
      select.destroy();
    },
  };
}

/**
 * Etiqueta legible de un motor, tomada del propio selector del contrato: la
 * posición del coverage en `optionEngines` es la del nombre en `options`. Nada de
 * una tabla de nombres en la UI, que sería una segunda SSOT.
 */
function engineLabel(engineControl, engine) {
  const index = (engineControl?.optionEngines ?? []).indexOf(engine);

  return index >= 0 ? engineControl.options[index] : engine;
}
