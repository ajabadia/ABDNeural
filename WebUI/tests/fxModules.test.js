/**
 * Los MODULOS DE EFECTO del cajon EFECTOS: un `.fx-module` por hueco, con el
 * tema de la familia del efecto puesto y solo los mandos que ese efecto declara.
 *
 * QUE COMPRUEBA Y POR QUE HACE FALTA UN TEST PROPIO. Las aserciones de inventario
 * que ya hay (cuantas celdas, cuantos cajones, cuantos ids) dicen que el modulo
 * esta MONTADO. No dicen que diga la verdad, y aqui hay tres cosas que pueden
 * fallar en silencio y seguir verde:
 *
 *   - que el tema venga del EFECTO y no de la FAMILIA (entonces dos efectos de
 *     la misma familia se verian distintos, que es justo lo que el tema por
 *     familia evita);
 *   - que un efecto con menos mandos deje knobs de mas painting knobs muertos;
 *   - que la familia que trae el catalogo tenga un tema en la hoja compartida.
 *     Un modulo con `data-fx-theme="space"` y ninguna regla que lo pinte sale
 *     con los tokens NEUTROS, que es un modulo gris: se ve, no se nota que
 *     este mal, y por eso hace falta una asercion que lo compare con la hoja.
 */

import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import { describe, expect, it, beforeEach } from 'vitest';

import { createFxModules } from '../src/ui/fxModules.js';
import { createVisual } from '../src/ui/visuals.js';
import { SECTIONS, SECTION_VISUALS } from '../src/contracts/sections.js';
import { describeControl } from '../src/contracts/parameters.js';
import { normalizedFromChoiceIndex } from '../src/contracts/paramValue.js';
import { formatValue, fromNormalized } from '../src/contracts/parameters.js';
import { FX_CATALOG } from '../generated/fx-catalog.generated.js';

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, '..');

/** Los ids que el store posee del bus del hueco 1. Los huecos 2..4 no tienen
    ninguno: por eso el modulo se construye con un solo juego de controles. */
const BUS_1 = [
  'fx1Type', 'fx1Gain', 'fx1Mix',
  'fx1Param1', 'fx1Param2', 'fx1Param3', 'fx1Param4',
];

const busControls = (ids = BUS_1) => ids.map(describeControl).filter(Boolean);

/** Un snapshot donde el hueco 1 tiene el efecto del indice dado. */
const withType = (index, rest = {}) => ({
  fx1Type: normalizedFromChoiceIndex(describeControl('fx1Type'), index),
  fx1Mix: 1,
  ...rest,
});

/** El indice del catalogo de un efecto, por su nombre tecnico. */
const indexOf = (technicalName) =>
  FX_CATALOG.effects.findIndex((row) => row.technicalName === technicalName);

const moduleOf = (view, slot) =>
  view.element.querySelector(`.fx-module[data-fx-slot="${slot}"]`);

beforeEach(() => {
  document.body.innerHTML = '';
});

