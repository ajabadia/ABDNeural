#!/usr/bin/env bash
#
# verify_all_check.sh -- ¿DICEN LO MISMO EL .sh Y EL .bat?
#
#   Lanza los dos gemelos, uno detrás de otro, sobre el MISMO build, y compara
#   lo que cada uno dice. Si no coinciden, sale en rojo y dice dónde.
#
# ── POR QUE ESTE SCRIPT EXISTE ────────────────────────────────────────────
#
# Hay dos verify_all: el .sh para bash y el .bat para Windows. Hacen lo mismo,
# leen la misma lista de conocidos y cubren los mismos cinco pasos. Y es
# facilísimo que dejen de hacerlo: cada uno lleva su cuenta, su parsing del log
# de ctest, su regla de clasificación y su forma de avisar de un lento. Basta
# con que uno se toque y el otro no, y los dos siguen dando numeros distintos
# sobre el mismo build, sin que nada lo delate.
#
# Eso ya ha pasado, y no como teoria. Se ha medido: sobre el MISMO build el
# .bat daba 8 fallos y el .sh 7, y no porque uno mintiera, sino porque cada uno
# tenia su lista y su cuenta. El rojo "de mas" del .bat no era un fallo del
# build: era una entrada de mas en una copia de la lista que el otro no tenia.
#
# Un twin que mide distinto del twin no es un twin. Es dos fuentes de verdad, y
# de dos fuentes siempre se lee la que acaba de ejecutarse, que aqui es
# justamente lo que hay que evitar. Este script lo convierte en una pregunta
# con codigo de salida: lanza los dos, compara, y falla si no dicen lo mismo.
#
# ── QUE SE COMPARA, Y QUE NO ──────────────────────────────────────────────
#
# Los dos scripts vuelcan un "resumen" antes de salir (verify_all.sh y
# verify_all.bat, con la orden `resumen` de verify_all_node.js): una linea por
# recuento y una linea por rojo y por lento lento, en un formato de tabulador,
# ordenado. Se compara eso y solo eso.
#
# NO se comparan los tiempos. Dos corridas seguidas dan tiempos distintos
# (el mismo test que tardaba 108,51 s puede tardar 95,10 s con la maquina menos
# cargada), y comparar tiempos haria que este check fallara por el motivo
# equivocado. Un check que falla por lo que no debe es peor que no tener
# check: entrena a ignorar el rojo. Los segundos se ven en la salida de cada
# verify, que es donde se necesitan; aqui solo se mira SI un test es lento, no
# CUANTO. Es un check de "dicen lo mismo", no de "tardan lo mismo".
#
# ── POR QUE NO COMPARA LAS SALIDAS DE PANTALLA ────────────────────────────
#
# Porque las dos salidas difieren en rutas (el .sh dice /d/... y el .bat
# D:\...), en el banner, en el reloj y en como cada uno formatea su tabla de
# lentos. Comparar eso es comparar dos textos parecidos, y lo unico que
# interesa es el campo que ha cambiado. Por eso el resumen lleva solo lo que
# es una AFIRMACION sobre el build, no su forma.
#
# ── USO ────────────────────────────────────────────────────────────────────
#
#   ./Scripts/verify_all_check.sh                  # los cinco pasos, dos veces
#   ./Scripts/verify_all_check.sh --only=2         # solo el paso 2 (ctest)
#   ./Scripts/verify_all_check.sh --no-build       # salta el build en los dos
#   ./Scripts/verify_all_check.sh --estricto       # un rojo intermitente tambien
#                                                 # cuenta como divergencia
#   ./Scripts/verify_all_check.sh --comparar A B   # compara dos resumenes que ya
#                                                 # hay, sin lanzar nada
#
# Los dos gemelos se lanzan uno DESPUES del otro, nunca a la vez: comparten el
# lock, y el segundo, si encuentra el lock del primero, se niega a arrancar
# (con razon) y su resumen no se escribiria. Y escriben cada uno su resumen en
# un fichero DISTINTO, no en el que por defecto usa cada uno: si escribieran en
# el mismo, el segundo pisaria al primero y compararia un fichero consigo
# mismo, que sale siempre bien.
#
# Salidas: 0 dicen lo mismo; 1 no dicen lo mismo; 2 uso incorrecto o no se ha
# podido lanzar uno de los dos (falta node, falta bash, falta el build, ...).
#
# ── UN ROJO INTERMITENTE NO ES UNA DIVERGENCIA ──────────────────────────────
#
# Lo de arriba miente en un caso, y el caso es real: un test que sale rojo unas
# veces y verdes otras. Se ha MEDIDO el 2026-10-02: el .sh corrio con
# NEURONiK_WebUiLocalModeE2e en rojo y el .bat en verde de la misma corrida (8
# relanzamientos: 6 verdes, 2 rojos), y el bloque de "no dicen lo mismo" salio
# con 1 mandando a mirar un `diff` de los dos scripts. No habia ninguna regla
# tocada en ningun gemelo: lo que habia era un test que va a veces, y el
# mensaje que sale es peor que un rojo falso, porque dice una causa que no es la
# causa y entrena a ignorar el bloque entero.
#
# Asi que hay TRES veredictos mas alla del rojo, y salen con 0 para que el check
# no se rompa:
#
#   0  dicen lo mismo (o solo se ha movido un rojo intermitente / una vuelta ha
#      ido peor: ver abajo)
#   1  NO dicen lo mismo: una regla esta en un gemelo y no en el otro
#   3  INTERMITENTE: solo se ha movido un rojo que sale a veces
#   4  DEGRADADO: cuentan igual, pero una vuelta ha ido PEOR (timeouts de mas)
#
# El 3 y el 4 los pone `compara`, no este script, para que quien llame a la orden
# por separado pueda distinguirlos del 0 sin leer el texto. Aqui se traducen a
# bloques propios que nombran lo que se ha movido y dicen que hacer, y el check
# sale en 0 a proposito: comparar dos vueltas en las que la maquina ha ido
# distinta no dice nada de los dos scripts, solo de la maquina.
#
# EL 4 ES NUEVO, y viene de un caso medido el 2026-10-02: el .sh corrio los cinco
# pasos con 0 timeouts y el .bat con 2, y los dos tests web que el .bat mato por
# tiempo le salieron LENTOS al .bat y no al .sh. Los dos scripts CUENTAN IGUAL, y
# sin este veredicto eso salia con 1 y un "mire el diff de los dos scripts", que
# era un consejo falso de principio a fin: no habia ninguna regla tocada, habia
# una maquina que en la segunda vuelta iba peor.
#
# COMO SE SABE QUE ES ESO Y NO UNA DIVERGENCIA. La pregunta es una: si se le
# suman al lado que no vio el rojo las lineas que le sobran, los dos vuelven a
# decir exactamente lo mismo? La comprobacion la hace `compara` con la misma
# aritmetica de siempre, y por eso no es una opinion: si los RECUENTOS no
# cuadran con las lineas que sobran (un `fallos` que no es la diferencia, un
# `mios` que no sube lo que sube el numero de lineas MIO) no cuenta, y sale
# con 1 como antes.
#
# Y UN `lento` DE MAS NO SE PERDONA POR SU CUENTA. El umbral de lento es una
# REGLA, y este script existe para ver si las dos reglas son la misma: perdonarlo
# sin mas seria una tautologia ("los lentos no cuadran, asi que los lentos no cuentan").
# Solo se perdona cuando lo RESPALDA un timeout de mas, porque un timeout es un
# hecho de aquella vuelta y no hay ninguna regla escrita aqui que lo produzca o
# lo quite. Sin ese timeout, un `lento` de mas sale con 1, que es lo que hacia
# antes de que existiera el 4.
#
# LO QUE EL 4 NO PUEDE DECIR. Con dos vueltas no hay forma de distinguir "la
# maquina fue mas lenta" de "al gemelo que fue mas lento le cambiaron el umbral":
# la segunda es una divergencia de verdad. Por eso el 4 no dice "es la maquina" ni
# sale con 0 sin mas: sale con 0 PERO DICE QUE HAY QUE REPETIR EL CHECK, y con
# `--estricto` sale con 1, que es la via para no depender de la buena fe. Un 3 se
# resuelve relanzando los tests que nombra; un 4 solo se resuelve relanzando el
# check entero.
#
# LO QUE NO SE PUEDE SABER, y por eso el bloque no dice "es intermitente" sino
# "este test se ha movido": con dos corridas no hay forma de distinguir un rojo
# a veces de un rojo real que a uno de los dos no le ha salido. La salida es
# relanzar esos tests, y si vuelven a salir rojos ya no son intermitentes. Con
# `--estricto` se quita el tercero de los veredictos y todo lo que no cuadra
# sale con 1, que es la via para no perder el diente.
#
# El codigo de salida de cada verify NO se propaga. Un verify en rojo por un
# fallo de este trabajo es lo normal y no es lo que este script comprueba: lo
# que comprueba es que los dos SEAN IGUALES. Si uno sale en rojo y el otro
# tambien, y dicen lo mismo, este check sale en verde a proposito: no esta
# juzgando el build, esta juzgando a los dos scripts. (Los dos verify siguen
# dando su propio 0/1/2 a quien los lance por separado.)

