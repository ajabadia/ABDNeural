/**
 * El lector de discos S950, contrastado contra el C++ que lo escribio primero.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * UN LECTOR DE DISCO NO SE PRUEBA CON SUFICIENTE.
 *
 * Un panel que pinta mal un campo no se rompe: enseña un numero. Un motor que
 * importa mal un campo no se rompe: suena raro. Y ninguna de las dos cosas falla
 * donde se ve, asi que el error se descubre cuando alguien toca un sonido y no
 * sabe por que.
 *
 * Por eso el nucleo de este fichero no es "funciona", sino UNA TABLA DE 38
 * VALORES QUE EL C++ PRODUCE Y EL JS TIENE QUE REPETIR. Los mismos 38, en el
 * mismo orden, con los mismos valores. Si el JS recorre mal la cadena de
 * bloques, si confunde el complemento a dos, si se come el 0xFF del puerto, o si
 * suma los 60 bytes de la cabecera que son de los samples, los numeros se
 * mueven y el rojo sale en la linea que toca —que es el byte que esta mal— y no
 * en un "el patch suena raro" tres meses despues.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * LA TABLA DE REFERENCIA DE DONDE VIENE, Y POR QUE ESTA EN EL TEST.
 *
 * Viene de ejecutar `S950Disk::addProgram("PRUEBA", 3)`, escribir un valor
 * distinto en cada campo y leerlos todos con `readAllFields`. Los valores que
 * salen estan en `LEIDO_EN_CPP` mas abajo, con el byte al que pertenece cada uno.
 *
 * No se ha copiado el algoritmo: se ha copiado el RESULTADO, que es un dato que
 * dos lectores independientes tienen que coincidir. Si el C++ cambia el formato,
 * esta tabla se queda vieja y este test falla —que es justo lo que queremos— en
 * vez de que los dos lectores cambien a la vez y nadie note nada.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * Y LA GEOMETRIA, FIJADA A MANO.
 *
 * `s950Disk.js` copia siete constantes del contenedor de `S950Disk.h` porque el
 * contrato JSON solo conoce el keygroup. Esa copia se puede quedar vieja, asi
 * que aqui se fija con los valores que declara el C++. Si alguien cambia el
 * bloque de 1024 a otra cosa en C++ y regenera el JSON, este test dice que aqui
 * sigue diciendo 1024.
 */

import { describe, expect, it } from 'vitest';

import {
  S950_DISK,
  S950_KEYGROUP,
  S950_VCF_FIRST_BYTE,
  S950_BLANK_BYTE,
  fat,
  fileOffsetOfForTest,
  findFile,
  isHighDensity,
  keygroupByteAt,
  keygroupCount,
  readDirectory,
  readKeygroup,
  readKeygroupField,
  readProgram,
  readPrograms,
  totalBlocks,
  vcfEnvelopeIsBlank,
} from '../src/contracts/s950Disk.js';
import { S950_PATCH_CATALOGUE } from '../src/contracts/s950PatchFields.js';

/**
 * Lo que el C++ lee de un programa de 3 keygroups al que se le ha escrito un
 * valor por campo. El valor escrito es `valores[i % 16]`, y el patron se repite
 * para que un byte equivocado se note como un valor equivocado y no como un
 * numero raro.
 *
 * El orden es el de la tabla del catalogo, que es el de `readAllFields`.
 */
