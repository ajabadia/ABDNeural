#!/usr/bin/env bash
#
# verify_all.sh -- LOS CINCO PASOS DE LA VERIFICACION, en un comando y con un
# unico codigo de salida.
#
#   1. build de ABDNeural (los targets que existen, no "todo")
#   2. ctest -C Release
#   3. vitest de la WebUI
#   4. vitest de ABDSharedAssets
#   5. los contratos cruzados entre los dos repositorios
#
# ── POR QUE ESTE SCRIPT DICE EN ROJO Y NO "TODO BIEN" ──────────────────────
#
# Hoy la bateria tiene rojos que NO son de este trabajo: un hilo dejo el modulo
# compartido a medias (`shelf`, `Phaser4`, `envelopeCurve`), y hay tests que
# expectan un catalogo de 6 motores y 3 ids reservados cuando ya hay 8 y 0.
#
# La tentacion con un verify es "separar los rojos conocidos y salir en verde".
# Eso es una trampa, y la trampa es esta: en cuanto un verify sale en verde con
# 9 rojos dentro, toda la gente aprende a mirar el codigo de salida sin mirar la
# salida. Y el dia que aparezca un rojo NUEVO, se confunde con el ruido de
# siempre y nadie lo ve. Un verify que dice "ok" con lo que hay dentro es peor
# que no tener verify, porque ocupa el sitio del que avisaria.
#
# Asi que: sale en rojo si hay CUALQUIER fallo, y al final lista cada uno con su
# motivo, separando "este es mio" de "este es del otro trabajo". Que la
# separacion exista no significa que se pueda ignorar el rojo.
#
# ── USO ────────────────────────────────────────────────────────────────────
#
#   ./Scripts/verify_all.sh              # los cinco pasos
#   ./Scripts/verify_all.sh --no-build   # salta el paso 1 (build ya hecho)
#   ./Scripts/verify_all.sh --only=3     # un solo paso, para depurar
#
# Salidas: 0 todo en verde; 1 algun paso con fallos; 2 uso incorrecto.
#
# Variables de entorno (las dos las leen tambien verify_all.bat):
#   VERIFY_TIMEOUT  segundos que un test puede tardar antes de que ctest lo mate
#                   (600 por defecto). Sube el TECHO: no cambia los avisos.
#   VERIFY_LENTO    segundos a partir de los cuales un test que pasa se avisa
#                   por lento (60 por defecto).
#
# `verify_all.bat` hace EXACTAMENTE lo mismo en Windows, con los mismos codigos.

set -u

# ── DONDE ESTAMOS ──────────────────────────────────────────────────────────
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ASSETS="$(cd "$RAIZ/.." && pwd)/ABDSharedAssets"
BUILD="$RAIZ/build-reference"
CONFIG="Release"
LOCK="$RAIZ/Scripts/.verify_all.lock"

# ── EL FICHERO DE CONOCIDOS, Y POR QUE SE BUSCA ASI ────────────────────────
#
# El mismo fichero que lee el .bat, y por la MISMA regla: el que esta junto al
# script. Antes el .sh lo buscaba en "$RAIZ/Scripts/verify_all_known.json" y el
# .bat en "%~dp0", que son el mismo sitio mientras el .sh viva en Scripts/.
# En cuanto uno de los dos se copia, se symlinka, o se ejecuta desde otro arbol,
# cada uno se lee SU lista, y los dos scripts empiezan a dar numeros distintos
# sobre el mismo build sin que nada lo delate. Dos rutas para un fichero es una
# lista de mas escondida.
DIRSCRIPT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONOCIDOJSON="$DIRSCRIPT/verify_all_known.json"

# ── EL RESUMEN COMPARABLE, Y SUS TRES TEMPORALES ─────────────────────
#
# Lo que este script DICE, en una forma que se pueda comparar con el .bat
# linea a linea. No se imprimia: se escribia en un fichero que nadie leia.
# Con el, verify_all_check.sh lanza los dos gemelos y pregunta si dicen
# lo mismo, que es la pregunta que mas veces ha salido mal sobre este repo.
#
# La ruta se puede dar por fuera con VERIFY_RESUMEN. Lo necesita el check:
# los dos gemelos escriben el suyo en ficheros DISTINTOS, porque si
# escribieran en el mismo, el segundo pisaria al primero y compararia un
# fichero consigo mismo, que sale siempre bien.
#
# Y por defecto cada uno en el suyo, con el nombre del gemelo dentro, en el
# directorio de temporales de ctest y NO junto al script: un fichero generado
# en la carpeta Scripts/ aparece en `git status` al lado del codigo, y un
# verify que ensucia el repositorio teaches a meter sus salidas en el
# .gitignore, que es donde acaban las que no deberian estar a la vista.
RESUMEN="${VERIFY_RESUMEN:-$BUILD/Testing/Temporary/verify_all_resumen.sh.txt}"

# El volcado de `fallos[]`, y la lista de nombres de los lentos. Los dos
# los lee verify_all_node.js, que es quien les aplica el mismo formato a
# los dos gemelos. Van en ficheros y no en argumentos porque un motivo de
# fallo puede llevar tabuladores y saltos de linea dentro, y un motivo con
# un tabulador dentro partido en campos se convierte en dos rojos.
RES_ENTRADAS="${TMPDIR:-/tmp}/verify_entradas.$$"
RES_LENTOS="${TMPDIR:-/tmp}/verify_lentos.$$"

# ── SALIDA A COLOR, PERO SOLO SI LA PANTALLA LA AGUANTA ────────────────────
if [ -t 1 ] && [ "${NO_COLOR:-}" = "" ]; then
    R=$'\033[31m'; V=$'\033[32m'; A=$'\033[33m'; T=$'\033[36m'; N=$'\033[0m'
else
    R=''; V=''; A=''; T=''; N=''
fi



