/**
 * LOS IDS DE LA PAGINA CONTRA EL CONTRATO GENERADO, en un solo sitio.
 * ==============================================================================
 * Cada superficie de la pagina nombra ids de parametros a mano --la ficha, el
 * menu del LCD, las curvas de envolvente, el pad de morphs, el espejo del
 * hilo de audio-- y cada una lleva su cuenta. Lo que NO habia era una cuenta
 * que las mirara a todas, y por eso un id retirado se puede quedar en una
 * superficie sin que nadie se entere: el store IGNORA un id que no posee, la
 * llamada se come sin error y el control deja de hacer lo que dice hacer. Pasó
 * con el SATURATION del menu del LCD, que apuntaba a `fxSaturation` desde que
 * la migracion del hueco 1 retiro ese mando, y no se vio hasta que alguien leyo
 * la tabla de ids.
 *
 * POR QUE UN TEST Y NO UNA CONVENCION: una convencion la recuerda quien escribe
 * el id nuevo, y aqui los ids los escribe gente distinta en ficheros distintos.
 * Un test corre en cada cambio y no depende de que nadie se acuerde.
 *
 * LA CUENTA TIENE DOS MITADES, y la segunda es la que no se puede hacer con un
 * import: ademas de las colecciones EXPORTADAS (que se importan y se comprueban
 * una a una), este test LEE LAS FUENTES y mira cada literal que esta en una
 * posicion de id. Sin esa mitad se escaparian los ids de lo que no se exporta
 * --`MORPH_IDS`, `LAYER_IDS`, `DISCRETE_FIELD_IDS`-- y los que van en linea
 * dentro de un objeto, que es justo como estaba el `fxSaturation`.
 *
 * Y TIENE UNA TERCERA COSA, que es la que hace que no se rompa en silencio: el
 * escaner tiene que ENCONTRAR algo. Un test que pasa porque su patron dejo de
 * casar es peor que no tener test, porque sale verde mientras no comprueba nada.
 */

import { readdirSync, readFileSync, statSync } from 'node:fs';
import { dirname, join, relative, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';

import { PARAMETERS, getDescriptor, PILOT_PARAMETER_IDS } from '../src/contracts/parameters.js';
import {
  GENERAL_PARAMETER_IDS,
  VISUAL_PARAMETER_IDS,
  SLOT_MODULE_PARAMETER_IDS,
  SCREEN_PARAMETER_IDS,
} from '../src/contracts/screens.js';
import { SECTION_PARAMETER_IDS } from '../src/contracts/sections.js';
import { MATRIX_ROUTE_IDS } from '../src/ui/envelopeViews.js';
import { buildMenuTree } from '../src/ui/lcdTop.js';
import { CONTRACT_TO_GP_FIELD } from '../src/wasm/audioParams.js';

const aqui = dirname(fileURLToPath(import.meta.url));
// La raiz de la pagina, para que el fallo nombre `src/ui/lcdTop.js:110` y no
// `../src/ui/lcdTop.js:110`: es como lo escribe uno cuando va a arreglarlo.
const RAIZ = join(aqui, '..');
const SRC = join(RAIZ, 'src');

/**
 * Ids que se escriben en una posicion de id y NO son parametros del contrato.
 *
 * VACIA a proposito, y con una sola entrada, porque cada elemento es una
 * exception que alguien tiene que justificar: `RESET_MIDI` es el RESET ALL del
 * menu del LCD, que es una ACCION del D-pad y no un mando del APVTS. Si anadir
 * otro aqui, el motivo va en la linea de al lado.
 */
const NO_ES_UN_PARAMETRO = new Map([
  ['RESET_MIDI', 'accion del D-pad (RESET ALL), no un mando del APVTS'],
]);

// ── El escaner ────────────────────────────────────────────────────────────────

/**
 * Posiciones del codigo donde un literal ES un id de parametro.
 *
 * Se listan a mano y a proposito: un patron que cogiera "cualquier literal con
 * forma de camelCase" traeria encima cada clase de CSS, cada etiqueta de DOM y
 * cada palabra de un texto de la pagina. Estas son las que de verdad llevan un
 * id, que es donde se puede colar uno retirado.
 */
const RANURAS = [
  // Un control suelto: { label: 'SATURATION', paramId: 'fxSaturation' }.
  { patron: /paramId:\s*'([^']+)'/g, que: 'paramId', varios: false },
  // El descriptor del cajon de efectos y el resto de descriptores.
  { patron: /parameterId:\s*'([^']+)'/g, que: 'parameterId', varios: false },
  // Las colecciones que se declaran en linea: ids: [...], *_IDS = [...],
  // *_IDS = new Set([...]) y el objeto MORPH_IDS = { x: 'morphX', ... }.
  { patron: /ids:\s*\[([^\]]*)\]/g, que: 'ids', varios: true },
  { patron: /_IDS\s*=\s*\[([^\]]*)\]/g, que: 'coleccion', varios: true },
  { patron: /_IDS\s*=\s*new Set\(\s*\[([^\]]*)\]/g, que: 'coleccion', varios: true },
  { patron: /_IDS\s*=\s*\{([^}]*)\}/g, que: 'coleccion-objeto', varios: true },
];