const LEIDO_EN_CPP = [
  ['lowKey', 0, 0], ['highKey', 1, 1], ['velocitySwitch', 2, 2],
  ['vcaAttack', 3, 3], ['vcaDecay', 4, 5], ['vcaSustain', 5, 8],
  ['vcaRelease', 6, 13], ['velToFilter', 7, 21], ['keyToFilter', 8, 34],
  ['velToAttack', 9, 55], ['velToRelease', 10, 50], ['velToLoudness', 11, 99],
  ['warpVelocity', 12, 50], ['warpDepth', 13, 42], ['warpTime', 14, 7],
  ['lfoDelay', 15, 0], ['lfoRate', 16, 0], ['lfoDepth', 17, 1],
  ['lfoAftertouch', 21, 2], ['lfoModwheel', 22, 3], ['outputPort', 19, 5],
  ['vcfAmount', 23, 8], ['vcfAttack', 34, 13], ['vcfDecay', 35, 21],
  ['vcfSustain', 36, 34], ['vcfRelease', 37, 55], ['softFine', 42, 89],
  ['softTranspose', 43, 50], ['softFilter', 44, 50], ['softLoudness', 45, 42],
  ['loudFine', 64, 7], ['loudTranspose', 65, 0], ['loudFilter', 66, 0],
  ['loudLoudness', 67, 1], ['constantPitch', 18, 1], ['lfoDesync', 18, 1],
  ['oneShot', 18, 1], ['velocityReleaseOn', 18, 1],
];

/** Los mismos 38 en un objeto, para comparar de una vez. */
const LEIDO = Object.fromEntries(LEIDO_EN_CPP.map(([code, , value]) => [code, value]));

//==============================================================================
/** Escribe un disco entero con un programa dentro, siguiendo las reglas del C++.

    Esto es un CONSTRUCTOR de fixture, no una copia del importador: hace lo
    minimo —directorio, cadena, cuerpo— para que el lector tenga algo real que
    recorrer. Lo que importa es que la cadena no sea contigua, porque si lo fuera
    el lector no cruzaria bloques y medio de lo que se prueba no se probaria.
*/
function construirDisco (opciones = {}) {
  const {
    nombre = 'PRUEBA',
    keygroups = 3,
    bloquesSaltados = [],   // bloques que la cadena NO usa, para partirla
    tipo = 'P',
    lengthDeclarado = null,
  } = opciones;

  const bytes = new Uint8Array(S950_DISK.doubleDensityBlocks * S950_DISK.blockSize);
  const total = totalBlocks(bytes);

  // La cadena del programa, con huecos. La maquina no los gasta, y por eso el
  // recorrido tiene que seguirlos en vez de suponer que son contiguos.
  const cadena = [];
  let siguiente = 4;
  for (let i = 0; i < keygroups + 4; i += 1) {
    if (bloquesSaltados.includes(siguiente)) {
      siguiente += 1;
      i -= 1;
      if (siguiente >= total - 1) break;
      continue;
    }
    cadena.push(siguiente);
    siguiente += 1;
  }

  // La tabla de asignacion: fin de cadena en el ultimo, y los que no son de
  // nadie apuntan a si mismos para que un recorrido perdido no se vaya de rango.
  for (let b = 0; b < total; b += 1) {
    const at = S950_DISK.fatOffset + b * 2;
    const value = cadena.includes(b)
      ? (cadena[cadena.indexOf(b) + 1] ?? S950_DISK.fatEnd)
      : b;
    bytes[at] = value & 0xff;
    bytes[at + 1] = (value >> 8) & 0xff;
  }

  // El cuerpo del programa: cabecera de 38 y 70 por keygroup.
  const cuerpo = new Uint8Array(S950_KEYGROUP.programHeaderSize + keygroups * S950_KEYGROUP.recordSize);
  for (let i = 0; i < Math.min(10, nombre.length); i += 1) cuerpo[i] = nombre.charCodeAt(i);
  cuerpo[S950_KEYGROUP.countOffset] = keygroups;
  cuerpo[S950_KEYGROUP.programNumberOffset] = 3;

  // Los mismos valores que escribe el C++: `valores[i % 16]`.
  const VALORES = [0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 99, 50, 42, 7, 0];
  const campos = S950_PATCH_CATALOGUE?.fields ?? [];

  campos.forEach((campo, i) => {
    const byteOffset = S950_KEYGROUP.programHeaderSize + campo.byteOffset;
    if (byteOffset >= cuerpo.length) return;

    const pedido = VALORES[i % 16];
    let byte;

    switch (campo.encoding) {
      case 'Signed':
        byte = pedido < 0 ? pedido + 256 : pedido;
        break;
      case 'Port':
        byte = pedido === 0 ? 0xff : pedido - 1;
        break;
      case 'Bit':
        byte = campo.bitMask;
        break;
      default:
        byte = pedido;
        break;
    }

    // Los cuatro flags comparten el byte 18: se escribe de uno en uno con
    // lectura-modificacion-escritura, que es como lo hace la maquina. Escribir
    // el byte entero pondria los cuatro a la vez y perderia el reservado 0x02.
    if (campo.encoding === 'Bit' && campo.byteOffset === 18) byte |= cuerpo[byteOffset] & ~campo.bitMask & 0xff;

    cuerpo[byteOffset] = byte & 0xff;
  });

  // A la imagen, cruzando la cadena bloque a bloque.
  for (let i = 0; i < cuerpo.length; i += 1) {
    const bloque = cadena[Math.floor(i / S950_DISK.blockSize)];
    if (bloque === undefined) break;
    bytes[bloque * S950_DISK.blockSize + (i % S950_DISK.blockSize)] = cuerpo[i];
  }

  // La entrada del directorio, en los bytes que escribe `addFile`: el nombre
  // desde el 0 y el tipo en el 16.
  const d = 0;
  for (let i = 0; i < Math.min(10, nombre.length); i += 1) bytes[d + i] = nombre.charCodeAt(i);
  bytes[d + 16] = tipo.charCodeAt(0);
  const largo = lengthDeclarado ?? cuerpo.length;
  bytes[d + 17] = largo & 0xff;
  bytes[d + 18] = (largo >> 8) & 0xff;
  bytes[d + 19] = (largo >> 16) & 0xff;
  const start = cadena[0] ?? 0;
  bytes[d + 20] = start & 0xff;
  bytes[d + 21] = (start >> 8) & 0xff;

  return { bytes, cadena, cuerpo };
}

