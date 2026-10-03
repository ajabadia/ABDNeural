/**
 * syncWasm.test.js — ABDNeural: que se considera "el mismo artefacto".
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * POR QUE ESTE TEST EXISTE
 *
 * `sync-wasm.mjs` empezo comparando byte a byte, con un comentario que hacia jura
 * de que no habia CRLF que normalizar porque lo que se comparaba era el `.wasm`
 * compilado. Era falso, y hacia dano:
 *
 *   · `build-wasm/` NO esta versionado (es salida de Emscripten), asi que en
 *     Windows llega con CRLF.
 *   · El destino, `WebUI/public/worklet/`, lleva `eol: lf` en `.gitattributes`.
 *
 * Las dos mitades del mismo fichero con finales de linea distintos son 2 bytes de
 * diferencia, y el check declaraba "DESACTUALIZADO" un DSP que no habia cambiado.
 * Un rojo que no corresponde a nada de lo que el script dice vigilar es peor que no
 * tener check: entrena a ignorar el check. Y la unica "solucion" era copiar, que
 * metia CRLF en un fichero que el repo exige en LF y dejaba el working tree
 * ensuciado con algo que no era un cambio del DSP.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * LO QUE ESTE TEST PROTEGE, EN UNA FRASE
 *
 * Que la normalizacion se aplique AL TEXTO Y NO AL BINARIO. Si alguien la extiende
 * a `.wasm`, el `.wasm` dara "stale" mientras el contenido no cambie y el fallo
 * vuelve a ser invisible: un binario que cambia de un byte a otro es otro binario,
 * y un binario puede contener 0D 0A por casualidad.
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * POR QUE NO HAY beforeEach
 *
 * Cada test monta SU PROPIO temporal y lo borra al terminar, en vez de apoyarse en
 * `beforeEach`. El harness de verificacion de ABDSharedAssets ejecuta los hooks una
 * sola vez para todo el fichero, no uno por test, asi que un `beforeEach` ahi daria
 * un unico arbol compartido — y justo estos tests se pisan entre si, porque todos
 * escriben `neuronik_dsp.js`. Con el helper explicito cada test es independiente
 * bajo vitest y tambien bajo el harness.
 */
import { describe, it, expect } from 'vitest';
import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';

import { ES_TEXTO, contenido, huella, sincronizar } from '../scripts/sync-wasm.mjs';

function preparar () {
  const raiz = mkdtempSync (path.join (tmpdir (), 'sync-wasm-'));
  mkdirSync (path.join (raiz, 'build-wasm'), { recursive: true });
  mkdirSync (path.join (raiz, 'WebUI', 'public', 'worklet'), { recursive: true });
  return raiz;
}

function poner (raiz, rel, datos) {
  const p = path.join (raiz, rel);
  mkdirSync (path.dirname (p), { recursive: true });
  writeFileSync (p, datos);
  return p;
}

const destino = (raiz, rel) => path.join (raiz, 'WebUI', 'public', 'worklet', rel);

const correr = (raiz, check) => sincronizar ({
  check,
  sourceDir: path.join (raiz, 'build-wasm'),
  targetDir: path.join (raiz, 'WebUI', 'public', 'worklet'),
  root: raiz,
});

// Un par de artefactos identicos, para que cada test parta de "todo al dia" y solo
// tenga que romper lo que este probando.
const base = (raiz) => {
  poner (raiz, 'build-wasm/neuronik_dsp.js', 'async function M(){}\n');
  poner (raiz, 'WebUI/public/worklet/neuronik_dsp.js', 'async function M(){}\n');
  poner (raiz, 'build-wasm/neuronik_dsp.wasm', Buffer.from ([1, 2, 3]));
  poner (raiz, 'WebUI/public/worklet/neuronik_dsp.wasm', Buffer.from ([1, 2, 3]));
};

