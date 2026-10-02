#!/usr/bin/env node
//
//  USO:  node Scripts/selftest_verify_all_node.js
//
// ============================================================================
//
//  POR QUE ESTE FICHERO EXISTE
//
//  Toda la logica de la lista de conocidos vive en UNA funcion, `cargar()` de
//  verify_all_node.js, y lo que se ha roto en esa lista siempre ha sido un
//  caso raro: un nombre repetido, un fichero de motivo que no esta, un prefijo
//  en la linea que no es la primera. Esos casos no salen con un uso normal.
//
//  Y no los vigila nadie. El unico check automatico que hay es el de los
//  gemelos (verify_all_check.sh), que compara lo que dicen el .sh y el .bat
//  sobre el MISMO build: pasa por el camino feliz y por el `bateria`, y su
//  trabajo es que los dos CUENTEN igual, no que la lista cargue bien. Un `cargar`
//  que se rompe deja los dos gemelos rompiendose IGUAL, y ahi el check sigue en
//  verde.
//
//  Esto es lo que convierte una tarde de pruebas a mano en una comprobacion que
//  se puede volver a pasar. Los casos de aqui estan todos MEDIDOS, no inventados:
//  o se rompió algo y se probo, o se probo antes de que rompiera nada.
//
//  No usa nada de node mas alla de lo estandar, y no toca el repositorio: todo
//  lo que escribe va a un temporal del sistema.
//
// ============================================================================

'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const cp = require('child_process');

const LIB = path.join(__dirname, 'verify_all_node.js');

let  fallos = 0;
let  hechos  = 0;

// ── EL TEMPORAL ────────────────────────────────────────────────────────────
//
// En el temporal del SISTEMA y no en Scripts/ ni en la raiz del proyecto: una
// prueba que escribe donde esta el codigo se puede comer un fichero bueno el dia
// que alguien cambia un nombre. Ademas el repo ha tenido ficheros sin rastrear
// en la raiz que se confunden con esto (unos cuantos llamados `0`, `4`,
// `999999`, de una version anterior de este mismo programa, que leia un numero
// como nombre de fichero).
const TMP = fs.mkdtempSync(path.join(os.tmpdir(), 'verify_selftest_'));
process.on('exit', () => { try { fs.rmSync(TMP, { recursive: true, force: true }); } catch (e) {} });

// ── LOS TRES MOLDES DE ASSERT ──────────────────────────────────────────────
function ok (nombre, condicion, detalle) {
    hechos += 1;
    if (condicion) { process.stdout.write ('  ok    ' + nombre + '\n'); return; }
    fallos += 1;
    process.stdout.write ('  FALLA ' + nombre + '\n');
    if (detalle) process.stdout.write ('          ' + String(detalle).replace(/\n/g, '\n          ') + '\n');
}
function igual (nombre, obtenido, esperado) {
    ok (nombre, obtenido === esperado,
       obtenido === esperado ? '' : 'esperado: ' + JSON.stringify (esperado) +
                               '\nobtenido: ' + JSON.stringify (obtenido));
}

// ── CORRER EL PROGRAMA QUE SE PRUEBA ──────────────────────────────────────
//
// Por proceso aparte, y no importando el modulo, porque `verify_all_node.js` es
// un programa que hace `process.exit` en la ultima linea: importarlo mataria
// ESTE proceso. Ademas hay que poder mirar el codigo de salida, y eso solo se ve
// desde fuera. Y el temporal va en MSYS o en nativo segun como se invoque, asi
// que se pasa con `path.resolve`.
function corre (args, opciones) {
    const op = opciones || {};
    const r = cp.spawnSync (process.execPath, [LIB].concat (args),
                            { encoding: 'utf8', cwd: op.cwd });
    return {
        rc:     r.status,
        out:    String (r.stdout || ''),
        err:    String (r.stderr || ''),
        outR:   String (r.stdout || '').replace(/\r/g, ''),
        errR:   String (r.stderr || '').replace(/\r/g, '')
    };
}

// ── MONTAR UNA LISTA EN UN DIRECTORIO NUEVO ────────────────────────────────
//
// Cada caso recibe su propio directorio. Compartir uno daria el falso positivo
// de que un fichero de un caso tapa el de otro, que es justo lo que se quiere
// descartar: un motivo tiene que existir SOLO si toca.
function lista (entradas, motivos, opciones) {
    const op = opciones || {};
    const dir = fs.mkdtempSync(path.join(TMP, 'c'));
    const dirMotivos = path.join(dir, op.carpeta === undefined ? 'known' : String (op.carpeta));
    fs.mkdirSync(dirMotivos, { recursive: true });

    for (const [nombre, texto] of Object.entries (motivos || {})) {
        const b = Buffer.isBuffer (texto) ? texto : Buffer.from (String (texto), 'utf8');
        fs.writeFileSync (path.join (dirMotivos, nombre + '.txt'), b);
    }
    // Los motivos sueltos, los que no tienen nombre en el indice.
    for (const nombre of (op.sueltos || [])) {
        fs.writeFileSync (path.join (dirMotivos, nombre + '.txt'), 'ajeno: sin entrada\n');
    }

    const j = {};
    if (!op.sinMotivos) j['_motivos'] = dirMotivos;
    if (entradas !== undefined) j['entradas'] = entradas;
    // `extras` va AL FINAL y puede pisar cualquier clave, que es como se monta
    // el caso de "falta _motivos" sin tener que renombrar el parametro.
    for (const [k, v] of Object.entries (op.extras || {})) j[k] = v;

    const fichero = path.join (dir, 'verify_all_known.json');
    fs.writeFileSync (fichero, JSON.stringify (j, null, 2) + '\n');
    return { fichero, dir, dirMotivos };
}

function seccion (titulo) {
    process.stdout.write ('\n' + titulo + '\n');
}

// ── LA LISTA DE LOS NO MEDIDOS, LEIDA ──────────────────────────────────────
//
// El fichero que deja `rancios` lleva una linea por test: el nombre, un
// tabulador y el por que no se ha medido. Casi todos los casos solo miran los
// nombres, asi que se separa aqui una vez en vez de repetir el corte en cada uno.
//
// Y devuelve tambien el POR QUE, que es la mitad de la linea que de verdad se
// mira en la seccion 14: una comparacion que mirara el nombre con su motivo
// pegado daria verde con el motivo equivocado, que es justo lo que no se puede
// hacer con un aviso que dice si hay que recompilar o no.
function noMedidos (fichero) {
    const crudas = fs.readFileSync (fichero, 'utf8').replace (/\r/g, '').split ('\n').filter (s => s);
    return crudas.map (c => {
        const corte = c.indexOf ('\t');
        return corte > 0 ? { nombre: c.slice (0, corte), porque: c.slice (corte + 1) }
                         : { nombre: c, porque: '' };
    });
}

// ============================================================================
//  1. LA LISTA BIEN ESTA
// ============================================================================
seccion ('1. La lista bien esta');

// El camino feliz: es el que usan los dos gemelos, y no por eso puede fallar.
{
    const { fichero } = lista (['Uno', 'Dos'],
                               { Uno: 'MIO: mio de verdad\n', Dos: 'ajeno: del otro hilo\n' });

    const c = corre (['cuenta', fichero]);
    igual ('cuenta devuelve el numero de entradas', c.out.trim(), '2');
    igual ('cuenta sale con 0', c.rc, 0);

    const m = corre (['motivo', fichero, 'Uno']);
    igual ('motivo devuelve el motivo tal cual', m.out.trim(), 'MIO: mio de verdad');

    const k = corre (['conocidos', fichero]);
    igual ('conocidos devuelve un par por entrada', k.out.split('\0').filter(Boolean).length, 2);

    const noEsta = corre (['motivo', fichero, 'NoEstaEnLaLista']);
    igual ('motivo de un nombre que no esta devuelve vacio', noEsta.out.trim(), '');
    igual ('motivo de un nombre que no esta sale con 0', noEsta.rc, 0);
}

// El separador NUL y el aplanado: un motivo con saltos y tabuladores tiene que
// llegar al informe como UNA linea. Es lo que hace que el recuento de los dos
// scripts no se desmonte en un motivo largo.
{
    const { fichero } = lista (['Largo'],
                               { Largo: 'ajeno: primera linea\nsegunda linea\n\ntercera\n' });
    const m = corre (['motivo', fichero, 'Largo']);
    igual ('un motivo partido en lineas sale en una sola',
           m.out.trim(), 'ajeno: primera linea segunda linea tercera');
}

// Un comentario se quita y no sale en el informe; y las lineas con # en medio
// tampoco. Medido: es lo que permite escribir el por que de la entrada sin que
// ensucie la linea del rojo.
{
    const { fichero } = lista (['ConComentario'],
                               { ConComentario: '# una nota\nMIO: el motivo\n# otra nota\n' });
    const m = corre (['motivo', fichero, 'ConComentario']);
    igual ('las lineas con # no salen en el motivo', m.out.trim(), 'MIO: el motivo');
}

// El prefijo decide la clasificacion, y va en la PRIMERA linea que no sea
// comentario. Las dos mitades de esto estan medidas: un comentario delante NO
// rompe nada (el aplanado quita los comentarios antes de mirar el prefijo), y
// un prefijo que no sea MIO:/ajeno: da SIN CLASIFICAR, que es lo correcto.
{
    const conComentario = lista (['X'], { X: '# nota antes\nMIO: el prefijo va en la segunda\n' });
    igual ('un comentario antes del prefijo no lo estropea',
           corre (['motivo', conComentario.fichero, 'X']).out.trim(),
           'MIO: el prefijo va en la segunda');

    const sinPrefijo = lista (['Y'], { Y: 'Nota: se me ha olvidado el prefijo\n' });
    const r = corre (['motivo', sinPrefijo.fichero, 'Y']);
    igual ('un motivo sin prefijo sale entero', r.out.trim(), 'Nota: se me ha olvidado el prefijo');
    igual ('un motivo sin prefijo NO es un error (sale 0)', r.rc, 0);
}

// El BOM: se comprobo que NO rompe el prefijo. Node no lo quita al leer, pero el
// aplanado convierte los espacios en espacios y se lo lleva con ellos. Este
// caso esta aqui para que siga siendo verdad, no porque se esperase lo otro.
{
    const conBOM = lista (['B'],
                          { B: Buffer.concat ([Buffer.from ([0xEF, 0xBB, 0xBF]),
                                              Buffer.from ('MIO: con BOM delante\n', 'utf8')]) });
    igual ('un BOM delante NO rompe el prefijo',
           corre (['motivo', conBOM.fichero, 'B']).out.trim(), 'MIO: con BOM delante');
}

// El CR de Windows: ctest escribe sus logs con CRLF y el motivo puede venir de
// ahi. Sin quitarlo, el motivo trae un \r pegado.
{
    const conCR = lista (['C'], { C: 'MIO: con CR al final\r\n' });
    igual ('un CR al final del motivo se quita',
           corre (['motivo', conCR.fichero, 'C']).out.trim(), 'MIO: con CR al final');
}

// La carpeta de motivos es RELATIVA AL FICHERO JSON, no al directorio de
// trabajo. Resuelta desde el cwd, un verify lanzado desde otro sitio lee otra
// lista sin que nada lo delate. Se prueba cambiando de cwd a proposito.
{
    const { fichero } = lista (['R'], { R: 'MIO: relativo al JSON\n' });
    const r = cp.spawnSync (process.execPath, [LIB, 'motivo', fichero, 'R'],
                            { encoding: 'utf8', cwd: process.env.SystemRoot || 'C:/' });
    igual ('la carpeta se resuelve con el JSON delante, no con el cwd',
           String (r.stdout || '').trim(), 'MIO: relativo al JSON');
}

// Y con una ruta RELATIVA de verdad en el JSON, que es como esta escrito.
{
    const dir = fs.mkdtempSync(path.join(TMP, 'r'));
    const km = path.join(dir, 'known');
    fs.mkdirSync(km, { recursive: true });
    fs.writeFileSync(path.join(km, 'Z.txt'), 'ajeno: con carpeta relativa\n');
    const f = path.join(dir, 'j.json');
    fs.writeFileSync(f, JSON.stringify ({ '_motivos': 'known', entradas: ['Z'] }));
    igual ('una carpeta relativa dentro del JSON tambien funciona',
           corre (['motivo', f, 'Z']).out.trim(), 'ajeno: con carpeta relativa');
}

// ============================================================================
//  2. LA LISTA MAL ESTA: ESTOS CASOS TIENEN QUE FALLAR EN VOZ ALTA
// ============================================================================
seccion ('2. La lista mal esta (tiene que fallar, y decir por que)');

// Cada uno de estos salia con codigo 1 y SIN UNA PALABRA: el motivo vivia en
// `cargar` y solo lo imprimia `bateria`, que no es quien se llama a mano. Ahora
// el motivo va a stderr, y estos casos comprueban que siga yendo.
// Cada caso dice QUE se rompe, y el helper monta el JSON con lo que hace falta
// para que se rompa ESO y no otra cosa. La clave `_motivos` se pone SIEMPRE
// salvo que el caso sea el de "_motivos falta": si no, el `cargar` se para en
// la primera comprobacion y todos los casos dicen lo mismo, que es como se
// empiezan a leer pruebas que no prueban nada.
const casos = [
    ['sin la clave _motivos',        { entradas: ['A'] },   { A: 'MIO: a\n' }, 'sinMotivos', '_motivos'],
    ['con entradas que no es lista', { entradas: 'A' },      { A: 'MIO: a\n' }, null,         'no es una lista'],
    ['con un nombre repetido',       { entradas: ['A','A'] },{ A: 'MIO: a\n' }, null,         'repetido'],
    ['con un nombre con guion bajo', { entradas: ['_A'] },   { A: 'MIO: a\n' }, null,         'guion bajo'],
    ['con un nombre que no vale como fichero', { entradas: ['a/b'] }, { A: 'MIO: a\n' }, null, 'no vale como nombre'],
    ['sin el fichero de motivo',     { entradas: ['NO_EXISTE'] }, {},             null,         'no esta el fichero de motivo']
];

