// ============================================================================
//
//  verify_all_node.js -- LA PARTE DE NODE DE LOS DOS VERIFY, EN UN SOLO FICHERO
//
//  verify_all.sh y verify_all.bat hacen exactamente lo mismo, y para eso
//  comparten este fichero en vez de llevar cada uno su copia del programa.
//
//  POR QUE ESTA AQUI Y NO DENTRO DE CADA SCRIPT
//
//  Antes cada .bat y cada .sh llevaba dentro su copia del programa de node, y
//  no podia ser de otra manera: en batch el programa va dentro de la linea de
//  comandos, entre comillas, con EnableDelayedExpansion activo. Eso obliga a
//  tres apenos que se pagan cada vez que se toca el programa:
//
//    - Ningun `!` en el programa. El `!` abre expansion, lo que sigue se busca
//      en el entorno y sale vacio, y a node le llega troceado. Medido.
//    - Ningun `"`, ni siquiera `\"`: en batch una barra invertida delante de
//      una comilla LLEGA a node. Las comillas se sacan con
//      String.fromCharCode(34).
//    - Ningun backtick, y los saltos de linea con chr(10) a mano.
//
//  Y el coste no es solo lo que se ve: una copia es una lista mas. El
//  programa del limpiador de procesos son 2.900 caracteres duplicated en dos
//  sitios, y si uno se toca y el otro no, los dos scripts dan distinta cuenta
//  del mismo lock sin que nada lo delate.
//
//  En un fichero no hay nada de eso: el programa es un programa, con sus
//  comillas, sus `!==` y sus plantillas. Lo unico que va en la linea de
//  comandos es la llamada, que no tiene ningun caracter prohibido.
//
// ============================================================================
//
//  USO:  node verify_all_node.js <orden> [argumentos]
//
//  ordenes:
//
//    vivo       <pid> [destino]          true|false
//    resumen    <entradas> <destino> <conocidos> <tests> <lentos> <tocoTimeout>
//               [pasos] [huerfanos]
//               OJO: `entradas` y `lentos` son FICHEROS (la cuenta sale de
//               contarlos); `conocidos`, `tests` y `tocoTimeout` son numeros.
//               Un numero donde toca un fichero no da error: node lo lee como
//               descriptor. Ver `rutaValida`.
//    bateria    <json> <testdir> <config> [a] [n] [cuenta]
//               las entradas de la lista cuyo test ya no esta
//    compara    <resumenA> <resumenB> [nombreA] [nombreB]  0 si dicen lo mismo
//    pidpropio  [destino]                el PID del padre, que es la sesion
//    leepid     <fichero> [destino]      el PID que hay escrito en un fichero
//    limpia     <pid> [destino] [ms]    ok|hijos|muertos|resisten, o vivo
//    conocidos  <json> [destino]         pares "clave<TAB>motivo<NUL>"
//    cuenta     <json> [destino]         numero de entradas
//    motivo     <json> <test> [destino]  el motivo de un rojo, o ""
//    arreglados <json> <rojos> [a] [n]   el bloque ARREGLO, por pantalla
//    targets    <testdir> <config>
//               los targets que hay que compilar para que la bateria mida el
//               codigo de ahora: uno por cada test nativo, ordenados.
//    rancios    <testdir> <config> [a] [n] [--sin-build] [construidos...]
//               tests cuyo .exe es mas viejo que el codigo que se ha escrito.
//               Ver el aviso: sin esto, un rojo de binario rancio se lee como un
//               rojo de codigo, y salen cuatro falsos de golpe.
//
//  `destino` es SIEMPRE el ultimo argumento y es opcional: si no se pasa, el
//  resultado sale por stdout. Y el destino NUNCA puede ser uno de los
//  argumentos de entrada: `escribe` lo comprueba y se niega. No es
//  defensiva teorica, es un fallo que se cometio aqui al escribir el primer
//  prototipo, donde `cuenta` escribia el numero sobre el propio JSON y lo
//  dejaba en un byte. Un fichero de entrada que se puede sobrescribir por
//  accidente no es un fichero de entrada.
//
//  Y la FIRMA de vivo y de limpia es (pid, destino) con el destino detras:
//  con el orden al reves, `vivo(undefined)` da false SIEMPRE y un proceso vivo
//  parece muerto. Es el fallo que costo una tarde.
//
//  Cuando hay fichero, el resultado se escribe ahi Y se escribe en stdout, con
//  el MISMO texto: quien lee un fichero de una linea vacio (que es lo que pasa
//  al coger stdout con `> fichero` en windows) tiene el mismo numero de
//  fuentes que puede leer.
//
//  `resumen`, `bateria` y `compara` son la EXCEPCION a la regla del destino
//  ultimo: el resumen tiene dos ficheros de entrada y uno de salida, porque
//  lo que sale no es un texto para imprimir sino un estado para comparar, y
//  poner el destino en medio es lo unico que no obliga a los dos scripts a
//  pasar sus propios ficheros temporales como argumentos sueltos. `bateria`
//  escribe un numero suelto al lado de su bloque, porque el bloque es para el
//  ojo y el numero es para el resumen. `compara` no escribe nada: se lee en
//  pantalla, y su codigo de salida es la respuesta.
//
// ============================================================================

'use strict';

const fs = require('fs');
const cp = require('child_process');
const os = require('os');
const path = require('path');

const q = String.fromCharCode(34); // " sin escribirla

// ── EJECUTAR UN PROGRAMA DE WINDOWS ─────────────────────────────────────────
//
// SIEMPRE execFileSync con ARRAY de argumentos, nunca execSync. Con execSync
// (que pasa por cmd) o desde el bash, MSYS convierte `/fo` en una RUTA y
// tasklist responde "Argumento u opcion no valido - C:/Program Files/Git/fo":
// lista vacia, y un proceso vivo parece muerto. Medido, en los dos scripts.
function run (cmd, args, ms) {
    try {
        return String(cp.execFileSync (cmd, args, {
            encoding: 'utf8',
            timeout: ms || 60000,
            stdio: ['ignore', 'pipe', 'ignore']
        }));
    } catch (e) {
        return '';
    }
}

// UN ARGUMENTO QUE ES UN NUMERO NO ES UNA RUTA, Y NODE NO TE AVISA.
//
// `fs.existsSync('0')` y `fs.readFileSync('0')` no abren el fichero `0`: leen
// el descriptor 0, que es la entrada estandar. Es silencioso, y del tipo de
// fallo que no se ve hasta que alguien mira el repositorio y se encuentra
// siete ficheros basura en la raiz con un numero de nombre.
//
// Y no es teorico: este es el fallo medido en este repo. `vivo <pid>
// <destino>` con el destino en el argumento equivocado escribe `false` en un
// fichero llamado como el PID, y `limpia` escribe `sin-pid` en el `0`. En la
// raiz del proyecto quedaron `0`, `4`, `999999`, `20632`, `25096`, `25228` y
// `27016`, SIN RASTREAR, que es decir que un `git add -A` los mete en el
// indice. Se comprueba aqui, en el unico sitio por donde pasa todo.
//
// El numero es un dato, no una ruta, y los dos tienen la misma forma. Lo que se
// distingue es el papel que cumplen, no el tipo: el guard de `destinoEsEntrada`
// protege de escribir encima, este protege de escribir en la nada.
const ES_NUMERO = /^[0-9]+$/;

function rutaValida (valor, etiqueta, contexto) {
    const v = String (valor == null ? '' : valor);
    if (v !== '' && ES_NUMERO.test (v)) {
        process.stderr.write (
            'verify_all_node.js: ' + etiqueta + ' de ' + contexto + ' es "' + v +
            '", que es un numero, no una ruta.\n' +
            '          Node lo leeria como el descriptor de fichero ' + v +
            ' (la entrada estandar), no como un nombre.\n' +
            '          Medido: asi se escribieron siete ficheros basura en la raiz del proyecto.\n');
        process.exit (2);
    }
    return v;
}

// El destino no puede ser una entrada. Ver la cabecera: hay un motivo concreto
// y medido para esta comprobacion (una cuenta escribio su numero sobre el
// propio JSON de los conocidos y lo dejo en un byte).
function destinoEsEntrada (destino, entradas) {
    for (const e of entradas || []) {
        if (String (e || '') !== '' && path.resolve (destino) === path.resolve (String (e))) {
            process.stderr.write ('verify_all_node.js: el destino ES una entrada: ' + destino + '\n');
            process.exit (2);
        }
    }
    return true;
}

