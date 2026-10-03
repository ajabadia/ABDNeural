/**
 * La sección de ejes del S950: seis horizontales, cuatro rayados, un rótulo.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * LO QUE ESTA SECCION DICE Y POR QUE HACE FALTA QUE SE COMPRUEBE.
 *
 * La tabla de calibración del S950 está vacía a propósito —nadie ha medido esas
 * curvas—. Un panel tiene dos salidas malas con una tabla vacía: dibujar el eje
 * vertical igualmente con un mínimo inventado, o no dibujar nada sin decir por
 * qué. Las dos mienten, y de formas distintas: la primera inventa un dato con
 * la tipografía de una medida, y la segunda hace que quien mira piense que el
 * panel va a medias.
 *
 * Lo que hace esta sección es lo único que no miente: pinta los seis ejes
 * HORIZONTALES, que son dato aunque no haya medidas, y RAYA los cuatro
 * verticales que no se pueden dibujar. El rayado dice dos cosas a la vez: que
 * ahí no hay nada todavía, y que la ausencia es deliberada.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * LA CUENTA ES EL DATO.
 *
 * De las seis curvas, solo dos tienen vertical dibujable: `lfoRate` y
 * `filterEnvOctaves`, que son lineales. Las cuatro logarítmicas no lo tienen,
 * porque un eje log sin mínimo no tiene eje. Este test fija ese 6/4/2 porque es
 * la afirmación que hace la sección: si mañana alguien añade una curva, o si el
 * contrato deja de estar vacío, el recuento cambia y tiene que cambiar a
 * propósito, no por accidente.
 *
 * Y el `6/4/2` no se mira solo aquí: sale del contrato por `S950_AXES` y por
 * `s950AxesWithoutVertical()`, que son del paquete compartido. Un panel que
 * contara por su cuenta podría dar un 6/4/2 mientras el otro da un 6/3/3, y los
 * dos parecerían correctos.
 */

import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { createS950Axes, s950AxesWithoutVertical } from '../src/ui/s950Axes.js';
import { createPanel } from '../src/ui/panel.js';
import {
  S950_AXES,
  S950_COVERAGE,
  s950AxesWithoutVertical as sinVerticalDelContrato,
} from '../src/contracts/s950Calibration.js';

const here = dirname(fileURLToPath(import.meta.url));

/**
 * La hoja del panel, LEIDA DEL DISCO y no del DOM.
 *
 * Es la convencion de `sections.test.js` y de `xyPad.test.js`, y hay una razon
 * tecnica: en vitest con jsdom la hoja la mete vite, no el HTML, asi que el DOM
 * no la tiene y buscarla ahi daria `false` con el CSS puesto. Lo que se quiere
 * comprobar es que la REGLA EXISTE, y el fichero es donde vive.
 */
const stylesheet = readFileSync(join(here, '../src/styles/main.css'), 'utf8');

const axes = createS950Axes();

