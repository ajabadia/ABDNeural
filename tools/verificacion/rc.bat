@echo off
REM rc.bat: correr otro .bat y DEVOLVER su ERRORLEVEL.
REM
REM POR QUE EXISTE
REM
REM MEDIDO el 2026-10-03: `cmd //c "otro.bat & echo RC=%ERRORLEVEL%"` MIENTE.
REM El `%ERRORLEVEL%` se expande AL MONTAR la linea, antes de que corra el otro
REM script, asi que imprime el del `cmd` y no el del .bat. Un banco de pruebas
REM armado asi da verde de un script que ha fallado, que es lo peor que puede
REM pasarle a un banco.
REM
REM La salida de un .bat a otro con `call` devuelve el codigo de salida en
REM %ERRORLEVEL%, pero con expansion INMEDIATA hay que leerlo con
REM `!ERRORLEVEL!`, y eso obliga a `setlocal EnableDelayedExpansion`.
REM
REM USO
REM
REM     rc.bat otro.bat argumentos
REM
REM Sale con el codigo de salida del otro .bat. Si no hay argumento, con 2.
REM
REM Sin `endlocal &`: el setlocal se cierra solo al salir del batch, y el `&`
REM fazia que cmd escribiera un "endlocal " de ruido en la salida del que llama.
REM MEDIDO el 2026-10-03.
REM
REM Lo que hace falta despues, en un .bat que llama a este:
REM
REM     call rc.bat un.bat
REM     if errorlevel 1 echo [ROJO] un.bat ha fallado
setlocal EnableDelayedExpansion

if "%~1"=="" (
    echo rc.bat: falta el .bat que hay que correr ^(uso: rc.bat otro.bat argumentos^) 1>&2
    exit /b 2
)

call "%~1" %2 %3 %4 %5 %6 %7 %8 %9
set "RC=!ERRORLEVEL!"

REM No se imprime nada: el que llama ya tiene el codigo en ERRORLEVEL, y un echo
REM de mas se leia como ruido en medio de su propia salida. MEDIDO el 2026-10-03,
REM que ademas arrastraba un "exit /b N" que no habia pedido nadie.

exit /b %RC%
