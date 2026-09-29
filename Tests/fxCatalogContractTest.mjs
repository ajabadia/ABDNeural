/**
 * Paridad del CATALOGO de efectos: lo que exporta el motor de NEURONiK
 * (`WebUI/generated/fx-catalog.generated.js`) contra el CONTRATO COMPARTIDO
 * (`ABDSharedAssets/contracts/fx-effects.json`).
 *
 * QUE COMPRUEBA, Y POR QUE HACE FALTA UN TEST ENTRE DOS REPOSITORIOS.
 *
 * El id de un efecto es el vocabulario que comparten varios productos, y el
 * riesgo no es que este repositorio se equivoque al escribirlo: es que el otro
 * lo renumere. Si ABDEep inserta un efecto en su fabrica y desplaza los ids de
 * despues, el 10 dejara de ser "Stereo Chorus" en el contrato mientras aqui
 * sigue pinning al 10, y el sintoma seria un modulo con el nombre y la familia
 * de un efecto que ya no es ese. Ese fallo no aparece en ningun test de este
 * repositorio —los dos ficheros estan bien, estan diciendo cosas distintas— asi
 * que el unico sitio donde se ve es uno que lee los dos.
 *
 * La ASIMETRIA ES DELIBERADA y es la parte importante del test:
 *
 *   - Para las filas `aligned: true`, id, nombre y familia tienen que COINCIDIR
 *     con el contrato. Si el contrato cambia, aqui se cae.
 *   - Para las filas `aligned: false` (reservas; ver Source/DSP/FxCatalogue.h),
 *     el test exige que el id este OCUPADO de verdad en el contrato pero que se
 *     diga OTRA cosa. Es decir: la reserva tiene que seguir siendo coherente
 *     (su id existe y pertenece a la familia que hemos reservado) sin
 *     pretender que el efecto sea el mismo. El dia que el contrato gane la fila,
 *     este test CAE, y entonces hay que poner `aligned: true` en el C++.
 *
 * LO QUE NO SE COMPRUEBA, y por que. La columna `params` del contrato compartido
 * NO se compara con la del catalogo, y no por pereza: cuenta los mandos que
 * acepta la implementacion de ABDEep, no la de este motor. El coro tiene alli 11
 * y aqui 2, el delay 12 y aqui 2, la reverberacion 12 y aqui 4. Compararlas
 * haria fallar un contrato que esta bien. Los mandos se comparan contra el
 * MOTOR, que es donde estan, y para eso esta `NEURONiK_FxCatalogueTest`.
 */

import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, '..');

let failures = 0;

function check(condition, what) {
  if (condition) {
    console.log(`  [ok]   ${what}`);
  } else {
    console.log(`  [FAIL] ${what}`);
    failures += 1;
  }
}

/** El catalogo exportado por el motor. Se lee el `.json`: es el mismo contenido
 *  que el `.js` y asi este test no necesita transpilar nada. */
const catalog = JSON.parse(
  readFileSync(resolve(root, 'WebUI', 'generated', 'fx-catalog.generated.json'), 'utf8'),
);

// El contrato vive en ABDSharedAssets, un repositorio hermano. Si no esta a mano
// (un clon de este repo solo) el test se salta CON EL MOTIVO, nunca en verde:
// un parity check que se salta sin decir nada es peor que no tenerlo.
const sharedPath = resolve(root, '..', 'ABDSharedAssets', 'contracts', 'fx-effects.json');

let shared;

try {
  shared = JSON.parse(readFileSync(sharedPath, 'utf8'));
} catch (error) {
  console.log(`  [skip] ABDSharedAssets/contracts/fx-effects.json no esta a mano (${error.code})`);
  console.log('         Sin el, la paridad de ids no se puede comprobar.');
  process.exit(0);
}

const sharedById = new Map(shared.effects.map((effect) => [effect.id, effect]));

