/**
 * La ayuda de gestos de la cabecera (ui/gestureHelp.js): lo que se documenta
 * de los controles COMPARTIDOS, que son de otro repo y no documentan sus gestos
 * al mundo.
 */

import { describe, expect, it } from 'vitest';

import { CORNER_KEYBOARD_HINT } from '../src/ui/xyPad.js';
import { GESTURE_HELP_ITEMS, GESTURE_HELP_SUMMARY, createGestureHelp } from '../src/ui/gestureHelp.js';

describe('la ayuda de gestos de la cabecera', () => {
  it('es un <details> plegado, con su resumen y su lista', () => {
    const { element } = createGestureHelp();
    document.body.append(element);

    expect(element.tagName).toBe('DETAILS');
    expect(element.open).toBe(false);
    expect(element.querySelector('summary').textContent).toBe(GESTURE_HELP_SUMMARY);

    const items = [...element.querySelectorAll('li')];
    expect(items).toHaveLength(GESTURE_HELP_ITEMS.length);
    for (const [index, [term]] of GESTURE_HELP_ITEMS.entries())
      expect(items[index].textContent).toContain(`${term}:`);

    // Abrir es un gesto del usuario (nativo): el contenido esta ahi.
    element.open = true;
    expect(element.querySelector('ul')).not.toBeNull();

    element.remove();
  });

  it('documenta los CUATRO gestos del boton redondo, con sus magnitudes medidas', () => {
    const text = GESTURE_HELP_ITEMS.map(([, description]) => description).join(' | ');

    // Arrastrar y el doble clic (que va al MINIMO del rango: `value: 0` al
    // construir el knob, no al default del contrato — ver el doc del modulo).
    expect(text).toContain('arrastrar = valor');
    expect(text).toContain('doble clic = al mínimo del rango');
    // Shift = 0.2x en drag-core (`event.shiftKey ? 0.2 : 1.0`).
    expect(text).toContain('1/5 de velocidad');
    // Rueda = un veinteavo de la pista por muesca.
    expect(text).toContain('rueda = paso fino');
    // Teclado = SOLO las cuatro flechas a paso 0.01. Ni RePag ni Inicio/Fin:
    // los del aro, que tienen otro manejador. Documentarlos aqui seria mentira.
    expect(text).toContain('flechas = ±1% del rango');
    expect(text).not.toMatch(/flechas.*RePag|flechas.*Inicio/);
  });

  it('las pistas de teclado y de esquinas no se reescriben: vienen de su sitio', () => {
    const text = GESTURE_HELP_ITEMS.map(([, description]) => description).join(' | ');

    // La de las esquinas es la MISMA constante que pintan los `title` del pad
    // (xyPad.js): dos palabras con dos fuentes seria el defecto que este
    // fichero viene a evitar, aqui y en la ayuda.
    expect(text).toContain(CORNER_KEYBOARD_HINT);

    // Y el pad y el aro NO se repiten: solo se remite a donde ya estan
    // documentados (su propio title y el cajon de MODELOS).
    expect(text).toContain('cajón de MODELOS');
    expect(text).not.toContain('1/10');   // el fino del pad: en su title
  });
});
