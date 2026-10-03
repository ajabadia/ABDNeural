/**
 * La sección de ejes del S950: seis ejes, cuatro rayados.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * QUE ENSENA Y POR QUE LOS HAY.
 *
 * El panel tiene una sección que responde a una pregunta que no es "cómo suena"
 * sino "qué sabe este repo sobre la máquina". Responde con seis ejes
 * horizontales, uno por curva, y con el rótulo de cobertura encima: cuántas hay
 * medidas de cuántas hay.
 *
 * Los seis horizontales se pintan SIEMPRE, y eso no es un detalle: el eje
 * horizontal va en unidades de PANEL —el 0..99 del mando, el −50..+50 del
 * transversal— y ese rango lo declara el contrato aunque no haya ninguna
 * medida. Es el dato que un panel siempre puede enseñar, y sin él no hay con
 * qué comparar cuando llegue la primera.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * LOS CUATRO VERTICALES, RAYADOS.
 *
 * De las seis curvas, solo DOS tienen eje vertical dibujable: `lfoRate` y
 * `filterEnvOctaves`, que son lineales. Las otras cuatro —`envelopeTime`,
 * `warpTime`, `filterCutoff`, `sustainDb`— son logarítmicas, y un eje log sin
 * mínimo no tiene eje: el cero está en el infinito, así que cualquier mínimo
 * que se invente decide por dónde empieza la escala.
 *
 * Aquí es donde un panel tiene tentacion de hacer dos cosas malas. La primera es
 * dibujar el eje vertical igualmente, con un mínimo inventado del rango del
 * panel: sale un attack de 0.5 ms abajo del todo y de 5 s arriba, y parece
 * medido. La segunda es no dibujar nada y no decir por qué, que deja al que mira
 * pensando que el panel va a medias.
 *
 * Lo que hace esta sección es lo tercero: pinta el hueco. El hueco es real, es
 * un dato, y rayarlo dice dos cosas a la vez —que aquí no hay nada todavía y
 * que la ausencia es deliberada—. Un rayado es mejor que un espacio en blanco
 * porque un espacio en blanco se lee como un olvido y un rayado se lee como
 * "esto está pendiente a propósito".
 *
 * ─────────────────────────────────────────────────────────────────────────
 * POR QUÉ NO SE USA EL RANGO DEL PANEL COMO MÍNIMO.
 *
 * Porque `lo: 0` en un eje de tiempo es un instante, y un instante es un click.
 * El contrato tiene `excludeZero` justo para esto —los dB admiten el 0 porque
 * 0 dB es un nivel de referencia, y un tiempo de cero no es un tiempo— y un
 * panel que usara el cero como mínimo de un eje logarítmico estaría pintando un
 * ataque instantáneo con la autoridad de un eje. La regla está en el contrato y
 * aquí no se reimplementa: se lee.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * Y LO QUE NO PIDE ESTA SECCIÓN.
 *
 * No pide un número medido a nadie, ni calcula un mínimo, ni dibuja una curva.
 * Solo presenta lo que el contrato declara y marca lo que falta. Esa
 * separación es la misma del resto del trabajo: el dato y su ausencia son dos
 * estados con nombre, y esta sección no los confunde ni un momento.
 *
 * @example
 *   import { createS950Axes } from './s950Axes.js';
 *
 *   const axes = createS950Axes();
 *   footer.append(axes.element);
 *   // axes.element.dataset.s950Rayados === '4'   para un test o un badge
 */

import {
  S950_AXES,
  S950_COVERAGE,
  s950AxesWithoutVertical,
} from '../contracts/s950Calibration.js';

//==============================================================================
/** Un eje, ya con lo que hay que pintar y lo que hay que rayar. */
function ejeAPintar (axis) {
  return {
    code: axis.code,

    // El rango del PANEL, que es el eje horizontal. Viene del contrato y es
    // dato aunque la curva no tenga ni un punto medido.
    lo: axis.lo,
    hi: axis.hi,
    span: axis.span,

    label: axis.label,
    scale: axis.scale,
    rises: axis.rises,
    excludeZero: axis.excludeZero,

    /**
     * Si el VERTICAL se puede dibujar, y con el por que cuando no.
     *
     * `needsMeasuredMinimum` no es un adorno: viene del contrato y es la misma
     * regla que usa el paquete compartido para la lista de "lo que me falta".
     * Si aquí se calculara aparte, un panel podría rayar unos y otros no según
     * le saliera, y el recuento dejaria de ser el mismo en los dos sitios.
     */
    verticalDibujable: axis.needsMeasuredMinimum !== true,

    /** La razón del rayado, en castellano, para el `title` y para el lector. */
    motivo: axis.needsMeasuredMinimum
      ? 'eje logaritmico: falta un minimo medido para dibujarlo'
      : null,
  };
}

