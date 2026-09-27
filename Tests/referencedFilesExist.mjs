/**
 * ABDNeural — guard de ficheros REFERENCIADOS (todo lo que el build y el E2E
 * nombran tiene que estar versionado).
 *
 * Que caza: el fallo de "funciona aqui y se rompe en un clone". Un fichero que
 * el CMakeLists compila, o que el `playwright.config.js` levanta con un
 * `webServer`, puede quedarse sin `git add` sin que se note en la maquina donde
 * se escribio — y en el clone se rompe con un 404 o con un target que no
 * compila, muy lejos del cambio que lo produjo.
 *
 * Ya paso, y de la forma mas tore: el `webServer` segundo del `playwright.config.js`
 * arranca un servidor de Vite para `/needle-probe/` (la pagina de prueba de la
 * aguja), y ni el spec (`WebUI/e2e/needleProbe.spec.js`) ni su fixture
 * (`WebUI/needle-probe/`) estaban versionados. En local, 22/22 en verde; en un
 * clone limpio, el `url` de arranque da 404 y `pnpm test:e2e` se queda esperando
 * hasta el timeout, sin decir que falta ni quien lo nombro.
 *
 * Que mira, y por que cada cosa:
 *   - CMakeLists.txt: cada ruta de fichero que nombra (comentarios fuera: un
 *     `WebUI/BridgeSelftest.h` escrito en un comentario no es una dependencia).
 *     Un GLOB con `*` o `${...}` no se comprueba: es salida de build o depende
 *     de una variable, no un fichero del repo.
 *   - playwright.config.js: cada `*.spec.js` del `testDir` (los nombrados y los
 *     que anaden despues, que es como entro la sonda), la `index.html` de cada
 *     `webServer.url` con ruta propia, los ficheros que esa pagina y su script
 *     referencian en local, y cada `npm run <script>` de los `command` contra
 *     los scripts de verdad de WebUI/package.json (un comando a un script que no
 *     existe revienta al arrancar, no al escribir).
 *
 * Mismo trato que `workletSyncTest.mjs`: el fallo NOMBRA el fichero y quien lo
 * nombro, y sin git (un tarball, un zip de release) avisa y pasa en vez de
 * inventar un veredicto.
 *
 * Se lanza de ctest: NEURONiK_ReferencedFiles (este fichero via node).
 */

import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const thisDir = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(thisDir, '..');
const webuiDir = path.join(root, 'WebUI');

/** Rutas posix relativas a la raiz del repo: es como las devuelve git. */
const toRepoRelative = (absolute) =>
  path.relative(root, absolute).split(path.sep).join('/');

// ── Los versionados, o null si aqui no hay git (tarball, zip de release) ──
let tracked = null;

try {
  // `stdio` con el stderr tapado: en un tarball (sin .git) el `fatal: not a git
  // repository` de git no es un fallo del guard, y verlo en el log de ctest
  // confunde mas de lo que informa. El SKIP de abajo es el que dice la verdad.
  const listing = execFileSync('git', ['-C', root, 'ls-files', '-z'], {
    encoding: 'utf8',
    maxBuffer: 64 * 1024 * 1024,
    stdio: ['ignore', 'pipe', 'ignore'],
  });

  tracked = new Set(listing.split('\0').filter(Boolean));
} catch {
  tracked = null;
}

if (tracked === null) {
  console.log('[referenced-files] SKIP: sin git en el arbol — nada que comparar contra el indice.');
  process.exit(0);
}

/** Cada comprobacion remembers QUIEN nombro el fichero: un fallo sin autor es un fallo sin pista. */
const problems = [];
let checked = 0;

function requireTracked(repoRelative, namedBy) {
  checked += 1;

  const absolute = path.join(root, ...repoRelative.split('/'));
  const exists = fs.existsSync(absolute);

  if (!exists) {
    problems.push(`${repoRelative} — NO EXISTE — nombrado por ${namedBy}`);
    return;
  }

  // Un `<base href="/">` (o cualquier referencia a una carpeta) no es un fichero
  // que haya que versionar: es el sitio desde el que se resuelven los demas.
  if (!fs.statSync(absolute).isFile()) return;

  if (!tracked.has(repoRelative)) {
    problems.push(`${repoRelative} — SIN VERSIONAR — nombrado por ${namedBy}`);
  }
}

