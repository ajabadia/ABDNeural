/**
 * El editor de patches del S950: lee un programa real de un disco y lo pinta
 * con los nombres y los rangos que declara el contrato.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * QUE JUNTA ESTE FICHERO.
 *
 * Tres cosas que viven en sitios distintos y que se necesitan juntas:
 *
 *   - `s950Disk.js` LEE. Del `.img` sale un programa, sus keygroups y sus bytes.
 *   - `s950_patch_fields.json` NOMBRA. De 38 campos trae el nombre que ve la
 *     persona, el rango del panel y la codificacion del byte.
 *   - `s950_calibration.json` MIDE. Dice en que unidad esta cada curva, y si
 *     alguien la ha medido.
 *
 * Un editor sin las tres se equivoca de tres maneras distintas, y ninguna se
 * ve en la pantalla: pone un 32 donde hay un espacio, enseña un 0..99 donde la
 * maquina tiene un 0..255, y escribe "0.000 s" donde no hay ni un milisegundo
 * medido.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * LO QUE ESTE EDITOR NO HACE, Y POR QUE.
 *
 * NO ESCRIBE. Un editor que escribe un programa es un importador, y un
 * importador que se equivoca al escribir destruye un patch que no se puede
 * recuperar. Este solo lee y pinta, y por eso es seguro abrir cualquier disco.
 *
 * NO DIBUJA EL VALOR EN UNIDADES MEDIDAS CUANDO NO HAY MEDICION. Y esto es lo
 * unico de aqui que merece explicarse.
 *
 * La calibracion del S950 esta VACIA a proposito: nadie ha medido esas curvas, y
 * la tabla que si las tiene es AGPL. Un editor que al abrir un patch escribiera
 * "Attack: 0.0021 s" estaria inventando el numero mas preciso que puede
 * mostrar, y lo haria con la tipografia de una medida. Quien lo lee no tiene
 * forma de saber que es una conjetura: un panel no lleva una marca de "esto no
 * lo se".
 *
 * Asi que el editor dice LAS DOS COSAS y deja claro cual es cual: el mando se
 * pinta con su nombre y su rango del panel —que si son datos—, y la unidad de
 * abajo sale como "sin medir", no como un numero. Un ataque a 55 es un 55
 * inequivoco: es lo que hay en el byte y lo que la maquina ensena. Lo que no se
 * sabe es cuantos milisegundos son 55, y eso no se inventa.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * LA DEGRADACION, IGUAL QUE EN EL RESTO DEL TRABAJO.
 *
 * Un disco danado no lanza: devuelve `null` y un motivo. Un keygroup ilegible no
 * devuelve una lista a medias. Y un campo sin valor devuelve `null`, que el
 * panel pinta como desconocido y no como 0 —porque un 0 en un tiempo es un
 * click, y un click en un editor es un patch que suena mal sin motivo aparente.
 *
 * @example
 *   import { abrirPrograma, editarPrograma } from '../ui/s950PatchEditor.js';
 *
 *   const editor = abrirPrograma(bytes, 0);
 *   if (editor.ok) {
 *     for (const campo of editor.grupos[0].campos)
 *       console.log(campo.label, campo.value, campo.axis?.estado);
 *   } else {
 *     console.warn(editor.motivo);   // por que no se pudo abrir
 *   }
 */

import {
  S950_PATCH_CATALOGUE,
  S950_GROUPS,
  formatS950Name,
  isS950Bipolar,
} from '../contracts/s950PatchFields.js';
import { S950_CALIBRATION, s950ValueAt } from '../contracts/s950Calibration.js';
import { keygroupCount, readKeygroup, readProgram } from '../contracts/s950Disk.js';

//==============================================================================
/** A que curva de calibracion responde cada campo.

    Y el mapa NO ES UN ADORNANTE: sin el, un attack y un transpose se pintarian
    igual, y el panel acabaria mostrando "0.0034 s" al lado de "3 semitonos" con
    la misma tipografia, que es exactamente la confusion que hace que una cifra
    inventada parezca una medida.

    Sale de la DESCRIPCION de cada curva, que esta en el contrato: `envelopeTime`
    es "Attack / Decay / Release", `filterCutoff` es "Soft / Loud filter", y asi
    con las seis. Un campo que no esta en el mapa no tiene eje, y el panel lo
    pinta sin eje en vez de buscarle uno parecido.

    Los `sustain` van a `sustainDb` y NO a `envelopeTime`, y esa distincion no es
    cosmetica: son curvas distintas, con unidades distintas —una en segundos y
    otra en decibelios— y con la regla distinta sobre el cero, porque 0 dB es
    un nivel legitimo y un tiempo de cero es un instante. */
