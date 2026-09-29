/**
 * Selección de parámetros de la página y anclajes que el host consulta.
 *
 * Aquí vivían las pestañas provisionales del andamiaje de 8.1 (BRIDGE, GENERAL,
 * KEYS). El ticket 8.2 las retira: el reparto vive ahora en `sections.js` y la
 * página es UN lienzo único con todas las fichas. Este fichero conserva las dos
 * cosas que NO son reparto y que un consumidor externo tiene fijadas:
 *
 *   1. `GENERAL_PARAMETER_IDS`: los 11 ids que el selftest del host comprueba en
 *      el estado de la página (`Source/WebUI/BridgeSelftest.h`). Su forma de
 *      declaración está pinchada por `Tests/webuiSelftestContractTest.mjs`: el
 *      regex de ese test espera exactamente `export const GENERAL_PARAMETER_IDS
 *      = [ 'id', ... ];` con los mismos ids y en el mismo orden que el C++.
 *   2. `KEYS_TAB` / `KEYS_TAB_SELECTOR`: el anclaje de la franja de
 *      interpretación. El host pulsa `[data-tab="keys"]` antes de leer la rueda
 *      de modulación, así que el atributo sigue existiendo aunque ya no haya
 *      pestañas; el selector se publica aquí para que la suite de la página lo
 *      use tal cual (si alguien lo renombra, falla aquí y no dentro de WebView2).
 */

import { SECTION_PARAMETER_IDS, SECTION_VISUALS } from './sections.js';
import { PARAMETERS } from './parameters.js';

/** Ids de la pestaña GENERAL del panel nativo (mirador del contrato del host). */
export const GENERAL_PARAMETER_IDS = [
  'engineType',
  'envAttack', 'envDecay', 'envSustain', 'envRelease',
  'unisonDetune', 'unisonSpread',
  'randomStrength',
  'freezeResonator', 'freezeFilter', 'freezeEnvelopes',
];

/**
 * Los ids que solo viven en una VISTA, sin celda de ficha que los reclame: el pad
 * XY de MODELOS edita morphX/morphY, que dejaron las celdas en la FASE 10 (camino
 * B). Salen del catálogo (`SECTION_VISUALS`), no de una lista escrita aquí.
 */
export const VISUAL_PARAMETER_IDS = [
  ...new Set(Object.values(SECTION_VISUALS).flatMap((visual) => visual.parameterIds ?? [])),
].filter((id) => !SECTION_PARAMETER_IDS.includes(id));

/**
 * Los ids que el store POSEYE pero que ninguna ficha reclama.
 *
 * POSEEDOS Y NO PINTADOS SON COSAS DISTINTAS, y confundirlas rompe el store en
 * silencio. Un id fuera de `SCREEN_PARAMETER_IDS` es un id que el store ignora
 * cuando llega del host (la regla de abajo), asi que un id del bus que no
 * estuviera aqui haria que mover el selector de efecto en el host no le llegara
 * a la pagina: el knob se quedaria quieto en pantalla mientras el motor ya habia
 * cambiado.
 *
 * DESDE EL 2026-09-29 SON LOS SESENTA DEL BUS: los cuatro huecos publican su
 * bus entero (tipo, ganancia, mezcla y doce mandos), y la pagina solo tiene
 * celda para el hueco 1. Los otros tres huecos (y los mandos 5..12 del hueco 1)
 * son ids que el store POSEE y todavia no PINTA, que es exactamente lo que esta
 * lista es.
 *
 * LOS QUE LA PAGINA YA PINTA NO ESTAN, y no por limpieza: el hueco 1 esta en
 * `SECTION_PARAMETER_IDS` (su modulo esta en la ficha de efectos), y un id en
 * las dos listas esta dos veces en `SCREEN_PARAMETER_IDS`, que es lo que
 * `screens.test.js` prohibe.
 *
 * Y SE CALCULA DEL CONTRATO, no se escribe. Setenta y dos ids del bus en una
 * lista a mano son setenta y dos sitios donde un hueco nuevo se queda fuera sin
 * que nada lo diga --y lo que pasaria es que el store descartaria su mensaje y el
 * knob se moveria en el host sin que la pagina se enterase, que es el fallo que
 * esta lista existe para tapar--. El prefijo del hueco es lo unico que los
 * distingue (`fx1Type`, `fx4Param12`), asi que la regla basta.
 *
 * LO QUE ESTA LISTA NO DICE, y hay que decirlo porque se confunde con lo de
 * arriba: POSEER no es PINTAR. Un id de aqui no tiene celda todavia, y por eso
 * `sections.test.js` los cuenta aparte. La lista no es "lo que la pagina
 * enseña": es "lo que la pagina no puede ignorar".
 */
export const SLOT_MODULE_PARAMETER_IDS = PARAMETERS
  .map((parameter) => parameter.id)
  .filter((id) => /^fx[1-4](Type|Gain|Mix|Param[0-9]+)$/.test(id)
                 && !SECTION_PARAMETER_IDS.includes(id));

/**
 * Todos los ids que posee el store: los del lienzo, en orden de lectura, MÁS los
 * que solo viven en una vista y los del modulo del hueco.
 *
 * Los dos importan por lo MISMO, y es una regla del store, no un detalle: ignora
 * los mensajes nativos de un id que no tiene (`entry.id in parameters`), así que un
 * id sin dueño se queda en su valor local para siempre — el motor lo mueve (un
 * preset, el RANDOM, el XYPad nativo, la automatización) y la página no se entera.
 * Con morphX/morphY fuera de esta lista, el pad dibujaba una esquina que el motor
 * ya no tenía: la divergencia salió en vivo, con el banco CZ101 cargado.
 */
export const SCREEN_PARAMETER_IDS = [
  ...SECTION_PARAMETER_IDS,
  ...VISUAL_PARAMETER_IDS,
  ...SLOT_MODULE_PARAMETER_IDS,
];

/** Valor del atributo `data-tab` de la franja de teclado. */
export const KEYS_TAB = 'keys';

/** Selector con el que el host y esta página localizan la franja de teclado. */
export const KEYS_TAB_SELECTOR = '[data-tab="keys"]';