# El PROGRAMA COMPARTIDO: la misma logica que usa el .bat (vivo, limpieza de
# procesos huerfanos, lista de conocidos) esta en verify_all_node.js, al lado de
# este script. Antes cada uno llevaba dentro su copia. En el .sh no habia
# problema para escribir el programa (el `!` y el `"` solo muerden en batch), pero
# la copia seguia siendo una lista mas: si una se toca y la otra no, los dos
# scripts dan distinta cuenta del mismo lock sin que nada lo delate.
#
# Y si el fichero falta, el script PARA. Sin el no se puede ni comprobar si el
# lock esta vivo (y entonces cualquier verify se pisa el build del otro) ni
# clasificar un rojo: un verify sin clasificador sale en verde con lo que tenga
# delante.
NODE_LIB="$DIRSCRIPT/verify_all_node.js"
if [ ! -f "$NODE_LIB" ]; then
    printf '%sNo esta %s, y sin el este script no puede verificar nada.%s\n' "$R" "$NODE_LIB" "$N" >&2
    printf '  Esta junto a este script, en la carpeta Scripts.\n' >&2
    exit 2
fi

# ── LOS FALLOS CONOCIDOS, CON SU MOTIVO ────────────────────────────────────
#
# No es una lista de "estos no cuentan". Es una lista de POR QUE son rojos, para
# que quien lo lea pueda decidir si le affects. Un rojo sin motivo es un rojo que
# hay que silenciar; un rojo con motivo es uno que se puede ignorar a proposito.
#
# La clave de cada entrada es el nombre del test. El valor es una linea que dice
# de quien es. Un test que este aqui y aun asi pase no molesta: se avisa igual
# de que la lista esta vieja, que es informacion buena.
#
# Las claves que empiezan por guion bajo son comentarios, no entradas: asi el
# JSON se puede documentar sin que el lector tenga que adivinar. Y por eso el
# valor de un comentario puede ser una lista de lineas y el de una entrada
# tiene que ser TEXTO: una entrada con un array debajo se classificaria con un
# motivo que es "a,b,c". La comprobacion de abajo avisa de eso en vez de
# clasificarlo en silencio.
leer_conocidos() {
    local dump

    if [ ! -f "$CONOCIDOJSON" ]; then
        printf '  %sAviso: no esta %s%s\n' "$A" "$CONOCIDOJSON" "$N"
        printf '          sin el, los rojos de ctest salen SIN CLASIFICAR\n'
        return 0
    fi

    # La carga, la validacion y el reparto en pares los hace verify_all_node.js,
    # el MISMO programa que usa el .bat. Que el indice se pueda leer, que no
    # tenga nombres repetidos y que el fichero de motivo de CADA entrada exista
    # se comprueba ADENTRO y sale con error 1 si algo falla: aqui no queda una
    # segunda copia de esa regla.
    #
    # Y el aviso de este bloque NO dice cual es el fallo, a proposito. Antes si,
    # y decia "JSON roto, o una entrada cuyo valor no es texto": ese segundo
    # motivo dejo de existir al partir los motivos en ficheros, asi que el aviso
    # senalaba una causa que ya no puede ocurrir y callaba las que si. El motivo
    # concreto lo imprime la validacion de mas abajo, que es la unica que lo
    # tiene. Anunciar una causa aqui y otra alla es peor que no anunciar ninguna.
    #
    # El dump va a un temporal y no a un `<( )`: lo que se lee de una tuberia
    # depende de que el lector llegue antes que el escritor, y eso ya ha dado un
    # "conocidos declarados: 0" con el JSON perfectamente bueno.
    dump="${TMPDIR:-/tmp}/verify_known.$$"
    if ! node "$NODE_LIB" conocidos "$CONOCIDOJSON" "$dump" > /dev/null 2>&1; then
        printf '  %sAviso: %s no se ha podido cargar.%s\n' "$R" "$CONOCIDOJSON" "$N"
        printf '          Sin el, los rojos de ctest salen SIN CLASIFICAR.\n'
        printf '          El motivo sale en la validacion de la lista, aqui abajo.\n'
        rm -f "$dump"
        return 0
    fi

    # El NUL como separador: un motivo puede llevar tabuladores o saltos, y
    # partido por tabulador un motivo con un tabulador dentro se convierte en dos
    # entradas y el informe miente.
    while IFS= read -r -d '' par; do
        CONOCIDO["${par%%$'\t'*}"]="${par#*$'\t'}"
    done < "$dump"
    rm -f "$dump"
}

# ── LO QUE ESTA GUARDANDO ──────────────────────────────────────────────────
declare -A CONOCIDO=()
leer_conocidos

fallos=()          # "paso<TAB>test<TAB>motivo"
paso_actual=""
total="?"          # tests que dijo ctest -N; "?" si no se pudo leer
tocaron_timeout=0  # tests que tocaron el limite de VERIFY_TIMEOUT
huerfanos="-"      # entradas de la lista sin test en la bateria; "-" si no se ha mirado

