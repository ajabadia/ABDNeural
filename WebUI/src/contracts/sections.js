/**
 * Reparto del lienzo: qué fichas hay, cuántos carriles ocupa cada una y qué ids
 * del contrato van en cada una.
 *
 * Es DATO, no marcado: el panel lo recorre para construir el DOM, el store saca
 * de aquí los ids que posee y el test de encaje (tests/sections.test.js) mide con
 * estos mismos números si los 70 controles caben en el lienzo. Una ficha no puede
 * desviarse de lo que la página pinta porque las dos cosas salen de aquí.
 *
 * Decisión de diseño (ticket 8.2, revisado): NEURONiK se pinta en UN SOLO LIENZO,
 * grande, sin pestañas de parámetros. Los hermanos de la suite no lo hacen así
 * (ABDMS2000 tiene editor de 1080x680 con `slideDrawer` por sección), y por eso
 * el lienzo es bastante mayor: 1440x900 de superficie de página. Con eso caben
 * los 70 controles repartidos en tres bandas de fichas.
 *
 * Y con eso CABÍAN, que no es lo mismo que estar bien: 70 controles a la vez es
 * mucha densidad, y la matriz de modulación (12 celdas, 4 rutas de tres campos)
 * es la sección que peor se lee apretada contra las demás. Así que una ficha puede
 * declarar `drawer`: sus celdas se montan en el cajón lateral (el patrón de
 * ABDMS2000/ABDEep/ABDCZ101) y en el lienzo queda un resumen de sus valores más el
 * botón que lo abre. Los `ids` NO se mudan de aquí: el reparto sigue siendo el del
 * contrato y lo que cambia es DÓNDE se pinta la celda, así que el store, el
 * recuento del selftest y la cobertura de los 70 siguen intactos.
 *
 * La agrupación NO es inventada: sigue los paneles nativos de `Source/UI/Panels`
 * (OSCILLATOR, FILTER/ENV, FX, MODULATION) más la pestaña GENERAL de
 * `Source/UI/ParameterPanel.cpp`. El orden de los ids dentro de una ficha también
 * es el nativo, para que la vista y el panel de JUCE se lean igual.
 *
 * Geometría: los números de abajo los usa `styles/main.css` como custom
 * properties (`--abd-canvas-w`, `--abd-cell-h`...). El test de encaje comprueba
 * que la CSS declara los mismos valores, así que cambiar uno sin el otro falla.
 *
 * Dos fichas NO tienen celdas y por eso se declaran aparte en los tests: la de
 * cajón (sus controles viven en el panel lateral) y la de MODELOS (sus cuatro
 * ranuras son del motor, no del APVTS). Ninguna de las dos aporta filas al encaje,
 * y las DOS siguen apareciendo en el reparto porque es la SSOT de qué se pinta.
 */

/**
 * Lienzo de diseño. Es el área ÚTIL de la página (lo que ocupa el WebView en el
 * editor a tamaño base): la ventana del plugin añade su barra de menú aparte
 * (ver `NEURONiKEditor::menuBarHeight`).
 */
export const CANVAS = {
  width: 1440,
  height: 900,
  /** Carriles de la rejilla horizontal; las bandas suman exactamente esto. */
  lanes: 12,
};

/**
 * Alturas del armazón, en píxeles de diseño. La celda de control manda: el resto
 * se reparte alrededor. `cell` es una celda de knob (dial 48 + etiqueta + lectura
 * + separaciones); una celda de choice (desplegable) es más baja y sobra sitio,
 * que es lo que quiere una rejilla regular.
 *
 * Los nombres describen EXACTAMENTE lo que aporta cada uno a la altura; el
 * borde y el hueco entre filas están aquí porque la primera cuenta que hice los
 * dejó fuera y el lienzo desbordaba 33 px (lo cazó la medición en un motor real,
 * no el test). `panelPadding` es el relleno del armazón (6 arriba + 4 abajo).
 */
export const GEOMETRY = {
  knob: 48,          // diámetro del dial (--abd-knob-size): manda el alto de celda
  top: 40,           // cabecera: título, estado del puente, contrato
  bandGap: 10,       // separación entre bandas
  cardHeader: 20,    // título de la ficha
  cardPadding: 16,   // relleno interior de la ficha (8 + 8)
  cardRowGap: 4,     // hueco entre filas de celdas dentro de una ficha
  cardBorder: 2,     // 1 px arriba + 1 px abajo
  cell: 80,          // alto de una fila de controles
  keys: 120,         // franja de interpretación (ruedas + teclado)
  footer: 44,        // pie de página (el `<code>` que el host lee como JSON)
  panelPadding: 10,  // relleno del armazón de la página (6 arriba + 4 abajo)
};

/**
 * Acciones de ficha: botones que NO son parámetros (no tienen id en el APVTS),
 * asi que no ocupan celda ni cuentan en el encaje. Van aqui, con el resto del
 * reparto, para que el panel no tenga casos especiales por ficha.
 *
 * `randomize` es la unica hoy: pide al HOST que sortee el estado (el procesador
 * tiene los rangos y los congelados; la pagina no puede inventarse ese sorteo).
 * Vivio en el panel nativo hasta que se retiro (ticket 8.2).
 */
