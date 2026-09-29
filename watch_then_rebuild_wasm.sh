#!/usr/bin/env bash
#
# Espera a que el otro hilo termine y commitee, y entonces recompila el
# `.wasm` para que la comprobacion de la firma pueda saltar.
#
#   ./watch_then_rebuild_wasm.sh [segundos_entre_sondeos]
#
# POR QUE NO ES "esperar al commit y ya". La firma y el `.wasm` tienen que
# salir del MISMO arbol. La firma la genera NEURONiK_LayoutExport (nativo,
# Visual Studio) y el binario lo compila emscripten: dos compiladores
# distintos leyendo las mismas cabeceras. Si uno lee el arbol antes de que
# el otro hilo lo cambie, los dos artefactos son correctos por separado y
# se contradicen -- y la pagina avisaria de un desajuste que no existe.
# Por eso el disparador no es "ha committeado" sino "ha committeado Y el
# arbol que alimenta el layout esta limpio": ahi arbol == commit y da igual
# en que orden se compilen.
#
# QUE RECOMPILA, EN QUE ORDEN. Primero la firma (nativo, segundos) y luego
# el `.wasm` (emscripten, minutos). build_wasm.bat ya hace el resto: paridad
# contra nativo, smoke, sincronizacion a public/worklet y el guard por hash.
#
set -uo pipefail
cd "$(dirname "$0")"

POLL="${1:-30}"
# BASE se arma SOLO al arrancar: el commit que hay en este momento. No
# escribirlo a mano porque cualquier commit propio lo dejaria viejo, y el
# vigilante creeria que tu commit es del otro hilo. Se puede pasar como
# segundo argumento para arrancar ya con la guardia de otro commit.
BASE="${2:-$(git rev-parse --short HEAD)}"
LOG="wasm-rebuild.log"

say() { printf '[%s] %s\n' "$(date '+%H:%M:%S')" "$*" | tee -a "$LOG"; }

# Latido. Sin esto el log queda igual de silencioso si el vigilante esta
# esperando que si se ha muerto, que es justo la confusion que haria pasar
# un "sigo aqui" por un "funciona". Uno cada 10 sondeos, no en cada vuelta.
TICKS=0
heartbeat() {
  TICKS=$((TICKS + 1))
  [ $((TICKS % 10)) -eq 0 ] || return 0
  local dirty
  dirty="$(git status --porcelain -- Source WebUI/generated | wc -l)"
  say "sigo esperando: HEAD $(git rev-parse --short HEAD), $dirty rutas sin commitear en el arbol del layout"
}

# El layout se construye desde Source/ y WebUI/generated/. Si algo de ahi
# esta modificado, el arbol ya no es el commit y compilar seria una loteria.
feed_dirty() {
  local out
  out="$(git status --porcelain -- Source WebUI/generated)"
  [ -n "$out" ]
}

say "=== esperando a que el otro hilo commitee (base $BASE, sondeo ${POLL}s)"

while true; do
  HEAD_SHORT="$(git rev-parse --short HEAD)"
  if [ "$HEAD_SHORT" != "$BASE" ]; then
    if feed_dirty; then
      say "commit $HEAD_SHORT detectado, pero el arbol sigue en vuelo:"
      git status --porcelain -- Source WebUI/generated | sed 's/^/    /' | tee -a "$LOG"
      sleep "$POLL"
      heartbeat
      continue
    fi

    say "commit $HEAD_SHORT detectado y arbol quieto. A recompilar."
    say "    $(git log -1 --format='%s')"

    # --- 1. la firma, desde el mismo arbol -------------------------------
    say "[1/3] NEURONiK_LayoutExport -> WebUI/generated/gp-layout.generated.js"
    if ! cmake --build build-reference --config Release --target NEURONiK_LayoutExport >>"$LOG" 2>&1; then
      say "    FALLO al compilar el exportador. Sigo esperando."
      sleep "$POLL"
      heartbeat
      continue
    fi
    if ! ./build-reference/Release/NEURONiK_LayoutExport.exe WebUI/generated >>"$LOG" 2>&1; then
      say "    FALLO al generar la firma. Sigo esperando."
      sleep "$POLL"
      heartbeat
      continue
    fi
    say "    $(grep -o 'LAYOUT_FINGERPRINT = [0-9]*' WebUI/generated/gp-layout.generated.js)"
    say "    $(grep -o 'LAYOUT_FIELD_COUNT = [0-9]*' WebUI/generated/gp-layout.generated.js)"

    # --- 2. el binario ---------------------------------------------------
    say "[2/3] build_wasm.bat (emscripten; puede tardar minutos)"
    if ! cmd //c build_wasm.bat nopause >>"$LOG" 2>&1; then
      say "    FALLO build_wasm.bat. Ver wasm-last-run.log y $LOG"
      exit 1
    fi

    # --- 3. que los dos hablen del mismo layout ---------------------------
    say "[3/3] comprobando el binario servido contra la pagina"
    if node Tests/wasmLayoutFingerprintTest.mjs 2>&1 | tee -a "$LOG"; then
      say "LISTO: el .wasm y la pagina hablan del mismo layout."
      say "      La pagina ya no avisara. Commit y artefactos juntos:"
      say "      git status --short"
      exit 0
    fi
    say "El .wasm y gp-layout.generated.js no cuadran. NO los commitees separados."
    exit 1
  fi
  sleep "$POLL"
  heartbeat
done