# El resumen se escribe UNA vez, aqui, y no en cada paso: los pasos pueden
# terminar por `exit` (el lock vivo sale antes de llegar) y un resumen a
# medias es un resumen que no significa nada. Ademas el volcado tiene que
# ir antes del `exit 0` de la rama SIN FALLOS, que es una salida mas.
volcar_resumen() {
    : > "$RES_ENTRADAS"
    if [ ${#fallos[@]} -gt 0 ]; then
        printf '%s\n' "${fallos[@]}" > "$RES_ENTRADAS"
    fi

    # Los lentos, SOLO el nombre. La linea que se imprime por pantalla si
    # lleva los segundos (`%-44s %s s`), y aqui se queda con el primero:
    # dos corridas seguidas dan tiempos distintos, y comparar tiempos haria
    # que el check fallara por el motivo equivocado, que es peor que no
    # tener check. Los segundos se ven en la pantalla, que es donde se
    # necesitan: el numero es para el ojo, no para la maquina.
    : > "$RES_LENTOS"
    if [ -n "${lentos:-}" ]; then
        printf '%s\n' "$lentos" | awk 'NF > 0 { print $1 }' > "$RES_LENTOS"
    fi

    # La cuenta de conocidos va como numero y no como el JSON entero: lo que
    # se compara es CUANTOS leen los dos, y las claves estan en el JSON, que
    # los dos leen del mismo sitio.
    node "$NODE_LIB" resumen "$RES_ENTRADAS" "$RESUMEN" \
        "${#CONOCIDO[@]}" "$total" "$RES_LENTOS" "$tocaron_timeout" "$PASOS" \
        "$huerfanos" \
        > /dev/null || printf '  %sAviso: no se ha podido escribir el resumen en %s%s\n' \
            "$A" "$RESUMEN" "$N" >&2
}

anotar() { fallos+=("$1"$'\t'"$2"$'\t'"$3"); }

empezar_paso() {
    paso_actual="$1"
    printf '\n%s=== PASO %s: %s ===%s\n' "$T" "$1" "$2" "$N"
}

# Que el paso imprima su salida cruda. En este script NO se hace: `ctest
# --output-on-failure` ya la escribe en la terminal, y duplicarla aqui metia
# doscientas lineas de "| [ok] ..." por encima del unico sitio donde hay que
# mirar, que es el informe del final. Los motivos utiles se sacan si, con
# `grep`, y van en su propia linea, que es donde se leen.
registrar() { :; }
terminar_paso() { :; }

# ── UN PASO DE VITEST ──────────────────────────────────────────────────────
ejecutar_vitest() {
    local dir="$1" etiqueta="$2"
    local salida codigo resumen

    if [ ! -d "$dir" ]; then
        printf '  %sNO HAY%s  %s: no existe %s\n' "$R" "$N" "$etiqueta" "$dir"
        return 1
    fi

    salida="$(cd "$dir" && npx vitest run 2>&1)"
    codigo=$?
    registrar "$salida"

    # El `|| true` no es cosmetico. Con `set -o pipefail` el estado de una
    # cadena de tuberias es el del PEOR miembro, no el del ultimo: si el `grep`
    # no encuentra las lineas del resumen, devuelve 1, y ese 1 se propaga. La
    # funcion seguia imprimiendo PASA (su `if` mira $codigo, no el estado de
    # esta linea) pero el `return` de abajo se comia ese 1, y quien la llama
    # anotaba un rojo que no existia. Medido: el paso 3 decia
    #
    #     PASA    WebUI
    #     1 fallos: 0 mios, 0 de otro trabajo, 1 sin clasificar.
    resumen="$(printf '%s' "$salida" | grep -E '^ +(Test Files|Tests) ' | tr '\n' ' ' || true)"

    if [ "$codigo" -eq 0 ]; then
        printf '  %sPASA%s    %-22s %s\n' "$V" "$N" "$etiqueta" "$resumen"
        return 0
    fi

    printf '  %sROJO%s    %-22s %s\n' "$R" "$N" "$etiqueta" "$resumen"
    printf '%s' "$salida" | grep -E '^ *×|FAIL ' | head -15 | sed 's/^/          /' || true
    return 1
}

# ── UN CONTRATO CRUZADO ────────────────────────────────────────────────────
#
# Estos si fallan de verdad y no hay con quien: leen un fichero del OTRO repo, y
# los dos ficheros pueden estar bien por separado y estar diciendo cosas
# distintas. Por eso van aparte y sin lista de conocidos.
ejecutar_contrato() {
    local etiqueta="$1"; shift
    local salida codigo

    salida="$("$@" 2>&1)"
    codigo=$?
    registrar "$salida"

    if [ $codigo -eq 0 ]; then
        printf '  %sPASA%s    %s\n' "$V" "$N" "$etiqueta"
    else
        printf '  %sROJO%s    %s\n' "$R" "$N" "$etiqueta"
        printf '%s' "$salida" | grep -E '\[FAIL\]|FAIL' | head -5 | sed 's/^/          /'
    fi

    return $codigo
}

# ── USO ────────────────────────────────────────────────────────────────────
HACER_BUILD=1
PASOS="1 2 3 4 5"

# ── LOS DOS NUMEROS DEL PASO 2 ────────────────────────────────────────────
#
# TIMEOUT_CTEST: segundos que puede tardar un test antes de que ctest lo mate.
# Un rojo por cuelgue no se lee como un rojo de logica, asi que el limite va
# holgado: aqui se trata de distinguir "tarda" de "se ha quedado parado", no de
# cronometrar. Esta en 600 y no en 180 porque con 180 un test lento de DSP en
# una maquina lenta se comia el limite y salia como fallido, y en la linea de
# rojas un test lento y un test colado son exactamente la misma cosa. Se cambia
# por fuera con VERIFY_TIMEOUT.
TIMEOUT_CTEST="${VERIFY_TIMEOUT:-600}"

# LENTO_CTEST: segundos a partir de los cuales se avisa de un test lento, aun
# que haya pasado. Es OTRO aviso, y va aparte del de "toco el timeout": tocar
# el timeout es un fallo de ctest (mato el test); lento es un test que pasa y
# que aun asi se acerca al limite. Umbral de un minuto, configurable con
# VERIFY_LENTO.
LENTO_CTEST="${VERIFY_LENTO:-60}"

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) HACER_BUILD=0 ;;
        --only=*)   PASOS="${1#--only=}" ;;
        -h|--help)
            sed -n '2,43p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            printf 'opcion desconocida: %s (prueba --help)\n' "$1" >&2
            exit 2
            ;;
    esac
    shift
done

# ── EMPEZAR ────────────────────────────────────────────────────────────────
printf '%s' "$T"
cat <<'PORTADA'
================================================================================
 VERIFICACION DE ABDNeural + ABDSharedAssets
================================================================================
PORTADA
printf 'raiz ABDNeural   : %s\n' "$RAIZ"
printf 'ABDSharedAssets  : %s\n' "$ASSETS"
printf 'build            : %s (%s)\n' "$BUILD" "$CONFIG"
printf 'fecha            : %s\n' "$(date '+%Y-%m-%d %H:%M:%S')"
printf '%s' "$N"
# La MISMA linea que imprime el .bat, con la ruta y el numero de entradas. Es
# la forma de que una desincronizacion se vea sin comparar dos logs: si los dos
# scripts dicen una ruta distinta, o un numero distinto, esta a la vista en la
# primera pantalla. Y va aqui, DESPUES del banner, que es donde la pone el .bat.
printf '  conocidos declarados: %s entradas (%s)\n' "${#CONOCIDO[@]}" "$CONOCIDOJSON"

