/**
 * El editor de patches: lee un disco real y lo pinta sin inventar una medida.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * LO QUE ESTE FICHERO PROTEGE, Y POR QUE NO ES LO DE MAS ARRIBA.
 *
 * `s950Disk.test.js` ya comprueba que el lector acierta con los bytes. Aqui lo
 * que se comprueba es que el EDITOR noicede algo con esos bytes: que ponga el
 * nombre del contrato y no uno suyo, que respete el rango del panel, y sobre
 * todo que no escriba un numero medido donde no lo hay.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * LA PARTE IMPORTANTE: LA CALIBRACION VACIA NO SE PINTA COMO UN NUMERO.
 *
 * La tabla de calibracion del S950 esta vacia a proposito —nadie ha medido esas
 * curvas, y la del estudio Mz950 que si las tiene es AGPL—. Un editor que
 * abriera un patch y escribiera "Attack: 0.0021 s" estaria inventando la cifra
 * mas precisa que puede mostrar, con la tipografia de una medida. Y no habria
 * forma de distinguirla: quien lo lee ve un numero y una unidad, que es
 * exactamente lo que hace que un numero inventado sea peor que no tener dato.
 *
 * Asi que el editor dice las dos cosas: el mando con su nombre y su rango —que
 * si son datos—, y la unidad como "sin medir". El byte es un hecho; cuantos
 * milisegundos son 55 es una conjetura, y el editor no la disfraza.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * LA TRAMPA DEL SUSTAIN.
 *
 * `vcaSustain` y `vcfSustain` van a la curva `sustainDb`, NO a `envelopeTime`, y
 * no es un detalle de agrupacion: son unidades distintas —segundos contra
 * decibelios— y la regla del cero es distinta, porque 0 dB es un nivel
 * legitimo y un tiempo de cero es un instante que no existe. Un editor que
 * metiera los sustains en la curva de tiempos daria a un sustain de 55 un
 * numero en segundos que no son segundos, y nadie lo podria ver.
 */

import { describe, expect, it } from 'vitest';

import {
  CAMPO_A_CURVA,
  abrirPrograma,
  editarKeygroup,
  editarPrograma,
  ejeDeCampo,
  valorMedidoDeCampo,
} from '../src/ui/s950PatchEditor.js';
import { S950_DISK, S950_KEYGROUP } from '../src/contracts/s950Disk.js';
import { S950_PATCH_CATALOGUE, S950_GROUPS } from '../src/contracts/s950PatchFields.js';
import { S950_CALIBRATION } from '../src/contracts/s950Calibration.js';

/**
 * Un disco minimo con un programa de un keygroup. Va a proposito simple: lo que
 * se prueba aqui es el contrato del editor, no el recorrido de la cadena, que ya
 * tiene su fichero. Aqui solo hace falta que los bytes LLEGAN.
 */
