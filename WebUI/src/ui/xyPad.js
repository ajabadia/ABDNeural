/**
 * El pad XY del bloque MODEL: morphX/morphY dibujados, con los nombres de los
 * modelos A–D en las esquinas (A arriba-izquierda ... D abajo-derecha, igual
 * que pinta el XYPad nativo con `setModelNames`).
 *
 * El COMPONENTE es el compartido (`@abdsynths/shared` XYPad: superficie
 * absoluta, y=1 arriba, setValue silencioso para snapshots, esquinas con
 * `setCorners`). Aquí solo vive el wiring de NEURONiK:
 *
 *   - morphX/morphY son DOS parámetros del contrato que el pad edita a la vez,
 *     así que un gesto suyo son DOS gestos coordinados (uno por eje) con su
 *     fase completa: `begin` con el valor actual, `change` en cada movimiento,
 *     `end` al soltar. Un paso de teclado no abre gesto: viaja como `end`,
 *     que es como el store cierra un toggle.
 *   - los nombres vienen por el puente (`state.models`, la misma fuente de las
 *     ranuras): esquina con nombre = ranura cargada; ranura vacía o `EMPTY`,
 *     esquina oculta.
 *   - mientras se arrastra, un snapshot del host NO mueve el pulgar: no se
 *     pelea con el dedo del usuario (el valor de verdad llega al soltar).
 *
 * ANILLO MORPH-Z (FASE 10.3, el tercer eje del Neuron): un aro SVG circular
 * centrado sobre el pad — overlay propio de esta página, el componente
 * compartido no se toca (su contrato lo pinean SUS tests). Convenciones:
 *
 *   - el ángulo ES el valor: 0 en las 12 (top), horario, 1 = círculo completo
 *     (z es un eje CÍCLICO temporal — LFO2 lo anima en bucle — no un sweep de
 *     knob de 270°);
 *   - un drag del aro es UN gesto de morphZ con fase completa (begin/change/
 *     end), coordinado igual que los ejes del pad; los hit-areas son solo los
 *     trazos (`pointer-events: stroke`): el interior sigue siendo del pad;
 *   - teclado: role=slider con flechas (±0.01), PageUp/Down (±0.1) y
 *     Home/End (0/1), cada paso viaja como `end` y SOLO si cambió;
 *   - paint pinta el aro sin eco y, durante un drag del aro, un snapshot del
 *     host no lo mueve (misma regla que el pulgar del pad);
 *   - telemetría: `setZMod(contribution)` pinta el ARCO de modulación del aro
 *     (color --color-mod-ring de la familia): DESDE la base, en el sentido del
 *     signo — horario si suma, ANTIHORARIO si resta, envolviendo por las 12 —
 *     y SIN el recorte 0..1 del motor: con morphZ a 0 la semionda negativa se
 *     ve. El arco enseña LA CONTRIBUCION; el efectivo lo sigue clampeando la
 *     voz. Es pintura, nunca estado: no viaja al store.
 */

import { XYPad } from '@abdsynths/shared/components';

import { displayableName, MODEL_SLOT_LABELS } from './modelSlots.js';

const MORPH_IDS = { x: 'morphX', y: 'morphY', z: 'morphZ' };

// FASE 11.4: el aro mezcla CAPAS — cada capa tiene su PROPIA linea de
// frames (morphZ, morphZ2, morphZ3) y el aro mueve las tres a la vez:
// cada una sigue su camino desde su valor de reposo, la suma es la mezcla.
const LAYER_IDS = ['morphZ2', 'morphZ3'];

const Z_STEP = 0.01;
const Z_PAGE = 0.1;