/**
 * Vistas de ficha: dibujos que NO son controles (no tienen parametro propio, asi
 * que no ocupan celda de control ni cuentan en el encaje). Cada una declara de que
 * parametros se alimenta.
 *
 * `amp-envelope` es la curva ADSR: se pinta en la celda LIBRE de su ficha (11
 * controles en una rejilla de 6x2), por eso anadirla no mueve la geometria.
 *
 * `mod-summary` es el resumen de las 4 rutas de la matriz: NO ocupa celda (llena el
 * cuerpo de su ficha, que en el lienzo ya no tiene celdas porque las suyas viven en
 * el cajon). Se alimenta de los 12 ids de la matriz.
 *
 * `model-slots` son las cuatro ranuras de modelo espectral A-D (los loadA..loadD del
 * panel nativo). NO son parametros: no tienen id en el APVTS y sus botones piden al
 * HOST un fichero, asi que `parameterIds` va vacio y la ficha no ocupa celda. El dato
 * que pinta (nombre cargado por ranura) llega por el puente en `modelsState`.
 */
export const SECTION_VISUALS = {
  'amp-envelope': {
    id: 'amp-envelope',
    parameterIds: ['envAttack', 'envDecay', 'envSustain', 'envRelease'],
  },
  'model-slots': {
    id: 'model-slots',
    parameterIds: [],
  },
  'mod-summary': {
    id: 'mod-summary',
    parameterIds: [
      'mod1Source',
      'mod1Destination',
      'mod1Amount',
      'mod2Source',
      'mod2Destination',
      'mod2Amount',
      'mod3Source',
      'mod3Destination',
      'mod3Amount',
      'mod4Source',
      'mod4Destination',
      'mod4Amount',
    ],
  },
};

export const SECTION_ACTIONS = {
  randomize: {
    id: 'randomize',
    label: 'RANDOM',
    title: 'Sortea el timbre en el plugin, respetando los congelados y la fuerza de STRENGTH',
  },
};

/**
 * Las siete fichas, en orden de lectura. `span` es cuantos carriles ocupa y
 * `columns` cuantas celdas tiene por fila dentro; los ids van en el orden en que
 * se pintan (izquierda a derecha, arriba abajo). `action` (opcional) es un boton
 * de ficha del catalogo de arriba.
 */
export const SECTIONS = [
  {
    id: 'oscillator',
    title: 'OSCILADOR',
    subtitle: 'Motor espectral · excitación · unísono',
    span: 6,
    columns: 6,
    ids: [
      'engineType',
      'oscLevel',
      'oscInharmonicity',
      'oscRoughness',
      'morphX',
      'morphY',
      'oscExciteNoise',
      'excitationColor',
      'impulseMix',
      'unisonEnabled',
      'unisonDetune',
      'unisonSpread',
    ],
  },
  {
    id: 'resonator',
    title: 'RESONADOR',
    subtitle: 'Banco de resonadores',
    span: 2,
    columns: 2,
    ids: ['resonatorRes', 'resonatorRolloff', 'resonatorParity', 'resonatorShift'],
  },
  {
    id: 'global',
    title: 'GLOBAL & MASTER',
    subtitle: 'Nivel, tempo, MIDI y congelados',
    span: 4,
    columns: 5,
    // La accion RANDOM vive aqui porque es una accion de ESTADO (todo el APVTS,
    // con los tres freeze como filtro), no de una seccion de timbre concreta.
    action: 'randomize',
    // masterLevel va PRIMERO: su fader es el `input[type=range]` que el selftest
    // del host conduce en las dos direcciones (contrato de 8.1 paso 2c).
    ids: [
      'masterLevel',
      'masterBPM',
      'velocityCurve',
      'midiChannel',
      'midiThru',
      'randomStrength',
      'freezeResonator',
      'freezeFilter',
      'freezeEnvelopes',
    ],
  },
  {
    id: 'filterEnv',
    title: 'FILTRO & ENVOLVENTE',
    subtitle: 'Envolvente de amplitud · filtro multimodo',
    span: 5,
    columns: 6,
    // La curva va en la celda que sobra (11 controles en 6x2), asi que el encaje
    // de la ficha no cambia: sigue siendo la misma rejilla de dos filas.
    visual: 'amp-envelope',
    ids: [
      'envAttack',
      'envDecay',
      'envSustain',
      'envRelease',
      'filterCutoff',
      'filterRes',
      'filterEnvAmount',
      'filterAttack',
      'filterDecay',
      'filterSustain',
      'filterRelease',
    ],
  },
  {
    id: 'fx',
    title: 'EFECTOS',
    subtitle: 'Saturación · delay · chorus · reverb',
    span: 7,
    columns: 6,
    ids: [
      'fxSaturation',
      'fxDelayTime',
      'fxDelayFeedback',
      'fxDelaySync',
      'fxDelayDivision',
      'fxChorusRate',
      'fxChorusDepth',
      'fxChorusMix',
      'fxReverbSize',
      'fxReverbDamping',
      'fxReverbWidth',
      'fxReverbMix',
    ],
  },
  {
    id: 'lfo',
    title: 'LFO 1 & 2',
    subtitle: 'Forma, tempo y profundidad',
    span: 4,
    columns: 5,
    ids: [
      'lfo1Waveform',
      'lfo1RateHz',
      'lfo1SyncMode',
      'lfo1RhythmicDivision',
      'lfo1Depth',
      'lfo2Waveform',
      'lfo2RateHz',
      'lfo2SyncMode',
      'lfo2RhythmicDivision',
      'lfo2Depth',
    ],
  },
  {
    id: 'models',
    title: 'MODELOS A–D',
    subtitle: 'Parciales del motor',
    span: 2,
    columns: 1,
    // Ranuras del MOTOR, no parametros: la ficha no tiene celdas (ver
    // SECTION_VISUALS['model-slots']), asi que no entra en el encaje de los 70.
    visual: 'model-slots',
    ids: [],
  },
  {
    id: 'modMatrix',
    title: 'MATRIZ DE MODULACIÓN',
    subtitle: '4 rutas: fuente → destino → cantidad',
    span: 6,
    columns: 6,
    // Sus 12 celdas viven en el cajón (ver la cabecera): en el lienzo queda el
    // resumen de las 4 rutas y el botón. `groups` es la agrupación con la que el
    // cajón las pinta (una fila por ruta) y el test exige que sea, en orden, `ids`.
    drawer: {
      badge: '4 RUTAS',
      trigger: 'EDITAR',
      groups: [
        ['mod1Source', 'mod1Destination', 'mod1Amount'],
        ['mod2Source', 'mod2Destination', 'mod2Amount'],
        ['mod3Source', 'mod3Destination', 'mod3Amount'],
        ['mod4Source', 'mod4Destination', 'mod4Amount'],
      ],
    },
    visual: 'mod-summary',
    ids: [
      'mod1Source',
      'mod1Destination',
      'mod1Amount',
      'mod2Source',
      'mod2Destination',
      'mod2Amount',
      'mod3Source',
      'mod3Destination',
      'mod3Amount',
      'mod4Source',
      'mod4Destination',
      'mod4Amount',
    ],
  },
];