function discoDeUnKeygroup (valores = {}) {
  const bytes = new Uint8Array(S950_DISK.doubleDensityBlocks * S950_DISK.blockSize);
  const total = Math.floor(bytes.length / S950_DISK.blockSize);
  const bloque = 4;

  // Una cadena de dos bloques: 38 + 70 = 108 bytes, asi que uno basta, pero dos
  // dejan el recorrido igual de valido y sin casos raros.
  const cadena = [bloque, bloque + 1];

  for (let b = 0; b < total; b += 1) {
    const at = S950_DISK.fatOffset + b * 2;
    const v = cadena.includes (b) ? (cadena[cadena.indexOf (b) + 1] ?? S950_DISK.fatEnd) : b;
    bytes[at] = v & 0xff;
    bytes[at + 1] = (v >> 8) & 0xff;
  }

  const cuerpo = new Uint8Array (S950_KEYGROUP.programHeaderSize + S950_KEYGROUP.recordSize);
  const nombre = 'PRUEBA';
  for (let i = 0; i < nombre.length; i += 1) cuerpo[i] = nombre.charCodeAt (i);
  cuerpo[S950_KEYGROUP.countOffset] = 1;
  cuerpo[S950_KEYGROUP.programNumberOffset] = 7;

  for (const [code, valor] of Object.entries (valores)) {
    const campo = S950_PATCH_CATALOGUE.field (code);
    if (!campo) continue;

    // El `byteOffset` del catalogo es DENTRO del keygroup, y el keygroup
    // empieza tras la cabecera de 38 del programa. Sin esa suma el byte cae en
    // la cabecera, que es donde el nombre va, y el campo sale 0 sin que nada
    // falle: un 0 es un valor valido, asi que el fallo pasa inadvertido.
    const at = S950_KEYGROUP.programHeaderSize + campo.byteOffset;

    cuerpo[at] = campo.encoding === 'Port'
      ? (valor === 0 ? 0xff : valor - 1)
      : (valor < 0 ? valor + 256 : valor);
  }

  for (let i = 0; i < cuerpo.length; i += 1) {
    const b = cadena[Math.floor (i / S950_DISK.blockSize)];
    bytes[b * S950_DISK.blockSize + (i % S950_DISK.blockSize)] = cuerpo[i];
  }

  const d = 0;
  for (let i = 0; i < nombre.length; i += 1) bytes[d + i] = nombre.charCodeAt (i);
  bytes[d + 16] = 'P'.charCodeAt (0);
  const largo = cuerpo.length;
  bytes[d + 17] = largo & 0xff;
  bytes[d + 18] = (largo >> 8) & 0xff;
  bytes[d + 19] = (largo >> 16) & 0xff;
  bytes[d + 20] = bloque & 0xff;

  return bytes;
}

describe('abrir un programa', () => {
  it('abre uno valido y da su nombre, su numero y su cuenta de keygroups', () => {
    const p = abrirPrograma (discoDeUnKeygroup (), 0);

    expect(p.ok).toBe(true);
    expect(p.name).toBe('PRUEBA');
    expect(p.number).toBe(7);
    expect(p.keygroups).toBe(1);
  });

  it('no escribe: el editor es de lectura, y lo dice', () => {
    // Un editor que escribe es un importador, y un importador que se equivoca
    // destruye un patch que no se puede recuperar. Este solo pinta, y por eso
    // es seguro abrir cualquier disco. Que el `editable` sea `false` explicito
    // es lo que hace que un consumidor no lo tome por editable.
    expect(abrirPrograma (discoDeUnKeygroup (), 0).editable).toBe(false);
  });

  it('un disco vacio no es un programa sin programas', () => {
    const p = abrirPrograma (new Uint8Array (0), 0);
    expect(p.ok).toBe(false);
    expect(p.motivo).toBeTruthy();
  });

  it('una ranura libre se dice que esta libre, y no que el programa esta danado', () => {
    // Que el motivo sea el correcto importa porque es lo que se ense￱a�a. Un panel
    // que dice "esta danado" cuando lo que pasa es que no hay nada, hace que
    // quien lo mira busque un problema de datos donde lo que hay es un disco
    // con hueco.
    const p = abrirPrograma (discoDeUnKeygroup (), 5);
    expect(p.ok).toBe(false);
    expect(p.motivo).toMatch (/libre/i);
  });

  it('una ranura con un sample no dice que el programa esta danado', () => {
    const bytes = discoDeUnKeygroup ();
    bytes[16] = 'S'.charCodeAt (0);

    const p = abrirPrograma (bytes, 0);
    expect(p.ok).toBe(false);
    expect(p.motivo).toMatch (/tipo/i);
  });

  it('un programa danado explica por que, y el motivo es el calculo', () => {
    // El caso de los 60 bytes de mas, que es el error mas facil de cometer con
    // este formato. El motivo lo nombra, porque un "no se puede abrir" sin mas
    // hace que quien lo ve busque por todo el sitio menos por la cabecera.
    const bytes = discoDeUnKeygroup ();
    const largo = (38 + 70) + 60;
    bytes[17] = largo & 0xff;
    bytes[18] = (largo >> 8) & 0xff;
    bytes[19] = (largo >> 16) & 0xff;

    const p = abrirPrograma (bytes, 0);
    expect(p.ok).toBe(false);
    expect(p.motivo).toMatch (/38 \+ n \* 70/);
  });
});