set -u

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIRSCRIPT="$RAIZ/Scripts"
NODE_LIB="$DIRSCRIPT/verify_all_node.js"
SELFTEST="$DIRSCRIPT/selftest_verify_all_node.js"
SH="$DIRSCRIPT/verify_all.sh"
BAT="$DIRSCRIPT/verify_all.bat"

if [ -t 1 ] && [ "${NO_COLOR:-}" = "" ]; then
    R=$'\033[31m'; V=$'\033[32m'; A=$'\033[33m'; T=$'\033[36m'; N=$'\033[0m'
else
    R=''; V=''; A=''; T=''; N=''
fi

# Los dos ficheros de resumen del check, en un directorio propio y no en el
# directorio de temporales de ctest: los dos gemelos escriben ahi sus resumenes
# de por si, y si el check usara las mismas rutas, uno le pisaria al otro al
# propio build. Con `%RANDOM%` en vez de `$$` a proposito: dos checks a la vez
# (en dos terminals) no se pisan los temporales, que es el mismo motivo por el
# que cada verify usa un nombre de fichero con su PID.
DIRCHECK="${TMPDIR:-/tmp}/verify_check.$$.${RANDOM}"
mkdir -p "$DIRCHECK" 2>/dev/null
if [ ! -d "$DIRCHECK" ]; then
    printf '%sNo se ha podido crear %s%s\n' "$R" "$DIRCHECK" "$N" >&2
    exit 2
