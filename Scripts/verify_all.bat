@echo off
REM ============================================================================
REM
REM  verify_all.bat -- LOS CINCO PASOS DE LA VERIFICACION, en un comando y con
REM  un unico codigo de salida. El gemelo exacto de verify_all.sh.
REM
REM    1. build de ABDNeural (los targets que existen, no "todo")
REM    2. ctest -C Release
REM    3. vitest de la WebUI
REM    4. vitest de ABDSharedAssets
REM    5. los contratos cruzados entre los dos repositorios
REM
REM  -- POR QUE DICE EN ROJO Y NO "TODO BIEN" -------------------------------
REM
REM  Hoy la bateria tiene rojos que NO son de este trabajo: un hilo dejo el
REM  modulo compartido a medias (shelf, Phaser4, envelopeCurve), y hay tests que
REM  expectan un catalogo de 6 motores y 3 ids reservados cuando ya hay 8 y 0.
REM
REM  La tentacion con un verify es "separar los rojos conocidos y salir en
REM  verde". Eso es una trampa: en cuanto un verify sale en verde con 9 rojos
REM  dentro, toda la gente aprende a mirar el codigo de salida sin mirar la
REM  salida. Y el dia que aparezca un rojo NUEVO, se confunde con el ruido de
REM  siempre y nadie lo ve. Un verify que dice "ok" con lo que hay dentro es
REM  peor que no tener verify, porque ocupa el sitio del que avisaria.
REM
REM  Asi que: sale en rojo si hay CUALQUIER fallo, y al final lista cada uno con
REM  su motivo, separando "este es mio" de "este es del otro trabajo". Que la
REM  separacion exista no significa que se pueda ignorar el rojo.
REM
REM  -- USO ------------------------------------------------------------------
REM
REM    Scripts\verify_all.bat              :: los cinco pasos
REM    Scripts\verify_all.bat --no-build   :: salta el paso 1
REM    Scripts\verify_all.bat --only=3     :: un solo paso, para depurar
REM
REM  Salidas: 0 todo en verde; 1 algun paso con fallos; 2 uso incorrecto.
REM
REM  Variables de entorno (las dos las leen tambien verify_all.sh):
REM    VERIFY_TIMEOUT  segundos que un test puede tardar antes de que ctest lo
REM                    mate (600 por defecto). Sube el TECHO, no los avisos.
REM    VERIFY_LENTO    segundos a partir de los cuales un test que pasa se avisa
REM                    por lento (60 por defecto).
REM
REM ============================================================================

setlocal EnableDelayedExpansion

REM --- DONDE ESTAMOS -------------------------------------------------------
for %%I in ("%~dp0..") do set "RAIZ=%%~fI"
for %%I in ("%~dp0..\..") do set "RAIZPADRE=%%~fI"
set "ASSETS=%RAIZPADRE%\ABDSharedAssets"
set "BUILD=%RAIZ%\build-reference"
set "CONFIG=Release"
REM -- LOS DOS NUMEROS DEL PASO 2 --------------------------------------------
REM
REM TIMEOUT_CTEST: segundos que puede tardar un test antes de que ctest lo mate.
REM El limite se deja holgado a proposito: aqui se trata de distinguir "tarda"
REM de "se ha quedado parado", no de cronometrar. Esta en 600 y no en 180 porque
REM con 180 un test lento de DSP en una maquina lenta se comia el limite y salia
REM como fallido, y en la linea de rojas un test lento y uno colado son
REM exactamente la misma cosa. Se cambia con VERIFY_TIMEOUT.
REM
REM LENTO_CTEST: segundos a partir de los cuales se avisa de un test lento, aun
REM que haya pasado. Es OTRO aviso, y va aparte del de "toco el timeout": tocar
REM el timeout es un fallo de ctest (mato el test); lento es un test que pasa y
REM que aun asi se acerca al limite. Se cambia con VERIFY_LENTO.
REM
REM Los dos scripts usan los mismos numeros por defecto.
if defined VERIFY_TIMEOUT (set "TIMEOUT_CTEST=%VERIFY_TIMEOUT%") else set "TIMEOUT_CTEST=600"
if defined VERIFY_LENTO (set "LENTO_CTEST=%VERIFY_LENTO%") else set "LENTO_CTEST=60"
REM Milisegundos que se espera a que mueran los procesos de una sesion de verify
REM que ya no existe, cuando el lock sale rancio. VERIFY_LIMPIEZA.
if defined VERIFY_LIMPIEZA (set "LIMPIA_MS=%VERIFY_LIMPIEZA%") else set "LIMPIA_MS=10000"
REM La carpeta del script, en una VARIABLE y no con %~dp0 suelto. Motivo,
REM medido con un .bat de nueve lineas: el `shift` del bucle de argumentos
REM tambien desplaza `%0`, y `%~dp0` sale el directorio de trabajo en vez
REM de la carpeta del script. Antes, la lista de conocidos se buscaba en
REM la raiz y no se encontraba, y los cinco rojos ajenos salian SIN
REM CLASIFICAR. Todo lo que se calcule DESPUES del bucle tiene que usar
REM %DIRSCRIPT%, no %~dp0.
set "DIRSCRIPT=%~dp0"
set "LOCK=%~dp0.verify_all.lock"

REM --- COLOR: solo si la consola lo aguanta (un .bat redirigido a un fichero
REM ---escribe caracteres de escape se ve como basura, y un log con basura no se
REM --- puede leer en un log de CI). ------------------------------------------
set "R=" & set "V=" & set "A=" & set "T=" & set "N="
if not defined NO_COLOR (
    for /f "tokens=2 delims=:" %%C in ('chcp 2^>nul') do (
        set "CP=%%C"
    )
    echo !CP! | findstr /r "^[0-9][0-9][0-9]$" >nul
    if not errorlevel 1 (
        if !CP! GEQ 65001 (
            set "R=^[[31m" & set "V=^[[32m" & set "A=^[[33m" & set "T=^[[36m" & set "N=^[[0m"
        )
    )
)

REM ============================================================================
REM  EL RESUMEN COMPARABLE
REM
REM  Lo que este script DICE, en una forma que se pueda comparar con el .sh
REM  linea a linea. No se imprimia: se escribia en un fichero que nadie leia.
REM  Con el, verify_all_check.bat lanza los dos gemelos y pregunta si dicen
REM  lo mismo, que es la pregunta que mas veces ha salido mal sobre este repo.
REM
REM  La ruta se puede dar por fuera con VERIFY_RESUMEN. Lo necesita el check:
REM  los dos gemelos escriben el suyo en ficheros DISTINTOS, porque si
REM  escribieran en el mismo, el segundo pisaria al primero y compararia un
REM  fichero consigo mismo, que sale siempre bien.
REM
REM  Y por defecto cada uno en el suyo, con el nombre del gemelo dentro, en el
REM  directorio de temporales de ctest y NO junto al script: un fichero
REM  generado en la carpeta Scripts aparece en `git status` al lado del codigo,
REM  y un verify que ensucia el repositorio teaches a meter sus salidas en el
REM  .gitignore, que es donde acaban las que no deberian estar a la vista.
REM
REM  Los lentos van a un fichero aparte con SOLO el nombre. La linea que se
REM  imprime por pantalla si lleva los segundos, y aqui no: dos corridas
REM  seguidas dan tiempos distintos, y comparar tiempos haria que el check
REM  fallara por el motivo equivocado, que es peor que no tener check. Los
REM  segundos se ven en la pantalla, que es donde se necesitan.
REM ============================================================================
if defined VERIFY_RESUMEN (set "RESUMEN=%VERIFY_RESUMEN%") else set "RESUMEN=%BUILD%\Testing\Temporary\verify_all_resumen.bat.txt"
set "LENTOSFILE=%TEMP%\verify_lentos_%RANDOM%.txt"

REM --- LO QUE ESTA GUARDANDO -------------------------------------------------
REM Los fallos van a un fichero temporal, uno por linea: paso TAB test TAB motivo.
REM Este MISMO fichero es el que se le pasa a la orden `resumen` de node: no se
REM escribe una segunda copia de los rojos, porque una segunda copia es una
REM segunda lista, y una segunda lista es lo que hay que evitar.
set "REPORT=%TEMP%\verify_all_%RANDOM%.txt"
type nul > "%REPORT%"

REM Los contadores que van al resumen. Se inicializan aqui, y no donde se
REM usan, porque con `--only=3` el paso 2 no se ejecuta y `!TOTAL!` no llega a
REM existir: leerlo sin definir da error, y el resumen se queda sin campo.
set "TOTAL=?"
set /a COLGADOS=0
set "NCONOCIDOS=0"
REM Las entradas de la lista sin test en la bateria. A "-", que es "no se ha
REM podido validar", y no a 0, que es "la lista esta limpia": los dos son
REM numeros y se escriben igual de bien en el resumen, y solo uno de los dos
REM es verdad cuando el JSON no se ha podido leer.
set "HUERFANOS=-"

