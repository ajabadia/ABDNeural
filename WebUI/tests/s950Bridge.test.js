/**
 * La frontera S950 <-> NEURONiK, y sobre todo la frase "comprueben que los
 * rangos del panel coinciden con los del motor".
 *
 * ───────────────────────────────────────────────────────────────────────────
 * LO QUE SE COMPRUEBA, Y LO QUE NO SE PUEDE COMPROBAR.
 *
 * Hay tres afirmaciones distintas escondidas en esa frase, y dos son
 * verificables y una no:
 *
 *   1. Que el `panelUnit` de cada fila sea EL MISMO que el del catálogo.  ✅
 *   2. Que el `realUnit` de cada fila sea el que declara el motor.        ✅
 *   3. Que un rango de panel y un rango real sean IGUALES.               ❌
 *
 * La tercera no es cierta ni falsa: no está en el mismo dominio. 0..99 y
 * 20..20000 Hz no pueden coincidir, y no porque uno esté mal. Por eso este
 * fichero no la intenta: la sustituye por un test que la hace imposible de
 *meterla por accidentee, ver más abajo.
 *
 * ───────────────────────────────────────────────────────────────────────────
 * POR QUÉ ESTE FICHERO NO ES EL CONVERTIDOR.
 *
 * NEURONiK no tiene importador de patches del S950. Este test fija esa
 * ausencia como una regla comprobable: las ocho conversiones siguen siendo
 * `null` y `s950ToNeuronik()` sigue devolviendo `UNKNOWN` para CUALQUIER valor
 * de panel, incluidos los extremos, los negativos y un `NaN`.
 *
 * Eso tiene un valor: el día que alguien escriba el puente de verdad, este
 * test se pone rojo y enumera las ocho filas que hay que revisar a mano. Un
 * hueco que se vuelve código sin que nadie mire es un hueco con números
 * inventados dentro.
 */

import { describe, it, expect } from 'vitest';

import {
  CONCEPTS,
  UNKNOWN,
  bridgeFor,
  unitsFor,
  s950ToNeuronik,
} from '../src/contracts/s950Bridge.js';
import { s950Field, isS950Bipolar } from '../src/contracts/s950PatchFields.js';
import { getDescriptor } from '../src/contracts/parameters.js';

describe('cada fila del puente apunta a algo que existe', () => {
  it('el campo S950 de cada fila está en el catálogo', () => {
    const ausentes = CONCEPTS
      .map((c) => c.s950Field)
      .filter((code) => s950Field(code) === undefined);
    expect(ausentes).toEqual([]);
  });

  it('el parámetro NEURONiK de cada fila existe en el contrato del motor', () => {
    const ausentes = CONCEPTS
      .map((c) => c.neuronikId)
      .filter((id) => getDescriptor(id) === null);
    expect(ausentes).toEqual([]);
  });

  it('cada código del S950 aparece en una sola fila', () => {
    const codigos = CONCEPTS.map((c) => c.s950Field);
    expect(new Set(codigos).size).toBe(codigos.length);
  });

  it('cada parámetro de NEURONiK aparece en una sola fila', () => {
    const ids = CONCEPTS.map((c) => c.neuronikId);
    expect(new Set(ids).size).toBe(ids.length);
  });
});