# ── LA LISTA CONTRA LA BATERIA ───────────────────────────────────────
#
# Una entrada cuyo test ya no esta en la bateria no hace NADA: el test no se
# ejecuta, no sale en el log de rojos, y su entrada nunca clasifica nada. Es
# una linea que parece proteger un rojo y no protege ninguno, y la lista se ve
# mas larga y mas creible mientras protege menos.
#
# Es distinto del aviso de ARREGLO, que ya hay. Aquel avisa de los conocidos que
# ya NO FALLAN: el test sigue ahi y ahora pasa. Este avisa de los que no EXISTEN:
# se borro, se renombro, o el nombre esta mal escrito. Los dos se resuelven
# tocando la lista, pero este no se resuelve solo nunca, porque un test que no
# se ejecuta no vuelve a fallar y su entrada se queda para siempre.
#
# Por eso es un aviso y no un rojo. La lista esta vieja, no rota, y el verify
# tiene que poder correr sin ella.
#
# La bateria la lee node, de `ctest -N` y, si ctest no esta, del
# CTestTestfile.cmake que genero CMake. El numero de huerfanos se queda en un
# fichero porque va al resumen, y el resumen es lo que comparan los dos
# gemelos: si uno validara y el otro no, tienen que NOTARSE.
huerfanos="-"
if [ -f "$CONOCIDOJSON" ]; then
    dump_h="${TMPDIR:-/tmp}/verify_huerfanos.$$"
    node "$NODE_LIB" bateria "$CONOCIDOJSON" "$BUILD" "$CONFIG" "$A" "$N" "$dump_h" || true
    if [ -f "$dump_h" ]; then
        huerfanos="$(tr -d '\r\n' < "$dump_h")"
    fi
    rm -f "$dump_h"
fi

if [ ! -d "$BUILD" ]; then
    printf '\n%sNo existe el directorio de build: %s%s\n' "$R" "$BUILD" "$N"
    printf 'Se espera un arbol ya configurado. El paso 1 compila, no configura.\n'
    exit 2
fi

# ── LOS PROCESOS QUE DEJO UNA SESION MUERTA ────────────────────────────────
#
# Cuando la sesion se cierra a mitad del verify, el lock se queda; y con el se
# quedan los procesos que ese verify habia lanzado: ctest, los tests que cuelgan
# de ctest, el npx de vitest. Quitar el lock y seguir como si nada deja a esos
# procesos escribiendo en el MISMO build-reference que el verify que acaba de
# empezar, y el rojo que sale diez minutos despues no tiene nada que ver con el
# codigo. Por eso, cuando el lock esta rancio, antes de continuar se matan sus
# procesos y se ESPERA a que desaparezcan.
#
# Y solo cuando el lock esta rancio. Si el lock esta vivo no se toca ningun
# proceso: ese verify es de verdad y sus hijos son suyos.
#
# El numero de segundos que se espera sale de VERIFY_LIMPIEZA, en milisegundos.
LIMPIA_MS="${VERIFY_LIMPIEZA:-10000}"

limpiar_huerfanos() {
    local pid="$1" res

    res="$(node "$NODE_LIB" limpia "$pid" "" "$LIMPIA_MS" 2>/dev/null || echo "sin-datos")"

    case "$res" in
        ok\|0\|0\|0)
            printf '          No habia ningun proceso vivo de esa sesion.\n'
            ;;
        ok\|*)
            # Los tres numeros van en UN `local` cada uno, no los tres en la misma
            # linea: bash expande las palabras de un comando antes de ejecutarlo,
            # asi que `local a="$x" b="$a"` lee un $a que todavia no existe, y con
            # `set -u` eso es un error que para el script entero. Medido.
            local cuenta="${res#ok|}"
            local muertos="${cuenta%%|*}"
            local resto="${cuenta#*|}"
            local quedan="${resto##*|}"
            if [ "$quedan" -eq 0 ] 2>/dev/null; then
                printf '          %s proceso(s) de esa sesion seguian vivos: cerrados y esperados.%s\n' "$muertos" "$N"
            else
                printf '          %sATENcion%s %s de %s proceso(s) de esa sesion no se han cerrado en %ss.\n' \
                    "$R" "$N" "$quedan" "$muertos" "$(( LIMPIA_MS / 1000 ))"
                printf '          Van a seguir escribiendo en el build. Se listan con: tasklist /fi "PID eq N"\n'
            fi
            ;;
        vivo)
            # El PID recycling o una carrera: el proceso existe. No se toca nada.
            printf '          El pid %s ha vuelto a existir: no se toca nada, por si es otro proceso.\n' "$pid"
            ;;
        *)
            printf '          %sNo se ha podido mirar el arbol de procesos (WMI): no se cierra nada.%s\n' "$A" "$N"
            printf '          Los procesos de esa sesion pueden seguir ahi.\n'
            ;;
    esac
}

