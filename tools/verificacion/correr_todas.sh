#!/usr/bin/env bash
# correr_todas.sh: la suite de verificacion entera, en orden y con resumen.
#
# QUE EJECUTA, Y QUE NO
#
#   convenciones.py        Los finales de linea, los vertical tabs, las vallas
#                          de los .md, el CJK y lo que se salga de la lista de
#                          los .bat. Corre sobre lo TRACKEADO, no sobre el arbol
#                          entero: hay temporales de otras sesiones sin trackear
#                          que no son fuente de nadie y que nadie puede arreglar.
#   banco_convenciones.py  El banco de los dos verificadores. Primero, porque
#                          si un verificador no detecta lo que debe, lo que salga
#                          despues no vale como medida de nada.
#   selftest_pre_commit_scripts.py
#                          El banco del hook de pre-commit (.bat, .ps1 y .sh), con `git commit` de
#                          verdad. Va el ultimo de los automaticos porque crea
#                          repos temporales y es el mas lento.
#   selftest_post_checkout_bats.py
#                          El banco del hook de post-checkout, con `git checkout`
#                          de verdad. Detras del otro porque tambien crea repos
#                          temporales.
#
# NO ejecuta `suite_bajo_carga.sh`, y es a proposito: esa satura la maquina y
# su resultado depende de lo que se le pase. Se lanza a mano, cuando la pregunta
# es si algo aguanta con la maquina ocupada.
#
# QUE SALIDA TIENE
#
# 0 si todo pasa. 1 si algo falla, y el runner dice QUE, porque un codigo de
# salida sin nombre no dice nada cuando hay cuatro cosas que se han ejecutado.
set -u

AQUI=$(dirname "$0")
PYTHON=python
command -v "$PYTHON" >/dev/null 2>&1 || PYTHON=python3

# La raiz se pide a Python, NO con `pwd`. MEDIDO el 2026-10-03: `cd .. && pwd`
# en Git Bash devuelve `/d/desarrollos/...`, y Python en Windows no entiende esa
# ruta: abria 473 ficheros y decia "no se ha podido leer" de TODOS, con el
# `.gitattributes` delante que si estaba. Que devuelva la ruta nativa.
RAIZ=$("$PYTHON" -c "
import os, sys
print(os.path.abspath(os.path.join(sys.argv[1], '..', '..')))
" "$AQUI")
cd "$RAIZ" || exit 1

FALLOS=0
PASOS=""

correr() {
    nombre="$1"
    shift
    echo ""
    echo "############################################################"
    echo "# $nombre"
    echo "############################################################"
    "$@"
    rc=$?
    if [ "$rc" -eq 0 ]; then
        PASOS="$PASOS$nombre: PASA\n"
    else
        PASOS="$PASOS$nombre: FALLA (rc=$rc)\n"
        FALLOS=$((FALLOS + 1))
    fi
    return 0
}

echo "Suite de verificacion de $RAIZ"
echo "Python: $("$PYTHON" --version 2>&1)"

# `--trackeados` y no una lista pasada desde aqui. MEDIDO el 2026-10-03: al
# pasar `$TRACKEADOS` sin comillas el shell parte la cadena por espacios, los
# nombres del repo tienen espacios, y 433 rutas llegaron como 473 trozos, con el
# `.gitattributes` de cabeza diciendo que no se podia leer.
correr "convenciones.py (los ficheros trackeados)" \
        "$PYTHON" "$AQUI/convenciones.py" --trackeados

correr "banco_convenciones.py (los verificadores detectan)" \
        "$PYTHON" "$AQUI/banco_convenciones.py"

correr "selftest_pre_commit_scripts.py (los .bat, .ps1 y .sh)" \
        "$PYTHON" "$RAIZ/Scripts/selftest_pre_commit_scripts.py"

correr "selftest_post_checkout_bats.py (el hook del checkout)" \
        "$PYTHON" "$RAIZ/Scripts/selftest_post_checkout_bats.py"

echo ""
echo "############################################################"
echo "# resumen"
echo "############################################################"
printf "%b" "$PASOS"

echo ""
if [ "$FALLOS" -eq 0 ]; then
    echo "Todo pasa."
    exit 0
fi
echo "$FALLOS paso(s) en rojo."
exit 1
