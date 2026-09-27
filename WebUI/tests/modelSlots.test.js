/**
 * Las ranuras de modelo espectral A–D (ficha MODELOS).
 *
 * Es la vista que sustituye al bloque MODEL del panel nativo: allí eran cuatro
 * botones `loadA..loadD` sobre el XYPad con el nombre del fichero en las esquinas;
 * aquí son cuatro filas con la letra, el nombre que el MOTOR tiene cargado y el botón
 * que pide la carga. Lo que se fija aquí:
 *
 *   - cuatro ranuras SIEMPRE (son del motor, `getNumModelSlots() == 4`), aunque el
 *     cable no haya dicho nada todavía;
 *   - "EMPTY" (y el vacío) son ranura vacía, no nombre: el procesador nace así;
 *   - un nombre con `isValid: false` es un preset que apunta a un fichero que ya no
 *     está: se MARCA en ámbar, no se oculta;
 *   - sin host el botón está deshabilitado y no pide nada: la carga la ejecuta el
 *     host (la página no tiene sistema de ficheros), así que fingirla sería mentir;
 *   - el fallo del host (`modelError`) se enseña con su motivo, y una respuesta
 *     nueva lo limpia;
 *   - OLVIDAR solo aparece con modelo y solo en modo local (con host la ranura es
 *     del preset), y el aviso de ese gesto sale por la MISMA linea de estado que el
 *     fallo, con su tono.
 */

import { afterEach, describe, expect, it, vi } from 'vitest';

import { MODEL_SLOT_LABELS, createModelSlots } from '../src/ui/modelSlots.js';

afterEach(() => {
  document.body.textContent = '';
});

/** Estado del store que necesita la vista (lo demás lo ignora). */
function makeState({ models = null, bridgeAvailable = true, modelError = null,
                     localModelReady = false, modelNotice = null } = {}) {
  return { models, bridgeAvailable, modelError, localModelReady, modelNotice };
}

/** Los cuatro slots tal cual los manda el puente (`modelsState`). */
function modelsPayload(names = []) {
  return MODEL_SLOT_LABELS.map((_label, slot) => {
    const name = names[slot] ?? 'EMPTY';

    return {
      slot,
      name,
      isValid: name !== 'EMPTY',
      amplitudes: new Array(64).fill(0),
      frequencyOffsets: new Array(64).fill(0),
    };
  });
}

function mount(options = {}) {
  const view = createModelSlots(options);

  document.body.append(view.element);

  return view;
}

const rowOf = (slot) => document.querySelector(`.model-slots__row[data-slot="${slot}"]`);

describe('ranuras de modelo / estructura', () => {
  it('monta las cuatro ranuras A–D aunque el cable no haya dicho nada', () => {
    mount();

    const rows = document.querySelectorAll('.model-slots__row');

    expect(rows).toHaveLength(4);
    expect([...rows].map((row) => row.querySelector('.model-slots__slot').textContent))
      .toEqual(MODEL_SLOT_LABELS);
    expect([...rows].map((row) => row.dataset.slot)).toEqual(['0', '1', '2', '3']);
  });

  it('no es una celda de parámetro: no lleva `data-parameter-id` ni la clase `cell`', () => {
    const view = mount();

    expect(view.element.dataset.visual).toBe('model-slots');
    expect(view.element.classList.contains('cell')).toBe(false);
    expect(view.element.querySelector('[data-parameter-id]')).toBeNull();
  });
});