//==============================================================================
/** Una celda de la rejilla: el nombre de la curva y su estado. */
function celdaDeEje (eje) {
  const celda = document.createElement('div');
  celda.className = 's950-axis';
  celda.dataset.code = eje.code;

  // El atributo de estado va en el DOM y no solo en la clase, porque un test lo
  // consulta con `dataset` y una clase necesita un `classList.contains`. Y va
  // en los dos: la clase para el CSS y el dato para quien lo lea.
  if (eje.verticalDibujable) {
    celda.classList.add('s950-axis--vertical');
    celda.dataset.s950Vertical = 'si';
  } else {
    celda.classList.add('s950-axis--rayado');
    celda.dataset.s950Vertical = 'no';
  }

  // ── el nombre ──
  const nombre = document.createElement('span');
  nombre.className = 's950-axis__name';
  nombre.textContent = eje.label;
  celda.append(nombre);

  // ── el eje HORIZONTAL, que siempre se puede pintar ──
  //
  // Va como texto y no como un `div` vacío con un fondo: un eje necesita sus
  // dos números en los extremos para que se sepa qué escala es, y un rectángulo
  // sin cifras es un adorno.
  const horizontal = document.createElement('span');
  horizontal.className = 's950-axis__horizontal';
  horizontal.dataset.lo = String(eje.lo);
  horizontal.dataset.hi = String(eje.hi);
  horizontal.textContent = `${eje.lo} … ${eje.hi}`;

  // El sentido de crecimiento va en el `title` y no en el texto: es una
  // información que hace falta saberla y no es algo que se lea de un vistazo.
  horizontal.title = eje.rises
    ? 'el valor crece con el mando hacia la derecha'
    : 'el valor baja con el mando hacia la derecha';
  celda.append(horizontal);

  // ── el rótulo del vertical ──
  //
  // Aqui esta el rayado, en forma de texto. Un rayado dibujado con bordes es
  // mas bonito y no se ve: un `title` no aparece sin pasar por encima, y este
  // panel se mira de lejos.
  const vertical = document.createElement('span');

  if (eje.verticalDibujable) {
    vertical.className = 's950-axis__vertical';
    vertical.textContent = eje.scale === 'log' ? 'log' : 'lineal';
  } else {
    vertical.className = 's950-axis__vertical s950-axis__vertical--sin-minimo';
    vertical.textContent = 'sin minimo medido';
    vertical.title = eje.motivo ?? '';
  }

  celda.append(vertical);

  return celda;
}

//==============================================================================
/**
 * Crea la sección de ejes del S950.
 *
 * @returns {{element: HTMLElement, ejes: object[], coverage: object|null,
 *            rayados: object[], pintados: object[]}}
 */
export function createS950Axes () {
  const element = document.createElement('section');
  element.className = 's950-axes';

  // ── el rótulo de COBERTURA, arriba ──
  //
  // Va antes que los ejes y no al pie, porque es lo que hay que leer primero:
  // los ejes son el detalle, la cobertura es el estado. Y el texto sale del
  // contrato —`S950_COVERAGE`—, que ya ha decidido cómo se dice esto, para que
  // dos paneles no cuenten lo mismo de dos maneras.
  const cabecera = document.createElement('header');
  cabecera.className = 's950-axes__header';

  const titulo = document.createElement('h2');
  titulo.className = 's950-axes__title';
  titulo.textContent = 'Ejes del S950';
  cabecera.append(titulo);

  const coverage = document.createElement('span');
  coverage.className = 's950-axes__coverage';

  if (S950_COVERAGE) {
    // `data-s950-cobertura` con los dos numeros, para poder comprobarlo sin
    // tener que parsear la frase. Y la frase entera, que es lo que se lee.
    coverage.dataset.s950Medidas = String(S950_COVERAGE.measured);
    coverage.dataset.s950Total = String(S950_COVERAGE.total);
    coverage.dataset.s950Completo = S950_COVERAGE.complete ? 'si' : 'no';
    coverage.textContent = S950_COVERAGE.label;
  } else {
    // Sin cobertura no se inventa un texto: se dice que no se sabe. Un "0 de 6"
    // puesto a mano seria una afirmacion sobre un dato que no se ha leido.
    coverage.dataset.s950Medidas = '';
    coverage.textContent = 'cobertura desconocida';
  }

  cabecera.append(coverage);
  element.append(cabecera);

  // ── la rejilla de ejes ──
  const rejilla = document.createElement('div');
  rejilla.className = 's950-axes__grid';

  const ejes = S950_AXES.map (ejeAPintar);

  for (const eje of ejes) rejilla.append(celdaDeEje (eje));

  element.append(rejilla);

  // El recuento en el propio `section`, para que un test o un badge lo lean sin
  // tener que recorrer seis celdas.
  const rayados = ejes.filter ((e) => !e.verticalDibujable);
  element.dataset.s950Rayados = String(rayados.length);
  element.dataset.s950Pintados = String(ejes.length - rayados.length);
  element.dataset.s950Total = String(ejes.length);

  // ── y una linea que dice lo que significa el rayado ──
  //
  // Sin ella, un rayado es un simbolo. Con ella, es un estado. Y el sitio donde
  // va —abajo, después de los ejes— es porque es la conclusion: primero se ven
  // los seis, y luego se lee que cuatro de ellos no se pueden pintar todavia.
  const pie = document.createElement('p');
  pie.className = 's950-axes__note';

  if (rayados.length > 0) {
    pie.textContent =
      `${rayados.length} de ${ejes.length} ejes verticales sin mínimo medido: ` +
      'un eje logarítmico no tiene escala sin un valor mínimo real, y ese ' +
      'valor todavía no lo ha medido nadie.';
  } else {
    pie.textContent = 'Los seis ejes verticales se pueden dibujar.';
  }

  element.append(pie);

  return {
    element,
    ejes,
    coverage: S950_COVERAGE,
    rayados,
    pintados: ejes.filter ((e) => e.verticalDibujable),
  };
}

export { s950AxesWithoutVertical };
