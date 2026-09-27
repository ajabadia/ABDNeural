/**
 * Fábrica de vistas de ficha: convierte el catálogo (`SECTION_VISUALS`) en la vista
 * ya montada, con los view-models de los parámetros que la alimentan.
 *
 * Vive en su propio módulo, y no dentro de app.js, porque la usan DOS consumidores:
 * la página (app.js) y la suite del panel (tests/panel.test.js). Cuando la fábrica
 * estaba escrita dentro del test, el harness montaba una curva ADSR para cualquier
 * vista declarada; al añadir el resumen de la matriz, el test contó dos curvas y
 * falló señalando al sitio equivocado. Una sola fábrica, un solo comportamiento.
 *
 * Una vista es DATO de ficha (no tiene parámetro propio ni celda): la página
 * compone, el panel solo le da sitio y estado. Los handlers que NO son dibujo
 * (cargar una ranura, editar el pad) viajan por `options`, que es quien los
 * conecta con el store.
 *
 * Las vistas no son lo mismo por dentro y no se disimula: la curva y el resumen
 * son DIBUJOS (se repintan con los parámetros); las ranuras PIDEN (su botón lanza
 * una carga en el host); el pad EDITA (dos morphs con gesto completo). Por eso
 * reciben handlers por `options` y por eso `paint` recibe el estado entero, del
 * que cada una lee lo suyo.
 */

import { createEnvelopeBlocks, createEnvelopeCurves } from './envelopeViews.js';
import { createSpectral } from './spectral.js';
import { createModelSlots } from './modelSlots.js';
import { createModSummary } from './modSummary.js';
import { createXyPad } from './xyPad.js';

import { MOD_DESTINATIONS } from '../../generated/parameters.generated.js';

/** Indice de telemetria del destino Morph Z (leido del contrato, no copiado). */
const MORPH_Z_TARGET = MOD_DESTINATIONS.findIndex((d) => d?.parameterId === 'morphZ');

/**
 * @param {string} visualId  id del catálogo SECTION_VISUALS
 * @param {object[]} controls  view-models de los parámetros que pide esa vista
 * @param {object} [options]
 * @param {(slot: number) => void} [options.onLoad]  carga de una ranura de modelo
 *   (`model-slots`): la ejecuta el host, así que la vista la recibe por aquí y no
 *   la pide al panel, que no sabe qué dibuja.
 * @param {(id: 'morphX'|'morphY', normalized: number,
 *          phase: 'begin'|'change'|'end') => void} [options.onEdit]  edición del
 *   pad XY (`model-slots`): el store la cierra con su protocolo de gestos.
 * @param {(notify: Function) => Function} [options.onTelemetry]  canal de
 *   telemetría en vivo (`model-slots`): lo consume el espectral; es pintura,
 *   no estado, así que va por su propio camino y no por el paint de snapshots.
 * @returns {{ element: HTMLElement, paint: Function, destroy?: Function }|null}
 *   null cuando el catálogo declara una vista que nadie construye (el llamador lo
 *   avisa en consola en vez de pintar un hueco vacío).
 */
