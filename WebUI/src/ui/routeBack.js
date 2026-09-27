/**
 * VOLVER A LA RUTA: el botón que reabre la MATRIZ en el MISMO slot que trajo
 * al usuario a ENVOLVENTES, mientras el retorno siga "fresco".
 *
 * ¿Cuándo es fresco? El panel (dueño del gesto) lo declara: nace con
 * `panelRouteJump(originId, slot, { returnTo: originId })` — un salto con
 * retorno —, se REFRESCA cada vez que el usuario completa una ida y vuelta
 * (cerrar la matriz reabre este cajón y el panel reancla) y muere cuando el
 * usuario cierra el cajón por sí mismo (su onClose limpia `routeReturn`).
 * Sin salto con retorno detrás, el botón no existe: el EDIT o las rutas del
 * lienzo no lo nacen (ese recorrido no deja retorno pendiente).
 *
 * La vista no sabe de gestos: guarda el último destino y pinta/oculta.
 */

/** El único flujo con retorno hoy: ENVOLVENTES -> MATRIZ (IR A LA RUTA). */
export const ROUTE_RETURN_ORIGIN = 'envelopes';
export const ROUTE_RETURN_TARGET = 'modMatrix';

/** Estilo y texto del botón, SSOT para la vista y los tests. */
export const ROUTE_BACK_TEXT = 'VOLVER A LA RUTA';
export const ROUTE_BACK_CLASS = 'env-block__back';

/**
 * Crea el botón VOLVER A LA RUTA y devuelve el control para el panel.
 *
 * El elemento puede colgarse y descolgarse del cuerpo del cajón sin perder
 * el wiring: el panel solo juega con `element.hidden` y `textContent`.
 */
export function createRouteBack() {
  const element = document.createElement('button');
  element.type = 'button';
  element.className = ROUTE_BACK_CLASS;
  element.hidden = true; // sin salto con retorno detrás, no existe
  element.addEventListener('click', () => onBack?.(lastTarget));

  let lastTarget = null;
  let onBack = null;

  return {
    element,

    /**
     * Declara el destino fresco tras un salto con retorno (o reancla el que ya
     * había, tras una ida y vuelta completa): slot visible y botón armado.
     */
    anchor(targetSlot) {
      lastTarget = Number(targetSlot) || 0;
      element.hidden = false;
      element.textContent = `${ROUTE_BACK_TEXT} ${lastTarget}`;
      element.title = `Reabre la RUTA ${lastTarget} en la MATRIZ DE MODULACIÓN`;
    },

    /** El retorno murió (cierre propio del cajón o salto nuevo sin retorno):
     * botón fuera del DOM — sin gesto pendiente, no existe. */
    clear() {
      lastTarget = null;
      element.hidden = true;
      element.removeAttribute('title');
      element.remove();
    },

    /** El gesto de volver: el panel lo cablea a su openDrawerRoute. */
    onBack(handler) {
      onBack = typeof handler === 'function' ? handler : null;
    },

    /** Lectura de prueba: el slot fresco, o null si el botón no existe. */
    target() {
      return element.hidden ? null : lastTarget;
    },
  };
}