set "SALIR_POR_ROJOS=0"
REM Si algun target del paso 1 no compila. El rojo de `(build)` se anota UNA vez,
REM despues del bucle, y no uno por target: seis targets caidos son un problema,
REM no seis, y el .sh ya lo hacia asi. Antes se anotaba dentro de
REM `:build_target`, con lo que el .bat contaba cinco fallos de build donde el
REM .sh contaba uno sobre el MISMO build roto, y el check lo senalaba como
REM divergencia. Medido.
set "FALLO_BUILD="
REM Los targets que HA CONSTRUIDO esta pasada. Lo necesita el aviso de
REM binarios rancios: la lista del paso 1 es corta a proposito, asi que casi
REM todos los tests de ctest quedan fuera, y sin avisar se leen resultados
REM de un binario que nadie sabe de cuando es.
set "CONSTRUIDOS="

REM --- ARGS ------------------------------------------------------------------
set "HACER_BUILD=1"
set "PASOS=1 2 3 4 5"
:ARGS
REM ============================================================================
REM  COMO SE LEEN LOS ARGUMENTOS AQUI, Y POR QUE ESTA ASI
REM
REM Lo primero, medido con un .bat de tres lineas que solo imprime sus
REM argumentos, para no partir de una suposicion:
REM
REM     ARG1=[--only]   ARG2=[5]   ARG3=[]   NARG=--only=5
REM
REM Los argumentos LLEGAN PARTIDOS: a `%1` le llega `--only` y a `%2` le llega
REM `5`. No es culpa del guion doble ni del igual; es la capa que invoca el
REM script, que entrega la linea a cmd ya troceada y `%*` la recompone despues.
REM Por eso comparar `%1` contra "--only=5" no casa nunca.
REM
REM Cuatro formas de leerlos, y las cuatro fallan:
REM
REM   1. `if /i "%~1"=="--only=5"`  -> no casa, por lo de arriba.
REM   2. comparar `%*`             -> `%*` llega entero, pero con DOS argumentos
REM      son los dos pegados (`--no-build --only=5`) y no casa con ninguno.
REM   3. `if /i "%~1:~0,7"`       -> no existe: `%~1:~0,7` es una expansion de
REM      una variable llamada `%~1:~0`. Es sintaxis de Bash.
REM   4. `set "ESTE=!%*!"`        -> `%*` dentro de `!` no se expande. El `!`
REM      abre la expansion, `%*` se expande antes, y el resultado no se vuelve
REM      a evaluar. `ESTE` sale vacio.
REM
REM Lo que funciona es un bucle en el CUERPO del script, con `shift`, que
REM reensambla el token suelto: cuando `%1` es `--only` se hace un `shift` mas
REM y se pega con lo que venga. Cada vuelta compara el token ya entero.
REM
REM Y dos trampas mas, ambas medidas, que hacen que este bucle parezca bueno y
REM no lo sea:
REM
REM   - El bucle NO puede vivir dentro de una subrutina llamada con `call`. Con
REM     `call :LEER_UN_ARG %*` y un `goto` de vuelta a la etiqueta no termina
REM     nunca: "*** RECURSION DE ARCHIVO POR LOTES supera los limites de la
REM     PILA, Recuento de recursiones=335". El `goto` devuelve al marco de
REM     argumentos del SCRIPT, no al de la llamada, de modo que `%1` no se
REM     agota y cada vuelta es una llamada de `call` mas.
REM   - El `shift` NO funciona dentro de un bloque `if (...) (...)`. Reensamblar
REM     `--only` con su valor dentro del `if` produce `--only=--only`. Por eso el
REM     pegado va con `goto` a una etiqueta suelta, sin parentesis.
REM
REM Lo que no se reconoce se ignora en vez de fallar: un verify que se niega a
REM arrancar por un flag de mas es un verify que nadie ejecuta.
goto LEER_UN_ARG

:LEER_UN_ARG
REM El bucle esta en el CUERPO del script a proposito. En una subrutina no
REM puede estar: un `goto` de vuelta a la etiqueta devuelve al marco de
REM argumentos del SCRIPT, `%1` no se agota y se desborda la pila.
if "%~1"=="" goto FIN_ARGS

set "ESTE=%~1"
if /i "%~1"=="--only" goto PEGAR_ONLY
goto CLASIFICAR

:PEGAR_ONLY
REM Fuera de todo bloque: dentro de un `if (...) (...)` el `shift` se pierde y
REM el token se pega consigo mismo, `--only=--only`.
shift
set "ESTE=--only=%~1"

:CLASIFICAR
if /i "!ESTE!"=="--no-build" set "HACER_BUILD=0"
if /i "!ESTE!"=="-h" goto AYUDA
if /i "!ESTE!"=="--help" goto AYUDA
if /i "!ESTE!"=="--only=1" set "PASOS=1"
if /i "!ESTE!"=="--only=2" set "PASOS=2"
if /i "!ESTE!"=="--only=3" set "PASOS=3"
if /i "!ESTE!"=="--only=4" set "PASOS=4"
if /i "!ESTE!"=="--only=5" set "PASOS=5"
shift
goto LEER_UN_ARG

:FIN_ARGS
REM Lo que no se reconoce se ignora en vez de fallar. Un verify que se niega a
REM arrancar por un flag de mas es un verify que nadie ejecuta.
if not defined PASOS (
    echo   aviso: no se ha entendido ningun --only=N, se hacen los cinco pasos
)
:ARGS_FIN

REM --- EMPEZAR --------------------------------------------------------------
echo %T%================================================================================%N%
echo %T% VERIFICACION DE ABDNeural + ABDSharedAssets%N%
echo %T%================================================================================%N%
echo raiz ABDNeural   : %RAIZ%
echo ABDSharedAssets  : %ASSETS%
echo build            : %BUILD% (%CONFIG%)

REM ---------------------------------------------------------------------------
REM  EL PROGRAMA COMPARTIDO DE NODE
REM
REM  La logica que los dos scripts comparten (vivo, limpieza de procesos
REM  huerfanos y lista de conocidos) esta en verify_all_node.js, al lado de este
REM  fichero. Antes cada script llevaba dentro su copia, y eso no era solo
REM  tapar un codigo largo: en batch el programa va dentro de la linea de
REM  comandos, con lo que obliga a no poder usar `!`, ni `"`, ni backticks, y a
REM  vivir con el `String.fromCharCode(34)` para las comillas. En un fichero no
REM  hay nada de eso. Ademas una copia es una lista mas, y si una se toca y la
REM  otra no, los dos scripts dan distinta cuenta del mismo lock sin que nada lo
REM  delate: que es justo lo que pasaba con la lista de conocidos.
REM
REM  Y si este fichero falta, el script PARA aqui. No puede seguir a medias: sin
REM  el no se puede ni comprobar si el lock esta vivo (y entonces cualquier
REM  verify se pisa el build del otro) ni clasificar un rojo. Un verify que se
REM  queda sin clasificador sale en verde con lo que tenga delante.
REM ---------------------------------------------------------------------------
set "NODELIB=%DIRSCRIPT%verify_all_node.js"
if not exist "%NODELIB%" (
    echo.
    echo %R%No esta %NODELIB% y sin el este script no puede verificar nada.%N%
    echo   Esta junto a este .bat, en la carpeta Scripts.
    exit /b 2
)