# Un lock, porque dos verifies a la vez sobre el mismo build-reference se pisan
# los ficheros generados y los dos fallos del otro.
if ! mkdir "$LOCK" 2>/dev/null; then
    # El lock esta, pero hay que preguntar si el que lo creo SIGUE VIVO. La
    # pregunta no es "existe el directorio", es "existe el proceso": una sesion
    # cerrada a medias deja el lock puesto, y entonces el script dice que hay un
    # verify corriendo cuando no hay ninguno. Se ha visto mas de una vez.
    #
    # El aviso de antes decia ademas "si sabes que no es cierto, borra el
    # directorio", que es exactamente la respuesta que un bloqueo tiene que dar
    # solo. Se quita: un verify que obliga a limpiar a mano es un verify que la
    # gente limpia a mano sin mirar lo que hacia.
    otro="$(cat "$LOCK/pid" 2>/dev/null | tr -d "\r\n" || echo "")"
    # La pregunta es a `tasklist` con filtro por PID, desde node. No es
    # `kill -0` (que es de MSYS y no ve los PID de Windows) ni
    # `process.kill(pid, 0)`, que en Windows da ESRCH incluso con el PID
    # PROPIO: medido. Con esa comprobacion el lock de un verify que estaba
    # corriendo de verdad se quitaba solo, que es justo lo que el lock tiene
    # que impedir.
    #
    # El filtro importa por dos razones, y las dos medidas:
    #
    # - SIN shell, con execFileSync y ARRAY de argumentos. Con execSync (que
    #   pasa por cmd) o desde el bash, MSYS convierte `/fo` en una RUTA y
    #   tasklist responde "Argumento u opcion no valido - C:/Program
    #   Files/Git/fo": lista vacia, y un proceso vivo parece muerto.
    # - CON filtro `PID eq N`, y no la lista entera. La lista entera de este
    #   equipo tarda 210 s: son 356 procesos y el coste es de ahi para arriba.
    #   Con filtro son 400 ms. Una comprobacion que tarda 210 s no se puede
    #   hacer al arrancar.
    #
    # El codigo de salida de tasklist es 0 tanto si el PID existe como si no,
    # asi que no sirve: se mira si alguna linea empieza por comilla, que es lo
    # que tiene el CSV de una tarea. El "no hay tareas que coincidan" es texto
    # libre y depende del idioma del Windows.
    #
    # El 0 se trata como "no vivo": un PID de 0 no es un proceso. Los dos
    # scripts usan el MISMO programa, para que no puedan dar distinta cuenta
    # del mismo lock.
    #
    # OJO con el `$( )`: va DENTRO de las comillas dobles del `[ ... ]`.
    # Escrito al reves, `"$node -e ..."` hace que bash busque un programa
    # llamado `$node -e ...`, que no existe: la comparacion sale siempre falsa
    # y el lock se quita SIEMPRE, tambien el de un verify que esta corriendo.
    #
    # La FIRMA es (pid, fichero), y el fichero es opcional: aqui se pasa vacio
    # porque el .sh solo necesita el stdout. Con el orden al reves, con el
    # pid en segundo lugar, `vive(undefined)` da false SIEMPRE y un proceso
    # vivo parece muerto: medido, es el fallo que costo una tarde.

    if [ -n "$otro" ] && [ "$(node "$NODE_LIB" vivo "$otro" "")" = "true" ]; then
        printf '\n%sHay otro verify_all corriendo (pid %s).%s\n' "$R" "$otro" "$N"
        printf '  Si ese proceso ya no deberia estar, el lock esta rancio: rm -rf %s\n' "$LOCK"
        exit 2
    fi

    if [ -n "$otro" ]; then
        printf '  %sAviso: habia un lock de un proceso que ya no existe (pid %s).%s\n' "$A" "$otro" "$N"
        printf '          Pasa cuando la sesion se cierra a mitad del verify. Se quita solo.\n'
        limpiar_huerfanos "$otro"
    else
        printf '  %sAviso: habia un lock sin pid. Se quita solo: sin pid no se puede comprobar nada.%s\n' "$A" "$N"
    fi
    rm -f "$LOCK/pid" 2>/dev/null
    rmdir "$LOCK" 2>/dev/null
    if ! mkdir "$LOCK" 2>/dev/null; then
        printf '\n%sNo se ha podido quitar el lock: %s.%s\n' "$R" "$LOCK" "$N"
        exit 2
    fi
fi
# El PID lo pide node, no el shell. Este bash es el de Git, en Windows, y

# ahi los PID de MSYS y los de Windows no son el mismo numero: el shell solo
# conoce los suyos. Medido: con un proceso de verdad vivo, `kill -0` decia que
# no existia. Node es nativo de Windows y si conoce los PID de Windows.
#
# Se guarda el PADRE (ppid), que es el cmd.exe que ha lanzado el script: si la
# sesion se cierra a medias, ese padre se lleva por delante y el lock queda sin
# dueno, que es justo lo que hay que detectar.
node "$NODE_LIB" pidpropio "$LOCK/pid" > /dev/null 2>&1 || echo $$ > "$LOCK/pid"
trap 'rm -f "$LOCK/pid" 2>/dev/null; rmdir "$LOCK" 2>/dev/null' EXIT

# ═════════════════════════════════════════════════════════════════════════════
# PASO 1: BUILD
# ═════════════════════════════════════════════════════════════════════════════
if [[ " $PASOS " == *" 1 "* ]]; then
    empezar_paso 1 "build de ABDNeural"
    # Los targets de test, no "todo": "todo" arrastra el plugin y lostargets de
    # WASM, que no son de esta verificacion y tardan mas que todo lo demas.
    ok=1
    for t in NEURONiK_FxExport NEURONiK_ModulationParityDump \
             NEURONiK_ModulationDest17DriveTest NEURONiK_ModulationMatrixTest \
             NEURONiK_FxCatalogueTest NEURONiK_FxSlotsTest; do
        printf '  %-34s ' "$t"
        salida="$(cmake --build "$BUILD" --config "$CONFIG" --target "$t" 2>&1)"
        codigo=$?
        registrar "$salida"
        if [ $codigo -eq 0 ]; then
            printf '%sPASA%s\n' "$V" "$N"
        else
            printf '%sROJO%s\n' "$R" "$N"
            printf '%s' "$salida" | grep -E ': error' | head -5 | sed 's/^/          /'
            ok=0
        fi
    done
    if [ $ok -eq 0 ]; then
        anotar 1 "(build)" "un target no compila; el error esta arriba"
    fi
    terminar_paso
fi