describe ('sync-wasm — el texto se compara normalizado', () => {
  it ('el .js con CRLF y su gemelo con LF son EL MISMO fichero', () => {
    const raiz = preparar ();
    try {
      const a = poner (raiz, 'a/neuronik_dsp.js', 'linea 1\r\nlinea 2\r\n');
      const b = poner (raiz, 'b/neuronik_dsp.js', 'linea 1\nlinea 2\n');

      expect ({ misma: huella (a, 'neuronik_dsp.js') === huella (b, 'neuronik_dsp.js') })
        .toEqual ({ misma: true });
    } finally { rmSync (raiz, { recursive: true, force: true }); }
  });

  it ('pero el contenido distinto SI se nota, normalizado como sea', () => {
    const raiz = preparar ();
    try {
      const a = poner (raiz, 'a/neuronik_dsp.js', 'linea 1\r\nlinea 2\r\n');
      const b = poner (raiz, 'b/neuronik_dsp.js', 'linea 1\nlinea OTRA\n');

      expect ({ misma: huella (a, 'neuronik_dsp.js') === huella (b, 'neuronik_dsp.js') })
        .toEqual ({ misma: false });
    } finally { rmSync (raiz, { recursive: true, force: true }); }
  });

  it ('NO APLANA AL BINARIO: un byte 0D basta para que sean distintos', () => {
    // El caso que justifica que la normalizacion sea por lista y no global: un
    // `.wasm` puede contener 0D 0A por casualidad, en medio de una cadena de bytes.
    // Si se normalizara, esos dos binarios —que son binarios distintos— se
    // declararian el mismo, y el check dejaria de detectar un DSP que SI cambio.
    const raiz = preparar ();
    try {
      const conCr = poner (raiz, 'a/neuronik_dsp.wasm', Buffer.from ([0x00, 0x0d, 0x0a, 0xff]));
      const conLf = poner (raiz, 'b/neuronik_dsp.wasm', Buffer.from ([0x00, 0x0a, 0x0a, 0xff]));

      expect ({ mismo: huella (conCr, 'neuronik_dsp.wasm') === huella (conLf, 'neuronik_dsp.wasm') })
        .toEqual ({ mismo: false });
    } finally { rmSync (raiz, { recursive: true, force: true }); }
  });

  it ('normalizar no toca NINGUN otro byte, ni los de encima de 0x7F', () => {
    // `toString('utf8')` sobre bytes invalidos los destrozaria (cada byte suelto se
    // convierte en U+FFFD), asi que el viaje de ida y vuelta tiene que ser en
    // latin1. Un texto UTF-8 con enye y con emoji es justo donde se notaria.
    const raiz = preparar ();
    try {
      const p = poner (raiz, 'x/neuronik_dsp.js', Buffer.from ([0xc3, 0xb1, 0xe2, 0x9c, 0x93, 0x0d, 0x0a]));

      expect (contenido (p, 'neuronik_dsp.js').equals (Buffer.from ([0xc3, 0xb1, 0xe2, 0x9c, 0x93, 0x0a])))
        .toBe (true);
    } finally { rmSync (raiz, { recursive: true, force: true }); }
  });

  it ('al atravesar los 256 valores de byte solo se pierde el CR de un CRLF', () => {
    const raiz = preparar ();
    try {
      // Con un CRLF intercalado cada dos bytes. Montarlo asi a proposito: una
      // secuencia 0..255 tiene un LF en la posicion 10 y un CR en la 13, que NO
      // son contiguos, asi que no contiene ni un solo CRLF — y un test que esperase
      // "255 bytes de salida" sobre ella estaria comprobando que no ocurre nada.
      const intercalados = [];
      for (const b of Array.from ({ length: 256 }, (_, i) => i)) { intercalados.push (b, 0x0d, 0x0a); }
      const entrada = Buffer.from (intercalados);
      const salida = contenido (poner (raiz, 'y/neuronik_dsp.js', entrada), 'neuronik_dsp.js');

      // Desaparecen los CR que formaban CRLF, y solo ellos: el CR que ya venia como
      // byte de datos va seguido de otro CR, no de un LF, y se queda.
      const esperado = Buffer.from (intercalados.filter ((b, i) => !(b === 0x0d && intercalados[i + 1] === 0x0a)));
      expect ({sameLength: salida.length === esperado.length, sameBytes: salida.equals (esperado) })
        .toEqual ({sameLength: true, sameBytes: true});
    } finally { rmSync (raiz, { recursive: true, force: true }); }
  });

  it ('un CR y un LF que NO son contiguos no se tocan', () => {
    // El caso que hace inutilizable la normalizacion ingenua de "quitar todos los CR":
    // en la secuencia de bytes 0..255 el 0x0A esta en la posicion 10 y el 0x0D en la
    // 13. Un "borra los CR" distraido se llevaria por delante un byte de datos.
    const raiz = preparar ();
    try {
      const entrada = Buffer.from (Array.from ({ length: 256 }, (_, i) => i));
      const salida = contenido (poner (raiz, 'z/neuronik_dsp.js', entrada), 'neuronik_dsp.js');

      expect ({sameLength: salida.length === entrada.length, sameBytes: salida.equals (entrada)})
        .toEqual ({sameLength: true, sameBytes: true});
    } finally { rmSync (raiz, { recursive: true, force: true }); }
  });

  it ('ES_TEXTO nombra el .js y NO el .wasm', () => {
    expect ([...ES_TEXTO].sort ()).toEqual (['neuronik_dsp.js']);
  });
});

