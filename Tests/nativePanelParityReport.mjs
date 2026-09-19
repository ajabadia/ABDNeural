/**
 * Lienzo web vs. superficies NATIVAS, con el MISMO preset cargado.
 *
 * Comparar "a ojo" dos UIs que viven en dos lenguajes distintos se desvía sola: el
 * inventario nativo está repartido en cinco paneles de C++ y el web sale del
 * contrato. Este script extrae los DOS inventarios de sus fuentes y los cruza
 * id a id, así que cada diferencia del informe se puede volver a comprobar:
 *
 *   - NATIVO: patrones de las fuentes (`setupControl`, `setupChoice`,
 *     `setupAmountControl`, `setupRotaryControl` y los `...Attachment`), con los
 *     bucles de ModulationPanel expandidos (2 LFO y 4 rutas);
 *   - WEB: `contracts/sections.js` (qué id va en cada ficha) + el contrato
 *     generado (`describeControl`: etiqueta, tipo y opciones).
 *
 * El "mismo preset" son los valores NORMALIZADOS de los 70 parámetros: por
 * defecto el INIT del contrato (`defaultNormalized`, lo que carga un plugin
 * recién abierto), y con `--preset <fichero.neuronikpreset>` el estado guardado
 * (XML del APVTS: `<PARAM id="..." value="0..1"/>`). Con ese preset aplicado el
 * informe dice qué parámetros puede VER y EDITAR cada superficie — que es la
 * diferencia que de verdad le importa al usuario que carga un preset.
 *
 * Uso:
 *   node Tests/nativePanelParityReport.mjs
 *   node Tests/nativePanelParityReport.mjs --preset "$APPDATA/NEURONiK/Presets/X.neuronikpreset"
 *   node Tests/nativePanelParityReport.mjs --quiet      (solo el resumen)
 *
 * Sale con código 1 SOLO ante una inconsistencia estructural (un id del lienzo o
 * del C++ que no existe en el contrato, o un id repetido en el lienzo), no porque
 * las dos superficies se vean distintas: eso es el informe, no un fallo.
 */

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import {
  BANDS,
  CANVAS,
  SECTIONS,
  SECTION_PARAMETER_IDS,
} from '../WebUI/src/contracts/sections.js';
import { PARAMETERS, describeControl } from '../WebUI/src/contracts/parameters.js';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..');

// ---------------------------------------------------------------------------
// 1. Inventario NATIVO (extraído de las fuentes de C++)
// ---------------------------------------------------------------------------

/**
 * Las superficies nativas que pueden pintar un control de parámetro. `class` es
 * lo que se busca para saber si alguien la MONTA; el fichero de la propia clase
 * no cuenta como montaje.
 *
 * ESTA LISTA SIGUE AL ÁRBOL, no al revés: el 2026-09-19 se retiraron los cuatro
 * paneles de parámetros, el browser de presets, el LCD y los visualizadores sin
 * montar (ver DOCS/WEBUI_VS_NATIVE_PARITY.md), así que aquí solo quedan las dos
 * superficies que la bancada monta de verdad más el menú del editor. Un fichero
 * que falte es una inconsistencia dura: la lista se quedó atrás.
 */
const NATIVE_SURFACES = [
  { file: 'Source/UI/ParameterPanel.cpp', surface: 'GENERAL (ParameterPanel)', class: 'ParameterPanel' },
  { file: 'Source/UI/XYPad.cpp', surface: 'XY PAD (morph)', class: 'XYPad' },
  { file: 'Source/Main/NEURONiKEditor.cpp', surface: 'MENÚ DEL EDITOR', class: 'NEURONiKEditor' },
];

/**
 * Bucles de los paneles: `IDs::lfo1RateHz` se instancia dos veces
 * (`for (int i = 0; i < 2; ++i) setupLfoControls (i)`) y `IDs::mod1Amount` cuatro
 * (`for (int i = 0; i < 4; ++i) setupModSlotControls (i)`). Se expanden aquí
 * porque en el C++ el id se arma con `.replace ("1", lfoId)` y no aparece literal.
 */
const LOOP_VARS = { lfoId: 2, slotId: 4 };

/**
 * Patrones que SÍ son un control (una lectura de parámetro en un timer no pinta
 * nada, y por eso no se cuentan). `idGroup` es el grupo del id; `loopGroup`, si lo
 * hay, el de la variable del bucle que arma el id con `.replace("1", ...)`.
 */