console.log(`catalogo de NEURONiK: ${catalog.effects.length} filas `
  + `(bypass incluido), bus de ${catalog.maxParams} mandos\n`);

// --- 1. Las filas alineadas COINCIDEN con el contrato -----------------------
const aligned = catalog.effects.filter((effect) => effect.aligned && effect.sharedId !== 0);

check(aligned.length > 0, `el catalogo declara al menos un efecto alineado (${aligned.length})`);

for (const effect of aligned) {
  const row = sharedById.get(effect.sharedId);

  if (!row) {
    check(false, `el id ${effect.sharedId} ("${effect.name}") sigue existiendo en el contrato`);
    continue;
  }

  check(row.name === effect.name,
    `id ${effect.sharedId}: el nombre coincide ("${effect.name}")`);
  check(row.family === effect.family,
    `id ${effect.sharedId}: la familia coincide ("${effect.family}")`);
}

// --- 2. Las reservas son coherentes, y NO se hacen pasar por alineadas -----
const reserved = catalog.effects.filter((effect) => !effect.aligned);

for (const effect of reserved) {
  const row = sharedById.get(effect.sharedId);

  check(row !== undefined,
    `reserva ${effect.sharedId} ("${effect.displayName}"): el id esta ocupado en el contrato`);
  // Una reserva lleva SIEMPRE el nombre que el contrato da a ese id (es el sitio
  // donde caeria), asi que compararlo con `name` no distinguiria nada: seria
  // igual por construccion. Lo que dice si una reserva es una reserva es que el
  // motor que nosotros ponemos ahi NO se llame como lo que el contrato llama a
  // ese id: id 1 es "Hall" en el contrato y aqui lo ocupa nuestra reverberacion
  // de FreeVerb, que se llama "Reverb". Si algun dia se llamaran igual, la
  // reserva habria dejado de ser una reserva y habria que poner `aligned: true`.
  check(row !== undefined && row.name !== effect.displayName,
    `reserva ${effect.sharedId}: el contrato dice "${row?.name}" y aqui va "${effect.displayName}", que no es lo mismo`);
  check(row !== undefined && row.family === effect.family,
    `reserva ${effect.sharedId}: la familia reservada (${effect.family}) es la del contrato`);
}

// --- 3. Los ids del catalogo son unicos -----------------------------------
const ids = catalog.effects.map((effect) => effect.sharedId);

check(new Set(ids).size === ids.length, 'ningun id compartido esta repetido en el catalogo');
check(ids.every((id) => id !== 0 || catalog.effects[0].displayName === 'Bypass'),
  'el id 0 es el bypass y solo el bypass');

// --- 4. El bus cabe en lo que el motor acepta ------------------------------
check(catalog.maxParams > 0 && catalog.maxParams <= 12,
  `el bus de ${catalog.maxParams} mandos cabe en el maximo del motor (12)`);

for (const effect of catalog.effects) {
  check(effect.params.length <= catalog.maxParams,
    `"${effect.displayName}": pide ${effect.params.length} mandos, caben en el bus`);
}

// --- 5. Ningun mando sin nombre ni rango -----------------------------------
for (const effect of catalog.effects) {
  for (const param of effect.params) {
    check(typeof param.name === 'string' && param.name.length > 0,
      `"${effect.displayName}".${param.name}: tiene nombre`);
    check(param.max > param.min,
      `"${effect.displayName}".${param.name}: el rango tiene los dos extremos`);
    check(param.skew > 0,
      `"${effect.displayName}".${param.name}: el sesgo es positivo`);
    check(param.steps === 0 || param.steps > 1,
      `"${effect.displayName}".${param.name}: los pasos discretos son 0 o > 1`);
  }
}

console.log('');

if (failures > 0) {
  console.log(`FxCatalog contract: ${failures} problema(s). El catalogo y el contrato `
    + 'compartido se han desviado.');
  process.exit(1);
}

console.log('FxCatalog contract: OK (ids alineados y reservas coherentes con el contrato compartido).');
