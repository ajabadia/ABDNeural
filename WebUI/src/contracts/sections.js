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
  // 900 hasta el pad dibujado (8.3), 990 con el bloque compuesto de MODELOS,
  // 728 con la ficha al CENTRO (solo pad en el lienzo). La separacion
  // FILTRO/ENVOLVENTES + la caja LFO (8.3) lo llevan a 892: la banda del
  // fondo ahora la manda el FRONTAL del LFO (2 filas de rate+depth); la
  // matriz y el global son armazon de cajon. La CSS declara el MISMO numero
  // (--abd-canvas-h, test de geometria); el fit-stage escala en ventanas
  // menores. 946 = 892 + la fila del LCD superior (GEOMETRY.lcd = 54, la
  // migracion del LcdDisplay del C++) + 8 de aire (regla del test de encaje);
  // el editor C++ entrega este alto (canvasHeight de NEURONiKEditor.h).
  height: 946,
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
  lcd: 54,           // fila del LCD superior (pantalla 2 líneas + D-pad, con aire ≥8: regla del test de encaje)
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
 * `model-pad` es el pad XY de la ficha MODELOS (morphX/morphY, 8.3): en el lienzo
 * va SOLO el pad, porque la ficha vive en el centro del synthe y su cuerpo es el
 * que cierra su banda.
 *
 * `model-slots` es el DETALLE del cajon de la ficha MODELOS: las cuatro ranuras de
 * modelo espectral A-D (los loadA..loadD del panel nativo) y el espectral de
 * parciales en vivo. NO son parametros: no tienen id en el APVTS y sus botones
 * piden al HOST un fichero, asi que `parameterIds` va vacio. El dato que pintan
 * (nombre cargado por ranura) llega por el puente en `modelsState`.
 */