/**
 * La pista de teclado del gesto de ESQUINA, en UNA SOLA FUENTE: la usan el
 * `title` de las cuatro esquinas (paintCorners, mas abajo) y la ayuda
 * contextual del cajon de MODELOS (`ui/visuals.js`, item 'Esquinas A-D').
 *
 * Antes la llevaba solo la ayuda, es decir que solo se leia donde NO hace
 * falta: el tooltip sale encima de la esquina, que es justo donde el usuario
 * ya esta mirando y sin tener que abrir un cajon. Por eso vive aqui y no ahi:
 * el gesto (y su teclado) es de ESTE modulo, que es quien despacha el clic y el
 * keydown; la ayuda solo lo documenta.
 *
 * Es el texto, no el comportamiento: quien despacha es el `keydown` delegado
 * de mas abajo (`Enter`/`Space` sobre una esquina con foco), y ese no se puede
 * deducir de una constante — por eso el test comprueba que la pista aparece en
 * los dos sitios en vez de fiarse de que la constante no se puede mentir.
 */
export const CORNER_KEYBOARD_HINT = 'Enter/Space';

/** aro SVG: track + hit (trazo ancho invisible) + fill con dash por pathLength */
function ringSvg() {
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.setAttribute('viewBox', '0 0 100 100');
  svg.innerHTML = `
    <circle class="zring-track" cx="50" cy="50" r="47"></circle>
    <circle class="zring-hit"   cx="50" cy="50" r="47"></circle>
    <circle class="zring-fill"  cx="50" cy="50" r="47" pathLength="100"
            transform="rotate(-90 50 50)"></circle>
    <circle class="zring-mod"   cx="50" cy="50" r="47" pathLength="100"
            transform="rotate(-90 50 50)" stroke-dasharray="0 100"></circle>`;
  return svg;
}

/**
 * @param {object} [options]
 * @param {(id: 'morphX'|'morphY'|'morphZ', normalized: number,
 *          phase: 'begin'|'change'|'end') => void} [options.onEdit]
 *   empuja una edición al store (`pushParameter`): la fábrica la inyecta para
 *   que el panel siga sin saber qué dibuja cada vista.
 * @param {(slot: number) => void} [options.onCornerClick]
 *   clic (o las teclas de CORNER_KEYBOARD_HINT con foco) sobre una esquina
 *   CARGADA: abre el cajón de MODELOS en esa ranura — el gesto inverso a IR A
 *   LA RUTA. Lo ejecuta el panel vía openDrawerRoute; aquí solo viaja el slot
 *   (0..3).
 * @returns {{ element: HTMLElement, pad: object, paint: Function, destroy: Function }}
 *   `pad` es la instancia compartida (handle de inspección para tests).
 */
