@echo off
REM ============================================================================
REM  verify_all_check.bat -- ?DICEN LO MISMO EL .sh Y EL .bat?
REM
REM  Este fichero NO reimplementa el check: lo delega en verify_all_check.sh.
REM
REM  Y no es pereza, es la unica forma sensata. El check tiene que lanzar LOS
REM  DOS gemelos, y uno de ellos es el .sh, que necesita bash. Un .bat que
REM  checkeara sin bash no podria lanzar la mitad de lo que hay que comparar, y
REM  un check que compara medio lo que dice no es un check: es un check que
REM  sale en verde con la mitad sin mirar. Ademas, en batch no hay
REM  `printf %s\n`, ni `awk` para recortar el nombre de un lento, ni arrays:
REM  el check tendria que ir contra los mismos ficheros con las mismas
REM  trampas de comillas, y su copia se desincronizaria de la del .sh sin que
REM  nada lo dijera. Que es exactamente el problema que este check existe para
REM  cazar, freshness incluida: escribir dos veces el check es la forma mas
REM  rapida de tener dos checks que no coinciden.
REM
REM  Lo unico que hace este fichero es encontrar un BASH DE VERDAD. Y aqui hay
REM  una trampa: el `bash.exe` de C:\Windows\System32 es el lanzador de WSL, no
REM  el de Git. Lanzar el check con ese da un WSL que no ve las rutas de
REM  Windows y falla con un error que no tiene nada que ver con el check. Por
REM  eso se busca el de Git explicitamente y se descarta System32.
REM
REM  Uso:  Scripts\verify_all_check.bat [--only=N] [--no-build] [--comparar A B]
REM ============================================================================
setlocal EnableDelayedExpansion

set "DIRSCRIPT=%~dp0"
set "CHECK=%DIRSCRIPT%verify_all_check.sh"

if not exist "%CHECK%" (
    echo   No esta %CHECK%, y sin el este check no puede comparar nada.
    echo   Esta junto a este .bat, en la carpeta Scripts.
    exit /b 2
)

REM -- BUSCAR UN BASH QUE SEPA LEER RUTAS DE WINDOWS -------------------------
REM En este orden:
REM   1. el `bash` del PATH, si NO es el de System32 (que es el de WSL);
REM   2. el de Git, en los dos sitios donde se instala.
REM
REM Con `%ProgramFiles%` a mano y no con una variable de entorno que puede no
REM estar: un .bat lanzado desde un servicio, o desde una cuenta de servicio, no
REM la tiene. Y una ruta de Git montada a mano con `C:\` a pelo no funciona si
REM el Windows esta en otro disco.
set "BASH="
for /f "usebackq delims=" %%B in (`where bash 2^>nul`) do (
    if not defined BASH (
        REM El de System32 es el lanzador de WSL. Se descarta por el NOMBRE del
        REM fichero y no por el contenido de `bash --version`: ese ultimo es un
        REM programa que cuesta medio segundo, y con que uno llegue aqui con
        !BASH! ya definido no se vuelve a mirar.
        echo %%B | findstr /i /v /c:"\System32\" >nul
        if not errorlevel 1 set "BASH=%%B"
    )
)
if not defined BASH if exist "%ProgramFiles%\Git\bin\bash.exe" set "BASH=%ProgramFiles%\Git\bin\bash.exe"
if not defined BASH if exist "%ProgramFiles(x86)%\Git\bin\bash.exe" set "BASH=%ProgramFiles(x86)%\Git\bin\bash.exe"
if not defined BASH if exist "%ProgramW6432%\Git\bin\bash.exe" set "BASH=%ProgramW6432%\Git\bin\bash.exe"

if not defined BASH (
    echo.
    echo   No se ha encontrado un bash de verdad en esta maquina.
    echo.
    echo   Este check necesita bash porque tiene que lanzar LOS DOS gemelos, y
    echo   uno de ellos es verify_all.sh. Sin el solo se podria lanzar el .bat,
    echo   y comparar el .bat consigo mismo sale siempre bien.
    echo.
    echo   Se busca en el PATH ^(descartando el de System32, que es el de WSL^)
    echo   y en %%ProgramFiles%%\Git\bin.
    echo.
    echo   Con Git instalado, esto funciona:
    echo       Scripts\verify_all_check.bat
    exit /b 2
)

REM -- LANZAR -----------------------------------------------------------------
REM La ruta del .sh va en formato Windows porque se la pasa a un bash de
REM Windows, que no entiende de /d/... aunque venga del mismo disco.
REM
REM Y los argumentos se pasan con `%*`, sin reensamblarlos: el bucle de
REM argumentos que hace falta en verify_all.bat no hace falta aqui, porque este
REM no tiene que CLASIFICAR los que le llegan, solo pasarlos. Un `shift` de mas
REM en un bucle de reensamblado es la forma de que `--only=2` llegue como dos
REM argumentos y los dos gemelos hagan pasos distintos.
echo.
echo   El check lo hace verify_all_check.sh, con: %BASH%
echo.
"%BASH%" "%CHECK%" %*
exit /b %errorlevel%
