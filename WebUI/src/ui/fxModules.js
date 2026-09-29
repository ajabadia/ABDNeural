/**
 * Los MODULOS DE EFECTO del cajon EFECTOS: uno por hueco del rack, con el tema de
 * la FAMILIA del efecto que hay puesto.
 *
 * POR QUE UNA VISTA Y NO UNAS CELDAS MAS DE LA FICHA. Un hueco no son cinco
 * mandos: es un selector de efecto y, DEPUES de elegirlo, un numero de mandos
 * que depende de la fila. Pintar las celdas en la rejilla obligaria a decidir el
 * numero de mandos al construir el panel, cuando el efecto elegido todavia no se
 * sabe; y obligaria a que cambiar de efecto mviera celdas por el lienzo, que es
 * un rearranque de la rejilla entera. El modulo es el unico sitio donde "este
 * hueco tiene tres mandos" puede ser una verdad que cambia sin redibujar nada.
 *
 * LA PUERTA, Y ES LA MISMA QUE LA DE LA MIGRACION. Un modulo solo se llena si el
 * hueco tiene BUS: la prueba es que el store posea `fxNType`. Hoy solo el hueco 1
 * lo tiene, asi que los otros tres se pintan con su chasis, su LED y su titulo, y
 * en el cuerpo una frase que dice que su bus no esta publicado. No se inventan
 * ids para llenar el hueco, ni se les pone el efecto de la cadena por defecto
 * (que seria una segunda verdad: la del motor, en C++).
 *
 * Y la puerta tiene una ventaja sobre una lista de ids escrita a mano: cuando el
 * hueco 2 publique su bus, este modulo se llena solo, sin que nadie edite esta
 * pagina. Un id escrito aqui que el host no declare es un id que el store ignora
 * y un modulo que muestra un mando que no suena.
 *
 * EL TEMA VIENE DE LA FAMILIA, Y LA FAMILIA DEL CATALOGO EXPORTADO. No se
 * importa `contracts/fx-effects.json` en la pagina: ese fichero vive en el
 * repositorio hermano y la pagina se embebe en el plugin como binario, asi que
 * un import ahi haria fallar el build en un clon sin el hermano. El exportador
 * (`NEURONiK_FxExport`) lee el contrato y deja aqui el `family` de cada efecto,
 * que es la unica razon por la que existe `data-fx-theme` en la hoja compartida.
 */

import { FX_CATALOG } from '../../generated/fx-catalog.generated.js';
import { choiceIndexFromNormalized, realFromNormalized } from '../contracts/paramValue.js';
import { formatValue, fromNormalized } from '../contracts/parameters.js';

/** El bus de un hueco: el prefijo de sus ids es el del hueco (1..numSlots). */
const busId = (slot, field) => `fx${slot}${field}`;

/**
 * La fila del catalogo que corresponde a un valor de `fxNType`.
 *
 * El `fxNType` es un `choice` CERO BASADO sobre el indice del catalogo (0 es
 * bypass), y el catalogo exportado numera igual: `effects[0]` es el bypass. Por
 * eso aqui se busca por POSICION y no por `id`: las dos cosas coinciden hoy, y
 * cuando dejen de coincidir lo dira el catalogo, no esta funcion.
 *
 * Un valor que no cae en ninguna fila (un host con un catalogo mas largo que el
 * exportado) devuelve `null` y el modulo se pinta como bypass: un tema
 * equivocado es peor que ningun tema, porque el motor SI esta sonando algo.
 */
function rowForIndex(index) {
  const row = FX_CATALOG.effects[index];

  return row ?? FX_CATALOG.effects[0];
}

/**
 * El texto de un mando del BUS, en las unidades del EFECTO que hay puesto.
 *
 * POR QUE HAY QUE CONVERTER Y NO DELEGAR EN LA CELDA. El hueco publica sus
 * mandos NORMALIZADOS, y el descriptor del host los declara 0..1, asi que la
 * celda escribe "38%". Pero ese 0.38 es el drive de una saturacion (1..8) o el
 * decay de un Schroeder (0.10..0.98): el 38% no es un numero que el usuario
 * pueda relacionar con nada. La fila del catalogo trae el rango y el sesgo
 * REALES de cada mando, y la conversion es la MISMA que usa el resto de la
 * pagina (`fromNormalized`/`formatValue` sobre un descriptor con la forma que
 * piden), no una cuenta propia: dos formulas para el mismo sesgo son dos
 * numeros que acaban discrepando.
 *
 * Y SI EL EFECTO NO TIENE ESE MANDO, SE DEJA EL TEXTO DE LA CELDA: es un mando
 * escondido, y reescribirlo seria pintar unidades de un efecto que ya no esta.
 */