fi
# El trap se pone justo despues de crear el directorio y no antes: si el
# `mkdir` falla y no hay directorio, un `rm -rf` sobre el camino equivocado
# podria borrar algo. Y `rm -rf` con comillas, que sin ellas un temporal con
# espacios en el nombre (el de %TEMP% de Windows los tiene) se parte en dos.
trap 'rm -rf "$DIRCHECK"' EXIT
RES_SH="$DIRCHECK/resumen_sh.txt"
RES_BAT="$DIRCHECK/resumen_bat.txt"

# ── LO QUE HAY QUE TENER ───────────────────────────────────────────────────
# Se comprueba TODO antes de lanzar nada, y en un solo sitio. Comprobar por
# partes significaba un caso en el que se gastaba un minuto de ctest para
# descubrir al final que faltaba el .bat: el error se ve en el sitio donde se
# puede arreglar, no despues de pagar la prueba.
faltan=0
falta() {
    printf '  %sFalta %s%s\n' "$R" "$1" "$N" >&2
    faltan=$((faltan + 1))
}
[ -f "$NODE_LIB" ] || falta "verify_all_node.js (junto a este script)"
[ -f "$SH" ]       || falta "verify_all.sh (junto a este script)"
[ -f "$BAT" ]      || falta "verify_all.bat (junto a este script)"
[ -f "$SELFTEST" ] || falta "selftest_verify_all_node.js (junto a este script)"
command -v node > /dev/null 2>&1 || falta "node en el PATH"
command -v cmd  > /dev/null 2>&1 || falta "cmd en el PATH (para lanzar el .bat)"