REM ---------------------------------------------------------------------------
REM  LA LISTA DE FALLOS CONOCIDOS
REM
REM  Se lee de Scripts\verify_all_known.json, que es UN solo sitio para los dos
REM  scripts. Antes cada uno tenia la suya escrita dentro, y ya se midio lo que
REM  cuesta: sobre el mismo build el .bat daba 8 fallos y el .sh 7, no porque
REM  uno mintiera, sino porque cada uno tenia una lista distinta Y median
REM  distinto. Dos listas son dos verdades.
REM
REM  Y se busca por la MISMA regla que el .sh: el fichero de al lado del script,
REM  `%DIRSCRIPT%`, no una ruta montada a mano. Si cada uno lo busca donde le
REM  sale, en cuanto uno se copia o se symlinka los dos se leen listas
REM  distintas y nadie lo ve. Por eso los dos imprimen la MISMA linea, con la
REM  ruta y el numero de entradas: una desincronizacion se ve en la primera
REM  pantalla, sin comparar dos logs.
REM
REM  La LEE node, que ya es dependencia de los pasos 3, 4 y 5. Y la lee
REM  preguntandole, entrada a entrada, en vez de partiendola en un fichero de
REM  texto: batch no sabe sacar el segundo campo de una linea con tabulador, y
REM  se ha medido. Con `tokens=1,2` los %%M salen literales, y con
REM  `tokens=1,*` tambien. Ademas el motivo puede llevar tabuladores y batch no
REM  tiene estructura para eso.
REM
REM  Batch no tiene arrays asociativos, asi que la busqueda es una llamada a
REM  node por rojo. Con cinco entradas y solo cuando hay un rojo, da igual.
REM
REM  Y el filtro de las claves de comentario va como
REM  `k.charAt(0)==='_'?false:true` y no como `k[0]!=='_'`. Con `EnableDelayedExpansion`
REM  activo, cualquier `!` de la linea se come: el `!` abre expansion, lo que
REM  sigue se busca en el entorno y sale vacio, y a node le llega la linea
REM  troceada. Medido. Se escoge el `charAt` y no el `^` de escape porque el
REM  escape se pierde en cuanto alguien edita la linea, y el fallo es un node
REM  que no arranca en medio del paso 2.
REM ---------------------------------------------------------------------------
set "CONOCIDOJSON=%DIRSCRIPT%verify_all_known.json"
if not exist "%CONOCIDOJSON%" (
    echo %R%Aviso: no esta %CONOCIDOJSON%%N%
    echo           sin el, los rojos de ctest salen SIN CLASIFICAR
    set "NCONOCIDOS=0"
    echo   conocidos declarados: 0 entradas ^(%CONOCIDOJSON%^)
) else (
    REM `cuenta` valida Y cuenta en la misma pasada: comprueba que el indice se
    REM pueda leer, que no haya nombres repetidos y que el fichero de motivo de
    REM CADA entrada exista, y sale con error 1 si algo de eso falla. El stderr de
    REM node va a `nul` a proposito: una torre de quince lineas de node en mitad
    REM del verify no es un aviso del verify, es un node que no arranca, en la
    REM linea que mas importa.
    REM
    REM Y el aviso de este bloque no dice cual es el fallo, a proposito. Antes si,
    REM y decia "JSON roto, o una entrada cuyo valor no es texto": ese segundo
    REM motivo dejo de existir al partir los motivos en ficheros, asi que senalaba
    REM una causa que ya no puede ocurrir y callaba las que si. El motivo concreto
    REM lo imprime la validacion de mas abajo, que es la unica que lo tiene.
    node "%NODELIB%" cuenta "%CONOCIDOJSON%" "%TEMP%\verify_known.txt" >nul 2>&1
    if errorlevel 1 (
        echo   %R%Aviso: %CONOCIDOJSON% no se ha podido cargar.%N%
        echo           Sin el, los rojos de ctest salen SIN CLASIFICAR.
        echo           El motivo sale en la validacion de la lista, aqui abajo.
        set "NCONOCIDOS=0"
        echo   conocidos declarados: 0 entradas ^(%CONOCIDOJSON%^)
    ) else (
        set "NCONOCIDOS="
        for /f "usebackq delims=" %%L in ("%TEMP%\verify_known.txt") do set "NCONOCIDOS=%%L"
        echo   conocidos declarados: !NCONOCIDOS! entradas ^(%CONOCIDOJSON%^)
    )
    del "%TEMP%\verify_known.txt" >nul 2>&1
)

REM ============================================================================
REM  LA LISTA DE CONOCIDOS CONTRA LA BATERIA
REM
REM  Una entrada cuyo test ya no esta en la bateria no hace NADA: el test no
REM  se ejecuta, no sale en el log de rojos, y su entrada nunca clasifica
REM  nada. Es una linea que parece proteger un rojo y no protege ninguno, y la
REM  lista se ve mas larga y mas creible mientras protege menos.
REM
REM  Es distinto del aviso de ARREGLO, que ya hay. Aquel avisa de los conocidos
REM  que ya NO FALLAN: el test sigue ahi y ahora pasa. Este avisa de los que NO
REM  EXISTEN: se borro, se renombro, o el nombre esta mal escrito. Los dos se
REM  resuelven tocando la lista, pero este no se resuelve solo nunca, porque un
REM  test que no se ejecuta no vuelve a fallar y su entrada se queda para
REM  siempre.
REM
REM  Por eso es un aviso y no un rojo. La lista esta vieja, no rota, y el verify
REM  tiene que poder correr sin ella.
REM
REM  Va aqui y no en el paso 2 porque tiene que salir en TODAS las corridas,
REM  incluida una con `--only=5`: en el paso 2 solo apareceria cuando se ejecuta
REM  ctest, que es justo cuando el sitio donde se lee la lista ya se ha
REM  impreso. Y el paso 2 es el que mas tarda.
REM
REM  La linea que sale por pantalla la imprime node, y es la MISMA que imprime el
REM  .sh: es el unico sitio donde se ve de un vistazo si los dos gemelos cuentan
REM  igual. El numero va a un fichero aparte porque va al resumen, y el resumen es
REM  lo que verify_all_check compara entre los dos.
REM ============================================================================
if exist "%CONOCIDOJSON%" (
    set "HUEFICH=%TEMP%\verify_huerfanos_%RANDOM%.txt"
    node "%NODELIB%" bateria "%CONOCIDOJSON%" "%BUILD%" "%CONFIG%" "%A%" "%N%" "!HUEFICH!"
    if exist "!HUEFICH!" (
        for /f "usebackq delims=" %%H in ("!HUEFICH!") do set "HUERFANOS=%%H"
    )
    del "!HUEFICH!" >nul 2>&1
)

if not exist "%BUILD%" (
    echo.
    echo %R%No existe el directorio de build: %BUILD%%N%
    echo Se espera un arbol ya configurado. El paso 1 compila, no configura.
    exit /b 2
)

REM ---------------------------------------------------------------------------
REM  EL LOCK
REM
REM  El lock esta, pero hay que preguntar si el que lo creo SIGUE VIVO. La
REM  pregunta no es "existe el directorio", es "existe el proceso": una sesion
REM  cerrada a medias deja el lock puesto, y entonces el script dice que hay un
REM  verify corriendo cuando no hay ninguno. Se ha visto mas de una vez, y lo
REM  peor no es el bloqueo: es que el aviso parece una proteccion, asi que da la
REM  sensacion de que hay que esperar, y no hay a quien esperar.
REM
REM  El PID lo pide node, que ya es dependencia de los pasos 3, 4 y 5. NO se usa
REM  `tasklist /fi "IMAGENAME eq cmd.exe"`, que devuelve el primer cmd.exe de la
REM  lista y no necesariamente este: un lock rancio podria parecer vivo.
REM  `process.ppid` es el padre, que es el cmd.exe que ha lanzado el .bat, y ese
REM  se lleva por delante cuando la sesion muere, que es lo que se quiere notar.
REM ---------------------------------------------------------------------------
if not exist "%LOCK%" mkdir "%LOCK%" 2>nul
if not exist "%LOCK%" (
    echo.
    echo %R%No se ha podido crear el lock: %LOCK%%N%
    exit /b 2
)
if exist "%LOCK%\pid" (
    set "OTRO="
    for /f "usebackq delims=" %%P in (`node "%NODELIB%" leepid "%LOCK%\pid"`) do set "OTRO=%%P"
    call :vivo "!OTRO!"
    if not errorlevel 1 (
        echo.
        echo %R%Hay otro verify_all corriendo ^(pid !OTRO!^).%N%
        echo Si ese proceso ya no deberia estar, el lock esta rancio: borra %LOCK%
        set "HAY_OTRO_VERIFY=1"
        goto LOCK_OCUPADO
    )
    echo.
    echo   %A%Aviso: habia un lock de un proceso que ya no existe ^(pid !OTRO!^).%N%
    echo           Pasa cuando la sesion se cierra a mitad del verify. Se quita solo.
    call :limpia_huerfanos "!OTRO!"
    del "%LOCK%\pid" >nul 2>&1
del "%TEMP%\verify_mipid.txt" >nul 2>&1
rmdir "%LOCK%" >nul 2>&1
    mkdir "%LOCK%" 2>nul
)
REM ---------------------------------------------------------------------------
REM  SALIR CON 2 SI EL LOCK ESTA VIVO
REM
REM  El `exit /b 2` NO puede ir dentro del `if not errorlevel 1 (...)`, que ya va
REM  dentro de otro `if`. Anidado en dos niveles, cmd no sale: imprime el aviso y
REM  se queda con un 0. Medido con un .bat de seis lineas, y medido aqui: el
REM  .bat decia "hay otro verify_all corriendo" y salia con 0, mientras el .sh
REM  sale con 2. Un verify que no se ha ejecutado y sale en verde es peor que no
REM  tener verify, porque el que llama cree que ha verificado.
REM
REM  Por eso el `if` solo pone una bandera y sale con `goto`, y el `exit` esta
REM  aqui, ya fuera de los dos bloques.
REM ---------------------------------------------------------------------------
:LOCK_OCUPADO
if defined HAY_OTRO_VERIFY exit /b 2

call :mi_pid
> "%LOCK%\pid" echo !PIDLOCK!

