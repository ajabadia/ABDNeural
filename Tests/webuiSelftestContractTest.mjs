/**
 * ABDNeural — anti-drift del contrato entre el selftest del puente y la pagina.
 *
 * El selftest de seis direcciones (`Source/WebUI/BridgeSelftest.h`) habla con la
 * WebUI por unos anclajes concretos: el primer `input[type=range]`, el `<code>` del
 * pie de pagina, la pestana KEYS, la rueda de modulacion del teclado compartido, la
 * ficha MODELOS A-D (fila + nombre de cada ranura), el cajon lateral de la matriz
 * (disparador, cajon abierto, ruta y la celda de un parametro) y el helper de MIDI de
 * la pagina (`__pilotSendMidi`). Ademas comprueba que los 11 ids de la pestana GENERAL
 * esten en el estado de la pagina.
 *
 * Son un CONTRATO, no detalles: si alguien renombra un anclaje en el C++ o lo quita de
 * la pagina, el selftest deja de comprobar lo que cree que comprueba y aun asi puede
 * dar OK (o fallar por una razon que no es la suya). Este test lo fija por los dos
 * lados, en el mismo espiritu que `bridgeProtocolContractTest.mjs` (literales del
 * contrato versionado contra el codigo real) y el guard de direccion del canal:
 *
 *   1. los anclajes declarados en el C++ son EXACTAMENTE los esperados;
 *   2. cada anclaje sigue vivo en el fichero de la pagina que lo posee;
 *   3. cada anclaje sigue PINCHADO en la suite de la propia pagina (quitarlo de la
 *      pagina no puede pasar inadvertido);
 *   4. los 11 ids de GENERAL coinciden con `GENERAL_PARAMETER_IDS` de la pagina.
 *
 * ABDNeural no tiene runner JS: se lanza con `node` desde ctest, igual que los otros
 * dos. Exit 0 si el contrato esta entero, 1 si algo se ha movido.
 */

import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import { fileURLToPath } from 'node:url';

const testsDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.dirname(testsDirectory);

const failures = [];

function check(condition, description) {
  if (condition) {
    console.log(`  [ok]   ${description}`);
    return;
  }
  console.log(`  [FAIL] ${description}`);
  failures.push(description);
}

const read = (...segments) => fs.readFileSync(path.join(repositoryRoot, ...segments), 'utf8');

/** Todos los ficheros .js / .css de un arbol, en rutas relativas a `base`. */
function listSources(relativeDirectory) {
  const absolute = path.join(repositoryRoot, relativeDirectory);
  const found = [];

  for (const entry of fs.readdirSync(absolute, { withFileTypes: true, recursive: true })) {
    if (!entry.isFile()) continue;
    if (!/\.(js|css)$/.test(entry.name)) continue;

    const full = path.join(entry.parentPath ?? entry.path, entry.name);
    found.push({ relative: path.relative(repositoryRoot, full).replaceAll('\\', '/'), text: fs.readFileSync(full, 'utf8') });
  }

  return found;
}

// --- 1. Los anclajes del arnes C++ ------------------------------------------------

const harnessSource = read('Source', 'WebUI', 'BridgeSelftest.h');

const namespaceStart = harnessSource.indexOf('namespace SelftestPage');
check(namespaceStart >= 0, 'BridgeSelftest.h declara el namespace SelftestPage');

const namespaceBlock = namespaceStart >= 0
  ? harnessSource.slice(namespaceStart, harnessSource.indexOf('\n}', namespaceStart))
  : '';

/** Un literal C++ con sus escapes ya resueltos. */
const unescape = (raw) => raw.replaceAll('\\"', '"').replaceAll('\\\\', '\\');

const declaredAnchors = new Map();

for (const match of namespaceBlock.matchAll(/inline constexpr const char\*\s+(\w+)\s*=\s*"((?:[^"\\]|\\.)*)";/g)) {
  declaredAnchors.set(match[1], unescape(match[2]));
}

// Los valores esperados. Si un anclaje cambia de forma en el C++, este test falla
// hasta que el cambio se acepte TAMBIEN en la pagina (y en su suite).
const expectedAnchors = {
  firstRange: 'input[type=range]',
  stateCode: '.panel-footer code',
  keysTab: '[data-tab="keys"]',
  modWheelSlider: '#mod-wheel-container .kbd-wheel-slider',
  drawerTriggerAttribute: 'data-drawer-trigger',
  openDrawerClass: 'drawer--open',
  visibleBackdropClass: 'drawer-backdrop--visible',
  drawerSlotClass: 'drawer-slot',
  parameterCellAttribute: 'data-parameter-id',
  modelSlotRow: '.model-slots__row',
  modelSlotName: '.model-slots__name',
  midiHelper: '__pilotSendMidi',
};

for (const [name, value] of Object.entries(expectedAnchors)) {
  check(declaredAnchors.get(name) === value, `SelftestPage::${name} sigue siendo "${value}"`);
}

// --- 2 y 3. Cada anclaje vive en la pagina y esta pinchado en su suite -----------

const pageSources = listSources(path.join('WebUI', 'src'));
const pageTests = listSources(path.join('WebUI', 'tests'));

/**
 * Quien PUBLICA cada anclaje en la pagina (el contrato lo dice en su comentario), y
 * con que forma. `keysTab` y las dos ranuras de modelo difieren: el host consulta
 * SELECTORES (`[data-tab="keys"]`, `.model-slots__row`) y la pagina lo que publica en
 * el modulo que los crea es el NOMBRE de la clase o el atributo, asi que ahi se busca
 * la mitad que la pagina posee de verdad (la otra mitad —el punto— la pone el CSS,
 * que tambien lleva el literal).
 */
