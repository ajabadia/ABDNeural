@echo off
REM ============================================================================
REM  junctions-workspace.bat - deja los enlaces del WORKSPACE como JUNCTIONS.
REM
REM  QUE HACE Y POR QUE
REM
REM  Los paquetes del workspace (`@abdsynths/shared` y `@abdsynths/midi-keyb`)
REM  no se Bajan: son enlaces a los repos de al lado. Esos enlaces los escribe
REM  pnpm, y en esta maquina los escribe con la ruta TAL CUAL la tiene el shell,
REM  o sea POSIX (`/d/tmp/ws12/ABDSharedAssets`). Node y Vite la leen como
REM  Windows y empiezan por `D:\`, con lo cual apuntan a un sitio que no existe.
REM
REM  El sintoma NO lo dice el enlace: el paquete parece estar y simplemente no
REM  resuelve. Medido el 2026-10-02:
REM
REM    [vite]: Rollup failed to resolve import "@abdsynths/shared/components/wheel.js"
REM      from ".../ABDSharedCode/MidiKeyboard/src/keyboard.js"
REM
REM  que sale en el paso 4 de build.bat ("Fallo la exportacion de la WebUI"), a
REM  nueve minutos de compilar, por un enlace de cuatro lineas. Y lo mas caro:
REM  el fallo depende de QUIEN mire primero. La pagina entra por MidiKeyboard, y
REM  ahi el enlace roto gana a la junction buena de WebUI/node_modules, asi que
REM  un paquete que se resuelve bien desde la pagina se deja de resolver en el
REM  mismo build. Por eso el script repite el arreglo en TODOS los repos.
REM
REM  LA OTRA MITAD DEL MISMO FALLO: EL node_modules ENTERO
REM
REM  Un symlink POSIX no solo cae donde toca el paquete. Medido el mismo dia en
REM  ABDSharedAssets: `node_modules` entero era un symlink POSIX a
REM  `node_modules_ok\node_modules`, o sea una COPIA del proyecto entero. Todo
REM  parecia en su sitio y no habia ni vitest dentro, asi que el paso 4 del
REM  verify caia con un rojo que no senalaba ni al enlace ni al store.
REM
REM  Un enlace de esa clase NO lo arregla este script -arreglarlo es
REM  reinstalar-, pero si se puede avisar, y eso es lo que hace el testigo de
REM  mas abajo. El aviso CUENTA como fallo a proposito: cuando se llega al paso
REM  4 de build.bat ya se han gastado nueve minutos, y un corte aqui con el
REM  motivo a la vista sale mas barato que un rojo sin pistas al final.
REM
REM  POR QUE JUNCTION Y NO SYMLINK
REM
REM  `mklink /D` (symlink) pide privilegios de administrador y falla. `mklink /J`
REM  (junction) no los pide y es lo que Windows usa de nativo para esto.
REM
REM  QUE HACE CUANDO HAY ALGO QUE NO PUEDE BORRAR
REM
REM  Si el enlace viejo no se puede borrar (error 5, el mismo del store v10 que
REM  documenta el cuarto atranco), se APARTA con un `ren` - renombrar si funciona
REM  cuando borrar no - y se avisa. Los apartados son `*.roto`, y se pueden
REM  borrar luego a mano; son enlaces, no paquetes, asi que no ocupan nada.
REM
REM  USO
REM
REM    Scripts\junctions-workspace.bat
REM
REM  Es idempotente: si el enlace ya resuelve, lo dice y no lo toca. Se puede
REM  ejecutar las veces que haga falta, y hay que ejecutarlo DESPUES de cada
REM  `pnpm install`, porque ese es el que los vuelve a escribir como symlink.
REM  build.bat ya lo llama solo antes del paso 3.
REM ============================================================================
setlocal EnableExtensions EnableDelayedExpansion

REM %~dp0 trae la barra final, y a esa hay que quitarle el `..` de la carpeta
REM Scripts. Se hace en dos pasos y con `cd` porque es la unica forma de que
REM quede una ruta YA NORMALIZADA: a mano, el WebUI salia impreso como
REM `...\Scripts\..\WebUI`, que funciona pero ensucia cada mensaje.
set "RAIZ=%~dp0.."
if "%RAIZ:~-1%"=="\" set "RAIZ=%RAIZ:~0,-1%"
pushd "%RAIZ%"
set "RAIZ=%CD%"
popd

REM DONDE ESTA LA SUITE, y por que es una variable de entorno y no una
REM constante a pelo.
REM
REM En local es `D:\desarrollos\ABDSynths`, y ese valor esta aqui por defecto para
REM que el script siga funcionando sin que nadie lo configure. PERO en CI no es
REM ese sitio: el workflow clona los hermanos como subdirectorios sueltos de
REM `$GITHUB_WORKSPACE` (ABDNeural, ABDSharedCode, ABDSharedAssets...), y
REM MEDIDO lo que pasaba sin esto: el script resolvia `%SUITE%` a la ruta
REM local, no encontraba ahi a los hermanos del runner, se saltaba los cinco, y
REM aun asi imprimia
REM
REM     RESULTADO: OK, 6 repos revisados y todos los enlaces resuelven
REM
REM o sea un verde que no habia mirado nada. Peor que un rojo: en CI eso deja
REM pasar un enlace roto sin que nadie mire, que es justo lo que se quiere evitar.
REM
REM `NEURONIK_SUITE` lo pone el workflow antes de llamar a este script. Con las
REM variables de DelayedExpansion, `%NEURONIK_SUITE:~1%` es un error de parseo
REM cuando la variable NO esta definida, asi que el `if defined` va antes y la
REM expansion con exclamaciones es la que no peta.
REM OJO con la barra final: el `workflow` pasa `$GITHUB_WORKSPACE`, que NO
REM lleva barra, y quitarle el ULTIMO CARACTER a pelo rompia el nombre. MEDIDO al
REM probar esto con un workspace que se llamaba `ci`: `%VAR:~0,-1%` lo dejo en
REM `D:\tmp\rev\c`, y el script fue a buscar los hermanos ahi, que no existe.
REM Por eso se quita la barra SOLO si la hay, en vez de quitar un caracter.
if defined NEURONIK_SUITE (
    set "SUITE=%NEURONIK_SUITE%"
    if "!SUITE:~-1!"=="\" set "SUITE=!SUITE:~0,-1!"
) else (
    set "SUITE=D:\desarrollos\ABDSynths"
)
set "SHARED=%SUITE%\ABDSharedAssets"
set "TECLADO=%SUITE%\ABDSharedCode\MidiKeyboard"

echo === Enlaces del workspace como junctions ===
echo.

set "FALLOS=0"
set "REVISADOS=0"

REM ---------------------------------------------------------------------------
REM LOS REPOS QUE SE REVISAN
REM
REM Son los cinco miembros del workspace de pnpm (pnpm-workspace.yaml, en
REM D:\desarrollos\ABDSynths) mas el WebUI de NEURONiK, que lleva su propio
REM workspace. No es un capricho recorrerlos todos: el dia 2026-10-02 el que
REM rompia el paso 4 del verify era ABDSharedAssets, que no aparece en ningun
REM sitio de este repo y al que solo se llega al final.
REM
REM Y LA LISTA TIENE QUE MANTENERSE AL DIA al anadir un repo al workspace. Es
REM el unico sitio donde eso se ve: un miembro nuevo que no este aqui se queda
REM sin revisar y vuelve a poder romper su build sin que nadie mire.
REM
REM Las llamadas van una por repo y SIN bucle a proposito: un `for` con una
REM lista dentro de una variable, con las comillas y el parentesis que lleva, lo
REM parsea mal el cmd ("No se esperaba y en este momento"). Con seis lineas no
REM hay nada que interpretar, y la lista se lee de un vistazo, que es lo que
REM importa cuando hay que anadir un repo.
REM ---------------------------------------------------------------------------
REM DOS MODOS, y por que hacen falta dos. El de por defecto es el de la maquina
REM de desarrollo, donde estan los seis repos y si falta uno es que se borro
REM (y eso se avisa en rojo). En CI no: el workflow clona SOLO los hermanos que
REM cada job necesita, y MEDIDO el 2026-10-03: en el job de la regresion visual
REM no estan ABDMS2000, ABDCZ101 ni ABDEep, con lo cual el paso de CI fallaria
REM SIEMPRE con un rojo de "falta el repo" en un job donde el repo no hace falta.
REM Un rojo siempre es un job caido.
REM
REM `NEURONIK_CI=1` dice "solo mira los repos que esten". Los que falten se
REM cuentan como Saltados y salen en el resumen, para que se vea que no se han
REM mirado en vez de no aparecer. Los que ESTEN se comprueban igual que en local,
REM que es lo que importa: el enlace roto es del repo que esta, no del que falta.
if defined NEURONIK_CI (
    set "SOLO_LO_QUE_EXISTE=1"
) else (
    set "SOLO_LO_QUE_EXISTE=0"
)
set "SALTADOS=0"
set "HOOKS_PUESTOS=0"
set "HOOKS_YA_ESTABAN=0"

call :repos "%RAIZ%\WebUI"
call :repos "%SUITE%\ABDSharedAssets"
call :repos "%SUITE%\ABDSharedCode\MidiKeyboard"
call :repos "%SUITE%\ABDMS2000"
call :repos "%SUITE%\ABDCZ101"
call :repos "%SUITE%\ABDEep"

call :hook_bat "%RAIZ%"
call :hook_bat "%SUITE%\ABDSharedAssets"
call :hook_bat "%SUITE%\ABDSharedCode\MidiKeyboard"
call :hook_bat "%SUITE%\ABDMS2000"
call :hook_bat "%SUITE%\ABDCZ101"
call :hook_bat "%SUITE%\ABDEep"

echo.
REM Sin parentesis en NINGUN texto de este bloque, y el comentario que hay aqui
REM lo dice porque ya se ha pagado: dentro de un `if ( ... )` un parentesis sin
REM escapar rompe el parseo entero, y el error sale como "No se esperaba <algo>
REM en este momento" en una linea que no tiene nada que ver. Se fijo primero
REM `repo(s)` y el parseo se rompio con
REM
REM   No se esperaba saltados en este momento
REM
REM o sea el fallo aparecio blaming a la VARIABLE que se estaba imprimiendo, y
REM no al parentesis que lo causaba. Por eso el conteo de saltados va con
REM etiquetas y no con un `if` anidado.
if not "%FALLOS%"=="0" goto resumen_con_problemas

echo RESULTADO: OK, %REVISADOS% repos revisados y todos los enlaces del
echo          workspace resuelven.
if "%SALTADOS%"=="0" goto resumen_ok
echo          %SALTADOS% repos saltados por NEURONIK_CI: no estaban en esta maquina.
if "%HOOKS_PUESTOS%"=="0" goto resumen_sin_hooks
echo          %HOOKS_PUESTOS% hook(s) de .bat instalados en esta corrida.
:resumen_sin_hooks
:resumen_ok
endlocal
exit /b 0

:resumen_con_problemas
echo RESULTADO: %FALLOS% problemas sin resolver. Mira las lineas de arriba.
endlocal
exit /b 1

:hook_bat
REM %1 = la raiz de un repo. Le instala el hook de los .bat si lo trae.
REM
REM QUE HACE Y POR QUE
REM
REM `core.hooksPath` es config LOCAL de cada clon: no se commitea y no viaja
REM con el repositorio. MEDIDO el 2026-10-03: con el hook puesto en el repo y
REM sin esto, un clon nuevo se commitea igual, y el `.bat` con LF se cuela sin
REM que nadie se entere, que es justo el fallo que el hook existe para cazar.
REM
REM Solo se instala donde el repo trae Scripts\hooks\pre-commit versionado.
REM En este workspace solo ABDNeural lo trae hoy; el resto no imprime nada y
REM no es un fallo. Instalar el hooksPath en un repo sin hook no rompe nada,
REM pero tampoco vigila nada, asi que se comprueba y se sale.
REM
REM IDEMPOTENTE, que es lo que tiene que ser un script que se puede correr mil
REM veces: si ya apunta ahi, no se repite nada.
set "HR=%~1"
if not exist "!HR!\Scripts\hooks\pre-commit" exit /b 0
if not exist "!HR!\.git" exit /b 0

set "HACTUAL="
for /f "usebackq delims=" %%H in (`git -C "!HR!" config --get core.hooksPath 2^>nul`) do set "HACTUAL=%%H"
if "!HACTUAL!"=="Scripts/hooks" goto hook_ya_estaba

git -C "!HR!" config core.hooksPath Scripts/hooks >nul 2>&1
if errorlevel 1 goto hook_no_puesto

REM Se vuelve a LEER, no se da por hecho. MEDIDO el 2026-10-03 que un config
REM puede salir con 0 y no haber escrito nada, y un hook que se dice puesto y
REM no lo esta es peor que uno que no dice nada.
set "HACTUAL="
for /f "usebackq delims=" %%H in (`git -C "!HR!" config --get core.hooksPath 2^>nul`) do set "HACTUAL=%%H"
if not "!HACTUAL!"=="Scripts/hooks" goto hook_no_puesto

set /a HOOKS_PUESTOS+=1
echo   [HOOK]    El hook de los .bat instalado en !HR!
exit /b 0

:hook_ya_estaba
set /a HOOKS_YA_ESTABAN+=1
exit /b 0

:hook_no_puesto
REM El repo trae el hook y no se ha podido instalar: es un FALLO y no un aviso.
REM A partir de aqui los .bat de ese repo entran sin vigilancia, y el rojo
REM sale despues, en el build, sin que nadie lo relacione con esto.
echo   [ERROR]   !HR! trae el hook de los .bat y no se ha podido instalar.
echo             git config core.hooksPath Scripts/hooks dentro de ese repo.
set /a FALLOS+=1
exit /b 1


:repos
REM %1 = la raiz de un repo
set "REPO=%~1"

if not exist "%REPO%\package.json" goto no_es_repo

set /a REVISADOS+=1
set "NM=!REPO!\node_modules"

echo.
echo   [REPO]   !REPO!

REM ---------------------------------------------------------------------------
REM TESTIGO 1: LA CARPETA node_modules ES UN ENLACE
REM
REM El testigo NO es buscar un package.json dentro: no lo tiene NINGUNA
REM instalacion buena -pnpm deja ahi .pnpm, .modules.yaml y .bin, y un
REM package.json dentro de node_modules solo si alguien lo pone a mano-. Con
REM ese testigo el script se quejaba de los seis repos con todo en su sitio.
REM
REM El testigo bueno es si la CARPETA EN SI es un punto de reanilisis. Lo dice
REM `fsutil reparsepoint query`: sale con 0 si la ruta es un enlace -junction o
REM symlink, los dos- y con error 4390 si es una carpeta de verdad. Medido el
REM 2026-10-02 sobre los seis repos: los seis dan 4390, o sea ninguno tiene el
REM node_modules enlazado.
REM ---------------------------------------------------------------------------
call :es_enlace "!NM!"
if not errorlevel 1 goto nm_enlazado

REM ---------------------------------------------------------------------------
REM TESTIGO 2: HAY CARPETA PERO NO HAY INSTALACION DENTRO
REM
REM Una instalacion de pnpm siempre deja `.bin` (los ejecutables) y las que
REM son workspace-linked dejan ademas `.pnpm`. Si no esta ninguno de los dos
REM hay una carpeta con cosas dentro pero sin install, que es como se
REM manifiesta un node_modules a medias. Un repo que ni siquiera tiene la
REM carpeta no se toca: todavia no se ha instalado, y no es un problema.
REM
REM Y los dos testigos cortan aqui, sin mirar los enlaces de @abdsynths. Meter
REM junctions dentro de un node_modules que no es una carpeta de modulos solo
REM produce ruido: la junction se crea en un sitio que no es el que parece y
REM el paso siguiente falla con "la junction no ha quedado bien", un error que
REM no senala la causa. Si hay que arreglar ese node_modules, la causa esta en
REM el aviso de arriba, no en los enlaces.
REM ---------------------------------------------------------------------------
if not exist "!NM!" exit /b 0
if exist "!NM!\.bin" goto enlaces_de_este_repo
if exist "!NM!\.pnpm" goto enlaces_de_este_repo
goto nm_a_medias

:no_es_repo
REM Un repo de la lista que no esta se salta en silencio en MODO CI, y es un
REM ROJO en el modo de por defecto. El enrutado va PRIMERO porque es lo unico
REM que decide entre las dos, y usa `!VAR!` a proposito: dentro de un bloque,
REM `%VAR%` se expande al analizar la linea y llegaria vacio.
if "!SOLO_LO_QUE_EXISTE!"=="1" goto repo_saltado

REM En el modo de POR DEFECTO, un repo de la lista que no esta es un ROJO, y se
REM cambio el 2026-10-03 por un motivo medido. Ese dia se quiso meter este script
REM en el workflow de CI, donde el layout es OTRO, y al simularlo salio esto:
REM
REM   [REPO]   D:\tmp\rev\ci\ABDNeural\WebUI
REM   [REPO]   D:\desarrollos\ABDSynths\ABDSharedAssets
REM   [REPO]   D:\desarrollos\ABDSynths\ABDCZ101      <- no existe en el layout de CI
REM   ...
REM   RESULTADO: OK, 6 repos revisados y todos los enlaces del workspace resuelven
REM
REM O sea: los repos que no estaban NO se miraron, y el resumen seguia diciendo
REM "6 repos revisados" y "OK". En el runner de CI eso es un verde que no ha
REM mirado nada, que es peor que un rojo: deja pasar un enlace roto sin que nadie
REM lo note. Con el rojo, el layout equivocado se dice en voz alta.
REM
REM La lista de `call :repos` es la declaracion de que repos existen en esta
REM maquina. Si uno no esta, o la maquina no es la que el script cree, o el repo
REM se borro, y las dos cosas se dicen en vez de desaparecer en el recuento.
set /a FALLOS+=1
echo   [ERROR]   !REPO! no tiene package.json, y estaba en la lista de repos
echo             que este script revisa. O no es la maquina que el script cree, o
echo             el repo se borro. Si el repo es de otro layout y aqui no aplica,
echo             quitalo de la lista de `call :repos` que hay arriba.
exit /b 1

:repo_saltado
REM En modo CI (`NEURONIK_CI=1`) un repo de la lista que no esta se salta, y se
REM cuenta aparte para que el resumen diga cuantos se han quedado sin mirar. Es
REM un AVISO y no un fallo, y no por cortesia: el job de CI clona a proposito
REM solo los hermanos que usa, asi que un repo ausente ahi es lo normal y no un
REM problema de nadie.
set /a SALTADOS+=1
echo   [SALTADO]  !REPO! no esta en esta maquina; se salta (NEURONIK_CI=1).
exit /b 0

:nm_enlazado
echo   [AVISO]   !NM! es un ENLACE, no una carpeta de modulos. Asi se rompio
echo             el paso 4 el 2026-10-02: un symlink POSIX a una copia del
echo             proyecto entero, sin vitest dentro y sin nada que lo dijera en
echo             el rojo. Este script no arregla eso -arreglarlo es reinstalar-:
echo             borra el enlace con `rmdir /q` y luego reinstalla con el rodeo
echo             de Scripts\COMO-ARREGLAR-EL-BUILD.md.
set /a FALLOS+=1
exit /b 0

:nm_a_medias
echo   [AVISO]   !NM! existe pero no tiene ni .bin ni .pnpm: parece una
echo             instalacion a medias. Reinstala antes de fiarte de este repo.
set /a FALLOS+=1
exit /b 0

:enlaces_de_este_repo

REM ---------------------------------------------------------------------------
REM LOS ENLACES DE CADA REPO, Y SOLO LOS QUE ESE REPO CONSUME
REM
REM La lista es de paquetes, no de enlaces ya vistos, para que anadir un
REM paquete nuevo al workspace sea solo anadirlo aqui. Pero un paquete se
REM comprueba contra el package.json del repo ANTES de tocar nada, y si el repo
REM no lo declara no se hace NADA: ni crear, ni avisar.
REM
REM El filtro no es cosmetico. Sin el, el 2026-10-02 este script creo cuatro
REM junctions que no hacia falta: ABDSharedAssets se enlazo a si mismo y a
REM MidiKeyboard, y MidiKeyboard se enlazo a si mismo por su propio nombre de
REM paquete. Un repo que se enlaza a si mismo a travers de su node_modules es
REM un bucle de resolucion de modulos esperando a que algo lo recorra, y lo
REM de mas es un aviso que hay que aprender a ignorar.
REM
REM OJO con el patron, que es donde esta el detalle fino. El nombre del paquete
REM se busca como CLAVE de dependencia, o sea `"@abdsynths/shared":` con la
REM coma y los dos puntos de detras. Sin los dos puntos el filtro no filtra
REM nada: el nombre aparece tambien en el campo "name" del propio repo
REM -ABDSharedAssets se llama a si mismo @abdsynths/shared- y se enlazaba a si
REM mismo. Y con barra invertida en vez de normal `findstr` no encuentra nada
REM nunca, que es el otro jeito de tener un filtro que no filtra.
REM ---------------------------------------------------------------------------
call :asegura "!REPO!" "shared" "%SHARED%"
call :asegura "!REPO!" "midi-keyb" "%TECLADO%"
exit /b 0

:es_enlace
REM %1 = ruta. Devuelve 0 si es un punto de reanilisis, 1 si no lo es.
fsutil reparsepoint query "%~1" >nul 2>&1
exit /b %ERRORLEVEL%

:declara
REM %1 = repo, %2 = nombre de paquete. Devuelve 0 si lo declara COMO DEPENDENCIA.
REM
REM El patron lleva los dos puntos porque se busca la clave de dependencies, no
REM el nombre del repo. Medido el 2026-10-02 con este patron:
REM
REM   repo          shared   midi-keyb
REM   WebUI         si       si
REM   ABDSharedAssets no     no
REM   MidiKeyboard  si       no
REM   ABDMS2000     si       si
REM   ABDCZ101      si       si
REM   ABDEep        si       si
findstr /c:"\"@abdsynths/%~2\":" "%~1\package.json" >nul 2>&1
exit /b %ERRORLEVEL%

:asegura
REM %1 = repo, %2 = carpeta dentro de @abdsynths, %3 = repo destino
set "REPO=%~1"
set "NOMBRE=%~2"
set "DESTINO=%~3"
set "NM=%REPO%\node_modules"
set "ENLACE=%NM%\@abdsynths\%NOMBRE%"

REM La subrrutina va con ETIQUETAS y no con `if/else` anidados a proposito: dentro
REM de un bloque, `%VAR%` se expande al analizar la linea, no al ejecutarla, asi
REM que un `set` hecho en la misma rama llega como vacio. Con etiquetas y `!VAR!`
REM no hay nada que expandir antes de tiempo.

REM Si el repo no declara el paquete, no se toca nada. Este `if` tiene que
REM quedar ANTES del de "no hay node_modules" para que un repo que no lo
REM declara salga limpio en vez de avisar de algo que no le pasa.
call :declara "!REPO!" "!NOMBRE!"
if errorlevel 1 goto pasa

REM Si la carpeta node_modules no esta, no hay nada que arreglar: todavia no se
REM ha instalado, o se esta instalando.
if not exist "!NM!" goto pasa

REM Ya resuelve: no se toca. Un enlace roto tampoco existe a traves de si
REM mismo, asi que esta comprobacion vale para los dos casos.
if exist "!ENLACE!\package.json" goto ok

if exist "!ENLACE!" goto roto
goto nuevo

:pasa
exit /b 0

:ok
exit /b 0

:nuevo
echo   [NUEVO]   !NOMBRE! en !NM! no estaba; se crea la junction.
goto creado

:roto
echo   [ROTO]    !NOMBRE! en !NM! no resuelve.
set "APARTADO=!ENLACE!.roto"

REM Un apartado de una corrida anterior, si lo hay, se va antes: si tampoco se
REM puede, se avisa y se sigue, porque el nombre exacto da igual para el enlace.
if not exist "!APARTADO!" goto apartar
rmdir /q "!APARTADO!" >nul 2>&1
if exist "!APARTADO!" echo   [AVISO]   No se puede quitar el apartado previo "!APARTADO!". Quitalo a mano.

:apartar
REM El enlace roto se borra como lo que es: un enlace. `rmdir` quita junctions y
REM symlinks de Windows sin pedir nada, y es lo que funciona casi siempre. El
REM `ren` va de SEGUNDO porque sobre una junction no cuela, y solo se llega a el
REM cuando el enlace no se deja borrar.
rmdir /q "!ENLACE!" >nul 2>&1
if not exist "!ENLACE!" goto creado

set "APARTADO2=!ENLACE!.roto2"
ren "!ENLACE!" "!APARTADO2!" >nul 2>&1
if exist "!ENLACE!" goto no_se_aparta
echo             El enlace roto se ha apartado a "!APARTADO2!" (se puede borrar).
goto creado

:no_se_aparta
echo   [ERROR]   Ni borrando ni renombrando se ha podido quitar el enlace roto,
echo             asi que la junction no se puede poner encima. El store v10
echo             bloqueado (error 5, el del cuarto atranco) suele ser la causa:
echo             renombra la CARPETA node_modules entera, que si se deja renombrar.
set /a FALLOS+=1
exit /b 1

:creado
if not exist "!DESTINO!" goto destino_ausente
goto hacer

:destino_ausente
echo   [ERROR]   El destino !DESTINO! no existe: no hay nada a lo que enlazar.
set /a FALLOS+=1
exit /b 1

:hacer
mklink /J "!ENLACE!" "!DESTINO!" >nul 2>&1
if not exist "!ENLACE!\package.json" goto junction_mala
echo   [LISTO]   !NOMBRE! en !NM! ^-^> !DESTINO!
exit /b 0

:junction_mala
echo   [ERROR]   La junction no ha quedado bien. Comprueba que el destino es un repo con package.json.
set /a FALLOS+=1
exit /b 1