describe('fx-modules / la puerta del bus', () => {
  it('hay UN modulo por hueco, y son los del rack', () => {
    const view = createFxModules({ controls: busControls() });

    expect(view.element.querySelectorAll('.fx-module')).toHaveLength(FX_CATALOG.numSlots);
    // Los ids de los modulos son 1..numSlots, en orden: el que ve el usuario
    // tiene que coincidir con el hueco del motor, y no con la posicion en el
    // array del catalogo.
    expect([...view.element.querySelectorAll('.fx-module')]
      .map((el) => el.dataset.fxSlot))
      .toEqual(['1', '2', '3', '4']);
  });

  it('SOLO el hueco con bus se llena: los demas dicen que no lo tienen', () => {
    const view = createFxModules({ controls: busControls() });
    view.paint(withType(indexOf('saturation')));

    expect(moduleOf(view, 1).dataset.fxBus).toBe('true');
    expect(moduleOf(view, 1).querySelector('.fx-module__params').hidden).toBe(false);

    // Y no es decoracion: los tres modulos sin bus NO tienen donde colgar sus
    // celdas, asi que la vista no puede ni offerselas.
    for (const slot of [2, 3, 4]) {
      const module = moduleOf(view, slot);

      expect(module.dataset.fxBus, `hueco ${slot}`).toBe('false');
      expect(module.querySelector('.fx-module__empty').hidden).toBe(false);
      expect(module.querySelector('.fx-module__params').hidden).toBe(true);
      expect(module.querySelector('[data-parameter-id]')).toBeNull();
    }
  });

  it('SIN ningun control no hay ni un bus: la puerta no tiene exceptions', () => {
    // El caso limite, y el que mas importa: una pagina montada sin host (modo
    // local, o el store todavia sin snapshot) no debe pintar un modulo lleno de
    // mandos que no puede escribir.
    const view = createFxModules({ controls: [] });
    view.paint({});

    for (const slot of [1, 2, 3, 4]) {
      expect(moduleOf(view, slot).dataset.fxBus, `hueco ${slot}`).toBe('false');
    }

    expect(view.claimBlocks().size).toBe(0);
  });

  it('un bus a MEDIAS se queda con lo que hay, sin inventar la posicion que falta', () => {
    // Un host que declara tres de las cuatro posiciones: la cuarta no se reclama,
    // porque una celda de un id que el store no posee se escribiria al vacio.
    const view = createFxModules({ controls: busControls(BUS_1.slice(0, 5)) });
    const claimed = view.claimBlocks();

    expect([...claimed.keys()].sort())
      .toEqual(['fx1Gain', 'fx1Mix', 'fx1Param1', 'fx1Param2', 'fx1Type']);
    expect(claimed.has('fx1Param4')).toBe(false);
  });

  it('el selector de efecto cae en la CABECERA y los mandos en la rejilla', () => {
    const view = createFxModules({ controls: busControls() });
    const claimed = view.claimBlocks();
    const module = moduleOf(view, 1);

    expect(claimed.get('fx1Type').closest('.fx-module__header')).toBeTruthy();
    expect(claimed.get('fx1Mix').closest('.fx-module__params')).toBeTruthy();
    // Y la cabecera del modulo es la del hueco 1, no la de otro.
    expect(claimed.get('fx1Type').closest('.fx-module')).toBe(module);
  });
});

describe('fx-modules / donde CAEN las celdas (no solo que existan)', () => {
  // ESTE BLOQUE EXISTE POR UN FALLO REAL. Con la vista del cajon montada sin los
  // controles de la ficha (que es lo que paso: la fabrica `parameterIds: []`),
  // ningun modulo se pintaba con bus, `claimBlocks()` devovia vacio y las SIETE
  // celdas del bus se caian al cuerpo de la ficha por el `?? body` del panel.
  //
  // Los tests de inventario no lo_VIEWAN: contaban 77 celdas, cuadraban, y la
  // pagina se veia entera y correcta. Lo unico que faltaba era preguntar DONDE
  // estaba cada celda, y esa es la pregunta que se hace aqui.
  const section = SECTIONS.find((candidate) => candidate.id === 'fx');
  const draw = () => createVisual(
    section.drawer.visual,
    [],
    { controls: section.ids.map(describeControl).filter(Boolean) },
  );

  it('la vista del cajon recibe los controles de la FICHA, no los suyos', () => {
    // `parameterIds` va vacio a proposito (una vista no reparte celdas), asi que
    // los view-models tienen que llegar por `options.controls`. Sin eso la vista
    // se fabrica sin controles y no puede saber que huecos tienen bus.
    expect(SECTION_VISUALS[section.drawer.visual].parameterIds).toEqual([]);

    const view = draw();

    expect(view.claimBlocks().size).toBe(section.drawer.blocks[0].length);
  });

  it('los ids del bus caen DENTRO del modulo, y los planos en la ficha', () => {
    const view = draw();
    const claimed = view.claimBlocks();

    // Los SIETE del bus, en su modulo y no en ningun otro sitio.
    for (const id of section.drawer.blocks[0]) {
      expect(claimed.has(id), `${id} debe estar reclamado`).toBe(true);
      expect(claimed.get(id).closest('.fx-module'), id).not.toBeNull();
    }

    // Y los ONCE planos, que son de la rejilla, NO se reclaman: si se reclamaran
    // tambien, el panel los colgaria en el cajon y la ficha se quedaria a medias
    // de lo que el usuario ve sin abrir nada.
    for (const id of section.drawer.frontal) expect(claimed.has(id), id).toBe(false);

    // El reparto entre ambos es EXHAUSTIVO: ningun id de la ficha se queda sin
    // destino, que es la forma de que un id caiga en el sitio que no toca.
    expect(claimed.size + section.drawer.frontal.length).toBe(section.ids.length);
  });

  it('el hueco 1 es el unico que se pinta con bus, y dice que efecto lleva', () => {
    const view = draw();
    view.paint({ fx1Type: normalizedFromChoiceIndex(describeControl('fx1Type'), 4), fx1Mix: 1 });

    expect(moduleOf(view, 1).dataset.fxBus).toBe('true');
    expect(moduleOf(view, 1).dataset.fxTheme).toBe(FX_CATALOG.effects[4].family);
    // Los tres sin bus no tienen ni una celda: sin destino no hay celda.
    for (const slot of [2, 3, 4]) {
      expect(moduleOf(view, slot).querySelector('[data-parameter-id]'), `hueco ${slot}`)
        .toBeNull();
    }
  });
});