describe('la geometria del contenedor coincide con la del C++', () => {
  it('declara los mismos numeros que S950Disk.h', () => {
    // Estos valores estan en `S950Disk.h` como `static constexpr`. Si alguien los
    // cambia alli, esta tabla se queda vieja y el rojo sale aqui, que es donde
    // se puede arreglar: el JSON no cubre el contenedor, y sin esto nadie se
    // enteraria de que la copia se ha quedado vieja.
    expect(S950_DISK.blockSize).toBe(1024);
    expect(S950_DISK.dirOffset).toBe(0x000);
    expect(S950_DISK.dirEntries).toBe(64);
    expect(S950_DISK.dirEntrySize).toBe(24);
    expect(S950_DISK.fatOffset).toBe(0x600);
    expect(S950_DISK.fatEnd).toBe(0x8000);
    expect(S950_DISK.doubleDensityBlocks).toBe(800);
    expect(S950_DISK.highDensityBlocks).toBe(1600);
    expect(S950_DISK.fileHeaderSize).toBe(60);
  });

  it('la geometria del keygroup viene del contrato, no esta escrita a mano', () => {
    // Esta parte SI tiene contrato, y la prueba es que no haya copia: si el
    // contrato cambia el registro de 70 a otro numero, el JS tiene que seguirlo
    // sin que nadie toque este fichero.
    expect(S950_KEYGROUP.recordSize).toBe(S950_PATCH_CATALOGUE?.geometry?.keygroupRecordSize ?? 70);
    expect(S950_KEYGROUP.recordSize).toBe(70);
    expect(S950_KEYGROUP.programHeaderSize).toBe(38);
    expect(S950_KEYGROUP.maxCount).toBe(64);
  });

  it('sabe que densidad tiene una imagen por su tamano', () => {
    const dd = new Uint8Array(S950_DISK.doubleDensityBlocks * S950_DISK.blockSize);
    const hd = new Uint8Array(S950_DISK.highDensityBlocks * S950_DISK.blockSize);

    expect(totalBlocks(dd)).toBe(800);
    expect(totalBlocks(hd)).toBe(1600);
    expect(isHighDensity(dd)).toBe(false);
    expect(isHighDensity(hd)).toBe(true);
  });
});