export const SECTION_VISUALS = {
  /*
   * ENVOLVENTES (diseño 9.x): la única curva ADSR se retira y entran las DOS
   * vistas de la ficha, una por sitio:
   *
   *   - `envelope-curves` es el LIENZO: dos columnas (ENV 1 amp / ENV 2
   *     filtro), cada una con su curva y debajo las rutas que la MATRIZ tiene
   *     asignadas a su fuente (6/7), con su amount. Se alimenta de las ocho
   *     ADSR y de los doce ids de la matriz: lo que pinta es lo que el cable
   *     lleva, sin copia de las rutas.
   *   - `envelope-blocks` es el CAJÓN: un bloque por envolvente con SU curva
   *     encima de sus cuatro knobs. El panel monta las celdas dentro de los
   *     bloques que devuelve la vista (`blockOf`).
   *
   * Ninguna reparte celdas (son vistas, como el resumen de la matriz): con la
   * ficha pasada a cajón, sus ids ya no empujan el encaje del lienzo.
   */
  'envelope-curves': {
    id: 'envelope-curves',
    parameterIds: [
      'envAttack', 'envDecay', 'envSustain', 'envRelease',
      'filterAttack', 'filterDecay', 'filterSustain', 'filterRelease',
      'mod1Source', 'mod1Destination', 'mod1Amount',
      'mod2Source', 'mod2Destination', 'mod2Amount',
      'mod3Source', 'mod3Destination', 'mod3Amount',
      'mod4Source', 'mod4Destination', 'mod4Amount',
    ],
    // Cuerpo que cierra SU banda (la de ENVOLVENTES + MODELOS + EFECTOS): 206
    // de ficha - 42 de armazon = 164, el MISMO cuerpo que piden el pad de
    // MODELOS y el cuerpo real de la banda en el navegador. Es el mecanismo del
    // pad; el sobrante lo absorbe el SVG (fila 1fr), asi que con cero rutas las
    // curvas estiran y nada queda hueco.
    minBodyHeight: 164,
  },
  'envelope-blocks': {
    id: 'envelope-blocks',
    parameterIds: ['envAttack', 'envDecay', 'envSustain', 'envRelease', 'filterAttack', 'filterDecay', 'filterSustain', 'filterRelease'],
  },
  // El pad SOLO: es lo que vive en la ficha del lienzo (el cuerpo de la ficha
  // lo cierra su minBodyHeight). Edita morphX/morphY: UN control UN nodo — la
  // capa del pad es la unica declaracion visual (camino B, FASE 10).
  'model-xy': {
    id: 'model-xy',
    parameterIds: ['morphX', 'morphY'],
    // Cuerpo del pad en el lienzo: es lo que cierra su banda (antes lo cerraba
    // el bloque compuesto de ranuras + pad). BALANCEO 9.x: 164, no 150 — la
    // banda la manda ENVOLVENTES (164 de cuerpo) y el pad se estira con ella;
    // a 150 dejaba 16 px muertos en el centro de la banda. El 220x150+14 del
    // constructor vive en ui/xyPad.js (mismo numero, dos caras del mismo
    // acuerdo: pad 150 + fila de readout X/Y % = 164); el test de la ficha
    // MODELOS exige que esto sea > 0.
    minBodyHeight: 164,
  },
  // El detalle del cajon: espectral de parciales en vivo + las cuatro ranuras
  // A-D. Los morph van en `parameterIds` porque el espectral se alimenta del
  // mismo morfeo; las ranuras, como siempre, del estado del puente.
  'model-slots': {
    id: 'model-slots',
    parameterIds: ['morphX', 'morphY'],
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
    // TRANSICION 9.3: con el LFO en la banda del motor, la banda del fondo es
    // matriz + global (ambas fichas de cajon, sin filas): sin cuerpo propio la
    // banda colapsa al armazon (42). El cuerpo de la casa (164, el mismo del
    // pad y las curvas) la mantiene a 206 hasta el reparto definitivo de la
    // fila que el usuario decidira.
    minBodyHeight: 164,
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
    // CAMINO B (FASE 10): morphX/morphY dejan las celdas — el pad `model-xy`
    // de MODELOS ya los edita en el lienzo y `model-slots` los pinta en el
    // cajon (misma declaracion de fuente, un control un nodo). Su hueco lo
    // ocupan los tres shaping del motor ADITIVO (rolloff/parity/shift), que
    // estaban de visita en RESONADOR: aqui es donde pertenecen.
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
      'resonatorRolloff',
      'resonatorParity',
      'resonatorShift',
      'oscExciteNoise',
      'excitationColor',
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
    // El banco modal y su excitacion: resonancia, impulso y arco (FASE 10).
    ids: ['resonatorRes', 'impulseMix', 'oscExciteBow'],
  },
  {
    id: 'filter',
    title: 'FILTRO',
    subtitle: 'Filtro multimodo',
    // SEPARACION 8.3: la antigua FILTRO & ENVOLVENTE se parte en dos fichas.
    // BALANCEO 9.3: filterEnvAmount se RETIRO (2026-09-26) — la ruta ENV 2 ->
    // Filter Cutoff de la matriz es LA profundidad (su amount, bipolar) y el
    // knob era una segunda profundidad en el mismo camino. Con dos controles,
    // la ficha se ESTRECHA a 2 carriles y los APILA (una columna, un control
    // encima de otro); el LFO sube a SU derecha (misma banda del motor).
    span: 2,
    columns: 1,
    ids: ['filterCutoff', 'filterRes'],
  },
  {
    // BALANCEO 9.3: la caja LFO SUBE a la banda del motor, a la derecha del
    // FILTRO (que al perder filterEnvAmount se estrecho a 2 carriles apilados).
    // Mismo mueble: frontal 2x2 (rate/depth de cada LFO), cajon con forma,
    // sync y division. Su salida ya es de matriz (fuentes 1 y 2).
    id: 'lfo',
    title: 'LFO 1 & 2',
    subtitle: 'Forma, tempo y profundidad',
    // CAJA 8.3: en el lienzo quedan rate + depth de cada LFO (2x2); forma,
    // sync y division viven en el cajon (EDIT). Su SALIDA ya es de matriz
    // (LFO 1 y LFO 2 son las fuentes 1 y 2): la profundidad del LFO es el
    // trim de su fuente y las rutas concretas se editan en la MATRIZ.
    span: 2,
    columns: 2,
    drawer: {
      badge: '4 LFO',
      trigger: 'EDIT',
      // Los cuatro del frontal NO se replican en el cajon (un control, un
      // nodo DOM; patron masterLevel en GLOBAL & MASTER).
      frontal: ['lfo1RateHz', 'lfo1Depth', 'lfo2RateHz', 'lfo2Depth'],
    },
    ids: [
      'lfo1RateHz',
      'lfo1Depth',
      'lfo2RateHz',
      'lfo2Depth',
      'lfo1Waveform',
      'lfo1SyncMode',
      'lfo1RhythmicDivision',
      'lfo2Waveform',
      'lfo2SyncMode',
      'lfo2RhythmicDivision',
    ],
  },
  {
    id: 'envelopes',
    title: 'ENVOLVENTES',
    subtitle: 'ENV 1 y ENV 2 · destinos desde la matriz',
    // DISENO 9.x: las DOS ADSR se editan en su CAJON (un bloque por
    // envolvente, su curva encima de sus cuatro knobs); en el lienzo quedan
    // las dos curvas y, debajo de cada una, las rutas que la MATRIZ tiene
    // asignadas a su fuente (ENV 1/ENV 2 son fuentes 6 y 7). La profundidad
    // de la ruta ENV 2 -> Filter Cutoff es el amount de la propia ruta (el
    // knob filterEnvAmount se retiro: era la misma profundidad dos veces).
    span: 5,
    columns: 2,
    visual: 'envelope-curves',
    drawer: {
      badge: '2 ADSR',
      trigger: 'EDIT',
      // La vista del cajon NO reparte celdas: el detalle la monta el panel
      // entera (ver buildCard) y las celdas caen dentro por `blockOf`.
      visual: 'envelope-blocks',
      blocks: [
        ['envAttack', 'envDecay', 'envSustain', 'envRelease'],
        ['filterAttack', 'filterDecay', 'filterSustain', 'filterRelease'],
      ],
    },
    ids: [
      'envAttack',
      'envDecay',
      'envSustain',
      'envRelease',
      'filterAttack',
      'filterDecay',
      'filterSustain',
      'filterRelease',
    ],
  },
  {
    id: 'models',
    title: 'MODELOS A–D',
    subtitle: 'Parciales del motor',
    span: 2,
    columns: 1,
    // MUDANZA 8.3: la ficha se ADELANTA al centro del synthe (entre FILTRO &
    // ENVOLVENTE y EFECTOS, el morfeo es el corazon del motor) y en el lienzo
    // queda SOLO el pad XY (visual `model-xy`). El detalle (espectral en vivo
    // + las cuatro ranuras) vive en el cajon: visual `model-slots`, que el
    // panel monta DENTRO del cajon (ver ui/panel.js) con los mismos handlers
    // que el interface (mismas variables, sin duplicar estado).
    visual: 'model-xy',
    drawer: {
      badge: '4 RANURAS',
      trigger: 'EDIT',
      // El detalle es una VISTA (model-slots), no celdas del APVTS: el panel
      // la monta entera en el cuerpo del cajon y el flujo de controles no la
      // toca (ver buildCard).
      visual: 'model-slots',
    },
    // MORPH-Z (FASE 10): tercer eje temporal de los frames. Ficha con cajon =>
    // su celda vive SOLO en el cajon (bajo la vista de ranuras), el lienzo se
    // queda con el pad. El anillo del pad puede modularlo desde la matriz
    // (destino 28), que es el gesto Neuron por excelencia.
    //
    // FASE 11.3: morphZ2/morphZ3 son los z de las CAPAS 1 y 2 del modelo (el
    // sampler por capa). Son parametros del APVTS, asi que la pagina tiene que
    // POSEERLOS —el store ignora los mensajes de un id que no tiene, y el motor
    // los mueve: un preset, la matriz, la automatizacion— y su control va aqui,
    // junto al de morphZ: las tres perillas del eje temporal, una por capa.
    ids: ['morphZ', 'morphZ2', 'morphZ3'],
  },
  {
    id: 'fx',
    title: 'EFECTOS',
    subtitle: 'Saturación · delay · chorus · reverb',
    span: 5,
    columns: 6,
    // MUDANZA 8.3: pierde un carril, el que gana MODELOS al adelantarse al
    // centro. Sin cajon: 12 controles en la rejilla 6x2, solo mas estrechos.
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
    id: 'modMatrix',
    title: 'MATRIZ DE MODULACIÓN',
    subtitle: '4 rutas: fuente → destino → cantidad',
    // TRANSICION 9.3: con el LFO subido a la banda del motor, esta banda queda
    // matriz + global: la matriz toma los 2 carriles que el LFO dejo libres
    // (el resumen respira mas ancho). PENDIENTE: el usuario decidira el
    // reparto final de esta ultima fila mas adelante.
    span: 8,
    columns: 8,
    // Sus 12 celdas viven en el cajón (ver la cabecera): en el lienzo queda el
    // resumen de las 4 rutas y el botón. `groups` es la agrupación con la que el
    // cajón las pinta (una fila por ruta) y el test exige que sea, en orden, `ids`.
    // Banda con GLOBAL & MASTER (la mudanza de MODELOS al centro las deja solas).
    // El trigger es EDIT con icono: es el PRIMER `data-drawer-trigger` del DOM y
    // el selftest del host lo usa como ancla del cajón de la matriz.
    drawer: {
      badge: '4 RUTAS',
      // El distintivo es un DATO VIVO, no una constante de la ficha: `liveBadge`
      // declara de QUE se cuenta —los ids de FUENTE, una ruta por cada uno— y el
      // panel lo recalcula con cada snapshot y lo escribe con `setHeader` del
      // mueble compartido (ver ui/panel.js). Una ruta esta ASIGNADA cuando su
      // fuente no es la primera opcion ("Off"); con los defaults del contrato son
      // dos (ENV 1 y ENV 2), y el literal '4 RUTAS' no podia decirlo. El literal
      // queda para lo que si es: el inventario antes del primer paint.
      liveBadge: {
        ids: ['mod1Source', 'mod2Source', 'mod3Source', 'mod4Source'],
        label: 'RUTAS',
      },
      trigger: 'EDIT',
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
  {
    // MUDANZA 8.3: GLOBAL & MASTER vive al final del lienzo, abajo a la
    // izquierda (primera de su banda: cierra la lectura). Patron de la
    // matriz: en el lienzo queda el master visible y EDITAR abre el cajon
    // con el resto (tempo, MIDI, congelados). RANDOM vive aqui por ser
    // accion de ESTADO (todo el APVTS, con los freeze como filtro).
    id: 'globalFull',
    title: 'GLOBAL & MASTER',
    subtitle: 'Tempo, MIDI, congelados y aleatorio',
    // TRANSICION 9.3: la banda del fondo es MATRIZ (8) + global (4) — el LFO
    // vive ahora en la banda del motor. El master sigue en la primera celda.
    span: 4,
    columns: 4,
    action: 'randomize',
    drawer: {
      badge: '8 GLOBAL',
      // Los desplegables del cajon son los MISMO select que pinta el lienzo
      // cuando su ficha los tiene (misma variable, mismo DOM por id), asi que
      // no hay estado que sincronizar: es el mismo nodo repartido en dos sitios.
      trigger: 'EDIT',
      // Sin `groups`: el cajon apila en una columna (patron global, no el de
      // rutas de la matriz) TODOS los ids menos el control base: masterLevel
      // lo pinta buildCard en la ficha (es el `input[type=range]` que consulta
      // el host, contrato de 8.1 paso 2c) y el cajon no recibe copia.
    },
    // masterLevel va PRIMERO: el control base siempre en la primera celda.
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
  // Una ficha de cajon no aporta filas... salvo que tenga FRONTAL (caja
  // LFO): sus controles SI viven en la rejilla del lienzo y empujan el
  // encaje igual que una ficha normal.
  if (section.drawer) {
    return section.drawer.frontal
      ? Math.ceil(section.drawer.frontal.length / section.columns)
      : 0;
  }

  return Math.ceil(section.ids.length / section.columns);
}

/**
 * Alto de una ficha: relleno + cabecera + hueco + filas de celdas (con sus
 * huecos) + borde. Con cero filas (ficha de cajón) solo cuenta el armazón.
 */
export function cardHeight(section) {
  const rows = rowsOf(section);
  const rowsStack = rows * GEOMETRY.cell + Math.max(rows - 1, 0) * GEOMETRY.cardRowGap;

  // Una vista puede pedir cuerpo propio (el pad XY de MODELOS: ranuras + pad).
  // Con filas de celdas el max no cambia nada; con cero filas (ficha de cajon
  // o de motor) es lo que decide si la ficha estira o cierra su banda.
  const visualBody = SECTION_VISUALS[section.visual]?.minBodyHeight ?? 0;

  return GEOMETRY.cardPadding
    + GEOMETRY.cardHeader
    + GEOMETRY.cardRowGap
    + Math.max(rowsStack, visualBody)
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
    + GEOMETRY.lcd
    + bands
    + GEOMETRY.keys
    + GEOMETRY.footer
    + (BANDS.length + 2) * GEOMETRY.bandGap;
}
