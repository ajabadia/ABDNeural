/**
 * Ajuste del lienzo de diseño al viewport del editor
 * ==================================================
 *
 * El lienzo es de diseño FIJO (`CANVAS`: 1440x990 — el encaje de los 70
 * controles es un número, no una impresión) y el editor es redimensionable:
 * sin ajuste, una ventana más baja que el diseño CORTA por abajo el pie y la
 * franja de teclado (el fallo que trajo "no se distinguen las teclas").
 *
 * Solución de la página (el componente compartido del teclado no tiene culpa
 * ni arreglo aquí): escalar el lienzo con `transform` para que CAIGA ENTERO
 * en el viewport — el "escala del contenedor" que el ROADMAP pone como
 * sustituto del zoom nativo. Rango de escala como el zoom que tenía el
 * editor: 0.25x a 3x; se centra en el eje que sobra.
 *
 * `transform` no cambia el BOX de layout: el body va con `overflow: hidden`
 * (ver main.css) para que el lienzo sin escalar no genere scrollbars.
 */

const MIN_SCALE = 0.25;
const MAX_SCALE = 3;

/**
 * Escala y centrado para que el diseño entre entero en el viewport.
 *
 * @param {object} p
 * @param {number} p.viewportWidth   ancho disponible (CSS px)
 * @param {number} p.viewportHeight  alto disponible (CSS px)
 * @param {number} p.designWidth     ancho del lienzo de diseño
 * @param {number} p.designHeight    alto del lienzo de diseño
 * @returns {{ scale: number, offsetX: number, offsetY: number }}
 *   `scale` acotado a [0.25, 3]; los offsets (>= 0) centran el eje que sobra.
 */
export function computeFit({ viewportWidth, viewportHeight, designWidth, designHeight }) {
  const vw = Math.max(1, Number(viewportWidth) || 0);
  const vh = Math.max(1, Number(viewportHeight) || 0);
  const dw = Math.max(1, Number(designWidth) || 0);
  const dh = Math.max(1, Number(designHeight) || 0);

  const scale = Math.min(MAX_SCALE, Math.max(MIN_SCALE, Math.min(vw / dw, vh / dh)));

  return {
    scale,
    offsetX: Math.max(0, (vw - dw * scale) / 2),
    offsetY: Math.max(0, (vh - dh * scale) / 2),
  };
}

/**
 * Monta el ajuste sobre el elemento del lienzo y lo mantiene con cada resize.
 *
 * @param {HTMLElement} stage      el lienzo (#app — lleva el tamaño de diseño en CSS)
 * @param {object} [options]
 * @param {number} options.width   ancho de diseño
 * @param {number} options.height  alto de diseño
 * @param {object} [options.viewport]  objeto con innerWidth/innerHeight y
 *   addEventListener (inyectable para tests; por defecto `window`)
 * @returns {() => void} quita el listener del resize
 */
export function mountFitStage(stage, { width, height, viewport = window } = {}) {
  const apply = () => {
    const { scale, offsetX, offsetY } = computeFit({
      viewportWidth: viewport.innerWidth,
      viewportHeight: viewport.innerHeight,
      designWidth: width,
      designHeight: height,
    });

    stage.style.transformOrigin = 'top left';
    stage.style.transform = `scale(${scale})`;
    stage.style.marginLeft = `${offsetX}px`;
    stage.style.marginTop = `${offsetY}px`;
  };

  apply();
  viewport.addEventListener('resize', apply);

  return () => viewport.removeEventListener('resize', apply);
}