export function createVisual(visualId, controls, options = {}) {
  // La ficha ENVOLVENTES pinta DOS curvas (ENV 1 amp / ENV 2 filtro), cada una
  // con debajo sus rutas de matriz. El catálogo `envelope-curves` pide las ocho
  // ADSR + los doce de la matriz: `routeControls` viaja por options porque los
  // demás catálogos no lo necesitan (y el test de la fábrica no lo suelta).
  if (visualId === 'envelope-curves') {
    const curves = createEnvelopeCurves({
      controls,
      routeControls: options.routeControls ?? [],
      // Aguja de nivel: el frame de telemetria trae envelopes=[amp, filter].
      onTelemetry: options.onTelemetry ?? null,
    });

    // El opener llega TARDE (la vista se fabrica antes del panel): app.js lo
    // conecta con setRouteOpener cuando ya tiene el panel a mano.
    curves.setRouteOpener(options.onOpenRoute ?? null);

    return curves;
  }

  // El cuerpo del CAJÓN de ENVOLVENTES: un bloque por envolvente (curva encima
  // de sus cuatro knobs). Los knobs los monta el panel dentro de los bloques
  // que devuelve la vista (`blockOf`); aquí solo se fabrica con los ids.
  if (visualId === 'envelope-blocks') {
    const blocks = createEnvelopeBlocks({
      controls,
      ids: options.ids ?? [],
      // IR A LA RUTA: resuelve el slot contra el snapshot (los doce de la
      // matriz), igual que las rutas del lienzo.
      routeControls: options.routeControls ?? [],
      // Aguja de nivel del cajon: el MISMO canal que el lienzo (el app.js ya
      // se lo pasa; la vista lo consumia desde el paro).
      onTelemetry: options.onTelemetry ?? null,
    });

    // El opener llega TARDE (la vista se fabrica antes del panel).
    blocks.setRouteOpener(options.onOpenRoute ?? null);

    return blocks;
  }

  // Las 4 rutas de la matriz: sus 12 controles viven en el cajón, así que en el
  // lienzo va el resumen (y el cajón da el detalle). Filas BOTON: el mismo salto
  // que las rutas de ENVOLVENTES — el opener llega tarde (setRouteOpener).
  if (visualId === 'mod-summary') {
    const summary = createModSummary({ controls });

    summary.setRouteOpener(options.onOpenRoute ?? null);

    return summary;
  }

  // La ficha MODELOS A-D vive en el CENTRO del synthe (mudanza 8.3) y en el
  // lienzo solo lleva el pad XY (morphX/morphY con los nombres en las esquinas):
  // es el corazon del motor y no necesita mas en pantalla.
  if (visualId === 'model-xy') {
    const pad = createXyPad({ onEdit: options.onEdit ?? null });

    // El anillo exterior (morphZ) vive de la TELEMETRIA: frame.modulation[t]
    // es la contribucion con signo que la matriz acumula sobre el destino.
    // En standalone el feed es el meter del worklet (app.js lo enchufa a la
    // misma vista via setZMod): dos caminos, un solo destino.
    const stopTelemetry = options.onTelemetry && MORPH_Z_TARGET >= 0
      ? options.onTelemetry((frame) => {
        const modulation = Array.isArray(frame?.modulation) ? frame.modulation : [];
        const contribution = modulation[MORPH_Z_TARGET];

        if (typeof contribution === 'number')
          pad.setZMod(contribution);
      })
      : null;

    return {
      element: pad.element,
      paint(parameters, state) {
        pad.paint(parameters, state);
      },
      // Feed standalone (meter del worklet, destino 28): app.js enchufa
      // onWorkletMorphZ aqui. En plugin lo hace la telemetria nativa
      // (onTelemetry, arriba) — mismo pad, dos caminos.
      setZMod(mod) {
        pad.setZMod(mod);
      },
      destroy() {
        stopTelemetry?.();
        pad.destroy();
      },
    };
  }

  // El detalle del cajon de MODELOS: espectral de parciales en vivo (telemetria,
  // nunca snapshots: un snapshot no debe congelar las barras) encima de las
  // cuatro ranuras. La mitad que el interface no ensena: mismos handlers, mismo
  // puente, mismas variables que el interface.
  if (visualId === 'model-slots') {
    const slots = createModelSlots({ onLoad: options.onLoad ?? null });
    const spectral = createSpectral({ onFrame: options.onTelemetry ?? null });

    const element = document.createElement('div');
    element.className = 'model-block model-block--drawer';
    element.append(spectral.element, slots.element);

    return {
      element,
      paint(parameters, state) {
        slots.paint(parameters, state);
      },
      destroy() {
        spectral.destroy();
        slots.destroy();
      },
    };
  }

  return null;
}
