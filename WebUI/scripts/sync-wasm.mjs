/**
 * Copy the WASM DSP artifacts from build-wasm/ into WebUI/public/worklet/.
 *
 * Run this after `build_wasm.bat` whenever the DSP changes — the AudioWorklet
 * path serves the binary from the static export (public/ -> dist/), so a stale
 * copy here would silently ship an outdated DSP to the browser.
 *
 * Vivia en el piloto (`WebPilotVite/scripts/sync-wasm.mjs`) y se mudo a la WebUI
 * con su `public/` en la retirada del piloto (ticket 8.4): lo que sincroniza es el
 * worklet que sirve ESTA pagina, y `publicDir` de la WebUI es este `public/`.
 *
 * Usage: node WebUI/scripts/sync-wasm.mjs              (from ABDNeural/, or any cwd)
 *        node WebUI/scripts/sync-wasm.mjs --check      # NO writes. Exits 1 if stale.
 *        pnpm sync:wasm                                (from WebUI/)
 *
 * WHY `--check` IS NOT OPTIONAL. This copies the compiled DSP OVER the copy the
 * AudioWorklet serves. The failure it prevents is not "the file changed", it is
 * "the file was REVERTED": running it after an unrelated `git checkout`, or
 * pointing it at an older `build-wasm/`, replaces a working binary with a stale one
 * and the browser serves it silently — the whole point of the module comment
 * above. Without `--check` the only way to find out is to notice that the sound
 * changed, which is not a debugging method.
 */

import { existsSync, mkdirSync, statSync, readFileSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const thisDir = path.dirname (fileURLToPath (import.meta.url));
const neuralRoot = path.resolve (thisDir, '..', '..');

const sourceDir = path.join (neuralRoot, 'build-wasm');
const targetDir = path.join (neuralRoot, 'WebUI', 'public', 'worklet');

const ARTIFACTS = ['neuronik_dsp.js', 'neuronik_dsp.wasm'];

// Que artefactos son TEXTO y cuales binarios, porque no se comparan igual.
//
// El `.wasm` se compara byte a byte: un binario que cambia un byte es otro binario.
//
// El `.js` se compara con los finales de linea normalizados, y no por capricho.
// `build-wasm/` NO esta versionado — es salida de Emscripten — asi que en Windows
// llega con CRLF. El destino, en cambio, lleva `eol: lf` en `.gitattributes`.
// Sin normalizar, el check declara "stale" un DSP que no ha cambiado por dos bytes de
// salto de linea, y en rojo para siempre: lo unico que se puede hacer para callarlo
// es copiar, y copiar mete CRLF en un fichero que el repo exige en LF. Se arregla el
// sintoma, se ensucia el arbol, y el aviso del comentario de cabecera ("a stale copy
// here would silently ship an outdated DSP") deja de ser creible porque el unico rojo
// que produce no es ninguno de los dos que ese aviso describe.
export const ES_TEXTO = new Set (['neuronik_dsp.js']);

// Exportadas para que el test pueda fijarlas sin montar un arbol de DSP entero: son
// la parte del sincronizador que decide que dos ficheros son "el mismo".
//
// `binary` (latin1) en vez de `utf8` en el viaje de ida y vuelta: es la unica
// codificacion de Node que deja pasar los 256 valores de byte sin cambiar ninguno,
// asi que normalizar finales de linea no puede alterar el resto del contenido.
const bytes = (file) => readFileSync (file);
export const contenido = (file, nombre) => {
  const buf = bytes (file);
  return ES_TEXTO.has (nombre)
    ? Buffer.from (buf.toString ('binary').replace (/\r\n/g, '\n'), 'binary')
    : buf;
};
export const huella = (file, nombre) => createHash ('sha256').update (contenido (file, nombre)).digest ('hex');

export function sincronizar ({ check, sourceDir: origen, targetDir: destino, root = origen }) {
  let failed = false;
  const stale = [];

  for (const name of ARTIFACTS) {
    const source = path.join (origen, name);
    const target = path.join (destino, name);

    if (!existsSync (source)) {
      console.error (`[sync-wasm] falta ${name} en ${origen} — ejecuta build_wasm.bat primero`);
      failed = true;
      continue;
    }

    const alDia = existsSync (target) && huella (source, name) === huella (target, name);

    if (check) {
      if (!alDia) { stale.push (name); }
      continue;
    }

    mkdirSync (destino, { recursive: true });
    // Se copia el CONTENIDO ya normalizado, no el fichero tal cual. Escribir los bytes
    // de `build-wasm/` tal cual meteria CRLF en un destino que `.gitattributes` declara
    // LF; el contenido indexado seria identico, pero el working tree quedaria con el
    // fichero marcado como modificado hasta que git lo reescribiera — ruido que parece
    // un cambio del DSP y no lo es.
    if (!alDia) { writeFileSync (target, contenido (source, name)); }
    // El tamaño que se anuncia es el de lo que ha QUEDADO en el destino, no el del
    // origen: si difieren (el CRLF que se acaba de normalizar) y se anuncia el del
    // origen, el log dice que se han escrito 14454 bytes de un fichero de 14452, e
    // invita a depurar una diferencia que ya no existe.
    // La ruta se muestra relativa a la RAIZ del repo, no al origen: `build-wasm/
    // neuronik_dsp.js` no dice nada, `WebUI/public/worklet/neuronik_dsp.js` si.
    console.log (`[sync-wasm] ${name} -> ${path.relative (root, target).replace (/\\/g, '/')} (${statSync (target).size} bytes${alDia ? ', ya estaba al dia' : ''})`);
  }

  return { stale, failed };
}

// Todo lo de arriba es importable; esto es lo que solo pasa cuando el fichero se
// EJECUTA. Sin esta puerta, un `import` del test se encontraria con un
// `process.exit` a media importacion y se llevaria por delante el resto del proceso.
function ejecutar () {
  const args = process.argv.slice (2);
  const check = args.includes ('--check');
  const help = args.includes ('--help') || args.includes ('-h');
  const desconocido = args.find ((a) => a.startsWith ('-') && a !== '--check' && a !== '--help' && a !== '-h');

  if (help) {
    console.log ([
      'Copia los artefactos del DSP WASM de build-wasm/ a WebUI/public/worklet/.',
      '',
      'Uso: node WebUI/scripts/sync-wasm.mjs [--check] [--help]',
      '',
      '  (sin flag)  Copia los dos artefactos encima.',
      '  --check     NO escribe nada. Sale 1 si alguno esta stale.',
    ].join ('\n'));
    process.exit (0);
  }

  if (desconocido) {
    console.error (`[sync-wasm] argumento desconocido: ${desconocido}`);
    process.exit (2);
  }

  const { stale, failed } = sincronizar ({ check, sourceDir, targetDir, root: neuralRoot });

  if (check) {
    console.log (`[sync-wasm] ${ARTIFACTS.length} artefacto(s); stale: ${stale.length}`);
    for (const name of stale) { console.log (`  ~ ${name}`); }
    if (stale.length > 0) {
      console.error ('[sync-wasm] DESACTUALIZADO — ejecuta el sincronizador sin --check.');
      process.exit (1);
    }
    console.log ('[sync-wasm] OK — nada que sincronizar.');
  }

  process.exit (failed ? 1 : 0);
}

if (process.argv[1] && import.meta.url === pathToFileURL (process.argv[1]).href) { ejecutar (); }