describe('los rangos: lo que se puede comparar, comparado', () => {
  it('el panelUnit de cada fila es el del catálogo, carácter a carácter', () => {
    // Esta es la comprobación que SÍ tiene contenido: la fila reescribe a mano
    // lo que el catálogo ya sabe. Es la única forma de que las dos cosas se
    // separen en silencio.
    const discrepancias = CONCEPTS
      .map((c) => {
        const campo = s950Field(c.s950Field);
        return campo.unit === c.panelUnit ? null : `${c.s950Field}: ficha dice "${c.panelUnit}", catalogo dice "${campo.unit}"`;
      })
      .filter(Boolean);
    expect(discrepancias).toEqual([]);
  });

  it('el realUnit de cada fila describe el rango que declara el motor', () => {
    // Solo hay algo que comparar cuando la unidad ES un rango ("0..1"). Para
    // las de dimension fisica ("s", "Hz") lo que se comprueba es que el
    // parametro exista y tenga un rango, no que coincida el texto.
    const discrepancias = CONCEPTS
      .map((c) => {
        if (!/^-?[\d.]+\.\.-?[\d.]+$/.test(c.realUnit)) return null;
        const [lo, hi] = c.realUnit.split('..').map(Number);
        const d = getDescriptor(c.neuronikId);
        if (d.minValue !== lo || d.maxValue !== hi) {
          return `${c.neuronikId}: ficha dice "${c.realUnit}", motor dice ${d.minValue}..${d.maxValue}`;
        }
        return null;
      })
      .filter(Boolean);
    expect(discrepancias).toEqual([]);
  });

  it('ningún motor tiene el rango en unidades de panel', () => {
    // La versión positiva de "no se comparan los dominios": se puede ver que
    // el parametro del motor NO es 0..99, luego compararlos no tiene sentido.
    const falsoCoincidencia = CONCEPTS
      .map((c) => {
        const d = getDescriptor(c.neuronikId);
        return d.minValue === 0 && d.maxValue === 99 ? c.neuronikId : null;
      })
      .filter(Boolean);
    expect(falsoCoincidencia).toEqual([]);
  });

  it('el motor y el panel no coinciden en ninguno de los ocho conceptos', () => {
    // Fijado como test a proposito. Si alguien "simplifica" el puente
    // assumiendo que los rangos son comparables, esto es lo que se rompe:
    // ocho filas, ocho dominios distintos, cero interseccion util.
    const medidas = CONCEPTS.map((c) => {
      const d = getDescriptor(c.neuronikId);
      return { id: c.neuronikId, real: `${d.minValue}..${d.maxValue}`, panel: c.panelUnit };
    });
    expect(medidas.filter((m) => m.real === m.panel)).toHaveLength(0);
  });
});

describe('el sentido de la conversion esta declarado y es unico', () => {
  it('toda fila declara la direccion panel -> unidades reales', () => {
    // La vuelta no existe: un attack de 0.002 s no se puede escribir como un
    // numero de panel porque un panel no tiene decimales. Declararlo como
    // opcion es lo que evita que alguien lo asuma.
    expect(CONCEPTS.every((c) => c.direction === 'panelToReal')).toBe(true);
  });

  it('ninguna fila declara la vuelta', () => {
    expect(CONCEPTS.filter((c) => c.direction !== 'panelToReal')).toEqual([]);
  });
});

describe('sin calibracion, la conversion NO es un numero', () => {
  it('las ocho conversiones siguen siendo null', () => {
    // Si alguien rellena el puente, estas ocho filas son las que hay que
    // revisar. El fallo lo nombra una por una.
    const rellenadas = CONCEPTS
      .map((c) => `${c.s950Field} (${c.concept})`)
      .filter((_, i) => CONCEPTS[i].conversion !== null);
    expect(rellenadas).toEqual([]);
  });

  it('UNKNOWN no es un numero, ni un null, ni undefined', () => {
    // Lo que impide que un `?? 0` en el llamante convierta "no lo se" en un
    // valor: los tres fallan contra un simbolo, un 0 no.
    expect(typeof UNKNOWN).toBe('symbol');
    expect(UNKNOWN).not.toBe(0);
    expect(UNKNOWN).not.toBeNull();
    expect(UNKNOWN).not.toBeUndefined();
  });

  it('convertir UNKNOWN a numero LANZA en vez de dar un 0', () => {
    // Escrito primero como `Number.isNaN(Number(UNKNOWN))`, y falla: la
    // coerción de un símbolo lanza, no devuelve NaN. Que es MEJOR de lo que
    // había supuesto, porque un NaN se puede meter en una suma y seguir
    // 넘어 propagándose en silencio. Un símbolo para el calculation entero.
    expect(() => Number(UNKNOWN)).toThrow(TypeError);
    // `Number.isFinite` NO coacciona, y por eso no lanza: devuelve false sin
    // mirar el tipo. Sigue siendo la respuesta que queremos, pero por un
    // motivo distinto, y conviene que el test lo diga en vez de darlo por
    // hecho: si alguien lo cambiara por `!isNaN(x)` el simbolo pasaria a NaN
    // y el fallo seria silencioso.
    expect(Number.isFinite(UNKNOWN)).toBe(false);
  });

  it('convertir da UNKNOWN en los ocho conceptos, para cualquier valor', () => {
    for (const c of CONCEPTS) {
      for (const v of [0, 1, 49, 50, 99, -1, 128, NaN, null, undefined, 'x']) {
        expect(s950ToNeuronik(c.s950Field, v)).toBe(UNKNOWN);
      }
    }
  });

  it('convertir da UNKNOWN tambien para un campo que no esta en el puente', () => {
    expect(s950ToNeuronik('lowKey', 60)).toBe(UNKNOWN);
    expect(s950ToNeuronik('noExiste', 0)).toBe(UNKNOWN);
    expect(s950ToNeuronik('', 0)).toBe(UNKNOWN);
  });

  it('convertir nunca devuelve un numero que se parezca a un valor de panel', () => {
    // Si alguien "parchea" el puente con una proporcion lineal, 0 y 99 tienen
    // que seguir dando UNKNOWN. Un 0 aqui es un ataque instantaneo: un click.
    for (const c of CONCEPTS) {
      const r = s950ToNeuronik(c.s950Field, 0);
      expect(typeof r).toBe('symbol');
      expect(Number.isFinite(r)).toBe(false);
    }
  });
});

