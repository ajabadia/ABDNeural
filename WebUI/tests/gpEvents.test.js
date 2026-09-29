/**
 * LA TRADUCCION DE LOS EVENTOS (`public/worklet/gpEvents.js`).
 *
 * Lo que se prueba es aritmetica pura: el recorte a 14 bits de la velocidad
 * y del pitch bend, y el relleno del POD. Antes vivia dentro de
 * `stageEvents`, en un `AudioWorkletProcessor` que se registra al importarse:
 * no se podia ni mirar. Un error ahi no da excepcion, suena mal --una
 * velocidad de 1.0 pasada como 16383 donde el motor espera 8191, un pitch
 * bend cuyo centro cae en 0-- y eso no lo caza nadie.
 *
 * El POD es `Runtime::Event`, 24 bytes, static_assert'ado en el puente:
 *   [0] i32 type  [1] i32 channel  [2] i32 note  [3] i32 value14
 *   [4] f32 value [5] i32 sampleOffset
 */

import { describe, expect, it } from 'vitest';

import {
  DEFAULT_NOTE,
  EVENT_CHANNEL,
  EVENT_TYPE,
  eventFromMessage,
} from '../public/worklet/gpEvents.js';

describe('gpEvents / eventFromMessage', () => {
  it('una nota entra con su numero y su velocidad a 14 bits', () => {
    const evento = eventFromMessage({ kind: 'noteOn', note: 64, velocity: 0.5 });

    expect(evento.type).toBe(EVENT_TYPE.NOTE_ON);
    expect(evento.note).toBe(64);
    expect(evento.value).toBe(0.5);
    // 0.5 de 16383: el centro exacto del rango, que es lo que hace que una
    // velocidad de 0.5 suene a 0.5 y no a 0.49997.
    expect(evento.value14).toBe(8192);
  });

  it('la velocidad SE RECORTA a 14 bits en vez de desbordar', () => {
    // Sin el recorte, 1.0 * 16383 da 16383 pero una velocidad de 2 (que
    // llega de un knob mal normalizado) daria 32766, y el motor leeria un
    // numero de 14 bits que no existe. El recorte es lo que evita que un
    // valor raro se convierta en un silencio o en un ruido.
    expect(eventFromMessage({ kind: 'noteOn', note: 60, velocity: 1 }).value14)
      .toBe(16383);
    expect(eventFromMessage({ kind: 'noteOn', note: 60, velocity: 2 }).value14)
      .toBe(16383);
    expect(eventFromMessage({ kind: 'noteOn', note: 60, velocity: -1 }).value14)
      .toBe(0);
  });

  it('sin nota ni velocidad, sale la nota y la velocidad por defecto', () => {
    const evento = eventFromMessage({ kind: 'noteOn' });

    expect(evento.note).toBe(DEFAULT_NOTE);
    expect(evento.value).toBe(0.8);
  });

  it('un OFF lleva la nota y NO lleva velocidad', () => {
    // Si el OFF llevara la velocidad con la que se pulso el ON, el motor la
    // leeria como un release disparado a fondo: la nota se cortaria de golpe
    // en vez de soltar la envolvente.
    const evento = eventFromMessage({ kind: 'noteOff', note: 64, velocity: 1 });

    expect(evento.type).toBe(EVENT_TYPE.NOTE_OFF);
    expect(evento.note).toBe(64);
    expect(evento.value).toBe(0);
  });

  it('el pitch bend centrado cae en el CENTRO de 14 bits, no en cero', () => {
    // La conversion que mas facil se equivoca: -1..1 a 0..16383. Sin el
    // desplazamiento, el centro caeria en 0, la rueda doblaria el tono al
    // subir y la musica estaria desafinada en la mitad de su recorrido.
    expect(eventFromMessage({ kind: 'pitchBend', value: 0 }).value14)
      .toBe(8192);
    expect(eventFromMessage({ kind: 'pitchBend', value: -1 }).value14)
      .toBe(0);
    expect(eventFromMessage({ kind: 'pitchBend', value: 1 }).value14)
      .toBe(16383);
  });

  it('el pitch bend se recorta a -1..1 antes de convertirlo', () => {
    // Una rueda que manda 3 (o NaN) tiene que acabar en el tope, no en un
    // numero de 14 bits desbordado.
    expect(eventFromMessage({ kind: 'pitchBend', value: 3 }).value14)
      .toBe(16383);
    expect(eventFromMessage({ kind: 'pitchBend', value: -3 }).value14)
      .toBe(0);
    expect(Number.isFinite(eventFromMessage({ kind: 'pitchBend', value: NaN }).value14))
      .toBe(true);
  });

  it('la presion se recorta a 0..1 y va a 14 bits', () => {
    expect(eventFromMessage({ kind: 'pressure', value: 0.5 }).value14)
      .toBe(8192);
    expect(eventFromMessage({ kind: 'pressure', value: 4 }).value14)
      .toBe(16383);
  });

  it('un mensaje que no es un evento sale mudo, no undefined', () => {
    // Un `undefined` obligaria a que cada consumidor decidiera que hacer, y
    // esa decision es justo lo que se quiere tener en un solo sitio.
    const evento = eventFromMessage({ kind: 'panic' });

    expect(evento).not.toBeUndefined();
    expect(evento.type).toBe(EVENT_TYPE.NOTE_ON);
  });

  it('llena los SEIS campos del POD, y el offset de muestra es 0', () => {
    // El struct tiene seis campos y el puente lo static_assert'ea a 24 bytes.
    // Un campo que se olvide deja el que toque con lo que hubiera de antes:
    // un `type` sin escribir, por ejemplo, es el evento anterior.
    const evento = eventFromMessage({ kind: 'noteOn', note: 60, velocity: 1 });

    expect(Object.keys(evento).sort()).toEqual([
      'channel', 'note', 'sampleOffset', 'type', 'value', 'value14',
    ]);
    expect(evento.channel).toBe(EVENT_CHANNEL);
    expect(evento.sampleOffset).toBe(0);
  });

  it('los numeros de tipo son el ABI, no un enum que se pueda reordenar', () => {
    // El motor los compara con numeros en C++. Cambiar el orden de estas
    // claves haria que una nota sonara como un OFF.
    expect(EVENT_TYPE.NOTE_ON).toBe(0);
    expect(EVENT_TYPE.NOTE_OFF).toBe(1);
    expect(EVENT_TYPE.PITCH_BEND).toBe(2);
    expect(EVENT_TYPE.CHANNEL_PRESSURE).toBe(3);
  });
});