for (const [nombre, j, motivos, sinMotivos, trozo] of casos) {
    const { fichero } = lista (j.entradas, motivos, { sinMotivos: sinMotivos });
    const r = corre (['cuenta', fichero]);
    igual (nombre + ': sale con 1', r.rc, 1);
    ok   (nombre + ': dice POR QUE en stderr', r.errR.indexOf (trozo) !== -1,
         'stderr: ' + JSON.stringify (r.errR.trim()));
    ok   (nombre + ': stdout queda vacio', r.out.trim() === '',
         'stdout: ' + JSON.stringify (r.out.trim()));
}

// El motivo vacio y el de solo comentarios: NO es un aviso, es un error. Una
// entrada con el motivo en blanco clasifica el rojo sin decir nada, que es peor
// que no clasificarlo.
{
    const vacio = lista (['V'], { V: '' });
    igual ('un motivo vacio sale con 1', corre (['cuenta', vacio.fichero]).rc, 1);

    const soloComments = lista (['C'], { C: '# todo comentario\n# mas comentarios\n' });
    const r = corre (['cuenta', soloComments.fichero]);
    igual ('un motivo de solo comentarios sale con 1', r.rc, 1);
    ok   ('...y dice que esta vacio', r.errR.indexOf ('vacio') !== -1, 'stderr: ' + r.errR.trim());
}

// JSON ilegible del todo, y una raiz que no es objeto.
{
    const dir = fs.mkdtempSync(path.join(TMP, 'r'));
    const roto = path.join(dir, 'j.json');
    fs.writeFileSync(roto, '{ esto no es json');
    const r = corre (['cuenta', roto]);
    igual ('un JSON ilegible sale con 1', r.rc, 1);
    ok   ('...y dice que no se puede leer como JSON', r.errR.indexOf ('JSON') !== -1, 'stderr: ' + r.errR.trim());

    const array = path.join(dir, 'a.json');
    fs.writeFileSync(array, '[1,2,3]');
    igual ('una raiz que es un array sale con 1', corre (['cuenta', array]).rc, 1);
}

// ============================================================================
//  3. EL DESTINO
// ============================================================================
seccion ('3. El destino');

// Un numero NO es una ruta: node lo lee como descriptor de fichero, no como
// nombre, y escribe en la nada. Este es el fallo que dejo siete ficheros basura
// en la raiz del proyecto ("false" y "sin-pid"), sin rastrear, que un `git add
// -A` se lleva. Se comprueba que ahora se NEGA y que no se crea el fichero.
{
    const { fichero } = lista (['D'], { D: 'MIO: d\n' });
    // El destino DE VERDAD tiene que ser un numero, porque un nombre de fichero
    // como "no-deberia-existir" es una ruta legitima y no hay nada que negar.
    // Con un nombre, el programa hace bien lo que tiene que hacer: escribir ahi.
    const numerico = '424242';
    const ruta = path.join(TMP, 'no-deberia-existir');

    // Y se lanza con `cwd` en un temporal, NO en el de quien lanza esto. Motivo:
    // un destino relativo se resuelve contra el cwd del proceso, asi que si
    // esta prueba se lanzara desde la raiz del proyecto y el guard fallara, el
    // fichero apareceria EN EL REPO. Que es exactamente lo que paso al romper
    // el guard a proposito: se quedo un `424242` con un `1` dentro en la raiz.
    // Una prueba de regresion que ensucia el arbol cuando el codigo esta roto
    // es una prueba que hay que ir a limpiar a mano.
    const r = corre (['cuenta', fichero, numerico], { cwd: TMP });
    igual ('un destino numerico sale con 2', r.rc, 2);
    ok   ('...y dice que es un numero', r.errR.indexOf ('numero') !== -1, 'stderr: ' + r.errR.trim());
    ok   ('...y NO crea un fichero con ese nombre', !fs.existsSync(path.join(TMP, numerico)),
         'se ha creado ' + path.join(TMP, numerico));

    // Y una ruta de verdad sigue funcionando, que es lo que no hay que romper.
    const ok1 = corre (['cuenta', fichero, ruta], { cwd: TMP });
    igual ('una ruta normal sigue escribiendo', ok1.rc, 0);
    ok   ('...y el fichero tiene el numero dentro', fs.existsSync(ruta) &&
         fs.readFileSync(ruta, 'utf8').trim() === '1',
         'contenido: ' + (fs.existsSync(ruta) ? JSON.stringify(fs.readFileSync(ruta,'utf8')) : 'no existe'));
}

// Y el guard de antes: el destino no puede ser uno de los ficheros de entrada,
// que es el fallo que borro el JSON de los conocidos en el primer prototipo.
{
    const { fichero } = lista (['E'], { E: 'MIO: e\n' });
    const r = corre (['cuenta', fichero, fichero]);
    igual ('escribir encima de la entrada sale con 2', r.rc, 2);
    ok   ('...y dice que el destino ES una entrada', r.errR.indexOf ('ES una entrada') !== -1,
         'stderr: ' + r.errR.trim());
}

// Sin destino, el resultado sale por stdout y no se crea nada.
{
    const { fichero } = lista (['F'], { F: 'MIO: f\n' });
    const r = corre (['cuenta', fichero]);
    igual ('sin destino, el resultado va por stdout', r.out.trim(), '1');
    ok   ('...y no escribe ficheros', fs.readdirSync(path.dirname(fichero)).indexOf('1') === -1);
}

// ============================================================================
//  4. ARREGLADOS: UN LOG QUE NO SE LEE NO ES UN LOG SIN ROJOS
// ============================================================================
seccion ('4. arreglados');

// El fallo que se arreglo: con el log de rojos AUSENTE, `arreglados` decia que
// los cinco conocidos ya no fallaban, con el texto de quitar cada entrada del
// indice y BORRAR su fichero de motivo. Un log que no se lee no es un log sin
// rojos: no se ha ejecutado nada, no se ha pasado nada.
{
    const { fichero } = lista (['G'], { G: 'MIO: g\n' });
    const r = corre (['arreglados', fichero, path.join(TMP, 'no-existe.log'), '', '']);
    igual ('con el log ausente sale con 1', r.rc, 1);
    ok   ('...y NO dice ARREGLO', r.out.indexOf('ARREGLO') === -1, 'stdout: ' + JSON.stringify(r.out.trim()));
    ok   ('...y explica por que', r.errR.indexOf('log de rojos') !== -1, 'stderr: ' + r.errR.trim());
}

// El log VACIO es otro caso y NO es un error: ctest trunca el fichero cuando
// todo pasa, asi que vacio quiere decir de verdad cero rojos.
//
// Y aqui se pasa la lista de MEDIDOS, porque en la seccion 12 se explica por que
// sin lista no se puede decir ARREGLO de nada: lo que sale de "no estar en la
// lista de rojos" es "ha pasado" solo si el binario es de esta pasada. Esta
// seccion va de otra cosa (el log), asi que hay que dar por buena esa segunda
// mitad para poder mirar la primera. Y se pasa una lista de NO MEDIDOS VACIA,
// que es lo que significa "todos los binarios son de esta pasada".
{
    const { fichero } = lista (['H'], { H: 'MIO: h\n' });
    const vacio = path.join(TMP, 'vacio.log');
    fs.writeFileSync(vacio, '');
    const medidos = path.join(TMP, 'medidos4.txt');
    fs.writeFileSync(medidos, '');
    const r = corre (['arreglados', fichero, vacio, '', '', medidos]);
    igual ('con el log vacio sale con 0', r.rc, 0);
    ok   ('...y avisa de ARREGLO', r.out.indexOf('ARREGLO') !== -1, 'stdout: ' + r.out.trim());
}

// Con el log de verdad, solo se quejan los que NO estan en el. El CR se quita
// dentro: sin eso el nombre del test nunca casa y TODO conocido sale arreglado.
{
    const { fichero } = lista (['Falla', 'Pasa'],
                               { Falla: 'MIO: este falla\n', Pasa: 'MIO: este ya pasa\n' });
    const log = path.join(TMP, 'rojos.log');
    fs.writeFileSync(log, '12:Falla\r\n');
    const medidos = path.join(TMP, 'medidos4b.txt');
    fs.writeFileSync(medidos, '');
    const r = corre (['arreglados', fichero, log, '', '', medidos]);
    ok   ('avisa de ARREGLO solo del que no esta en el log',
         r.out.indexOf('ARREGLO') !== -1 && r.out.indexOf('Pasa') !== -1 &&
         r.out.split('ARREGLO').length - 1 === 1,
         'stdout: ' + r.out.trim());
    ok   ('...y del que falla no dice nada', r.out.indexOf('Falla') === -1, 'stdout: ' + r.out.trim());
}

// ============================================================================
//  5. EL RESUMEN Y LA COMPARA
// ============================================================================
seccion ('5. El resumen y la compara');

// El resumen es lo que comparan los gemelos, asi que una cuenta mal ahi es una
// divergencia inventada. Se comprueba que el motivo manda sobre el paso, y que
// las comillas que pone el .bat no se cuelan en el recuento.
{
    const entradas = path.join(TMP, 'rojos.txt');
    fs.writeFileSync(entradas,
        '2\tNEURONiK_Uno\tMIO: mio\n' +
        '2\tNEURONiK_Dos\tajeno: del otro\n' +
        '2\tNEURONiK_Tres\tSIN CLASIFICAR: no esta en la lista\n' +
        '2\tNEURONiK_Cuatro\t"motivo entre comillas"\n');
    const res = path.join(TMP, 'res.txt');
    const lentos = path.join(TMP, 'lentos.txt');
    fs.writeFileSync(lentos, 'NEURONiK_Lento1\nNEURONiK_Lento2\n');

    const r = corre (['resumen', entradas, res, '2', '53', lentos, '0', '5', '0']);
    igual ('resumen sale con 0', r.rc, 0);

    const leido = {};
    for (const l of fs.readFileSync(res, 'utf8').split(/\r?\n/)) {
        const p = l.split('\t');
        if (p.length >= 2) leido[p[0]] = p[1];
    }
    igual ('cuenta los rojos',            leido['fallos'],        '4');
    igual ('cuenta los mios',             leido['mios'],          '1');
    igual ('cuenta los ajenos',           leido['ajenos'],        '1');
    igual ('cuenta los sin clasificar',   leido['sinClasificar'], '2');
    igual ('cuenta los lentos',           leido['lentos'],        '2');
    igual ('guarda el numero de pasos',   leido['pasos'],         '5');
    igual ('guarda los huerfanos',         leido['huerfanos'],     '0');
    ok   ('el motivo con comillas NO cuenta como mio',
         !fs.readFileSync(res, 'utf8').includes('NEURONiK_Cuatro\tMIO'), 'resumen:\n' + fs.readFileSync(res, 'utf8'));
    // Y el por que de las dos filas SIN CLASIFICAR, que es donde se ve que las
    // comillas son texto y no una etiqueta: el motivo de una empieza por
    // "SIN CLASIFICAR:" y el de la otra es un texto entre comillas.
    ok   ('las comillas del motivo no decides la clasificacion',
         fs.readFileSync(res, 'utf8').includes('NEURONiK_Cuatro\tSIN CLASIFICAR'),
         'resumen:\n' + fs.readFileSync(res, 'utf8'));
}

// `lentos` es un FICHERO y no un numero. Pasarle un 0 no da error: node lo lee
// como el descriptor 0 y cuenta lo que le llegue por stdin.
{
    const entradas = path.join(TMP, 'rojos2.txt');
    fs.writeFileSync(entradas, '1\tT\tMIO: m\n');
    const r = corre (['resumen', entradas, path.join(TMP, 'r2.txt'), '1', '53', '0', '0', '', '']);
    igual ('un lentos numerico sale con 2', r.rc, 2);
    ok   ('...y dice que es un numero', r.errR.indexOf ('numero') !== -1, 'stderr: ' + r.errR.trim());
}

// La compara: dos resumenes iguales dan 0, y uno distinto da 1 nombrando el
// campo. Es la pregunta que mas veces ha salido mal sobre este repo.
{
    const a = path.join(TMP, 'a.txt');
    const b = path.join(TMP, 'b.txt');
    const c = path.join(TMP, 'c.txt');
    const cuerpo = 'v\t1\npasos\t5\nconocidos\t2\ntests\t53\nfallos\t1\nmios\t1\najenos\t0\nsinClasificar\t0\nlentos\t0\ntocoTimeout\t0\nhuerfanos\t0\nrojo\t2\tT\tMIO\n';
    fs.writeFileSync(a, cuerpo);
    fs.writeFileSync(b, cuerpo);
    fs.writeFileSync(c, cuerpo.replace('fallos\t1', 'fallos\t2'));

    igual ('dos resumenes iguales dan 0', corre (['compara', a, b, 'sh', 'bat']).rc, 0);
    igual ('dos resumenes distintos dan 1', corre (['compara', a, c, 'sh', 'bat']).rc, 1);
    igual ('un resumen que no se puede leer da 2',
           corre (['compara', a, path.join(TMP, 'no-existe.txt'), 'sh', 'bat']).rc, 2);
}

// ============================================================================
//  6. LOS BINARIOS RANCIOS
// ============================================================================
seccion ('6. Los binarios rancios');