describe('fx-modules / el tema por FAMILIA', () => {
  it('cada efecto toma el tema de SU familia, no el suyo', () => {
    const view = createFxModules({ controls: busControls() });

    for (const row of FX_CATALOG.effects) {
      view.paint(withType(row.id));

      expect(moduleOf(view, 1).dataset.fxTheme, row.displayName).toBe(row.family);
    }
  });

  it('dos efectos de la misma familia se ven IGUAL (es el punto del tema)', () => {
    // El coro (1) y el coro BBD (6) son motores distintos, familia `chorus`. Si
    // el atributo saliera del efecto y no de la familia, aqui se verian dos.
    const view = createFxModules({ controls: busControls() });

    view.paint(withType(indexOf('chorus')));
    const chorus = moduleOf(view, 1).dataset.fxTheme;

    view.paint(withType(indexOf('bbd')));
    const bbd = moduleOf(view, 1).dataset.fxTheme;

    expect(chorus).toBe(bbd);
    expect(chorus).toBe('chorus');
  });

  it('el bypass se pinta como bypass, no como el ultimo tema que hubiera', () => {
    const view = createFxModules({ controls: busControls() });

    view.paint(withType(indexOf('saturation')));
    expect(moduleOf(view, 1).dataset.fxTheme).toBe('distortion');

    view.paint(withType(0));
    expect(moduleOf(view, 1).dataset.fxTheme).toBe('bypass');
  });

  it('el descriptor y el catalogo tienen el MISMO numero de efectos', () => {
    // El invariante que hace que `rowForIndex` no reciba un indice fuera de
    // rango: el `fxNType` recorta al numero de opciones del contrato, asi que
    // solo se sale del catalogo si el host declara mas efectos de los que el
    // exportador connu. Cuando eso pase, la vista cae a bypass (un tema
    // equivocado es peor que ninguno, porque el motor SI esta sonando algo),
    // pero lo que se vigila aqui es la DESINCRONIA, que es lo que hay que
    // arreglar: se regenera el catalogo y el contrato, en ese orden.
    const choices = describeControl('fx1Type').options;

    expect(choices).toHaveLength(FX_CATALOG.effects.length);
    // Y en el MISMO orden, que es lo que hace que el indice del selector y la
    // fila del catalogo sean el mismo numero. Un efecto nuevo insertado en
    // medio de una de las dos tablas se ve aqui, no en un modulo con el tema
    // del efecto de al lado.
    expect(choices).toEqual(FX_CATALOG.effects.map((row) => row.displayName));
  });

  it('TODA familia del catalogo tiene tema en la hoja compartida', () => {
    // LA ASERCION QUE SOSTIENE AL RESTO. Sin ella, un modulo puede llevar un
    // `data-fx-theme` perfectamente valido y salir GRIS: `fx.css` no tiene regla
    // para esa familia, cae a los tokens neutros y no se ve por ningun lado.
    // El fallo es silencioso por construccion, asi que se compara contra la hoja.
    const css = readFileSync(
      resolve(root, 'node_modules', '@abdsynths', 'shared', 'styles', 'components', 'fx.css'),
      'utf8',
    );

    const themed = new Set(
      [...css.matchAll(/\.fx-module\[data-fx-theme='([a-z-]+)'\]/g)].map((m) => m[1]),
    );

    const families = new Set(FX_CATALOG.effects.map((row) => row.family));

    // SOLO EN UN SENTIDO, y el otro no se comprueba a proposito: la hoja es el
    // vocabulario COMPLETO (once familias para los 57 efectos del contrato) y
    // este catalogo solo usa cinco. Un tema sin ningun efecto aqui es lo
    // normal, y hacerlo fallar obligaria a inflar el catalogo de mentira.
    expect([...families].filter((family) => !themed.has(family))).toEqual([]);
  });
});