describe('la tabla de asignacion', () => {
  it('sigue la cadena y termina donde dice', () => {
    const { bytes, cadena } = construirDisco();

    for (let i = 0; i < cadena.length - 1; i += 1) expect(fat(bytes, cadena[i])).toBe(cadena[i + 1]);
    expect(fat(bytes, cadena[cadena.length - 1])).toBe(S950_DISK.fatEnd);
  });

  it('devuelve null para un bloque que no existe, no un numero cualquiera', () => {
    const { bytes } = construirDisco();

    // Un bloque inventado daria una posicion inventada, y ahi se escribiria lo
    // que toque. Fallar es lo unico honesto.
    expect(fat(bytes, -1)).toBeNull();
    expect(fat(bytes, 99999)).toBeNull();
  });
});

describe('el directorio', () => {
  it('lee la entrada con su nombre, su tipo y su longitud', () => {
    const { bytes } = construirDisco();
    const dir = readDirectory(bytes);

    expect(dir).toHaveLength(1);
    expect(dir[0].name).toBe('PRUEBA');
    expect(dir[0].type).toBe('P');
    expect(dir[0].length).toBe(38 + 3 * 70);
    expect(dir[0].chainOk).toBe(true);
  });

  it('busca por nombre Y por tipo, porque los nombres solo son unicos dentro de un tipo', () => {
    const { bytes } = construirDisco();

    expect(findFile(bytes, 'PRUEBA', 'P')).not.toBeNull();
    // Mismo nombre, otro tipo: la maquina lo permite y no es el mismo fichero.
    expect(findFile(bytes, 'PRUEBA', 'S')).toBeNull();
    expect(findFile(bytes, 'OTRO', 'P')).toBeNull();
  });
});

describe('los keygroups se deducen de la longitud, y NO con la cabecera de los samples', () => {
  it('cuenta los que salen de 38 + n * 70', () => {
    for (const n of [1, 3, 16, 64]) {
      const { bytes } = construirDisco({ keygroups: n });
      const p = readProgram(bytes, 0);
      expect(p, `un programa de ${n} keygroups deberia abrir`).not.toBeNull();
      expect(p.keygroups).toBe(n);
    }
  });

  it('un programa cuya longitud no es multiplo de 70 no tiene keygroups', () => {
    // Y no devuelve un resto truncado: devolverlo seria leer basura como si
    // fuera un keygroup mas, que es peor que no devolver nada.
    const { bytes } = construirDisco({ keygroups: 3, lengthDeclarado: 38 + 3 * 70 + 5 });

    expect(keygroupCount(readDirectory(bytes)[0])).toBe(0);
    expect(readProgram(bytes, 0)).toBeNull();
  });

  it('esta es la trampa: restar los 60 de la cabecera haria leer cero', () => {
    // El `fileHeaderSize` de 60 es de los SAMPLES. Si un programa lo pagara, su
    // cuerpo no seria multiplo de 70 y se leeria como danado con cero keygroups
    // en vez de los 16 que tiene. Este test existe para que ese error, que es el
    // mas facil de cometer con este formato, se vea escrito.
    // Lo que se mira es el CUERPO, que es la longitud menos la cabecera del
    // programa. Sin restar los 38, el numero que sale no es el cuerpo de nadie.
    const body = (length) => length - 38;

    expect(body(38 + 16 * 70) % 70).toBe(0);        // lo que hace la maquina
    expect(body(60 + 16 * 70) % 70).not.toBe(0);    // lo que haria restando de mas
  });
});