REM ============================================================================
REM  PASO 1: BUILD
REM ============================================================================
echo.
call :QUIERE 1
if not errorlevel 1 (
    echo.
    echo %T%=== PASO 1: build de ABDNeural ===%N%
    if "%HACER_BUILD%"=="1" (
        call :build_target NEURONiK_FxExport
        call :build_target NEURONiK_ModulationParityDump
        call :build_target NEURONiK_ModulationDest17DriveTest
        call :build_target NEURONiK_ModulationMatrixTest
        call :build_target NEURONiK_FxCatalogueTest
        call :build_target NEURONiK_FxSlotsTest
    )
    REM Un solo rojo para los seis, y fuera del bloque: el nombre del problema es
    REM "un target no compila", y son los targets, no los pasos. Los errores de
    REM Los errores de compilacion de cada uno los ha impreso ya `:build_target`.
    if defined FALLO_BUILD call :anotar 1 "(build)" "un target no compila; el error esta arriba"
    REM EL AVISO DE LOS BINARIOS RANCIOS. Va aqui, y no mas adelante, porque es
    REM la unica vez que se sabe que targets se han construido: si se espera, el
    REM paso 2 ya ha corrido la bateria entera sobre binarios que nadie sabe de
    REM cuando son. Con --no-build no se dice nada, porque ahi el aviso seria ruido.
    node "%NODELIB%" rancios "%BUILD%" "%CONFIG%" "%A%" "%N%" !CONSTRUIDOS!
)

REM ============================================================================
REM  PASO 2: CTEST
REM ============================================================================
call :QUIERE 2
if not errorlevel 1 (
    echo.
    echo %T%=== PASO 2: ctest -C %CONFIG% ===%N%
    call :paso2
)

REM ============================================================================
REM  PASO 3: VITEST DE LA WEBUI
REM ============================================================================
call :QUIERE 3
if not errorlevel 1 (
    echo.
    echo %T%=== PASO 3: vitest de WebUI ===%N%
    call :paso3
)

REM ============================================================================
REM  PASO 4: VITEST DE ABDSharedAssets
REM ============================================================================
call :QUIERE 4
if not errorlevel 1 (
    echo.
    echo %T%=== PASO 4: vitest de ABDSharedAssets ===%N%
    call :paso4
)

REM ============================================================================
REM  PASO 5: CONTRATOS CRUZADOS
REM ============================================================================
REM Estos leen ficheros del OTRO repositorio, que es lo que ningun test de un
REM solo repo puede ver. Ya estan registrados en ctest, asi que se ejecutan AQUI
REM y no en el paso 2, para que un fallo de paridad se lea como lo que es.
call :QUIERE 5
if not errorlevel 1 (
    echo.
    echo %T%=== PASO 5: contratos cruzados entre los dos repos ===%N%
    call :paso5
)

goto INFORME

REM ============================================================================
REM  SUBRUTINAS
REM ============================================================================

REM ---------------------------------------------------------------------------
REM  QUIERE: este paso esta en la lista de PASOS?
REM
REM  Sin esto, `--only=5` no hacia nada: PASOS se leia y se guardaba, y no lo
REM  miraba nadie. Medido: los cinco pasos se ejecutaron igual.
REM
REM  Un espacio a cada lado de los dos lados, para que buscar el 5 no case con
REM  un 15. En el .sh esto es una linea de bash; en batch es una subrutina,
REM  porque a pelo son tres niveles de parentesis.
:QUIERE
set "QTODOS= %PASOS% "
set "QBUSCADO= %~1 "
if "!QTODOS:%QBUSCADO%=!"=="!QTODOS!" exit /b 1
exit /b 0

:build_target
set "TGT=%~1"
echo   %TGT%
cmake --build "%BUILD%" --config "%CONFIG%" --target "%TGT%" > "%TEMP%\verify_bt.txt" 2>&1
if errorlevel 1 (
    echo   %R%ROJO%N%    %TGT%
    findstr /c:": error" "%TEMP%\verify_bt.txt" >nul
    if not errorlevel 1 (
        findstr /c:": error" "%TEMP%\verify_bt.txt" | findstr /r /v "^$" > "%TEMP%\verify_err.txt"
        set /a MOSTRAR=0
        for /f "usebackq delims=" %%L in ("%TEMP%\verify_err.txt") do (
            if !MOSTRAR! LSS 5 (
                echo             %%L
                set /a MOSTRAR+=1
            )
        )
    )
    set "FALLO_BUILD=1"
    set "SALIR_POR_ROJOS=1"
) else (
    echo   %V%PASA%N%
    if defined CONSTRUIDOS (
        set "CONSTRUIDOS=!CONSTRUIDOS! %~1"
    ) else (
        set "CONSTRUIDOS=%~1"
    )
)
exit /b 0

:paso2
REM ---------------------------------------------------------------------------
REM  CTEST TIENE QUE ESTAR, Y NO ES UN DETALLE
REM
REM  Sin esta comprobacion el paso 2 MIENTE, y no un poco. Si ctest no esta en
REM  el PATH, el `ctest -N` no imprime nada, el total se queda en "?", y los dos
REM  ficheros que se leen luego (LastTestsFailed.log y LastTest.log) son los de
REM  la ULTIMA vez que algo se ejecuto aqui: pueden ser de ayer. El paso
REM  repetiria enteros los rojos y los tiempos de la corrida anterior, con la
REM  misma seguridad que si acabaran de salir, y el informe los contaria como si
REM  fueran de este turno. Medido en este equipo: CMake dejo de estar
REM  instalado, y el paso 2 seguia listando 5 rojos con fecha de la manana y
REM  sin decir nada.
REM
REM  Un rojo que no es de esta corrida es el peor rojo posible, porque no se
REM  arregla: no hay nada que arreglar todavia. Por eso esto no es un aviso, es
REM  un rojo, con su `anotar` y su salida en rojo.
REM ---------------------------------------------------------------------------
where ctest >nul 2>nul
if errorlevel 1 (
    echo   %R%CTEST NO ESTA EN EL PATH: el paso 2 no se ha ejecutado.%N%
    echo             Los rojos y los tiempos de este paso NO serian de esta corrida:
    echo             serian los de la ultima vez que ctest llego a correr aqui.
    echo             Sin ctest no hay paso 2, ni rojos, ni tiempos que valgan.
    call :anotar 2 "(ctest)" "ctest no esta en el PATH; el paso 2 no se ha ejecutado y lo que se lea seria de otra corrida"
    set "SALIR_POR_ROJOS=1"
    exit /b 0
)

REM El total, antes de ejecutar. `-N` solo lista, no lanza nada.
set "TOTAL=?"
for /f "usebackq tokens=2 delims=:" %%L in (`ctest --test-dir "%BUILD%" -C "%CONFIG%" -N 2^>nul ^| findstr /r /c:"Total Tests:"`) do set "TOTAL=%%L"
for /f "tokens=* delims= " %%L in ("!TOTAL!") do set "TOTAL=%%L"

REM ---------------------------------------------------------------------------
REM  UNA SOLA PASADA, Y DESPUES SE LEE QUE FALLO
REM
REM  Esto no es una optimizacion. La version anterior lanzaba `ctest -R
REM  ^nombre$` una vez por cada uno de los 53 tests, en serie. Medido: el .bat
REM  daba 8 fallos con WorkletSync sin clasificar, y el .sh daba 7 con
REM  WebUiNeedleProbeE2e como ajeno, sobre el MISMO build. Cincuenta y tres
REM  ejecuciones sueltas dejan tests sin correr entre medias, y varios de esta
REM  bateria dejan ficheros o puertos detras. Un gemelo que mide distinto del
REM  gemelo no es un gemelo. De paso tardaba mas de diez minutos.
REM ---------------------------------------------------------------------------
REM `--timeout` para que un test colgado no se lleve el script entero por
REM delante. Sin el, un cuelgue se ve igual que un verify lento: nada,
REM durante diez minutos. Con el, ctest mata ese test y lo marca como
REM fallido, que es lo que hay que saber para no ir a buscar un fallo de
REM logica donde lo que hay es un cuelgue.
ctest --test-dir "%BUILD%" -C "%CONFIG%" -j --timeout "!TIMEOUT_CTEST!" > nul 2>&1
set "FALLOSLOG=%BUILD%\Testing\Temporary\LastTestsFailed.log"
set "LOG=%BUILD%\Testing\Temporary\LastTest.log"
set "ROJOS=0"

if not exist "%FALLOSLOG%" (
    echo   %R%ctest no dejo LastTestsFailed.log: no se puede leer que fallo%N%
    echo             Se ejecutaron !TOTAL! tests.
    call :anotar 2 "(ctest)" "no se genero LastTestsFailed.log; no se puede saber que fallo"
    exit /b 0
)