// Escribe en `destino` y en stdout a la vez, con el MISMO texto: quien lee un
// fichero de una linea vacio (que es lo que pasa al coger stdout con
// `> fichero` en windows) tiene el mismo numero de fuentes que puede leer.
function escribe (destino, valor, entradas) {
    if (destino) {
        rutaValida (destino, 'el destino', 'una escritura');
        destinoEsEntrada (destino, entradas);
        fs.writeFileSync (destino, valor);
    }
    process.stdout.write (valor);
}

// Un numero suelto a un fichero, SIN ir a stdout. Lo usa `bateria`, que
// escribe un bloque para el ojo y un numero para el resumen: si el numero
// tambien saliera por pantalla, el bloque y el numero se separan y el que lee
// el resumen tiene que adivinar cual de las dos lineas es el numero.
function escribeCuenta (destino, valor, entradas) {
    if (!destino) return;
    rutaValida (destino, 'el destino de la cuenta', 'una escritura');
    destinoEsEntrada (destino, entradas);
    fs.writeFileSync (destino, valor + '\n');
}

// ── VIVO ───────────────────────────────────────────────────────────────────
//
// El codigo de salida de tasklist es 0 tanto si el PID existe como si no, asi
// que no sirve: se mira si alguna linea empieza por comilla, que es lo que
// tiene el CSV de una tarea. El "no hay tareas que coincidan" es texto libre y
// depende del idioma del Windows.
//
// Y CON FILTRO `PID eq N`, nunca la lista entera: la lista entera de esta
// maquina no termina ni en 120 s (son ~350 procesos y el coste es de ahi para
// arriba). Con filtro son 0,9 s. Una comprobacion que tarda 120 s no se puede
// hacer al arrancar de un script.
//
// Un PID de 0 no es un proceso, es la ausencia de PID: 0 es "no vivo". Y un
// PID con letras tampoco: aqui no hay nombres, solo numeros.
function vivo (pid) {
    pid = String (pid === undefined || pid === null ? '' : pid).trim();
    if (!/^[1-9][0-9]*$/.test (pid)) return false;

    const salida = run ('tasklist', ['/fo', 'csv', '/nh', '/fi', 'PID eq ' + pid], 20000);
    return salida.split (/\r?\n/).some (linea => linea.trim().charAt (0) === q);
}

// ── LIMPIA: LOS PROCESOS QUE DEJO UNA SESION MUERTA ────────────────────────
//
// Cuando la sesion se cierra a mitad de un verify, el lock se queda; y con el
// se quedan los procesos que ese verify habia lanzado: ctest, los tests que
// cuelgan de ctest, el npx de vitest. Quitar el lock y seguir como si nada
// deja a esos procesos escribiendo en el MISMO build-reference que el verify
// que acaba de empezar, y el rojo que sale diez minutos despues no tiene nada
// que ver con el codigo.
//
// Y SOLO cuando el lock esta rancio, que es el unico caso en que esos
// procesos son huerfanos de verdad: si el lock esta vivo no se llama. Quien
// llama ya ha comprobado que el PID esta muerto, y aqui se vuelve a comprobar
// antes de tocar nada, por si el PID se ha reciclado entre medias.
//
// EL ARBOL DE PROCESOS SE PIDE POR WMI CON cscript, NO CON POWERSHELL. En esta
// maquina PowerShell tarda 6 s solo en arrancar, y de 14 a 46 s en la
// consulta: medido, tres veces. Con cscript + WMI son 0,8 s. `wmic` ya no viene
// en Windows 11, asi que tampoco es una salida.
//
// El JScript va en un temporal que escribe node, y todas sus cadenas llegan
// como argumentos: JScript no tiene comillas simples, y escribir una cadena
// suelta dentro del programa obligaria a doblar cada comilla. Aqui eso ya no
// es un problema, pero el temporal sigue siendo lo mas simple.
//
// Los nietos se matan de mas a menos profundo y en UNA sola llamada a taskkill
// (que acepta varios /PID). Con una llamada por proceso eran 400 ms cada uno.
function limpia (pid, esperaMs) {
    pid = String (pid === undefined || pid === null ? '' : pid).trim();
    if (!/^[1-9][0-9]*$/.test (pid)) return 'sin-pid';
    if (vivo (pid)) return 'vivo';

    const js = 'var fso=new ActiveXObject(WScript.Arguments(0));'
             + 'var out=[];'
             + 'try{'
             + 'var svc=GetObject(WScript.Arguments(1));'
             + 'var cols=svc.ExecQuery(WScript.Arguments(2));'
             + 'for(var e=new Enumerator(cols);e.atEnd()===false;e.moveNext()){'
             + 'var p=e.item();'
             + 'out.push(p.ProcessId+String.fromCharCode(44)+p.ParentProcessId)'
             + '}'
             + '}'
             + 'catch(err){out.push(WScript.Arguments(3))}'
             + 'var f=fso.CreateTextFile(WScript.Arguments(4),true);'
             + 'f.Write(out.join(String.fromCharCode(10)));'
             + 'f.Close();';

    const base = path.join (os.tmpdir(), 'verify_arbol_' + process.pid);
    fs.writeFileSync (base + '.js', js);

    run ('cscript.exe', ['/nologo', base + '.js',
                         'Scripting.FileSystemObject',
                         'winmgmts:',
                         'SELECT ProcessId,ParentProcessId FROM Win32_Process',
                         'ERROR',
                         base + '.txt']);

    let lista = '';
    try { lista = fs.readFileSync (base + '.txt', 'utf8'); } catch (e) { lista = ''; }
    try { fs.unlinkSync (base + '.js'); } catch (e) {}
    try { fs.unlinkSync (base + '.txt'); } catch (e) {}

    if (lista.trim() === '' || lista.indexOf('ERROR') === 0) return 'sin-datos';

    const hijos = {};
    const padre = {};

    lista.split (/\r?\n/).forEach (linea => {
        const t = linea.trim();
        if (t === '') return;
        const dos = t.split (',');
        if (dos.length !== 2) return;
        const hijo = Number (dos[0]);
        const pa = Number (dos[1]);
        if (hijo > 0 && pa > 0) { padre[hijo] = pa; (hijos[pa] = hijos[pa] || []).push (hijo); }
    });

    // Losprocesos que NUNCA se tocan: los nuestros y los de nuestros antepasados.
    // Si el PID rancio se ha reciclado y resulta ser un ancestro nuestro, sus
    // hijos no son de la sesion muerta: son del verify que esta arrancando.
    const mios = {};
    let a = process.pid;
    let guardia = 0;
    while (a && guardia++ < 64) { mios[a] = true; a = padre[a]; }
    mios[process.ppid] = true;

    // El arbol, por niveles. Ocho es de sobra: ctest cuelga de cmd, los tests
    // cuelgan de ctest, y a partir de ahi no hay nada del verify.
    let nivel = [Number (pid)];
    const niveles = [];
    const visto = {};

    for (let i = 0; i < 8 && nivel.length > 0; i++) {
        const siguiente = [];
        nivel.forEach (p => {
            (hijos[p] || []).forEach (h => {
                if (mios[h] === undefined && visto[h] === undefined) { visto[h] = true; siguiente.push (h); }
            });
        });
        niveles.push (siguiente);
        nivel = siguiente;
    }

    const plano = [];
    niveles.forEach ((n, i) => n.forEach (p => plano.push ({ p, n: i })));
    plano.sort ((x, y) => y.n - x.n);

    if (plano.length === 0) return 'ok|0|0|0';

    const argumentos = ['/F'];
    plano.forEach (o => argumentos.push ('/PID', String (o.p)));
    argumentos.push ('/T');
    run ('taskkill', argumentos, 30000);

    // Y la espera: se pregunta cada medio segundo, con el filtro por PID, hasta
    // que no quede ninguno o se agote el presupuesto. Sin ella, el script
    // seguiria con procesos que todavia escriben en el build.
    const dormir = ms => Atomics.wait (new Int32Array (new SharedArrayBuffer (4)), 0, 0, ms);
    let quedan = plano.slice();
    const t0 = Date.now();

    while (true) {
        quedan = plano.filter (o => vivo (String (o.p)));
        if (quedan.length === 0) break;
        if (Date.now() - t0 >= esperaMs) break;
        dormir (500);
    }

    return 'ok|' + plano.length + '|' + (plano.length - quedan.length) + '|' + quedan.length;
}