describe('fx-modules / los mandos que el efecto DECLARA', () => {
  const cellOf = (view, id) => view.element.querySelector(`[data-parameter-id="${id}"]`);

  /** Las celdas las cuelga el PANEL, no la vista: aqui se factoryan las
      minimas que el panel crearia (una celda por id, con su etiqueta de knob),
      para no levantar el panel entero y poder mirar un modulo suelto. */
  const mountBusCells = (view, ids = BUS_1) => {
    const claimed = view.claimBlocks();

    for (const id of ids) {
      const host = claimed.get(id);

      if (!host) continue;

      const cell = document.createElement('div');
      cell.dataset.parameterId = id;

      if (id !== 'fx1Type') {
        const label = document.createElement('span');
        label.className = 'abd-knob__label';
        label.textContent = id;
        cell.append(label);

        // El readout lo escribe la celda en su propio paint; aqui se deja puesto
        // con el texto que pondria la celda, para poder distinguir "la vista lo
        // reescribio" de "no lo toco".
        const readout = document.createElement('span');
        readout.className = 'cell__readout';
        readout.textContent = id;
        cell.append(readout);
      }

      host.append(cell);
    }
  };

  it('la saturacion (UN mando) deja escondidos los otros tres', () => {
    const view = createFxModules({ controls: busControls() });
    mountBusCells(view);
    view.paint(withType(indexOf('saturation')));

    // La vista pinta los cuatro; el panel cuelga las celdas; lo que se ESCONDE
    // es lo que el efecto no usa. Un knob escondido no es un knob roto.
    expect(cellOf(view, 'fx1Param1').hidden).toBe(false);
    expect(cellOf(view, 'fx1Param2').hidden).toBe(true);
    expect(cellOf(view, 'fx1Param3').hidden).toBe(true);
    expect(cellOf(view, 'fx1Param4').hidden).toBe(true);
  });

  it('el Schroeder (CUATRO) no esconde ninguno', () => {
    const view = createFxModules({ controls: busControls() });
    mountBusCells(view);
    view.paint(withType(indexOf('schroeder')));

    for (const id of ['fx1Param1', 'fx1Param2', 'fx1Param3', 'fx1Param4'])
      expect(cellOf(view, id).hidden, id).toBe(false);
  });

  it('cambiar de efecto vuelve a esconder lo que el nuevo no usa', () => {
    // El caso que importa: la lista de mandos cambia CON EL EFECTO. Si solo se
    // pintara en el primer frame, un efecto de cuatro a uno dejaria tres knobs
    // muertos en pantalla.
    const view = createFxModules({ controls: busControls() });
    mountBusCells(view);

    view.paint(withType(indexOf('schroeder')));
    expect(cellOf(view, 'fx1Param4').hidden).toBe(false);

    view.paint(withType(indexOf('saturation')));
    expect(cellOf(view, 'fx1Param4').hidden).toBe(true);

    // Y el recuento NO se escribe a mano: se deriva de la fila, que es la
    // unica que sabe cuantos mandos tiene el efecto. Escribir el 2 del retardo
    // aqui seria una tercera cuenta del numero de mandos, y la que se
    // equivocaria en silencio el dia que el motor cambie.
    view.paint(withType(indexOf('delay')));

    const delay = FX_CATALOG.effects[indexOf('delay')];

    for (let k = 0; k < FX_CATALOG.maxParams; k += 1) {
      const id = `fx1Param${k + 1}`;

      expect(cellOf(view, id).hidden, id).toBe(k >= delay.params.length);
    }
  });

  it('el readout va en las unidades del EFECTO, no en el 0..1 del bus', () => {
    // El hueco publica normalizados y el host los declara 0..1, asi que la celda
    // escribe "38%". Ese 0.38 es el decay de un Schroeder, que va de 0.10 a 0.98:
    // un porcentaje no le dice al usuario nada. La fila trae el rango real y el
    // sesgo, y la conversion es la MISMA de la pagina (no una cuenta nueva).
    const view = createFxModules({ controls: busControls() });

    mountBusCells(view);
    view.paint(withType(indexOf('schroeder'), { fx1Param1: 0.5 }));

    const decay = FX_CATALOG.effects[indexOf('schroeder')].params[0];

    // Y el valor ESPERADO sale de las MISMAS funciones que usa la vista
    // (`fromNormalized`/`formatValue` de la pagina), no de una cuenta escrita
    // aqui: repetirla seria una cuarta implementacion del sesgo, y la que se
    // equivoca en silencio el dia que la de la pagina cambie.
    const descriptor = {
      kind: 'float', minValue: decay.min, maxValue: decay.max,
      interval: 0, skew: decay.skew, symmetricSkew: false, unit: '',
    };

    expect(cellOf(view, 'fx1Param1').querySelector('.cell__readout').textContent)
      .toBe(formatValue(descriptor, fromNormalized(descriptor, 0.5)));
  });

  it('un mando escondido se queda con el texto de la celda, no con units de otro', () => {
    // Si se reescribiera el readout de un mando que el efecto no tiene, estaria
    // pintando las unidades de un efecto que ya no suena.
    const view = createFxModules({ controls: busControls() });

    mountBusCells(view);
    view.paint(withType(indexOf('saturation')));

    expect(cellOf(view, 'fx1Param2').querySelector('.cell__readout').textContent)
      .toBe('fx1Param2');
  });

  it('el nombre del mando lo pone la FILA, no el id del host', () => {
    // `fx1Param1` se llama asi porque el host no puede saber de antemano que
    // efecto va a haber en el hueco. El nombre que el usuario lee ("drive") solo
    // existe en el catalogo, asi que se reescribe la etiqueta al pintar.
    const view = createFxModules({ controls: busControls() });
    mountBusCells(view);

    view.paint(withType(indexOf('saturation')));
    expect(cellOf(view, 'fx1Param1').querySelector('.abd-knob__label').textContent)
      .toBe('drive');

    view.paint(withType(indexOf('schroeder')));
    expect(cellOf(view, 'fx1Param4').querySelector('.abd-knob__label').textContent)
      .toBe('predelay');
  });
});