const CAMPO_A_CURVA = {
  vcaAttack: 'envelopeTime',
  vcaDecay: 'envelopeTime',
  vcaRelease: 'envelopeTime',
  vcfAttack: 'envelopeTime',
  vcfDecay: 'envelopeTime',
  vcfRelease: 'envelopeTime',
  lfoDelay: 'envelopeTime',
  vcaSustain: 'sustainDb',
  vcfSustain: 'sustainDb',
  lfoRate: 'lfoRate',
  warpTime: 'warpTime',
  softFilter: 'filterCutoff',
  loudFilter: 'filterCutoff',
  vcfAmount: 'filterEnvOctaves',
};

//==============================================================================
/** El eje de un campo, con su estado honesto.

    @param {string} code el `code` del campo
    @returns {{curva: string, unidad: string, estado: 'medido'|'sinMedir',
               valor: number|null, logarithmic: boolean, min: number,
               max: number}|null} `null` si el campo no tiene curva.
*/
export function ejeDeCampo (code) {
  const curva = CAMPO_A_CURVA[code];
  if (!curva || !S950_CALIBRATION) return null;

  const c = S950_CALIBRATION.curves.find((x) => x.code === curva);
  if (!c) return null;

  // `sinMedir` es el estado que la tabla tiene HOY y el unico que la tabla
  // puede tener mientras nadie mida. Se decide aqui, en el editor, y no se deja
  // que cada control lo decida por su cuenta: si uno se olvidara, ese uno
  // inventaria un numero y los demas no, que es la forma mas dificil de
  // quitar despues porque parece un fallo de un solo sitio.
  const measured = c.measured && c.points.length >= 2;

  return {
    curva,
    unidad: c.unit,
    estado: measured ? 'medido' : 'sinMedir',
    logarithmic: c.logarithmic,
    min: c.storedLo,
    max: c.storedHi,
  };
}

//==============================================================================
/** El valor medido de un campo, o `null` si no hay tabla.

    Y aqui esta la parte que no se negocia: si la curva no esta medida, esto
    devuelve `null`. No devuelve el valor del panel, no devuelve un cero, y no
    devuelve una interpolacion del rango. Un numero sin medir no es una
    aproximacion: es una cifra con la autoridad de una medida.
*/
export function valorMedidoDeCampo (code, stored) {
  const eje = ejeDeCampo (code);
  if (!eje || eje.estado !== 'medido') return null;

  return s950ValueAt (eje.curva, stored);
}

//==============================================================================
/** Un campo, listo para pintar. */
function campoAPintar (field, value) {
  const eje = ejeDeCampo (field.code);

  return {
    code: field.code,
    label: formatS950Name (field.name),
    // El nombre CRUDO se conserva para el tooltip: el del panel es para la
    // pantalla, y el de la maquina es para quien tiene el manual delante.
    rawName: field.name,
    group: field.group,
    lo: field.lo,
    hi: field.hi,
    unit: field.unit,
    bipolar: isS950Bipolar (field),
    byteOffset: field.byteOffset,
    value,

    // El eje con su estado. `null` cuando el campo no responde a ninguna curva,
    // y eso tambien es informacion: un transpose no tiene eje vertical porque
    // no es una magnitud que se pueda escalar, y fingir que si lo tiene es como
    // se inventan los mins que no existen.
    axis: eje,

    // El valor en la unidad de la curva, o `null`. Separado del `value` a
    // proposito: el `value` SIEMPRE existe —es el byte— y el `measured` puede no
    // existir nunca. Juntarlos en un campo obliga a decidir en el consumidor,
    // y ese consumidor es un boton que no sabe de calibracion.
    measured: value === null ? null : valorMedidoDeCampo (field.code, value),
  };
}

/** Un grupo de la pagina, con sus campos y si se puede pintar entero. */
function grupoAPintar (nombreGrupo, campos) {
  return {
    name: nombreGrupo,
    campos,
    // Un grupo con un campo sin valor no se pinta entero, y se dice por que.
    // Un control que nace sin su rango se ve saltar un frame, y uno que nace sin
    // su valor muestra un 0 que nadie ha medido.
    completo: campos.every((c) => c.value !== null),
  };
}

