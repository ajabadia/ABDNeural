/**
 * El camino completo desde la WebUI a las curvas de calibracion del motor.
 *
 * Lo que se prueba aqui no es el modulo del paquete compartido —eso ya esta
 * probado alla, con 44 comprobaciones— sino QUE ESTA RUTA FUNCIONA: que el
 * JSON se exporta por la ruta que se importa, que el barrel expone el indice, y
 * que un panel de esta WebUI puede pedir un eje y saber, sin inventar nada, si
 * ese eje se puede pintar.
 *
 * Un `import` de un JSON que no esta exportado no falla al compilar en
 * cualquier sitio: falla en runtime, en el navegador, cuando alguien abre la
 * pagina. Este test es lo que convierte eso en un rojo en el CI.
 */

import { describe, it, expect } from 'vitest';

import {
  S950_CALIBRATION,
  S950_CALIBRATION_CONTRACT,
  S950_COVERAGE,
  S950_AXES,
  S950_UNITS,
  s950Curve,
  s950Axis,
  s950AxesWithoutVertical,
  s950ValueAt,
} from '../src/contracts/s950Calibration.js';

describe('las curvas de calibracion S950 llegan a la WebUI', () => {
  it('el JSON se importa y trae las seis curvas, en el orden del enum', () => {
    expect(S950_CALIBRATION).not.toBeNull();
    expect(S950_CALIBRATION.curves).toHaveLength(6);
    expect(S950_CALIBRATION_CONTRACT.curves).toHaveLength(6);

    // El ORDEN es parte del contrato, no una preferencia estetica: el motor
    // indexa los puntos medidos con el entero del enum, asi que una tabla
    // reordenada no da ningun fallo en C++ y lo unico que pasa es que cada
    // curva dibuja los puntos de otra.
    expect(S950_CALIBRATION.curves.map((c) => c.code)).toEqual([
      'envelopeTime', 'lfoRate', 'warpTime', 'filterCutoff', 'filterEnvOctaves', 'sustainDb',
    ]);
  });

  it('el contrato dice de donde sale, para que un desfase tenga donde mirar', () => {
    expect(S950_CALIBRATION_CONTRACT.sourceOfTruth)
      .toBe('ABDSharedCode/SynthCore/S950Calibration.h');
    // Y dice que NO tiene ni un valor medido, en el propio dato y no solo en un
    // comentario: es la respuesta que un panel pinta.
    for (const curve of S950_CALIBRATION.curves) {
      expect(curve.measured).toBe(false);
      expect(curve.pointCount).toBe(0);
      expect(curve.points).toEqual([]);
      expect(curve.measuredRange).toBeNull();
    }
  });

  it('un panel puede pedir el eje de una curva por su codigo', () => {
    const attack = s950Axis('envelopeTime');
    expect(attack.code).toBe('envelopeTime');
    expect(attack.lo).toBe(0);
    expect(attack.hi).toBe(99);
    expect(attack.span).toBe(99);
    // Y lo que un eje de TIEMPO tiene que decir: escala log, no sube con el
    // byte, y el cero es un instante —un click— y no un valor.
    expect(attack.scale).toBe('log');
    expect(attack.rises).toBe(false);
    expect(attack.excludeZero).toBe(true);

    expect(s950Axis('noExiste')).toBeNull();
    expect(s950Curve('noExiste')).toBeUndefined();
  });

  it('los seis ejes se pintan enteros, con la forma que un panel necesita', () => {
    expect(S950_AXES).toHaveLength(6);
    for (const axis of S950_AXES) {
      expect(axis.code, 'eje con codigo').toBeTruthy();
      expect(typeof axis.lo).toBe('number');
      expect(typeof axis.hi).toBe('number');
      expect(axis.span).toBe(axis.hi - axis.lo);
      expect(axis.lo).toBeLessThanOrEqual(axis.hi);
      expect(['log', 'linear']).toContain(axis.scale);
      expect(typeof axis.rises).toBe('boolean');
      expect(typeof axis.excludeZero).toBe('boolean');
      expect(axis.label, `${axis.code} con etiqueta de eje`).toBeTruthy();
    }
  });

  it('el codigo de cada eje es el SUYO, y no el de la curva de al lado', () => {
    // El emparejamiento se hace antes de tirar nulos, no por indice despues: si
    // se emparejara por posicion, el fallo seria un ataque con la etiqueta de
    // un sustain y no daria ningun error.
    const esperado = S950_CALIBRATION.curves.map((c) => c.code);
    expect(S950_AXES.map((a) => a.code)).toEqual(esperado);
  });

  it('CUATRO de los seis ejes verticales NO se pueden pintar, y se dice cuales', () => {
    // El numero que le importa a quien esta pintando. Un eje en escala log
    // necesita un minimo REAL, y el minimo real es un valor medido: sin el,
    // `Math.log(0)` es -Infinity y la curva se va a menos infinito.
    const sinVertical = s950AxesWithoutVertical();
    expect(sinVertical.map((c) => c.code)).toEqual([
      'envelopeTime', 'warpTime', 'filterCutoff', 'sustainDb',
    ]);

    // Y las dos que SI se pueden pintar son las lineales, que con un minimo
    // nominal ya estan —aunque no haya ni un punto medido que dibujar.
    const conVertical = S950_AXES.filter((a) => !a.needsMeasuredMinimum);
    expect(conVertical.map((a) => a.code)).toEqual(['lfoRate', 'filterEnvOctaves']);
    expect(conVertical.every((a) => a.scale === 'linear')).toBe(true);
  });

  it('poder pintar el eje NO es lo mismo que haberlo medido', () => {
    // El VCF amount se puede pintar y sigue sin medirse. Un panel que
    // confundiera las dos cosas dibujaria una octava de trazado, que es un
    // numero con toda la pinta de un dato.
    const octaves = s950Curve('filterEnvOctaves');
    expect(s950Axis('filterEnvOctaves').needsMeasuredMinimum).toBe(false);
    expect(octaves.measured).toBe(false);
  });

  it('el valor de una curva es SIEMPRE null, y el 0 no puede colarse', () => {
    // La razon de que esto sea un modulo y no un hueco: 0 s de attack NO es
    // "no medido", es un ataque instantaneo, que es un click. Y 0 es un numero,
    // asi que un `valueAt(...) || 0` en el panel lo volveria indistinguible de
    // un dato. Aqui se prueba para las seis y para un codigo que no existe.
    for (const curve of S950_CALIBRATION.curves) {
      expect(s950ValueAt(curve.code, curve.storedLo)).toBeNull();
      expect(s950ValueAt(curve.code, curve.storedHi)).toBeNull();
      expect(s950ValueAt(curve.code, 0)).toBeNull();
    }
    expect(s950ValueAt('noExiste', 0)).toBeNull();
  });

  it('el panel puede ENSEÑAR que no lo sabe, con un rotulo y no con un error', () => {
    // `valueAt()` devuelve null y aun asi el panel tiene algo que pintar: la
    // cuenta de lo medido. Sin esto, "sin medir" se parecería a "no hay datos".
    expect(S950_COVERAGE.measured).toBe(0);
    expect(S950_COVERAGE.total).toBe(6);
    expect(S950_COVERAGE.complete).toBe(false);
    expect(S950_COVERAGE.label).toContain('Ninguna curva medida');
    expect(S950_CALIBRATION.unmeasured()).toHaveLength(6);
    expect(S950_CALIBRATION.measuredCount()).toBe(0);
  });

  it('la unidad del eje vertical es de la lista cerrada', () => {
    // Un "s" mal escrito como "segundos" haria la etiqueta el doble de ancha en
    // un eje estrecho, y el error se veria solo mirandolo.
    for (const curve of S950_CALIBRATION.curves) {
      expect(S950_UNITS, `${curve.code} con unidad`).toContain(curve.unit);
    }
    expect(S950_UNITS).toEqual(['s', 'Hz', 'octaves', 'dB']);
  });
});