function readoutFor(param, normalized) {
  const descriptor = {
    kind: 'float',
    minValue: param.min,
    maxValue: param.max,
    interval: param.steps > 1 ? (param.max - param.min) / (param.steps - 1) : 0,
    skew: param.skew,
    symmetricSkew: false,
    unit: '',
  };

  return formatValue(descriptor, fromNormalized(descriptor, normalized));
}

/**
 * Un modulo por hueco, en el orden del rack.
 *
 * @param {object[]} controls  los view-models de la ficha (`describeControl`)
 * @returns {{element: HTMLElement, claimBlocks: Function, paint: Function}}
 */
export function createFxModules({ controls = [] } = {}) {
  const controlsById = new Map(controls.map((control) => [control.id, control]));

  const element = document.createElement('div');
  element.className = 'fx-modules';
  element.dataset.fxModules = 'true';

  const modules = [];

  for (let slot = 1; slot <= FX_CATALOG.numSlots; slot += 1) {
    const module = buildModule(slot);
    modules.push(module);
    element.append(module.element);
  }

  return {
    element,
    // El panel cuelga aqui las celdas de la ficha: el mapa id -> contenedor. Los
    // ids que NO salen en el mapa caen en el cuerpo de la ficha (el `frontal`),
    // que es el patron que ya usan LFO y GLOBAL para el control base.
    claimBlocks() {
      const claimed = new Map();

      for (const module of modules) {
        for (const [id, host] of module.hosts) claimed.set(id, host);
      }

      return claimed;
    },
    paint(parameters = {}) {
      for (const module of modules) module.paint(parameters);
    },
  };

  /** Un `.fx-module` con su cabecera, su LED y su cuerpo. El chasis y los once
      temas estan en la hoja compartida (`fx.css`); aqui solo se pone el
      atributo que los elige. */
  function buildModule(slot) {
    const typeId = busId(slot, 'Type');
    const typeControl = controlsById.get(typeId);

    // LA PUERTA. Sin `fxNType` no hay bus, y sin bus no hay nada que repartir:
    // el modulo sale con su chasis y su frase, y el resto de ids planos que hoy
    // manejan ese hueco se quedan en la ficha del lienzo.
    const hasBus = typeControl !== undefined;

    const root = document.createElement('section');
    root.className = 'fx-module';
    root.dataset.fxSlot = String(slot);
    root.dataset.fxTheme = 'bypass';
    root.dataset.fxBus = String(hasBus);
    root.dataset.active = 'false';
    root.setAttribute('aria-label', `Hueco ${slot} de efectos`);

    const header = document.createElement('header');
    header.className = 'fx-module__header';

    const led = document.createElement('span');
    led.className = 'fx-module__led';

    const title = document.createElement('span');
    title.className = 'fx-module__title';

    // El selector de efecto vive en la CABECERA y no en la rejilla de mandos:
    // la cabecera es lo unico que no es un mando, y es donde el nombre se lee.
    // La celda la monta el panel (los controles son suyos), aqui solo se le
    // reserva el sitio.
    const typeHost = document.createElement('span');
    typeHost.className = 'fx-module__type';

    header.append(led, title, typeHost);

    const body = document.createElement('div');
    body.className = 'fx-module__body';

    const params = document.createElement('div');
    params.className = 'fx-module__params';

    const empty = document.createElement('p');
    empty.className = 'fx-module__empty';
    empty.textContent = 'Sin bus: el hueco todavía no publica sus mandos.';

    params.hidden = !hasBus;
    empty.hidden = hasBus;

    body.append(params, empty);
    root.append(header, body);

    // Los mandos del BUS: la ganancia y la mezcla son del hueco, y los `max`
    // parametros son del EFECTO (cuantos hay los dice la fila, no el hueco).
    const hosts = new Map();

    if (hasBus) {
      hosts.set(typeId, typeHost);
      hosts.set(busId(slot, 'Gain'), params);
      hosts.set(busId(slot, 'Mix'), params);

      for (let index = 0; index < FX_CATALOG.maxParams; index += 1) {
        const id = busId(slot, `Param${index + 1}`);

        // Se reclama SOLO si el store lo posee. Un hueco con bus tiene las cuatro
        // posiciones, pero un host que no declare la cuarta no recibe una celda
        // fantasma que el store ignoraria al escribirle.
        if (controlsById.has(id)) hosts.set(id, params);
      }
    }

    return {
      element: root,
      hosts,
      paint(parameters) {
        if (!hasBus) {
          // El titulo de un hueco sin bus dice solo el numero. Ponerle aqui el
          // efecto de la cadena por defecto seria escribir en la pagina la
          // tabla que ya vive en `fxDefaultTypeForSlot`, y las dos copias
          // acabarian discrepando sin que nadie lo viera.
          title.textContent = `HUECO ${slot} · SIN BUS`;
          return;
        }

        // Sin el id en el snapshot se usa el default DEL CONTRATO, que en un
        // `choice` es el indice (`defaultChoiceIndex`). Con 0 a pelo se
        // dibujaria el bypass de un hueco cuyo efecto de serie es la
        // saturacion, y eso se ve como un modulo apagado en el primer frame.
        const normalized = parameters[typeId];
        const index = normalized === undefined
          ? (typeControl.defaultValue ?? 0)
          : choiceIndexFromNormalized(typeControl, normalized);
        const row = rowForIndex(index);
        const mixId = busId(slot, 'Mix');
        const mixControl = controlsById.get(mixId);
        // Un bus sin mezcla no se oye: sin celda de mezcla el LED se apaga, y
        // no se inventa un valor para encenderlo.
        const wet = mixControl ? realFromNormalized(mixControl, parameters[mixId] ?? 0) : 0;

        root.dataset.fxTheme = row.family;
        // EL LED DICE SI EL HUECO SUENA, no si tiene efecto. Un modulo con
        // efecto puesto y la mezcla a cero esta tan apagado como uno en bypass,
        // y ponerlo encendido seria un LED mintiendo.
        root.dataset.active = String(row.id !== 0 && wet > 0);
        title.textContent = `HUECO ${slot} · ${row.displayName}`;

        // Los knobs que el efecto NO tiene se esconden. El bus publica cuatro
        // posiciones para todos los efectos porque el hueco no sabe cuantos
        // mandos va a necesitar el que le pongas: quien lo sabe es la fila. Un
        // mando escondido no es un mando roto, y por eso el que no existe no se
        // pinta: se esconde, y esconderlo aqui es lo que lo delata.
        for (let index = 0; index < FX_CATALOG.maxParams; index += 1) {
          const id = busId(slot, `Param${index + 1}`);
          const cell = root.querySelector(`[data-parameter-id="${id}"]`);

          if (!cell) continue;

          const spec = row.params[index];
          const used = spec !== undefined;

          cell.hidden = !used;

          // El nombre del mando lo da la FILA, no el descriptor: el host llama
          // `fx1Param1` a las cuatro posiciones porque no puede saber de antemano
          // que efecto va a haber en el hueco. Se reescribe el texto de la
          // etiqueta que el knob compartido ya pintó, y solo cuando hay nombre:
          // un mando sin nombre se queda con el del id, que al menos identifica.
          const label = used
            ? root.querySelector(`[data-parameter-id="${id}"] .abd-knob__label`)
            : null;

          if (label) label.textContent = spec.name;

          // Y el readout, en las unidades del efecto (ver `readoutFor`). La
          // celda lo escribe con el 0..1 del bus en su propio `paint`, asi que
          // este va DESPUES: el orden es el del panel (controles, luego vistas).
          const readout = used
            ? root.querySelector(`[data-parameter-id="${id}"] .cell__readout`)
            : null;

          if (readout) readout.textContent = readoutFor(spec, parameters[id] ?? 0);
        }
      },
    };
  }
}
