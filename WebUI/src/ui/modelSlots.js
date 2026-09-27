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
 *
 * Cada fila lleva su OLVIDAR (solo con modelo y solo en modo local, ver `paint`),
 * que la vacía y saca su texto de la memoria local. El aviso de ese gesto —y el
 * fallo de una carga— salen por la MISMA línea de estado: uno dice que no se
 * pudo, el otro que sí. Un sitio, dos tonos.
 */

import { displayableModelName as displayableName } from '../audio/localModels.js';

/** Etiquetas de las cuatro ranuras, en el orden del motor (0 = A ... 3 = D). */
export const MODEL_SLOT_LABELS = ['A', 'B', 'C', 'D'];

// La pregunta "¿esta ranura tiene nombre?" la responde la MISMA funcion que usa
// el store al olvidar (audio/localModels.js). Se re-exporta porque el panel la
// consume aqui (ui/panel.js) y ese no es su trabajo: la verdad no vive en la vista.
export { displayableName };

/**
 * @param {object} [options]
 * @param {(slot: number) => void} [options.onLoad]  pide al host cargar un fichero
 *   en esa ranura. Sin handler (o sin host) el botón no hace nada: el host es quien
 *   abre el diálogo.
 * @param {(slot: number) => void} [options.onForget]  vacía esa ranura (solo modo
 *   local: con host las ranuras son del PRESET y quien las recarga es él).
 * @returns {{ element: HTMLElement, rows: object[], paint: Function, destroy: Function }}
 */
export function createModelSlots({ onLoad = null, onForget = null } = {}) {
  const element = document.createElement('div');
  element.className = 'model-slots';
  element.dataset.visual = 'model-slots';
  // 2x2: la mitad de alto que la columna (el espectral comparte la ficha).

  const rows = MODEL_SLOT_LABELS.map((label, slot) => {
    const row = document.createElement('div');
    row.className = 'model-slots__row';
    row.dataset.slot = String(slot);
    // Resalte de salto (esquina del pad -> cajón): la numeración es la del
    // MOTOR (0-based), la misma que viaja del clic. Espejo del data-slot de
    // las filas de celdas (drawer-slot), que son 1-based — el panel resalta
    // por el que exista (ver openDrawerRoute en ui/panel.js).
    row.dataset.slotVisual = String(slot);

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

    // OLVIDAR: la ranura vuelve a EMPTY y su texto sale de la memoria local, para
    // que un F5 no la devuelva. Nace OCULTO (no deshabilitado) porque no hay nada
    // que olvidar en una ranura vacía: un botón apagado que no hace nada es ruido
    // en las tres de cuatro filas que suelen estar vacías. Aparece al cargarse.
    const forget = document.createElement('button');
    forget.type = 'button';
    forget.className = 'model-slots__forget';
    forget.textContent = 'OLVIDAR';
    forget.addEventListener('click', () => onForget?.(slot));

    row.append(badge, name, button, forget);
    element.append(row);

    return { slot, label, row, name, button, forget };
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
      // Disponible = hay a quien pedirle la carga: el host (dialogo nativo)
      // o el input de fichero local (modo navegador, 2026-09-26). Sin host y
      // sin camino local, deshabilitado — no se finge una carga.
      const available = state?.bridgeAvailable === true || state?.localModelReady === true;
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
          : 'sin host y sin motor local: nada puede cargar el fichero';

        // OLVIDAR solo existe en MODO LOCAL: con host la ranura la trae el preset
        // (su `modelPath<slot>`) y el motor la volvería a cargar en cuanto se
        // recargase el proyecto, así que vaciarla aquí sería mentir por omisión.
        const forgettable = name !== null && state?.bridgeAvailable !== true;
        const forgetWhy = state?.bridgeAvailable === true
          ? 'con host la ranura es del preset: su fichero vuelve con el proyecto'
          : 'sin memoria local no hay nada que olvidar';

        row.forget.hidden = !forgettable;
        row.forget.disabled = !forgettable || typeof onForget !== 'function';
        row.forget.title = forgettable
          ? `Vaciar el slot ${row.label}: la ranura vuelve a EMPTY y no sobrevive al F5`
          : forgetWhy;
      }

      // UNA línea, un mensaje. `modelError` (el fallo) manda sobre `modelNotice`
      // (el aviso de un gesto que SÍ salió: OLVIDAR): si ahora hay un fallo de
      // carga, el "olvidada" de hace un rato ya no es la novedad y enseñarlo
      // sería mezclar dos hechos en una sola línea. El aviso lleva su propio tono
      // (`ok`/`warn`), no el rojo de un error: vaciar una ranura no es un fallo, y
      // un aviso pintado como error acabaría leyéndose como tal.
      const error = state?.modelError ?? null;
      const notice = state?.modelNotice ?? null;
      const tone = notice?.tone === 'warn' ? 'warn' : 'ok';

      if (error) {
        status.textContent = `✕ ${error.detail}`;
        status.dataset.state = 'error';
        status.title = error.slot >= 0
          ? `slot ${MODEL_SLOT_LABELS[error.slot] ?? error.slot}: ${error.detail}`
          : error.detail;
      } else if (notice) {
        status.textContent = `${tone === 'warn' ? '⚠' : '✓'} ${notice.detail}`;
        status.dataset.state = tone;
        status.title = notice.slot >= 0
          ? `slot ${MODEL_SLOT_LABELS[notice.slot] ?? notice.slot}: ${notice.detail}`
          : notice.detail;
      } else {
        status.textContent = `${loaded}/${rows.length} cargados`;
        status.dataset.state = 'ok';
        status.title = '';
      }
    },

    destroy() {
      element.textContent = '';
    },
  };
}
