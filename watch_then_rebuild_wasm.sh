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
# QUE RECOMPILA: UN SOLO PASO, Y EL QUE EJECUTA ES build_wasm.bat. Este
# vigilante no hace nada del trabajo: arma el entorno, espera el commit, y le
# pasa el.build. Lo que antes vivia aqui --generar la firma con
# NEURONiK_LayoutExport, y despues volver a pasar el test de la firma-- se ha ido
# dentro, donde ya estaba su sitio.
#
# POR QUE ESO MEJORA, Y NO ES SOLO MENOS TRABAJO. Los dos eran pasos que podian
# quedar a medias: si este vigilante regeneraba la tabla y el build fallaba tres
# minutos despues, la firma se quedaba nueva y el `.wasm` viejo, que es
# exactamente el desajuste que el montaje pretendia cerrar. Y el test que se
# volvia a pasar aqui no podia fallar nunca: build_wasm.bat lo corre como su
# ultimo cierre, asi que si el build habia salido bien, aqui ya no quedaba nada
# que decidir. Era un paso que no podia cambiar el resultado y que solo existia
# para que el vigilante tuviera algo que decir.
#
# Lo que este vigilante conserva es lo que el build no da: ESPERAR, y decir
# "commit y artefactos juntos" en el momento en que ya cuadran, que es la parte
# accionable.
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

    # --- 1. el binario, y con el la comprobacion --------------------------
    # Un solo paso. build_wasm.bat regenera la firma antes de compilar, sincroniza
    # el binario y CIERRA con el test de la firma, asi que si esto sale bien los
    # dos artefactos ya estan de acuerdo: no hay un segundo paso aqui que volver
    # a pasar lo mismo, ni un sitio donde el resultado de la comprobacion se
    # guarde aparte del log del build.
    #
    # El ".\\" NO es cosmetico: desde Git Bash, `cmd //c build_wasm.bat` no
    # encuentra el fichero --MSYS desactiva la busqueda del directorio actual--
    # y falla al instante con "no se reconoce como un comando", que el vigilante
    # leia como un fallo del build. Comprobado: con ".\\" arranca. El vigilante
    # esta pensado para Git Bash, de ahi el prefijo.
    say "[1/1] build_wasm.bat (emscripten; puede tardar minutos)"
    if ! cmd //c ".\build_wasm.bat" nopause >>"$LOG" 2>&1; then
      say "    FALLO build_wasm.bat. Ver wasm-last-run.log y $LOG"
      say "    Lo mas probable es la FIRMA: si el .wasm y gp-layout.generated.js"
      say "    no cuadran, el propio build lo dice al final. NO los commitees por"
      say "    separado: o los dos del mismo arbol, o ninguno."
      exit 1
    fi

    say "LISTO: el .wasm y la pagina hablan del mismo layout."
    say "      La pagina ya no avisara. Commit y artefactos juntos:"
    say "      git status --short"
    exit 0
  fi
  sleep "$POLL"
  heartbeat
done
