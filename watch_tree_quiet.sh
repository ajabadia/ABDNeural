#!/usr/bin/env bash
#
# Vigila el repositorio y avisa en cuanto el arbol quede SIN cambios del
# otro hilo. No recompila nada: solo mira y dice. Uso:
#
#   ./watch_tree_quiet.sh [segundos_entre_sondeos]
#
# QUE CUENTA COMO "SUYO". Todo lo que git status reporte menos los ficheros
# de MINE. Esos se declaran a mano y se imprimen al arrancar, porque
# excluirlos en silencio seria una forma de mentir: un vigilante que dice
# "libre" mientras esconde tus propios cambios es peor que uno que no
# existe.
#
# EL LOG TAMBIEN SE EXCLUYE, y no por capricho. Este script se cuenta a si
# mismo: sin esa exclusion, en cuanto escribiera su primera linea se
# veria como un fichero nuevo ajeno y el recuento nunca llegaria a cero.
# En este repo no se nota porque "*.log" esta en .gitignore, pero es
# casualidad del repo, no propiedad del script: en cualquier otro el
# vigilante no callaria nunca.
#
# QUE CUENTA COMO "LIBRE". Cero cambios suyos pendientes. Da igual que los
# haya commiteado o que los haya tirado: las dos cosas son "ha terminado".
# Cuando HEAD se mueve se dice explicitamente, porque committear es la
# forma buena de terminar y revertir es la forma mala, y conviene saber
# cual ha sido.
#
# LO QUE LA EXCLUSION NO PUEDE SABER. La lista MINE es por ruta, no por
# autor. Si el otro hilo tocara uno de esos ficheros, el vigilante lo
# contaria como mio y direia LIBRE. No se puede distinguir solo: un
# fichero NUEVO ("??") es con casi seguridad mio, pero una MODIFICACION de
# algo ya commiteado (" M") es justo el caso que conviene mirar. Por eso
# el informe final separa los dos y avisa de los segundos.
#
# LIMITACION CONOCIDA. git status entrecomilla las rutas que llevan
# caracteres raros y escribe los renombrados como "viejo -> nuevo". Para
# esos casos la exclusion por nombre no casa. Con los nombres de MINE --
# que son los de este repo y no tienen ni espacios ni acentos-- funciona;
# anadir uno con espacios haria que el vigilante lo contara como suyo.
#
set -uo pipefail
cd "$(dirname "$0")"

POLL="${1:-20}"
LOG="watch-tree.log"

# Ficheros mios, sin commitear a proposito. Anadirlos aqui es la unica
# manera de que "libre" signifique algo: si esto crece sin querer, el
# vigilante dejara de avisar de un cambio real.
MINE=(
  "watch_tree_quiet.sh"
  "watch_then_rebuild_wasm.sh"
  "Tests/wasmLayoutFingerprintTest.mjs"
  "$LOG"
)

# El patron tiene que agruparse ANTES de anclar, no despues. En ERE la
# alternacion tiene la precedencia mas baja, asi que "a|b|c$" se lee como
# "(a)|(b)|(c$)" y SOLO el ultimo queda anclado: un fichero como
# "watch_tree_quiet.sh.bak" se colaba por el primero y el vigilante
# diria libre con trabajo sin commitear. Ademas los nombres llevan un "."
# que sin escapar casa con cualquier caracter. Las dos cosas se arreglan
# escapando cada nombre y envolviendo la alternancia entera.
# Solo se ancla el final: la linea de git status es "XY <ruta>", asi que
# anclar el principio no casaria nunca.
esc_re() { printf '%s' "$1" | sed 's/[][\\.*^$(){}?+|\/]/\\&/g'; }
MINE_PATTERNS=()
for f in "${MINE[@]}"; do MINE_PATTERNS+=("$(esc_re "$f")"); done
MINE_RE="($(IFS='|'; echo "${MINE_PATTERNS[*]}"))$"

say() { printf '[%s] %s\n' "$(date '+%H:%M:%S')" "$*" | tee -a "$LOG"; }