describe('la seccion de ejes se pinta entera', () => {
  it('enseña los seis ejes, uno por curva del contrato', () => {
    expect(S950_AXES).toHaveLength(6);
    expect(axes.ejes).toHaveLength(6);
    expect(axes.element.querySelectorAll('.s950-axis')).toHaveLength(6);
  });

  it('cada celda lleva el codigo de su curva, y son los seis distintos', () => {
    // El `data-code` es lo que permite consultar una celda desde un test o
    // desde un futuro badge sin depender del texto, que se puede cambiar por
    // tipografía sin que nadie avise.
    const codigos = [...axes.element.querySelectorAll('.s950-axis')].map((c) => c.dataset.code);

    expect(new Set(codigos).size).toBe(6);
    for (const axis of S950_AXES) expect(codigos).toContain(axis.code);
  });

  it('el horizontal se pinta en los seis, porque su rango es de panel', () => {
    // Y esto es lo que separa esta sección de la tentación de rayarlo todo: el
    // eje horizontal va en unidades de PANEL, y ese rango lo declara el
    // contrato aunque la curva no tenga ni un punto. Rayar el horizontal seria
    // rayar un dato que sí existe.
    const horizontales = axes.element.querySelectorAll('.s950-axis__horizontal');
    expect(horizontales).toHaveLength(6);

    for (const h of horizontales) {
      expect(h.textContent, 'el horizontal deberia enseñar sus dos extremos').toMatch(/\d/);
    }
  });

  it('el rango del horizontal es el del contrato, no uno puesto aqui', () => {
    // Si el panel escribiera 0..99 a mano, el -50..+50 del transversal se
    // perderia y nadie se enteraria: un mando bipolar dibujado de 0 a 99 no
    // avisa de nada, solo se ve raro si sabes lo que miran.
    for (const eje of axes.ejes) {
      const celda = axes.element.querySelector(`[data-code="${eje.code}"] .s950-axis__horizontal`);

      expect(Number(celda.dataset.lo)).toBe(eje.lo);
      expect(Number(celda.dataset.hi)).toBe(eje.hi);
    }

    // Y hay uno que no es 0..99, para que el caso se vea.
    const bipolar = axes.ejes.find((e) => e.lo < 0);
    expect(bipolar, 'deberia haber al menos un eje con rango negativo').toBeTruthy();
    expect(bipolar.hi).toBe(50);
  });
});

describe('los cuatro verticales sin minimo van rayados', () => {
  it('cuatro rayados y dos pintados, y el recuento esta en el DOM', () => {
    expect(axes.rayados).toHaveLength(4);
    expect(axes.pintados).toHaveLength(2);
    expect(axes.element.dataset.s950Rayados).toBe('4');
    expect(axes.element.dataset.s950Pintados).toBe('2');
    expect(axes.element.dataset.s950Total).toBe('6');
  });

  it('los rayados son las cuatro logaritricas, y no otras', () => {
    // No basta con que sean cuatro: tienen que ser LAS cuatro. Cuatro celdas
    // rayadas porque han salido cuatro seria lo de siempre, y el dato que vale
    // es cuales.
    const rayados = axes.rayados.map((e) => e.code).sort();

    expect(rayados).toEqual(['envelopeTime', 'filterCutoff', 'sustainDb', 'warpTime']);
  });

  it('las dos que se pintan son las lineales', () => {
    const pintados = axes.pintados.map((e) => e.code).sort();
    expect(pintados).toEqual(['filterEnvOctaves', 'lfoRate']);
  });

  it('el atributo de estado esta en la celda, y el rayado tiene su clase', () => {
    // Las dos cosas, y por dos razones distintas: el `dataset` lo lee un test o
    // un badge sin depender de la hoja de estilos, y la clase la pinta el CSS.
    // Con solo la clase, el test pasa en verde aunque el CSS no exista; con solo
    // el dato, la celda no se ve rayada.
    for (const eje of axes.rayados) {
      const celda = axes.element.querySelector(`[data-code="${eje.code}"]`);

      expect(celda.dataset.s950Vertical).toBe('no');
      expect(celda.classList.contains('s950-axis--rayado')).toBe(true);
    }

    for (const eje of axes.pintados) {
      const celda = axes.element.querySelector(`[data-code="${eje.code}"]`);

      expect(celda.dataset.s950Vertical).toBe('si');
      expect(celda.classList.contains('s950-axis--rayado')).toBe(false);
    }
  });

  it('una celda rayada DICE que no hay minimo, y una pintada no lo dice', () => {
    // El rayado dibujado con bordes no se ve a distancia, y un `title` no
    // aparece sin pasar por encima. El dato tiene que estar en el texto.
    for (const eje of axes.rayados) {
      const v = axes.element.querySelector(`[data-code="${eje.code}"] .s950-axis__vertical`);

      expect(v.textContent).toMatch (/sin m/i);
      expect(v.classList.contains('s950-axis__vertical--sin-minimo')).toBe(true);
    }

    for (const eje of axes.pintados) {
      const v = axes.element.querySelector(`[data-code="${eje.code}"] .s950-axis__vertical`);

      expect(v.textContent).not.toMatch (/sin m/i);
      expect(['log', 'lineal']).toContain(v.textContent);
    }
  });

  it('el recuento de rayados coincide con el del contrato compartido', () => {
    // Y no se cuenta de otra manera. Esta lista sale del paquete compartido
    // justamente para que dos paneles no respondan cosas distintas a "qué me
    // falta": si aqui se hiciera `filter(...)` por cuenta propia, un cambio en
    // el contrato dejaria un panel diciendo 4 y el otro diciendo 3.
    expect(axes.rayados).toHaveLength(sinVerticalDelContrato().length);
    expect(s950AxesWithoutVertical()).toHaveLength(sinVerticalDelContrato().length);
  });

  it('el CSS del rayado existe, y no es solo una clase en el DOM', () => {
    // Con la clase puesta y el CSS ausente, los tests de arriba pasarian y el
    // panel no tendria rayado: el DOM se aria igual. Se comprueba la hoja
    // porque "rayado" es una cosa VISUAL, y una clase sin regla no rayea nada.
    expect(stylesheet).toMatch(/\.s950-axis--rayado/);
  });

  it('el rayado se ve de verdad: hay una trama y no solo un borde punteado', () => {
    // Un borde discontinuo marca el contorno pero no el interior, y lo que
    // comunica "aqui no hay nada" es el interior. Ademas el rayado no puede
    // ser una imagen suelta: seria una peticion mas y no escalaria con el zoom
    // del panel. La trama es un `repeating-linear-gradient`, que no depende de
    // ninguna de las dos cosas.
    const bloque = bloqueDelSelector('.s950-axis--rayado');

    expect(bloque, 'no hay regla para el rayado').toBeTruthy();
    expect(bloque).toMatch(/repeating-linear-gradient/);
  });

  it('el estado "sin minimo" se ve mas que el texto de al lado, no menos', () => {
    // Es el dato mas importante de la seccion y tiene que leerse. Un estado de
    // "todavia no" pintado al 40% de opacidad, que es lo que se suele hacer con
    // lo que no se sabe, deja el dato importante como lo menos visible del
    // panel. Aqui el sin-minimo tiene color propio y no se apaga.
    const bloque = bloqueDelSelector('.s950-axis__vertical--sin-minimo');

    expect(bloque, 'no hay regla para el estado sin minimo').toBeTruthy();
    expect(bloque).not.toMatch (/opacity:\s*0?\.[0-4]/);
  });
});

