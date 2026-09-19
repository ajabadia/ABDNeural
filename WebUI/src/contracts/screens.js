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

import { SECTION_PARAMETER_IDS } from './sections.js';

/** Ids de la pestaña GENERAL del panel nativo (mirador del contrato del host). */
export const GENERAL_PARAMETER_IDS = [
  'engineType',
  'envAttack', 'envDecay', 'envSustain', 'envRelease',
  'unisonDetune', 'unisonSpread',
  'randomStrength',
  'freezeResonator', 'freezeFilter', 'freezeEnvelopes',
];

/**
 * Todos los ids que posee el store: ahora los 70 del lienzo, en orden de lectura.
 * Antes eran "los del puente + los de GENERAL"; con el lienzo único la página
 * cubre el contrato entero y no queda ningún parámetro sin control.
 */
export const SCREEN_PARAMETER_IDS = SECTION_PARAMETER_IDS;

/** Valor del atributo `data-tab` de la franja de teclado. */
export const KEYS_TAB = 'keys';

/** Selector con el que el host y esta página localizan la franja de teclado. */
export const KEYS_TAB_SELECTOR = '[data-tab="keys"]';