//==============================================================================
/** Edita UN keygroup de un programa abierto. */
export function editarKeygroup (programa, indice = 0) {
  if (!programa) return null;

  const bytes = programa.bytes;
  const leidos = readKeygroup (bytes, programa.entry, indice);

  if (leidos === null) {
    // No se devuelve una lista a medias. Un keygroup que se lee a medias es un
    // keygroup con un hueco en el medio, y el hueco se ve como un 0.
    return {
      ok: false,
      motivo: `el keygroup ${indice} no se ha podido leer entero`,
      index: indice,
      // La MISMA forma que la del camino bueno, con las listas vacias. Un
      // consumidor que se recorta con `?? []` funciona; uno que se fia de
      // que `campos` existe se rompe con un TypeError en vez de con un estado,
      // y un TypeError no dice nada del keygroup que falta.
      campos: [],
      grupos: [],
      sinGrupo: 0,
    };
  }

  const porGrupo = new Map();
  for (const g of S950_GROUPS) porGrupo.set (g, []);

  for (const field of leidos) {
    const destino = porGrupo.get (field.group);
    if (destino) destino.push (campoAPintar (field, field.value));
  }

  const grupos = S950_GROUPS.map ((g) => grupoAPintar (g, porGrupo.get (g) ?? []));

  // Los grupos con campos que el contrato no lista en S950_GROUPS no se perderian
  // sin avisar: se cuentan aqui, para que un grupo nuevo en el contrato se vea
  // en el panel como campos sin pagina en vez de desaparecer.
  const sinGrupo = leidos.filter ((f) => !S950_GROUPS.includes (f.group)).length;

  return {
    ok: true,
    index: indice,
    name: programa.nombreKeygroup ?? null,
    campos: leidos.map ((f) => campoAPintar (f, f.value)),
    grupos,
    sinGrupo,
  };
}

/** Edita TODOS los keygroups de un programa, que es lo que un panel quiere. */
export function editarPrograma (programa) {
  if (!programa) return null;

  const total = keygroupCount (programa.entry);
  const keygroups = [];

  for (let i = 0; i < total; i += 1) {
    const k = editarKeygroup (programa, i);
    keygroups.push (k);
  }

  return {
    ok: keygroups.every ((k) => k.ok),
    name: programa.name,
    number: programa.number,
    total,
    keygroups,
  };
}

//==============================================================================
/** Abre un programa de un disco. Devuelve el error en vez de lanzar. */
export function abrirPrograma (bytes, slot) {
  if (!bytes || bytes.length === 0) {
    return {
      ok: false,
      motivo: 'el fichero esta vacio',
      editable: false,
    };
  }

  const programa = readProgram (bytes, slot);

  if (!programa) {
    return {
      ok: false,
      motivo: describeFallo (bytes, slot),
      editable: false,
    };
  }

  // `readProgram` devuelve la entrada y no los bytes, porque son unos 800 KB que
  // no tiene sentido copiar por cada keygroup. Se pegan aqui una vez para que
  // `editarKeygroup` pueda recorrerlos.
  return {
    ok: true,
    editable: false,   // este editor pinta, no escribe. Ver la cabecera.
    bytes,
    entry: programa.entry,
    name: programa.name,
    number: programa.number,
    keygroups: programa.keygroups,
    slot: programa.slot,
  };
}

/** Por que no se pudo abrir, dicho en castellano y no como un codigo. */
function describeFallo (bytes, slot) {
  // No se lanza ni se devuelve un codigo suelto: quien lo muestra es un panel,
  // y un panel que enseña "E_SLOT_3" no ayuda a nadie. Se prueba en orden de lo
  // probable, que es como se depura.
  const d = slot * 24;

  if (d + 24 > bytes.length) return `la ranura ${slot} no cabe en la imagen`;

  const type = String.fromCharCode (bytes[d + 16] ?? 0);
  if (type === '\0') return `la ranura ${slot} esta libre`;

  if (type !== 'P') return `la ranura ${slot} es un fichero de tipo '${type}', no un programa`;

  const length = bytes[d + 17] | (bytes[d + 18] << 8) | (bytes[d + 19] << 16);

  if (length < 38 + 70) return `el programa de la ranura ${slot} es demasiado corto (${length} bytes)`;

  if ((length - 38) % 70 !== 0) {
    return `el programa de la ranura ${slot} esta danado: ${length} bytes no son 38 + n * 70, ` +
           'asi que su cuerpo no es un numero entero de keygroups';
  }

  return `el programa de la ranura ${slot} no se ha podido leer: su cadena de bloques no cubre los ${length} bytes`;
}

export { CAMPO_A_CURVA };