// Esta orden se escribio porque costs cuatro rojos falsos y varios dias: el
// paso 1 del verify compila SEIS targets a proposito, no los tests, asi que casi
// todos los .exe se quedan de una pasada anterior y sus resultados se cuentan
// como si fueran de ahora. Aqui se monta un arbol de mentira con un
// CTestTestfile y se comprueba que avisa de lo que tiene que avisar.
function arbolRuncios (casos) {
    const dir = fs.mkdtempSync (path.join (TMP, 'b'));
    const rel = path.join (dir, 'Release');
    fs.mkdirSync (rel, { recursive: true });

    const lineas = [];
    for (const c of casos) {
        if (! c.creaExe) continue;
        fs.writeFileSync (path.join (rel, c.nombre + '.exe'), 'x');
        lineas.push ('add_test("' + c.nombre + '" "' + path.join (rel, c.nombre + '.exe') + '")');
    }
    // Un test cuyo programa NO esta en este arbol: los de Playwright y los de
    // contrato se registran con el node.exe del sistema. No lo compila nadie aqui
    // y no hay binario rancio que mirar.
    lineas.push ('add_test("NEURONiK_WebUiLocalModeE2e" "C:/Program Files/nodejs/node.exe" "e2e/localMode.spec.js")');

    fs.writeFileSync (path.join (dir, 'CTestTestfile.cmake'), lineas.join ('\n') + '\n');
    return dir;
}

// El caso de no-falso-positivo: si el paso 1 dice que los ha construido todos,
// no puede quedar ninguno marcados. Este es el que hace que el aviso sirva para
// algo: un aviso que se queja siempre acaba mirandose sin leerse.
{
    const dir = arbolRuncios ([{ nombre: 'T1', creaExe: true }, { nombre: 'T2', creaExe: true }]);
    const r = corre (['rancios', dir, 'Release', '', '', 'T1', 'T2']);
    igual ('si todo se ha construido, no avisa de ninguno (rc 0)', r.rc, 0);
    ok   ('...y no dice BINARIO RANCIO', r.outR.indexOf ('BINARIO RANCIO') === -1, 'stdout: ' + r.outR.trim());
    ok   ('...y cuenta los construidos', r.outR.indexOf ('2 reconstruidos') !== -1, 'stdout: ' + r.outR.trim());
}

// Y el caso real: solo uno se ha construido, el otro se queda fuera.
{
    const dir = arbolRuncios ([{ nombre: 'T1', creaExe: true }, { nombre: 'T2', creaExe: true }]);
    const r = corre (['rancios', dir, 'Release', '', '', 'T1']);
    igual ('con uno sin construir avisa (rc 0, es un aviso y no un fallo)', r.rc, 0);
    ok   ('...y nombra al que falta', r.outR.indexOf ('T2') !== -1, 'stdout: ' + r.outR.trim());
    ok   ('...y no nombra al que si', r.outR.split ('T1').length - 1 === 0 || !r.outR.split('BINARIO RANCIO')[1].includes('T1'),
         'stdout: ' + r.outR.trim());
}

// Sin lista de construidos no se juzga nada: es el caso de llamarlo a mano, y
// avisar de los cincuenta y tres seria mentir.
{
    const dir = arbolRuncios ([{ nombre: 'T1', creaExe: true }]);
    const r = corre (['rancios', dir, 'Release', '', '']);
    igual ('sin lista de construidos no avisa de nada', r.outR.indexOf ('BINARIO RANCIO'), -1);
}

// PERO --sin-build si avisa, y con otro encabezado. Es el caso de `--no-build`:
// el paso 1 no se ha ejecutado, no hay lista que mandar, y el silencio de
// antes hacia leer 53 tests de binarios que nadie habia compilado creyendose
// que si. Aqui el aviso tiene que ser el mas fuerte de los tres.
{
    const dir = arbolRuncios ([{ nombre: 'T1', creaExe: true }, { nombre: 'T2', creaExe: true }]);
    const r = corre (['rancios', dir, 'Release', '', '', '--sin-build']);
    igual ('--sin-build avisa (rc 0, sigue siendo un aviso)', r.rc, 0);
    ok   ('...con su propio encabezado', r.outR.indexOf ('NO SE HA CONSTRUIDO NADA') !== -1, 'stdout: ' + r.outR.trim());
    ok   ('...y dice que no hay ninguno reconstruido', r.outR.indexOf ('0 reconstruidos') !== -1, 'stdout: ' + r.outR.trim());
    ok   ('...y no usa el encabezado de rancio', r.outR.indexOf ('BINARIO RANCIO') === -1, 'stdout: ' + r.outR.trim());
    ok   ('...y el flag no se cuela como un target', r.outR.indexOf ('--sin-build') === -1, 'stdout: ' + r.outR.trim());
    ok   ('...y cuenta los dos', /2 nativos/.test (r.outR) && /2 con el binario de una pasada anterior/.test (r.outR),
         'stdout: ' + r.outR.trim());
}

// La linea de cabecera tiene que CUADRAR: los reconstruidos mas los que se
// quedan fuera son los tests nativos del arbol. Antes contaba los targets del
// paso 1, y dos de los seis no son tests, asi que la suma no salia y el aviso
// parecia estar contando cosas que no son.
{
    const casos = [{ nombre: 'T1', creaExe: true }, { nombre: 'T2', creaExe: true },
                   { nombre: 'T3', creaExe: true }, { nombre: 'T4', creaExe: true }];
    const dir = arbolRuncios (casos);
    // Dos tests, y en la lista un target de mas que no es test (FxExport) y otro
    // que no esta ni en la lista ni en la bateria.
    const r = corre (['rancios', dir, 'Release', '', '', 'T1', 'T2', 'NoEsUnTest']);
    const m = /(\d+) nativos, (\d+) reconstruidos por el paso 1(?:, (\d+) con el binario)?/.exec (r.outR);
    ok   ('la cabecera se puede sumar', m !== null, 'stdout: ' + r.outR.trim());
    if (m) {
        igual ('nativos = reconstruidos + rancios',
               Number (m[1]), Number (m[2]) + Number (m[3] || 0));
        igual ('cuenta los tests, no los targets', Number (m[2]), 2);
    }
}

// Un test registrado sin .exe se avisa aparte, porque se esta contando como
// verde algo que no se ha ejecutado nunca.
{
    const dir = fs.mkdtempSync (path.join (TMP, 'b'));
    fs.writeFileSync (path.join (dir, 'CTestTestfile.cmake'),
        'add_test("NEURONiK_NoEsta" "' + path.join (dir, 'Release', 'NoEsta.exe') + '")');
    const r = corre (['rancios', dir, 'Release', '', '', 'NEURONiK_NoEsta']);
    ok   ('un test sin .exe avisa SIN BINARIO', r.outR.indexOf ('SIN BINARIO') !== -1, 'stdout: ' + r.outR.trim());
}

// Y el caso de los tests de node: NO tienen que aparecer ni como rancios ni
// como sin binario. Un aviso que se queja de trece tests que no existen es un
// aviso que nadie lee.
{
    const dir = fs.mkdtempSync (path.join (TMP, 'b'));
    const rel = path.join (dir, 'Release');
    fs.mkdirSync (rel, { recursive: true });
    fs.writeFileSync (path.join (rel, 'T1.exe'), 'x');
    fs.writeFileSync (path.join (dir, 'CTestTestfile.cmake'),
        'add_test("T1" "' + path.join (rel, 'T1.exe') + '")' + '\n' +
        'add_test("NEURONiK_WebUiVisualRegression" "C:/Program Files/nodejs/node.exe" "e2e/visual.spec.js")' + '\n' +
        'add_test("NEURONiK_WorkletSync" "C:/Program Files/nodejs/node.exe" "x.mjs")' + '\n');
    const r = corre (['rancios', dir, 'Release', '', '', 'T1']);
    ok   ('los tests de node no se cuentan como tests nativos',
         !r.outR.includes ('WebUiVisualRegression') && !r.outR.includes ('WorkletSync') &&
         !r.outR.includes ('SIN BINARIO'),
         'stdout: ' + r.outR.trim());
}

// Un arbol sin CTestTestfile dice que no lo ha encontrado, y sale con 1: sin
// ese fichero no hay ni un test que mirar, y callarse seria un "todo bien".
{
    const dir = fs.mkdtempSync (path.join (TMP, 'b'));
    const r = corre (['rancios', dir, 'Release', '', '', 'T1']);
    igual ('sin CTestTestfile sale con 1', r.rc, 1);
    ok   ('...y dice que no lo ha podido leer', r.errR.indexOf ('CTestTestfile') !== -1, 'stderr: ' + r.errR.trim());
}

// ============================================================================
//  7. LAS DEMAS ORDENES
// ============================================================================
seccion ('7. Las demas ordenes');

{
    const dir = fs.mkdtempSync(path.join(TMP, 'v'));
    const f = path.join(dir, 'pid.txt');
    fs.writeFileSync(f, '4242\r\n');
    const r = corre (['leepid', f]);
    igual ('leepid lee el PID y le quita el CR', r.out.trim(), '4242');
}
{
    // vivo de un PID imposible: no existe, y aun asi tiene que responder con
    // algo interpretable en vez de callarse.
    const r = corre (['vivo', '999999', '']);
    ok   ('vivo responde false|true', r.out.trim() === 'false' || r.out.trim() === 'true',
         'stdout: ' + JSON.stringify(r.out.trim()));
}
{
    const r = corre (['ordenQueNoExiste']);
    igual ('una orden desconocida sale con 2', r.rc, 2);
    ok   ('...y dice cual es el problema', r.errR.indexOf ('desconocida') !== -1, 'stderr: ' + r.errR.trim());
}

// ============================================================================
//  8. LA LISTA DE TARGETS DEL PASO 1
// ============================================================================
seccion ('8. La lista de targets del paso 1');

// El paso 1 compila lo que esta orden dice, y lo que no diga, no se compila: es
// la diferencia entre medir el codigo de ahora y medir el .exe de hace dias. Asi
// que esta orden no puede fallar callada. Se comprueba con un arbol de mentira,
// porque lo que importa es la REGLA (que entra y que no), no este proyecto.

// El mismo arbol que usa la seccion 6, aqui por lo que importa: de los tests que
// se registran, entran los nativos y NO entran los de node.
function arbolTargets (casos) {
    const dir = fs.mkdtempSync (path.join (TMP, 't'));
    const rel = path.join (dir, 'Release');
    fs.mkdirSync (rel, { recursive: true });
    const lineas = [];
    for (const c of casos) {
        if (c.creaExe) fs.writeFileSync (path.join (rel, c.nombre + '.exe'), 'x');
        lineas.push ('add_test("' + c.nombre + '" "' + path.join (rel, c.nombre + '.exe') + '")');
    }
    // Los de node: el node.exe del sistema, fuera del arbol. NO son targets de
    // este proyecto. Si se colaran, el paso 1 pediria a cmake un target que no
    // existe y se caeria el build entero.
    lineas.push ('add_test("NEURONiK_WebUiLocalModeE2e" "C:/Program Files/nodejs/node.exe" "e2e/localMode.spec.js")');
    lineas.push ('add_test("NEURONiK_WorkletSync" "C:/Program Files/nodejs/node.exe" "x.mjs")');
    fs.writeFileSync (path.join (dir, 'CTestTestfile.cmake'), lineas.join ('\n') + '\n');
    return dir;
}

{
    const dir = arbolTargets ([{ nombre: 'T1', creaExe: true }, { nombre: 'T2', creaExe: true }]);
    const r = corre (['targets', dir, 'Release']);
    igual ('la lista de targets sale con 0', r.rc, 0);
    igual ('...y son los nativos, ordenados, uno por linea', r.out.trim (), 'T1\nT2');
}

{
    // Un test cuyo .exe NO existe tambien entra: es justamente uno que todavia no
    // se ha compilado, y la lista es la de lo que hay que COMPILAR.
    const dir = arbolTargets ([{ nombre: 'T1', creaExe: true }, { nombre: 'T2', creaExe: false }]);
    const r = corre (['targets', dir, 'Release']);
    igual ('un test sin .exe tambien sale en la lista (hay que compilarlo)', r.out.trim (), 'T1\nT2');
}

{
    const dir = arbolTargets ([{ nombre: 'T1', creaExe: true }]);
    const r = corre (['targets', dir, 'Release']);
    ok   ('los tests de node NO salen en la lista de targets',
         !r.out.includes ('WebUiLocalModeE2e') && !r.out.includes ('WorkletSync'),
         'stdout: ' + r.out.trim ());
}

// Y lo que NO PUEDE ser: una lista vacia. Seria un paso 1 que compila nada y
// dice que ha compilado, que es el peor de los dos mundos. Con un arbol que no
// tiene CTestTestfile, la orden tiene que FALLAR, no devolver una lista vacia.
{
    const dir = fs.mkdtempSync (path.join (TMP, 't'));
    const r = corre (['targets', dir, 'Release']);
    igual ('sin CTestTestfile la lista sale con 1, no con una vacia', r.rc, 1);
    ok   ('...y dice cual es el problema', r.errR.indexOf ('CTestTestfile') !== -1, 'stderr: ' + r.errR.trim ());
}

// Y un arbol con CTestTestfile pero SOLO tests de node: tampoco hay nada que
// compilar, y tampoco puede ser una lista vacia.
{
    const dir = fs.mkdtempSync (path.join (TMP, 't'));
    fs.writeFileSync (path.join (dir, 'CTestTestfile.cmake'),
        'add_test("NEURONiK_WebUiLocalModeE2e" "C:/Program Files/nodejs/node.exe" "e2e/x.spec.js")\n');
    const r = corre (['targets', dir, 'Release']);
    igual ('un arbol solo con tests de node sale con 1', r.rc, 1);
    ok   ('...y lo dice', r.errR.indexOf ('nativo') !== -1, 'stderr: ' + r.errR.trim ());
}

// La lista que sale tiene que ser la MISMA que la que usa el aviso de rancios
// para contar los nativos, o los dos estan contando cosas distintas y no se
// ve. Aqui se comprueba con la cuenta que imprime la cabecera del aviso.
{
    const dir = arbolTargets ([{ nombre: 'T1', creaExe: true }, { nombre: 'T2', creaExe: true }]);
    const lista = corre (['targets', dir, 'Release']).out.trim ().split ('\n').filter (s => s).length;
    const r = corre (['rancios', dir, 'Release', '', '']);
    const m = /(\d+) nativos/.exec (r.outR);
    ok   ('la lista de targets y el aviso de rancios cuentan los mismos',
         m !== null && Number (m[1]) === lista,
         'lista: ' + lista + '  aviso: ' + (m ? m[1] : '?') + '\nstdout: ' + r.outR.trim ());
}