// ── 1. CMakeLists.txt ────────────────────────────────────────────────────────
// Comentarios fuera antes de tokenizar: un nombre de fichero escrito en un
// comentario es una explicacion, no una dependencia del build.
const cmake = fs
  .readFileSync(path.join(root, 'CMakeLists.txt'), 'utf8')
  .split('\n')
  .filter((line) => !/^\s*#/.test(line))
  .join('\n');

const cmakePath = /(?:Source|Tests|WebUI|Scripts|scripts|ABDSharedCode)\/[A-Za-z0-9_.\-/]*[A-Za-z0-9_.\-]\.[A-Za-z0-9]+/g;

for (const [match] of cmake.matchAll(cmakePath)) {
  if (match.includes('*')) continue;   // GLOB: salida de build o patron, no un fichero
  requireTracked(match, 'CMakeLists.txt');
}

// ── 2. playwright.config.js ──────────────────────────────────────────────────
const configPath = path.join(webuiDir, 'playwright.config.js');
const config = fs.readFileSync(configPath, 'utf8');

/** Un `src`/`href`/`from` LOCAL: ni URL absoluta, ni `data:`, ni specifier desnudo. */
const localReference = (specifier, fromFile) => {
  if (!specifier || /^(?:[a-z]+:|\/\/)/i.test(specifier) || specifier.startsWith('data:')) return null;

  // Con `<base href="/">` en la pagina, las rutas que empiezan por `/` cuelgan
  // de la RAIZ del servidor, no de la carpeta del fichero; las demas son relativas.
  if (specifier.startsWith('/')) return toRepoRelative(path.join(webuiDir, specifier));
  if (!specifier.startsWith('./') && !specifier.startsWith('../')) return null;

  const fromDir = path.dirname(path.join(root, ...fromFile.split('/')));
  return toRepoRelative(path.resolve(fromDir, specifier));
};

const testDirMatch = /testDir:\s*['"]\.\/([^'"]+)['"]/.exec(config);
const testDir = testDirMatch ? testDirMatch[1] : 'e2e';
const testDirAbs = path.join(webuiDir, testDir);

if (fs.existsSync(testDirAbs)) {
  // Todos los spec del testDir, no solo los que el config nombra: un spec anadido
  // despues corre igual (`npx playwright test` sin argumentos), asi que entra en
  // la cuenta aunque el config no lo mencione.
  for (const entry of fs.readdirSync(testDirAbs).sort()) {
    if (!entry.endsWith('.spec.js')) continue;
    requireTracked(`WebUI/${testDir}/${entry}`, 'playwright.config.js (testDir)');
  }
} else {
  problems.push(`WebUI/${testDir}/ — NO EXISTE — es el testDir de playwright.config.js`);
}

for (const [match, urlPath] of config.matchAll(/url:\s*[`'"](?:https?:\/\/[^`'"]*?)(\/[^`'"]*)[`'"]/g)) {
  const pagePath = urlPath.replace(/^\/|\/$/g, '');

  if (pagePath === '') continue;   // la raiz del servidor no es un fichero

  const page = `WebUI/${pagePath}/index.html`;

  requireTracked(page, `playwright.config.js (webServer url ${urlPath})`);

  const pageAbs = path.join(root, ...page.split('/'));

  if (!fs.existsSync(pageAbs)) continue;

  const html = fs.readFileSync(pageAbs, 'utf8');

  for (const [, specifier] of html.matchAll(/(?:src|href)=["']([^"']+)["']/g)) {
    const local = localReference(specifier, page);

    if (local) requireTracked(local, `${page} (${specifier})`);
  }

  // Y el script de la pagina: sus imports locales tambien son ficheros del repo.
  for (const [, specifier] of html.matchAll(/(?:src|href)=["']([^"']+\.js)["']/g)) {
    const local = localReference(specifier, page);

    if (!local || !fs.existsSync(path.join(root, ...local.split('/')))) continue;

    const script = fs.readFileSync(path.join(root, ...local.split('/')), 'utf8');

    for (const [, imported] of script.matchAll(/(?:from|import)\s+['"]([^'"]+)['"]/g)) {
      const fromScript = localReference(imported, local);

      if (fromScript) requireTracked(fromScript, `${local} (import ${imported})`);
    }
  }
}

// Cada `npm run X` de un `command` tiene que existir en el package.json de la WebUI:
// un script borrado rompe al ARRANCAR el servidor, con un error que no dice de
// que script se trata.
const packageJson = JSON.parse(fs.readFileSync(path.join(webuiDir, 'package.json'), 'utf8'));
const scripts = packageJson.scripts ?? {};

for (const [, script] of config.matchAll(/npm run ([A-Za-z0-9:_-]+)/g)) {
  checked += 1;

  if (!(script in scripts)) {
    problems.push(`WebUI/package.json — el script "${script}" no existe — lo invoca playwright.config.js`);
  }
}

// ── Veredicto ────────────────────────────────────────────────────────────────
if (problems.length > 0) {
  for (const problem of problems) console.error(`  [FAIL] ${problem}`);
  console.error(
    `\n[referenced-files] FAIL: ${problems.length} referencia${problems.length > 1 ? 's' : ''} ` +
    `sin resolver de ${checked} comprobadas. Un clone limpio no las tiene.`,
  );
  console.error('[referenced-files] Accion: git add de lo que falte (fixture o spec), o corrige quien lo nombra.');
  process.exit(1);
}

console.log(`[referenced-files] OK: ${checked} referencias.versionadas y en disco.`);
