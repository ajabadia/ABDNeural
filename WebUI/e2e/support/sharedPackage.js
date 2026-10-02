/**
 * El guard del PAQUETE COMPARTIDO (`@abdsynths/shared`), que la WebUI importa
 * desde el repositorio hermano ABDSharedAssets.
 *
 * POR QUE ESTE GUARD EXISTE, y por que no basta con que el build pase.
 *
 * La WebUI declara `@abdsynths/shared` como `workspace:*`, asi que no la baja de
 * ningun registro: pnpm la ENLAZA al arbol del hermano. Un enlace puede existir
 * y aun asi no servir. Hay tres formas de que la regresion visual mida menos de
 * lo que dice, y las tres se han medido en este repo:
 *
 *   1. EL HERMANO NO ESTA. Sin `ABDSharedAssets` al lado, el enlace no se crea y
 *      el build de Vite revienta al resolver el import. Esto SI falla, pero
 *      veinte pasos mas tarde y con un error de Vite que no nombra al workspace.
 *   2. EL HERMANO ESTA PERO LE FALTA ALGO. Aqui es donde el guard se gana: el
 *      arbol puede tener todo `styles/` y `components/` y aun asi no tener un
 *      fichero que la WebUI importa. Medido el 2026-09-29:
 *      `components/envelopeCurve.js`, `components/envelopeGestures.js`,
 *      `components/envelopePad.js` y `styles/components/envelope.css` existen en
 *      el disco de ABDSharedAssets y NO estan en NINGUN commit. Un clon limpio
 *      -que es lo que hace el runner de CI- se queda sin ellos, y el arbol
 *      compartido queda incompleto sin que nada lo diga hasta que falla el build.
 *   3. EL ENLACE APUNTA A OTRO SITIO. Un `node_modules` de una instalacion
 *      anterior puede dejar un `@abdsynths/shared` que resuelve a OTRO arbol de
 *      ABDSharedAssets, con otra pintura. Las referencias se compararian contra
 *      un mueble que no es el de este commit, y el fallo que saldria seria
 *      "cambia la pintura" en vez de "estas mirando el paquete equivocado".
 *
 * QUE COMPRUEBA, y por que cada parte esta:
 *
 *   - QUE EL ENLACE RESUELVE. `createRequire` desde `WebUI/`, que es donde vive
 *     el `node_modules` de verdad. Un enlace roto o ausente falla aqui.
 *   - QUE LOS FICHEROS QUE LA WEBUI IMPORTA ESTAN. La lista es la de los
 *     `import ... from '@abdsynths/shared/...'` REALES de `src/`, no una lista
 *     inventada. Para que no se quede vieja sola, `avisosDeCobertura` compara la
 *     lista con lo que hay en `src/` y AVISA de cualquier ruta que la WebUI
 *     importе y la lista no cubra.
 *   - QUE EL BARREL EXPORTA LO QUE LA WEBUI USA. Un barrel que pierde una
 *     exportacion rompe el build, pero uno que la pierde y la WebUI todavia no
 *     usa deja la suite en verde sin avisar: aqui se importa de verdad.
 *
 * QUE NO HACE, y por que: no juzga si el contenido es "correcto". Eso lo miden las
 * referencias; este guard solo responde a si hay algo que comparar. Un arbol
 * equivocado que se resuelve NO se distingue por estructura, solo por pintura, y
 * para eso estan las capturas.
 */

import { createRequire } from 'node:module';
import { readFileSync, readdirSync, statSync } from 'node:fs';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { dirname, join, resolve } from 'node:path';

const PKG = '@abdsynths/shared';

/**
 * Lo que la WebUI importa del paquete, RUTA POR RUTA.
 *
 * Sale de `grep -rho "@abdsynths/shared/[^'\" ]*" WebUI/src`, y son rutas de
 * FICHERO, no de modulo: `styles/tokens.css` lo importa Vite como CSS plano y
 * `require.resolve` lo resuelve igual, que es justo lo que se quiere comprobar.
 * `@abdsynths/shared/components` a secas NO va aqui a proposito: es el barrel y
 * lo comprueba `exportacionesDelBarrel`, resolviendo su `index.js`.
 */
const RUTAS = [
  'styles/tokens.css',
  'styles/components/widgets.css',
  'styles/components/backgrounds.css',
  'styles/components/fx.css',
  'styles/components/envelope.css',
  'contracts/s950_patch_fields.json',
];