// ============================================================================
//  9. EL PARSEO DE LOS DOS GEMELOS
// ============================================================================
seccion ('9. El parseo de los dos gemelos');

// Esta seccion es la unica que lanza los DOS scripts, y por eso es la cara:
// cada caso son dos procesos de mas, y el unico caso que ejecuta algo de verdad
// (paso 1) tarda unos segundos. Se paga porque lo que vigila aqui no se puede
// mirar de otra manera.
//
// QUE VIGILA. Que los dos gemelos decidan LO MISMO con los MISMOS argumentos:
// el mismo codigo de salida, los mismos pasos, y el mismo aviso. El check de
// gemelos (verify_all_check.sh) no lo pilla: ese compara el RESUMEN del final, y
// un parseo que se equivoca se equivoca antes de llegar al resumen. Medido lo
// que hacia cada uno, con los mismos argumentos:
//
//   --only=9          el .sh no ejecutaba nada y salia con 0 ("todo en verde");
//                     el .bat se iba a los CINCO pasos, 70 s
//   --solo=1          el .sh se negaba a arrancar (rc 2); el .bat lo ignoraba
//                     sin decir ni una palabra
//   --NO-BUILD        construia en el .bat y no en el .sh
//   --only 1          la forma con espacio solo existia en el .bat
//
// O sea que un flag mal escrito hacia una de dos cosas muy distintas segun con
// que terminal se lanzara. Ahora los dos dicen lo mismo, y estos casos lo
// comprueban ejecutandolos de verdad en vez de leyendo el codigo.

// Corre un gemelo. El .bat va por `cmd /c` porque en Windows un .bat no es un
// ejecutable: hay que lanzarlo con la consola.
function gemelo (cual, args) {
    const esSh = cual === 'sh';
    const guion = path.join (__dirname, esSh ? 'verify_all.sh' : 'verify_all.bat');
    const r = esSh
        ? cp.spawnSync ('bash', [guion].concat (args), { encoding: 'utf8' })
        : cp.spawnSync ('cmd', ['/c', guion].concat (args), { encoding: 'utf8' });
    const todo = String (r.stdout || '') + String (r.stderr || '');
    return {
        rc:   r.status,
        txt:  todo.replace(/\r/g, ''),
        // Los pasos que empiezan. Es lo que de verdad dice si han aplicado la
        // misma lista, y no se parece a nada que comparen los dos al imprimirlo:
        // rutas, colores y el reloj ya se llevarían la comparacion.
        pasos: (todo.match (/PASO ([0-9])/g) || []).join (' ')
    };
}

// Un caso = una lista de argumentos, y lo que tienen que dar los dos.
const PARSEO = [
    // Un --only que no es un paso: ERROR y rc 2 en los dos. Antes uno no hacia
    // nada y salia en verde, y el otro se comia la bateria entera.
    { args: ['--only=9'],                      rc: 2, pasos: '',           texto: 'no es un paso' },
    { args: ['--only='],                       rc: 2, pasos: '',           texto: 'no es un paso' },
    { args: ['--only=1', '--only=9'],          rc: 2, pasos: '',           texto: 'no es un paso' },
    { args: ['--only', '9'],                   rc: 2, pasos: '',           texto: 'no es un paso' },
    // Un token con un espacio DENTRO: `--only="1 3"`. Es la forma de varios
    // pasos, y aqui es un error, porque un paso por llamada. Al .bat le llega
    // partido en tres (`--only`, `1 3`) por la capa de argumentos, que esta
    // misma se explica mas arriba en el bloque ARGS, asi que sin el aviso de
    // "el valor venia partido" se lo comia como `--only=1` y se ponia a
    // compilar. Medido: 50 segundos y la bateria entera.
    { args: ['--only=1 3'],                    rc: 2, pasos: '',           texto: 'no es un paso' },
    // Un flag que no se reconoce: AVISO y se sigue. Y el aviso lleva el token
    // entero, `--solo=1` y no `--solo` partido en dos lineas.
    { args: ['--solo=1', '--only=9'],          rc: 2, pasos: '',           texto: 'opcion no reconocida: --solo=1' }
];

for (const c of PARSEO) {
    const a = gemelo ('sh', c.args);
    const b = gemelo ('bat', c.args);
    const etq = '[' + c.args.join (' ') + ']';
    igual (etq + ' el .sh sale con ' + c.rc, a.rc, c.rc);
    igual (etq + ' el .bat sale con ' + c.rc, b.rc, c.rc);
    igual (etq + ' los dos dicen los mismos pasos', a.pasos, b.pasos);
    ok   (etq + ' ninguno se salta ningun paso', a.pasos === c.pasos && b.pasos === c.pasos,
         'sh: ' + JSON.stringify (a.pasos) + '  bat: ' + JSON.stringify (b.pasos));
    ok   (etq + ' los dos avisan de lo mismo (' + c.texto + ')',
         a.txt.indexOf (c.texto) !== -1 && b.txt.indexOf (c.texto) !== -1,
         'sh: ' + JSON.stringify (a.txt.slice (0, 160)) + '\nbat: ' + JSON.stringify (b.txt.slice (0, 160)));
}

// Y el caso que ejecuta de verdad, que es el que no se puede comprobar sin
// arrancar algo: `--only=1` tiene que arrancar SOLO el paso 1, y --no-build
// tiene que saltarse la compilacion sin quitar la cabecera del paso. Con la
// cabecera dentro del `if` de la compilacion, el .sh no imprimia "PASO 1" y el
// .bat si: los dos hacian lo mismo y parecian dos medidas distintas.
//
// Y va en MAYUSCULAS a proposito, por una razon: es el unico caso que
// comprueba que las opciones no distinguen mayusculas. Si el .sh dejara de pasar
// el token por `${1,,}`, `--NO-BUILD` pasaria a ser una opcion desconocida (que
// avisa y se sigue), el paso 1 compilaria, y la linea de "no se ha compilado
// nada" no saldria. En minuscula el caso pasaria igual con o sin la conversion,
// o sea que no comprobaria nada: una comprobacion que no puede fallar no es una
// comprobacion.
{
    const a = gemelo ('sh', ['--NO-BUILD', '--only=1']);
    const b = gemelo ('bat', ['--NO-BUILD', '--only=1']);
    igual ('--only=1 arranca solo el paso 1 (sh)', a.pasos, 'PASO 1');
    igual ('--only=1 arranca solo el paso 1 (bat)', b.pasos, 'PASO 1');
    ok   ('--no-build no quita la cabecera del paso, en los dos',
         a.txt.indexOf ('PASO 1: build') !== -1 && b.txt.indexOf ('PASO 1: build') !== -1);
    ok   ('--no-build en mayusculas salta la compilacion en los dos',
         a.txt.indexOf ('no se ha compilado nada') !== -1 &&
         b.txt.indexOf ('no se ha compilado nada') !== -1,
         'sh: ' + JSON.stringify (a.txt.slice (0, 200)));
}

// Y la ayuda, que es donde se lee cuando alguien ha escribido mal un flag. No se
// compara entera: cada uno imprime su nombre de script. Se compara que los dos
// digan las tres reglas, que es donde se separaban.
{
    const a = gemelo ('sh', ['--help']);
    const b = gemelo ('bat', ['--help']);
    igual ('--help sale con 0 en los dos', a.rc + '/' + b.rc, '0/0');
    for (const frase of ['no distinguen mayusculas', 'no se reconoce avisa', 'del 1 al 5', 'VERIFY_LENTO']) {
        ok   ('los dos --help dicen "' + frase + '"',
             a.txt.indexOf (frase) !== -1 && b.txt.indexOf (frase) !== -1,
             'sh: ' + JSON.stringify (a.txt.slice (0, 200)));
    }
}

// ============================================================================
//  10. EL `echo` CON UN PARENTESIS DENTRO DE UN BLOQUE
// ============================================================================
seccion ('10. Los parentesis sueltos en los bloques de batch');

// EL FALLO, MEDIDO. En :build_todos del .bat, el camino de error del build
// acababa con `echo (!LOGB!)`. Para cmd ese parentesis de cierre no es texto: es
// el cierre del `if errorlevel 1 (` de cuatro lineas mas arriba. El bloque se
// acababa ahi, y las tres lineas siguientes -- `set "FALLO_BUILD=1"`,
// `set "SALIR_POR_ROJOS=1"` y `exit /b 0` -- quedaban FUERA del `if`.
//
// O sea que se ejecutaban SIEMPRE, y el paso 1 no podia salir en verde: cmake
// acababa con rc 0, el log no tenia ni un error, y el paso 1 se quejaba de que
// "un target no compila". La linea de PASA no salia nunca.
//
// POR QUE NO SE PILLABA NADA. No hay sintaxis mal, no hay aviso de cmd, y el
// .bat entero se ejecutaba de principio a fin. Los casos de la seccion 9 pasan
// porque el parseo de argumentos va bien; los de la 8, porque la orden `targets`
// esta en el .js y no en el batch. El fallo estaba en una linea que solo se
// ejecuta cuando el build falla, y que hacia que el build pareciera que siempre
// falla. Es la razon de que esta seccion mire el TEXTO y no lance nada.
//
// QUE VIGILA. Que en ningun `.bat` haya un `echo` dentro de un bloque
// `if ... (` con un parentesis sin escapar. Se lee el fichero de arriba abajo
// llevando un contador de nivel de bloque: por encima de cero, un parentesis
// suelto en un `echo` se come estructura.
//
// El coste es que hay que distinguir un parentesis de TEXTO de uno de SINTAXIS,
// y el unico que sabe eso es cmd. La regla es conservadora a proposito: se
// ignoran las comillas (dentro si es texto), los parentesis ya escapados con ^,
// y las lineas de `for`, que llevan su propio parentesis de constructor. Ante la
// duda se avisa: revisarlo cuesta un minuto y saltarselo cuesta un rojo que
// nadie entiende.

// Deja una linea en la forma que le interesa a este detector: sin lo que va
// entre comillas, que es texto, y sin lo que va escapado con ^, que tampoco
// cuenta. El escape hay que comerse de dos en dos y no con una sustitucion:
// `^()` es un parentesis de cierre escapado pegado a uno de apertura, y quitar
// solo el `^(` dejaria un `)` suelto que el detector contaria como estructura.
function sinComillasNiEscapados (cruda) {
    let fuera = '';
    let dentro = false;
    for (let i = 0; i < cruda.length; i++) {
        const c = cruda[i];
        if (c === '"') { dentro = !dentro; continue; }
        if (dentro) continue;
        if (c === '^') { i += 1; continue; }   // el siguiente, sea el que sea
        fuera += c;
    }
    return fuera;
}

// Devuelve las lineas sospechosas y el nivel de bloque final de un .bat.
function parentesisSueltos (texto) {
    const lineas = texto.split (/\r?\n/);
    const malas = [];
    let nivel = 0;
    for (let i = 0; i < lineas.length; i++) {
        const cruda = lineas[i];
        if (/^\s*(REM|::)\b/i.test (cruda)) continue;
        const limpia = sinComillasNiEscapados (cruda);
        const esEco = /^\s*echo\b/i.test (limpia) || /[&|]\s*echo\b/i.test (limpia);
        if (nivel > 0 && esEco && /[()]/.test (limpia)) {
            malas.push ((i + 1) + ': ' + cruda.trim ());
        }
        const abre = (limpia.match (/\(/g) || []).length;
        const cierra = (limpia.match (/\)/g) || []).length;
        nivel += abre - cierra;
    }
    return { malas: malas, nivel: nivel };
}

for (const nombre of ['verify_all.bat']) {
    const ruta = path.join (__dirname, nombre);
    ok (nombre + ' esta para poder leerlo', fs.existsSync (ruta));
    if (!fs.existsSync (ruta)) continue;
    const r = parentesisSueltos (fs.readFileSync (ruta, 'utf8'));
    ok (nombre + ' no tiene `echo` con parentesis dentro de un bloque (' +
         r.malas.length + ' sospechoso(s))',
        r.malas.length === 0,
        r.malas.join ('\n'));
    // El detector tambien tiene que cerrar lo que abre. Si no, daria "todo en
    // verde" por no mirar nada, que es la forma mas facil de que un test verde
    // no mire. Este fichero tiene que dejar el nivel en cero.
    ok (nombre + ' el detector deja el nivel de bloque en cero',
        r.nivel === 0,
        'nivel final: ' + r.nivel);
}

// Y que el detector NO se coma los parenteses legitimos, que son la mayoria y
// son la razon de que el aviso sea raro: si los senalara todos, dejaria de
// mirarse en cuanto saliera el primero de verdad.
//
// Ojo con el caso de `^(` pegado a `)`: en batch NO es un parentesis escapado
// doble, es uno de apertura escapado y uno de cierre de verdad, y el segundo se
// come el bloque. Por eso el ejemplo los lleva separados, que es la forma en que
// se escriben de verdad.
{
    const r = parentesisSueltos (
        '@echo off\r\n' +
        'REM (esto es un parentesis de comentario)\r\n' +
        'if 1 EQU 1 (\r\n' +
        '    echo   con ^(escapado^) de verdad\r\n' +
        '    echo   y entre comillas "(esto tambien)" es texto\r\n' +
        '    echo   con %%V%% y %%L%% de una variable\r\n' +
        '    for /f "usebackq delims=" %%L in ("x.txt") do (\r\n' +
        '        echo   %%L\r\n' +
        '    )\r\n' +
        ')\r\n' +
        'echo   este ni esta dentro de un bloque (asi que no molesta)\r\n');
    ok ('el detector no senala parenteses legitimos', r.malas.length === 0,
        r.malas.join ('\n'));
    ok ('el detector cuenta bien el bloque del ejemplo', r.nivel === 0,
        'nivel final: ' + r.nivel);
}

