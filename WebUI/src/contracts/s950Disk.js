/**
 * Lector de discos S900/S950 para la WebUI, en JS.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * QUE HACE ESTE FICHERO Y DE DONDE SALE.
 *
 * `ABDSharedCode/SynthCore/S950Disk.h` es el importador en C++ y sabe leer un
 * programa real. Este fichero es la otra mitad de esa lectura: la geometría del
 * disco, la cadena de bloques y las cuatro codificaciones de byte, escritas
 * para que un panel pueda pintar un programa que ha leido de un `.img`.
 *
 * NO HAY CODIGO COPIADO. La geometría del disco son DATOS de la máquina —el
 * directorio son 64 entradas de 24 bytes en 0x000, la tabla de asignación empieza
 * en 0x600, un programa son 38 bytes de cabecera más 70 por keygroup— y el
 * recorrido lo está haciendo aquí un lector independiente. Es el mismo criterio
 * con el que se escribió el de C++, y el que separación exige.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * LO QUE NO SE PIDE AL JSON, Y POR QUE.
 *
 * El catálogo `s950_patch_fields.json` trae la geometría del KEYGROUP (registro
 * de 70 bytes, cabecera de 38, desplazamiento de cada campo) y se importa. La
 * del DISCO no viene: `S950PatchFields.h` no la exporta, y el contenedor entero
 * —bloques, directorio, tabla— vive en `S950Disk.h`, que no genera contrato.
 *
 * Aquí eso son siete constantes, duplicadas a mano. Es una copia y se dice. La
 * alternativa era inventar un contrato nuevo para el contenedor, que es trabajo
 * de verdad y no cabe aquí. Lo que SÍ se ha hecho es que esas siete cifras las
 * fije un test contra los valores que declara el C++, de modo que si alguien
 * cambia el formato en C++ y no cambia el JSON, el rojo sale en la suite y en
 * ningún otro sitio.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * LAS CUATRO CODIFICACIONES, QUE SON TODO EL TRABAJO.
 *
 * Leer "el byte" no basta, y por eso leer el byte es lo primero que sale mal:
 *
 *   - `Unsigned`: el byte tal cual.
 *   - `Signed`: complemento a dos de 8 bits, así que 0x80 es −128.
 *   - `Port`: el panel 0..10 se guarda UNO MENOS, y el 0xFF del disco es el 0 del
 *     panel ("todos"). Sin esto, un programa en estéreo se leería como 255, que
 *     no es una salida: es la de todos.
 *   - `Bit`: un bit suelto, y los cuatro flags comparten el byte 18.
 *
 * Y el cuarto caso, que es una letra donde esperaba un número: la envolvente del
 * filtro EN BLANCO. Un S900 no tenía envolvente de filtro, y lo que escribía en
 * sus cuatro bytes eran ESPACIOS. Un 0x20 leído como valor es 32, y 32 no es un
 * ajuste que nadie eligió. Leerlo como 32 suena a una envolvente lenta y
 * falsa que no se puede corregir desde el panel, porque el panel no sabe que
 * ahí hay un espacio. Así que sale la envolvente plana —sustain 99, el resto 0—,
 * que es lo que la maquina ensena.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * NADA DE ESTO LANZA.
 *
 * Un disco puede estar dañado, y un lector que lanza ante un fichero raro no
 * puede decir cuál. Todas las funciones devuelven `null` o un motivo, y quien
 * llama decide. Es el mismo contrato que el de C++.
 *
 * @example
 *   import { readProgram, readKeygroup } from '../contracts/s950Disk.js';
 *
 *   const bytes = new Uint8Array(await file.arrayBuffer());
 *   const programa = readProgram(bytes, 0);
 *   if (programa) {
 *     for (const campo of readKeygroup(programa, 0))
 *       console.log(campo.name, campo.value, campo.lo, campo.hi);
 *   }
 */

import { S950_PATCH_CATALOGUE } from './s950PatchFields.js';
import { S950_PATCH_CONTRACT } from './s950PatchFields.js';

/**
 * La geometría del CONTENEDOR del disco.
 *
 * Estos siete números son copia de `S950Disk.h` y NO vienen del contrato, que
 * solo conoce el keygroup. `s950DiskGeometry.test.js` los fija contra lo que
 * declara el C++, que es lo unico que puede detectar que se han quedado viejos.
 */