const CONTROL_PATTERNS = [
  {
    re: /setupControl\s*\(\s*\w+\s*,\s*IDs::(\w+)\s*,\s*"([^"]*)"/g,
    control: 'knob', idGroup: 1, labelGroup: 2,
  },
  {
    re: /setupChoice\s*\(\s*\w+\s*,\s*IDs::(\w+)\s*,\s*"([^"]*)"/g,
    control: 'combo', idGroup: 1, labelGroup: 2,
  },
  {
    re: /setupAmountControl\s*\(\s*[\w.]+\s*,\s*juce::String\s*\(\s*IDs::(\w+)\s*\)\s*\.replace\s*\(\s*"1"\s*,\s*(\w+)\s*\)/g,
    control: 'fader', idGroup: 1, loopGroup: 2,
  },
  {
    re: /setupRotaryControl\s*\(\s*[^,]+,\s*[\w.]+\s*,\s*juce::String\s*\(\s*IDs::(\w+)\s*\)\s*\.replace\s*\(\s*"1"\s*,\s*(\w+)\s*\)\s*,\s*"([^"]*)"/g,
    control: 'knob', idGroup: 1, loopGroup: 2, labelGroup: 3,
  },
  {
    // El id del attachment también puede venir envuelto por el bucle:
    // `ComboBoxAttachment (vts, juce::String(IDs::mod1Source).replace("1", slotId), ...)`.
    // OJO con los grupos: el 1 es la CLASE (la que da el tipo), el id es el 2.
    re: /\b(Button|ComboBox|Slider)Attachment\s*>\s*\(\s*[\w.()]+,\s*(?:juce::String\s*\(\s*)?IDs::(\w+)\s*(?:\)\s*\.replace\s*\(\s*"1"\s*,\s*(\w+)\s*\))?/g,
    control: 1, idGroup: 2, loopGroup: 3,
  },
  {
    re: /getParameter\s*\(\s*IDs::(\w+)/g,
    control: 'menu', idGroup: 1,
  },
];

const ATTACHMENT_CONTROLS = { Button: 'button', ComboBox: 'combo', Slider: 'fader' };

function read (relative) {
  return fs.readFileSync(path.join(root, relative), 'utf8');
}

/** Todo el C++ del proyecto, para saber si una superficie llega a MONTARSE. */
function cppFiles (directory = path.join(root, 'Source')) {
  const found = [];

  for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
    const full = path.join(directory, entry.name);

    if (entry.isDirectory()) found.push(...cppFiles(full));
    else if (/\.(cpp|h)$/.test(entry.name)) found.push(full);
  }

  return found;
}

/** Ficheros (relativos) que instancian esa clase: `make_unique<...Clase>`/`new Clase`. */
function mountingFiles (className, ownFile) {
  const pattern = new RegExp(`(make_unique\\s*<[^>]*?${className}\\s*>|new\\s+[\\w:]*${className}\\s*\\()`);

  return cppFiles()
    .filter((file) => path.relative(root, file).replace(/\\/g, '/') !== ownFile)
    .filter((file) => pattern.test(fs.readFileSync(file, 'utf8')))
    .map((file) => path.relative(root, file).replace(/\\/g, '/'));
}

function nativeInventory () {
  const byId = new Map();
  const missing = [];
  const note = (id, entry) => {
    if (!byId.has(id)) byId.set(id, []);
    byId.get(id).push(entry);
  };

  for (const { file, surface, class: className } of NATIVE_SURFACES) {
    if (!fs.existsSync(path.join(root, file))) {
      missing.push(file);
      continue;
    }

    const source = read(file);
    const mountedIn = mountingFiles(className, file);

    for (const pattern of CONTROL_PATTERNS) {
      const { re, control, idGroup, loopGroup, labelGroup } = pattern;
      re.lastIndex = 0;

      let match;
      while ((match = re.exec(source)) !== null) {
        const control_ = typeof control === 'number'
          ? ATTACHMENT_CONTROLS[match[control]] ?? 'otro'
          : control;

        const rawId = match[idGroup];
        const label = labelGroup ? match[labelGroup] : null;
        const loopVar = loopGroup ? match[loopGroup] : null;

        if (loopVar && LOOP_VARS[loopVar] === undefined)
          throw new Error(`${file}: bucle desconocido "${loopVar}" (añádelo a LOOP_VARS)`);

        const instances = LOOP_VARS[loopVar] ?? 1;

        for (let index = 1; index <= instances; ++index) {
          const id = instances === 1 ? rawId : rawId.replace('1', String(index));

          note(id, { surface, control: control_, label, mountedIn, file });
        }
      }
    }
  }

  return { byId, missing };
}

// ---------------------------------------------------------------------------
// 2. Inventario WEB (el lienzo, tal cual lo pinta la página)
// ---------------------------------------------------------------------------

function webInventory () {
  const byId = new Map();

  for (const section of SECTIONS)
    for (const id of section.ids)
      byId.set(id, { card: section.title, columns: section.columns, ...describeControl(id) });

  return byId;
}