describe('el rotulo de cobertura', () => {
  it('se pinta, y dice el texto del contrato', () => {
    // El texto sale de `S950_COVERAGE`, que ya ha decidido como se dice esto.
    // Un panel que escribiera su propia cuenta tendria dos versiones del mismo
    // estado, y solo una estaria revisada.
    const rotulo = axes.element.querySelector('.s950-axes__coverage');

    expect(rotulo).not.toBeNull();
    expect(rotulo.textContent).toBe(S950_COVERAGE.label);
  });

  it('lleva los numeros en el DOM, para no tener que parsear la frase', () => {
    const rotulo = axes.element.querySelector('.s950-axes__coverage');

    expect(rotulo.dataset.s950Medidas).toBe('0');
    expect(rotulo.dataset.s950Total).toBe('6');
    expect(rotulo.dataset.s950Completo).toBe('no');
  });

  it('no dice "vacio", que suena a que alguien lo dejo a medias', () => {
    // El contrato lo dice con estas palabras a proposito, asi que el test las
    // fija: la ausencia de una tabla de calibracion es una decision, y el
    // rotulo tiene que leer como decision.
    expect(S950_COVERAGE.label).not.toMatch (/vac/i);
    expect(S950_COVERAGE.complete).toBe(false);
  });

  it('el rotulo va ANTES de los ejes, porque es lo que se lee primero', () => {
    // Los ejes son el detalle; la cobertura es el estado. Si el rotulo fuera
    // debajo, quien mira el panel de reojo veria seis rayados y tendria que
    // bajar a leer para saber si es un fallo o es lo de siempre.
    const hijos = [...axes.element.children];
    const indiceCabecera = hijos.findIndex((c) => c.classList.contains('s950-axes__header'));
    const indiceRejilla = hijos.findIndex((c) => c.classList.contains('s950-axes__grid'));

    expect(indiceCabecera).toBeGreaterThan(-1);
    expect(indiceRejilla).toBeGreaterThan(-1);
    expect(indiceCabecera).toBeLessThan(indiceRejilla);
  });
});