/** Todos los ids del lienzo, en orden de lectura y sin duplicados. */
export const SECTION_PARAMETER_IDS = SECTIONS.flatMap((section) => section.ids);

/**
 * Las bandas: fichas consecutivas cuyos carriles suman el ancho del lienzo.
 * Se DERIVAN de los spans (no se declaran a mano) para que una ficha nueva no
 * pueda dejar un hueco que nadie vigile: la suma de cada banda se comprueba en
 * el test y en tiempo de construcción del panel.
 */
export const BANDS = (() => {
  const bands = [];
  let current = [];

  for (const section of SECTIONS) {
    current.push(section);

    const used = current.reduce((total, candidate) => total + candidate.span, 0);

    if (used === CANVAS.lanes) {
      bands.push(current);
      current = [];
    } else if (used > CANVAS.lanes) {
      throw new Error(
        `sections: la banda desborda el lienzo (${used} > ${CANVAS.lanes}) al añadir "${section.id}"`,
      );
    }
  }

  if (current.length > 0) {
    const used = current.reduce((total, candidate) => total + candidate.span, 0);
    throw new Error(`sections: la última banda queda a medias (${used} de ${CANVAS.lanes})`);
  }

  return bands;
})();

/**
 * Filas de celdas que una ficha ocupa en el LIENZO. Una ficha de cajón no tiene
 * ninguna: sus celdas no están en la rejilla, así que no empujan el encaje (su alto
 * en el lienzo lo pone la banda, al estirarse como cualquier ficha).
 */
export function rowsOf(section) {
  if (section.drawer) return 0;

  return Math.ceil(section.ids.length / section.columns);
}

/**
 * Alto de una ficha: relleno + cabecera + hueco + filas de celdas (con sus
 * huecos) + borde. Con cero filas (ficha de cajón) solo cuenta el armazón.
 */
export function cardHeight(section) {
  const rows = rowsOf(section);

  return GEOMETRY.cardPadding
    + GEOMETRY.cardHeader
    + GEOMETRY.cardRowGap
    + rows * GEOMETRY.cell
    + Math.max(rows - 1, 0) * GEOMETRY.cardRowGap
    + GEOMETRY.cardBorder;
}

/**
 * Alto total del lienzo si se apilan las bandas con su separación. Es la cuenta
 * que el test compara con CANVAS.height: "cabe" es un número, no una impresión.
 *
 * Los huecos de banda son los del armazón (cabecera, lienzo, franja y pie: 3) más
 * los de dentro del lienzo (bandas - 1) y el relleno del armazón.
 */
export function canvasHeight() {
  const bands = BANDS.reduce(
    (total, band) => total + Math.max(...band.map(cardHeight)),
    0,
  );

  return GEOMETRY.panelPadding
    + GEOMETRY.top
    + bands
    + GEOMETRY.keys
    + GEOMETRY.footer
    + (BANDS.length + 2) * GEOMETRY.bandGap;
}
