#!/usr/bin/env bash
# suite_bajo_carga.sh: correr algo con los nucleos saturados, que es como
# aparecen los rojos que no aparecen en una pasada limpia.
#
# POR QUE EXISTE
#
# MEDIDO el 2026-10-03 con la suite de ABDSharedAssets: en una pasada limpia el
# test mas lento tardaba 6,97 s y el p95 eran 4,56 s. Con los 8 nucleos
# saturados, el mas lento tardaba 37,4 s y el p95 eran 13,1 s. El p95 limpio ya
# se comia casi todo el default de timeout de 5 s, y por eso el rojo salia con
# la maquina ocupada y no con el codigo roto.
#
# O sea: hay una clase de rojo que no es un rojo, es una medida mal hecha. Este
# script reproduce la CONDICION para poder preguntar si un arreglo aguanta o si
# solo aguanta cuando nadie mas esta corriendo.
#
# POR QUE MIDE SU PROPIA CARGA, Y NO LA DA POR HECHA
#
# MEDIDO el 2026-10-03: la primera version lanzaba los procesos de carga y ya
# estaba. Con 8 procesos corriendo, el comando de prueba tardaba 0,96 s contra
# 1,00 s en reposo: NADA. Los procesos estaban ahi (12,9 s de CPU en 3 s de
# reloj, medido), pero el planificador de Windows les deja hueco y el comando
# no nota nada. Un script que dice "con 8 nucleos saturados" cuando no hay
# contencion es peor que uno que no dice nada, porque el rojo que se busca sale
# igual y el dato parece bueno.
#
# Asi que antes de lanzar el comando, este script mide un calibrado en reposo y
# otro con la carga puesta, y avisa si la diferencia no llega al umbral. El
# calibrado es el mismo trabajo de CPU en los dos casos, y lo hace el mismo
# interprete, para que la comparacion sea de la maquina y no del codigo.
#
# LO QUE NO HACE, Y POR QUE
#
# No lanza dos veces lo que sea a la vez, a proposito. En la suite de
# ABDSharedAssets hay un test que REGENERA `contracts/`, y dos escritores a la vez
# se pisan y dan un rojo que no es el que se quiere medir. La carga son procesos
# de trabajo tonto, no una segunda copia de lo que se mide.
#
# USO
#
#     tools/verificacion/suite_bajo_carga.sh -- <comando> [argumentos]
#
# Salida con el codigo de salida de <comando>. Los procesos de carga se apagan
# siempre, pase lo que pase, con una trampa al final del script.
#
# QUE ES UN "VERDE" AQUI
#
# Que el comando salio con 0. Que la carga haya mordido lo dice el script, y si
# no ha mordido, el rc=0 no significa que el arreglo aguante: significa que no
# se ha reproducido la condicion.
set -u

if [ "${1:-}" != "--" ]; then
    echo "uso: $(basename "$0") -- <comando> [args]" >&2
    echo "  ejemplo: $(basename "$0") -- npx vitest run --no-color" >&2
    exit 2
fi
shift

if [ "$#" -eq 0 ]; then
    echo "falta el comando que hay que correr bajo carga" >&2
    exit 2
fi

# Cuanto mas lento tiene que ser el calibrado para que se de la carga por buena.
# Por debajo de 1,25x no hay contencion que hablar, y por debajo de 1,1x es
# ruido de la propia maquina.
UMBRAL=1.25

NUCLEOS=$(python -c "import os; print(os.cpu_count() or 4)" 2>/dev/null || echo 4)
CARGA="$*"
PIDS=""

limpiar() {
    for p in $PIDS; do
        kill "$p" 2>/dev/null || true
    done
}
trap limpiar EXIT INT TERM

# El calibrado: un bucle de CPU acotado, que tarda lo justo para medirse bien
# sin quitarle el rato a quien lo lanza.
calibrar() {
    python -c "
import time
t = time.time()
s = 0
for i in range(4000000):
    s += i * i
print('%.4f' % (time.time() - t))
"
}

echo "nucleos de la maquina: $NUCLEOS"
echo "carga:                 $CARGA"

echo ""
echo "1. calibrado en reposo..."
REPOSO=$(calibrar)
echo "   $REPOSO s"

echo ""
echo "2. saturando con $NUCLEOS procesos..."
i=0
while [ "$i" -lt "$NUCLEOS" ]; do
    python -c "
while True:
    pass
" >/dev/null 2>&1 &
    PIDS="$PIDS $!"
    i=$((i + 1))
done

# Dos segundos para que la carga este de verdad delante de la medicion. Sin
# esto el comando arranca antes de que los procesos acumulen nada.
sleep 2

echo ""
echo "3. calibrado con la carga puesta..."
BAJO=$(calibrar)
echo "   $BAJO s"

CARGA_MUERDE=0
if python -c "
import sys
reposo, bajo = float('$REPOSO'), float('$BAJO')
print('   ratio %.2fx (umbral %.2fx)' % (bajo / reposo, $UMBRAL))
sys.exit(0 if bajo / reposo >= $UMBRAL else 1)
"; then
    CARGA_MUERDE=1
fi

# Los procesos se apagan ANTES del comando real: si se dejaran, mediriamos el
# comando Y la carga de la prueba de la carga a la vez, que es otra cosa.
limpiar
PIDS=""

echo ""
if [ "$CARGA_MUERDE" -eq 1 ]; then
    echo "LA CARGA HA MORDIDO: la medicion de abajo es en condiciones de maquina ocupada."
else
    echo "AVISO: LA CARGA NO HA MORDIDO. La maquina no se ha puesto ocupada de verdad"
    echo "        y el rc de abajo NO prueba que el arreglo aguante con gente corriendo."
    echo "        Con mas procesos, o bajando UMBRAL, o en una maquina con menos"
    echo "        nucleos logicos. Un rc=0 aqui no es un verde."
fi

echo ""
echo "4. corriendo el comando..."
INICIO=$(date +%s)
"$@"
RC=$?
FIN=$(date +%s)

echo ""
echo "rc=$RC   duracion=$((FIN - INICIO)) s   carga=$([ "$CARGA_MUERDE" -eq 1 ] && echo 'si' || echo 'NO')"
exit $RC