describe('fx-modules / el LED y el titulo', () => {
  const titleOf = (view) => moduleOf(view, 1).querySelector('.fx-module__title').textContent;

  it('el LED dice si el hueco SUENA, no si tiene efecto', () => {
    const view = createFxModules({ controls: busControls() });

    view.paint(withType(indexOf('saturation'), { fx1Mix: 0 }));
    expect(moduleOf(view, 1).dataset.active).toBe('false');

    view.paint(withType(indexOf('saturation'), { fx1Mix: 1 }));
    expect(moduleOf(view, 1).dataset.active).toBe('true');

    // Y con el bypass puesto, aunque la mezcla este al maximo, no suena: un LED
    // encendido sobre un hueco en bypass es un LED mintiendo.
    view.paint(withType(0, { fx1Mix: 1 }));
    expect(moduleOf(view, 1).dataset.active).toBe('false');
  });

  it('el titulo dice el hueco Y el efecto', () => {
    const view = createFxModules({ controls: busControls() });

    view.paint(withType(indexOf('reverb')));
    expect(titleOf(view)).toContain('HUECO 1');
    expect(titleOf(view)).toContain(FX_CATALOG.effects[indexOf('reverb')].displayName);
  });

  it('sin snapshot se pinta el efecto POR DEFECTO, no el bypass', () => {
    // El primer frame llega antes que el snapshot. Con un 0 a pelo se dibujaria
    // un modulo apagado en un hueco cuyo efecto de serie es la saturacion, y eso
    // se ve como un fallo aunque sea solo un frame.
    const view = createFxModules({ controls: busControls() });
    view.paint({});

    const porDefecto = describeControl('fx1Type').defaultValue;

    expect(titleOf(view)).toContain(FX_CATALOG.effects[porDefecto].displayName);
  });

  it('un modulo sin bus NO dice que efecto lleva: no lo sabe', () => {
    // Ponerle el de la cadena por defecto seria escribir aqui la tabla que ya
    // vive en `fxDefaultTypeForSlot`, y las dos copias acabarian discrepando.
    const view = createFxModules({ controls: busControls() });
    view.paint(withType(indexOf('saturation')));

    expect(moduleOf(view, 2).querySelector('.fx-module__title').textContent)
      .toContain('SIN BUS');
    expect(moduleOf(view, 2).querySelector('.fx-module__title').textContent)
      .not.toContain('Saturation');
  });
});