describe ('sync-wasm — el check y la copia, sobre un arbol de verdad', () => {
  it ('el caso real: build-wasm en CRLF, worklet en LF -> NO esta stale', () => {
    // El escenario exacto que tenia el repo en rojo: el origen con CRLF por ser
    // salida de Emscripten sin versionar, el destino con LF por `.gitattributes`.
    const raiz = preparar ();
    try {
      poner (raiz, 'build-wasm/neuronik_dsp.js', 'async function M(){}\r\n');
      poner (raiz, 'WebUI/public/worklet/neuronik_dsp.js', 'async function M(){}\n');
      poner (raiz, 'build-wasm/neuronik_dsp.wasm', Buffer.from ([1, 2, 3]));
      poner (raiz, 'WebUI/public/worklet/neuronik_dsp.wasm', Buffer.from ([1, 2, 3]));

      expect (correr (raiz, true).stale).toEqual ([]);
    } finally { rmSync (raiz, { recursive: true, force: true }); }
  });

  it ('pero si el .js de verdad cambia, lo sigue viendo', () => {
    const raiz = preparar ();
    try {
      base (raiz);
      poner (raiz, 'build-wasm/neuronik_dsp.js', 'async function M(){}\r\n');
      poner (raiz, 'WebUI/public/worklet/neuronik_dsp.js', 'async function OTRO(){}\n');

      expect (correr (raiz, true).stale).toEqual (['neuronik_dsp.js']);
    } finally { rmSync (raiz, { recursive: true, force: true }); }
  });

  it ('y si el .wasm cambia un byte, tambien lo ve', () => {
    const raiz = preparar ();
    try {
      base (raiz);
      poner (raiz, 'WebUI/public/worklet/neuronik_dsp.wasm', Buffer.from ([1, 2, 4]));

      expect (correr (raiz, true).stale).toEqual (['neuronik_dsp.wasm']);
    } finally { rmSync (raiz, { recursive: true, force: true }); }
  });

  it ('sincronizar escribe el destino en LF: NO propaga el CRLF del origen', () => {
    // Lo que hacia el sincronizador viejo: copiar los bytes tal cual. El contenido
    // indexado era el mismo, pero el working tree quedaba marcado como modificado
    // por dos bytes que no eran un cambio del DSP.
    const raiz = preparar ();
    try {
      poner (raiz, 'build-wasm/neuronik_dsp.js', 'una\r\ndos\r\n');
      poner (raiz, 'WebUI/public/worklet/neuronik_dsp.js', 'contenido viejo\n');
      poner (raiz, 'build-wasm/neuronik_dsp.wasm', Buffer.from ([1]));

      correr (raiz, false);

      const escrito = readFileSync (destino (raiz, 'neuronik_dsp.js'));
      expect ({ texto: escrito.toString ('utf8'), tieneCR: escrito.includes (0x0d) })
        .toEqual ({ texto: 'una\ndos\n', tieneCR: false });
    } finally { rmSync (raiz, { recursive: true, force: true }); }
  });

  it ('y tras sincronizar, el check queda verde de verdad', () => {
    const raiz = preparar ();
    try {
      poner (raiz, 'build-wasm/neuronik_dsp.js', 'una\r\ndos\r\n');
      poner (raiz, 'build-wasm/neuronik_dsp.wasm', Buffer.from ([1, 2, 3]));

      correr (raiz, false);
      expect (correr (raiz, true).stale).toEqual ([]);
    } finally { rmSync (raiz, { recursive: true, force: true }); }
  });

  it ('si falta un artefacto en el origen, lo dice y no finge que esta al dia', () => {
    // El fallo que `stale` no cuenta: un artefacto que no esta. Sin esto, un
    // `build-wasm/` vacio daria "0 stale, OK" y quien lo lee se queda pensando que
    // el worklet sirve el DSP recien compilado.
    const raiz = preparar ();
    try {
      const r = correr (raiz, true);

      expect ({ stale: r.stale, fallo: r.failed }).toEqual ({ stale: [], fallo: true });
    } finally { rmSync (raiz, { recursive: true, force: true }); }
  });

  it ('NEGATIVO: no basta con que el .js se parezca, tiene que ser el mismo', () => {
    // El fallo facil de introducir al arreglar el falso positivo: comparar solo el
    // numero de lineas, solo el tamano, o ignorar el contenido por completo.
    const raiz = preparar ();
    try {
      base (raiz);
      poner (raiz, 'build-wasm/neuronik_dsp.js', 'const a = 1;\n');
      poner (raiz, 'WebUI/public/worklet/neuronik_dsp.js', 'const b = 1;\n');

      expect (correr (raiz, true).stale).toEqual (['neuronik_dsp.js']);
    } finally { rmSync (raiz, { recursive: true, force: true }); }
  });

  it ('NEGATIVO: un 0D suelto NO es un final de linea que normalizar', () => {
    // Un CR sin LF no es un CRLF. Si tambien se normalizara, dos ficheros que solo se
    // diferencian en eso —que en un .js minificado puede ser justo al final— se
    // declararian identicos, y el DSP que cambio se serviria sin avisar.
    const raiz = preparar ();
    try {
      base (raiz);
      poner (raiz, 'build-wasm/neuronik_dsp.js', 'texto');
      poner (raiz, 'WebUI/public/worklet/neuronik_dsp.js', 'texto\r');

      expect (correr (raiz, true).stale).toEqual (['neuronik_dsp.js']);
    } finally { rmSync (raiz, { recursive: true, force: true }); }
  });
});