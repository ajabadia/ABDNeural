/**
 * LOS EVENTOS DE LA PAGINA, traducidos al POD que el motor entiende.
 *
 * POR QUE ESTA FUERA DEL WORKLET. El worklet es un `AudioWorkletProcessor`
 * que se registra al importarse y no exporta nada, asi que en vitest
 * (jsdom) no se puede ni mirar. Todo lo que hay aqui --el recorte de la
 * velocidad a 14 bits, el del pitch bend a 0..16383, el relleno del struct--
 * es aritmetica que se puede equivocar en silencio: un valor fuera de rango
 * no da error, suena mal. Con un modulo son funciones y se prueban.
 *
 * El POD es `Runtime::Event`, 24 bytes, static_assert'ado en el puente:
 *
 *     [0] i32 type  [1] i32 channel  [2] i32 note  [3] i32 value14
 *     [4] f32 value [5] i32 sampleOffset
 *
 * Y NO se escribe con una sola vista: `value` es float y los otros cinco
 * campos son enteros, asi que hace falta la vista de float en el cuarto. Esa
 * distincion es invisible si el bucle esta dentro de una clase de la que no
 * se puede sacar nada.
 */

/** Los tipos de evento del motor. Los numeros son el ABI. */
export const EVENT_TYPE = Object.freeze({
  NOTE_ON: 0,
  NOTE_OFF: 1,
  PITCH_BEND: 2,
  CHANNEL_PRESSURE: 3,
});

/** El canal por el que salen los eventos de la interfaz. */
export const EVENT_CHANNEL = 1;

/** Si no viene nota, esta. El motor no decide: decide quien manda. */
export const DEFAULT_NOTE = 60;

/** Rango del valor de 14 bits, sin signo. */
const MAX_14 = 16383;

/**
 * Un valor que no es un numero, al valor por defecto de su rango.
 *
 * POR QUE ESTA PUERTA Y NO UN `Math.max` a secas: `Math.max(-1,
 * Math.min(1, NaN))` es NaN, no 0. Y NaN escrito en el heap es un patron de
 * bits cualquiera que el motor lee como un numero sin sentido, asi que una
 * rueda que mande NaN --un knob mal normalizado, un `undefined`-- no suena
 * mal: suena a lo que le da la gana. El 0 es lo que el motor ya espera
 * cuando no hay gesto: el centro del pitch bend, la ausencia de presion.
 */
const finito = (value, porDefecto) =>
  Number.isFinite (value) ? value : porDefecto;

/**
 * Un mensaje de la pagina al registro que el motor va a leer.
 *
 * Se devuelve un objeto plano, no un puntero: quien lo recibe lo escribe en
 * el heap, y esa parte es la que necesita el `Module`. Aqui solo se decide
 * QUE se escribe, que es lo que se puede probar sin wasm.
 *
 * @param {object} message el mensaje de la pagina (`neuronik:midi`).
 * @returns {{ type: number, channel: number, note: number, value14: number,
 *            value: number, sampleOffset: number }}
 */
export function eventFromMessage (message) {
  // Un mensaje de otra clase no es un evento: sale un NOTE_ON mudo en vez de
  // `undefined`. Un `undefined` obligaria a que cada consumidor decidiera que
  // hacer con el, y esa decision es justo lo que se quiere tener en un sitio.
  const kind = message?.kind;

  let type = EVENT_TYPE.NOTE_ON;
  let note = DEFAULT_NOTE;
  let value = 0.0;
  let value14 = 8192;

  if (kind === 'noteOn') {
    type = EVENT_TYPE.NOTE_ON;
    note = finito (message.note, DEFAULT_NOTE);
    value = finito (Number (message.velocity), 0.8);
    value14 = Math.max (0, Math.min (MAX_14, Math.round (value * MAX_14)));
  } else if (kind === 'noteOff') {
    type = EVENT_TYPE.NOTE_OFF;
    note = finito (message.note, DEFAULT_NOTE);
  } else if (kind === 'pitchBend') {
    type = EVENT_TYPE.PITCH_BEND;
    // El pitch bend del motor va de -1 a 1, y el de 14 bits de 0 a 16383
    // sale de desplazar ese rango. Es la conversion que mas facil se
    // equivoca: sin el `-1` y el `* 8191.5` el centro caeria en 0 y la rueda
    // doblaria el tono al subir.
    value = Math.max (-1, Math.min (1, finito (Number (message.value), 0)));
    value14 = Math.max (0, Math.min (MAX_14, Math.round ((value + 1) * 8191.5)));
  } else if (kind === 'pressure') {
    type = EVENT_TYPE.CHANNEL_PRESSURE;
    value = Math.max (0, Math.min (1, finito (Number (message.value), 0)));
    value14 = Math.round (value * MAX_14);
  }

  return {
    type,
    channel: EVENT_CHANNEL,
    note,
    value14,
    value,
    // El offset de muestra: para notas de la interfaz vale la frontera del
    // bloque, que es lo que hacia el worklet y no hay motivo para cambiarlo.
    sampleOffset: 0,
  };
}