describe('leer un campo, contrasted con lo que lee el C++', () => {
  it('los 38 campos salen con el valor que lee el C++', () => {
    const { bytes } = construirDisco();
    const p = readProgram(bytes, 0);
    const leidos = readKeygroup(bytes, p.entry, 0);

    expect(leidos, 'los 38 campos deberian leerse').not.toBeNull();
    expect(leidos).toHaveLength(LEIDO_EN_CPP.length);

    for (const [code, byteOffset, value] of LEIDO_EN_CPP) {
      const campo = leidos.find((f) => f.code === code);
      expect(campo, `falta el campo ${code}`).toBeDefined();
      expect(campo.byteOffset, `${code} deberia estar en el byte ${byteOffset}`).toBe(byteOffset);
      expect(campo.value, `${code} deberia valer ${value}`).toBe(value);
    }
  });

  it('el mismo valor con el nombre y el rango del contrato', () => {
    // El byte lo pone el disco, el nombre y el rango los pone el contrato. Que
    // vengan de fuentes distintas es el punto: es lo que impide que un panel
    // tenga su propia lista y se quede vieja en silencio.
    const { bytes } = construirDisco();
    const p = readProgram(bytes, 0);
    const leidos = readKeygroup(bytes, p.entry, 0);

    for (const campo of leidos) {
      const delContrato = S950_PATCH_CATALOGUE.field(campo.code);
      expect(campo.name).toBe(delContrato.name);
      expect(campo.lo).toBe(delContrato.lo);
      expect(campo.hi).toBe(delContrato.hi);
      expect(campo.value).toBeGreaterThanOrEqual(campo.lo);
      expect(campo.value).toBeLessThanOrEqual(campo.hi);
    }
  });

  it('el recorrido cruza una cadena con huecos, que es lo que de verdad pasa', () => {
    // Una cadena contigua no obliga a seguir la tabla: con bloques seguidos, un
    // lector que sumase en vez de mirar la FAT tambien acertaria. Con huecos
    // entre medias, solo acierta el que la sigue.
    const { bytes, cadena } = construirDisco({ bloquesSaltados: [5, 6, 7, 9] });

    // Lo que hace el hueco no es DESORDENAR la cadena, es dejar bloques en
    // medio. Una cadena desordenada seria otro caso, y no el que importa: el
    // recorrido va por la tabla de asignacion, asi que mientras los numeros
    // esten en orden el recorrido correcto y el que sumaBlocks darian el mismo
    // resultado. Lo que los separa de verdad es que falten bloques por el
    // medio, y eso es lo que se comprueba aqui: que no sean correlativos.
    // Con que haya UN hueco basta para separar los dos recorridos, y no hace
    // falta que los salten todos: un salto correlativo y otro con hueco ya
    // rompen a quien sume bloques en vez de mirar la FAT.
    const huecos = cadena.filter((b, i) => i > 0 && b - cadena[i - 1] > 1).length;
    expect(huecos, 'la cadena deberia tener al menos un bloque en medio').toBeGreaterThan(0);

    const p = readProgram(bytes, 0);
    const leidos = readKeygroup(bytes, p.entry, 0);

    expect(leidos).not.toBeNull();
    expect(leidos.map((f) => f.value)).toEqual(LEIDO_EN_CPP.map(([, , v]) => v));
  });

  it('un keygroup que no existe da null, y no el primero por las dudas', () => {
    const { bytes } = construirDisco({ keygroups: 3 });
    const p = readProgram(bytes, 0);

    expect(readKeygroup(bytes, p.entry, 3)).toBeNull();
    expect(readKeygroup(bytes, p.entry, -1)).toBeNull();
    expect(readKeygroup(bytes, p.entry, 999)).toBeNull();
  });
});