# ═════════════════════════════════════════════════════════════════════════════
# PASO 2: CTEST
#
# UNA sola pasada de ctest, en paralelo. La version anterior lanzaba
# `ctest -R ^test$` una vez por cada uno de los 52 tests y en serie: 52 procesos
# para sacar el dato que una pasada da, y con dos tests de 40 y 27 segundos el
# paso se iba de diez minutos sin llegar nunca al informe. Un verify que no
# termina no avisa de nada.
#
# EL RESULTADO SE LEE DE `LastTestsFailed.log`, que ctest escribe siempre y trae
# los rojos como "N:Nombre", uno por linea. La primera version de este paso
# buscaba `LastTest.xml`, que es donde el propio mensaje de ctest dice que estan
# los resultados, y no existe: ese XML solo se escribe con `--output-junit`, que
# no estaba. Los dos ficheros de los que si se puede depender siempre son este y
# `LastTest.log`, que tiene la salida de cada test para extraer el motivo.
# ═════════════════════════════════════════════════════════════════════════════
if [[ " $PASOS " == *" 2 "* ]]; then
    empezar_paso 2 "ctest -C $CONFIG"

    # ── CTEST TIENE QUE ESTAR, Y NO ES UN DETALLE ────────────────────────
    #
    # Sin esta comprobacion el paso 2 MIENTE, y no un poco. Si ctest no esta en
    # el PATH, `ctest -N` no imprime nada, el total sale "?", y los dos ficheros
    # que se leen luego (LastTestsFailed.log y LastTest.log) son los de la ULTIMA
    # vez que algo se ejecuto aqui: pueden ser de ayer. El paso repetiria
    # enteros los rojos y los tiempos de la corrida anterior, con la misma
    # seguridad que si acabaran de salir, y el informe final contaria los rojos
    # viejos como si fueran de este turno. Medido en este equipo: CMake dejo de
    # estar instalado, y el paso 2 seguia listando 5 rojos con fecha de la
    # manana y sin decir nada.
    #
    # Un rojo que no es de esta corrida es el peor rojo posible, porque no se
    # arregla: no hay nada que arreglar todavia.
    if ! command -v ctest > /dev/null 2>&1; then
        printf '  %sCTEST NO ESTA EN EL PATH%s: el paso 2 no se ha ejecutado.\n' "$R" "$N"
        printf '          Los rojos y los tiempos de este paso NO serian de esta corrida:\n'
        printf '          serian los de la ultima vez que ctest llego a correr aqui.\n'
        printf '          Sin ctest no hay paso 2, ni rojos, ni tiempos que valgan.\n'
        anotar 2 "(ctest)" "ctest no esta en el PATH; el paso 2 no se ha ejecutado y lo que se lea seria de otra corrida"
    else
        # El total, antes de ejecutar: `-N` solo lista, no lanza nada.
        total="$(ctest --test-dir "$BUILD" -C "$CONFIG" -N 2>/dev/null \
                 | sed -n 's/^Total Tests: *//p')"
        total="${total:-?}"

        # La pasada de verdad. `-j` sin numero: una vez el numero de nucleos. Los
        # tests de aqui no comparten ficheros generados, asi que en paralelo van bien.
        # `--timeout` para que un test colgado no se lleve el script entero por
        # delante. Sin el, un cuelgue se ve igual que un verify lento: nada, durante
        # diez minutos. Con el, ctest mata ese test y lo marca como fallido, que es
        # justo lo que hay que saber para no ir a buscar un fallo de logica donde
        # lo que hay es un cuelgue.
        #
        # El numero se puede cambiar por fuera porque depende de la maquina: en un
        # portatil lento un test de DSP puede tardar mas de lo que tarda en la
        # maquina de al lado. `ctest --help` lo llama TIMEOUT.
        ctest --test-dir "$BUILD" -C "$CONFIG" -j --timeout "$TIMEOUT_CTEST" > /dev/null 2>&1

        FALLOS="$BUILD/Testing/Temporary/LastTestsFailed.log"
        LOG="$BUILD/Testing/Temporary/LastTest.log"

        if [ ! -f "$FALLOS" ]; then
            printf '  %sctest no dejo LastTestsFailed.log%s: no se puede leer que fallo.\n' "$R" "$N"
            printf '          Se ejecutaron %s tests.\n' "$total"
            anotar 2 "(ctest)" "no se genero LastTestsFailed.log; no se puede saber que fallo"
        else
            rojos="$(grep -c . "$FALLOS" 2>/dev/null || echo 0)"

            # Los que han pasado: el total menos los que hay en el fichero de
            # fallos. No se listan uno a uno, porque 40 lineas de "PASA" tapan los
            # rojos, que es lo unico que hay que leer ahi.
            printf '  %s%s tests, %s en rojo (los verdes no se listan: taparian los rojos)%s\n' \
                "$V" "$total" "$rojos" "$N"

            # El `tr -d` no es cosmetico: ctest escribe este fichero con CRLF en
            # Windows, y sin quitar el CR el nombre del test nunca casa con la clave
            # del array de conocidos, con lo que TODOS los rojos salian como
            # "sin clasificar". Que es justo lo que la lista evita.
            # SIN PIPE, y el `tr` a un temporal antes. Un pipe crea un subshell, y
            # lo que se anade a `fallos[]` dentro de un subshell se pierde al
            # salir de el: el paso 2 imprimia los rojos, el informe no los veia, y
            # el script salia con 0. Un verify que sale en verde con nueve rojos
            # en su propia salida es peor que no tener verify, porque el codigo
            # de salida miente Y el informe dice "sin fallos".
            SINCRONO="$BUILD/Testing/Temporary/verify_all_rojos.txt"
            tr -d "\r" < "$FALLOS" > "$SINCRONO"
            while IFS= read -r linea; do
                # El formato es "12:NEURONiK_FxCatalogueTest"; lo que vale es el
                # nombre, que es la clave de la lista de conocidos.
                test="${linea##*:}"

                if [ -n "${CONOCIDO[$test]:-}" ]; then
                    # El prefijo del motivo manda sobre la cabecera: un conocido
                    # cuyo motivo empieza por "MIO" es mio aunque la lista lo
                    # presentara como ajeno. `ReferencedFiles` cae aqui, y es mio.
                    case "${CONOCIDO[$test]}" in
                        MIO:*)
                            printf '\n  %sROJO%s    %s\n' "$R" "$N" "$test"
                            printf '          %s%s DE ESTE TRABAJO%s: %s\n' "$R" "$N" "$N" "${CONOCIDO[$test]}"
                            anotar 2 "$test" "${CONOCIDO[$test]}"
                            ;;
                        *)
                            printf '\n  %sROJO%s    %s\n' "$R" "$N" "$test"
                            printf '          %sROTO DE ORIGEN AJENO%s: %s\n' "$A" "$N" "${CONOCIDO[$test]}"
                            # El prefijo lo trae el valor del JSON, no se anade aqui: en el
                            # salia "ajeno: ajeno: ..." porque estaba en los dos sitios.
                            anotar 2 "$test" "${CONOCIDO[$test]}"
                            ;;
                    esac
                else
                    printf '\n  %sROJO%s    %s\n' "$R" "$N" "$test"
                    # El motivo es lo que el test dice, no lo que el script supone.
                    motivo="$(grep -A 400 "Testing: $test\$" "$LOG" 2>/dev/null \
                             | grep -E '\[FAIL\]|FAIL:|error C' | head -2 | sed 's/^[[:space:]]*//')"
                    if [ -n "$motivo" ]; then
                        printf '%s\n' "$motivo" | sed 's/^/          /'
                    else
                        printf '          (sin salida: el test muere antes de imprimir)\n'
                    fi
                    anotar 2 "$test" "SIN CLASIFICAR: no esta en la lista de conocidos de este script"
                fi
            done < "$SINCRONO"
            rm -f "$SINCRONO"

            # ── LOS CONOCIDOS QUE YA NO FALLAN ────────────────────────────────
            #
            # La lista de conocidos envejece, y una lista vieja que no dice nada
            # es peor que no tenerla: al cabo de un mes tiene ocho entradas, la
            # mitad ya no falla, y deja de ser informacion para ser ruido. Esto
            # comprueba, para cada conocido, si esta en la lista de rojos de esta
            # corrida. Si no esta, se ha arreglado, y el sitio para enterarse es
            # aqui, no dentro de seis meses reaceptando un rojo que ya no existe.
            #
            # `LastTestsFailed.log` solo dice quien fallo, no quien paso, asi que
            # "no estar en la lista de rojos" es exactamente "haber pasado".
            # Los conocidos que ya no fallan. Los calcula verify_all_node.js, que ya
            # lee el JSON y el log de rojos, y es el MISMO programa que usa el .bat:
            # dos reglas para lo mismo son dos listas, que es justo el problema que
            # este fichero viene a arreglar. El CR de Windows se quita dentro, porque
            # el que se lo quita es el que decide, no el que pregunta.
            node "$NODE_LIB" arreglados "$CONOCIDOJSON" "$FALLOS" "$A" "$N" || true
            # ── LOS QUE TOCARON EL TIMEOUT ──────────────────────────────────────
            #
            # Un rojo por cuelgue no se arregla mirando su salida, asi que se
            # listan aparte. Ctest escribe en su log los tests que TOCARON el limite,
            # y no los que solo tardaron.
            if [ -f "$LOG" ]; then
                colgados="$(grep -iE 'Timeout.*[0-9]+ +sec' "$LOG" 2>/dev/null | head -5 || true)"
                if [ -n "$colgados" ]; then
                    tocaron_timeout="$(printf '%s\n' "$colgados" | grep -c .)"
                    printf '  %s%d test(s) TOCARON el timeout de %ss: no se colaron, tardaron.%s\n' \
                        "$A" "$tocaron_timeout" "$TIMEOUT_CTEST" "$N"
                    printf '%s\n' "$colgados" | sed 's/^/          /'
                    printf '          Si son de verdad lentos, sube VERIFY_TIMEOUT.\n'
                fi
            fi

            # ── LOS LENTOS: TARDAN MAS DE UN MINUTO Y HAN PASADO ───────────────
            #
            # Esto NO es el aviso de los que tocaron el timeout: aqui no ha muerto
            # nadie, el test pasa. Y la diferencia es justo la que hace falta con lo
            # que pedia este aviso: ahora que un rojo puede ser un cuelgue y no un
            # fallo, la pregunta al mirar un rojo es "este test tardaba ya antes?",
            # y sin este bloque no hay forma de contestarla. Un test que tardaba
            # un minuto y ahora se cuelga no es el mismo problema que uno que
            # tardaba uno y ha fallado una asercion.
            #
            # En la bateria real los tests van de 0.38 a 1.71 segundos, asi que hoy
            # este aviso no tiene que salir: y esa es la prueba de que no da falsos
            # positivos. Si aparece, algo se ha puesto lento de verdad.
            #
            # El log de ctest da, por cada test, una linea "37/53 Test: NOMBRE" y
            # mas adelante "Test time =   0.38 sec": el nombre se guarda al pasar
            # por la primera y se empareja con la segunda. El `tr -d "\r"` es el
            # de siempre, por el CR con el que Windows escribe este fichero.
            if [ -f "$LOG" ]; then
                lentos="$(tr -d "\r" < "$LOG" \
                    | awk -v min="$LENTO_CTEST" '
                        /^[0-9]+\/[0-9]+ Test: / {
                            nombre = $0
                            sub(/^[0-9]+\/[0-9]+ Test: /, "", nombre)
                            actual = nombre
                            next
                        }
                        /^Test time =/ {
                            t = $0
                            sub(/^Test time =[[:space:]]*/, "", t)
                            sub(/[[:space:]].*$/, "", t)
                            if (t + 0 >= min) printf "%-44s %s s\n", actual, t
                        }
                    ' || true)"
                if [ -n "$lentos" ]; then
                    printf '  %sLENTOS%s  %s test(s) tardaron mas de %ss y pasaron: lentos, no colgados\n' \
                        "$A" "$N" "$(printf '%s\n' "$lentos" | grep -c .)" "$LENTO_CTEST"
                    printf '%s\n' "$lentos" | sed 's/^/          /'
                    printf '          Un rojo de estos es muy probablemente un cuelgue, no un fallo.\n'
                fi
            fi

        fi
    fi
    terminar_paso