if [ "$faltan" -ne 0 ]; then
    printf '\n  Sin eso no se puede comparar: los dos scripts comparten la parte de node\n' >&2
    printf '  y el .bat necesita cmd, asi que un gemelo sin el otro no compara nada.\n' >&2
    exit 2
fi

# ── ARGUMENTOS ─────────────────────────────────────────────────────────────
# Los que se pasan, se pasan a los DOS. Un check al que se le puede cambiar
# el caso de un solo lado no es un check: es dos verifies mas una comparacion
# de dos cosas que no vinieron del mismo sitio. Se rechazan los que no son de
# verify_all, porque un flag mal understood que se le pase a uno y no al otro
# es exactamente la divergencia que este script existe para cazar.
#
# `--estricto` es la excepcion, y por una sola razon: no cambia lo que se mide,
# cambia COMO se juzga la diferencia, y ese juicio es de este script y no de los
# gemelos. Pasarselo a los dos verify seria pasarselo a uno que no lo entiende y
# que se negaria a arrancar.
HACER_BUILD=1
PASOS=""
DESDE_FICHEROS=0
ESTRICTO=0
F_A=""
F_B=""

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build)   HACER_BUILD=0 ;;
        --only=*)     PASOS="${1#--only=}" ;;
        --estricto)   ESTRICTO=1 ;;
        --comparar)   DESDE_FICHEROS=1
                      F_A="${2:-}"; F_B="${3:-}"
                      [ -n "$F_A" ] && [ -n "$F_B" ] && shift 2 || shift $# ;;
        -h|--help)
            # La cabecera va hasta la primera linea que NO es comentario, y no
            # hasta un numero de linea fijo. Un rango fijo se pudre en cuanto se
            # anade una linea al principio, y entonces el --help enseña media
            # frase y se corta sin avisar, que es como un menu a medias.
            awk 'NR > 1 && /^#/ { sub(/^# ?/, ""); print; next } NR > 1 { exit }' "$0"
            exit 0
            ;;
        *)
            printf 'opcion desconocida: %s (prueba --help)\n' "$1" >&2
            exit 2
            ;;
    esac
    shift
done

# El flag para `compara`, montado UNA vez y usado en los dos sitios donde se
# compara (el modo normal y el `--comparar`). Va con `${arr[@]+"${arr[@]}"}` y no
# con `${arr[@]}` porque en bash 3 un array vacio con `set -u` casca, y el flag
# opcional es justo el caso de array vacio.
if [ "$ESTRICTO" -eq 1 ]; then
    COMPARA_ESTRICTO=(--estricto)
else
    COMPARA_ESTRICTO=()
fi

# ── EL SELFTEST DE verify_all_node.js, ANTES DE GASTAR NADA ────────────────
#
# Los dos gemelos comparten la parte de node, asi que un fallo ahi los rompe a
# los dos IGUAL, y este check sigue en verde: su trabajo es que CUENTEN igual,
# no que la lista cargue bien. Por eso el selftest va antes, y no es opcional:
# pagar dos verificaciones enteras (que son minutos) para que las dos se rompan
# por lo mismo, y encima salir con 0 diciendo que todo coincide, es el peor
# resultado que puede dar este script.
#
# Y va antes de lanzar los gemelos, no despues: si la logica compartida esta
# rota, el resultado de la comparacion no significa nada y se ha perdido el
# tiempo para nada. Primero lo barato, que son dos segundos.
printf '\n%s=== 0 de 2: selftest de verify_all_node.js ===%s\n' "$T" "$N"
cod_selftest=0
node "$SELFTEST" > "$DIRCHECK/selftest.txt" 2>&1 || cod_selftest=$?
if [ "$cod_selftest" -eq 0 ]; then
    printf '  %sok%s: %s\n' "$V" "$N" "$(grep -c '^  ok' "$DIRCHECK/selftest.txt") comprobaciones de la parte compartida"