// Y el caso bueno de verdad: el texto que ROMPIO el paso 1 tiene que
// detectarse, y la version arreglada tiene que salir limpia. Sin esto, el test
// de arriba pasaria con un detector que no detecta nada, o con uno que senala
// todo y nadie lee.
{
    const roto = [
        '@echo off',
        'if errorlevel 1 (',
        '    echo   el build ha fallado',
        '    set "FALLO_BUILD=1"',
        '    echo           (!LOGB!)',
        '    exit /b 0',
        ')',
        'echo   PASA',
    ].join ('\r\n');
    const r = parentesisSueltos (roto);
    ok ('el detector pilla el `echo (!LOGB!)` que rompio el paso 1',
        r.malas.length === 1 && r.malas[0].indexOf ('LOGB') !== -1,
        r.malas.join ('\n'));

    // La MISMA estructura con el parentesis quitar, que es como quedo. Si esta
    // saliera sospechosa, el detector estorbaria mas de lo que ayuda.
    const arreglado = roto.replace ('echo           (!LOGB!)', 'echo           log del build: !LOGB!');
    const r2 = parentesisSueltos (arreglado);
    ok ('la version arreglada no da ninguna alarma', r2.malas.length === 0,
        r2.malas.join ('\n'));
}

// ============================================================================
//  11. EL PASO 2 DECIDE CON EL CODIGO DE SALIDA DE CTEST
// ============================================================================
seccion ('11. El paso 2 no lee rojos de otra corrida');

// EL FALLO, MEDIDO. El paso 2 leia `LastTestsFailed.log` y lo daba por bueno,
// con el comentario de que "ctest escribe siempre". No lo escribe: ese fichero
// SOLO aparece cuando hay algo que falla. Y eso producia rojos que no existian
// de dos maneras, ambas vistas el 2026-10-01 con la bateria entera en verde
// (53 de 53):
//
//   - fichero ausente -> "no se genero LastTestsFailed.log; no se puede saber
//     que fallo", anotado como rojo;
//   - fichero de una corrida vieja -> se leia entero y listaba esos rojos.
//
// Las dos son el mismo fallo por los dos lados: un fichero que no es de ESTA
// corrida leido como si lo fuera. Y es justo lo que el paso 2 lleva unas lineas
// mas arriba avisando de que hacia mal. Se avisaba del caso de que ctest no
// estuviera en el PATH, y no del caso de que ctest no escribiera el fichero.
//
// QUE VIGILA. Que los dos scripts decidan los rojos con el codigo de salida de
// ctest, y no leyendo el fichero a pelo. Se comprueba en el texto porque el
// caso de verdad (una bateria entera) tarda tres minutos y aqui lo que importa
// es que la decision no vuelva a depender del fichero.

// Las dos lineas que deciden, tal cual estan en cada gemelo.
{
    const sh = fs.readFileSync (path.join (__dirname, 'verify_all.sh'), 'utf8');
    const bat = fs.readFileSync (path.join (__dirname, 'verify_all.bat'), 'utf8');

    // El codigo de salida tiene que estar GUARDADO en los dos.
    ok ('el .sh guarda el codigo de salida de ctest',
        /ctest --test-dir[^\n]*\n\s*rc_ctest=\$\?/.test (sh));
    ok ('el .bat guarda el codigo de salida de ctest',
        /ctest --test-dir[^\n]*\n\s*set "RC_CTEST=!ERRORLEVEL!"/.test (bat));

    // Y la rama de "todo en verde" tiene que existir en los dos, y tiene que
    // salir ANTES de leer el fichero. Sin eso, con la bateria en verde se sigue
    // leyendo un log que no es de esta corrida.
    const shVerde = sh.indexOf ('tests, 0 en rojo');
    const batVerde = bat.indexOf ('tests, 0 en rojo');
    ok ('los dos tienen la rama de "0 en rojo"', shVerde !== -1 && batVerde !== -1);
    ok ('los dos deciden por el codigo de salida y no por el fichero',
        shVerde !== -1 && shVerde < sh.indexOf ('rojos="$(grep -c .') &&
        batVerde !== -1 && batVerde < bat.indexOf ('in ("%FALLOSLOG%") do'));

    // Y el comentario viejo, que es lo que convencio de que estaba bien.
    ok ('ningun gemelo afirma que ctest escriba siempre ese fichero',
        sh.indexOf ('que ctest escribe siempre') === -1 &&
        bat.indexOf ('ctest escribe siempre') === -1);
}

// Y que el motivo de un rojo conocido no senale una causa comprobada que no lo
// era. El caso concreto: los tres WebUi*E2e se quitaron de la lista cuando
// pasaron, pero eso no sirve de nada si un rojo NUEVO vuelve a llevar el
// antivirus en el motivo, porque el antivirus no era la causa: no habia ninguna
// deteccion de Defender, y el bloqueo estaba en el store de pnpm.
{
    const sh = fs.readFileSync (path.join (__dirname, 'verify_all.sh'), 'utf8');
    const bat = fs.readFileSync (path.join (__dirname, 'verify_all.bat'), 'utf8');
    ok ('ningun gemelo culpa al antivirus de los .js de node_modules',
        sh.indexOf ('el antivirus de esta maquina no deja leer') === -1 &&
        bat.indexOf ('el antivirus de esta maquina no deja leer') === -1);
}

// ============================================================================
//  12. LOS DOS AVISOS DEL MISMO TEST NO PUEDEN DECIRSE LO CONTRARIO
// ============================================================================
seccion ('12. El aviso de rancios y el de ARREGLO, en el mismo test');

// EL FALLO, MEDIDO. Los dos avisos, separados por el ctest entero, se pisaban
// sin que se vieran. Con el caso real de un solo test (el .exe sin compilar en
// esta pasada) la pantalla decia:
//
//   BINARIO RANCIO / NO SE HA CONSTRUIDO NADA  1 test(s) ... NEURONiK_EjemploTest
//   ...
//   ARREGLO  NEURONiK_EjemploTest
//             quita "NEURONiK_EjemploTest" del indice y borra known/NEURONiK_EjemploTest.txt
//
// El primero dice "de este test no se sabe nada: su .exe es de antes". El
// segundo dice, del MISMO test, "esta arreglado, borra su motivo". Los dos
// tenian razon por su cuenta y los dos razon a medias: `arreglados` no recibia
// la lista de lo que se ha construido, asi que para el "no estar en la lista de
// rojos" no podia mas que leerlo como "haber pasado".
//
// Y el segundo aviso es el que MANDO. Pide borrar ficheros, asi que el que se
// equivoca aqui no dice una cosa falsa: dice que borres una entrada de la lista
// de conocidos por un test que puede que ni siquiera se haya ejecutado. Con la
// lista de conocidos de hace unos dias (tres entradas) y `--no-build` la trampa
// estaba armada.
//
// QUE VIGILA. Que `rancios` deje la lista de los no medidos, y que `arreglados`
// con esa lista NO pueda decir ARREGLO de ninguno de ellos.

// El AVISO DE RANCIOS DEJA LA LISTA. Con tres tests de los que solo uno se ha
// construido, el fichero tiene que decir los otros dos y NO el construido.
//
// Y la lista se pide con un FLAG (`--medidos=RUTA`), no con un argumento mas al
// final: la lista de construidos es de longitud variable, asi que un argumento
// suelto no tendria forma de separarse de ella. Que el flag no se cuele como un
// target lo vigila el caso de `--sin-build` de la seccion 6.
{
    const dir = arbolTargets ([{ nombre: 'T1', creaExe: true },
                              { nombre: 'T2', creaExe: true },
                              { nombre: 'T3', creaExe: true }]);
    const fichero = path.join (TMP, 'nom_medidos.txt');
    const r = corre (['rancios', dir, 'Release', '', '', '--medidos=' + fichero, 'T1']);
    igual ('rancios sale con 0 dejando la lista', r.rc, 0);
    const escrito = noMedidos (fichero);
    igual ('la lista son los no medidos, y solo ellos',
           escrito.map (e => e.nombre).join (','), 'T2,T3');
    // Y cada uno con SU motivo. La comprobacion del nombre va sola: si el motivo
    // se pegara al nombre, el nombre ya no seria "T2" y esta comprobacion caeria.
    ok   ('...y cada uno con el motivo de no haber sido reconstruido',
         escrito.length === 2 &&
         escrito.every (e => e.porque.indexOf ('no se ha compilado en esta pasada') !== -1),
         'escrito: ' + JSON.stringify (escrito));
}

// Sin construir nada (`--sin-build`), la lista es TODOS. Es el caso limite del
// aviso, y tambien el de la trampa: con `--no-build` no hay ni un solo binario
// de esta pasada.
{
    const dir = arbolTargets ([{ nombre: 'T1', creaExe: true },
                              { nombre: 'T2', creaExe: true }]);
    const fichero = path.join (TMP, 'nom_medidos_sinbuild.txt');
    const r = corre (['rancios', dir, 'Release', '', '', '--medidos=' + fichero, '--sin-build']);
    igual ('con --sin-build sale con 0', r.rc, 0);
    const escrito = noMedidos (fichero);
    igual ('...y la lista es la bateria entera',
           escrito.map (e => e.nombre).join (','), 'T1,T2');
    // Y el motivo del caso limite es el suyo, no el de "no lo ha compilado el
    // paso 1": con --no-build no es que estos testsuales .exe sean viejos, es
    // que no se ha compilado NADA. El texto de los dos es parecido a proposito
    // para que se confundan si alguien los lee por encima.
    ok   ('...y el motivo dice que no se ha compilado nada, no que su .exe es viejo',
         escrito.every (e => e.porque.indexOf ('no se ha compilado nada') !== -1),
         'escrito: ' + JSON.stringify (escrito));
}

// Y LA LISTA VACIA ES UNA LISTA, NO UNA AUSENCIA. Con destino pero SIN lista de
// construidos (un `rancios` llamado a mano) no se puede juzgar nada, y lo que no
// se puede juzgar no se anade: el fichero sale vacio, que `arreglados` lee como
// "se ha ejecutado el paso 1 y no habia nada rancio". La confusion importante es
// la de al reves, y la resuelve el caso siguiente.
{
    const dir = arbolTargets ([{ nombre: 'T1', creaExe: true },
                              { nombre: 'T2', creaExe: true }]);
    const fichero = path.join (TMP, 'nom_medidos_vacio.txt');
    const r = corre (['rancios', dir, 'Release', '', '', '--medidos=' + fichero]);
    igual ('sin lista de construidos sale con 0', r.rc, 0);
    igual ('...y la lista de no medidos sale VACIA (no juzga nada)',
           fs.readFileSync (fichero, 'utf8'), '');
    ok   ('...y no dice que haya ninguno rancio',
         r.outR.indexOf ('BINARIO RANCIO') === -1,
         'stdout: ' + r.outR.trim ());
}

// EL CHOQUE, MONTADO. Dos conocidos, ninguno en el log de rojos (ctest entero
// en verde) y uno de ellos sin medir: el que se ha ejecutado con el .exe de
// antes NO puede salir como ARREGLO, y en ningun caso con el consejo de borrar.
//
// Y la lista de no medidos aqui se escribe A MANO, como la de la seccion 13
// pero sin el motivo: es la lista de las de antes, y tiene que seguir valiendo.
// Un formato que se rompe al cambiarlo deja los avisos mudos en vez de mudar,
// que es el peor fallo posible de un formato.
{
    const { fichero } = lista (['T1', 'T2'],
                               { T1: 'MIO: uno\n', T2: 'MIO: dos\n' });
    const rojos = path.join (TMP, 'rojos12.log');
    fs.writeFileSync (rojos, '');
    const medidos = path.join (TMP, 'medidos12.txt');
    fs.writeFileSync (medidos, 'T2\n');

    const r = corre (['arreglados', fichero, rojos, '', '', medidos]);
    igual ('con la lista de no medidos sale con 0', r.rc, 0);

    // El que SI se ha construido: ARREGLO entero, con su consejo de quitar.
    ok   ('el conocido MEDIDO sigue avisando de ARREGLO',
         r.outR.indexOf ('ARREGLO  T1') !== -1,
         'stdout: ' + r.outR.trim ());
    // El que NO: SIN MEDIR, y con la razon.
    ok   ('el conocido SIN MEDIR sale como SIN MEDIR',
         r.outR.indexOf ('SIN MEDIR  T2') !== -1,
         'stdout: ' + r.outR.trim ());
    // Y lo que NO PUEDE pasar: que del T2 diga que esta arreglado, y sobre todo
    // que le diga que borre su entrada. Esto es el fallo entero en una linea.
    ok   ('...y del que no se ha medido NO dice ARREGLO',
         r.outR.split ('ARREGLO').length - 1 === 1,
         'stdout: ' + r.outR.trim ());
    ok   ('...y NO le dice que borre su entrada del indice',
         r.outR.indexOf ('quita "T2"') === -1 &&
         r.outR.indexOf ('borra known/T2.txt') === -1,
         'stdout: ' + r.outR.trim ());
    // Y que el recuento lo diga: sin esto, el SIN MEDIR sale suelto y parece un
    // aviso mas.
    ok   ('...y el recuento dice cuantos se han quedado sin medir',
         /1 de los 2 conocidos no se han medido/.test (r.outR),
         'stdout: ' + r.outR.trim ());
}

