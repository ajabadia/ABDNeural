/**
 * LOS CAMPOS DEL ESPEJO QUE EL MOTOR NO PUBLICA, y el aviso que sale de
 * ellos.
 *
 * El fallo que se esta corrigiendo: `writeGpField` se come en silencio un
 * campo que el layout no publica. Con el `.wasm` viejo, que publicaba 34
 * campos frente a los 40 que la pagina escribe, los seis mandos del hueco se
 * movian en la pagina y no llegaban al motor. Ni un error, ni un rojo: seis
 * knobs muertos.
 *
 * POR QUE CUENTA Y NO AVISA CADA VEZ. Un snapshot son 40 campos, y con el
 * binario viejo seis no existen. Avisando en cada escritura salian seis
 * mensajes para un solo `postMessage`, con la cuenta de menor a mayor. Y el
 * aviso NO se repite en los pushes siguientes: los campos que faltan son
 * siempre los mismos --el contrato no cambia con el tiempo-- y repetirlo
 * cada vez que se mueve un knob llenaria la consola de la pagina.
 *
 * Aqui solo se cuenta y se decide si hay que avisar. Decidir A QUIEN se le
 * avisa (el port, la linea de audio de la pagina) es de quien llama.
 */

/**
 * El registro de los campos que el motor no publica.
 *
 * @param {() => number} fieldCountCounted como el numero de campos que el
 *   motor publica, para el mensaje. Se pasa como funcion porque el layout se
 *   lee una vez al arrancar y el numero no cambia; asi el aviso no lo lleva
 *   guardado y no puede quedarse viejo.
 */
export function createFieldAudit (fieldCountCounted) {
  /** Los indices que el layout no tiene. */
  const missing = new Set ();
  let reported = 0;

  return {
    /** Anota un campo que no se ha podido escribir. */
    note (fieldIndex) {
      missing.add (fieldIndex);
    },

    /**
     * Devuelve el aviso SI hay alguno nuevo, y `null` si no.
     *
     * "Nuevo" quiere decir que la lista ha crecido: si el push siguiente
     * vuelve a pedir los mismos seis, no se avisa otra vez. Asi que la
     * cuenta se recuerda, y un aviso que ya se dio no se repite aunque la
     * lista sea la misma.
     */
    takeReport () {
      if (missing.size === 0 || missing.size === reported) return null;

      reported = missing.size;
      const fields = Array.from (missing).sort ((a, b) => a - b);

      return {
        fields,
        fieldCount: fieldCountCounted (),
        missingCount: fields.length,
      };
    },

    /** Cuantos campos se han anotado, para los tests. */
    get size () {
      return missing.size;
    },

    /** La lista, ordenada, sin decidir nada. Para los tests. */
    list () {
      return Array.from (missing).sort ((a, b) => a - b);
    },
  };
}