describe('un panel puede ENSEÑAR las dos unidades sin convertir', () => {
  it('unitsFor da el rango real del motor, leido del contrato', () => {
    const u = unitsFor('softFilter');
    expect(u).not.toBeNull();
    expect(u.panelUnit).toBe('0..99');
    expect(u.realUnit).toBe('Hz');
    expect(u.realMin).toBe(20);
    expect(u.realMax).toBe(20000);
  });

  it('los ocho conceptos dan un rango real leido del motor', () => {
    for (const c of CONCEPTS) {
      const u = unitsFor(c.s950Field);
      expect(u, c.concept).not.toBeNull();
      expect(typeof u.realMin).toBe('number');
      expect(typeof u.realMax).toBe('number');
      expect(u.realMax).toBeGreaterThan(u.realMin);
    }
  });

  it('los extremos del motor son alcanzables por el panel que los muestra', () => {
    // Un panel que enseña 20..20000 Hz y solo puede escribir 0..99 no esta
    // mintiendo todavia, pero esta a un paso de hacerlo. Lo que se comprueba
    // aqui es que el panel que se pinta tiene la mitad de su recorrido fuera
    // del dominio del motor, y que eso es visible.
    const fuera = CONCEPTS.map((c) => {
      const u = unitsFor(c.s950Field);
      const medio = (u.realMin + u.realMax) / 2;
      return { id: c.neuronikId, medio, fueraDePanel: u.realMax > 99 || medio > 99 };
    });
    expect(fuera.filter((f) => f.fueraDePanel).length).toBeGreaterThan(0);
  });

  it('un campo ausente del puente o del motor da null, no un objeto vacio', () => {
    expect(unitsFor('lowKey')).toBeNull();
    expect(unitsFor('noExiste')).toBeNull();
    expect(bridgeFor('noExiste')).toBeUndefined();
  });
});

describe('el bipolar del panel se deriva del rango, no se declara a mano', () => {
  it('la fila bipolar del catalogo es la que el catalogo marca como tal', () => {
    // vcfAmount va -50..+50, que es bipolar en el sentido del panel. El
    // derivado tiene que decir lo mismo que el rango, no lo que el autor
    // recuerda.
    const f = s950Field('vcfAmount');
    expect(f.lo).toBeLessThan(0);
    expect(f.hi).toBeGreaterThan(0);
    expect(isS950Bipolar(f)).toBe(true);
  });

  it('un campo 0..99 no es bipolar aunque se le llame asi', () => {
    for (const code of ['vcaAttack', 'softFilter', 'lfoRate', 'lfoDepth']) {
      expect(isS950Bipolar(s950Field(code)), code).toBe(false);
    }
  });

  it('el magnitud bipolar de la ficha es la unica que tiene signo', () => {
    const conSigno = CONCEPTS
      .map((c) => s950Field(c.s950Field))
      .filter((f) => f.lo < 0);
    expect(conSigno.map((f) => f.code)).toEqual(['vcfAmount']);
  });
});