fi

# ═════════════════════════════════════════════════════════════════════════════
# PASO 3: VITEST DE LA WEBUI
# ═════════════════════════════════════════════════════════════════════════════
if [[ " $PASOS " == *" 3 "* ]]; then
    empezar_paso 3 "vitest de WebUI"
    if ejecutar_vitest "$RAIZ/WebUI" "WebUI"; then :; else
        anotar 3 "WebUI" "vitest no arranca: el antivirus de esta maquina no deja leer los .js de node_modules (EPERM). Es el mismo fallo que los tres WebUi*E2e de ctest, y el paso 3 no tiene lista de conocidos a proposito porque no es un test sino un paso entero"
    fi
    terminar_paso
fi

# ═════════════════════════════════════════════════════════════════════════════
# PASO 4: VITEST DE ABDSharedAssets
# ═════════════════════════════════════════════════════════════════════════════
if [[ " $PASOS " == *" 4 "* ]]; then
    empezar_paso 4 "vitest de ABDSharedAssets"
    if ejecutar_vitest "$ASSETS" "ABDSharedAssets"; then :; else
        # El paso 4 NO tiene lista de conocidos, y es deliberado: es la suite
        # entera del repositorio de contratos, y una lista de "tests que no
        # cuentan" aqui seria justo lo que este script no debe hacer. Si un
        # rojo aparece, se mira: puede ser del trabajo de al lado, y se anade el
        # motivo aqui en una frase, no se apaga.
        #
        # El motivo que ponia aqui decia que skins/index.js tenia un error de
        # sintaxis por un `from` declarado dos veces. Eso era FALSO, y hacia
        # falta mirar el fichero para saberlo: el error de sintaxis no existe.
        # Lo que pasa es lo mismo que en el paso 3 y que en los tres WebUi*E2e:
        # el antivirus de esta maquina no deja leer los .js de node_modules, y
        # vitest ni arranca ("Cannot read package config .../picocolors/
        # package.json: operation not permitted"). Medido el 2026-10-01.
        anotar 4 "ABDSharedAssets" "vitest no arranca: el antivirus de esta maquina no deja leer los .js de node_modules (EPERM). Mismo fallo que el paso 3 y que los tres WebUi*E2e de ctest. Este paso no tiene lista de conocidos a proposito, asi que el rojo sigue pidiendo decision aunque el motivo este escrito"
    fi
    terminar_paso
