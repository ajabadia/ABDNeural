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
REM  mismo build. Por eso el script repite el arreglo en los DOS node_modules.
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
REM ============================================================================
setlocal EnableExtensions EnableDelayedExpansion

set "RAIZ=%~dp0.."
if "%RAIZ:~-1%"=="\" set "RAIZ=%RAIZ:~0,-1%"

set "SHARED=D:\desarrollos\ABDSynths\ABDSharedAssets"
set "TECLADO=D:\desarrollos\ABDSynths\ABDSharedCode\MidiKeyboard"

echo === Enlaces del workspace como junctions ===
echo.

set "FALLOS=0"

REM %1 = la carpeta node_modules donde vive el enlace
REM %2 = el nombre del paquete dentro de @abdsynths
REM %3 = el repo al que tiene que apuntar
call :asegura "%RAIZ%\WebUI\node_modules" "shared" "%SHARED%"
call :asegura "%RAIZ%\WebUI\node_modules" "midi-keyb" "%TECLADO%"
call :asegura "%TECLADO%\node_modules" "shared" "%SHARED%"

echo.
if "%FALLOS%"=="0" (
    echo RESULTADO: OK, todos los enlaces del workspace resuelven.
    endlocal
    exit /b 0
)

echo RESULTADO: %FALLOS% enlace(s) sin resolver. Mira las lineas de arriba.
endlocal
exit /b 1

:asegura
set "NM=%~1"
set "NOMBRE=%~2"
set "DESTINO=%~3"
set "ENLACE=%NM%\@abdsynths\%NOMBRE%"

REM La subrrutina va con ETIQUETAS y no con `if/else` anidados a proposito: dentro
REM de un bloque, `%VAR%` se expande al analizar la linea, no al ejecutarla, asi
REM que un `set` hecho en la misma rama llega como vacio. Con etiquetas y `!VAR!`
REM no hay nada que expandir antes de tiempo.

REM Si la carpeta node_modules no esta, no hay nada que arreglar: todavia no se
REM ha instalado, o se esta instalando.
if not exist "!NM!" goto pasa

REM Ya resuelve: no se toca. Un enlace roto tampoco existe a traves de si
REM mismo, asi que esta comprobacion vale para los dos casos.
if exist "!ENLACE!\package.json" goto ok

if exist "!ENLACE!" goto roto
goto nuevo

:pasa
echo   [SE PASA] !NOMBRE! - no hay node_modules en esta carpeta.
exit /b 0

:ok
echo   [OK]      !NOMBRE! en !NM! ya apunta a !DESTINO!
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
if not exist "!DESTINO!" (
    echo   [ERROR]   El destino !DESTINO! no existe: no hay nada a lo que enlazar.
    set /a FALLOS+=1
    exit /b 1
)

mklink /J "!ENLACE!" "!DESTINO!" >nul 2>&1
if not exist "!ENLACE!\package.json" (
    echo   [ERROR]   La junction no ha quedado bien. Comprueba que el destino es un repo con package.json.
    set /a FALLOS+=1
    exit /b 1
)

echo   [LISTO]   !NOMBRE! en !NM! ^-^> !DESTINO!
exit /b 0