// ── LA LISTA DE FALLOS CONOCIDOS ───────────────────────────────────────────
//
// El JSON es UN solo sitio para los dos scripts (verify_all_known.json). Antes
// cada uno tenia la suya escrita dentro, y ya se midio lo que cuesta: sobre el
// mismo build el .bat daba 8 fallos y el .sh 7. Dos listas son dos verdades.
//
// ── POR QUE EL JSON SOLO TIENE NOMBRES, Y LOS MOTIVOS EN FICHEROS ──────────
//
// Antes cada entrada era "nombre": "motivo de doscientas caracteres en una
// linea". Con cinco Known tests cabia, y se leia. El problema es que el JSON es
// el fichero que se abre para saber QUE HAY, y a los treinta tests eso es un
// muro de texto del que no se ve la lista. Y un motivo largo en una linea es
// incomodo de mantener: no se puede partir en lineas, no se puede comentar, y
// un cambio de cinco palabras en un `git diff` enseña la linea entera.
//
// El motivo de cada rojo esta en su propio fichero, junto al JSON, en
// `known/<nombre>.txt`, y el JSON es solo el indice. Tres cosas salen de ahi:
//
//   - El JSON se lee de un vistazo: son cinco lineas con cinco nombres.
//   - Cada motivo se parte en lineas y se comenta con `#`, que es como se
//     escribe un texto de verdad.
//   - Un test que no este en el indice no tiene fichero, y un fichero sin
//     indice no se lee. Las dos mitades no se pueden desincronizar en silencio
//     porque las dos se comprueban al cargar.
//
// Y al aplanar para el informe no se pierde nada: el motivo que sale por
// pantalla es el mismo texto, con los espacios donde despues los habia.
//
// ── QUE SE COMPRUEBA AL CARGAR, Y POR QUE NO ES DEFENSIVA TEORICA ─────────
//
// Todo lo de aqui puede pasar con un dedo, y dos de los casos hacen que la
// lista PAREZCA funcionar mientras no clasifica nada de lo que dice clasificar:
//   - Un nombre que no se puede usar como nombre de fichero (con `/`, con `:`,
//     con `*`). En Windows, "a/b.txt" lee un fichero de otra carpeta.
//   - Un nombre en el indice sin su fichero de motivo. `motivo` devuelve "",
//     el rojo sale SIN CLASIFICAR, y no hay ningun sintoma visible.
//   - Un fichero de motivo vacio, o con solo comentarios. El mismo fallo.
//   - Un motivo que NO empieza por `MIO:` ni por `ajeno:`. Este NO es un error:
//     hay motivos legitimos sin prefijo y salen SIN CLASIFICAR a proposito. Pero
//     se avisa, porque casi siempre es un prefijo olvidado y no una decision.
//
// El aviso de este ultimo lo saca `bateria`, que es lo unico cuya salida no se
// manda a `nul` en los dos scripts. Asi el "no se ha podido leer" del banner va
// acompanado de WHICH, sin tocar ni el .sh ni el .bat.
const NOMBRE_OK = /^[A-Za-z0-9._-]+$/;