/**
 * Lo que la WebUI le pide al BARREL. Sale de los `import { ... } from
 * '@abdsynths/shared/components'` de `src/`, y son los nombres que existen HOY.
 *
 * Un nombre que el barrel no exporta rompe el build de Vite, asi que este
 * contrato solo puede fallar si el hermano esta a medias; se comprueba igual
 * porque el mensaje es mucho mejor que el de Vite, y porque asi el fallo se ve
 * en el guard y no veinte pasos mas tarde.
 */
const EXPORTACIONES = [
  'Knob',
  'NumberBox',
  'S950_ENCODINGS',
  'S950_GROUPS',
  'Segmented',
  'Select',
  'Slider',
  'ThemeSwitcher',
  'Toggle',
  'WAVEFORM_GLYPHS',
  'WAVEFORM_NAMES',
  'XYPad',
  'buildS950Catalogue',
  'createDrawer',
  'createEnvelopeCurve',
  'createLcdPanel',
  'formatS950Name',
  'isS950Bipolar',
  'mountFitStage',
];

/** La carpeta `WebUI/`, deducida de laubicacion de este propio fichero. */
export const RAIZ_WEBUI = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..');

/**
 * El resultado del guard, o el motivo por el que no se pudo hacer.
 *
 * @typedef {object} GuardCompartido
 * @property {boolean}  ok        si el paquete sirve para lo que la WebUI importa.
 * @property {string}   [motivo]  por que no, en una frase accionable.
 * @property {string}   [raiz]    donde se resolvio el paquete.
 * @property {string[]} [avisos]  cosas que no rompen pero conviene decir.
 * @property {string[]} [rutas]   las rutas comprobadas, para el fallo las liste.
 */

/**
 * Las rutas que la WebUI importa del paquete AHORA MISMO, leidas de `src/`.
 *
 * Es la red de seguridad de `RUTAS`: si alguien anade un
 * `import '@abdsynths/shared/nuevo.css'` y no actualiza la lista, esta funcion
 * lo ve y el guard lo dice. Sin esto la lista se quedaria vieja en silencio,
 * que es justo el fallo que este guard viene a evitar.
 *
 * @returns {string[]} rutas relativas al paquete, sin el prefijo.
 */
export function rutasQueImportaLaWebUI(raizWebUI = RAIZ_WEBUI) {
  const encontradas = new Set();
  const prefijo = `${PKG}/`;

  const visitar = (dir) => {
    let entradas;
    try {
      entradas = readdirSync(dir, { withFileTypes: true });
    } catch {
      return;
    }
    for (const entrada of entradas) {
      const ruta = join(dir, entrada.name);
      if (entrada.isDirectory()) {
        visitar(ruta);
        continue;
      }
      if (!entrada.name.endsWith('.js')) continue;

      let texto;
      try {
        texto = readFileSync(ruta, 'utf8');
      } catch {
        continue;
      }
      // Se acota al especificador: lo que siga tras el es la comilla de cierre o
      // el punto y coma del import, y ninguno de los dos entra en la lista.
      const patron = new RegExp(prefijo + "([^'\"`\\s]+)", "g");
      for (let m = patron.exec(texto); m !== null; m = patron.exec(texto)) {
        const especificador = m[1].replace(/[;,)]+$/, "");
        if (especificador) encontradas.add(especificador);
      }
    }
  };

  visitar(join(raizWebUI, 'src'));
  return [...encontradas].sort();
}

/**
 * Comprueba que `@abdsynths/shared` esta y sirve para lo que la WebUI importa.
 *
 * NO lanza: devuelve el motivo. Quien llama decide que hacer con el, porque un
 * test que dice "falta el hermano, esto no se puede medir" es un fallo con
 * nombre, y un `throw` en un import es un fallo sin.
 *
 * @param {string} [raizWebUI]  la carpeta `WebUI/`, donde esta el `node_modules`.
 * @returns {Promise<GuardCompartido>}
 */
