/**
 * EL ESPEJO DE GlobalParams, en su parte pura: donde esta cada campo, de que
 * clase se escribe, y la cuenta de campos que el motor publica.
 *
 * ESTE MODULO NO ADIVINA NADA DEL STRUCT. Las dos mitades --el offset y la
 * clase-- las publica el puente (`neuronikGlobalParamsLayout` y
 * `neuronikGlobalParamsFieldKinds`), que las construye de la misma tabla en
 * `Source/Wasm/GlobalParamsLayout.h`.
 *
 * HACE UN RATO el worklet llevaba dentro, con DOS listas escritas a mano:
 * `INT_FIELDS = new Set([12, 14, 15, 17, 19, 20, 22, 23, 25, 26, 28, 29, 31,
 * 32])` y `BPM_FIELD = 2`. Eran una COPIA del struct, y copia significa que se
 * puede quedar vieja sin que nada se entere: un miembro nuevo que fuera `int` se
 * escribia con `Float32Array` y el motor leia el patron de bits de IEEE (un
 * 2.0 como 0x40000000). Sin excepcion, sin aviso: el mando se movia y no
 * sonaba. Ahora la lista la da el unico que sabe que es cada miembro.
 *
 * Vive aparte, y no dentro del worklet, por la SEGUNDA mitad del arreglo: el
 * worklet es un `AudioWorkletProcessor` que se registra al importarse y no
 * exporta nada, asi que en vitest (jsdom) no se puede ni mirar. Aqui la
 * traduccion es una funcion, y se puede probar con un modulo de mentira, que es
 * justo lo que hacia falta: que el test diga que el campo 14 se escribe como
 * entero y no lo componga de como le parecio al que lo escribio.
 */

/** Las clases que publica el puente. Los valores son el ABI del export. */
export const FIELD_CLASS = Object.freeze({
    FLOAT32: 0,
    INT32: 1,
    FLOAT64: 2,
});

/**
 * Las dos mitades del espejo, leidas del modulo.
 *
 * @param {object} Module el glue de emscripten ya instanciado.
 * @returns {{ fieldCount: number, byteOffsets: number[], fieldKinds: number[],
 *            fingerprint: number|null }}
 *
 * LANZA si los dos exports no cuentan los mismos campos: serian mitades
 * distintas de una misma verdad, que es el fallo silencioso que se ha estado
 * colandose toda la sesion. Aqui es un throw, que es lo unico que se puede
 * hacer sin inventar un tercer layout.
 */
export function readGpLayout (Module) {
    // EL EXPORT QUE NO EXISTE, que es un caso distinto del que no cuadra.
    // Sin este guardia, un `.wasm` anterior a la tabla unica revienta con un
    // "is not a function" de JavaScript: el aviso de aqui no llega nunca, la
    // pagina ve un error crudo en vez de su linea de "sin motor", y el
    // remedy (recompilar el .wasm) queda enterrado en un stack.
    if (typeof Module._neuronikGlobalParamsFieldKinds !== 'function')
        throw new Error(`este .wasm no publica la clase de los campos del espejo
            (neuronikGlobalParamsFieldKinds): es anterior a la tabla unica del layout.
            Recompila el .wasm (build_wasm.bat) y sincroniza WebUI/dist.`);

    const fieldCount = Module._neuronikGlobalParamsLayout(0, 0);
    const kindCount = Module._neuronikGlobalParamsFieldKinds(0, 0);

    if (fieldCount !== kindCount)
        throw new Error(`el motor publica ${fieldCount} offsets y ${kindCount} clases:`
            + ' el binario es anterior a la tabla unica del layout');

    const ptr = Module._malloc(4 * fieldCount);
    const at = ptr >> 2;

    Module._neuronikGlobalParamsLayout(ptr, fieldCount);
    const byteOffsets = Array.from(Module.HEAP32.subarray(at, at + fieldCount));

    Module._neuronikGlobalParamsFieldKinds(ptr, fieldCount);
    const fieldKinds = Array.from(Module.HEAP32.subarray(at, at + fieldCount));

    Module._free(ptr);

    // LA FIRMA DE ESTE BINARIO, y solo la de este binario. Aqui no se
    // compara con nada: el worklet no tiene ni debe tener la firma
    // esperada --esa vive en la pagina, en el fichero generado-- porque
    // el worklet no puede decir "tu motor esta viejo": no sabe de que
    // motor se trata. Solo puede decir de que motor es, que es
    // exactamente lo que hace un numero.
    //
    // `null` cuando el `.wasm` no publica el export, que es un binario
    // anterior a el. No se lanza por esto: el guardia de mas abajo ya
    // habria parado con un mensaje mejor, y llegar aqui con la clase
    // presente pero sin firma no ocurre.
    const fingerprint = typeof Module._neuronikGlobalParamsLayoutFingerprint === 'function'
        ? Module._neuronikGlobalParamsLayoutFingerprint()
        : null;

    return { fieldCount, byteOffsets, fieldKinds, fingerprint };
}

/**
 * Escribe UN campo del espejo en las vistas del `ArrayBuffer`.
 *
 * @param {object} layout lo que devuelve `readGpLayout`.
 * @param {number} fieldIndex indice de campo (la numeracion de la pagina).
 * @param {number} value el valor REAL, ya en las unidades del DSP.
 * @param {{ f32: Float32Array, i32: Int32Array, f64: Float64Array }} views
 *   las tres vistas sobre el MISMO espejo, que es lo que permite que un f64
 *   conviva con los float sin copias.
 * @returns {boolean} false si el campo no esta en el layout (quien llama
 *   avisa; aqui no se decide a quien se le avisa).
 */
export function writeGpField (layout, fieldIndex, value, views) {
    const byteOffset = layout.byteOffsets[fieldIndex];
    if (byteOffset === undefined) return false;

    const kind = layout.fieldKinds[fieldIndex];

    if (kind === FIELD_CLASS.FLOAT64) {
        const offset = byteOffset / 8;
        if (Number.isInteger(offset)) views.f64[offset] = Number(value);
        return true;
    }

    const offset = byteOffset / 4;
    if (!Number.isInteger(offset)) return false;

    if (kind === FIELD_CLASS.INT32) {
        // Miembro `int` en C++: se guarda el ENTERO, nunca su patron de bits
        // como float (2.0 -> 0x40000000, que el motor leeria como basura).
        views.i32[offset] = Math.round(Number(value));
        return true;
    }

    views.f32[offset] = Number(value);
    return true;
}