function fuentesDe (dir, dentro = []) {
  for (const nombre of readdirSync(dir)) {
    const ruta = join(dir, nombre);
    if (statSync(ruta).isDirectory()) fuentesDe(ruta, dentro);
    else if (nombre.endsWith('.js')) dentro.push(ruta);
  }
  return dentro;
}

function escaneaLasFuentes () {
  const hallazgos = [];

  for (const ruta of fuentesDe(SRC)) {
    const texto = readFileSync(ruta, 'utf8');
    const lineas = texto.split('\n');

    for (const { patron, que, varios } of RANURAS) {
      patron.lastIndex = 0;
      let coincidencia;
      while ((coincidencia = patron.exec(texto)) !== null) {
        const linea = texto.slice(0, coincidencia.index).split('\n').length;
        // El grupo de un patron de uno ES el id; el de uno de lista son las
        // comillas de dentro. Buscar comillas dentro del primer caso no
        // encuentra nada, que es como el escaner se pasaba en verde sin
        // haber mirado un solo `paramId`.
        const grupo = coincidencia[1];
        const crudos = varios ? (grupo.match(/'([^']+)'/g) || []) : [grupo];

        for (const crudo of crudos) {
          hallazgos.push({
            id: varios ? crudo.slice(1, -1) : crudo,
            donde: `${relative(RAIZ, ruta).split(sep).join('/')}:${linea}`,
            ranura: que,
          });
        }
      }
    }
  }

  // Un id repetido no es un hallazgo repetido: para el fallo que buscamos --
  // "este id no existe" -- cuenta una vez, y la linea que se nombra es la
  // primera, que es donde se corrige.
  const porId = new Map();
  for (const h of hallazgos) if (!porId.has(h.id)) porId.set(h.id, h);
  return { hallazgos, porId };
}

const { porId } = escaneaLasFuentes();