export async function compruebaPaqueteCompartido(raizWebUI = RAIZ_WEBUI) {
  const avisos = [];

  let requireDesde;
  try {
    requireDesde = createRequire(join(raizWebUI, 'package.json'));
  } catch (error) {
    return {
      ok: false,
      motivo:
        `no se puede crear un require desde '${raizWebUI}': ${error.message}. `
        + 'El guard necesita la raiz de la WebUI para encontrar su node_modules.',
    };
  }

  // 1. EL ENLACE RESUELVE.
  let raiz;
  try {
    raiz = dirname(requireDesde.resolve(`${PKG}/components`));
  } catch (error) {
    return {
      ok: false,
      motivo:
        `el paquete '${PKG}' no resuelve desde WebUI/ (${error.code ?? error.message}). `
        + 'Suele ser el hermano ABDSharedAssets sin clonar al lado, o un `pnpm install` '
        + 'que no ha enlazado los `workspace:*`. El bootstrap de CI lo comprueba en el '
        + 'paso "Verify the workspace layout"; en local, `pnpm install` desde WebUI/.',
    };
  }

  // 2. LOS FICHEROS QUE LA WEBUI IMPORTA ESTAN.
  const rutas = rutasQueImportaLaWebUI(raizWebUI).filter((r) => r !== 'components');
  const sinCubrir = rutas.filter((r) => !RUTAS.includes(r));
  if (sinCubrir.length > 0) {
    avisos.push(
      `la WebUI importa ${sinCubrir.length} ruta(s) del paquete que la lista del guard `
      + `no cubre: ${sinCubrir.join(', ')}. Anadelas a RUTAS en e2e/support/sharedPackage.js, `
      + 'o el guard dejara de comprobarlas.',
    );
  }

  const comprobadas = [...new Set([...RUTAS, ...rutas])];
  const ausentes = [];
  for (const ruta of comprobadas) {
    try {
      requireDesde.resolve(`${PKG}/${ruta}`);
    } catch {
      ausentes.push(ruta);
    }
  }
  if (ausentes.length > 0) {
    return {
      ok: false,
      raiz,
      avisos,
      rutas: comprobadas,
      motivo:
        `el paquete '${PKG}' esta enlazado (${raiz}) pero le faltan `
        + `${ausentes.length} fichero(s) que la WebUI importa: ${ausentes.join(', ')}. `
        + 'El arbol del hermano esta incompleto: esos ficheros estan en el disco de '
        + 'ABDSharedAssets pero en ningun commit, asi que un clon limpio (el del runner '
        + 'de CI) no los tiene. Sube el commit que los anade al hermano.',
    };
  }

  // 3. EL BARREL EXPORTA LO QUE LA WEBUI USA.
  const barrel = `${PKG}/components`;
  let modulo;
  try {
    modulo = await import(pathToFileURL(requireDesde.resolve(barrel)).href);
  } catch (error) {
    return {
      ok: false,
      raiz,
      avisos,
      rutas: comprobadas,
      motivo:
        `el barrel '${barrel}' esta ahi pero no se puede importar (${error.message}). `
        + 'Suele ser que le falta un modulo de los que el reexporta.',
    };
  }

  const sinExportar = EXPORTACIONES.filter((nombre) => !(nombre in modulo));
  if (sinExportar.length > 0) {
    return {
      ok: false,
      raiz,
      avisos,
      rutas: comprobadas,
      motivo:
        `el barrel '${barrel}' no exporta ${sinExportar.length} simbolo(s) que la WebUI `
        + `importa: ${sinExportar.join(', ')}. La pagina se quedaria sin ese mueble y las `
        + 'referencias compararian contra un lienzo incompleto.',
    };
  }

  return { ok: true, raiz, avisos, rutas: comprobadas };
}

/**
 * El texto para un `expect` de este guard, con el motivo develops en una linea
 * que quepa en el fallo de un test.
 *
 * @param {GuardCompartido} guard
 * @returns {string}
 */
export function mensajeDelGuard(guard) {
  if (guard.ok) return `el paquete compartido esta completo (${guard.raiz}).`;
  return `EL PAQUETE COMPARTIDO NO SIRVE, ASI QUE LA REGRESION VISUAL NO MIDE LO QUE DICE: ${guard.motivo}`;
}

// `statSync` se usa al final, para decir si la raiz es un enlace y a donde apunta.
export function describeRaiz(raiz) {
  try {
    const st = statSync(raiz);
    return st.isDirectory() ? raiz : `${raiz} (no es un directorio)`;
  } catch (error) {
    return `${raiz} (ilegible: ${error.message})`;
  }
}