describe('pintar un keygroup', () => {
  it('pone el nombre del contrato, no uno escrito aqui', () => {
    const p = abrirPrograma (discoDeUnKeygroup ({ vcaAttack: 42 }), 0);
    const k = editarKeygroup (p, 0);

    const attack = k.campos.find ((c) => c.code === 'vcaAttack');
    const delContrato = S950_PATCH_CATALOGUE.field ('vcaAttack');

    expect(attack.label).not.toBe('vcaAttack');
    expect(attack.rawName).toBe(delContrato.name);
    expect(attack.value).toBe(42);
  });

  it('el rango sale del panel, que es lo que la maquina ensena', () => {
    const p = abrirPrograma (discoDeUnKeygroup ({ vcaAttack: 42 }), 0);
    const k = editarKeygroup (p, 0);

    const attack = k.campos.find ((c) => c.code === 'vcaAttack');
    expect(attack.lo).toBe(0);
    expect(attack.hi).toBe(99);

    // Y hay un campo que NO es 0..99, para que el rango se vea venir del
    // contrato y no de un 0..99 puesto a mano en todas partes.
    const transpose = k.campos.find ((c) => c.code === 'softTranspose');
    expect(transpose.lo).toBe(-50);
    expect(transpose.hi).toBe(50);
  });

  it('los 38 campos salen, agrupados en las pestañas del panel', () => {
    const p = abrirPrograma (discoDeUnKeygroup (), 0);
    const k = editarKeygroup (p, 0);

    expect(k.ok).toBe(true);
    expect(k.campos).toHaveLength(38);
    expect(k.grupos.map ((g) => g.name)).toEqual (S950_GROUPS);
    expect(k.sinGrupo).toBe(0);
  });

  it('un keygroup que no existe no devuelve una lista a medias', () => {
    // Un keygroup con un hueco en medio se ve como un 0 en el hueco, y un 0 en
    // un ataque es un click. Cortar aqui es lo unico que no engana.
    const p = abrirPrograma (discoDeUnKeygroup (), 0);
    const k = editarKeygroup (p, 3);

    expect(k.ok).toBe(false);
    expect(k.campos).toEqual ([]);
  });
});

describe('la calibracion, que esta vacia y se dice', () => {
  it('las seis curvas estan declaradas y ninguna medida', () => {
    // El punto de partida, y el que hace que todo lo de abajo tenga sentido.
    expect(S950_CALIBRATION).toBeTruthy();
    expect(S950_CALIBRATION.curves).toHaveLength(6);

    for (const c of S950_CALIBRATION.curves)
      expect(c.measured, `${c.code} no deberia estar medida`).toBe(false);
  });

  it('el editor NO escribe un valor medido, y eso es lo que se comprueba', () => {
    // El numero que hay en el byte es un 55. Los milisegundos que son 55 no lo
    // sabe nadie. El editor tiene que devolver null, no un numero: un 0 seria
    // un instante, y una interpolacion del rango seria una conjetura con la
    // tipografia de una medida.
    const p = abrirPrograma (discoDeUnKeygroup ({ vcaAttack: 55 }), 0);
    const k = editarKeygroup (p, 0);

    const attack = k.campos.find ((c) => c.code === 'vcaAttack');

    expect(attack.value).toBe(55);                                  // el byte, si
    expect(attack.measured).toBeNull();                            // la medida, no
    expect(valorMedidoDeCampo ('vcaAttack', 55)).toBeNull();
  });

  it('el eje se marca sinMedir, con su unidad, y no se oculta', () => {
    // Que se diga "sin medir" y no "nada" es lo que permite a un panel pintar la
    // diferencia. Ocultar el eje seria tan inventado como escribir el numero.
    const eje = ejeDeCampo ('vcaAttack');

    expect(eje.curva).toBe('envelopeTime');
    expect(eje.unidad).toBe('s');
    expect(eje.estado).toBe('sinMedir');
    expect(eje.logarithmic).toBe(true);
  });

  it('un campo sin curva no recibe un eje parecido', () => {
    // Un transpose no es una magnitud escalable con una unidad. Buscaŕsele una
    // por parecido es como se inventan los minimos que no existen.
    expect(ejeDeCampo ('softTranspose')).toBeNull();
    expect(ejeDeCampo ('lowKey')).toBeNull();
    expect(ejeDeCampo ('outputPort')).toBeNull();
  });

  it('el panel puede distinguir un eje sin medir de uno medido', () => {
    // El contrato tiene que poder pintar las dos cosas sin ambiguedad, porque
    // hasta que alguien mida van a ser casi todos sin medir y ese es el estado
    // que hay que hacer legible.
    const p = abrirPrograma (discoDeUnKeygroup (), 0);
    const k = editarKeygroup (p, 0);

    const conEje = k.campos.filter ((c) => c.axis !== null);
    const sinEje = k.campos.filter ((c) => c.axis === null);

    expect(conEje.length).toBeGreaterThan(0);
    expect(sinEje.length).toBeGreaterThan(0);
    expect(conEje.every ((c) => c.axis.estado === 'sinMedir')).toBe(true);
  });
});