function cargar (fichero) {
    let j;
    try { j = JSON.parse (fs.readFileSync (fichero, 'utf8')); }
    catch (e) { return { error: 'no se ha podido leer como JSON' }; }
    if (j === null || typeof j !== 'object' || Array.isArray (j)) {
        return { error: 'la raiz no es un objeto' };
    }

    if (typeof j['_motivos'] !== 'string' || j['_motivos'] === '') {
        return { error: 'falta la clave "_motivos" (la carpeta de los motivos, relativa a este JSON)' };
    }
    if (!Array.isArray (j['entradas'])) {
        return { error: 'la clave "entradas" no es una lista de nombres' };
    }

    // La carpeta de los motivos es relativa al FICHERO, no al directorio de
    // trabajo. Resuelta desde el cwd, un verify lanzado desde otro sitio lee
    // otra lista sin que nada lo delate: es el mismo fallo que buscar el JSON
    // en dos sitios distintos, que ya se payo una vez.
    const dir = path.resolve (path.dirname (fichero), j['_motivos']);

    const entradas = [];
    const avisos = [];
    const vistos = new Set ();

    for (const nombre of j['entradas']) {
        if (typeof nombre !== 'string' || nombre === '') {
            return { error: 'hay un nombre de entrada que no es texto' };
        }
        if (vistos.has (nombre)) {
            return { error: 'el nombre "' + nombre + '" esta repetido' };
        }
        vistos.add (nombre);

        if (nombre.startsWith ('_')) {
            return { error: 'el nombre "' + nombre + '" empieza por guion bajo, y esa inicial es para comentarios' };
        }
        if (!NOMBRE_OK.test (nombre)) {
            return { error: 'el nombre "' + nombre + '" no vale como nombre de fichero (solo letras, digitos, punto, guion y guion bajo)' };
        }

        const ficheroMotivo = path.join (dir, nombre + '.txt');
        let texto;
        try { texto = fs.readFileSync (ficheroMotivo, 'utf8'); }
        catch (e) {
            return { error: 'no esta el fichero de motivo de "' + nombre + '": se esperaba ' + ficheroMotivo };
        }

        // Las lineas que empiezan por `#` son comentarios, para poder escribir
        // el por que de la entrada sin que se lea como parte del motivo. Lo que
        // queda se aplana a una sola linea, porque el informe es de una linea
        // por rojo y un motivo con saltos lo partiria en dos.
        const motivo = texto.split (/\r?\n/)
                          .filter (l => !/^\s*#/.test (l))
                          .join (' ')
                          .replace (/\s+/g, ' ')
                          .trim ();

        if (motivo === '') {
            return { error: 'el fichero de motivo de "' + nombre + '" esta vacio, o solo tiene comentarios' };
        }
        if (clasifica (motivo) === ETIQUETAS.desconocido) {
            avisos.push ('el motivo de "' + nombre + '" no empieza por "MIO:" ni por "ajeno:", asi que ese rojo saldra SIN CLASIFICAR');
        }

        entradas.push ([nombre, motivo]);
    }

    return { entradas, dir, avisos };
}

// Lo que necesitan los cinco que leen la lista. Antes `cargar` devolvia `null`
// y cada uno ponia su mensaje generico; ahora el fallo trae SU razon.
// El motivo del fallo va a STDERR, que es donde va un fallo, y no a stdout,
// que es el RESULTADO y lo leen los dos scripts. Antes salia codigo 1 y nada
// mas, en las cuatro ordenes que leen la lista: un fallo sin texto obliga a ir
// a mirar el JSON a proposito para descubrir que lo que faltaba era el fichero
// de motivo de UN test. Los dos scripts lo tapan a proposito (`2>&1` a `nul`)
// para que una torre de lineas de node no se coma el banner, asi que esto solo
// se ve al llamar las ordenes a mano, que es donde hace falta.
function cargarEntries (fichero) {
    const r = cargar (fichero);
    if (r.entradas === undefined) {
        process.stderr.write ('verify_all_node.js: no se ha podido cargar ' +
            String (fichero) + ': ' + r.error + '\n');
        return null;
    }
    return r;
}

// Los ficheros de motivo que NO tienen nombre en el indice. Se leen con
// readdirSync y se filtran: si la carpeta no existe no hay nada que avisar, y
// eso ya lo ha dicho el fallo de carga.
function sinIndice (dir, entradas) {
    let lista;
    try { lista = fs.readdirSync (dir); } catch (e) { return []; }

    const enElIndice = new Set (entradas.map (([k]) => k + '.txt'));
    return lista
        .filter (n => n.endsWith ('.txt'))
        .filter (n => !enElIndice.has (n))
        .sort ();
}

// Pares "clave<TAB>motivo<NUL>". El TAB y el NUL como separadores porque un
// motivo puede llevar saltos: partido por lineas, un motivo con un salto se
// convierte en dos entradas y el informe miente. El motivo llega ya aplanado
// de `cargar`, que es el unico sitio que decide como se aplana.
function cmdConocidos (fichero, destino) {
    const r = cargarEntries (fichero);
    if (r === null) { escribe (destino, '', [fichero]); process.exit (1); }
    escribe (destino,
             r.entradas.map (([k, v]) => k + '\t' + v + '\0').join (''),
             [fichero]);
    process.exit (0);
}

function cmdCuenta (fichero, destino) {
    const r = cargarEntries (fichero);
    if (r === null) { escribe (destino, '', [fichero]); process.exit (1); }
    escribe (destino, String (r.entradas.length), [fichero]);
    process.exit (0);
}

function cmdMotivo (fichero, test, destino) {
    const r = cargarEntries (fichero);
    if (r === null) { escribe (destino, '', [fichero]); process.exit (1); }
    const encontrada = r.entradas.find (([k]) => k === test);
    escribe (destino, encontrada ? encontrada[1] : '', [fichero]);
    process.exit (0);
}


// ── LOS CONOCIDOS QUE YA NO FALLAN ─────────────────────────────────────────
//
// La lista envejece, y una lista vieja que no dice nada es peor que no
// tenerla: al cabo de un mes tiene ocho entradas, la mitad ya no falla, y deja
// de ser informacion para ser ruido.
//
// `LastTestsFailed.log` solo dice quien fallo, no quien paso, asi que "no estar
// en la lista de rojos" es exactamente "haber pasado". Y se lee SIN CR: el
// fichero viene con CRLF y sin quitarlo el nombre del test nunca casa con la
// clave, con lo que TODO conocido sale como arreglado. Medido.
function cmdArreglados (fichero, logRojos, colorA, colorN) {
    const r = cargarEntries (fichero);
    if (r === null) process.exit (1);
    const entradas = r.entradas;

    // Un log que NO SE PUEDE LEER no es un log sin rojos. La diferencia es todo
    // este bloque: "no estar en la lista de rojos" significa "haber pasado" solo
    // si la lista de rojos existe, y si no existe no se ha pasado nada: no se ha
    // ejecutado nada. Sin esta comprobacion, un log ausente o ilegible hacia que
    // los CINCO conocidos dieran ARREGLO, con el texto de quitar cada entrada del
    // indice y borrar su fichero de motivo. Es un consejo DESTRUCTIVO nacido de
    // no saber nada.
    //
    // Los dos scripts miran que el log exista antes de llamar, asi que hoy esto
    // no se alcanza desde ellos. Se comprueba aqui porque `arreglados` es una
    // orden que se puede llamar a mano, y su salida dice BORRAR.
    //
    // Lo que SI es valido es el log VACIO: ctest trunca el fichero cuando todo
    // pasa, asi que vacio quiere decir de verdad cero rojos, que es justo el
    // caso que este bloque tiene que avisar.
    let rojos;
    try { rojos = fs.readFileSync (logRojos, 'utf8').replace (/\r/g, ''); }
    catch (e) {
        process.stderr.write (
            'verify_all_node.js: no se ha podido leer el log de rojos ' + String (logRojos) + '\n' +
            '          Sin el no se puede decir que nadie ha pasado, asi que NO se avisa de ARREGLO:\n' +
            '          un log que no se lee no es un log sin rojos.\n');
        process.exit (1);
    }

    const setRojos = new Set (rojos.split ('\n')
                                  .map (l => l.split (':').pop())
                                  .filter (Boolean));

    const ok = entradas.filter (([k]) => setRojos.has (k) === false);

    for (const [clave] of ok) {
        process.stdout.write ('  ' + colorA + 'ARREGLO' + colorN + '  ' + clave + '\n');
        // Los dos pasos, y en este orden. Borrar solo el nombre del indice
        // deja el fichero de motivo huerfano en la carpeta, y un fichero sin
        // indice no se lee: no da error, no molesta, y se queda ahi para siempre
        // sin que nada diga que deberia estar.
        process.stdout.write ('            quita "' + clave + '" del indice y borra known/' + clave + '.txt\n');
    }
    if (ok.length > 0) {
        process.stdout.write ('  ---- ' + ok.length + ' de los ' + entradas.length + ' conocidos ya no fallan\n');
    }
    process.exit (0);
}

// ── EL PID DE LA SESION ────────────────────────────────────────────────────
//
// El lock guarda el PID del PADRE (ppid), que es el cmd.exe que ha lanzado el
// script: si la sesion se cierra a medias, ese padre se lleva por delante y el
// lock queda sin dueno, que es justo lo que hay que detectar. Es el padre y no
// el propio porque el propio es el node de esta llamada, que dura un segundo.
//
// Y el PID lo pide node, no el shell: este bash es el de Git, en Windows, y ahi
// los PID de MSYS y los de Windows no son el mismo numero. Medido: con un
// proceso de verdad vivo, `kill -0` decia que no existia.
function cmdPidPropio (destino) {
    escribe (destino, String (process.ppid), []);
    process.exit (0);
}

function cmdLeePid (fichero, destino) {
    let pid = '';
    try { pid = String (fs.readFileSync (fichero, 'utf8')).trim(); } catch (e) { pid = ''; }
    escribe (destino, pid, [fichero]);
    process.exit (0);
}

// ── LA LISTA DE CONOCIDOS CONTRA LA BATERIA ────────────────────────────────
//
// Una entrada de la lista cuyo test ya no esta en la bateria no hace NADA: no
// se ejecuta, no sale en el log de rojos, y por tanto `motivo` nunca la
// devuelve. Es una linea que parece proteger un rojo y no protege ninguno, y
// lo peor es que la lista se ve cada vez mas larga y mas creible mientras
// protege menos. Por eso esto es un AVISO y no un error: la lista esta
// vieja, no rota, y el verify tiene que seguir pudiendo correr sin ella.
//
// Que es distinto del aviso de ARREGLO, que ya existe. `arreglados` avisa de
// los conocidos que YA NO FALLAN: el test sigue en la bateria y ahora pasa.
// Aqui el test no esta: se borro, se renombro, o el nombre esta mal escrito.
// Los dos se arreglan quitando o moviendo la entrada, pero el segundo ademas
// no se arregla solo nunca, porque un test que no se ejecuta no vuelve a
// fallar y la entrada se queda para siempre.
//
// LA BATERIA SE LEE DE DOS SITIOS, Y POR QUE.
//
//   1. `ctest -N`: es la autoridad, es lo que ctest dice que va a correr. Se
//      consulta primero.
//   2. `CTestTestfile.cmake`: el fichero que CMake genero al configurar. Es lo
//      mismo sin depender de ctest, y se lee cuando ctest no esta.
//
// El segundo hace falta porque el aviso tiene que salir AUNQUE ctest no este.
// En una maquina sin CMake en el PATH (medido en este equipo) el paso 2 no se
// ejecuta, y sin este respaldo el aviso solo apareceria justo cuando ctest
// esta disponible, que es cuando menos falta hace.
//
// Y el respaldo puede dar una lista mas ancha: el `CTestTestfile.cmake` tiene
// un `add_test` por configuracion, y un test que solo se registre para Debug
// aparece ahi y no en `ctest -C Release`. Por eso se deduplica por nombre (el
// nombre es el mismo en las dos configuraciones) y, cuando sale el respaldo,
// el bloque dice de donde se ha leido: un "0 huerfanos" sin decir la fuente es
// un "todo bien" que no se sabe de donde sale.
function nombresBateria (testdir, config) {
    // Con execFileSync y ARRAY, como siempre: con execSync (que pasa por cmd)
    // o desde el bash, MSYS convierte los argumentos que parecen rutas.
    const salida = run ('ctest', ['--test-dir', testdir, '-C', config || 'Release', '-N'], 60000);

    const nombres = new Set ();
    for (const l of salida.split (/\r?\n/)) {
        // "  Test #37: NEURONiK_FxCatalogueTest"
        const m = /^\s*Test\s*#\d+\s*:\s*(.+?)\s*$/.exec (l);
        if (m && m[1]) nombres.add (m[1]);
    }
    if (nombres.size > 0) return { nombres, fuente: 'ctest -N' };

    const fichero = path.join (testdir, 'CTestTestfile.cmake');
    try {
        const texto = fs.readFileSync (fichero, 'utf8');
        for (const m of texto.matchAll (/add_test\s*\(\s*"([^"]+)"/g)) {
            if (m[1]) nombres.add (m[1]);
        }
    } catch (e) { return { nombres, fuente: '' }; }

    if (nombres.size === 0) return { nombres, fuente: '' };
    return { nombres, fuente: 'CTestTestfile.cmake' };
}

function cmdBateria (fichero, testdir, config, colorA, colorN, cuenta) {
    // Aqui se mira el fallo CON SU RAZON, y no en silencio. Es el unico punto
    // de los cinco que imprime su salida sin que los dos scripts la tiren: el
    // aviso generico del banner ("no se ha podido leer") llega sin decir
    // WHICH, que es como un aviso que obliga a ir a mirar el JSON a proposito
    // para descubrir que lo que falta es el fichero de motivo de un test.
    const carga = cargar (fichero);
    if (!carga.entradas) {
        process.stdout.write ('  La lista de conocidos NO se ha podido cargar: ' + carga.error + '\n');
        process.stdout.write ('          Sin ella, los rojos de ctest salen SIN CLASIFICAR.\n');
        escribeCuenta (cuenta, '-2', [fichero]);
        process.exit (0);
    }
    const entradas = carga.entradas;

    // Los motivos sin prefijo. No rompen nada, pero suele ser un prefijo
    // olvidado, y enterarse aqui es mejor que ver un rojo SIN CLASIFICAR dentro
    // de tres meses sin entender por que.
    for (const a of carga.avisos) {
        process.stdout.write ('  ' + String (colorA || '') + 'AVISO' + String (colorN || '') +
                              '  ' + a + '\n');
    }

    const { nombres, fuente } = nombresBateria (testdir, config);

    // La otra mitad del mismo agujero. Partir la lista en dos (indice +
    // ficheros) abre una desincronizacion que antes no existia: un
    // `known/X.txt` que no esta en el indice. No lo lee nadie, no da error, y
    // no se nota: el fichero esta ahi, con su texto escrito, y no hace nada.
    // Pasa cuando se renombra un test y se copia el fichero sin tocar el JSON,
    // que es justo el caso que el aviso de huerfanos de arriba quiere arreglar.
    const sueltos = sinIndice (carga.dir, carga.entradas);
    if (sueltos.length > 0) {
        process.stdout.write ('\n  ' + String (colorA || '') + 'MOTIVO SIN ENTRADA' + String (colorN || '') +
                              '  ' + sueltos.length + ' fichero(s) de motivo sin nombre en el indice:\n');
        for (const s of sueltos) process.stdout.write ('        ' + s + '\n');
        process.stdout.write ('\n          Son ficheros que no se leen: el indice es el que dice que se lee. O el\n');
        process.stdout.write ('          nombre se borro del indice y el fichero se quedo, o el nombre esta\n');
        process.stdout.write ('          mal escrito en cualquiera de los dos sitios. Con el indice bien, el\n');
        process.stdout.write ('          fichero que sobra no molesta; se avisa para que no se acumulen.\n');
    }

    // Sin bateria NO se puede validar nada, y eso no es lo mismo que "0
    // huerfanos". Se dice con esas palabras porque un "todo bien" aqui seria
    // mentira: es un "no se ha mirado" vestido de "no hay nada".
    if (nombres.size === 0) {
        process.stdout.write (
            '  La bateria no se ha podido leer (ni ctest ni ' +
            path.join (testdir, 'CTestTestfile.cmake') + ').\n' +
            '          La lista tiene ' + entradas.length +
            ' entradas y NO se han validado contra nada.\n');
        escribeCuenta (cuenta, '-1', [fichero]);
        process.exit (0);
    }

    const huerfanos = entradas
        .map (([k]) => k)
        .filter (k => !nombres.has (k))
        .sort ();

    // Los colores llegan vacios si la orden se llama a mano, sin los dos
    // argumentos de color. Sin esto, el bloque sale con la palabra "undefined"
    // pegada en medio, que es como se ve un programa que se ha llamado mal.
    const ca = String (colorA == null ? '' : colorA);
    const cn = String (colorN == null ? '' : colorN);

    const sinEntrada = [...nombres].filter (n =>
        !entradas.some (([k]) => k === n)).length;

    const linea = '  lista de conocidos contra la bateria (' + fuente + '): ' +
                  entradas.length + ' entrada(s), ' + nombres.size + ' test(s) en la bateria, ' +
                  huerfanos.length + ' entrada(s) sin test, ' + sinEntrada +
                  ' test(s) sin entrada (lo normal: la lista son solo los rojos conocidos).';

    if (huerfanos.length === 0) {
        process.stdout.write (linea + '\n');
    } else {
        process.stdout.write (linea + '\n');
        process.stdout.write ('\n  ' + ca + 'SIN TEST EN LA BATERIA' + cn +
                              '  ' + huerfanos.length + ' entrada(s) de la lista no tienen ningun test:\n');
        for (const h of huerfanos) process.stdout.write ('        ' + h + '\n');
        process.stdout.write ('\n');
        process.stdout.write ('          Un test que no esta en la bateria no se ejecuta, asi que su entrada\n');
        process.stdout.write ('          no clasifica nada: nunca llega a la lista de rojos. O el nombre esta mal\n');
        process.stdout.write ('          escrito, o el test se renombro (y hay que mover el nombre al nuevo Y su\n');
        process.stdout.write ('          fichero de motivo), o se borro (y se quita el nombre del indice y el\n');
        process.stdout.write ('          fichero de motivo). No se arregla sola: mientras el test no se ejecute, no\n');
        process.stdout.write ('          vuelve a fallar y la entrada se queda para siempre.\n');
    }

    escribeCuenta (cuenta, String (huerfanos.length), [fichero]);
    process.exit (0);
}

// ── EL RESUMEN COMPARABLE ──────────────────────────────────────────────────
//
// Lo que los dos scripts DICEN, en una forma que se pueda comparar. Antes cada
// uno imprimia su informe y no habia ninguna manera de preguntar a la maquina
// "dicen lo mismo?", que es la pregunta que mas veces ha salido mal: sobre el
// mismo build, el .bat daba 8 fallos y el .sh 7, no porque uno mintiera sino
// porque cada uno tenia su lista y su cuenta.
//
// QUE VA EN EL Y QUE NO, y por que:
//
//   - Los rojos, con su clasificacion. Es lo que diverge, y lo que importa.
//   - Los recuentos: tests, fallos, mios, ajenos, sin clasificar.
//   - Los nombres de la lista de conocidos.
//   - Los tests LENTOS, pero SOLO el nombre. Los segundos NO: dos corridas
//     seguidas dan tiempos distintos, y comparar tiempos haria que el check
//     fallara por el motivo equivocado, que es peor que no tener check.
//   - Cuantos tests tocaron el timeout, y no cuales: si los matados cambian de
//     una corrida a otra, eso ya se ve en la lista de rojos de cada una.
//
// Ni rutas absolutas (el .sh dice /d/... y el .bat D:\...), ni fechas, ni
// duraciones. El fichero se ordena, para que un `diff` sea legible en vez de
// un amasijo de lineas movidas.
const ETIQUETAS = {
    mio: 'MIO',
    ajeno: 'AJENO',
    desconocido: 'SIN CLASIFICAR'
};

// La MISMA regla que usan los dos scripts para contar. Vive aqui para que no
// haya tres copias: el `case` del .sh, el de PowerShell del .bat, y esta. Un
// rojo sin prefijo NO es mio por ser mio: es SIN CLASIFICAR, que es como debe
// quedar un rojo del que nadie sabe nada.
function clasifica (motivo) {
    const m = String (motivo || '');
    if (m.startsWith('MIO:') || m.startsWith('mio:') || m.startsWith('MIO :')) return ETIQUETAS.mio;
    if (m.startsWith('ajeno:')) return ETIQUETAS.ajeno;
    return ETIQUETAS.desconocido;
}

// Las comillas que el .bat pone en cada campo del informe. El motivo viene de
// texto ajeno (el log de ctest, los motivos del JSON) y sin entrecomillar un
// "&" o un ">" ejecutaria el resto de la linea, asi que :anotar las pone. El
// PowerShell del .bat las quita al leer, con `.Trim([char]34)`, y aqui igual:
// si no, el paso del .bat llega con las comillas pegadas y no casa con nada.
//
// Se quitan SOLO de los extremos, como hace el PowerShell, y no con un replace
// global: un motivo con una comilla de verdad en medio es un motivo, no dos.
function sinComillas (s) {
    let t = String (s == null ? '' : s);
    while (t.length > 0 && t[0] === q) t = t.slice (1);
    while (t.length > 0 && t[t.length - 1] === q) t = t.slice (0, -1);
    return t.trim();
}

function cmdResumen (entradas, destino, conocidos, tests, lentos, tocoTimeout, pasos, huerfanos) {
    // `lentos` es un FICHERO y no un numero, y la cuenta sale de contarlo aqui.
    // Esta comprobacion es por el nombre: en la cabecera la lista de argumentos
    // pone `<conocidos> <tests> <lentos> <tocoTimeout>` en fila, y lo natural es
    // leerla como cuatro cuentas. Pasarle un `0` ahi no da error: node lo lee
    // como el descriptor 0 y cuenta los lentos de lo que venga por stdin.
    rutaValida (entradas, 'la entrada de rojos', 'resumen');
    rutaValida (lentos, 'el fichero de lentos', 'resumen');

    const lineas = [];

    let rojos = [];
    try {
        if (entradas && fs.existsSync (entradas)) {
            rojos = fs.readFileSync (entradas, 'utf8')
                         .split (/\r?\n/)
                         .map (l => l.trim())
                         .filter (l => l !== '')
                         .map (l => l.split('\t'));
        }
    } catch (e) { rojos = []; }

    const contados = { MIO: 0, AJENO: 0, 'SIN CLASIFICAR': 0 };
    const filas = [];

    for (const campos of rojos) {
        // El motivo es TODO lo que va del tercer campo en adelante, unido con
        // espacios. Lo hace el PowerShell del .bat porque un motivo puede
        // llevar tabuladores dentro, y partido en campos se pierde de la
        // clasificacion. Aqui igual, para que la cuenta sea la misma.
        const paso   = sinComillas (campos[0]);
        const test   = sinComillas (campos[1]);
        const motivo = sinComillas ((campos.slice (2).join(' ')).replace (/\s+/g, ' '));
        const etiqueta = clasifica (motivo);
        contados[etiqueta] += 1;
        filas.push ('rojo' + '\t' + paso + '\t' + test + '\t' + etiqueta);
    }
    filas.sort ();

    let lentos_lista = [];
    try {
        if (lentos && fs.existsSync (lentos)) {
            lentos_lista = fs.readFileSync (lentos, 'utf8')
                                .split (/\r?\n/).map (l => l.trim())
                                .filter (l => l !== '').sort();
        }
    } catch (e) { lentos_lista = []; }

    lineas.push ('v' + '\t' + '1');
    // Los pasos que se han ejecutado van en el resumen. Sin esto, comparar un
    // `--only=2` contra un verify de los cinco pasos da "coinciden" cuando lo
    // que ha pasado es que uno de los dos no ha mirado tres pasos.
    lineas.push ('pasos' + '\t' + String(pasos == null ? '' : pasos).trim());
    lineas.push ('conocidos' + '\t' + String(conocidos || 0));
    // El numero de tests puede ser "?" si ctest no esta: se escribe tal cual, y
    // que los dos scripts lo ignoren lo mismo es parte de lo que se comprueba.
    lineas.push ('tests' + '\t' + String(tests || 0));
    lineas.push ('fallos' + '\t' + String(rojos.length));
    lineas.push ('mios' + '\t' + String(contados.MIO));
    lineas.push ('ajenos' + '\t' + String(contados.AJENO));
    lineas.push ('sinClasificar' + '\t' + String(contados['SIN CLASIFICAR']));
    lineas.push ('lentos' + '\t' + String(lentos_lista.length));
    lineas.push ('tocoTimeout' + '\t' + String(tocoTimeout || 0));
    // Los CUANTOS, y no los nombres: los nombres se avisan en pantalla, y aqui
    // lo que se comprueba es que los dos scripts hayan contado lo mismo. El -1
    // es "no se ha podido validar", que es distinto de 0 y por eso no se
    // confunde con un "la lista esta limpia".
    lineas.push ('huerfanos' + '\t' + String(huerfanos == null || huerfanos === '' ? '-' : huerfanos));
    for (const f of filas) lineas.push (f);
    for (const l of lentos_lista) lineas.push ('lento' + '\t' + l);

    escribe (destino, lineas.join('\n') + '\n', [entradas, lentos]);
    process.exit (0);
}

// ── COMPARAR LOS DOS RESUMENES ────────────────────────────────────────────
//
// La pregunta que mas veces ha salido mal sobre este repo: "dicen lo mismo el
// .sh y el .bat?". No se podia contestar, porque lo que decian estaba en dos
// informes de pantalla con rutas, fechas y segundos. Aqui se contesta con un
// codigo de salida.
//
// El mensaje sale en dos partes, y separarlas es lo que lo hace util:
//
//   1. Los RECUENTOS, uno a uno, con los dos numeros al lado. "fallos 5 (sh) /
//      6 (bat)" se lee de un vistazo; un diff de lineas ordena el rojo de uno
//      al principio y esconde el numero que lo resume.
//
//   2. Las LINEAS que sobran en un lado, que es donde esta el nombre del rojo
//      divergente. Con la cuenta al lado: "solo en el .sh (1)" ya dice bastante
//      y el nombre lo dice todo.
function leerResumen (fichero) {
    let texto = '';
    try { texto = fs.readFileSync (fichero, 'utf8'); } catch (e) { return null; }
    const lineas = texto.split (/\r?\n/).map (l => l.trim()).filter (l => l !== '');
    const cabecera = new Map ();
    const detalle = [];
    for (const l of lineas) {
        if (l.startsWith ('rojo' + '\t') || l.startsWith ('lento' + '\t')) { detalle.push (l); continue; }
        const p = l.split ('\t');
        if (p.length >= 2) cabecera.set (p[0], p.slice (1).join('\t'));
        else cabecera.set (p[0], '');
    }
    return { cabecera, detalle };
}

function cmdCompara (a, b, nombreA, nombreB) {
    const etA = String (nombreA || 'A');
    const etB = String (nombreB || 'B');

    const A = leerResumen (a);
    const B = leerResumen (b);

    if (!A) {
        process.stderr.write ('verify_all_node.js: no se puede leer el resumen ' + String (a) + '\n');
        process.exit (2);
    }
    if (!B) {
        process.stderr.write ('verify_all_node.js: no se puede leer el resumen ' + String (b) + '\n');
        process.exit (2);
    }

    // La version va la primera: si los dos resúmenes son degeneraciones
    // distintas, comparar sus campos no significa nada.
    const va = A.cabecera.get('v');
    const vb = B.cabecera.get('v');
    if (va !== vb) {
        process.stdout.write ('  EL RESUMEN NO ES EL MISMO EN LOS DOS (' + etA + ' v' + va + ', ' + etB + ' v' + vb + ').\n');
        process.exit (1);
    }

    const recuentos = [];
    const claves = new Set ([...A.cabecera.keys(), ...B.cabecera.keys()]);
    for (const k of claves) {
        if (k === 'v') continue;
        const va2 = A.cabecera.has (k) ? A.cabecera.get (k) : '(ausente)';
        const vb2 = B.cabecera.has (k) ? B.cabecera.get (k) : '(ausente)';
        if (va2 !== vb2) recuentos.push ({ clave: k, a: va2, b: vb2 });
    }

    // Las de detalle, como multiconjuntos: dos rojas del mismo test y paso se
    // cuentan dos veces, y un Set las fundiria en una.
    const cuenta = new Map ();
    for (const l of B.detalle) cuenta.set (l, (cuenta.get (l) || 0) + 1);
    const soloA = [];
    for (const l of A.detalle) {
        const n = cuenta.get (l) || 0;
        if (n > 0) cuenta.set (l, n - 1); else soloA.push (l);
    }
    const soloB = [];
    for (const [l, n] of cuenta) for (let i = 0; i < n; i++) soloB.push (l);
    soloB.sort ();

    if (recuentos.length === 0 && soloA.length === 0 && soloB.length === 0) {
        process.stdout.write ('  Dicen lo mismo: ' + A.detalle.length + ' line(s) de detalle, ' +
                              (A.cabecera.size - 1) + ' recuento(s), sin una sola diferencia.\n');
        process.exit (0);
    }

    process.stdout.write ('\n  ' + etA + ' y ' + etB + ' NO DICEN LO MISMO sobre el mismo build.\n');
    if (recuentos.length > 0) {
        process.stdout.write ('\n  estos recuentos no coinciden:\n');
        for (const r of recuentos) {
            process.stdout.write ('        ' + r.clave + '        ' + r.a + '  (' + etA + ')   ' + r.b + '  (' + etB + ')\n');
        }
    }
    if (soloA.length > 0) {
        process.stdout.write ('\n  solo en ' + etA + ' (' + soloA.length + '):\n');
        for (const l of soloA.sort ()) process.stdout.write ('        ' + l.replace (/\t/g, '  ') + '\n');
    }
    if (soloB.length > 0) {
        process.stdout.write ('\n  solo en ' + etB + ' (' + soloB.length + '):\n');
        for (const l of soloB) process.stdout.write ('        ' + l.replace (/\t/g, '  ') + '\n');
    }
    process.stdout.write ('\n  Los dos scripts tienen que decir lo mismo del MISMO build. Si uno tiene\n');
    process.stdout.write ('  mas rojos, no es que el otro no los vea: es que cada uno lleva su cuenta.\n');
    process.exit (1);
}

// ── LOS BINARIOS RANCIOS ───────────────────────────────────────────────────
//
// ESTO CUESTO CUATRO ROJOS FALSOS Y VARIOS DIAS. Medido el 2026-10-01: de los
// 52 tests que ctest registra, solo 8 tenian el .exe de HOY. Los otros 30 eran
// del 29/09, porque el paso 1 del verify compila SEIS targets a proposito (los
// que exportan y los de la matriz de modulacion) y ningun otro. Los 30 que
// quedan no se recompilaban nunca, y sus resultados se contaban como si fueran
// de esta corrida: tres tests que llevan dias "en rojo" con el motivo "el APVTS
// ya declara mas parametros" PASABAN al recompilarse, sin tocar una linea.
//
// O sea que la causa comun de un rojo no se mira primero en el codigo, sino en
// la fecha del binario. Y eso no lo puede decir nadie a ojo en un informe que
// no lleva fechas.
//
// LA REGLA. Un binario mas viejo que el codigo que se acaba de compilar. No se
// compara con "hoy" porque un test que no se ha tocado lleva dias en su sitio
// sin que eso sea un problema: lo que delata el problema es que el .exe sea mas
// ANTIGUO que el fuente del target. Si alguien edita PresetMigrationParity.cpp y
// el .exe es de ayer, el test que se ejecuta no es el codigo que se ha escrito.
//
// Y hay un segundo criterio, mas barato y mas fuerte: lo que el paso 1 ha
// compilado en ESTA corrida. Un binario que el paso 1 no ha tocado, aqui, es
// rancio por definicion; ese solo necesita un aviso.
//
// Los .exe se buscan por el nombre del test, que es como los llama CMake.
//
// `configDe` saca el nombre de la configuracion de una ruta de CMake, que la
// escribe como .../Release/Nombre.exe. No es una regla nueva: es que la misma
// palabra se lee en un sitio y se decide en otro, que es como estos dos
// programas han acabado dando numeros distintos sobre el mismo build.
function configDe (ruta) {
    const m = /[\\/](Debug|Release|RelWithDebInfo|MinSizeRel)[\\/]/i.exec (String (ruta || ''));
    return m ? m[1] : '';
}
// LOS TESTS DE ESTE PROYECTO, Y EL TARGET QUE LOS COMPILA.
//
// Esto no es una añadidura de la orden `rancios`: es la regla de "que es un test
// nativo" sacada de ahi, porque la necesitan DOS ordenes y porque la respuesta
// equivocada no es un numero feo, es un numero que miente:
//
//   targets   la lista que el paso 1 tiene que compilar para que el paso 2
//             mida el codigo de ahora. Si aqui se cuela un test que no es de
//             este proyecto, el paso 1 compila un target que no existe y el
//             build entero se cae
//   rancios    cuantos .exe son de una pasada anterior. Si aqui se cuela uno de
//             node (los de Playwright y los de contrato se registran con el
//             node.exe del SISTEMA), el aviso diria que hay tests rancios que no
//             existen
//
// Estar en dos sitios era exactamente el fallo del otro dia: el .bat contando
// una cosa y el .sh otra, cada uno con su lista.
//
// LO QUE CUENTA COMO NATIVO: el .exe esta DENTRO del arbol de build. Los de node
// apuntan a .../nodejs/node.exe, fuera de aqui, y no los compila nadie en este
// repositorio. Y da igual que el .exe exista o no: la lista es la de los tests
// que hay que COMPILAR, y un test cuyo .exe no existe es justamente uno que
// todavia no se ha compilado.
//
// El target es el nombre del .exe sin extension, que es como los llama CMake
// (`add_executable (NEURONiK_FooTest ...)`) y como los pide `--target`.
function testsNativos (testdir, config) {
    const dir = String (testdir || '');

    let texto = '';
    try { texto = fs.readFileSync (path.join (dir, 'CTestTestfile.cmake'), 'utf8'); }
    catch (e) {
        process.stderr.write ('verify_all_node.js: no se ha podido leer el CTestTestfile.cmake de ' + dir + '\n');
        process.exit (1);
    }

    // CMake escribe el MISMO test cuatro veces, una por configuracion. Aguantarse
    // de la ultima es meterle una ruta que no existe y luego decir "sin binario"
    // de 39 tests que si lo tienen. Se elige la del config pedido, y si no esta,
    // la primera que aparezca.
    const vistos = new Map ();
    const cfg = String (config || 'Release');
    for (const m of texto.matchAll (/add_test\s*\(\s*"([^"]+)"\s*,?\s*"([^"]*\.exe)"/gi)) {
        const nombre = m[1];
        const exe = m[2].replace (/\//g, path.sep);
        const ya = vistos.get (nombre);
        if (ya === undefined) { vistos.set (nombre, { exe, config: configDe (exe) }); continue; }
        if (ya.config !== cfg && configDe (exe) === cfg)
            vistos.set (nombre, { exe, config: configDe (exe) });
    }

    // SOLO los que son un target de ESTE proyecto: el .exe dentro del arbol.
    const raizBuild = path.resolve (dir);
    const nativos = new Map ();
    for (const [nombre, entrada] of vistos) {
        const ruta = path.isAbsolute (entrada.exe) ? entrada.exe : path.join (dir, entrada.exe);
        if (path.resolve (path.dirname (ruta)).indexOf (raizBuild) !== 0) continue;
        nativos.set (nombre, {
            nombre,
            ruta,
            config: entrada.config,
            target: path.basename (entrada.exe, path.extname (entrada.exe))
        });
    }
    return { nativos, total: vistos.size };
}

// LA LISTA DE TARGETS DEL PASO 1. Una linea por target, ordenada, y SALIDA CON
// CODIGO si no se puede saber: una lista vacia aqui significaria "compila
// nada", que es un build que parece bueno y no ha medido nada.
function cmdTargets (testdir, config) {
    const { nativos } = testsNativos (testdir, config);
    if (nativos.size === 0) {
        process.stderr.write ('verify_all_node.js: ' + String (testdir) +
                              ' no tiene ningun test nativo en el CTestTestfile.cmake\n');
        process.stderr.write ('  Si el proyecto se acaba de cambiar, hay que volver a configurar antes de compilarlo.\n');
        process.exit (1);
    }
    const lista = Array.from (nativos.values ()).map (t => t.target).sort ();
    process.stdout.write (lista.join ('\n') + '\n');
    process.exit (0);
}

function cmdRuncios (testdir, config, colorA, colorN, construidos, sinBuild) {
    const ca = String (colorA || '');
    const cn = String (colorN || '');

    const leidos = testsNativos (testdir, config);
    const vistos = leidos.nativos;
    const totalBateria = leidos.total;

    // LO QUE EL PASO 1 HA CONSTRUIDO. Esta es la lista que decide, y no la fecha
    // de un fichero. La fecha miente por dos motivos que ya han pasado aqui: un
    // `git checkout` o un `cp` pone al dia el mtime de un fuente que no ha
    // cambiado de verdad, y comparar cada .exe contra el fuente MAS NUEVO del
    // arbol entero marca los cincuenta y tres rancios en cuanto se toca un
    // fichero que no tiene nada que ver. Lo que no miente es "¿este paso ha
    // rebuilding esto?": si no lo ha hecho, el .exe que va a correr es el que
    // hubiera, con la antiguedad que tenga.
    const hechos = new Set ((construidos || []).map (s => String (s)));
    // `--sin-build` es el caso limite: el paso 1 no se ha ejecutado, asi que no
    // hay lista que enviar y el silencio pareceria que todo esta al dia. Es justo
    // el caso donde mas hace falta el aviso, porque con --no-build NADA de esto
    // se ha compilado. Sin lista y sin el flag (llamado a mano) no se juzga
    // nada, que es otra cosa.
    const sinbuild = sinBuild === true;

    let sinConstruir = 0;
    let sinBinario = 0;
    let nativos = 0;        // tests de este arbol, con .exe o sin el
    let reconstruidos = 0;  // los que de verdad son un test de la lista de "hechos"
    const fuera = [];
    const ausentes = [];

    for (const [nombre, entrada] of vistos) {
        const ruta = entrada.ruta;
        ++nativos;

        let st;
        try { st = fs.statSync (ruta); }
        catch (e) { ++sinBinario; ausentes.push (nombre + '  (registrado en ' + entrada.config + ')'); continue; }

        // El target de CMake se llama como el .exe, sin extension (`testsNativos`).
        const target = entrada.target;

        if (sinbuild || (hechos.size > 0 && !hechos.has (target))) {
            ++sinConstruir;
            fuera.push (nombre);
            continue;
        }
        if (hechos.has (target)) ++reconstruidos;

        // Y si SI se ha construido en esta pasada, el .exe no puede ser mas viejo
        // que lo que se acaba de compilar. Si lo es, algo lo ha tocado por
        // detrás y eso si hay que decirlo.
        if (hechos.has (target)) {
            const fuente = fuenteDelTarget (ruta, nombre);
            if (fuente > st.mtimeMs) {
                ++sinConstruir;
                fuera.push (nombre + '  (construido, pero el .exe es anterior al fuente)');
            }
        }
    }

    fuera.sort ();
    ausentes.sort ();

    // La cuenta que se imprime es la que cuadra: tests reconstruidos de verdad
    // (NO los targets del paso 1, que incluyen dos que no son tests y se
    // comian dos de la diferencia) + los que se quedan fuera = los nativos.
    process.stdout.write ('  binarios de tests: ' + totalBateria +
                          ' en la bateria, ' + nativos + ' nativos, ' +
                          (sinbuild ? '0 reconstruidos' : reconstruidos + ' reconstruidos') +
                          ' por el paso 1' +
                          (fuera.length > 0 ? ', ' + fuera.length + ' con el binario de una pasada anterior' : '') + '\n');

    if (fuera.length > 0) {
        process.stdout.write ('\n  ' + ca + (sinbuild ? 'NO SE HA CONSTRUIDO NADA' : 'BINARIO RANCIO') + cn + '  ' + fuera.length +
                              (sinbuild ? ' test(s) se ejecutan sin haber sido compilados nunca en esta pasada:\n'
                                        : ' test(s) se ejecutan con el .exe de una pasada anterior:\n'));
        for (const n of fuera.slice (0, 10)) process.stdout.write ('        ' + n + '\n');
        if (fuera.length > 10)
            process.stdout.write ('        ... y ' + (fuera.length - 10) + ' mas (el script los lista todos)\n');
        process.stdout.write ('\n');
        process.stdout.write ('          Un rojo de aqui NO es del codigo de ahora, y un verde tampoco: los\n');
        process.stdout.write ('          dos vienen del binario de la fecha que tenga el .exe. Recompila el\n');
        process.stdout.write ('          target (`cmake --build ' + String (testdir || '') + ' --config ' +
                              String (config || 'Release') + ' --target NOMBRE`) y vuelve a mirar\n');
        process.stdout.write ('          antes de tocar nada.\n');
        process.stdout.write ('\n');
        if (sinbuild) {
            process.stdout.write ('          Ha ido con --no-build: el paso 1 no se ha ejecutado, asi que esto\n');
            process.stdout.write ('          no es que los binarios sean viejos, es que no se ha compilado\n');
            process.stdout.write ('          nada. Un rojo de aqui no es del codigo de ahora, y un verde\n');
            process.stdout.write ('          tampoco. Quita el --no-build, o compila a mano lo que quieras\n');
            process.stdout.write ('          mirar, y vuelve a pasar la bateria.\n');
        } else {
            process.stdout.write ('          El paso 1 compila los ' + nativos +
                                  ' tests nativos de este arbol, y no "todo": "todo" arrastra\n');
            process.stdout.write ('          el plugin y el WASM, que no son de esta verificacion y tardan mas\n');
            process.stdout.write ('          que todo lo demas. Si un target no compila, el paso 1 lo dice, y el\n');
            process.stdout.write ('          rojo de mas abajo puede ser suyo: mira el paso 1 antes de tocar\n');
            process.stdout.write ('          codigo.\n');
        }
    }
    if (sinBinario > 0) {
        process.stdout.write ('\n  ' + ca + 'SIN BINARIO' + cn + '  ' + sinBinario +
                              ' test(s) registrados sin .exe:\n');
        for (const n of ausentes.slice (0, 8)) process.stdout.write ('        ' + n + '\n');
        if (ausentes.length > 8)
            process.stdout.write ('        ... y ' + (ausentes.length - 8) + ' mas\n');
        process.stdout.write ('\n          Si alguno deberia estar aqui, se esta contando como verde un test que\n');
        process.stdout.write ('          no se ha ejecutado nunca.\n');
    }

    process.exit (0);
}

// El fuente mas reciente que el target `nombre` pudo usar: los mismos sitios que
// mira el CMakeLists para ese target. Es una aproximacion a proposito (el
// CMakeLists real lista ficheros, no carpetas), y por eso solo se usa para el
// caso de "construido en esta pasada y aun asi mas viejo", que unicamente puede
// salir mal si alguien toca el arbol por detras.
function fuenteDelTarget (rutaExe, nombre) {
    const build = path.dirname (rutaExe);
    const raiz = path.resolve (path.dirname (build));
    let masNueva = 0;
    for (const sub of ['Source', 'Tests', '']) {
        const d = path.join (raiz, sub);
        let nombres;
        try { nombres = fs.readdirSync (d); } catch (e) { continue; }
        for (const f of nombres) {
            if (!/\.(cpp|h|hpp)$/i.test (f)) continue;
            // Del target que nos interesa y de sus vecinos del mismo directorio,
            // que es lo que comparten la mayoria de estos tests.
            if (sub === '' && ! f.toLowerCase().includes (nombre.toLowerCase().replace(/^neuronik_/, ''))) continue;
            try {
                const t = fs.statSync (path.join (d, f)).mtimeMs;
                if (t > masNueva) masNueva = t;
            } catch (e) {}
        }
    }
    return masNueva;
}

// ── LA ORDEN ───────────────────────────────────────────────────────────────

// `argv` es el resto, SIN el node y SIN el nombre del fichero, y cada orden
// lee sus argumentos por posicion desde ahi. Los indices se cuentan aqui una
// vez y no en cada `process.argv[N]`: el prototipo que se escribio con indices
// sueltos fue justo lo que borro el JSON de los conocidos.
const argv = process.argv.slice (2);
const orden = argv[0];

switch (orden) {
    case 'vivo':
        escribe (argv[2], String (vivo (argv[1])), []);
        process.exit (0);

    case 'limpia':
        escribe (argv[2], limpia (argv[1], Number (argv[3] || 10000)), []);
        process.exit (0);

    case 'pidpropio': cmdPidPropio  (argv[1]); break;
    case 'leepid':    cmdLeePid     (argv[1], argv[2]); break;

    case 'resumen':   cmdResumen    (argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]); break;
    case 'bateria':   cmdBateria    (argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]); break;
    case 'compara':   cmdCompara    (argv[1], argv[2], argv[3], argv[4]); break;

    case 'conocidos': cmdConocidos  (argv[1], argv[2]); break;
    case 'cuenta':    cmdCuenta     (argv[1], argv[2]); break;
    case 'motivo':    cmdMotivo     (argv[1], argv[2], argv[3]); break;
    case 'arreglados':cmdArreglados (argv[1], argv[2], argv[3] || '', argv[4] || ''); break;
    case 'targets':   cmdTargets     (argv[1], argv[2]); break;

    case 'rancios': {
        // El flag va en medio de la lista, no delante: los dos scripts lo
        // escriben siempre, y separarlo seria una posicion mas que recordar.
        const resto = argv.slice (5);
        const sinbuild = resto.indexOf ('--sin-build') !== -1;
        cmdRuncios (argv[1], argv[2], argv[3] || '', argv[4] || '',
                    sinbuild ? resto.filter (s => s !== '--sin-build') : resto,
                    sinbuild);
        break;
    }

    default:
        process.stderr.write ('verify_all_node.js: orden desconocida: ' + String (orden) + '\n');
        process.stderr.write ('  vivo | limpia | conocidos | cuenta | motivo | arreglados | resumen\n');
        process.stderr.write ('  targets <testdir> <config>  los targets de los tests nativos, uno por linea\n');
        process.stderr.write ('  rancios <testdir> <config> [a] [n] [--sin-build] [construidos...]  tests con el .exe mas viejo que el codigo\n');
        process.stderr.write ('  bateria <json> <testdir> <config>\n');
        process.stderr.write ('  compara | pidpropio | leepid\n');
        process.exit (2);
}