// ---------------------------------------------------------------------------
// 3. El preset: valores NORMALIZADOS 0..1
// ---------------------------------------------------------------------------

/** INIT del contrato: lo que carga un plugin recién abierto. */
function initPreset () {
  const values = new Map();

  for (const descriptor of PARAMETERS)
    values.set(descriptor.id, descriptor.defaultNormalized);

  return { name: 'INIT (defaultNormalized del contrato)', values };
}

/** Estado del APVTS guardado: `<PARAM id="envAttack" value="0.25"/>` (0..1). */
function presetFromFile (file) {
  const xml = fs.readFileSync(file, 'utf8');
  const values = new Map();

  const re = /<PARAM\b[^>]*\bid="([\w]+)"[^>]*\bvalue="([^"]*)"/g;

  let match;
  while ((match = re.exec(xml)) !== null) {
    const value = Number(match[2]);

    if (Number.isFinite(value)) values.set(match[1], value);
  }

  if (values.size === 0)
    throw new Error(`"${file}": no he encontrado ni un <PARAM id=".." value=".."/>`);

  return { name: path.basename(file), values };
}

// ---------------------------------------------------------------------------
// 4. Informe
// ---------------------------------------------------------------------------

function main () {
  const args = process.argv.slice(2);
  const quiet = args.includes('--quiet');
  const presetIndex = args.indexOf('--preset');

  const preset = presetIndex === -1
    ? initPreset()
    : presetFromFile(path.resolve(args[presetIndex + 1] ?? ''));

  const { byId: native, missing } = nativeInventory();
  const web = webInventory();
  const contractIds = new Set(PARAMETERS.map((descriptor) => descriptor.id));
  const problems = [];

  for (const file of missing)
    problems.push(`la superficie nativa declarada ya no existe: ${file} (actualiza NATIVE_SURFACES)`);

  // --- invariantes duras: ids que no existen en el contrato -----------------

  for (const id of web.keys())
    if (!contractIds.has(id)) problems.push(`el lienzo pinta "${id}", que no está en el contrato`);

  for (const id of native.keys())
    if (!contractIds.has(id)) problems.push(`el C++ enlaza "${id}", que no está en el contrato`);

  const duplicates = SECTION_PARAMETER_IDS.filter((id, index) => SECTION_PARAMETER_IDS.indexOf(id) !== index);

  for (const id of duplicates) problems.push(`el lienzo repite "${id}" en dos celdas`);

  if (problems.length > 0) {
    console.error('INCONSISTENCIAS:');
    for (const problem of problems) console.error(`  - ${problem}`);
  }

  // --- el mismo preset en las dos superficies ------------------------------

  const presetIds = [...preset.values.keys()].filter((id) => contractIds.has(id));
  const unknownInPreset = [...preset.values.keys()].filter((id) => !contractIds.has(id));

  if (unknownInPreset.length > 0)
    console.log(`\nOJO: el preset trae ${unknownInPreset.length} id(s) que no están en el contrato: ${unknownInPreset.join(', ')}`);

  const nativeControlled = presetIds.filter((id) => native.has(id));
  const webControlled = presetIds.filter((id) => web.has(id));
  const invisibleNatively = presetIds.filter((id) => !native.has(id));
  const invisibleOnWeb = presetIds.filter((id) => !web.has(id));

/**
   * Dónde acaba montada una superficie. No es lo mismo la bancada de desarrollo
   * (WebPilotHost, que pinta el panel nativo al lado de la página) que el editor
   * que se ENVÍA (`NEURONiKEditor`, que hoy solo monta el WebView): un control que
   * solo vive en la bancada no lo ve ningún usuario del plugin.
   */
  const mountKind = (file) => {
    if (/Source\/WebPilotHost\.cpp$/.test(file)) return 'bancada (WebPilotHost)';
    if (/Source\/Main\/NEURONiK(Editor|Processor)\.cpp$/.test(file)) return 'plugin que se envía';

    return file;
  };

  /** Etiquetas de montaje de un id, sin repetir. */
  const mountsOf = (id) => [...new Set((native.get(id) ?? [])
    .flatMap((entry) => entry.mountedIn.map(mountKind)))];

  const surfaceStats = NATIVE_SURFACES.map(({ surface, class: className, file }) => ({
    surface,
    ids: [...native.entries()].filter(([, entries]) => entries.some((entry) => entry.surface === surface)),
    mountedIn: [...new Set([...native]
      .flatMap(([, entries]) => entries)
      .filter((entry) => entry.surface === surface)
      .flatMap((entry) => entry.mountedIn.map(mountKind)))],
  }));

  const withControl = (id) => native.has(id);
  const mountedIds = presetIds.filter((id) => mountsOf(id).length > 0);
  const shippedIds = presetIds.filter((id) => mountsOf(id).includes('plugin que se envía'));

  console.log('='.repeat(100));
  console.log(`PRESET COMPARADO: ${preset.name} · ${preset.values.size} parámetros guardados`);
  console.log('='.repeat(100));

  console.log(`\nLienzo web ................. ${webControlled.length}/${presetIds.length} del preset visibles (lienzo: ${web.size} celdas, ${SECTIONS.length} fichas, ${CANVAS.width}x${CANVAS.height})`);
  console.log(`Algún control nativo ...... ${nativeControlled.length}/${presetIds.length} (GENERAL + XYPad de la bancada y el menú del editor)`);
  console.log(`  montados (bancada+plug) .. ${mountedIds.length}/${presetIds.length}`);
  console.log(`  montados en el PLUGIN .... ${shippedIds.length}/${presetIds.length} (el editor que se envía solo monta el WebView)`);
  console.log(`  en paneles sin instanciar  ${nativeControlled.length - mountedIds.length}/${presetIds.length}`);

  // --- altas y bajas --------------------------------------------------------

  /** Lista de ids: tabla en modo normal, recuento en línea en modo `--quiet`. */
  const listFor = (ids, detail) => ids.length === 0
    ? '  (ninguno)'
    : quiet
      ? `  ${ids.length}: ${ids.join(', ')}`
      : ids.map((id) => `  ${id.padEnd(26)} ${detail(id)}`).join('\n');

  console.log('\n--- Solo en el lienzo web (el preset los mueve, ningún control nativo los muestra) ---');
  console.log(listFor(invisibleNatively, (id) => web.get(id)?.card ?? ''));

  console.log('\n--- Solo en el nativo (control sin celda en el lienzo) ---');
  console.log(listFor(invisibleOnWeb, (id) => (native.get(id) ?? []).map((entry) => entry.surface).join(', ')));

  // --- id a id --------------------------------------------------------------

  const shared = [...web.keys()].filter((id) => native.has(id)).sort();
  const labelDiffers = [];
  const kindDiffers = [];

  for (const id of shared) {
    const descriptor = web.get(id);
    const entries = native.get(id);
    const labels = [...new Set(entries.map((entry) => entry.label).filter(Boolean))];

    if (labels.length > 0 && !labels.some((label) => label.trim() === String(descriptor.label).trim()))
      labelDiffers.push({ id, web: descriptor.label, native: labels.join(' / ') });

    const kinds = new Set(entries.map((entry) => entry.control));
    const webKind = descriptor.kind === 'float' ? 'knob' : descriptor.kind === 'bool' ? 'button' : 'combo';

    if (!kinds.has(webKind) && !kinds.has('menu'))
      kindDiffers.push({ id, web: webKind, native: [...kinds].join('/'), card: descriptor.card });
  }

  if (!quiet) {
    console.log(`\n--- Etiquetas distintas (${labelDiffers.length} de ${shared.length} compartidos) ---`);
    for (const row of labelDiffers)
      console.log(`  ${row.id.padEnd(26)} web: ${String(row.web).padEnd(24)} nativo: ${row.native}`);

    console.log(`\n--- Tipo de control distinto (${kindDiffers.length} de ${shared.length} compartidos) ---`);
    for (const row of kindDiffers)
      console.log(`  ${row.id.padEnd(26)} web: ${row.web.padEnd(8)} nativo: ${row.native} (${row.card})`);
  } else {
    console.log(`\n  Etiquetas distintas: ${labelDiffers.length}/${shared.length} · tipos distintos: ${kindDiffers.length}/${shared.length}`);
  }

  // --- superficie por superficie -------------------------------------------

  if (!quiet) {
    console.log('\n--- Inventario nativo (superficie -> parámetros que pinta) ---');
    for (const { surface, ids, mountedIn } of surfaceStats)
      console.log(`  ${surface.padEnd(28)} ${String(ids.length).padStart(2)} params`
        + ` · montado en: ${mountedIn.length > 0 ? mountedIn.join(', ') : 'NADIE (compilado y sin instanciar)'}`);
  }

  console.log('\n--- Fichas del lienzo ---');
  for (const band of BANDS)
    console.log(`  ${band.map((section) => `${section.title} (${section.ids.length})`).join(' | ')}`);

  console.log(`\n${problems.length === 0 ? 'Estructura OK' : `${problems.length} inconsistencias`} · ` +
    `${native.size} ids nativos · ${web.size} celdas web · contrato ${contractIds.size}`);

  process.exit(problems.length === 0 ? 0 : 1);
}

main();