describe('ranuras de modelo / qué enseña', () => {
  it('sin datos, las cuatro ranuras están vacías', () => {
    const view = mount();

    view.paint({}, makeState());

    for (const slot of [0, 1, 2, 3]) {
      expect(rowOf(slot).querySelector('.model-slots__name').textContent).toBe('—');
      expect(rowOf(slot).dataset.loaded).toBe('false');
    }

    expect(document.querySelector('.model-slots__status').textContent).toBe('0/4 cargados');
  });

  it('pinta el nombre que el MOTOR tiene en cada ranura', () => {
    const view = mount();

    view.paint({}, makeState({ models: modelsPayload(['Campana', 'EMPTY', 'Cristal']) }));

    expect(rowOf(0).querySelector('.model-slots__name').textContent).toBe('Campana');
    expect(rowOf(2).querySelector('.model-slots__name').textContent).toBe('Cristal');
    expect(rowOf(0).dataset.loaded).toBe('true');
    expect(rowOf(1).dataset.loaded).toBe('false');
    expect(document.querySelector('.model-slots__status').textContent).toBe('2/4 cargados');
  });

  it('"EMPTY" es ranura vacía, no un nombre', () => {
    const view = mount();

    view.paint({}, makeState({ models: modelsPayload([]) }));

    expect(rowOf(0).querySelector('.model-slots__name').textContent).toBe('—');
    expect(document.querySelector('.model-slots__status').textContent).toBe('0/4 cargados');
  });

  it('un nombre que el motor no pudo cargar se MARCA (no se oculta)', () => {
    const view = mount();
    const models = modelsPayload(['Fantasma']);

    models[0].isValid = false;
    view.paint({}, makeState({ models }));

    // El preset apunta al fichero, así que el nombre se enseña...
    expect(rowOf(0).querySelector('.model-slots__name').textContent).toBe('Fantasma');
    // ...y se distingue de una ranura buena (lo que el sonido no hace: cargarlo).
    expect(rowOf(0).dataset.divergent).toBe('true');
    expect(rowOf(0).querySelector('.model-slots__name').title).toContain('Fantasma');
    expect(rowOf(1).dataset.divergent).toBe('false');
  });
});

describe('ranuras de modelo / la carga la pide al host', () => {
  it('el clic pide SU ranura', () => {
    const onLoad = vi.fn();
    const view = mount({ onLoad });

    view.paint({}, makeState());

    rowOf(2).querySelector('.model-slots__load').click();

    expect(onLoad).toHaveBeenCalledTimes(1);
    expect(onLoad).toHaveBeenCalledWith(2);
  });

  it('sin host PERO con camino local (localModelReady) el botón se habilita', () => {
    const onLoad = vi.fn();
    const view = mount({ onLoad });

    view.paint({}, makeState({ bridgeAvailable: false, localModelReady: true }));

    const button = rowOf(0).querySelector('.model-slots__load');
    expect(button.disabled).toBe(false);
    expect(button.title).toContain('slot A');

    button.click();
    expect(onLoad).toHaveBeenCalledWith(0);
  });

  it('sin host el botón está deshabilitado y no pide nada', () => {
    const onLoad = vi.fn();
    const view = mount({ onLoad });

    view.paint({}, makeState({ bridgeAvailable: false }));

    const button = rowOf(0).querySelector('.model-slots__load');

    expect(button.disabled).toBe(true);
    expect(button.title).toContain('sin host');
    button.click();
    expect(onLoad).not.toHaveBeenCalled();
  });

  it('con host el botón se habilita y dice qué carga', () => {
    const view = mount();

    view.paint({}, makeState({ bridgeAvailable: true }));

    const button = rowOf(3).querySelector('.model-slots__load');

    expect(button.disabled).toBe(false);
    expect(button.title).toContain('slot D');
  });
});

describe('ranuras de modelo / el fallo del host', () => {
  it('enseña el motivo del último fallo, con su ranura', () => {
    const view = mount();

    view.paint({}, makeState({ modelError: { slot: 1, detail: 'no file chosen' } }));

    const status = document.querySelector('.model-slots__status');

    expect(status.textContent).toBe('✕ no file chosen');
    expect(status.dataset.state).toBe('error');
    expect(status.title).toContain('slot B');
  });

  it('una respuesta del motor limpia el fallo anterior', () => {
    const view = mount();

    view.paint({}, makeState({ modelError: { slot: 1, detail: 'no file chosen' } }));
    view.paint({}, makeState({ models: modelsPayload(['Uno']) }));

    const status = document.querySelector('.model-slots__status');

    expect(status.textContent).toBe('1/4 cargados');
    expect(status.dataset.state).toBe('ok');
    expect(status.title).toBe('');
  });
});