describe('lo que la seccion NO hace', () => {
  it('no escribe ningun valor medido, y el contrato esta vacio', () => {
    // El motivo de que esta seccion sea segura: no hay ningun numero que
    // pueda ser inventado. Ni un ataque en segundos, ni un minimo, ni un
    // "aproximadamente". Lo unico que hay son rangos de panel, que son dato.
    expect(S950_COVERAGE.measured).toBe(0);

    for (const eje of axes.ejes) {
      // Ningun eje trae un valor. Ni medido, ni estimado, ni "aprox".
      expect(eje.value, `${eje.code} no deberia traer valor`).toBeUndefined();
      expect(eje.min, `${eje.code} no deberia traer un minimo inventado`).toBeUndefined();
    }
  });

  it('no usa el cero del panel como minimo de un eje logaritmico', () => {
    // `lo: 0` en un eje de tiempo es un instante, y un instante es un click. El
    // contrato tiene `excludeZero` para esto, y la seccion lo pasa tal cual sin
    // reinterpretarlo.
    for (const eje of axes.ejes) {
      if (eje.scale !== 'log') continue;
      expect(eje.verticalDibujable, `${eje.code} es log y no deberia pintarse`).toBe(false);
    }
  });

  it('dice la regla de excludeZero como la dice el contrato, sin recalcularla', () => {
    // Los dB admiten el cero y las frecuencias no. Que lo diga el contrato y no
    // una regla del panel es lo que hace que los dos no se separen en silencio.
    for (const eje of axes.ejes) {
      const delContrato = S950_AXES.find((a) => a.code === eje.code);
      expect(eje.excludeZero).toBe(delContrato.excludeZero);
    }

    const db = axes.ejes.find((e) => e.code === 'sustainDb');
    const hz = axes.ejes.find((e) => e.code === 'lfoRate');

    expect(db.excludeZero).toBe(false);   // 0 dB es un nivel de referencia
    expect(hz.excludeZero).toBe(true);    // 0 Hz no es una frecuencia
  });

  it('no dibuja ninguna curva, porque no hay puntos que dibujar', () => {
    // La seccion presenta el ESTADO del contrato. Dibujar una curva con dos
    // puntos seria instalar aqui la capacidad de pintar una tabla que no existe.
    const svg = axes.element.querySelectorAll('svg, canvas');
    expect(svg).toHaveLength(0);
  });
});

describe('la nota al pie', () => {
  it('explica el rayado en una frase, con la cuenta dentro', () => {
    // Sin ella, un rayado es un simbolo. Con ella, es un estado. Y el sitio es
    // abajo porque es la conclusion: primero se ven los seis, y luego se lee
    // que cuatro no se pueden pintar todavia.
    const nota = axes.element.querySelector('.s950-axes__note');

    expect(nota.textContent).toMatch (/4 de 6/);
    expect(nota.textContent).toMatch (/m.nimo medido/);
  });

  it('la nota va despues de la rejilla', () => {
    const hijos = [...axes.element.children];
    expect(hijos.findIndex((c) => c.classList.contains('s950-axes__grid')))
      .toBeLessThan(hijos.findIndex((c) => c.classList.contains('s950-axes__note')));
  });
});

/**
 * El bloque de reglas de un selector, con sus llaves.
 *
 * Buscar el selector y cortar en la llave que lo cierra, en vez de mirar si la
 * hoja "habla de rayados": asi el test comprueba QUE HACE la regla y no solo que
 * su nombre aparece. Una regla vacia que se limitara a nombrar el selector pasaria
 * la comprobacion de nombre y no rayaria nada.
 */
