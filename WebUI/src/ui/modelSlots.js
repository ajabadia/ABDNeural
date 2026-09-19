/**
 * Ranuras de modelo espectral A–D: lo que queda del bloque MODEL del panel nativo.
 *
 * De aquel bloque, el XYPad es hoy morphX/morphY en la ficha OSCILADOR y los cuatro
 * botones `loadA..loadD` son esto. Una ranura es del MOTOR, no del APVTS (un preset
 * lleva `modelPath<slot>`, no una copia de los parciales): no tiene id, no ocupa
 * celda y no entra en el encaje de los 70. Lo que pinta llega por el puente
 * (`modelsState`: nombre + validez) y lo que PIDE —cargar un fichero— lo ejecuta el
 * host, porque la página no tiene sistema de ficheros y no puede nombrar rutas.
 *
 * Las cuatro letras son fijas: el procesador expone `getNumModelSlots() == 4` y su
 * `modelNames` tiene cuatro huecos, así que el DOM se construye UNA vez y `paint`
 * solo escribe nombre, estado y disponibilidad — el mismo trato que el resto del
 * lienzo. Si el host publicara menos ranuras, las que falten se pintan vacías en vez
 * de desaparecer: que una ranura exista es cosa del motor, no del cable.
 *
 * Sin host el botón queda deshabilitado: en modo local no hay nada que cargar, y
 * fingirlo sería peor que no ofrecerlo (mismo criterio que el RANDOM del store).
 */

/** Etiquetas de las cuatro ranuras, en el orden del motor (0 = A ... 3 = D). */
export const MODEL_SLOT_LABELS = ['A', 'B', 'C', 'D'];

/** Nombre con el que el procesador marca una ranura vacía (`modelNames`). */
const EMPTY_SLOT_NAME = 'EMPTY';

/**
 * @param {object} [options]
 * @param {(slot: number) => void} [options.onLoad]  pide al host cargar un fichero
 *   en esa ranura. Sin handler (o sin host) el botón no hace nada: el host es quien
 *   abre el diálogo.
 * @returns {{ element: HTMLElement, rows: object[], paint: Function, destroy: Function }}
 */
export function createModelSlots({ onLoad = null } = {}) {
  const element = document.createElement('div');
  element.className = 'model-slots';
  element.dataset.visual = 'model-slots';

  const rows = MODEL_SLOT_LABELS.map((label, slot) => {
    const row = document.createElement('div');
    row.className = 'model-slots__row';
    row.dataset.slot = String(slot);

    const badge = document.createElement('span');
    badge.className = 'model-slots__slot';
    badge.textContent = label;

    const name = document.createElement('span');
    name.className = 'model-slots__name';

    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'model-slots__load';
    button.textContent = 'CARGAR';
    button.addEventListener('click', () => onLoad?.(slot));

    row.append(badge, name, button);
    element.append(row);

    return { slot, label, row, name, button };
  });

  const status = document.createElement('p');
  status.className = 'model-slots__status';
  element.append(status);

  return {
    element,
    rows,

    /**
     * Repinta las ranuras desde el snapshot. `parameters` se ignora a propósito (una
     * ranura no es un parámetro): el dato vive en `state.models` / `state.modelError`.
     */
    paint(_parameters, state) {
      const models = Array.isArray(state?.models) ? state.models : [];
      const available = state?.bridgeAvailable === true;
      let loaded = 0;

      for (const row of rows) {
        const entry = models.find((candidate) => candidate?.slot === row.slot) ?? null;
        const name = displayableName(entry);

        if (name !== null) loaded += 1;

        row.name.textContent = name ?? '—';
        row.row.dataset.loaded = String(name !== null);

        // Un nombre con `isValid: false` es un preset que apunta a un fichero que ya
        // no está: se MARCA (no se oculta) y se explica en el `title`, igual que el
        // gating del motor marca una opción que no aplica en vez de reescribirla.
        const divergent = name !== null && entry?.isValid === false;

        row.row.dataset.divergent = String(divergent);
        row.name.title = divergent
          ? `"${name}": el motor no ha podido cargar el fichero (¿movido o borrado?)`
          : name ?? 'ranura vacía';

        // La disponibilidad es del HOST, no de la ranura: la carga la ejecuta él.
        row.button.disabled = !available;
        row.button.title = available
          ? `Cargar un .neuronikmodel en el slot ${row.label}`
          : 'sin host: los modelos los carga el plugin';
      }

      const error = state?.modelError ?? null;

      status.textContent = error
        ? `✕ ${error.detail}`
        : `${loaded}/${rows.length} cargados`;
      status.dataset.state = error ? 'error' : 'ok';
      status.title = error
        ? (error.slot >= 0 ? `slot ${MODEL_SLOT_LABELS[error.slot] ?? error.slot}: ${error.detail}` : error.detail)
        : '';
    },

    destroy() {
      element.textContent = '';
    },
  };
}

/**
 * Nombre que se puede enseñar de una ranura, o null si no hay nada cargado.
 *
 * La verdad es `isValid`, no el texto: el procesador nace con las cuatro ranuras a
 * "EMPTY", así que ese literal (y el vacío) cuentan como ranura vacía aunque el
 * nombre exista.
 */
function displayableName(entry) {
  const name = typeof entry?.name === 'string' ? entry.name.trim() : '';

  if (name === '' || name === EMPTY_SLOT_NAME) return null;

  return name;
}