const anchorOwners = {
  firstRange: { files: ['WebUI/src/app.js', 'WebUI/src/ui/panel.js'] },
  stateCode: { files: ['WebUI/src/ui/panel.js'] },
  keysTab: { files: ['WebUI/src/contracts/screens.js'], pageForm: 'data-tab="keys"' },
  modWheelSlider: { files: ['WebUI/src/ui/keyboard.js'] },
  // Los cuatro anclajes del cajon son TOKENS (el atributo o la clase), no selectores
  // completos: el selector lo compone el script del arnes. El disparador ademas lo
  // ESCRIBE el panel como `dataset.drawerTrigger`, asi que su mitad es esa.
  drawerTriggerAttribute: { files: ['WebUI/src/ui/panel.js'], pageForm: 'dataset.drawerTrigger' },
  openDrawerClass: { files: ['WebUI/src/ui/drawer.js'], pageForm: 'drawer--open' },
  visibleBackdropClass: { files: ['WebUI/src/ui/drawer.js'], pageForm: 'drawer-backdrop--visible' },
  drawerSlotClass: { files: ['WebUI/src/ui/panel.js'], pageForm: 'drawer-slot' },
  parameterCellAttribute: { files: ['WebUI/src/ui/panel.js'], pageForm: 'data-parameter-id' },
  modelSlotRow: { files: ['WebUI/src/ui/modelSlots.js'], pageForm: 'model-slots__row' },
  modelSlotName: { files: ['WebUI/src/ui/modelSlots.js'], pageForm: 'model-slots__name' },
  midiHelper: { files: ['WebUI/src/contracts/paramStore.js'] },
};

for (const [name, owner] of Object.entries(anchorOwners)) {
  const anchor = expectedAnchors[name];
  const pageForm = owner.pageForm ?? anchor;

  const inOwner = owner.files.some((file) => {
    const source = pageSources.find((candidate) => candidate.relative === file);
    return source !== undefined && source.text.includes(pageForm);
  });

  check(inOwner, `${name} ("${pageForm}") sigue en su sitio de la pagina (${owner.files.join(', ')})`);

  const pinnedInSuite = pageTests.some((file) => file.text.includes(anchor));
  check(pinnedInSuite, `${name} sigue pinchado en la suite de la pagina (WebUI/tests)`);
}

// --- 4. Los 11 ids de GENERAL, por los dos lados ---------------------------------

const generalIdsBlock = /inline constexpr const char\* generalIds\[\]\s*=\s*\{([\s\S]*?)\};/.exec(namespaceBlock);
const cppGeneralIds = generalIdsBlock
  ? [...generalIdsBlock[1].matchAll(/"([^"]+)"/g)].map((match) => match[1])
  : [];

const screensSource = read('WebUI', 'src', 'contracts', 'screens.js');
const screensBlock = /export const GENERAL_PARAMETER_IDS\s*=\s*\[([\s\S]*?)\];/.exec(screensSource);
const pageGeneralIds = screensBlock
  ? [...screensBlock[1].matchAll(/'([^']+)'/g)].map((match) => match[1])
  : [];

check(cppGeneralIds.length === 11, `el arnes declara 11 ids de GENERAL (encontrados: ${cppGeneralIds.length})`);
check(
  pageGeneralIds.length === 11,
  `la pagina declara 11 ids de GENERAL (encontrados: ${pageGeneralIds.length})`,
);
check(
  JSON.stringify(cppGeneralIds) === JSON.stringify(pageGeneralIds),
  'los dos lados listan los MISMOS ids de GENERAL, en el mismo orden',
);

if (cppGeneralIds.length > 0 && pageGeneralIds.length > 0
    && JSON.stringify(cppGeneralIds) !== JSON.stringify(pageGeneralIds)) {
  console.error(`       C++   : ${cppGeneralIds.join(', ')}`);
  console.error(`       pagina: ${pageGeneralIds.join(', ')}`);
}

// --- 5. Las seis direcciones son obligatorias --------------------------------------
//
// Hasta el ticket 8.4 el arnes podia declarar una direccion NO APLICABLE cuando el
// dueno servia la pagina retirada del piloto, que no llevaba ni la ficha MODELOS A-D
// ni el cajon de la matriz. Con el piloto fuera no hay una segunda pagina a la que
// rebajar el liston, asi que la capacidad desaparece y ninguna de las dos se puede
// omitir. Este check fija que la puerta no vuelva por la puerta de atras:

check(
  !/PageCapabilities|retiredPilotPage|capabilitiesToUse/.test(harnessSource),
  'el arnes no conserva la maquinaria de direcciones NO APLICABLES (se fue con el piloto retirado)',
);

const skipMarks = [...harnessSource.matchAll(/matrixSkipped|modelsSkipped/g)];
check(
  skipMarks.length === 0,
  `ninguna direccion se puede saltar (encontrado: ${skipMarks.length} marca(s) de omitido)`,
);

// --- Veredicto -------------------------------------------------------------------

if (failures.length > 0) {
  console.error(`\nSelftest contract: ${failures.length} problema(s). El selftest y la pagina se han desviado.`);
  process.exit(1);
}

console.log('\nSelftest contract: OK (anclajes y ids de GENERAL fijados por los dos lados).');