describe('ranuras de modelo / OLVIDAR', () => {
  const forgetOf = (slot) => rowOf(slot).querySelector('.model-slots__forget');

  it('el botón pide SU ranura', () => {
    const onForget = vi.fn();
    const view = mount({ onForget });

    view.paint({}, makeState({
      models: modelsPayload(['Campana', 'EMPTY', 'Cristal', 'EMPTY']),
      bridgeAvailable: false,
      localModelReady: true,
    }));

    forgetOf(0).click();

    expect(onForget).toHaveBeenCalledTimes(1);
    expect(onForget).toHaveBeenCalledWith(0);
  });

  it('solo aparece con modelo: una ranura vacía no tiene nada que olvidar', () => {
    const view = mount({ onForget: () => {} });

    view.paint({}, makeState({
      models: modelsPayload(['Campana']),
      bridgeAvailable: false,
      localModelReady: true,
    }));

    expect(forgetOf(0).hidden).toBe(false);
    expect(forgetOf(0).title).toContain('slot A');
    for (const slot of [1, 2, 3]) expect(forgetOf(slot).hidden).toBe(true);
  });

  it('con host NO se ofrece: la ranura es del preset y su fichero vuelve con el proyecto', () => {
    const onForget = vi.fn();
    const view = mount({ onForget });

    view.paint({}, makeState({ models: modelsPayload(['Campana']), bridgeAvailable: true }));

    expect(forgetOf(0).hidden).toBe(true);
    forgetOf(0).click();
    expect(onForget).not.toHaveBeenCalled();
  });

  it('un nombre que el motor no pudo cargar TAMBIÉN se puede olvidar', () => {
    // La ranura tiene nombre, luego hay algo cargado: lo que fallo fue el fichero,
    // no la carga. Y al olvidarla desaparece el estado divergente.
    const view = mount({ onForget: () => {} });
    const models = modelsPayload(['Fantasma']);

    models[0].isValid = false;
    view.paint({}, makeState({ models, bridgeAvailable: false, localModelReady: true }));

    expect(forgetOf(0).hidden).toBe(false);

    view.paint({}, makeState({
      models: modelsPayload(),
      bridgeAvailable: false,
      localModelReady: true,
    }));

    expect(forgetOf(0).hidden).toBe(true);
  });
});

describe('ranuras de modelo / la linea de estado: fallo y aviso', () => {
  const status = () => document.querySelector('.model-slots__status');

  it('el acierto se anuncia con su tono (no es un error)', () => {
    const view = mount();

    view.paint({}, makeState({
      models: modelsPayload(),
      modelNotice: { slot: 1, tone: 'ok', detail: 'olvidada: la ranura vuelve a EMPTY' },
    }));

    expect(status().textContent).toBe('✓ olvidada: la ranura vuelve a EMPTY');
    expect(status().dataset.state).toBe('ok');
    expect(status().title).toContain('slot B');
  });

  it('el aviso a medias (el navegador no la deja olvidar) va en tono warn', () => {
    const view = mount();

    view.paint({}, makeState({
      modelNotice: { slot: 0, tone: 'warn', detail: 'al recargar volvera' },
    }));

    expect(status().textContent).toBe('⚠ al recargar volvera');
    expect(status().dataset.state).toBe('warn');
  });

  it('un fallo de carga MANDA sobre el aviso: una línea, un mensaje', () => {
    const view = mount();

    view.paint({}, makeState({
      modelError: { slot: 2, detail: 'no file chosen' },
      modelNotice: { slot: 1, tone: 'ok', detail: 'olvidada' },
    }));

    expect(status().textContent).toBe('✕ no file chosen');
    expect(status().dataset.state).toBe('error');
  });

  it('sin aviso ni fallo, la línea es el conteo', () => {
    const view = mount();

    view.paint({}, makeState({ models: modelsPayload(['Uno']) }));

    expect(status().textContent).toBe('1/4 cargados');
    expect(status().dataset.state).toBe('ok');
    expect(status().title).toBe('');
  });
});

describe('ranuras de modelo / ciclo de vida', () => {
  it('destroy vacía el nodo', () => {
    const view = mount();

    view.paint({}, makeState({ models: modelsPayload(['Campana']) }));
    view.destroy();

    expect(view.element.textContent).toBe('');
  });
});