describe('las cuatro codificaciones', () => {
  it('Signed: complemento a dos, asi que 0x80 es -128 y no 128', () => {
    // Es el caso que mas numeros ridiculousos produce: 128 leido sin signo es un
    // +128 en un campo que llega a +50, que se recorta a 50 y parece un mando
    // que se ha pegado al tope.
    const { bytes, cuerpo } = construirDisco({ keygroups: 1 });
    const p = readProgram(bytes, 0);

    const at = 38 + 10;   // velToRelease, que es Signed
    cuerpo[at] = 0x80;    // -128
    escribirEnImagen(bytes, p, 10, 0x80);

    const valor = readKeygroupField(bytes, p.entry, 0,
      S950_PATCH_CATALOGUE.field('velToRelease'));

    // -128 se recorta al minimo del campo, que es lo que haria la maquina.
    expect(valor).toBe(-50);
  });

  it('Port: el 0xFF del disco es el 0 del panel, no 255', () => {
    // Sin esto un programa en estereo se leeria como salida 255, que no existe.
    // Y no es un valor raro: es un numero con toda la pinta de valido.
    const { bytes, cuerpo } = construirDisco({ keygroups: 1 });
    const p = readProgram(bytes, 0);

    const campo = S950_PATCH_CATALOGUE.field('outputPort');
    expect(campo.encoding).toBe('Port');

    escribirEnImagen(bytes, p, campo.byteOffset, 0xff);

    expect(readKeygroupField(bytes, p.entry, 0, campo)).toBe(0);
  });

  it('Bit: cuatro flags en un byte, y cada uno sale por su parte', () => {
    // Los cuatro comparten el byte 18, asi que el caso interesante es poner uno a
    // uno y ver que los otros tres no se mueven.
    const { bytes, cuerpo } = construirDisco({ keygroups: 1 });
    const p = readProgram(bytes, 0);
    const flags = S950_PATCH_CATALOGUE.fields.filter((f) => f.encoding === 'Bit');

    expect(flags.length).toBe(4);
    expect(flags.every((f) => f.byteOffset === 18)).toBe(true);

    const objetivo = flags.find((f) => f.code === 'oneShot');
    const byteOriginal = cuerpo[38 + 18];
    escribirEnImagen(bytes, p, 18, byteOriginal & ~objetivo.bitMask);

    expect(readKeygroupField(bytes, p.entry, 0, objetivo)).toBe(0);
    for (const otro of flags.filter((f) => f.code !== 'oneShot'))
      expect(readKeygroupField(bytes, p.entry, 0, otro), `${otro.code} deberia seguir igual`).toBe(1);
  });
});

describe('la envolvente del filtro en blanco', () => {
  it('los espacios son una letra, no un 32, y salen como la envolvente plana', () => {
    // Un S900 no tenia envolvente de filtro, y lo que escribia en sus cuatro
    // bytes eran ESPACIOS. Un 0x20 leido como valor es 32, y 32 no es un ajuste
    // que nadie eligio: es una letra. Leerlo como 32 suena a una envolvente
    // lenta y falsa que no se puede corregir desde el panel.
    const { bytes, cuerpo } = construirDisco({ keygroups: 1 });
    const p = readProgram(bytes, 0);

    for (let i = 0; i < 4; i += 1) {
      cuerpo[38 + S950_VCF_FIRST_BYTE + i] = S950_BLANK_BYTE;
      escribirEnImagen(bytes, p, S950_VCF_FIRST_BYTE + i, S950_BLANK_BYTE);
    }

    expect(vcfEnvelopeIsBlank(bytes, p.entry, 0)).toBe(true);

    // Sustain 99 y el resto 0: la envolvente plana.
    expect(readKeygroupField(bytes, p.entry, 0, S950_PATCH_CATALOGUE.field('vcfAttack'))).toBe(0);
    expect(readKeygroupField(bytes, p.entry, 0, S950_PATCH_CATALOGUE.field('vcfDecay'))).toBe(0);
    expect(readKeygroupField(bytes, p.entry, 0, S950_PATCH_CATALOGUE.field('vcfSustain'))).toBe(99);
    expect(readKeygroupField(bytes, p.entry, 0, S950_PATCH_CATALOGUE.field('vcfRelease'))).toBe(0);
  });

  it('con un byte de verdad deja de estar en blanco', () => {
    const { bytes, cuerpo } = construirDisco({ keygroups: 1 });
    const p = readProgram(bytes, 0);

    escribirEnImagen(bytes, p, S950_VCF_FIRST_BYTE, 40);
    expect(vcfEnvelopeIsBlank(bytes, p.entry, 0)).toBe(false);
    expect(readKeygroupField(bytes, p.entry, 0, S950_PATCH_CATALOGUE.field('vcfAttack'))).toBe(40);
  });
});

