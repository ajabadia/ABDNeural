/**
 * Cajón lateral deslizante — el patrón de los hermanos de la suite (ABDMS2000,
 * ABDEep, ABDCZ101): panel fijo a la derecha, fondo que atenúa el lienzo, cierre
 * con ESC y con el botón, y `role="dialog"` para que sea un diálogo de verdad.
 *
 * DIFERENCIA DELIBERADA con el `slideDrawer` de ABDMS2000: allí `open()` recibe un
 * `render(container)` y **reconstruye** el contenido en cada apertura. Aquí no:
 * las celdas se construyen UNA vez (las monta el panel) y el cajón solo se desplaza.
 * Tres razones, y las tres son contratos de esta página:
 *
 *   1. los 70 controles tienen que estar SIEMPRE en el documento: el selftest del
 *      host y la suite cuentan celdas, y un control que solo existe mientras el
 *      cajón está abierto rompería ese recuento (y el `paint` del panel, que pinta
 *      estado sobre celdas que él mismo posee);
 *   2. reconstruir por apertura pierde el foco y el gesto en curso (un arrastre
 *      que empieza con el cajón abierto se quedaría sin destino);
 *   3. con el DOM estable, abrir/cerrar es una clase CSS: nada que pueda fallar.
 *
 * Es genérico (no sabe de la matriz de modulación): recibe título, distintivo y un
 * cuerpo donde el llamador pone lo que quiera. Candidato a ABDSharedAssets el día
 * que los tres hermanos quieran uno solo; hoy cada uno tiene el suyo.
 */

const OPEN_CLASS = 'drawer--open';
const BACKDROP_CLASS = 'drawer-backdrop--visible';

/**
 * @param {object} options
 * @param {string} options.id        id del cajón (y de su fondo).
 * @param {string} [options.title]   título del encabezado.
 * @param {string} [options.badge]   distintivo corto (p. ej. "4 RUTAS").
 * @param {string} [options.closeLabel] etiqueta accesible del cierre.
 * @returns {{ element: HTMLElement, backdrop: HTMLElement, body: HTMLElement,
 *             header: HTMLElement, open: Function, close: Function, toggle: Function,
 *             isOpen: Function, destroy: Function }}
 */
export function createDrawer({
  id,
  title = '',
  badge = '',
  closeLabel = 'Cerrar (Esc)',
}) {
  const backdrop = document.createElement('div');
  backdrop.className = 'drawer-backdrop';
  backdrop.dataset.drawerBackdrop = id;

  const element = document.createElement('aside');
  element.className = 'drawer';
  element.id = id;
  element.dataset.drawer = id;
  element.setAttribute('role', 'dialog');
  element.setAttribute('aria-modal', 'true');
  element.setAttribute('aria-label', title);
  element.setAttribute('aria-hidden', 'true');

  const header = document.createElement('div');
  header.className = 'drawer__header';

  const badgeElement = document.createElement('span');
  badgeElement.className = 'drawer__badge';
  badgeElement.textContent = badge;

  const titleElement = document.createElement('h2');
  titleElement.className = 'drawer__title';
  titleElement.textContent = title;

  const closeButton = document.createElement('button');
  closeButton.type = 'button';
  closeButton.className = 'drawer__close';
  closeButton.textContent = '✕';
  closeButton.title = closeLabel;
  closeButton.setAttribute('aria-label', closeLabel);

  const body = document.createElement('div');
  body.className = 'drawer__body';

  header.append(badgeElement, titleElement, closeButton);
  element.append(header, body);

  let open = false;
  // Un componente destruido no muta: si no, `open()` sobre un cajon ya quitado
  // seguiria cambiando estado y clases de nodos que nadie verá.
  let destroyed = false;

  const onKeyDown = (event) => {
    if (event.key === 'Escape' && open) closeDrawer();
  };

  closeButton.addEventListener('click', () => closeDrawer());
  backdrop.addEventListener('click', () => closeDrawer());
  document.addEventListener('keydown', onKeyDown);

  function closeDrawer() {
    if (destroyed || !open) return;

    open = false;
    element.classList.remove(OPEN_CLASS);
    element.setAttribute('aria-hidden', 'true');
    backdrop.classList.remove(BACKDROP_CLASS);
  }

  function openDrawer() {
    if (destroyed || open) return;

    open = true;
    element.classList.add(OPEN_CLASS);
    element.setAttribute('aria-hidden', 'false');
    backdrop.classList.add(BACKDROP_CLASS);
  }

  // La página los cuelga del documento (position: fixed), como los hermanos.
  document.body.append(backdrop, element);

  return {
    element,
    backdrop,
    body,
    header,
    open: openDrawer,
    close: closeDrawer,
    toggle: () => (open ? closeDrawer() : openDrawer()),
    isOpen: () => open,
    destroy() {
      destroyed = true;
      document.removeEventListener('keydown', onKeyDown);
      backdrop.remove();
      element.remove();
    },
  };
}
