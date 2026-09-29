/**
 * LA COLA DE EVENTOS del worklet, y el recorte por bloque.
 *
 * La pagina manda eventos por el port a su ritmo; el motor los lee al
 * principio de cada bloque, y de un bloque caben pocos. Lo que no cabe se
 * queda para el siguiente, que es lo correcto: perder una nota es peor que
 * retrasarla un bloque (2,7 ms a 48 kHz).
 *
 * POR QUE FUERA DEL WORKLET: lo que hay aqui es una regla ("este bloque
 * caben estos, el resto espera") y una cuenta. En el worklet eran cinco
 * lineas dentro de `stageEvents`, mezcladas con el volcado al heap, y por
 * tanto sin forma de probarlas: no se podia comprobar que un push de
 * cuarenta notas no perdia ninguna, ni que el orden se respetaba.
 *
 * `writeEventRecord` SI depende del heap, asi que se pasa como callback: el
 * modulo decide QUE se escribe y en que orden, y quien tiene el `Module`
 * pone los bytes. Asi el reparto se prueba con un heap de mentira.
 */

/** Cuantos eventos caben en un bloque. Los que sobren, al siguiente. */
export const MAX_EVENTS_PER_BLOCK = 16;

/**
 * Una cola de eventos pendientes.
 *
 * @param {{ write: (index: number, event: object) => void }} sink
 *   donde acaba cada evento ya traducido. `index` es la posicion DENTRO del
 *   bloque, desde cero, que es lo que el motor usa para leer.
 */
export function createEventQueue (sink) {
  if (!sink || typeof sink.write !== 'function')
    throw new TypeError ('createEventQueue necesita un sink con write()');

  /** Los mensajes que aun no se han escrito en un bloque. */
  const pending = [];

  return {
    /** Encola un mensaje de la pagina, todavia sin traducir. */
    push (message) {
      pending.push (message);
    },

    /**
     * Vuelca lo que cabe en este bloque y devuelve cuantos salieron.
     *
     * El recorte es por el PRINCIPIO: lo que se descarta es lo mas nuevo,
     * que es lo que llega detras. Descartar lo mas viejo seria peor --es la
     * nota que se pulsó antes-- y ademas el motor no veria el OFF de una
     * nota que sonó en un bloque anterior, con lo que la voz se quedaria
     * colgada.
     */
    flush (translate) {
      const count = Math.min (pending.length, MAX_EVENTS_PER_BLOCK);

      for (let i = 0; i < count; ++i)
        sink.write (i, translate (pending[i]));

      pending.splice (0, count);
      return count;
    },

    /**
     * Tira el MAS VIEJO de la cola.

     * Para el tope de seguridad, no para el reparto: si la pagina
     * mandara eventos sin parar --un bucle mal escrito-- la cola creceria
     * sin fin en el hilo de audio, que es donde mas caro sale. Se va el
     * mas viejo y no el mas nuevo porque lo mas viejo es una nota que ya
     * se pulso, y su OFF vendria despues: perderlo deja la voz colgada.
     */
    popOldest () {
      if (pending.length === 0) return undefined;
      return pending.shift ();
    },

    /** Cuantos quedan esperando, para los tests y para un aviso. */
    get length () {
      return pending.length;
    },
  };
}
