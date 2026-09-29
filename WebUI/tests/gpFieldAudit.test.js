/**
 * EL AVISO DE LOS CAMPOS QUE EL MOTOR NO PUBLICA
 * (`public/worklet/gpFieldAudit.js`).
 *
 * El fallo que se corrige con esto: `writeGpField` se come en silencio un
 * campo que el layout no publica. Con el `.wasm` viejo, que publicaba 34
 * campos frente a los 40 que la pagina escribe, los seis mandos del hueco se
 * movian en la pagina y no llegaban al motor. Ni error, ni rojo, ni sonido:
 * seis knobs muertos.
 *
 * Y el otro lado, que es el que da forma al aviso: con un snapshot de 40
 * campos, avisar en cada escritura son seis mensajes para un solo
 * `postMessage`, y repetirlo en cada push llenaria la consola de la pagina.
 * Por eso avisa UNA vez y solo si la lista ha crecido.
 *
 * Aqui solo se cuenta y se decide si hay que avisar. A quien se le avisa es
 * de quien llama, y asi lo prueban los otros ficheros.
 */

import { describe, expect, it } from 'vitest';

import { createFieldAudit } from '../public/worklet/gpFieldAudit.js';

describe('gpFieldAudit / createFieldAudit', () => {
  it('sin campos ausentes no hay aviso, y no lo inventa', () => {
    const audit = createFieldAudit (() => 58);

    expect(audit.takeReport ()).toBeNull();
  });

  it('anota los campos y los da ORDENADOS, no en el orden que llegaron', () => {
    // Ordenados, porque el aviso va a una consola que lee una persona: los
    // seis ids del hueco en orden dicen algo; en el orden en que los pidio
    // el ultimo push, no.
    const audit = createFieldAudit (() => 58);

    audit.note (39);
    audit.note (34);
    audit.note (38);

    expect(audit.list ()).toEqual ([34, 38, 39]);
  });

  it('el MISMO campo anotado dos veces cuenta una vez', () => {
    // Con un `.wasm` viejo, cada push pide los mismos seis. Sin el `Set`,
    // el aviso diria "doce campos que no publica" para seis de verdad.
    const audit = createFieldAudit (() => 34);

    audit.note (34);
    audit.note (34);
    audit.note (34);

    expect(audit.size).toBe(1);
  });

  it('el aviso sale UNA vez por push, y el segundo push callado', () => {
    // El aviso repetido en cada push llena la consola de la pagina cada vez
    // que alguien mueve un knob. Los campos que faltan no cambian con el
    // tiempo: el contrato no cambia.
    const audit = createFieldAudit (() => 34);

    audit.note (34);
    const primero = audit.takeReport ();

    expect(primero).not.toBeNull();
    expect(primero.missingCount).toBe(1);
    expect(audit.takeReport ()).toBeNull();
    expect(audit.takeReport ()).toBeNull();
  });

  it('un campo NUEVO vuelve a avisar, y el aviso lleva la lista entera', () => {
    // Un campo mas que antes no es un aviso mas: es un binario distinto, y
    // el aviso tiene que decir cuales son TODOS, no solo el nuevo.
    const audit = createFieldAudit (() => 34);

    audit.note (34);
    expect(audit.takeReport ().fields).toEqual ([34]);

    audit.note (39);
    const segundo = audit.takeReport ();

    expect(segundo.fields).toEqual ([34, 39]);
    expect(segundo.missingCount).toBe(2);
  });

  it('el aviso lleva cuantos campos publica el motor', () => {
    // El numero es lo que dice si el binario es viejo o si la pagina
    // escribe campos que ya no existen: son dos arreglos distintos.
    const audit = createFieldAudit (() => 34);

    audit.note (34);
    expect(audit.takeReport ().fieldCount).toBe(34);
  });

  it('la cuenta del motor se lee al avisar, no se guarda al construir', () => {
    // El layout se lee una vez al arrancar, pero el numero se pide como
    // FUNCION para que el aviso no lleve una copia guardada: una copia
    // guardada es una verdad mas que se puede quedar vieja.
    let cuenta = 34;
    const audit = createFieldAudit (() => cuenta);

    audit.note (40);
    cuenta = 58;
    audit.note (41);
    audit.note (42);

    const report = audit.takeReport ();
    expect(report.fieldCount).toBe(58);
  });

  it('el caso de HOY: seis mandos del hueco, un aviso, seis ids', () => {
    // Los seis que se perdian con el `.wasm` de 34 campos. Se escribe como
    // estaba, porque un test que invente numeros nuevos no demuestra que
    // el caso real esta cubierto.
    const audit = createFieldAudit (() => 34);

    for (const campo of [34, 35, 36, 37, 38, 39]) audit.note (campo);

    const report = audit.takeReport ();

    expect(report.missingCount).toBe(6);
    expect(report.fields).toEqual ([34, 35, 36, 37, 38, 39]);
    expect(report.fieldCount).toBe(34);
    // Y el segundo push, el de mover un knob, no dice nada.
    expect(audit.takeReport ()).toBeNull();
  });
});