else
    printf '  %sFALLA%s: la logica compartida esta rota, y los dos gemelos se romperian igual.\n' "$R" "$N"
    printf '        El check NO puede decir nada sobre el build con esto asi.\n'
    printf '\n'
    cat "$DIRCHECK/selftest.txt"
    exit 1
fi

# ── MODO --comparar: LOS DOS FICHEROS YA ESTAN ─────────────────────────────
# No lanza nada. Es el modo para responder "¿coinciden ESTOS dos resumenes?",
# que es la pregunta que uno se hace al mirar dos logs de ayer, y para
# comprobar el propio check sin pagar una verificacion entera. El selftest de
# arriba si se pasa, y a proposito: es lo que no depende del build.
if [ "$DESDE_FICHEROS" -eq 1 ]; then
    if [ -z "$F_A" ] || [ -z "$F_B" ]; then
        printf '  --comparar necesita LOS DOS ficheros: --comparar resumen.sh resumen.bat\n' >&2
        exit 2
    fi
    printf '\n%s COMPARANDO DOS RESUMENES YA ESCRITOS %s\n' "$T" "$N"
    # Ahi el 3 se propaga tal cual, sin pasarlo a 0 como en el modo normal: este
    # modo es el que se usa para mirar dos logs de ayer y preguntar por codigo,
    # y quien lo lance desde otro script quiere distinguir el 3 del 0. El texto
    # que sale es el mismo en los dos modos, asi que no hay nada que aprender.
    node "$NODE_LIB" compara "$F_A" "$F_B" "el primero" "el segundo" \
         ${COMPARA_ESTRICTO[@]+"${COMPARA_ESTRICTO[@]}"}
    exit $?
fi

# ── LOS ARGUMENTOS, MONTADOS PARA LOS DOS ─────────────────────────────────
# Se montan una vez y se pasan a los dos, identicos. La alternativa (leer los
# args dos veces, una por gemelo) es la forma de que un dia uno se entere de
# una opcion y el otro no, y el check pase sin comprobar nada.
ARGS=()
[ "$HACER_BUILD" -eq 0 ] && ARGS+=("--no-build")
[ -n "$PASOS" ] && ARGS+=("--only=$PASOS")

printf '\n%s================================================================================%s\n' "$T" "$N"
printf '%s LOS DOS VERIFY, SOBRE EL MISMO BUILD%s\n' "$T" "$N"
printf '%s================================================================================%s\n' "$T" "$N"
printf 'build     : %s/build-reference (Release)\n' "$RAIZ"
printf 'pasos     : %s\n' "${PASOS:-los cinco}"
printf 'build     : %s\n' "$([ "$HACER_BUILD" -eq 0 ] && echo 'saltado (--no-build)' || echo 'compilado por los dos')"
printf 'resumen sh: %s\n' "$RES_SH"
printf 'resumen bat: %s\n' "$RES_BAT"

# ── EL .sh ─────────────────────────────────────────────────────────────────
# El codigo de salida NO se mira para decidir el resultado: un verify en rojo
# por un fallo de este trabajo es lo esperado, y este script no juzga el build.
# Solo se guarda para poder decir al final "el .sh salio con 1", que es un dato
# que ayuda a entender una divergencia.
#
# Y se lanza UNA vez. Una primera version lo lanzaba dos veces: una con `if !`
# para ver si fallaba, y otra para recuperar el codigo de salida, porque el `!`
# se come el 0 y el 1 indistinguibles. Con eso el .sh corria los cinco pasos dos
# veces seguidas, y el ctest de la segunda pasada machacaba los ficheros
# generados de la primera: el check estaba alterando lo que media, y tardaba el
# doble. Aqui va con `||` y se guarda el `$?` en la propia linea.
printf '\n%s=== 1 de 2: verify_all.sh ===%s\n' "$T" "$N"
cod_sh=0
VERIFY_RESUMEN="$RES_SH" bash "$SH" "${ARGS[@]}" || cod_sh=$?
printf '  verify_all.sh: salida %s (no es el resultado de este check)\n' "$cod_sh"

