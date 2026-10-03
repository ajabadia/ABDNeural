/**
 * El camino completo desde la WebUI al catálogo del motor.
 *
 * Lo que se prueba aquí no es el componente —eso ya está probado en
 * `@abdsynths/shared`— sino QUE ESTA RUTA FUNCIONA: que el paquete compartido
 * exporta el JSON por la ruta que dice, que el barrel expone el índice, y que
 * un panel de esta WebUI puede pedir una pestaña y pintar con ella.
 *
 * Un `import` de un JSON que no está exportado no falla al compilar en
 * cualquier sitio: falla en runtime, en el navegador, cuando alguien abre la
 * página. Este test es lo que convierte eso en un rojo en el CI.
 */

import { describe, it, expect } from 'vitest';

import {
  S950_PATCH_CATALOGUE,
  S950_PATCH_CONTRACT,
  s950Field,
  s950Page,
  isS950Bipolar,
} from '../src/contracts/s950PatchFields.js';
import { S950_ENCODINGS, S950_GROUPS } from '@abdsynths/shared/components';

describe('el catalogo S950 llega a la WebUI', () => {
  it('el JSON se importa y trae los 38 campos', () => {
    expect(S950_PATCH_CATALOGUE).not.toBeNull();
    expect(S950_PATCH_CATALOGUE.fields).toHaveLength(38);
    expect(S950_PATCH_CONTRACT.fields).toHaveLength(38);
  });

  it('el catalogo dice de donde sale, para que un desfase tenga donde mirar', () => {
    expect(S950_PATCH_CONTRACT.sourceOfTruth)
      .toBe('ABDSharedCode/SynthCore/S950PatchFields.h');
  });

  it('un campo se busca por su codigo, que es como lo llama un patch', () => {
    expect(s950Field('softFine').hi).toBe(255);
    expect(s950Field('noExiste')).toBeUndefined();
  });

  it('las pestañas del panel se piden por su nombre', () => {
    expect(S950_GROUPS).toContain('FILTER');
    expect(s950Page('FILTER').length).toBeGreaterThan(0);
    expect(s950Page('PAGINA_INVENTADA')).toEqual([]);
  });

  it('la pagina trae el nombre ya formateado y el rango, que es lo que se pinta', () => {
    const page = s950Page('FILTER');
    for (const row of page) {
      expect(row.label.length, `${row.code} con etiqueta`).toBeGreaterThan(0);
      expect(row.lo).toBeLessThanOrEqual(row.hi);
      expect(typeof row.bipolar).toBe('boolean');
    }
    // El nombre CRUDO es el de la maquina, y el de pintar no: esa traduccion
    // ocurre aqui, no en el contrato.
    expect(S950_PATCH_CATALOGUE.field('outputPort').name).toBe('Output');
  });

  it('el bipolar se deriva del rango, asi que el fine no sale bipolar por llegar a 255', () => {
    // El unico campo con `hi` > 99 que no es bipolar, y el caso que hace
    // necesaria la DERIVACION: si `bipolar` fuera una columna del JSON,
    // alguien la pondria a true aqui por costumbre y el mando se centraria
    // sobre un valor que nunca es negativo.
    //
    // La primera version de este test lo miraba en la pagina de FILTER, y
    // softFine no esta ahi: es del grupo TUNING. Un `find` que devuelve
    // undefined y un `.bipolar` sobre el daba undefined, que era lo que
    // hacia que la comprobacion pasara sin comprobar nada.
    expect(s950Field('softFine').hi).toBe(255);
    expect(isS950Bipolar(s950Field('softFine'))).toBe(false);
    expect(isS950Bipolar(s950Field('vcfAmount'))).toBe(true);

    // Y en la pagina donde si vive, el valor derivado sale ya calculado.
    const tuning = s950Page('TUNING');
    expect(tuning.find((r) => r.code === 'softFine').bipolar).toBe(false);
    expect(s950Page('FILTER').find((r) => r.code === 'vcfAmount').bipolar).toBe(true);
  });

  it('un flag no es bipolar por tener rango 0..1', () => {
    const oneShot = s950Field('oneShot');
    expect(oneShot.encoding).toBe(S950_ENCODINGS.BIT);
    expect(oneShot.hi).toBe(1);
  });

  it('dice que campos comparten byte, que es lo que obliga a leer-modificar', () => {
    // La pregunta que hay que hacer ANTES de escribir en un byte. Un panel que
    // solo pinte no la necesita, pero un editor de patches si, y es la razon de
    // que el byte 18 salga con sus cuatro flags juntos en vez de por separado.
    expect(S950_PATCH_CATALOGUE.sharingByte(18)).toHaveLength(4);
  });

  it('los trims que mueven un campo se encuentran desde el campo', () => {
    expect(S950_PATCH_CATALOGUE.trimFor('softFilter').code).toBe('vcfCutoff');
    // Un campo sin trim es una respuesta, no un fallo: significa "este campo no
    // lo mueve ningun offset de Perform".
    expect(S950_PATCH_CATALOGUE.trimFor('softFine')).toBeUndefined();
  });
});