export const S950_DISK = {
  blockSize: 1024,
  dirOffset: 0x000,
  dirEntries: 64,
  dirEntrySize: 24,
  fatOffset: 0x600,
  fatEnd: 0x8000,
  doubleDensityBlocks: 800,
  highDensityBlocks: 1600,
  fileHeaderSize: 60,
};

/** El byte 18 de cada keygroup, donde viven los cuatro flags. */
export const S950_FLAGS_BYTE = 18;

/** El primer byte de la envolvente del filtro, y el byte que significa "en blanco". */
export const S950_VCF_FIRST_BYTE = 34;
export const S950_BLANK_BYTE = 0x20;

/** La geometría del KEYGROUP, que SÍ viene del contrato generado. */
export const S950_KEYGROUP = {
  recordSize: S950_PATCH_CONTRACT?.geometry?.keygroupRecordSize ?? 70,
  programHeaderSize: S950_PATCH_CONTRACT?.geometry?.programHeaderSize ?? 38,
  maxCount: S950_PATCH_CONTRACT?.geometry?.keygroupMaxCount ?? 64,
  countOffset: S950_PATCH_CONTRACT?.geometry?.keygroupCountOffset ?? 23,
  programNumberOffset: S950_PATCH_CONTRACT?.geometry?.programNumberOffset ?? 26,
  nameOffset: S950_PATCH_CONTRACT?.geometry?.keygroupNameOffset ?? 24,
  nameSize: S950_PATCH_CONTRACT?.geometry?.keygroupNameSize ?? 10,
};

/** Cuantos bloques tiene una imagen, deducido de su tamaño. */
export function totalBlocks (bytes) {
  return Math.floor(bytes.length / S950_DISK.blockSize);
}

/** La densidad, que la maquina deduce del tamaño y no de un campo. */
export function isHighDensity (bytes) {
  return totalBlocks(bytes) === S950_DISK.highDensityBlocks;
}

//==============================================================================
/** El siguiente bloque de la cadena. `null` si el bloque no existe. */
export function fat (bytes, block) {
  if (block < 0 || block >= totalBlocks(bytes)) return null;

  const at = S950_DISK.fatOffset + block * 2;
  if (at + 1 >= bytes.length) return null;

  return bytes[at] | (bytes[at + 1] << 8);
}

//==============================================================================
/** Una entrada del directorio. Los nombres son unicos DENTRO de un tipo. */
function readEntry (bytes, slot) {
  const d = S950_DISK.dirOffset + slot * S950_DISK.dirEntrySize;
  if (d + S950_DISK.dirEntrySize > bytes.length) return null;

  // ── DONDE ESTA EL NOMBRE Y EL TIPO ──
  //
  // El nombre arranca en el byte 0 de la entrada, NO en el 1, y el tipo va en el
  // 16, no al final de los nombre. Son los bytes que escribe `addFile` en
  // `S950Disk.h` (`image[d]..image[d+9]` el nombre, `image[d + 16]` el tipo), y
  // estan aqui porque los leidos de ahi.
  //
  // El byte 0 a cero no distingue "ranura libre" de "nombre vacio", asi que la
  // ranura libre se reconoce por lo que dice el tipo: un 0 ahi no es ningun
  // tipo de fichero que la maquina tenga.
  const type = String.fromCharCode(bytes[d + 16]);
  if (type === '\0') return null;   // ranura libre

  // 10 caracteres imprimibles y nada mas: lo demas es basura o relleno.
  let name = '';
  for (let i = 0; i < 10; i += 1) {
    const c = bytes[d + i];
    if (c < 0x20 || c >= 0x7f) break;
    name += String.fromCharCode(c);
  }

  const u16 = (at) => bytes[at] | (bytes[at + 1] << 8);
  const u24 = (at) => bytes[at] | (bytes[at + 1] << 8) | (bytes[at + 2] << 16);

  const length = u24(d + 17);
  const startBlock = u16(d + 20);

  const entry = {
    slot,
    name,
    type,
    length,
    startBlock,
    chainBlocks: 0,
    chainOk: false,
  };

  // La cadena tiene que COBRIR los bytes que el fichero declara. Si no llega, el
  // programa no se lee: dar por buenos los bloques que hay seria leer de mas.
  let block = startBlock;
  let covered = 0;
  let blocks = 0;

  while (block !== S950_DISK.fatEnd && block >= 0 && block < totalBlocks(bytes) && blocks < totalBlocks(bytes)) {
    covered += S950_DISK.blockSize;
    blocks += 1;
    block = fat(bytes, block);
  }

  entry.chainBlocks = blocks;
  entry.chainOk = length > 0 && covered >= length;

  return entry;
}