REM ---------------------------------------------------------------------------
REM  EL CR DE WINDOWS
REM
REM  ctest escribe este fichero con CRLF. Sin quitar el CR, el nombre del test
REM  llega con un \r pegado, no casa con la clave de la lista de conocidos, y
REM  TODOS los rojos salen como "sin clasificar". Que es justo lo que la lista
REM  de conocidos evita. En .sh se quita con `tr -d "\r"`; aqui no hay tr, y se
REM  quita solo con leer linea a linea con `for /f`, que se come el CR.
REM ---------------------------------------------------------------------------
for /f "usebackq delims=" %%L in ("%FALLOSLOG%") do (
    set "LINEA=%%L"
    REM El formato es "12:NEURONiK_FxCatalogueTest"; lo que vale es el nombre.
    set "TST=!LINEA:*:=!"
    set /a ROJOS+=1
    call :rojo_de_ctest "!TST!"
)
echo   ---- !TOTAL! tests, !ROJOS! en rojo ^(los verdes no se listan: taparian los rojos^)
call :arreglados
REM Los dos avisos de tiempo del paso 2. Antes :timeouts se llamaba desde una
REM linea suelta que estaba DESPUES de este `exit /b 0`, es decir, en tierra
REM de nadie: nadie la ejecutaba y el aviso de los que tocaron el timeout no
REM salia nunca en Windows. Aqui se llama con el resto, que es donde tiene que
REM estar.
call :timeouts
call :lentos
exit /b 0

REM ---------------------------------------------------------------------------
REM  UN ROJO DE CTEST: CLASIFICADO, O SIN CLASIFICAR
REM ---------------------------------------------------------------------------
REM -------------------------------------------------------------------------
REM  LOS QUE TOCARON EL TIMEOUT
REM
REM  Un rojo por cuelgue no se arregla mirando su salida, asi que se listan
REM  aparte. Ctest escribe en su log los tests que TOCARON el limite, y no los
REM  que solo tardaron.
REM
REM  Y va en su propia subrutina, no dentro del bucle de arriba: un `for /f`
REM  con backticks dentro de un bloque `do ( ... )` no funciona. cmd no resuelve
REM  la sustitucion y acaba intentando abrir un fichero llamado `findstr ...`.
REM  Medido en este mismo fichero, con :arreglados.
REM -------------------------------------------------------------------------
:timeouts
set /a COLGADOS=0
if not exist "%LOG%" exit /b 0
for /f "usebackq delims=" %%L in (`findstr /i /c:"Timeout" "%LOG%"`) do (
    set /a COLGADOS+=1
    if !COLGADOS! LEQ 5 echo               %%L
)
if !COLGADOS! GTR 0 echo   %A%!COLGADOS! test(s) TOCARON el timeout de !TIMEOUT_CTEST!s: no se colaron, tardaron.%N%
if !COLGADOS! GTR 0 echo               si son de verdad lentos, sube VERIFY_TIMEOUT
exit /b 0

:lentos
REM ---------------------------------------------------------------------------
REM  LOS LENTOS: TARDAN MAS DE UN MINUTO Y HAN PASADO
REM
REM  Esto NO es el aviso de los que TOCARON el timeout: aqui no ha muerto nadie,
REM  el test pasa. Y la diferencia es la que hace falta con lo que se pidio este
REM  aviso: ahora un rojo puede ser un cuelgue y no un fallo, asi que al mirar un
REM  rojo hay que poder contestar "este test tardaba ya antes?". Un test que
REM  tardaba un minuto y ahora se cuelga no es el mismo problema que uno que
REM  tardaba uno y ha fallado una asercion.
REM
REM  El log de ctest da, por cada test, una linea "37/53 Test: NOMBRE" y mas
REM  adelante "Test time =   0.38 sec": se leen linea a linea guardando el
REM  nombre. Aqui `for /f` SI se come el CR de Windows, asi que no hace falta
REM  quitarlo a mano como en la lista de rojos.
REM
REM  Y va en su propia subrutina por el mismo motivo que :timeouts: un `for /f`
REM  con backticks dentro de un bloque `do ( ... )` no funciona en cmd. La
REM  lectura linea a linea si, porque va contra un fichero, no contra un
REM  comando.
REM
REM  En la bateria real los tests van de 0.38 a 1.71 segundos, asi que hoy esto
REM  no tiene que salir, y esa es la prueba de que no da falsos positivos.
REM ---------------------------------------------------------------------------
set /a LENTOS=0
set "LENTO_NOMBRE="
REM El fichero de los lentos se crea vacio ANTES del bucle. Sin esto, y si
REM no hay ningun lento, el fichero no existe: `resumen` lo lee, y leer lo
REM que no esta no es "0 lentos", es un error que se come el resumen entero.
type nul > "!LENTOSFILE!"
if not exist "%LOG%" exit /b 0
for /f "usebackq delims=" %%L in ("%LOG%") do call :linea_lenta "%%L"
if !LENTOS! GTR 0 echo   %A%!LENTOS! test(s) tardaron mas de !LENTO_CTEST!s y pasaron: lentos, no colgados%N%
if !LENTOS! GTR 0 echo               un rojo de estos es probablemente un cuelgue, no un fallo
exit /b 0

:linea_lenta
set "LN=%~1"
REM "37/53 Test: NOMBRE": guarda el nombre del test que empieza aqui. La busqueda
REM es de " Test: " y no de "Testing:" porque en el log hay lineas de las dos
REM cosas y solo estas ultimas son el principio de un bloque de test.
if not "!LN:* Test: =!"=="!LN!" (
    set "LENTO_NOMBRE=!LN:* Test: =!"
    exit /b 0
)
REM "Test time =   0.38 sec": de aqui solo interesan los que pasan del umbral.
REM Se compara la parte entera del tiempo, que es lo que se puede comparar en
REM batch: 0.38 da 0 y 95.2 da 95, y para un umbral de 60 eso no cambia nada.
if "!LN:~0,11!"=="Test time =" (
    set "LT=!LN:~11!"
    for /f "tokens=1" %%V in ("!LT!") do set "LT=%%V"
    set "LTI="
    for /f "tokens=1 delims=." %%I in ("!LT!") do set "LTI=%%I"
    if defined LTI (
        if !LTI! GEQ !LENTO_CTEST! (
            set /a LENTOS+=1
            if !LENTOS! LEQ 5 echo               !LENTO_NOMBRE!  !LT! s
            >>"!LENTOSFILE!" echo !LENTO_NOMBRE!
        )
    )
)
exit /b 0

:rojo_de_ctest
set "TST=%~1"
call :motivo_de "!TST!"
echo   %R%ROJO%N%    %TST%
if defined MOTIVO (
    REM El prefijo del motivo manda sobre la cabecera: un conocido cuyo motivo
    REM empieza por MIO es mio aunque la lista lo presentara como ajeno.
    REM ReferencedFiles cae aqui, y es mio.
    if /i "!MOTIVO:~0,4!"=="MIO " (
        echo             %R%ROTO DE ESTE TRABAJO%N%: !MOTIVO:~4!
    ) else (
        echo             %A%ROTO DE ORIGEN AJENO%N%: !MOTIVO!
    )
    REM Aqui va el prefijo que el informe usa para clasificar. Sin el, un rojo
    REM conocido sale "SIN CLASIFICAR", que es justo lo contrario de lo que dice
    REM la lista de conocidos. Los que empiezan por MIO se quedan como estan: el
    REM clasificador lee el motivo por delante.
    if /i "!MOTIVO:~0,4!"=="MIO " (
        call :anotar 2 "%TST%" "!MOTIVO!"
    ) else (
        REM    El prefijo lo trae el valor del JSON; aqui no se anade, o sale
        REM    "ajeno: ajeno: ..." por estar en los dos sitios.
        call :anotar 2 "%TST%" "!MOTIVO!"
    )
    exit /b 0
)

REM Sin motivo conocido: se lee lo que el test dice, no lo que el script supone.
set /a MOSTRAR=0
for /f "usebackq delims=" %%L in (`findstr /c:"[FAIL]" /c:"FAIL:" /c:"error C" "%LOG%"`) do (
    if !MOSTRAR! LSS 2 (
        echo             %%L
        set /a MOSTRAR+=1
    )
)
if !MOSTRAR! EQU 0 echo             ^(el test muere antes de imprimir nada^)
call :anotar 2 "%TST%" "SIN CLASIFICAR: no esta en la lista de conocidos de este script"
exit /b 0