# ── EL .bat ────────────────────────────────────────────────────────────────
# Se lanza con `cmd /c`, que es lo que hay en Windows, y con la ruta en
# formato de Windows. `cygpath` la convierte: el bash de Git ve /d/... y cmd no
# entiende de /d/, y un cmd que no encuentra el .bat sale con 1, que aqui se
# leeria como "un rojo del build".
printf '\n%s=== 2 de 2: verify_all.bat ===%s\n' "$T" "$N"
bat_win="$(cygpath -w "$BAT")"
res_bat_win="$(cygpath -w "$RES_BAT")"
cod_bat=0
# El `//c` con dos barras, no `/c`: el bash de Git se come la barra y convierte
# `/c` en la ruta C:\. Medido, y por eso el .bat se lanza con `//c`.
VERIFY_RESUMEN="$res_bat_win" cmd //c "$bat_win" "${ARGS[@]}" > /dev/null 2>&1 || cod_bat=$?
printf '  verify_all.bat: salida %s (no es el resultado de este check)\n' "$cod_bat"

# ── LA COMPARACION ─────────────────────────────────────────────────────────
# Antes de comparar, que los dos resumenes EXISTAN. Si uno no se ha escrito, no
# es que los dos no coincidan: es que uno no ha llegado a final. Son fallos
# distintos y confundirlos es como un check empieza a decir "no coinciden" sin
# decir por que, que es un check que nadie aprende a mirar.
printf '\n%s================================================================================%s\n' "$T" "$N"
printf '%s COMPARANDO LO QUE DICEN%s\n' "$T" "$N"
printf '%s================================================================================%s\n' "$T" "$N"

for par in "sh:$RES_SH" "bat:$RES_BAT"; do
    quien="${par%%:*}"
    fichero="${par#*:}"
    if [ ! -f "$fichero" ]; then
        printf '\n  %sEl verify_all.%s no ha escrito su resumen%s (%s).\n' \
            "$R" "$quien" "$N" "$fichero"
        printf '  Eso no es que los dos no coincidan: es que el .%s no ha llegado al final.\n' "$quien"
        printf '  Mira arriba su salida (el .bat puede necesitar abrirse en una consola propia).\n'
        exit 2
    fi
done

# La comparacion la hace el MISMO node que han usado los dos scripts para
# contar. Que la_compare un tercero, en otra cuenta, seria medir una cuarta
# vez lo mismo de otra forma: el numero que sale de aqui tiene que salir de la
# misma regla que ha salido el de los dos, o el check no esta comparando los
# dos scripts sino comparando uno con una cuenta propia.
node "$NODE_LIB" compara "$RES_SH" "$RES_BAT" "el .sh" "el .bat" ${COMPARA_ESTRICTO[@]+"${COMPARA_ESTRICTO[@]}"}
rc=$?

if [ "$rc" -eq 0 ]; then
    printf '\n  %sLos dos scripts dicen lo mismo.%s\n' "$V" "$N"
    printf '  Salidas de los verify: .sh %s, .bat %s (a proposito no se comparan: el codigo\n' \
        "$cod_sh" "$cod_bat"
    printf '  de salida es de cada uno, y lo que se comprueba aqui es que CUENTEN igual.)\n'
    exit 0
fi