/** El directorio entero, con las ranuras libres ya fuera. */
export function readDirectory (bytes) {
  const out = [];
  for (let i = 0; i < S950_DISK.dirEntries; i += 1) {
    const e = readEntry(bytes, i);
    if (e) out.push(e);
  }
  return out;
}

/** Busca un fichero por nombre y tipo. Los dos importan, y por eso hay dos. */
export function findFile (bytes, name, type) {
  const wanted = String(name ?? '').toUpperCase();

  for (const e of readDirectory(bytes))
    if (e.type === type && e.name.toUpperCase() === wanted) return e;

  return null;
}

//==============================================================================
/** La posicion en la IMAGEN de un byte del FICHERO, cruzando la cadena.

    Un byte del fichero `offset` vive en el bloque `offset / 1024` de la cadena,
    en la posicion `offset % 1024` de ese bloque. La division entera y el modulo
    son complementos, y por eso un keygroup partido entre dos bloques no
    necesita un caso especial: solo necesita que se mire un byte cada vez.

    @returns el indice en la imagen, o `null` si ese byte no existe.
*/
function fileByteAt (bytes, entry, offset) {
  if (offset < 0 || offset >= entry.length) return null;

  const index = Math.floor(offset / S950_DISK.blockSize);
  if (index >= entry.chainBlocks) return null;

  let block = entry.startBlock;
  for (let i = 0; i < index; i += 1) {
    if (block === S950_DISK.fatEnd || block < 0 || block >= totalBlocks(bytes)) return null;
    block = fat(bytes, block);
  }

  if (block === S950_DISK.fatEnd || block < 0 || block >= totalBlocks(bytes)) return null;

  const at = block * S950_DISK.blockSize + (offset % S950_DISK.blockSize);
  return at < bytes.length ? at : null;
}

/** El offset DENTRO DEL FICHERO de un byte de un keygroup.

    Sin la cabecera de 60 bytes de los samples: un programa empieza directamente
    con sus 38. Ver `keygroupCount` para por que esto importa.
*/
function fileOffsetOf (keygroup, offset) {
  return S950_KEYGROUP.programHeaderSize + keygroup * S950_KEYGROUP.recordSize + offset;
}

/** El byte del registro de un keygroup, resuelto hasta la imagen. */
export function keygroupByteAt (bytes, entry, keygroup, offset) {
  if (entry.type !== 'P') return null;
  if (offset < 0 || offset >= S950_KEYGROUP.recordSize) return null;

  const n = keygroupCount(entry);
  if (keygroup < 0 || keygroup >= n) return null;

  return fileByteAt(bytes, entry, fileOffsetOf(keygroup, offset));
}

//==============================================================================
/** Cuantos keygroups tiene un programa, por su LONGITUD.

    Y aqui esta la trampa del formato, que es la que hace que un lector nuevo
    lea cero keygroups de un programa que tiene dieciseis: la cabecera de 60
    bytes que tiene `fileHeaderSize` es de los SAMPLES, no de los programas. Un
    programa empieza directamente con sus 38 bytes de cabecera, asi que su
    longitud es `38 + n * 70` y no `60 + 38 + n * 70`. Sumarle los 60 de mas
    hace que el cuerpo no sea multiplo de 70 y el programa se lea como danado.

    Y si no es multiplo, no se devuelve un resto truncado: devolverlo seria leer
    basura como si fuera un keygroup mas.
*/
export function keygroupCount (entry) {
  if (entry.type !== 'P') return 0;
  if (entry.length < S950_KEYGROUP.programHeaderSize + S950_KEYGROUP.recordSize) return 0;

  const body = entry.length - S950_KEYGROUP.programHeaderSize;
  return body % S950_KEYGROUP.recordSize === 0 ? body / S950_KEYGROUP.recordSize : 0;
}

//==============================================================================
/** Si la envolvente del filtro de este keygroup esta EN BLANCO. */
export function vcfEnvelopeIsBlank (bytes, entry, keygroup) {
  for (let i = 0; i < 4; i += 1) {
    const at = keygroupByteAt(bytes, entry, keygroup, S950_VCF_FIRST_BYTE + i);
    if (at === null) return false;
    if (bytes[at] !== S950_BLANK_BYTE) return false;
  }
  return true;
}