REM ---------------------------------------------------------------------------
REM  LOS CONOCIDOS QUE YA NO FALLAN
REM
REM  La lista de conocidos envejece, y una lista vieja que no dice nada es
REM  peor que no tenerla: al cabo de un mes tiene ocho entradas, la mitad ya no
REM  falla, y deja de ser informacion para ser ruido. Aqui se comprueba, para
REM  cada conocido, si esta en la lista de rojos de ESTA corrida. Si no esta, se
REM  ha arreglado, y el sitio para enterarse es aqui, no dentro de seis meses
REM  reaceptando un rojo que ya no existe.
REM
REM  Y el motivo va con el nombre, en la misma linea, porque en batch no hay
REM  arrays asociativos. El texto se repite once veces, y es el precio de no
REM  tenerlos. Lo que NO se hace es dejar la lista en un segundo fichero que se
REM  pueda desincronizar de este.
REM ---------------------------------------------------------------------------
REM ---------------------------------------------------------------------------
REM  EL MOTIVO DE UN CONOCIDO
REM
REM  Devuelve en MOTIVO, o lo deja vacio si el test no esta en la lista. Un
REM  rojo desconocido NO es un error del script: sale como SIN CLASIFICAR, que
REM  es lo que tiene que pasar cuando no se sabe de quien es un rojo.
REM ---------------------------------------------------------------------------
REM ---------------------------------------------------------------------------
REM  VIVO: ¿existe todavia ese proceso?
REM
REM  La pregunta se hace a `tasklist` con filtro por PID. Ver la subrutina:
REM  `process.kill(pid, 0)`, que es la forma habitual, NO funciona en Windows.
REM ---------------------------------------------------------------------------
REM ---------------------------------------------------------------------------
REM  VIVO: ¿existe todavia ese proceso?
REM
REM  Es lo que decide si el lock es de un verify que esta corriendo o de una
REM  sesion muerta. Con esta mal, el script quita el lock de un verify en
REM  marcha, que es justo lo que el lock tiene que impedir.
REM
REM  Tres cosas, y las tres son trampas de batch, las tres medidas:
REM
REM  1. NINGUN `!` EN EL PROGRAMA DE NODE. Con EnableDelayedExpansion activo un
REM     `!` abre expansion, lo que sigue se busca en el entorno y sale vacio, y
REM     a node le llega el programa troceado. Medido:
REM         function vive(p){if('utf8'});return new RegExp(
REM         Expected ')', got '}'
REM     El primero era el de `if(!p||...)`. Por eso `if(p===''||...)`.
REM
REM  2. QUE NODE ESCRIBA EL FICHERO EL MISMO, con fs.writeFileSync, en vez de
REM     coger su stdout con `> fichero`. Medido: eso deja el fichero a CERO
REM     BYTES, con la redireccion delante o detras del comando.
REM
REM  3. NINGUN `\"` EN EL PROGRAMA: en batch un `\"` llega a node como barra
REM     invertida y comilla. Las comillas van con String.fromCharCode(34).
REM
REM  Y dos decisiones que no son accidentales:
REM
REM  - CON FILTRO `PID eq N`, y no la lista completa. La lista entera de este
REM    equipo tarda 210 s: son 356 procesos y el coste es de ahi para arriba.
REM    Con filtro son 400 ms. Y el filtro necesita execFileSync con ARRAY de
REM    argumentos: con execSync (que pasa por cmd) MSYS convierte `/fo` en una
REM    RUTA y tasklist contesta "Argumento u opcion no valido", lista vacia, y
REM    un proceso vivo parece muerto. Medido en los dos scripts.
REM  - El codigo de salida de tasklist es 0 tanto si el PID existe como si no,
REM    asi que no sirve: se mira si alguna linea empieza por comilla, que es lo
REM    que tiene el CSV de una tarea. El "no hay tareas que coincidan" es texto
REM    libre y depende del idioma del Windows.
REM  - Un PID de 0 no es un proceso, es la ausencia de PID: 0 es "no vivo".
REM
REM  Los dos scripts hacen la MISMA pregunta con el MISMO programa, para que no
REM  puedan dar distinta cuenta del mismo lock.
REM
REM  Y una cuarta trampa, del arnes que prueba esta subrutina por separado (no
REM  del script, que la llama con `call` y va bien): al pegar :vivo en un .bat
REM  propio hay que poner un `goto :main` ANTES, porque si no cmd ejecuta la
REM  subrutina de arriba abajo antes de llegar al `call`, y el `exit /b` del
REM  final termina el script entero. Medido: sin el, el .bat de prueba no
REM  imprime nada y parece que la subrutina no dice nada.
REM ---------------------------------------------------------------------------
:vivo
if "%~1"=="" exit /b 1
if "%~1"=="0" exit /b 1
del "%TEMP%\verify_vivo.txt" >nul 2>&1
REM El `>nul` del final no es cosmetico: sin el, el `false` que imprime el
REM programa sale por pantalla en mitad del aviso del lock rancio, y parece una
REM linea del script. El resultado se lee del fichero, no de la consola.
node "%NODELIB%" vivo "%~1" "%TEMP%\verify_vivo.txt" >nul 2>&1
set "VIVOOK="
if exist "%TEMP%\verify_vivo.txt" for /f "usebackq delims=" %%P in ("%TEMP%\verify_vivo.txt") do set "VIVOOK=%%P"
del "%TEMP%\verify_vivo.txt" >nul 2>&1
if "%VIVOOK%"=="true" exit /b 0
exit /b 1

:limpia_huerfanos
REM ---------------------------------------------------------------------------
REM  LOS PROCESOS QUE DEJO ESA SESION MUERTA
REM
REM  Cuando la sesion se cierra a mitad del verify, el lock se queda; y con el se
REM  quedan los procesos que ese verify habia lanzado: ctest, los tests que
REM  cuelgan de ctest, el npx de vitest. Quitar el lock y seguir como si nada
REM  deja a esos procesos escribiendo en el MISMO build-reference que el verify
REM  que acaba de empezar, y el rojo que sale diez minutos despues no tiene
REM  nada que ver con el codigo. Por eso, con el lock rancio, antes de
REM  continuar se cierran esos procesos y se ESPERA a que desaparezcan.
REM
REM  Y SOLO con el lock rancio, que es el unico caso en que esos procesos son
REM  huerfanos de verdad. Si el lock esta vivo no se llama: ese verify es de
REM  verdad y sus hijos son suyos. Quien llama ya ha comprobado que el PID esta
REM  muerto, y el programa lo vuelve a comprobar antes de tocar nada, por si el
REM  PID se ha reciclado entre medias.
REM
REM  COMO SE HACE, y por que este camino lleva tanto cuidado:
REM
REM  - El arbol de procesos se pide por WMI con `cscript`, NO con PowerShell. En
REM    esta maquina PowerShell tarda 6 s solo en arrancar, y 14 a 46 s en la
REM    consulta: medido. Con cscript + WMI son 0,8 s. `wmic` ya no viene en
REM    Windows 11, asi que tampoco es una salida.
REM  - `tasklist` SIN filtro no sirve: la lista entera de esta maquina no termina
REM    ni en 120 s (medido). Con filtro por PID son 0,9 s, y es lo que se usa:
REM    una llamada por proceso y por ronda de la espera.
REM  - El JScript va en un temporal que escribe node, y todas sus cadenas llegan
REM    como argumentos: JScript no tiene comillas simples, y un `"` dentro del
REM    programa parte el comando de este .bat en dos.
REM  - El PROGRAMA ES EL MISMO que usa el .sh, con la misma firma
REM    (pid, fichero, espera_ms), para que los dos scripts no puedan dar
REM    distinta cuenta del mismo lock. Mismo criterio que :vivo.
REM  - La salida es ok|hijos|muertos|resisten. Si `resisten` es mayor que cero,
REM    algo no se ha cerrado, y eso se avisa: puede que siga escribiendo en el
REM    build, que es justo lo que venia dejando sucio.
REM ---------------------------------------------------------------------------
set "RES="
set "NH="
set "NM="
set "NR="
del "%TEMP%\verify_huerf.txt" >nul 2>&1
node "%NODELIB%" limpia "%~1" "%TEMP%\verify_huerf.txt" "!LIMPIA_MS!" >nul 2>&1
if exist "%TEMP%\verify_huerf.txt" for /f "usebackq delims=" %%L in ("%TEMP%\verify_huerf.txt") do set "RES=%%L"
del "%TEMP%\verify_huerf.txt" >nul 2>&1
REM "ok|0|0|0": el arbol estaba vacio. Se dice, porque "no habia nada" y "no se
REM ha podido mirar" son cosas distintas y aqui se distinguen.
if "!RES!"=="ok|0|0|0" (
    echo           No habia ningun proceso vivo de esa sesion.
    exit /b 0
)
REM "vivo": entre la comprobacion del lock y esta llamada el PID ha vuelto a
REM existir (PID reciclado, o carrera). No se toca NADA.
if "!RES!"=="vivo" (
    echo           El pid %~1 ha vuelto a existir: no se toca nada, por si es otro proceso.
    exit /b 0
)
REM Las cuatro partes en una sola vuelta: ok|hijos|muertos|resisten
REM
REM SIN usebackq, a proposito. Con usebackq lo que va entre comillas es un
REM FICHERO, y `!RES!` no es un fichero: cmd avisa "no se puede encontrar el
REM archivo ok|2|2|0" y los tres numeros salen vacios, que es exactamente lo
REM que hace que el script acaba diga que no ha podido mirar el arbol. Medido.
REM Sin usebackq las comillas son una cadena, y el `|` va dentro de ella: el
REM parser lo respeta y parte bien. Y con `echo` en el `in (...)` tampoco vale:
REM la salida se vuelve a parsear como comando. Medido tambien.
for /f "tokens=1-4 delims=|" %%A in ("!RES!") do (
    set "NH=%%B"
    set "NM=%%C"
    set "NR=%%D"
)
if not defined NH (
    echo           %A%No se ha podido mirar el arbol de procesos ^(WMI^): no se cierra nada.%N%
    echo           Los procesos de esa sesion pueden seguir ahi.
    exit /b 0
)
if "!NR!"=="0" (
    echo           !NM! proceso^(s^) de esa sesion seguian vivos: cerrados y esperados.
) else (
    echo           %A%ATENCION%N% !NR! de !NH! proceso^(s^) de esa sesion no se han cerrado.
    echo           Van a seguir escribiendo en el build. Se mira con: tasklist /fi "PID eq N"
)
exit /b 0

:mi_pid
set "PIDLOCK=0"
node "%NODELIB%" pidpropio "%TEMP%\verify_mipid.txt" >nul 2>&1
for /f "usebackq delims=" %%P in ("%TEMP%\verify_mipid.txt") do set "PIDLOCK=%%P"
del "%TEMP%\verify_mipid.txt" >nul 2>&1
exit /b 0

:motivo_de
REM Devuelve en MOTIVO, o lo deja vacio si el test no esta en la lista. Un
REM rojo desconocido NO es un error del script: sale como SIN CLASIFICAR, que es
REM lo que tiene que pasar cuando no se sabe de quien es un rojo.
set "MOTIVO="
if not exist "%CONOCIDOJSON%" exit /b 0
for /f "usebackq delims=" %%L in (`node "%NODELIB%" motivo "%CONOCIDOJSON%" "%~1"`) do set "MOTIVO=%%L"
exit /b 0

:arreglados
REM ---------------------------------------------------------------------------
REM  LOS CONOCIDOS QUE YA NO FALLAN
REM
REM  La lista envejece, y una lista vieja que no dice nada es peor que no
REM  tenerla: al cabo de un mes tiene ocho entradas, la mitad ya no falla, y
REM  deja de ser informacion para ser ruido. Aqui se comprueba, para cada
REM  conocido, si esta en la lista de rojos de ESTA corrida. Si no esta, se ha
REM  arreglado, y el sitio para enterarse es aqui, no dentro de seis meses
REM  reaceptando un rojo que ya no existe.
REM
REM  Se recorre el MISMO JSON que :motivo_de, no una lista propia. Antes eran
REM  dos: una linea `if /i` por entrada para el motivo, y once `call` para esto.
REM  Once entradas por un lado y por otro, mantenidas a mano.
REM
REM  Y node IMPRIME el bloque entero, en vez de que batch recorra los nombres.
REM  Medido por que no se hace de la otra manera:
REM
REM    - Un `for /f` con backticks DENTRO de un `do ( ... )` no funciona: cmd
REM      no resuelve la sustitucion y acaba intentando abrir un fichero llamado
REM      `node -e ...`. Por eso :motivo_de va bien (su `for /f` esta a nivel de
REM      sentencia) y esta subrutina no (el suyo estaba en un bloque).
REM    - Quitar los `!` de la linea de node no arregla eso. No era el `!`.
REM    - Ademas, un `set /a` dentro de un bloque con `EnableDelayedExpansion`
REM      vuelve a abrir la puerta a los `!`.
REM
REM  Que lo imprima node no es un parche: node ya leyo el JSON y el log, y ya
REM  sabe cuales estan arreglados. Pedirle los nombres para que batch los
REM  recorra es tirar trabajo hecho.
REM
REM  `LastTestsFailed.log` solo dice quien fallo, no quien paso, asi que "no
REM  estar en la lista de rojos" es exactamente "haber pasado".
REM ---------------------------------------------------------------------------
if not exist "%CONOCIDOJSON%" exit /b 0
if not exist "%FALLOSLOG%" exit /b 0

node "%NODELIB%" arreglados "%CONOCIDOJSON%" "%FALLOSLOG%" "%A%" "%N%"
exit /b 0
:paso3
call :vitest "!RAIZ!\WebUI" WebUI 3
exit /b 0

:paso4
call :vitest "!ASSETS!" ABDSharedAssets 4
exit /b 0

:vitest
REM El NUMERO DE PASO tambien llega por valor, y antes no llegaba: la subrutina
REM anotaba con un 3 en un sitio y un 4 en el otro, fijos, siendo la misma
REM subrutina para los dos pasos de vitest. Medido con el check: sobre el mismo
REM build el .sh decia "rojo 3 WebUI" y el .bat decia "rojo 4 WebUI" para el
REM MISMO rojo, y el check los senalaba como divergentes. El numero del paso va
REM en la llamada, que es donde se sabe, y no dentro de la subrutina, que es
REM donde no se sabe.
REM La ruta llega por VALOR, y con expansion retardada: los dos `call` de
REM arriba viven dentro de `if not errorlevel 1 ( ... )`, y dentro de un
REM bloque parenthesizado `%VAR%` se expande al MONTAR el bloque, no al
REM ejecutar la linea. Con `%` el camino llegaba literal y `pushd` empujaba
REM a una carpeta que no existe, de modo que vitest se quedaba donde estaba:
REM medido, el paso 4 llevaba toda la sesion ejecutando vitest en
REM ABDNeural/Assets, que no tiene tests, en vez de en ABDSharedAssets, que
REM tiene 1327. Daba rojo, pero de otra cosa.
set "VDIR=%~1"
set "VETI=%~2"
set "VSTEP=%~3"
if not exist "%VDIR%" (
    echo   %R%NO HAY%N%  %VETI%: no existe %VDIR%
    call :anotar %VSTEP% "%VETI%" "el directorio no existe"
    set "SALIR_POR_ROJOS=1"
    exit /b 0
)
pushd "%VDIR%"
call npx vitest run > "%TEMP%\verify_vt.txt" 2>&1
set "VCOD=%ERRORLEVEL%"
popd
if "%VCOD%"=="0" (
    echo   %V%PASA%N%    %VETI%
    for /f "usebackq delims=" %%L in (`findstr /r /c:"^ *Test Files" /c:"^ *Tests" "%TEMP%\verify_vt.txt"`) do echo             %%L
) else (
    echo   %R%ROJO%N%    %VETI%
    set "SALIR_POR_ROJOS=1"
    set /a MOSTRAR=0
    for /f "usebackq delims=" %%L in (`findstr /r /c:"^ *x " /c:"^ *FAIL" "%TEMP%\verify_vt.txt"`) do (
        if !MOSTRAR! LSS 15 (
            echo             %%L
            set /a MOSTRAR+=1
        )
    )
    REM Este paso NO tiene lista de conocidos, y es deliberado: es la suite
    REM entera del repositorio de contratos, y una lista de "tests que no cuentan"
    REM aqui seria justo lo que este script no debe hacer. El motivo anterior
    REM apuntava a skins/index.js, del otro hilo, que puede que ya este
    REM arreglado: un motivo que describe un rojo que ya no existe ensena a
    REM culpar al sitio equivocado. Se deja el generico, que es lo que se
    REM puede decir sin inventar.
    call :anotar %VSTEP% "%VETI%" "vitest no arranca: el antivirus de esta maquina no deja leer los .js de node_modules (EPERM). Mismo fallo que los tres WebUi*E2e de ctest. Este paso no tiene lista de conocidos a proposito, asi que el rojo sigue pidiendo decision aunque el motivo este escrito"
)
exit /b 0

:paso5
call :contrato "catalogo de efectos contra fx-effects.json" "%RAIZ%\Tests\fxCatalogContractTest.mjs"
call :contrato "anclajes del selftest contra WebUI" "%RAIZ%\Tests\webuiSelftestContractTest.mjs"
if exist "%ASSETS%\tests\modulationMatrixContract.test.js" (
    if exist "%ASSETS%\node_modules" (
        echo   %V%PASA%N%    matriz de modulacion, cruzada con neuronik_modulation_matrix.json
        echo             incluida en el paso 4
        echo             ver modulationMatrixContract.test.js, que lee el catalogo de
        echo             destinos de ABDNeural a traves de la tabla compartida
    ) else (
        echo   %R%NO HAY%N%  matriz de modulacion: faltan sus node_modules
        call :anotar 5 "ModulationMatrixContract" "faltan los node_modules de ABDSharedAssets"
        set "SALIR_POR_ROJOS=1"
    )
) else (
    echo   %R%NO HAY%N%  matriz de modulacion: falta modulationMatrixContract.test.js
    call :anotar 5 "ModulationMatrixContract" "falta el test del contrato de modulacion"
    set "SALIR_POR_ROJOS=1"
)
exit /b 0

:contrato
set "CE=%~1"
set "CF=%~2"
node "%CF%" > "%TEMP%\verify_ct2.txt" 2>&1
if errorlevel 1 (
    echo   %R%ROJO%N%    %CE%
    set "SALIR_POR_ROJOS=1"
    set /a MOSTRAR=0
    for /f "usebackq delims=" %%L in (`findstr /c:"[FAIL]" /c:"FAIL" "%TEMP%\verify_ct2.txt"`) do (
        if !MOSTRAR! LSS 5 (
            echo             %%L
            set /a MOSTRAR+=1
        )
    )
    call :anotar 5 "%CE%" "el contrato cruzado no dice lo mismo que el otro repo"
) else (
    echo   %V%PASA%N%    %CE%
)
exit /b 0

:AYUDA
echo.
echo verify_all -- los cinco pasos de la verificacion de los dos repos.
echo.
echo   Scripts\verify_all.bat              los cinco pasos
echo   Scripts\verify_all.bat --no-build   salta el paso 1
echo   Scripts\verify_all.bat --only=3     un solo paso
echo.
echo Salidas: 0 todo en verde; 1 algun fallo; 2 uso incorrecto.
echo.
echo Salir en rojo con fallos de otro trabajo es INTENCIONAL: un verify que
echo dice "ok" con rojos dentro ensena a mirar el codigo de salida sin mirar la
echo salida. El informe del final dice de quien es cada fallo.
exit /b 0

REM ============================================================================
REM  ANOTAR UN FALLO
REM
REM  Centraliza el formato del informe. No por ahorrar lineas: porque las ocho
REM  anotaciones que habia carryban el separador escrito a mano, y en batch un
REM  tabulador pegado a una redireccion `>>fichero echo ...` parte la sentencia
REM  en dos y el parser se come un "(" suelto. Medido con el eco de comandos:
REM  "No se esperaba un en este momento".
REM
REM  El separador va en una variable, no literal: asi la linea no contiene
REM  ningun tabulador y el parser no tiene donde partirla.
REM
REM  Se entrecomilla la linea entera porque el motivo viene de texto ajeno (el
REM  log de ctest, los motivos de CONOCIDO) y un "&" o un ">" sin comillas
REM  ejecutaria el resto como si fuera una orden. El PowerShell del informe
REM  quita las comillas al leer.
REM ============================================================================
:anotar
set "SEP=	"
>>"%REPORT%" echo "%~1%SEP%%~2%SEP%%~3"
exit /b 0

REM ============================================================================
REM  EL INFORME
REM
REM  El recuento y el listado se hacen con PowerShell, no con for/f. La razon
REM  no es que PowerShell quede mas bonito: es que clasificar cada fallo
REM  leyendo un fichero de tres campos separados por tabuladores, en batch,
REM  son cuatro niveles de comillas y un tokenizer que parte el motivo en trozos.
REM  PowerShell hace exactamente lo mismo en cuatro lineas, lee el MISMO
REM  fichero temporal que escriben los pasos de arriba, y no tiene un caso
REM  especial por cada forma de fallar. Mismo dato, misma clasificacion y mismo
REM  codigo de salida que verify_all.sh.
REM ============================================================================
:INFORME
REM ---------------------------------------------------------------------------
REM  EL RESUMEN COMPARABLE, VOLCADO ANTES DE LAS DOS SALIDAS
REM
REM  El resumen va aqui, y no dentro de la rama de "sin fallos", por lo mismo
REM  que en el .sh: un resumen que solo existe cuando todo ha ido bien no se
REM  puede comparar con nada, que es justo el caso en el que mas hace falta
REM  mirarlo. Y va antes de los `del` de abajo, que se llevan `%REPORT%`.
REM
REM  Los dos ficheros de entrada son `%REPORT%` (los rojos, que :anotar ya
REM  tiene en el formato paso TAB test TAB motivo) y `!LENTOSFILE!` (los nombres
REM  de los lentos). Los dos los cuenta y clasifica verify_all_node.js, que es
REM  el MISMO programa que usa el .sh: si la cuenta la hiciera cada script por su
REM  cuenta, compararlos seria comparar dos implementaciones del mismo numero.
REM
REM  Los tiempos que ctest tardo NO van al resumen. Dos corridas seguidas dan
REM  tiempos distintos, y comparar tiempos haria que el check fallara por el
REM  motivo equivocado. El numero de SEGUNDOS esta en la pantalla, que es donde
REM  hace falta; el resumen lleva solo si un test es lento, no cuanto.
REM ---------------------------------------------------------------------------
node "%NODELIB%" resumen "%REPORT%" "%RESUMEN%" !NCONOCIDOS! !TOTAL! "!LENTOSFILE!" !COLGADOS! "!PASOS!" "!HUERFANOS!" >nul 2>&1
if errorlevel 1 echo   %A%Aviso: no se ha podido escribir el resumen en %RESUMEN%%N%
echo.
echo %T%================================================================================%N%
echo %T% INFORME DE FALLOS%N%
echo %T%================================================================================%N%

set "RESULTADO="
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$f='%REPORT%'; $t='%T%'; $n='%N%'; $r='%R%'; $v='%V%'; $a='%A%';" ^
  "$v='%TEMP%\verify_sin_fallos';" ^
  "Remove-Item $v -ErrorAction SilentlyContinue;" ^
  "if (-not (Test-Path $f)) { Set-Content -Path $v -Value 'x'; Write-Output '';" ^
  "  Write-Output '  Sin fallos: no hay nada que listar.'; exit 0 };" ^
  "$l=@(Get-Content $f | Where-Object { $_.Trim() -ne '' });" ^
  "if ($l.Count -eq 0) { Set-Content -Path $v -Value 'x'; Write-Output '';" ^
  "  Write-Output '  Sin fallos: los pasos que se han ejecutado estan en verde.';" ^
  "  exit 0 };" ^
  "$mios=0; $ajenos=0; $sin=0;" ^
  "foreach ($x in $l) {" ^
  "  $p=$x -split [char]9;" ^
  "  $paso=$p[0].Trim([char]34); $test=$p[1].Trim([char]34);" ^
  "  $motivo=(($p[2..($p.Count-1)] -join ' ').Trim([char]34));" ^
  "  if ($motivo -like 'MIO*') { $et='MIO'; $mios++ }" ^
  "  elseif ($motivo -like 'ajeno:*') { $et='AJENO'; $ajenos++ }" ^
  "  else { $et='SIN CLASIFICAR'; $sin++ };" ^
  "  Write-Output '';" ^
  "  Write-Output ('  [' + $et + '] paso ' + $paso + '  ' + $test);" ^
  "  Write-Output ('        ' + $motivo);" ^
  "};" ^
  "Write-Output '';" ^
  "Write-Output ('  --------------------------------------------------------------------------');" ^
  "Write-Output ('  ' + $l.Count + ' fallos: ' + $mios + ' mios, ' + $ajenos + ' de otro trabajo, ' + $sin + ' sin clasificar.');" ^
  "if ($mios -gt 0) { Write-Output ('  ' + $r + 'HAY FALLO(S) DE ESTE TRABAJO.' + $n + ' El comando sale en rojo.') }" ^
  "else { Write-Output '  Ningun fallo es de este trabajo, pero el comando sale en ROJO igualmente.';" ^
  "        Write-Output '  No porque los rojos ajenos cuenten: porque un verify que dice \"ok\" con';" ^
  "        Write-Output '  rojos dentro ensena a mirar el codigo de salida sin mirar la salida, y';" ^
  "        Write-Output '  el dia que aparezca un rojo nuevo se confunde con el ruido de siempre.' };" ^
  "Write-Output '  Arregla los tuyos. Los ajenos, decide uno a uno: la lista de';" ^
  "Write-Output '  conocidos de este script dice de quien es cada uno y por que.';" ^
  "Write-Output '';" ^
  "Write-Output ($t + '================================================================================' + $n)" > "%TEMP%\verify_informe.txt" 2>&1

type "%TEMP%\verify_informe.txt"

REM La senal de "no hay nada que listar" va en un FICHERO, no en la salida. La
REM primera version la comunicaba con una palabra clave escrita en el informe,
REM y cualquier palabra clave escrita en el informe se acaba leyendo en
REM pantalla: la que se puso, "__SIN_FALLOS__", salia debajo del todo.
REM Un fichero que esta o no esta no se ensucia con el formato del informe.
if exist "%TEMP%\verify_sin_fallos" (
    del "%REPORT%" >nul 2>&1
    del "%LENTOSFILE%" >nul 2>&1
    del "%TEMP%\verify_informe.txt" >nul 2>&1
    del "%TEMP%\verify_sin_fallos" >nul 2>&1
    del "%LOCK%\pid" >nul 2>&1
del "%TEMP%\verify_mipid.txt" >nul 2>&1
rmdir "%LOCK%" >nul 2>&1
    exit /b 0
)

del "%REPORT%" >nul 2>&1
del "%LENTOSFILE%" >nul 2>&1
del "%TEMP%\verify_informe.txt" >nul 2>&1
del "%TEMP%\verify_sin_fallos" >nul 2>&1
del "%LOCK%\pid" >nul 2>&1
del "%TEMP%\verify_mipid.txt" >nul 2>&1
rmdir "%LOCK%" >nul 2>&1
exit /b 1

:muestra
echo.
echo     [%%~1] paso %%~2
exit /b 0