# rc = 4: los dos scripts CUENTAN IGUAL, pero una de las dos vueltas ha ido PEOR
# (timeouts de mas, y los lentos que los acompanhan). Sale en 0 a proposito, por
# el mismo motivo que el 3 y no por generosidad: comparar dos vueltas en las que
# la maquina ha ido distinta no dice nada de los dos scripts. Pero sale MUY
# visible y DICE QUE HAY QUE REPETIR EL CHECK, porque con dos vueltas no hay
# forma de distinguir "la maquina fue mas lenta" de "al gemelo que fue mas lento
# le cambiaron el umbral de lento", y la segunda es justo el bug que este check
# existe para cazar.
#
# Por eso este bloque NO es un "pasa": es un "vuelve a lanzarlo". Un 3 se
# resuelve relanzando los tests que nombra; un 4 solo se resuelve relanzando el
# check entero, y si sale igual otra vez ya no es la maquina.
if [ "$rc" -eq 4 ]; then
    # OJO con los `%s`: el `printf` de bash vuelve a imprimir el formato por cada
    # argumento que le sobra, asi que un formato con un `%s` y dos argumentos
    # escribe el bloque DOS veces (medido). Los dos van en la misma linea.
    printf '\n  %sDEGRADADO: los dos scripts cuentan igual, pero una vuelta ha ido PEOR.%s\n' "$A" "$N"
    printf '  ctest ha matado tests por tiempo en una de las dos vueltas. Eso no lo decide\n'
    printf '  ninguna regla escrita en estos dos scripts, asi que los tests que ahi salieron\n'
    printf '  lentos y en la otra no quedan perdonados por ella.\n'
    printf '\n  Los tests afectados, el lado que los vio y los timeouts estan en el bloque de\n'
    printf '  arriba. Salidas de los verify: .sh %s, .bat %s.\n' "$cod_sh" "$cod_bat"
    printf '\n  ESTO NO ES UN VEREDICTO DE "PASA": hay que REPETIR el check entero, con la\n'
    printf '  maquina descargada. Si sale igual otra vez ya no es la maquina, y lo que hay\n'
    printf '  que mirar es la regla que escribe la lista de lentos en los dos scripts.\n'
    printf '\n  Con --estricto este mismo caso sale con 1.\n'
    exit 0
fi

# rc = 3: lo UNICO que se ha movido es un rojo intermitente. Sale en 0 a
# proposito (ver la cabecera): comparar dos corridas de un test que a veces
# falla no dice nada de los dos scripts. Pero sale MUY visible, porque pasar por
# alto un test que a veces sale rojo es peor que verlo: lo que se hace aqui es
# dejarlo escrito en el sitio donde se lee el resultado del check.
if [ "$rc" -eq 3 ]; then
    # OJO con los `%s`: el `printf` de bash vuelve a imprimir el formato por cada
    # argumento que le sobra, asi que un formato con un `%s` y dos argumentos
    # escribe el bloque DOS veces (medido). Los dos van en la misma linea.
    printf '\n  %sINTERMITENTE: los dos scripts cuentan igual, y lo unico que se ha movido%s\n' "$A" "$N"
    printf '  es un test que sale rojo a veces. El check sale en 0 a proposito: eso no\n'
    printf '  dice nada de los dos scripts, solo de la maquina.\n'
    printf '\n  Los tests que se han movido estan en el bloque de arriba, con el lado que\n'
    printf '  los vio. Salidas de los verify: .sh %s, .bat %s.\n' "$cod_sh" "$cod_bat"
    printf '\n  Que hacer: relanza esos tests (ctest -R <nombre>, sin el build entero). Si\n'
    printf '  vuelven a salir verdes, era intermitente. Si vuelven a salir rojos, ya no lo\n'
    printf '  es: es un fallo de verdad, y lo que hay que arreglar es el test.\n'
    printf '\n  Con --estricto este mismo caso sale con 1, que es la via para no perder el\n'
    printf '  diente del check.\n'
    exit 0
fi

if [ "$rc" -eq 1 ]; then
    printf '\n  %sEl .sh y el .bat NO dicen lo mismo del mismo build.%s\n' "$R" "$N"
    printf '  Uno de los dos ve mas, menos, o distinto. Salidas: .sh %s, .bat %s.\n' \
        "$cod_sh" "$cod_bat"
    printf '  El que sobra o falta no es un fallo del build: es una regla que se ha\n'
    printf '  tocado en un gemelo y no en el otro. Se mira el `diff` de arriba, se\n'
    printf '  busca que parte del codigo produce esa linea, y se cambia en los dos.\n'
    exit 1
fi

# rc = 2: no se pudo leer uno de los resumenes. Ya se ha comprobado que los dos
# ficheros existen, asi que solo queda que no se puedan abrir (permisos, un
# fichero que se ha movido entre la comprobacion y aqui, ...).
printf '\n  %sNo se ha podido comparar.%s Uno de los dos resumenes no se puede leer.\n' "$R" "$N"
exit 2