describe('los ids de la pagina contra el contrato generado', () => {
  it('NINGUN literal en posicion de id apunta a un id retirado', () => {
    // La cuenta que sustituye a la del SATURATION del LCD, pero para TODAS las
    // superficies y no solo para una. El fallo sale con fichero y linea, que es
    // donde el id se escribe y donde se arregla.
    const desconocidos = [...porId.values()]
      .filter(({ id }) => getDescriptor(id) === null && !NO_ES_UN_PARAMETRO.has(id))
      .map(({ id, donde, ranura }) => `${id} (${donde}, ranura "${ranura}")`);

    // VACIA, y el propio test lo dice: hasta el 2026-09-29 era
    // "fxSaturation (src/ui/lcdTop.js:95, ranura paramId)", el id que la
    // migracion del rack retiro y que el store se comia sin error.
    expect(desconocidos).toEqual([]);
  });

  it('el escaner ENCUENTRA ids, y no se ha quedado sin mirar', () => {
    // Un test que pasa porque su regex dejo de casar sale verde mientras no
    // comprueba nada, que es peor que no tenerlo. Con 79 ids distintos hoy, un
    // numero de tres digitos deja margen de sobra para un descuido de un solo
    // caracter en el patron.
    expect(porId.size).toBeGreaterThan(60);
  });

  it('cada exception tiene su motivo escrito', () => {
    for (const [id, motivo] of NO_ES_UN_PARAMETRO) {
      expect(typeof motivo, `${id} necesita su motivo`).toBe('string');
      expect(motivo.length, `${id} necesita un motivo de verdad`).toBeGreaterThan(10);
    }
  });

  it('lo que el escaner cubre incluye las colecciones EXPORTADAS', () => {
    // La mitad del escaner es la que se apoya en el patron, y un patron puede
    // dejar de casar. Las colecciones exportadas se comprueban ademas una a
    // una, por import, para que el cambio de sintaxis de una no las saque de la
    // cuenta en silencio.
    const exportadas = new Map([
      ['PILOT_PARAMETER_IDS', PILOT_PARAMETER_IDS],
      ['GENERAL_PARAMETER_IDS', GENERAL_PARAMETER_IDS],
      ['VISUAL_PARAMETER_IDS', VISUAL_PARAMETER_IDS],
      ['SLOT_MODULE_PARAMETER_IDS', SLOT_MODULE_PARAMETER_IDS],
      ['SCREEN_PARAMETER_IDS', SCREEN_PARAMETER_IDS],
      ['SECTION_PARAMETER_IDS', SECTION_PARAMETER_IDS],
      ['MATRIX_ROUTE_IDS', MATRIX_ROUTE_IDS],
    ]);

    for (const [nombre, ids] of exportadas) {
      for (const id of ids) {
        expect(getDescriptor(id), `${nombre} trae "${id}", que no esta en el contrato`)
          .not.toBeNull();
        // Y que el escaner tambien lo ha visto: si una coleccion se declara
        // con una sintaxis que ningun patron reconoce, sale aqui y no antes.
        expect(porId.has(id), `${nombre}: "${id}" no lo encuentra el escaner de fuentes`)
          .toBe(true);
      }
    }
  });

  it('el menu del LCD y el espejo del hilo de audio tambien', () => {
    // Las dos superficies que ya tenían su cuenta propia y que aqui se
    // comprueban desde el mismo sitio, para que la cuenta sea UNA.
    const items = (nodo, dentro = []) => {
      for (const item of nodo) {
        if (item.sub) items(item.sub, dentro);
        else if (item.type !== 'action') dentro.push(item.paramId);
      }
      return dentro;
    };

    for (const engineType of [0, 1]) {
      for (const id of items(buildMenuTree(engineType))) {
        expect(getDescriptor(id), `el menu del LCD (motor ${engineType}) trae "${id}"`)
          .not.toBeNull();
      }
    }

    for (const contractId of Object.keys(CONTRACT_TO_GP_FIELD)) {
      expect(getDescriptor(contractId), `CONTRACT_TO_GP_FIELD trae "${contractId}"`)
        .not.toBeNull();
    }
  });

  it('el contrato no se ha quedado sin parametros que la pagina pueda usar', () => {
    // La comprobacion de que ningun id esta retirado es tan fuerte como la
    // lista de ids que la pagina tiene: si el contrato se vaciara por un
    // accidento, "ninguno desconocido" seria verde por falta de sujeto. Este
    // test dice cuantos hay.
    expect(PARAMETERS.length).toBeGreaterThan(60);
  });
});