/** Recorta al rango del campo. Una maquina recorta; un importador, no siempre. */
function clampToField (field, value) {
  if (value < field.lo) return field.lo;
  if (value > field.hi) return field.hi;
  return value;
}

//==============================================================================
/** Los bytes de la ENVOLVENTE DEL FILTRO, para el caso en blanco. */
const VCF_BLANK_CODES = new Set(['vcfAttack', 'vcfDecay', 'vcfSustain', 'vcfRelease']);

//==============================================================================
/** Lee un campo de un keygroup y lo devuelve en UNIDADES DE PANEL.

    @returns el valor, o `null` si el byte no se puede resolver.
*/
export function readKeygroupField (bytes, entry, keygroup, field) {
  if (!field) return null;

  const at = keygroupByteAt(bytes, entry, keygroup, field.byteOffset);
  if (at === null) return null;

  const raw = bytes[at];
  let out;

  switch (field.encoding) {
    case 'Signed':
      out = raw > 127 ? raw - 256 : raw;
      break;

    case 'Port':
      out = raw === 0xff ? 0 : raw + 1;
      break;

    case 'Bit':
      out = (raw & field.bitMask) !== 0 ? 1 : 0;
      break;

    case 'Unsigned':
    default:
      out = raw;
      break;
  }

  // El blanco de la envolvente del filtro, ANTES de recortar: un 0x20 son 32, y
  // 32 recortado a 0..99 sigue siendo 32, que es una envolvente lenta y falsa.
  if (VCF_BLANK_CODES.has(field.code) && vcfEnvelopeIsBlank(bytes, entry, keygroup))
    return field.byteOffset === S950_VCF_FIRST_BYTE + 2 ? 99 : 0;

  return clampToField(field, out);
}

//==============================================================================
/** Los 38 campos de un keygroup, en el orden de la tabla del contrato.

    Para un panel, que los quiere todos de una vez. Si el keygroup no se puede
    leer devuelve `null` y una lista vacia, nunca una lista a medias: un control
    que nace sin saber su rango es un control que se ve saltar.
*/
export function readKeygroup (bytes, entry, keygroup) {
  const fields = S950_PATCH_CATALOGUE?.fields ?? [];
  if (fields.length === 0) return [];

  const out = [];
  let leidos = 0;

  for (const field of fields) {
    const value = readKeygroupField(bytes, entry, keygroup, field);

    if (value === null) break;   // se para en vez de inventar el resto

    out.push({
      code: field.code,
      name: field.name,
      group: field.group,
      byteOffset: field.byteOffset,
      lo: field.lo,
      hi: field.hi,
      unit: field.unit,
      encoding: field.encoding,
      value,
    });
    leidos += 1;
  }

  return leidos === fields.length ? out : null;
}

//==============================================================================
/** Un programa abierto: la entrada y su geometria ya resueltas. */
export function readProgram (bytes, slot) {
  const entry = readEntry(bytes, slot);
  if (!entry) return null;
  if (entry.type !== 'P') return null;
  if (!entry.chainOk) return null;

  const keygroups = keygroupCount(entry);
  if (keygroups < 1) return null;

  // El nombre de la CABECERA del programa, que es lo que ve la persona en la
  // pantalla de la maquina, y no el del directorio. Son dos copias y a veces
  // no coinciden: el directorio es donde lo busca el importador, y la cabecera es
  // lo que la maquina enseña.
  const at = fileByteAt(bytes, entry, 0);
  let name = '';
  for (let i = 0; i < S950_KEYGROUP.nameSize; i += 1) {
    const b = fileByteAt(bytes, entry, i);
    if (b === null) break;
    if (bytes[b] < 0x20 || bytes[b] >= 0x7f) break;
    name += String.fromCharCode(bytes[b]);
  }

  const numAt = fileByteAt(bytes, entry, S950_KEYGROUP.programNumberOffset);

  return {
    entry,
    slot,
    name,
    number: numAt === null ? null : bytes[numAt],
    keygroups,
  };
}

/** Los programas de un disco, por ranura. */
export function readPrograms (bytes) {
  return readDirectory(bytes)
    .filter((e) => e.type === 'P' && e.chainOk && keygroupCount(e) > 0)
    .map((e, i) => readProgram(bytes, e.slot))
    .filter(Boolean)
    .map((p, i) => ({ ...p, index: i }));
}