function bloqueDelSelector (selector) {
  const at = stylesheet.indexOf(selector);
  if (at === -1) return null;

  const abre = stylesheet.indexOf('{', at);
  if (abre === -1) return null;

  // Llaves anidadas: un `@media` dentro del bloque haria que el primer `}` no
  // sea el final, y cortar ahi deja el resto de la regla fuera y el test pasa
  // por una regla a medias.
  let nivel = 0;
  for (let i = abre; i < stylesheet.length; i += 1) {
    if (stylesheet[i] === '{') nivel += 1;
    else if (stylesheet[i] === '}') {
      nivel -= 1;
      if (nivel === 0) return stylesheet.slice(abre, i + 1);
    }
  }

  return null;
}

describe('la seccion se monta en el PANEL, y no solo en su propio test', () => {
  // Una vista puede ser correcta y no estar en ningun sitio. Estos tests son la
  // diferencia entre «la funcion pinta bien» —que es todo lo de arriba— y «el
  // panel la enseña», que es lo que pedia el encargo.
  // Un panel con UNA sola banda y una sola seccion. `panel.test.js` monta el
  // arnés entero con todos los controles, que es lo que hace falta para probar
  // el panel; aqui lo unico que importa es que la seccion de ejes se anexa, y
  // para eso basta con la estructura minima que `createPanel` exige.
  // Los campos que `buildCard` recorre: `controls` tiene que existir aunque sea
  // vacio, porque hace un `for...of` sobre el. Un `ids: []` sin `controls` deja
  // el panel a medio construir, y un panel a medio construir no demuestra nada
  // sobre si la seccion de ejes se anexa bien.
  const seccionVacia = { id: 'sonda', title: 'Sonda', ids: [], controls: [] };
  const panel = createPanel({
    bands: [[seccionVacia]],
    baselineId: 'masterLevel',
  });

  it('aparece dentro del panel, no flotando aparte', () => {
    expect(panel.element.querySelector('.s950-axes')).not.toBeNull();
  });

  it('va despues del teclado y antes del pie', () => {
    // El orden es el de la lectura: lo que se USA, luego lo que se SABE. Un
    // panel que|first|...' primero el estado de la tabla de medicion hace que
    // quien lo mira compare el estado con algo que no tiene delante.
    const hijos = [...panel.element.children];
    const teclado = hijos.findIndex((c) => c.querySelector?.('#keys-root') != null);
    const ejes = hijos.findIndex((c) => c.classList.contains('s950-axes'));
    const pie = hijos.findIndex((c) => c.classList.contains('panel-footer'));

    expect(teclado).toBeGreaterThan(-1);
    expect(ejes).toBeGreaterThan(teclado);
    expect(pie).toBeGreaterThan(ejes);
  });

  it('trae los seis ejes y los cuatro rayados ya en el panel montado', () => {
    const seccion = panel.element.querySelector('.s950-axes');

    expect(seccion.querySelectorAll('.s950-axis')).toHaveLength(6);
    expect(seccion.querySelectorAll('.s950-axis--rayado')).toHaveLength(4);
    expect(seccion.dataset.s950Rayados).toBe('4');
  });

  it('el rotulo de cobertura se ve en el panel, no solo en la vista suelta', () => {
    const rotulo = panel.element.querySelector('.s950-axes__coverage');

    expect(rotulo).not.toBeNull();
    expect(rotulo.textContent).toBe(S950_COVERAGE.label);
  });

  it('no rompe el resto del panel', () => {
    // Una seccion que se monta pero desplaza el pie o tapa el lienzo deja el
    // panel entero estropeado, y eso no lo ve un test de la seccion sola.
    expect(panel.element.querySelector('.panel-footer')).not.toBeNull();
    expect(panel.element.querySelector('.canvas')).not.toBeNull();
    expect(panel.element.querySelector('#keys-root')).not.toBeNull();
  });
});