// Y el caso de mas: SIN LISTA no se puede decir ARREGLO de NINGUNO. Es el
// principio de la seccion 4 aplicado al otro lado: lo que no se sabe no se
// anuncia como bueno. Aqui el log de rojos si se ha leido (esta vacio, que es
// cero rojos de verdad), lo que no se sabe es QUE SE HA EJECUTADO.
{
    const { fichero } = lista (['U1', 'U2'],
                               { U1: 'MIO: uno\n', U2: 'MIO: dos\n' });
    const rojos = path.join (TMP, 'rojos12b.log');
    fs.writeFileSync (rojos, '');

    for (const [nombre, extra] of [['sin pasar la lista', []],
                                   ['con una lista que no existe',
                                    [path.join (TMP, 'no-existe12.txt')]]]) {
        const r = corre (['arreglados', fichero, rojos, '', ''].concat (extra));
        ok   ('sin lista de no medidos (' + nombre + ') sale con 0', r.rc === 0,
             'rc: ' + r.rc + '\nstderr: ' + r.errR.trim ());
        ok   ('...y NO dice ARREGLO de nada',
             r.outR.indexOf ('ARREGLO') === -1,
             'stdout: ' + r.outR.trim ());
        ok   ('...y NO pide borrar nada',
             r.outR.indexOf ('quita "') === -1 &&
             r.outR.indexOf ('borra known/') === -1,
             'stdout: ' + r.outR.trim ());
        ok   ('...y lo dice como SIN MEDIR, que si informa',
             r.outR.indexOf ('SIN MEDIR') !== -1,
             'stdout: ' + r.outR.trim ());
    }
}

