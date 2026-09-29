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
# QUE RECOMPILA: UN SOLO PASO. build_wasm.bat, y nada mas. El paso que
# generaba la firma aqui --NEURONiK_LayoutExport en nativo, con su propio
# manejo de fallos-- se ha ido a build_wasm.bat, que ya regenera la tabla antes
# de compilar y comprueba al final que el binario y la tabla cuadran. Aqui eran
# DOS pasos que podian quedar a medias: si este vigilante generaba la firma y el
# build fallaba tres minutos despues, la tabla se quedaba regenerada y el
# `.wasm` viejo, que es exactamente el desajuste que se daba por cerrado.
#
# Los dos compilanadores siguen siendo dos --la firma en nativo, el binario con
# emscripten-- pero ahora los manda el MISMO comando, en la misma pasada y sobre
# el mismo arbol. Ver el paso 3 y el cierre de build_wasm.bat.

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

    # --- 1. el binario ---------------------------------------------------
    # El ".\\" NO es cosmetico: desde Git Bash, `cmd //c build_wasm.bat` no
    # encuentra el fichero --MSYS desactiva la busqueda del directorio actual--
    # y falla al instante con "no se reconoce como un comando", que el vigilante
    # leia como un fallo del build. Comprobado: con ".\\" arranca. El vigilante
    # esta pensado para Git Bash, de ahi el prefijo.
    if ! cmd //c ".\build_wasm.bat" nopause >>"$LOG" 2>&1; then
      say "    FALLO build_wasm.bat. Ver wasm-last-run.log y $LOG"
      exit 1
    fi

    # --- 2. que los dos hablen del mismo layout ---------------------------
    #(build_wasm.bat YA ejecuta este mismo test como su ultimo guard, asi que
    #  si el paso 1 ha salido bien, este no puede fallar. Se queda por dos cosas
    #  que el build no da: la salida del test en el log del vigilante, que es lo
    #  que se lee al volver, y el aviso de "commit y artefactos juntos" de abajo,
    #  que es la parte accionable cuando los dos ya cuadran. Cuesta dos segundos.)
    say "[2/2] comprobando el binario servido contra la pagina"
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
