/**
 * La ayuda de GESTOS de la pagina: una sola, en la cabecera, plegada.
 *
 * Por que existia como hueco (2026-09-28): los botones redondos son 29 de los
 * controles que ve el usuario y el componente compartido les da CUATRO gestos
 * (arrastrar, Shift fino, rueda y flechas, mas el doble clic) y no se le
 * contaba a nadie en ninguna parte. La unica ayuda que habia —"Gestos del pad
 * XY", dentro del cajon de MODELOS— documenta los dos controles mas RAROS (pad
 * y aro, que ya llevan su pista en el `title`) y se esconde justo donde nadie
 * busca una ayuda general. Es decir: los controles mas numerosos, sin
 * documentacion; los mas raros, con documentacion.
 *
 * ESTE TEXTO ES LA FUENTE UNICA de los gestos de esta pagina, y sale de aqui
 * —no de los componentes— porque los controles son del paquete compartido
 * (`@abdsynths/shared`, otro repo) y aqui no hay un sitio comun donde descubrir
 * un gesto nuevo. Las magnitudes estan medidas contra el componente, no de memoria:
 *
 *   - arrastrar = valor 1:1, con la pista del dial; `Shift` = 0.2x (drag-core:
 *     `event.shiftKey ? 0.2 : 1.0`), o sea 1/5 de velocidad;
 *   - rueda = `-deltaY / (dragLanePx * 20)`, un veinteavo de la pista por muesca;
 *   - teclado = SOLO las cuatro flechas, a paso fijo (`step: 0.01`, o sea 1% del
 *     rango normalizado). Ni RePag ni Inicio/Fin: esos son del ARO, que tiene su
 *     propio manejador. Documentarlos aqui habria sido mentira;
 *   - doble clic = `clamp01(options.value ?? 0)`, y la WebUI construye el knob
 *     con `value: 0` (`ui/controls.js`), asi que el gesto va al MINIMO del
 *     rango de cualquier parametro, no a su valor por defecto. Se dice lo que
 *     hace, que es lo unico honesto, y el defecto es del paquete compartido (no
 *     se puede arreglar desde aqui; queda anotado en HANDOFF.md).
 *
 * Lo que NO se repite: el pad y el aro ya llevan su gesto en el `title` y su
 * lista completa esta en el cajon de MODELOS, asi que aqui solo se remite. Y la
 * pista de teclado de las esquinas se importa de donde vive (`xyPad.js`), para
 * que la palabra no tenga dos versiones.
 */

import { CORNER_KEYBOARD_HINT } from './xyPad.js';

export const GESTURE_HELP_SUMMARY = 'Gestos';

/** [termino, descripcion] — la lista, en el orden que se lee. */
export const GESTURE_HELP_ITEMS = [
  ['Botón redondo', 'arrastrar = valor · doble clic = al mínimo del rango'],
  ['Botón redondo, fino', 'con Shift = 1/5 de velocidad · rueda = paso fino'],
  ['Botón redondo, teclado', 'flechas = ±1% del rango (con el foco en el botón)'],
  ['Pad XY y aro de morphZ', 'llevan su propia pista encima · la lista completa, en el cajón de MODELOS'],
  ['Esquinas A–D del pad', `pulsar = abrir MODELOS en esa ranura · ${CORNER_KEYBOARD_HINT} con foco`],
];

/**
 * El `<details>` de la cabecera. Plegado no ocupa mas que su resumen; la lista
 * es un POPOVER (posicion absoluta en la CSS) porque la cabecera tiene altura
 * fija y anadirla como fila mas empujaria el LCD y las bandas — la geometria
 * que mide el arnés y que fijan las referencias visuales.
 */
export function createGestureHelp() {
  const element = document.createElement('details');
  element.className = 'gesture-help';

  const summary = document.createElement('summary');
  summary.textContent = GESTURE_HELP_SUMMARY;

  const list = document.createElement('ul');

  for (const [term, description] of GESTURE_HELP_ITEMS) {
    const item = document.createElement('li');
    const strong = document.createElement('strong');
    strong.textContent = `${term}: `;

    item.append(strong, document.createTextNode(description));
    list.append(item);
  }

  element.append(summary, list);

  return { element };
}