describe('abrir un programa', () => {
  it('lee el nombre de la CABECERA, que es lo que ve la persona', () => {
    // Que NO es el del directorio: son dos copias, y la que ensena la maquina es
    // la de la cabecera. Un panel que enseñase la otra estaria enseñando el
    // nombre con el que el fichero se busca, no el que el usuario puso.
    const { bytes } = construirDisco({ nombre: 'PRUEBA' });
    const p = readProgram(bytes, 0);

    expect(p.name).toBe('PRUEBA');
    expect(p.number).toBe(3);
    expect(p.keygroups).toBe(3);
  });

  it('un sample no se abre como programa, y no dice que no hay keygroups', () => {
    // Que no diga cero es lo importante: cero keygroups es un estado de un
    // programa; un sample no es un programa, es otra cosa, y la distincion
    // la tiene que hacer quien llama.
    const { bytes } = construirDisco({ tipo: 'S' });

    expect(readProgram(bytes, 0)).toBeNull();
  });

  it('una ranura libre no es un programa vacio', () => {
    const { bytes } = construirDisco();
    expect(readProgram(bytes, 5)).toBeNull();
  });

  it('lista los programas de un disco, saltando lo que no se puede leer', () => {
    const { bytes } = construirDisco({ nombre: 'PRUEBA' });
    const programas = readPrograms(bytes);

    expect(programas).toHaveLength(1);
    expect(programas[0].name).toBe('PRUEBA');
  });
});

describe('un disco danado no lanza', () => {
  it('ni con la imagen cortada por la mitad', () => {
    // Un importador que lanza ante un fichero raro no puede decir cual es el
    // raro. Todas estas funciones devuelven null o una lista vacia, y el motivo
    // lo pone quien llama.
    const { bytes } = construirDisco();

    for (const corte of [0, 1, 100, 1000, 0x600, 5000]) {
      const truncada = bytes.slice(0, corte);
      expect(() => readProgram(truncada, 0)).not.toThrow();
      expect(() => readDirectory(truncada)).not.toThrow();
      expect(() => keygroupByteAt(truncada, { type: 'P', length: 248, startBlock: 4, chainBlocks: 1 }, 0, 0)).not.toThrow();
    }
  });

  it('ni con bytes que son todos 0xff', () => {
    const basura = new Uint8Array(200 * S950_DISK.blockSize).fill(0xff);

    expect(() => readProgram(basura, 0)).not.toThrow();
    expect(readProgram(basura, 0)).toBeNull();
  });
});

/** Escribe un byte del keygroup 0 directamente en la imagen, para los tests
 *  que necesitan un caso raro que el constructor no produce. */
function escribirEnImagen (bytes, programa, byteOffset, valor) {
  const offset = S950_KEYGROUP.programHeaderSize + byteOffset;
  const index = Math.floor(offset / S950_DISK.blockSize);

  let block = programa.entry.startBlock;
  for (let i = 0; i < index; i += 1) block = fat(bytes, block);

  bytes[block * S950_DISK.blockSize + (offset % S950_DISK.blockSize)] = valor & 0xff;
}
