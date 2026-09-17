@echo off
setlocal enabledelayedexpansion
REM ============================================================
REM start.bat - Arranque de la version web de NEURONiK
REM
REM   1) Host del piloto WebPilot (WebView2 + bridge JUCE<->
REM      WebUI, igual que ABDMS2000 usa su Vite en el 8384).
REM   2) Modo navegador: la misma WebUI servida sin bridge
REM      (LOCAL MODE), para depurar la pagina a solas.
REM
REM Compila antes con build.bat si los ejecutables no existen.
REM ============================================================

set "HOST_EXE=build-reference\NEURONiK_WebPilotHost_artefacts\Release\NEURONiK Web Pilot.exe"
set "WEBUI_DIR=WebPilot\out"

echo ============================================================
echo  NEURONiK - Version web (WebPilot)
echo ============================================================

if not exist "%HOST_EXE%" (
    echo [ERROR] No existe el host del piloto:
    echo         %HOST_EXE%
    echo         Compila primero con build.bat
    goto :fin
)
if not exist "%WEBUI_DIR%\index.html" (
    echo [ERROR] No existe la WebUI exportada:
    echo         %WEBUI_DIR%\index.html
    echo         Compila primero con build.bat ^(paso 5^)
    goto :fin
)

echo.
echo   1. Piloto en WebView2 ^(bridge con el plugin^)  [recomendado]
echo   2. Solo la WebUI en el navegador ^(sin bridge, LOCAL MODE^)
echo   3. Selftest bidireccional del bridge ^(automatico, cierra solo^)
echo.
choice /C 123 /N /M "Elige una opcion [1-3]: "
if errorlevel 3 goto :selftest
if errorlevel 2 goto :navegador

:piloto
echo.
echo ============================================================
echo  Arrancando el host del piloto ^(WebView2^)...
echo  Cierra la ventana para salir.
echo ============================================================
start "" "%HOST_EXE%"
goto :fin

:navegador
echo.
echo ============================================================
echo  Sirviendo %WEBUI_DIR% en http://localhost:8399
echo  (Ctrl+C para detener; sin bridge: la pagina queda en
echo   LOCAL MODE y el estado vive solo en la pagina)
echo ============================================================
cd /d "%~dp0%WEBUI_DIR%"
npx -y serve -l 8399 .
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo El servidor se ha detenido o no se pudo iniciar.
)
goto :fin

:selftest
echo.
echo ============================================================
echo  Selftest del bridge: NATIVO-^>JS y JS-^>NATIVO...
echo ============================================================
"%HOST_EXE%" --selftest
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] El selftest ha fallado. Revisa la salida de arriba.
) else (
    echo.
    echo [OK] Bridge verificado en ambas direcciones.
)

:fin
echo.
pause
