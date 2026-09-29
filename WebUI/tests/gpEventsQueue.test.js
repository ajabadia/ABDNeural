/**
 * LA COLA DE EVENTOS (`public/worklet/gpEventsQueue.js`).
 *
 * Lo que se prueba es una regla de reparto: de un bloque caben 16, lo que
 * sobra espera al siguiente, y lo que se descarta es lo mas viejo. Antes
 * eran cinco lineas dentro de `stageEvents`, en un `AudioWorkletProcessor`
 * que se registra al importarse y no exporta nada: no se podia comprobar
 * que un push de cuarenta notas perdia exactamente 24 y por el otro orden,
 * ni que un OFF nunca se quedaba sin su ON.
 *
 * El `sink` es un heap de mentira. El reparto es lo que se prueba; el
 * volcado al heap de verdad se queda en el worklet.
 */

import { describe, expect, it } from 'vitest';

import {
  createEventQueue,
  MAX_EVENTS_PER_BLOCK,
} from '../public/worklet/gpEventsQueue.js';

/** Un sink que guarda lo que se le escribe, con su indice de bloque. */
function recordingSink () {
  const written = [];
  return {
    written,
    write (index, event) { written.push ({ index, event }); },
  };
}

/** Un traductor de mentira: devuelve el mensaje con una etiqueta. */
const etiquetar = (message) => ({ ...message, event: true });

describe('gpEventsQueue / createEventQueue', () => {
  it('reparte en orden y devuelve cuantos salieron', () => {
    const sink = recordingSink();
    const queue = createEventQueue (sink);

    queue.push ({ kind: 'noteOn', note: 60 });
    queue.push ({ kind: 'noteOff', note: 60 });
    queue.push ({ kind: 'noteOn', note: 64 });

    const count = queue.flush (etiquetar);

    expect(count).toBe(3);
    expect(sink.written.map ((w) => w.event.note)).toEqual ([60, 60, 64]);
    // El indice es la posicion DENTRO del bloque, que es como lo lee el
    // motor: si fuera un contador global, el segundo bloque empezaria en 3.
    expect(sink.written.map ((w) => w.index)).toEqual ([0, 1, 2]);
    expect(queue.length).toBe(0);
  });

  it('de lo que no cabe, se queda para el siguiente bloque Y EN ORDEN', () => {
    // Este es el caso que no se podia probar antes. Cuarenta notas de golpe:
    // un acorde grande, o una pagina que manda la entrada del raton de golpe.
    // Perder una nota es peor que retrasarla 2,7 ms, y perder el OFF de una
    // nota que ya suena deja la voz colgada para siempre.
    const sink = recordingSink();
    const queue = createEventQueue (sink);

    for (let i = 0; i < 40; ++i) queue.push ({ kind: 'noteOn', note: i });

    expect(queue.flush (etiquetar)).toBe(MAX_EVENTS_PER_BLOCK);
    expect(queue.length).toBe(40 - MAX_EVENTS_PER_BLOCK);

    // El primer bloque se llevo los 16 PRIMEROS, no 16 cualesquiera.
    expect(sink.written.map ((w) => w.event.note)).toEqual (
      Array.from ({ length: MAX_EVENTS_PER_BLOCK }, (_, i) => i),
    );

    // Y lo que queda sigue en orden, para el siguiente.
    expect(queue.flush (etiquetar)).toBe(MAX_EVENTS_PER_BLOCK);
    expect(sink.written.slice (MAX_EVENTS_PER_BLOCK).map ((w) => w.event.note))
      .toEqual (Array.from ({ length: MAX_EVENTS_PER_BLOCK }, (_, i) => i + 16));
  });

  it('un bloque vacio no escribe nada y no cuenta', () => {
    const sink = recordingSink();
    const queue = createEventQueue (sink);

    expect(queue.flush (etiquetar)).toBe(0);
    expect(sink.written).toEqual ([]);

    queue.push ({ kind: 'noteOn', note: 60 });
    expect(queue.flush (etiquetar)).toBe(1);
  });

  it('el indice vuelve a cero en cada bloque', () => {
    // El motor lee el array desde el principio en cada bloque, asi que un
    // indice que siguiera contando dejaria huecos: el motor leeria ceros,
    // que es un NOTE_ON con nota 0.
    const sink = recordingSink();
    const queue = createEventQueue (sink);

    queue.push ({ kind: 'noteOn', note: 60 });
    queue.flush (etiquetar);
    queue.push ({ kind: 'noteOn', note: 64 });
    queue.flush (etiquetar);

    expect(sink.written.map ((w) => w.index)).toEqual ([0, 0]);
  });

  it('el tope de seguridad tira lo MAS VIEJO, no lo mas nuevo', () => {
    // Se llega desde `handleMessage`, que encola sin limite. Si se tirara lo
    // mas nuevo se perderian los OFF de las notas que ya suenan, y cada voz
    // se quedaria colgada hasta el PANIC.
    const queue = createEventQueue (recordingSink());

    for (let i = 0; i < 300; ++i) queue.push ({ kind: 'noteOn', note: i });
    while (queue.length > 256) queue.popOldest ();

    expect(queue.length).toBe(256);

    // Los que quedan son los ULTIMOS 256 escritos, en orden: el 44 es el
    // primero que sobrevive, porque se escribieron 300 y se tiraron 44.
    // Si se tirara lo mas nuevo, el primero que sobrevive seria el 0.
    const sink = recordingSink ();
    const cola = createEventQueue (sink);
    for (let i = 0; i < 300; ++i) cola.push ({ kind: 'noteOn', note: i });
    while (cola.length > 256) cola.popOldest ();
    cola.flush (etiquetar);
    expect(sink.written.map ((w) => w.event.note)).toEqual (
      Array.from ({ length: MAX_EVENTS_PER_BLOCK }, (_, i) => i + 44),
    );
  });

  it('sin sink, falla al construirlo y no al usarlo', () => {
    // Un sink que no existe daria un error dentro del hilo de audio, que es
    // donde un error es mas caro y mas dificil de ver.
    expect(() => createEventQueue (null)).toThrow (/sink/);
    expect(() => createEventQueue ({})).toThrow (/sink/);
  });
});