// Y EL CODIGO DE SALIDA. El rc 3 es "sin rojos pero con tests sin medir", y no
// puede ser 1 porque `--no-build` es legitimo ni 0 porque 0 con 35 sin medir es
// un 0 que no se ha ganado. Se comprueba en el texto de los dos gemelos: el
// caso de verdad (treinta y cinco binarios rancios y cero rojos) tarda tres
// minutos, y lo que importa aqui es que la regla no vuelva a depender solo de
// la cuenta de fallos.
{
    const sh  = fs.readFileSync (path.join (__dirname, 'verify_all.sh'),  'utf8');
    const bat = fs.readFileSync (path.join (__dirname, 'verify_all.bat'), 'utf8');

    // Los dos tienen que pasar la lista de no medidos al aviso de ARREGLO. Sin
    // esa llamada, `arreglados` no puede saber nada y degrada a SIN MEDIR todo,
    // que es este mismo fallo, visto por el otro lado.
    ok   ('el .sh pasa la lista de no medidos a `arreglados`',
          /arreglados[^\n]*"\$NO_MEDIDOS"/.test (sh),
          'linea: ' + (sh.split('\n').find (l => l.indexOf ('arreglados "$CONOCIDOJSON') !== -1) || '?'));
    ok   ('el .bat pasa la lista de no medidos a `arreglados`',
          /arreglados[^\n]*"%NOMEDIDOS%"/.test (bat),
          'linea: ' + (bat.split('\n').find (l => l.indexOf ('arreglados "%CONOCIDOJSON') !== -1) || '?'));

    // Y los dos tienen que salir con 3 cuando hay no medidos y ningun rojo.
    ok   ('el .sh sale con 3 si hay no medidos',
          /if \[ "\$n_no_medidos" -gt 0 \]; then exit 3; fi/.test (sh));
    // Y el 3 del .bat va con `goto` a una etiqueta FUERA del bloque, no con un
    // `exit /b` dentro. Medido con un .bat de nueve lineas: dentro de un
    // `if ... ( ... )`, `if !N! GTR 0 exit /b 3` sale con 0 y el `exit` de la
    // linea siguiente no se ejecuta nunca. Es el mismo problema del
    // `echo (!LOGB!)` del paso 1, y por eso se mira que el `exit /b 3` este
    // DESPUES del cierre del bloque y no dentro.
    const bat3 = bat.indexOf ('goto SIN_MEDIDOS_RC3');
    // El `exit /b 3` se busca DESPUES de la etiqueta, no en el fichero entero: el
    // comentario que explica el fallo lo escribe, y buscar la PRIMERA vez que
    // aparece daria el comentario y no el codigo.
    const bat3cuerpo = bat.indexOf ('exit /b 3', bat3);
    ok   ('el .bat sale con 3 si hay no medidos',
          bat3 !== -1 && bat3cuerpo !== -1 &&
          bat.slice (bat3, bat3cuerpo).indexOf (':SIN_MEDIDOS_RC3') !== -1,
          'goto: ' + bat3 + '  exit /b 3: ' + bat3cuerpo);
    ok   ('...y el 3 esta fuera del bloque del "sin fallos"',
          bat.indexOf ('if !N_NO_MEDIDOS! GTR 0 exit /b 3') === -1);

    // Y los dos tienen que contarlos, que sin la cuenta el 3 no se puede decidir.
    ok   ('el .sh cuenta los no medidos',
          /n_no_medidos="\$\(grep -c \. "\$NO_MEDIDOS"\)"/.test (sh));
    ok   ('el .bat cuenta los no medidos',
          /for \/f "usebackq delims=" %%L in \("%NOMEDIDOS%"\) do set \/a N_NO_MEDIDOS\+=1/.test (bat));

    // Y el TERCER caso, que no es ni 0 ni ">0": con `--only=2` el paso 1 no se
    // ejecuta, y entonces no se sabe NADA de lo que se ha medido. Decir "0 sin
    // medir" ahi seria mentira, asi que los dos empiezan en -1 ("no lo se") y
    // el paso 1 es el unico que puede cambiarlo. Sin esto, con `--only` el
    // informe afirmaria que todo lo ejecutado es de esta pasada, que es
    // justo lo contrario de lo que se puede saber.
    ok   ('el .sh empieza la cuenta en "no se sabe", no en 0',
          /n_no_medidos=-1/.test (sh) &&
          /if \[ -f "\$NO_MEDIDOS" \]/.test (sh));
    ok   ('el .bat empieza la cuenta en "no se sabe", no en 0',
          /set \/a N_NO_MEDIDOS=-1/.test (bat) &&
          /if not exist "%NOMEDIDOS%" exit \/b 0\r\nset \/a N_NO_MEDIDOS=0/.test (bat));
    // Y los dos tienen que SAYIRLO: una cuenta que nadie dice no informa de
    // nada. Es la linea que explica que con --only no se puede saber.
    ok   ('los dos dicen que con --only no se sabe cuantos se han medido',
          sh.indexOf ('Sin saber cuantos se han medido') !== -1 &&
          bat.indexOf ('Sin saber cuantos se han medido') !== -1);

    // Y la salida 3 esta DOCUMENTADA en los tres sitios donde se documentan las
    // otras: la cabecera del .sh, la cabecera del .bat y su --help. Una salida
    // sin documentar es una salida que nadie usa.
    ok   ('el .sh documenta la salida 3', /# Salidas:[\s\S]*?\b3 sin rojos/.test (sh));
    ok   ('el .bat documenta la salida 3', /REM Salidas:[\s\S]*?3 sin rojos/.test (bat));
    ok   ('el --help del .bat documenta la salida 3',
          /echo Salidas:[\s\S]*?3 sin rojos/.test (bat));

    // Y el check de gemelos NO puede romperse con esto. No mira el codigo de
    // salida de los verify (compara solo el resumen), asi que un 3 no lo hace
    // fallar: se comprueba porque es la unica razon por la que el 3 es seguro.
    const chk = fs.readFileSync (path.join (__dirname, 'verify_all_check.sh'), 'utf8');
    ok   ('el check de gemelos sigue sin mirar el codigo de salida del verify',
          /El codigo de salida NO se mira/.test (chk));
}

// ============================================================================
//  13. LA PANTALLA DEL AVISO, CON UNA ENTRADA REAL
// ============================================================================
seccion ('13. La pantalla del aviso, con una entrada real');

// POR QUE ESTA SECCION, Y POR QUE ESTA AL FINAL. Las doce anteriores comprueban
// QUE el aviso no se contradiga, con nombres de mentira (T1, T2) que no estan
// en ninguna bateria. Asi se prueba la regla sin depender de este proyecto, que
// es lo que toca en una prueba automatica. Pero el aviso no sale NUNCA de verdad:
// `Scripts/verify_all_known.json` tiene `entradas` vacio, asi que un SIN MEDIR de
// verdad no sale hasta que vuelva a haber un rojo. Y un aviso que no sale nunca
// no se puede revisar ni por la ortografia ni por la claridad: se escribe bien,
// pasa las comprobaciones, y el dia que aparece por primera vez nadie lo ha
// leido nunca.
//
// Aqui se monta el caso COMO SE VERIA EN LA CONSOLA: los nombres de verdad de
// esta bateria, un motivo escrito como se escribe de verdad (prefijo en la linea 1,
// texto partido, comentarios con `#`) y la pantalla entera sacada entre lineas,
// para leerla. Las comprobaciones son las minimas, y no por pereza: lo que se
// vigila aqui es que la linea exista, se lea y no pierda texto con el color. La
// ortografia se mira leyendo, que es lo unico que sirve.
//
// Y va al final a proposito: es la seccion que enseña la pantalla, y para leerla
// hace falta que las de arriba hayan pasado. Un fallo de verdad antes dejaria una
// pantalla vieja en medio del informe, que es peor que no imprimirla.

// La pantalla, tal cual. Un prefijo de dos caracteres y una barra para que se
// distinga de las comprobaciones, y el texto sin tocar: si aqui se limpiasen los
// saltos o se cambiase una palabra, la pantalla dejaria de ser la que sale.
function pantalla (titulo, texto) {
    const raya = '='.repeat(72);
    process.stdout.write ('\n' + raya + '\n  ' + titulo + '\n' + raya + '\n');
    const limpio = String (texto || '').replace (/\r/g, '');
    if (limpio.length === 0) { process.stdout.write ('  (nada)\n'); return ''; }
    for (const l of limpio.replace (/\n$/, '').split ('\n'))
        process.stdout.write ('  | ' + l + '\n');
    return limpio;
}

{
    // ── EL ESCENARIO, CON NOMBRES DE VERDAD ────────────────────────────────
    //
    // Tres tests de esta bateria: uno reconstruido en esta pasada y dos que no.
    // Los nombres salen de verdad del CTestTestfile del proyecto, no se inventan
    // aqui: el ejercicio consiste en que el aviso se lea IGUAL que se leeria con
    // la lista de verdad, y un nombre de dos letras (T1) deja fuera justo lo que
    // hay que mirar, que es si el nombre cabe y si el motivo entero se entiende.
    const MEDIDO = 'NEURONiK_DSPReferenceTest';
    const RANCIOS = 'NEURONiK_ModulationMatrixTest';
    const SIN_EXE = 'NEURONiK_ModulationDest17DriveTest';

    // El arbol de tests. El segundo tiene el .exe de una pasada anterior (el caso
    // del aviso) y el tercero no tiene .exe, que es el otro camino al mismo SIN
    // MEDIR: no es que su binario sea viejo, es que no hay binario.
    const dir = arbolTargets ([{ nombre: MEDIDO,     creaExe: true  },
                              { nombre: RANCIOS,    creaExe: true  },
                              { nombre: SIN_EXE,    creaExe: false }]);

    // Y la lista de conocidos, con los dos motivos REDACTADOS como se redactan:
    // el prefijo decide la clasificacion y va en la primera linea, el resto es
    // texto libre partido en lineas, y lo que es contexto va con `#`, que no sale
    // en el informe.
    const { fichero } = lista ([MEDIDO, RANCIOS, SIN_EXE], {
        [MEDIDO]: 'MIO: la referencia contra el modulo compartido difiere en tres\n' +
                  'banos de la curva: el otro hilo todavia no ha movido su parametro.\n\n' +
                  '# No es codigo roto: las dos mitades miden cosas distintas y el\n' +
                  '# otro hilo lo sabe. Se queda rojo hasta que elija una.\n',
        [RANCIOS]: 'ajeno: del modulo compartido a medias (otro hilo): la matriz aun\n' +
                   'no incluye el destino 17, que es suyo y se anadio el mes pasado.\n\n' +
                   '# El test revienta ANTES de que su codigo llegue a escribir nada,\n' +
                   '# asi que el motivo no puede ser "lo que dice el log": no dice nada.\n',
        [SIN_EXE]: 'MIO: el destino 17 todavia no tiene destino en la matriz: el .exe\n' +
                   'de este test no se ha compilado desde que se anadio.\n\n' +
                   '# El binario no es viejo: no hay. El paso 1 no lo compila, asi que\n' +
                   '# nadie puede saber si este rojo sigue existiendo.\n'
    });

    // El log de rojos VACIO: la bateria entera en verde. Es el caso dangerouso que
    // activa el aviso entero, porque sin rojos "no estar en la lista" parece
    // "haber pasado" y el destino son dos ficheros borrados.
    const rojos = path.join (TMP, 'rojos13.log');
    fs.writeFileSync (rojos, '');
    const medidos = path.join (TMP, 'nom_medidos13.txt');

    // ── LOS DOS AVISOS, EN ORDEN, COMO LOS LLAMA EL SCRIPT ────────────────
    //
    // Primero el de binarios rancios, que es quien deja la lista de lo que NO se
    // ha medido; despues el de ARREGLO, que la lee. El orden no es el del
    // informe del script (que va entre un ctest entero y otro), pero es el que
    // importa para entender la pantalla: el segundo aviso no puede decir ARREGLO
    // sin lo que escribio el primero.
    const rRuncios = corre (['rancios', dir, 'Release', '', '',
                            '--medidos=' + medidos, MEDIDO]);
    const rArreglados = corre (['arreglados', fichero, rojos, '', '', medidos]);

    pantalla ('PASO 1: el aviso de binarios rancios, que deja la lista de los no medidos',
              rRuncios.out);
    pantalla ('PASO 2 (ctest entero en verde) y el aviso de ARREGLO, que lee esa lista',
              rArreglados.out);
    pantalla ('LO QUE SE PIDE EN PANTALLA, tal cual, con los colores de verdad',
              corre (['arreglados', fichero, rojos,
                      String.fromCharCode (27) + '[33m',
                      String.fromCharCode (27) + '[0m',
                      medidos]).out);

    // ── LO QUE SE COMPRUEBA ───────────────────────────────────────────────
    //
    // Pocas, y todas de las que un fallo tiene que ser visible en la pantalla
    // de arriba. La logica esta en la seccion 12; aqui se vigila que con nombres
    // de verdad y un motivo de verdad el aviso siga saliendo entero.
    ok   ('los dos avisos salen con 0',
         rRuncios.rc === 0 && rArreglados.rc === 0,
         'rancios: ' + rRuncios.rc + '\narreglados: ' + rArreglados.rc +
         '\nstderr: ' + rArreglados.errR.trim ());
    ok   ('el conocido de verdad sin binario sale como SIN MEDIR',
         rArreglados.outR.indexOf ('SIN MEDIR  ' + RANCIOS) !== -1,
         'stdout: ' + rArreglados.outR.trim ());
    ok   ('...y el que no tiene ni .exe tambien, con su nombre',
         rArreglados.outR.indexOf ('SIN MEDIR  ' + SIN_EXE) !== -1,
         'stdout: ' + rArreglados.outR.trim ());
    ok   ('el reconstruido sale como ARREGLO, con los dos pasos para quitarlo',
         rArreglados.outR.indexOf ('ARREGLO  ' + MEDIDO) !== -1 &&
         rArreglados.outR.indexOf ('quita "' + MEDIDO + '"') !== -1 &&
         rArreglados.outR.indexOf ('borra known/' + MEDIDO + '.txt') !== -1,
         'stdout: ' + rArreglados.outR.trim ());
    // Lo que NO PUEDE salir, con el nombre de verdad: el consejo de borrar los
    // dos motivos de los que no se ha medido nada. Es el fallo entero, aqui con
    // los nombres por los que se llega a ejecutar el paso.
    ok   ('...y NO pide borrar ninguno de los dos que no se han medido',
         rArreglados.outR.indexOf ('quita "' + RANCIOS + '"') === -1 &&
         rArreglados.outR.indexOf ('borra known/' + RANCIOS + '.txt') === -1 &&
         rArreglados.outR.indexOf ('quita "' + SIN_EXE + '"') === -1 &&
         rArreglados.outR.indexOf ('borra known/' + SIN_EXE + '.txt') === -1,
         'stdout: ' + rArreglados.outR.trim ());
    ok   ('...y el recuento dice los dos',
         /2 de los 3 conocidos no se han medido/.test (rArreglados.outR),
         'stdout: ' + rArreglados.outR.trim ());
    // Y que el motivo de verdad llegue entero al que lo lee. `arreglados` no
    // imprime los motivos (eso lo hace el informe, que es otra pantalla y ya
    // esta probada en la seccion 1), asi que aqui se mira por la orden que los
    // carga: aplanado en una linea, y sin los comentarios, que si se colaran
    // ensuciarian la linea del rojo en el informe de verdad.
    const motivo = corre (['motivo', fichero, RANCIOS]);
    ok   ('el motivo de verdad llega entero y en una linea',
         motivo.out.indexOf ('el destino 17, que es suyo') !== -1 &&
         motivo.out.indexOf ('\n') === -1,
         'stdout: ' + JSON.stringify (motivo.out));
    ok   ('...y sin los comentarios de contexto',
         motivo.out.indexOf ('#') === -1 &&
         motivo.out.indexOf ('no dice nada') === -1,
         'stdout: ' + JSON.stringify (motivo.out));
    // El color no puede cambiar el texto. Es la unica comprobacion de esta
    // seccion que no es de la regla: si el color se comiera parte de una linea
    // al pintarla, el aviso en la consola de verdad estaria corrotto y las once
    // comprobaciones de arriba (que van sin color) no lo verian.
    const conColor = corre (['arreglados', fichero, rojos,
                             String.fromCharCode (27) + '[33m',
                             String.fromCharCode (27) + '[0m', medidos]);
    const limpio = (s) => s.replace (new RegExp (String.fromCharCode (27) + '\\[[0-9;]*m', 'g'), '');
    ok   ('con color el texto es el MISMO que sin color',
         limpio (conColor.outR) === rArreglados.outR,
         'con color: ' + JSON.stringify (limpio (conColor.outR)) +
         '\nsin color: ' + JSON.stringify (rArreglados.outR));

    // ── Y EL POR QUE DE CADA UNO, QUE NO ES EL MISMO ──────────────────────
    //
    // El aviso entero, arriba, tiene los dos motivos ya: los escribe quien
    // ha mirado el .exe. Aqui se mira el FICHERO, que es donde viven, y se
    // comprueba que sean distintos entre si. La comparacion es por motivos, no
    // por nombres: si dos motivos distintos salieran iguales, esta comprobacion
    // cae; si un nombre cambiara y su motivo se quedara, tambien.
    const lines = noMedidos (medidos);
    const porNombre = new Map (lines.map (e => [e.nombre, e.porque]));
    ok   ('el fichero lleva los dos no medidos, con nombre y motivo, y NO el reconstruido',
         lines.length === 2 && porNombre.has (MEDIDO) === false &&
         porNombre.has (RANCIOS) && porNombre.has (SIN_EXE),
         'escrito: ' + JSON.stringify (lines));

    // El de SIN BINARIO, que es el que se ha arreglado aqui. Decia "su .exe no se
    // ha compilado en esta pasada", que es cierto y no es lo que hay que hacer:
    // ese test no se ha ejecutado nunca, no tiene un .exe viejo que recompilar, y
    // su rojo no desaparecera recompilando su target: hay que arreglar por que no
    // se construyo. Un texto generico aqui manda a la persona al sitio
    // equivocado con seguridad, que es peor que no decir nada.
    ok   ('el que NO tiene .exe no dice que su .exe este viejo',
         porNombre.get (SIN_EXE).indexOf ('no tiene .exe') !== -1 &&
         porNombre.get (SIN_EXE).indexOf ('no se ha ejecutado nunca') !== -1,
         'motivo: ' + JSON.stringify (porNombre.get (SIN_EXE)));
    // Y el de --no-build, que se parece a proposito al de arriba para que se
    // confundan si alguien los lee por encima, pero dice otra cosa.
    ok   ('el que tiene el .exe de antes dice que no se ha compilado en esta pasada',
         porNombre.get (RANCIOS).indexOf ('no se ha compilado en esta pasada') !== -1,
         'motivo: ' + JSON.stringify (porNombre.get (RANCIOS)));
    // Y los dos motivos tienen que ser DISTINTOS entre si. Sin esto, los dos de
    // arriba pueden estar bien escritos y aun asi ser el mismo texto, que es el
    // fallo que esto arregla.
    const unicos = new Set ([...porNombre.values ()]);
    ok   ('...y los dos motivos son distintos entre si',
         porNombre.size === 2 && unicos.size === 2,
         'motivos: ' + JSON.stringify ([...unicos]));
}

// ============================================================================
//  14. LOS TRES PORQUES, EN PANTALLA
// ============================================================================
seccion ('14. Los tres por que, uno por caso');

// EL QUE SE ENSEÑA, JUNTO. Los tres motivos en tres pantallas seguidas, que es
// como se leen: uno al lado de otro se ve que son distintos y uno debajo de otro
// se ven menos. La segunda pasada es con `--sin-build`, que es el caso limite
// (no se ha compilado nada) y el que mas se confunde con el primero.
//
// Y aqui se imprime la lista que deja `rancios`, que es donde estan los motivos:
// la pantalla del aviso los enseña ya, y verlo aqui es ver de donde salen.
{
    const MEDIDO   = 'NEURONiK_DSPReferenceTest';
    const RANCIOS  = 'NEURONiK_ModulationMatrixTest';
    const SIN_EXE  = 'NEURONiK_ModulationDest17DriveTest';

    const dir = arbolTargets ([{ nombre: MEDIDO,   creaExe: true  },
                              { nombre: RANCIOS,  creaExe: true  },
                              { nombre: SIN_EXE,  creaExe: false }]);
    const { fichero } = lista ([MEDIDO, RANCIOS, SIN_EXE], {
        [MEDIDO]:  'MIO: uno\n', [RANCIOS]: 'MIO: dos\n', [SIN_EXE]: 'MIO: tres\n'
    });
    const rojos = path.join (TMP, 'rojos14.log');
    fs.writeFileSync (rojos, '');

    for (const [rotulo, extra] of [['PASA 1, CON BUILD (lo normal): uno reconstruido, uno con el .exe de antes y uno sin .exe',
                                    [MEDIDO]],
                                   ['PASA 1, CON --no-build (el caso limite): no se ha compilado nada',
                                    ['--sin-build']]]) {
        const medidos = path.join (TMP, 'nom_medidos14_' + extra.join ('_').replace (/\W/g, '') + '.txt');
        const rRancios = corre (['rancios', dir, 'Release', '', '',
                                 '--medidos=' + medidos].concat (extra));
        const rArreglados = corre (['arreglados', fichero, rojos, '', '', medidos]);
        pantalla (rotulo, rRancios.out + rArreglados.out);
        ok   (rotulo + ': los dos avisos salen con 0',
             rRancios.rc === 0 && rArreglados.rc === 0,
             'rancios: ' + rRancios.rc + '\narreglados: ' + rArreglados.rc +
             '\nstderr: ' + rArreglados.errR.trim ());
    }

    // Y LO QUE NO PUEDE PASAR, con el motivo ya en pantalla: que los dos casos
    // que no se han medido por motivos DISTINTOS digan lo mismo. Con un solo
    // texto, el que no tiene .exe suena a "recompila", y quien lo lea recompila
    // un target que no existe y vuelve a ver el mismo rojo, tres dias mas.
    const medidos = path.join (TMP, 'nom_medidos14_final.txt');
    corre (['rancios', dir, 'Release', '', '', '--medidos=' + medidos, MEDIDO]);
    const r = corre (['arreglados', fichero, rojos, '', '', medidos]);
    // El bloque de un SIN MEDIR son TRES lineas: el nombre, el motivo y el
    // "no hay que quitarlo". Se cogen las tres: si se cogiera solo la segunda,
    // la comprobacion de "no hay que borrar nada" miraria una linea donde ese
    // texto no esta nunca, y pasaria siempre. Un aviso al que se mira una linea
    // de cada tres se parece a un aviso que no dice nada.
    const bloqueDe = (nombre) => {
        const ls = r.outR.split ('\n');
        const i = ls.findIndex (l => l.indexOf ('SIN MEDIR  ' + nombre) !== -1);
        if (i < 0) return [];
        return ls.slice (i, i + 3).map (l => l.trim ());
    };
    const motivoDe = (nombre) => { const b = bloqueDe (nombre); return b.length > 1 ? b[1] : ''; };
    ok   ('los dos SIN MEDIR dicen motivos DISTINTOS',
         motivoDe (RANCIOS) !== '' && motivoDe (SIN_EXE) !== '' &&
         motivoDe (RANCIOS) !== motivoDe (SIN_EXE),
         'rancios: ' + JSON.stringify (motivoDe (RANCIOS)) +
         '\nsin .exe: ' + JSON.stringify (motivoDe (SIN_EXE)));
    // Y los dos bloques enteros tienen que decir lo mismo al final: el motivo
    // cambia, la consecuencia NO. Un motivo distinto con una consecuencia
    // distinta seria dos reglas, y la regla es una: no se borra nada.
    ok   ('...y los dos siguen diciendo que NO hay que borrar nada',
         bloqueDe (RANCIOS).join (' ').indexOf ('NO hay que quitarlo') !== -1 &&
         bloqueDe (SIN_EXE).join (' ').indexOf ('NO hay que quitarlo') !== -1,
         'rancios: ' + JSON.stringify (bloqueDe (RANCIOS)) +
         '\nsin .exe: ' + JSON.stringify (bloqueDe (SIN_EXE)));
    // Y el texto generico solo cuando NO HAY motivo, que es cuando no hay nadie
    // que lo sepa. Es el caso de una lista hecha a mano, y no de un binario.
    const aMano = path.join (TMP, 'nom_medidos14_mano.txt');
    fs.writeFileSync (aMano, RANCIOS + '\n');
    const sinMotivo = corre (['arreglados', fichero, rojos, '', '', aMano]);
    ok   ('una lista sin motivo (hecha a mano) avisa igual, con el texto generico',
         sinMotivo.outR.indexOf ('SIN MEDIR  ' + RANCIOS) !== -1 &&
         sinMotivo.outR.indexOf ('no se sabe si falla') !== -1,
         'stdout: ' + sinMotivo.outR.trim ());

    // ── LOS DOS GEMELOS SEGUEN VIENDO LAS MISMAS LINEAS ─────────────────────
    //
    // Anadir el tabulador al formato es el cambio que puede romper el codigo 3 en
    // silencio, y el modo de romperse es sutil: si un gemelo se quedara contando
    // una sola linea (o ninguna), el 3 saldria con cualquier cuenta que no sea la
    // de verdad, y la linea del informe que la cuenta mentiria al que lee. Es el
    // fallo mas probable de este cambio y el mas dificil de ver sin medirlo.
    //
    // Se cuenta cada uno como lo cuenta cada uno: el .sh con `grep -c .` y el .bat
    // con su `for /f`. El esperado NO es un numero escrito aqui sino la cuenta
    // real de lineas del fichero, porque un numero fijo se queda viejo en cuanto
    // el escenario cambia y pasa a comprimir un caso que ya no existe.
    const CRLF = String.fromCharCode (13) + String.fromCharCode (10);
    const lineasReales = fs.readFileSync (medidos, 'utf8')
                             .replace (/\r/g, '').split ('\n').filter (s => s).length;
    const esperado = String (lineasReales);

    const enShell = cp.spawnSync ('bash', ['-c', 'grep -c . "$1"', 'sh', medidos],
                                   { encoding: 'utf8' });
    const cuentaSh = String (enShell.stdout || '').trim ();
    ok   ('el .sh cuenta con grep las lineas con tabulador',
         cuentaSh === esperado,
         'esperado: ' + esperado + '\nobtenido: ' + JSON.stringify (cuentaSh) +
         '\nstderr: ' + String (enShell.stderr || '').trim ());

    const batConta = path.join (TMP, 'cuenta14.bat');
    // El .bat con su `for /f` de verdad, y con la ruta de la lista. La ruta se
    // pasa con barras de windows porque es lo que le llega al .bat, y por eso
    // se escribe con chr(92) en vez de una barra invertida en el codigo: en un
    // fichero de este proyecto una barra suelta es un escape silencioso.
    fs.writeFileSync (batConta, [
        '@echo off',
        'setlocal EnableDelayedExpansion',
        'set "F=' + medidos.split ('/').join (String.fromCharCode (92)) + '"',
        'set /a N=0',
        'for /f "usebackq delims=" %%L in ("%F%") do set /a N+=1',
        'echo !N!',
        'exit /b 0',
        ''
    ].join (CRLF), 'binary');
    const enBat = cp.spawnSync (process.env.ComSpec || 'cmd.exe', ['/c', batConta],
                                { encoding: 'utf8' });
    const cuentaBat = String (enBat.stdout || '').trim ();
    ok   ('el .bat cuenta con su for /f las mismas lineas',
         cuentaBat === esperado,
         'esperado: ' + esperado + '\nobtenido: ' + JSON.stringify (cuentaBat) +
         '\nstderr: ' + String (enBat.stderr || '').trim ());
    // Y que los dos den la MISMA cuenta, que es lo que importa: el check de
    // gemelos compara el informe entero, y si estos dos no coinciden el 3 sale
    // distinto en cada script.
    ok   ('los dos gemelos cuentan lo mismo',
         cuentaSh === cuentaBat && cuentaSh === esperado,
         'sh: ' + JSON.stringify (cuentaSh) + '  bat: ' + JSON.stringify (cuentaBat) +
         '  lineas: ' + esperado);
    // Y el motivo no se ha colado en la cuenta como una linea mas. Con el
    // tabulador de separador no puede pasar, pero es justo el fallo que daria si
    // alguien cambiara el separador por un salto de linea "por legibilidad": el
    // recuento diria que hay mas tests sin medir de los que hay.
    ok   ('las lineas del fichero siguen siendo las lineas de tests, con un tabulador cada una',
         fs.readFileSync (medidos, 'utf8').replace (/\r/g, '').split ('\n')
           .filter (s => s).every (l => l.indexOf (String.fromCharCode (9)) !== -1),
         'fichero: ' + JSON.stringify (fs.readFileSync (medidos, 'utf8')));
}

// ============================================================================
//  15. EL ROJO INTERMITENTE FRENTE A LA DIVERGENCIA DE VERDAD
// ============================================================================
seccion ('15. El rojo intermitente no es una divergencia');

// El caso que motive todo esto esta MEDIDO (2026-10-02, en HANDOFF.md): el .sh
// corrio con NEURONiK_WebUiLocalModeE2e en rojo y el .bat en verde de la misma
// corrida, con NEURONiK_WorkletSync en rojo en LOS DOS. El check salio con 1
// diciendo que una regla se habia tocado en un gemelo, y no habia ninguna: lo
// que habia era un test que va a veces (8 relanzamientos, 6 verdes y 2 rojos).
//
// Se monta con la orden `resumen` de verdad y los rojos de verdad, no con dos
// ficheros escritos a mano: asi lo que se mide es la cadena entera (el `resumen`
// que escriben los dos gemelos y la `compara` que los juzga), y no una copia
// de como deberia ser.
const E2E = 'NEURONiK_WebUiLocalModeE2e';
const SYNC = 'NEURONiK_WorkletSync';
const SINCLAS = 'SIN CLASIFICAR: no esta en la lista de conocidos de este script';

{
    const d = fs.mkdtempSync (path.join (TMP, 'r15'));
    const lentos = path.join (d, 'lentos.txt');
    fs.writeFileSync (lentos, '');
    const rojosSh = path.join (d, 'rojos_sh.txt');
    const rojosBat = path.join (d, 'rojos_bat.txt');
    fs.writeFileSync (rojosSh,  '2\t' + SYNC + '\t' + SINCLAS + '\n' +
                            '2\t' + E2E  + '\t' + SINCLAS + '\n');
    fs.writeFileSync (rojosBat, '2\t' + SYNC + '\t' + SINCLAS + '\n');
    const rSh = path.join (d, 'res_sh.txt');
    const rBat = path.join (d, 'res_bat.txt');
    corre (['resumen', rojosSh,  rSh,  '0', '53', lentos, '0', '5', '0']);
    corre (['resumen', rojosBat, rBat, '0', '53', lentos, '0', '5', '0']);

    const c = corre (['compara', rSh, rBat, 'el .sh', 'el .bat']);
    const texto = pantalla ('EL CASO REAL: WorkletSync en los dos, E2e solo en el .sh', c.out);

    igual ('sale con 3, no con 1', c.rc, 3);
    ok   ('el texto nombra el test que se ha movido',
         texto.indexOf (E2E) !== -1, 'salida: ' + texto);
    ok   ('...y NO nombra el que los dos han visto (eso no se ha movido)',
         texto.indexOf (SYNC) === -1, 'salida: ' + texto);
    ok   ('...y dice de que lado lo ha visto',
         texto.indexOf ('Solo en el .sh') !== -1, 'salida: ' + texto);
    ok   ('...y no suelta el mensaje de divergencia',
         texto.indexOf ('NO DICEN LO MISMO') === -1 &&
         texto.indexOf ('tocado en un gemelo') === -1, 'salida: ' + texto);
    ok   ('...y dice que se puede relanzar ese test',
         texto.indexOf ('relanza') !== -1, 'salida: ' + texto);
    // Con --estricto el mismo caso es una divergencia, y sale con 1. Sin esta
    // comprobacion, quitar `--estricto` del codigo no se notaria: seguiria
    // saliendo 3, que es justo el fallo que se quiere poder evitar.
    igual ('con --estricto el mismo caso sale con 1',
           corre (['compara', rSh, rBat, 'el .sh', 'el .bat', '--estricto']).rc, 1);
    // Y el caso de verdad del lado CONTRARIO: el rojo sobrante en el .bat. La
    // regla es simetrica, y una regla que solo funciona en un sentido es una
    // regla que sale con 3 cuando no deberia.
    igual ('tambien en el otro lado (solo en el .bat sale con 3)',
           corre (['compara', rBat, rSh, 'el .sh', 'el .bat']).rc, 3);
}

// Y lo que NO es un intermitente. Cada uno de estos salia con 1 antes de este
// cambio y tiene que seguir saliendo con 1: si alguno pasara a 3, el check
// habria perdido el diente, que es justo el gasto que hace el `--estricto`.
//
// El cuerpo de base es un resumen real: 53 tests, un rojo SIN CLASIFICAR, y sin
// lentos. Los recuentos van ESCRITOS a mano en cada caso, no calculados: un
// generador que hiciera la cuenta por nosotros comprobaria que la cuenta cuadra
// con la cuenta que queremos, y no que el codigo sabecjuzgar una que no cuadra.
const BASE = 'v\t1\npasos\t5\nconocidos\t0\ntests\t53\n' +
             'fallos\t1\nmios\t0\najenos\t0\nsinClasificar\t1\n' +
             'lentos\t0\ntocoTimeout\t0\nhuerfanos\t0\n' +
             'rojo\t2\t' + SYNC + '\tSIN CLASIFICAR\n';
const ROJO_E2E = 'rojo\t2\t' + E2E + '\tSIN CLASIFICAR\n';
// El intermitente que SI cuadra: dos rojos en un lado, los recuentos suben 1.
const CON_E2E = BASE.replace ('fallos\t1', 'fallos\t2')
                    .replace ('sinClasificar\t1', 'sinClasificar\t2') + ROJO_E2E;

{
    const d = fs.mkdtempSync (path.join (TMP, 'r15b'));
    const par = (nombre, texto) => {
        const f = path.join (d, nombre);
        fs.writeFileSync (f, texto);
        return f;
    };
    const ref = par ('ref.txt', BASE);
    const compara = (nombre, otro) =>
        corre (['compara', ref, par (nombre + '.txt', otro), 'el .sh', 'el .bat']);

    igual ('el caso que cuadra sale con 3', compara ('ok', CON_E2E).rc, 3);

    // (a) UN LENTO que solo ve uno de los dos. El umbral de lento es una REGLA, y
    // este script existe para cazar reglas distintas entre gemelos: si se
    // perdonara, cambiar el umbral en un solo script pasaria desapercibido.
    //
    // MEDIDO, y hay que decirlo porque no es lo que parece: quitar la regla que
    // prohibe los lentos NO hace caer NINGUNA de estas dos comprobaciones. Lo
    // que las hace caer es la aritmetica de los recuentos: un `lento` no suma en
    // `fallos`, asi que las lineas que sobran nunca cuadran. Las dos reglas (los
    // dos lados, y solo-rojo) se han medido y son redundantes con esa cuenta; se
    // quedan en el codigo porque dicen en voz alta lo que se supone. Estas
    // comprobaciones, entonces, fijan el RESULTADO (un lento no es un
    // intermitente), no la regla que lo produce.
    const conLento = BASE.replace ('lentos\t0', 'lentos\t1') +
                     'lento\tNEURONiK_Lento\n';
    const rLento = compara ('lento', conLento);
    igual ('un lento que solo ve uno sale con 1', rLento.rc, 1);
    ok   ('...y el texto dice que el umbral de lento NO se perdona',
         rLento.outR.indexOf ('LENTOS') !== -1 && rLento.outR.indexOf ('no se perdona') !== -1,
         'salida: ' + rLento.outR);
    // Y el caso que mas se parece al de verdad: un rojo intermitente PERFECTO
    // (los recuentos cuadran con la linea que sobra) Y ADEMAS un lento que solo
    // ve uno. El rojo se perdona; el lento no. Si aqui saliera 3, el lento
    // estaria pasando por la puerta del rojo, que es justo el agujero.
    const rojoYLento = CON_E2E.replace ('lentos\t0', 'lentos\t1') +
                       'lento\tNEURONiK_Lento\n';
    const rMezcla = compara ('mezcla', rojoYLento);
    igual ('un rojo intermitente con un lento de mas sale con 1', rMezcla.rc, 1);
    ok   ('...y el texto avisa de los lentos igualmente',
         rMezcla.outR.indexOf ('LENTOS') !== -1, 'salida: ' + rMezcla.outR);

    // (b) Lineas que sobran en LOS DOS lados. Aqui no hay "el que fallo mas":
    // hay dos scripts que cuentan distinto. El caso mas feo de este es un MISMO
    // test rojo en los dos con distinta clasificacion, y tambien sale con 1.
    // (Igual que en (a): lo que lo rechaza es la cuenta de `fallos`, no la regla
    // de "un solo lado", que se ha medido redundante.)
    const dosLados = CON_E2E + 'rojo\t2\tNEURONiK_Otro\tMIO\n';
    igual ('lineas que sobran en los dos lados salen con 1', compara ('dos', dosLados).rc, 1);
    const reclasificado = CON_E2E.replace (SYNC + '\tSIN CLASIFICAR', SYNC + '\tMIO')
                                .replace ('mios\t0', 'mios\t1')
                                .replace ('sinClasificar\t2', 'sinClasificar\t1');
    igual ('el mismo rojo con otra clasificacion sale con 1',
           compara ('reclas', reclasificado).rc, 1);

    // (c) Las cuentas NO cuadran con las lineas que sobran. Este es el que
    // importa: si el codigo perdonara cualquier diferencia de rojos, bastaria
    // con que un gemelo contara mal para que el check saliera con 3 y el
    // ERROR PASARA POR ALTO. Por eso el intermitente tiene que ser una prueba
    // aritmetica y no una lista de nombres.
    const fallosMal = CON_E2E.replace ('fallos\t2', 'fallos\t3');
    igual ('las lineas sobrantes con un `fallos` que no cuadra salen con 1',
           compara ('fallos', fallosMal).rc, 1);
    const miosMal = CON_E2E.replace ('mios\t0', 'mios\t1');
    igual ('un `mios` que no sube lo que sube la linea MIO sale con 1',
           compara ('mios', miosMal).rc, 1);
    const sinClasMal = CON_E2E.replace ('sinClasificar\t2', 'sinClasificar\t1');
    igual ('un `sinClasificar` que no cuadra sale con 1',
           compara ('sinc', sinClasMal).rc, 1);

    // (d) Un recuento que NO cuenta rojos se ha movido tambien. Con un rojo
    // intermitente de por medio, eso es otra causa, y el texto lo dice por su
    // cuenta para que no haya que adivinarlo.
    const timeoutMal = CON_E2E.replace ('tocoTimeout\t0', 'tocoTimeout\t1');
    const rTimeout = compara ('timeout', timeoutMal);
    igual ('un tocoTimeout que se ha movido sale con 1', rTimeout.rc, 1);
    ok   ('...y el texto nombra los recuentos que no cuentan rojos',
         rTimeout.outR.indexOf ('tocoTimeout') !== -1, 'salida: ' + rTimeout.outR);
    const huerfanosMal = CON_E2E.replace ('huerfanos\t0', 'huerfanos\t3');
    igual ('unos huerfanos distintos salen con 1', compara ('huerf', huerfanosMal).rc, 1);

    // (e) Y los que ya estaban: iguales, distintos de version, ilegible. No es
    // que aqui se este tocando nada, es que el 3 nuevo no puede haber movido
    // de sitio un 0, un 1 ni un 2.
    igual ('dos resumenes iguales siguen dando 0', compara ('igual', BASE).rc, 0);
    igual ('versiones distintas siguen dando 1',
           corre (['compara', ref, par ('v.txt', BASE.replace ('v\t1', 'v\t2')), 'A', 'B']).rc, 1);
    igual ('un resumen ilegible sigue dando 2',
           corre (['compara', ref, path.join (d, 'no-existe.txt'), 'A', 'B']).rc, 2);
}

// ============================================================================
//  EL RESULTADO
// ============================================================================
process.stdout.write ('\n' + '-'.repeat(72) + '\n');
if (fallos === 0) {
    process.stdout.write ('  ' + hechos + ' comprobaciones, todas en verde.\n\n');
    process.exit (0);
}
process.stdout.write ('  ' + fallos + ' de ' + hechos + ' comprobaciones han FALLADO.\n\n');
process.exit (1);
