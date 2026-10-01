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
{
    const { fichero } = lista (['H'], { H: 'MIO: h\n' });
    const vacio = path.join(TMP, 'vacio.log');
    fs.writeFileSync(vacio, '');
    const r = corre (['arreglados', fichero, vacio, '', '']);
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
    const r = corre (['arreglados', fichero, log, '', '']);
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
//  6. LAS DEMAS ORDENES
// ============================================================================
seccion ('6. Las demas ordenes');

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
//  EL RESULTADO
// ============================================================================
process.stdout.write ('\n' + '-'.repeat(72) + '\n');
if (fallos === 0) {
    process.stdout.write ('  ' + hechos + ' comprobaciones, todas en verde.\n\n');
    process.exit (0);
}
process.stdout.write ('  ' + fallos + ' de ' + hechos + ' comprobaciones han FALLADO.\n\n');
process.exit (1);