# Cambios que no son mios, uno por linea.
foreign() {
  git status --porcelain | grep -v -E "$MINE_RE" || true
}

# Cuenta por tipo: lo que dice como esta el arbol, no solo cuanto.
census() {
  foreign | cut -c1-2 | sort | uniq -c | tr -s ' '
}

say "=== vigilando el arbol (sondeo ${POLL}s)"
say "excluyendo ${#MINE[@]} rutas declaradas como mias:"
for f in "${MINE[@]}"; do
  if [ ! -e "$f" ]; then
    say "    $f   -- no existe todavia (se creara al escribir)"
  elif git status --porcelain -- "$f" | grep -q .; then
    say "    $f   -- presente, con cambios sin commitear"
  else
    say "    $f   -- presente, sin cambios pendientes (commiteado o ignorado)"
  fi
done

BASE_HEAD="$(git rev-parse --short HEAD)"
PREV_COUNT=-1
TICKS=0

while true; do
  TICKS=$((TICKS + 1))
  OUT="$(foreign)"
  HEAD_SHORT="$(git rev-parse --short HEAD)"
  COUNT="$(printf '%s' "$OUT" | grep -c . || true)"

  # --- evento: el otro hilo ha commiteado -------------------------------
  if [ "$HEAD_SHORT" != "$BASE_HEAD" ]; then
    say "HEAD se ha movido: $BASE_HEAD -> $HEAD_SHORT"
    say "    $(git log -1 --format='%s' "$HEAD_SHORT")"
    BASE_HEAD="$HEAD_SHORT"
    PREV_COUNT=-1
  fi

  # --- evento: queda libre ----------------------------------------------
  if [ "$COUNT" -eq 0 ]; then
    say "LIBRE: no queda ningun cambio del otro hilo sin commitear."
    say "HEAD = $HEAD_SHORT"

    # Las rutas excluidas se listan SIN llamarlas "mias". Un "??" es
    # un fichero nuevo, que casi siempre es mio; un " M" es una
    # modificacion de algo ya commiteado, que puede no serlo, y por eso
    # se avisa en vez de esconderlo.
    NEW_FILES="$(git status --porcelain | grep -E "$MINE_RE" | grep -E '^\?\? ' || true)"
    MOD_FILES="$(git status --porcelain | grep -E "$MINE_RE" | grep -vE '^\?\? ' || true)"
    if [ -n "$NEW_FILES" ]; then
      say "rutas excluidas, ficheros nuevos:"
      printf '%s\n' "$NEW_FILES" | sed 's/^/    /' | tee -a "$LOG"
    fi
    if [ -n "$MOD_FILES" ]; then
      say "AVISO: rutas excluidas con MODIFICACIONES sobre ficheros ya"
      say "       commiteados. La exclusion es por ruta, no por autor, asi"
      say "       que aqui no puedo decir que sean mias. Revisalas:"
      printf '%s\n' "$MOD_FILES" | sed 's/^/    /' | tee -a "$LOG"
    fi
    if [ -z "$NEW_FILES" ] && [ -z "$MOD_FILES" ]; then
      say "no queda nada en el arbol: esta completamente limpio."
    fi

    say ""
    say "Ahora el .wasm se puede recompilar: ./watch_then_rebuild_wasm.sh"
    exit 0
  fi

  # --- evento: el recuento ha cambiado ----------------------------------
  if [ "$COUNT" -ne "$PREV_COUNT" ]; then
    if [ "$PREV_COUNT" -lt 0 ]; then
      say "el otro hilo tiene $COUNT cambios sin commitear:"
    else
      say "el otro hilo tiene $COUNT cambios (antes $PREV_COUNT)"
    fi
    census | sed 's/^/    /' | tee -a "$LOG"
    PREV_COUNT="$COUNT"
  elif [ $((TICKS % 15)) -eq 0 ]; then
    # Latido: distingue "sigue igual" de "se ha muerto". Un vigilante que
    # solo escribe cuando algo cambia calla igual esperando que caido.
    say "sigo vigilando: $COUNT cambios suyos, HEAD $HEAD_SHORT"
  fi

  sleep "$POLL"
done