export function createXyPad({ onEdit = null, onTelemetry = null, onCornerClick = null } = {}) {
  const element = document.createElement('div');
  element.className = 'xy-pad';
  // Consultable como vista de ficha (el espectral y las ranuras lo hacen).
  element.dataset.visual = 'model-xy';

  let dragging = false;

  const pad = new XYPad(element, {
    x: 0,
    y: 0,
    // AMPLITUD 10.x: 220 de ancho — mas pista horizontal de morfeo, y sigue
    // cabiendo en su ficha (el card centra el pad pelado; el contrato manda el
    // ALTO). Las esquinas A-D se recolocan solas (porcentajes sobre el pad); su
    // legibilidad se ajusta en main.css.
    width: 220,
    // BALANCEO 9.x: el cuerpo de la banda es 164 (206 de ficha - 42 de armazon,
    // el SSOT que comparte con ENVOLVENTES y EFECTOS). Aqui vive como pad 150 +
    // 14 de fila de readout propio (ver abajo): con esquinas A-D visibles, el
    // componente compartido OCULTA su readout interno (contrato del paquete:
    // las esquinas dicen QUE, el aria-valuetext conserva el valor) y esta pagina
    // lo muestra FUERA, debajo del pad — el X/Y % no desaparece del interface.
    height: 150,
    onDragStart() {
      dragging = true;
      const { x, y } = pad.getValue();

      onEdit?.(MORPH_IDS.x, x, 'begin');
      onEdit?.(MORPH_IDS.y, y, 'begin');
    },
    onChange({ x, y }) {
      if (dragging) {
        onEdit?.(MORPH_IDS.x, x, 'change');
        onEdit?.(MORPH_IDS.y, y, 'change');
      } else {
        // Edición sin gesto (teclado): solo el eje que de verdad cambió respecto
        // a lo que el cable ya sabe (`last` lo actualizan también los snapshots).
        if (x !== last.x) onEdit?.(MORPH_IDS.x, x, 'end');
        if (y !== last.y) onEdit?.(MORPH_IDS.y, y, 'end');
      }

      last = { x, y };
    },
    onDragEnd() {
      dragging = false;
      const { x, y } = pad.getValue();

      onEdit?.(MORPH_IDS.x, x, 'end');
      onEdit?.(MORPH_IDS.y, y, 'end');
      last = { x, y };
    },
  });

  // Lo que el cable ya sabe: el valor de arranque del pad y, desde entonces,
  // también lo que llegue pintado (los snapshots nativos no hay que reenviarlos).
  let last = pad.getValue();

  // --- readout X/Y % FUERA del pad (esquinas A-D lo esconden dentro) --------

  // El componente esconde su readout interno mientras haya esquinas (contrato
  // del paquete). Esta página lo quiere SIEMPRE visible: la fila de abajo es un
  // espejo del texto del readout interno, repintada tras CADA render del pad
  // (gesto, teclado y setValue silencioso de los snapshots: los tres pasan por
  // render()). Mismo formato que el componente (format option, % por defecto).
  const readoutRow = document.createElement('div');
  readoutRow.className = 'xy-pad__readout-row';
  readoutRow.setAttribute('aria-hidden', 'true'); // el valor accesible es el del pad (aria-valuetext)
  const readoutMirror = document.createElement('span');
  readoutMirror.textContent = pad.readout?.textContent ?? '';
  readoutRow.append(readoutMirror);
  element.append(readoutRow);

  // El render del compartido es la ÚNICA puerta por la que cambia el texto
  // (onChange -> render, setValue -> render): un wrap mantiene el espejo al día
  // sin conocer sus opciones (el `format` configurable del componente lo usa).
  const sharedRender = pad.render.bind(pad);
  pad.render = () => {
    sharedRender();
    readoutMirror.textContent = pad.readout?.textContent ?? '';
  };

  // El TERCER dato de la fila: el morphZ EFECTIVO = base (el aro, morphZ del
  // APVTS) + contribución de la matriz (frame.modulation[28], con signo),
  // recortado 0..1 como lo clampea la voz. renderZ() lo repinta: la base y
  // el módulo pasan AMBOS por ahí, y el texto nace con el primer render.
  const zEffective = document.createElement('span');
  zEffective.className = 'xy-pad__readout-z';
  readoutRow.append(zEffective);

  // --- DIVERGENCIA página <-> nativo en la fila de readout -----------------
  //
  // La página pinta SU snapshot (morphX/morphY del APVTS, vía paint) y la
  // telemetría trae `frame.morph` (lo que el motor dice tener AHORA). Dos
  // caminos que deberían contar lo mismo: mientras difieran por más que la
  // resolución del canal (un frame a ~15 Hz con diff de 1/255 puede ver el
  // valor de transición), la fila se COLOREA y su tooltip lo dice — la misma
  // honestidad de `cell--divergent` (dspStatus) y `data-divergent` (ranuras).
  const DIVERGENCE_EPSILON = 0.02; // ~5/255: un frame de transición no alarma
  let nativeMorph = null;

  function paintDivergence(pageValue) {
    if (nativeMorph === null) {
      delete readoutRow.dataset.divergent;
      return;
    }

    const divergent = Math.abs(pageValue.x - nativeMorph[0]) > DIVERGENCE_EPSILON
      || Math.abs(pageValue.y - nativeMorph[1]) > DIVERGENCE_EPSILON;

    readoutRow.dataset.divergent = String(divergent);
    readoutRow.title = divergent
      ? `DIVERGENCIA con el motor: página ${Math.round(pageValue.x * 100)}/${Math.round(pageValue.y * 100)}%, nativo ${Math.round(nativeMorph[0] * 100)}/${Math.round(nativeMorph[1] * 100)}%`
      : '';
  }

  const stopTelemetry = typeof onTelemetry === 'function'
    ? onTelemetry((frame) => {
      const morph = Array.isArray(frame?.morph) ? frame.morph : [];

      if (typeof morph[0] !== 'number' || typeof morph[1] !== 'number') return;

      nativeMorph = [morph[0], morph[1]];
      paintDivergence(pad.getValue());
    })
    : null;

  // Primer pintado del espejo (el constructor ya llamó a su render).
  readoutMirror.textContent = pad.readout?.textContent ?? '';

  // ---------------------- arrastre fino (Shift = 1/10) ----------------------

  // El compartido aplica el SALTO ABSOLUTO en cada pointermove (el pad ES la
  // superficie de valor). Con Shift pulsado durante un drag, esta página aplica
  // el convenio de los knobs profesionales: movimiento relativo a 1/10. Va en
  // CAPTURA sobre el contenedor (ancestro del pad): corre ANTES que el listener
  // del componente y stopImmediatePropagation le bloquea el salto — el resto de
  // gestos (sin Shift) pasan intactos. El aro morph-Z no se ve afectado: su
  // drag no pone `dragging` del pad.
  const FINE_SCALE = 0.1;
  const clamp01 = (value) => Math.min(1, Math.max(0, value));
  let fineActive = false;
  let fineAnchor = null; // { x, y, px, py }: valor y puntero al ENTRAR en fino

  const padSurface = pad.pad;

  element.addEventListener('pointermove', (event) => {
    if (!dragging || !event.shiftKey) return;

    // Entrar en fino: se ancla el valor ACTUAL con el puntero ACTUAL (el salto
    // del pointerdown ya ocurrió) — cero salto al activar el modo.
    if (!fineActive) {
      fineActive = true;
      fineAnchor = { x: pad.getValue().x, y: pad.getValue().y, px: event.clientX, py: event.clientY };
    }

    event.stopImmediatePropagation();

    // Relativo al anclaje, escalado 1/10 (el eje Y va invertido, como el pad).
    const rect = padSurface.getBoundingClientRect();
    const x = clamp01(fineAnchor.x + ((event.clientX - fineAnchor.px) / rect.width) * FINE_SCALE);
    const y = clamp01(fineAnchor.y - ((event.clientY - fineAnchor.py) / rect.height) * FINE_SCALE);

    // notify=true: el onChange del compartido lo entrega a ESTE wiring como
    // 'change' del gesto en marcha (el 'end' lo manda el pointerup de siempre).
    pad.setValue({ x, y }, true);
  }, true);

  const endFineDrag = () => { fineActive = false; };

  element.addEventListener('pointerup', endFineDrag, true);
  element.addEventListener('pointercancel', endFineDrag, true);

  // Descubribilidad: un modificador sin pista es invisible (el pad del host
  // nativo documenta su gesto fino en el manual; aqui, en el propio widget).
  padSurface.title = 'Arrastrar = mover · Shift = fino (1/10)';

  // ------------- esquinas clicables (abrir el cajón de MODELOS) -----------
  //
  // La esquina es la RANURA del motor puesta en el pad: clic en la de un slot
  // cargado abre su cajón en esa ranura (el mismo salto que IR A LA RUTA hace
  // desde ENVOLVENTES). Delegación en CAPTURA sobre la superficie: la esquina
  // vive DENTRO del pad (superficie absoluta, el pointerdown salta al punto) y
  // el gesto de abrir NO es un gesto de morfeo — stopPropagation le bloquea el
  // salto al componente compartido, cuyo contrato lo pinean SUS tests.
  const CORNER_SLOTS = { tl: 0, tr: 1, bl: 2, br: 3 }; // A arriba-izq ... D abajo-der

  const cornerFrom = (event) => event.target?.closest?.('.abd-xypad__corner') ?? null;

  // El pointerdown de apertura muere aquí: sin salto de pulgar ni gesto abierto.
  const blockCornerPress = (event) => {
    if (cornerFrom(event)) {
      event.stopPropagation();
      event.preventDefault();
    }
  };

  padSurface.addEventListener('pointerdown', blockCornerPress, true);

  padSurface.addEventListener('click', (event) => {
    const corner = cornerFrom(event);

    if (corner && onCornerClick) onCornerClick(CORNER_SLOTS[corner.dataset.corner] ?? 0);
  });

  // Accesible gratis: la esquina abrible se declara botón en paintCorners
  // (role + tabindex); las teclas de CORNER_KEYBOARD_HINT con foco pasan por
  // la misma puerta.
  padSurface.addEventListener('keydown', (event) => {
    if (event.key !== 'Enter' && event.key !== ' ') return;

    const corner = cornerFrom(event);

    if (corner && onCornerClick) {
      event.preventDefault();
      onCornerClick(CORNER_SLOTS[corner.dataset.corner] ?? 0);
    }
  });

  let cornerKey = null;

  function paintCorners(models) {
    const entries = [0, 1, 2, 3].map((slot) => {
      const entry = Array.isArray(models)
        ? models.find((candidate) => candidate?.slot === slot)
        : null;

      return { name: displayableName(entry) ?? '', divergent: entry?.isValid === false };
    });
    const names = entries.map((entry) => entry.name);

    // Repintar solo si el reparto de nombres cambió: `paint` corre con cada
    // snapshot y reconstruir cuatro spans sería basura de DOM por tick.
    const key = names.join('\u0000');

    if (key === cornerKey) return;

    cornerKey = key;
    pad.setCorners(names);

    // ESTADO de cada esquina, el MISMO que las ranuras del cajón (data-loaded /
    // data-divergent sobre .model-slots__row): cargada normal, cargada con
    // `isValid: false` = divergente (preset apuntando a un fichero que ya no
    // está). Las vacías no existen (el componente omite el span), así que su
    // "estado" es el fondo del pad — nada que colorear. El mismo gesto que el
    // del cajón, contado dos veces: el color es de la página, no del paquete.
    for (const corner of padSurface.querySelectorAll('.abd-xypad__corner')) {
      const index = ['tl', 'tr', 'bl', 'br'].indexOf(corner.dataset.corner);
      const entry = entries[index] ?? { name: '', divergent: false };

      corner.dataset.divergent = String(entry.divergent === true);

      // Esquina abrible = ranura cargada (la vacía ni existe como span). El
      // estado del botón va aquí porque la esquina se RECONSTRUYE con cada
      // cambio de nombres (renderCorners del compartido); atributos
      // idempotentes, cero listeners duplicados (el clic es delegado).
      if (entry.name !== '') {
        corner.dataset.clickable = 'true';
        corner.setAttribute('role', 'button');
        corner.tabIndex = 0;
        corner.title = entry.divergent
          ? `"${corner.textContent}": el motor no ha podido cargar el fichero (¿movido o borrado?) · Clic: abrir MODELOS · ${CORNER_KEYBOARD_HINT}`
          : `Abrir MODELOS: ranura ${MODEL_SLOT_LABELS[index]} "${corner.textContent}" · ${CORNER_KEYBOARD_HINT}`;
      } else {
        delete corner.dataset.clickable;
        corner.removeAttribute('role');
        corner.removeAttribute('tabindex');
        corner.title = '';
      }
    }
  }

  // ------------------------- anillo morph-Z -------------------------------

  let zValue = 0;
  let zMod = 0; // contribucion con signo de la matriz sobre morphZ (telemetria)
  let zDragging = false;

  const ring = document.createElement('div');
  ring.className = 'xy-pad__zring';
  ring.tabIndex = 0;
  ring.setAttribute('role', 'slider');
  ring.setAttribute('aria-label', 'Morph Z');
  ring.setAttribute('aria-valuemin', '0');
  ring.setAttribute('aria-valuemax', '1');
  ring.setAttribute('aria-valuenow', '0');
  ring.setAttribute('aria-valuetext', 'Morph Z 0%');

  const svg = ringSvg();
  ring.appendChild(svg);
  element.appendChild(ring);

  const zFill = svg.querySelector('.zring-fill');
  const zModFill = svg.querySelector('.zring-mod');

  // Descubribilidad del gesto fino del aro (mismo convenio que el pad).
  ring.title = 'Arrastrar = girar · Shift = fino (1/10)';

  function renderZ() {
    zFill.setAttribute('stroke-dasharray', `${zValue * 100} 100`);

    // arco de modulacion CON SIGNO (2026-09-27): DESDE la base, en el sentido
    // que marca la contribucion — horario si suma, ANTIHORARIO si resta — y
    // SIN el recorte 0..1 del motor: con morphZ a 0 la semionda negativa se
    // ve (el arco entra en el lado negativo del aro) y una contribucion que
    // pasa de la vuelta envuelve por las 12 (el dash de un circulo cerrado
    // ya envuelve solo). La voz sigue clampeando el efectivo: lo que aqui se
    // ensena es LA CONTRIBUCION, no el recorte.
    // Unidades enteras de guion (pathLength=100): resolucion de 3.6 grados.
    const snap = (v) => Math.round(v * 100);
    const baseInt = Math.max(0, Math.min(100, snap(zValue)));
    const spanInt = Math.min(100, Math.abs(snap(zMod)));
    const startInt = zMod < 0
      ? (((baseInt - spanInt) % 100) + 100) % 100   // antihorario: TERMINA en la base
      : baseInt;                                     // horario: NACE en la base

    zModFill.setAttribute('stroke-dashoffset', `${-startInt}`);
    zModFill.setAttribute('stroke-dasharray', `${spanInt} 100`);

    ring.setAttribute('aria-valuenow', `${zValue}`);
    ring.setAttribute('aria-valuetext', `Morph Z ${Math.round(zValue * 100)}%`);

    // El efectivo que lee la voz (la matriz puede empujarlo fuera de 0..1;
    // el aro enseña la CONTRIBUCIÓN sin recortar, el readout da el VALOR).
    const effective = Math.min(1, Math.max(0, zValue + zMod));

    zEffective.textContent = `· Z ${Math.round(effective * 100)}%`;
  }

  /** silencioso (snapshots) o con eco (ediciones propias) */
  function setZ(value, notify = false) {
    zValue = Math.min(1, Math.max(0, Number.isFinite(value) ? value : 0));
    renderZ();

    if (notify) {
      onEdit?.(MORPH_IDS.z, zValue, 'end');
      for (const id of LAYER_IDS) onEdit?.(id, zValue, 'end');
    }
  }

  /** Telemetria del destino 28 (solo pintura): contribucion con signo, normalizada. */
  function setZMod(modAmount) {
    const amount = Number(modAmount);

    zMod = Number.isFinite(amount) ? Math.min(1, Math.max(-1, amount)) : 0;
    renderZ();
  }

  /** ángulo -> valor: 0 en las 12, horario, 1 = vuelta completa. */
  function applyRingPointer(event) {
    const rect = svg.getBoundingClientRect();
    const dx = event.clientX - (rect.left + rect.width / 2);
    const dy = event.clientY - (rect.top + rect.height / 2);

    let v = Math.atan2(dx, -dy) / (2 * Math.PI);
    if (v < 0) v += 1;

    setZ(v);
    onEdit?.(MORPH_IDS.z, zValue, 'change');
    for (const id of LAYER_IDS) onEdit?.(id, zValue, 'change');
  }

  svg.addEventListener('pointerdown', (event) => {
    zDragging = true;
    try { svg.setPointerCapture(event.pointerId); } catch { /* jsdom */ }
    ring.focus({ preventScroll: true });

    // `begin` anuncia el gesto con el valor ACTUAL (protocolo del store);
    // el salto angular de este pointerdown viaja como `change`.
    onEdit?.(MORPH_IDS.z, zValue, 'begin');
    for (const id of LAYER_IDS) onEdit?.(id, zValue, 'begin');
    applyRingPointer(event);
  });

  svg.addEventListener('pointermove', (event) => {
    if (zDragging) applyRingPointer(event);
  });

  // ---------------- arrastre fino del aro (Shift = 1/10) -------------------
  //
  // El mismo convenio del pad (ver su bloque fino): con Shift, el movimiento
  // es RELATIVO al anclaje a 1/10, no el salto angular absoluto. Va en CAPTURA
  // sobre el ring (ancestro del svg): corre antes que el listener absoluto y
  // stopImmediatePropagation se lo salta — sin Shift, el gesto de siempre pasa
  // intacto. La particularidad es angular: el delta del ángulo se toma por el
  // CAMINO CORTO (cruzar las 12 no es dar la vuelta entera) y 360° equivalen a
  // 0.1 de valor (1/10 de vuelta).
  let zFineActive = false;
  let zFineAnchor = null; // { angle, z }: ángulo del puntero y valor al entrar

  function pointerAngle(event) {
    const rect = svg.getBoundingClientRect();
    const dx = event.clientX - (rect.left + rect.width / 2);
    const dy = event.clientY - (rect.top + rect.height / 2);

    let v = Math.atan2(dx, -dy) / (2 * Math.PI);
    return v < 0 ? v + 1 : v;
  }

  ring.addEventListener('pointermove', (event) => {
    if (!zDragging || !event.shiftKey) return;

    // Entrar en fino: ángulo del puntero ACTUAL con el valor ACTUAL — cero
    // salto al activar el modo.
    if (!zFineActive) {
      zFineActive = true;
      zFineAnchor = { angle: pointerAngle(event), z: zValue };
    }

    event.stopImmediatePropagation();

    // Delta por el camino corto (-0.5..0.5 de vuelta), escalado 1/10: mover
    // 36 grados equivale a 0.01 de valor.
    let delta = pointerAngle(event) - zFineAnchor.angle;

    if (delta > 0.5) delta -= 1;
    else if (delta < -0.5) delta += 1;

    setZ(zFineAnchor.z + delta * FINE_SCALE);
    onEdit?.(MORPH_IDS.z, zValue, 'change');
    for (const id of LAYER_IDS) onEdit?.(id, zValue, 'change');
  }, true);

  const endZFine = () => { zFineActive = false; };

  ring.addEventListener('pointerup', endZFine, true);
  ring.addEventListener('pointercancel', endZFine, true);

  const endZDrag = () => {
    if (!zDragging) return;
    zDragging = false;
    onEdit?.(MORPH_IDS.z, zValue, 'end');
    for (const id of LAYER_IDS) onEdit?.(id, zValue, 'end');
  };

  svg.addEventListener('pointerup', endZDrag);
  svg.addEventListener('pointercancel', endZDrag);

  ring.addEventListener('keydown', (event) => {
    const moves = {
      ArrowUp: Z_STEP,
      ArrowRight: Z_STEP,
      ArrowDown: -Z_STEP,
      ArrowLeft: -Z_STEP,
      PageUp: Z_PAGE,
      PageDown: -Z_PAGE,
    };

    if (event.key in moves) {
      setZ(zValue + moves[event.key]);
    } else if (event.key === 'Home') {
      setZ(0);
    } else if (event.key === 'End') {
      setZ(1);
    } else {
      return;
    }

    // Paso de teclado = edición cerrada (`end`), solo si cambió de verdad
    // (lastZ se actualiza aquí: un segundo End ya no viaja).
    if (zValue !== lastZ) {
      lastZ = zValue;
      onEdit?.(MORPH_IDS.z, zValue, 'end');
      for (const id of LAYER_IDS) onEdit?.(id, zValue, 'end');
    }
    event.preventDefault();
  });

  // Lo que el cable ya sabe del eje z (dedupe del teclado / sin eco de paint).
  let lastZ = zValue;

  renderZ();

  // ------------------------------ retorno ---------------------------------

  return {
    element,
    pad,
    setZMod,

    /**
     * Repinta desde el snapshot: valor de los dos morph (silencioso: nada de
     * eco), el del anillo z, y nombres de ranura en las esquinas.
     */
    paint(parameters, state) {
      paintCorners(state?.models ?? null);

      if (!zDragging) {
        setZ(parameters?.morphZ ?? 0);
        lastZ = zValue;
      }

      if (dragging) return;

      const value = {
        x: parameters?.morphX ?? 0,
        y: parameters?.morphY ?? 0,
      };

      pad.setValue(value);
      last = value;

      // El snapshot de la página es la mitad PÁGINA de la comparación.
      paintDivergence(value);
    },

    destroy() {
      stopTelemetry?.();
      pad.destroy();
      element.textContent = '';
    },
  };
}