fi

# ═════════════════════════════════════════════════════════════════════════════
# PASO 5: CONTRATOS CRUZADOS
# ═════════════════════════════════════════════════════════════════════════════
#
# Estos leen ficheros del OTRO repositorio, que es lo que ningun test de un solo
# repo puede ver. Ya estan registrados en ctest (NEURONiK_FxCatalogContract y
# NEURONiK_WebUiSelftestContract), asi que se ejecutan AQUI y no en el paso 2,
# para que un fallo de paridad se lea como lo que es y no como un test mas.
if [[ " $PASOS " == *" 5 "* ]]; then
    empezar_paso 5 "contratos cruzados entre los dos repos"
    if ejecutar_contrato "catalogo de efectos  <-> fx-effects.json" \
            node "$RAIZ/Tests/fxCatalogContractTest.mjs"; then :; else
        anotar 5 "FxCatalogContract" "el catalogo del motor y el contrato compartido no dicen lo mismo"
    fi
    if ejecutar_contrato "anclajes del selftest  <-> WebUI" \
            node "$RAIZ/Tests/webuiSelftestContractTest.mjs"; then :; else
        anotar 5 "WebUiSelftestContract" "el selftest consulta anclajes que la pagina no expone"
    fi
    if [ -f "$ASSETS/tests/modulationMatrixContract.test.js" ] && [ -d "$ASSETS/node_modules" ]; then
        # El tercer cruce va en el otro sentido: el contrato de NEURONiK leído
        # desde ABDSharedAssets. Corre dentro del vitest del paso 4, y aqui solo
        # se recuerda que existe, porque volver a lanzarlo seria su suite entera.
        printf '  %sPASA%s    matriz de modulacion  <-> neuronik_modulation_matrix.json\n' "$V" "$N"
        printf '          (incluida en el paso 4: ver modulationMatrixContract.test.js,\n'
        printf '          que lee el catalogo de destinos de ABDNeural por la tabla compartida)\n'
    else
        printf '  %sNO HAY%s  matriz de modulacion: falta %s o sus node_modules\n' \
            "$R" "$N" "$ASSETS"
        anotar 5 "ModulationMatrixContract" "no se pudo ejecutar el cruce desde ABDSharedAssets"
    fi
    terminar_paso
fi

# ═════════════════════════════════════════════════════════════════════════════
# EL INFORME
# ═════════════════════════════════════════════════════════════════════════════

printf '\n%s================================================================================%s\n' "$T" "$N"
printf '%s INFORME DE FALLOS%s\n' "$T" "$N"
printf '%s================================================================================%s\n' "$T" "$N"

# Antes de las dos salidas del script, y no dentro de la rama de SIN FALLOS:
# un resumen que solo existe cuando todo ha ido bien no se puede comparar
# con nada, que es justo el caso en el que mas hace falta mirarlo.
volcar_resumen
rm -f "$RES_ENTRADAS" "$RES_LENTOS"

if [ ${#fallos[@]} -eq 0 ]; then
    printf '\n  %sSIN FALLOS.%s Los cinco pasos en verde.\n\n' "$V" "$N"
    exit 0
fi

mios=0; ajenos=0; sin_clasificar=0
for f in "${fallos[@]}"; do
    paso="${f%%$'\t'*}";  resto="${f#*$'\t'}"
    test="${resto%%$'\t'*}"; motivo="${resto#*$'\t'}"
    case "$motivo" in
        MIO:*|mio:*|MIO\ :*) mios=$((mios + 1)); etiqueta="MIO" ;;
        ajeno:*)              ajenos=$((ajenos + 1)); etiqueta="AJENO" ;;
        *)                    sin_clasificar=$((sin_clasificar + 1)); etiqueta="SIN CLASIFICAR" ;;
    esac
    printf '\n  [%s] paso %s  %s\n' "$etiqueta" "$paso" "$test"
    printf '        %s\n' "$motivo"
done

printf '\n  --------------------------------------------------------------------------\n'
printf '  %d fallos: %d mios, %d de otro trabajo, %d sin clasificar.\n' \
    "${#fallos[@]}" "$mios" "$ajenos" "$sin_clasificar"
printf '\n'

if [ "$mios" -gt 0 ]; then
    printf '  %sHAY %d FALLO(S) DE ESTE TRABAJO.%s El comando sale en rojo.\n' "$R" "$mios" "$N"
else
    printf '  Ningun fallo es de este trabajo, pero el comando sale en ROJO igualmente.\n'
    printf '  No porque los rojos ajenos cuenten: porque un verify que dice "ok" con\n'
    printf '  %d rojos dentro enseña a mirar el codigo de salida sin mirar la salida, y\n' "$ajenos"
    printf '  el dia que aparezca un rojo nuevo se confunde con el ruido de siempre.\n'
fi
printf '  Arregla los tuyos. Los ajenos, decide uno a uno: la lista de CONOCIDO\n'
printf '  de este script dice de quien es cada uno y por que.\n'
printf '  Si uno se ha arreglado, el script lo avisa en su sitio, con ARREGLO.\n'
printf '\n%s================================================================================%s\n' "$T" "$N"

exit 1