describe('el mapa campo -> curva', () => {
  it('mete los envelopes en envelopeTime y los sustains en sustainDb', () => {
    // La distincion de la cabecera, comprobada: son curvas distintas con unidades
    // distintas, y un sustain metido en la curva de tiempos daria un numero en
    // segundos que no son segundos.
    expect(CAMPO_A_CURVA.vcaAttack).toBe('envelopeTime');
    expect(CAMPO_A_CURVA.vcaDecay).toBe('envelopeTime');
    expect(CAMPO_A_CURVA.vcaRelease).toBe('envelopeTime');
    expect(CAMPO_A_CURVA.vcfAttack).toBe('envelopeTime');
    expect(CAMPO_A_CURVA.vcaSustain).toBe('sustainDb');
    expect(CAMPO_A_CURVA.vcfSustain).toBe('sustainDb');
  });

  it('cada curva que el mapa nombra existe en el contrato', () => {
    // Si el mapa dice una curva que el contrato no tiene, el eje sale `null` y
    // el campo se queda sin unidad sin decir por que. Esto lo hace ruido.
    const codigos = new Set (S950_CALIBRATION.curves.map ((c) => c.code));

    for (const [campo, curva] of Object.entries (CAMPO_A_CURVA))
      expect(codigos.has (curva), `${campo} apunta a una curva que no existe: ${curva}`).toBe(true);
  });

  it('toda curva de calibracion tiene al menos un campo que la use', () => {
    // Al reves: si una curva se queda sin campos, no es que este mal mapeada, es
    // que hay una curva que el panel no sabe pintar. Hoy la de filtro.
    const usadas = new Set (Object.values (CAMPO_A_CURVA));
    for (const c of S950_CALIBRATION.curves)
      expect(usadas.has (c.code), `la curva ${c.code} no la usa ningun campo`).toBe(true);
  });
});

describe('editar un programa entero', () => {
  it('devuelve un keygroup por cada uno que tiene', () => {
    const bytes = discoDeUnKeygroup ();
    // Subir la cuenta a tres sin rehacer el disco: se engaña al directorio.
    bytes[S950_KEYGROUP.programNumberOffset] = 0;
    const largo = 38 + 3 * 70;
    bytes[17] = largo & 0xff;
    bytes[18] = (largo >> 8) & 0xff;
    bytes[19] = (largo >> 16) & 0xff;

    const p = abrirPrograma (bytes, 0);
    const e = editarPrograma (p);

    expect(e.total).toBe(3);
    expect(e.keygroups).toHaveLength(3);
  });

  it('el resultado lleva el nombre del programa, que es el de la cabecera', () => {
    const p = abrirPrograma (discoDeUnKeygroup (), 0);
    const e = editarPrograma (p);

    expect(e.name).toBe('PRUEBA');
    expect(e.number).toBe(7);
  });
});
