@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM  ABDNeural (NEURONiK) - Compilacion Release
REM
REM  Uso:  build.bat                    -> plugin + contrato + WebUI + tests + selftest
REM        build.bat <directorio>       -> usa otro directorio de build
REM        build.bat modelmaker         -> incluye la herramienta ModelMaker
REM        build.bat build modelmaker   -> build limpio incluyendo ModelMaker
REM        build.bat noselftest         -> omite el E2E del bridge (paso 8)
REM
REM  ModelMaker queda fuera por defecto a proposito: su target arrastra
REM  'UpdateVersion', que incrementa Source\ModelMaker\Version.h (fichero
REM  versionado) en cada compilacion.
REM
REM  El paso 8 abre brevemente la ventana del host del piloto y ejecuta el
REM  selftest bidireccional del bridge (nativo->JS y JS->nativo, sobre el
REM  canal real de WebView2). Exit code != 0 si alguna direccion no se mueve.
REM
REM  El script siempre termina con PAUSA, incluso si algo falla.
REM ============================================================================

set "BUILD_DIR="
set "WITH_MODELMAKER=0"
set "WITH_SELFTEST=1"

for %%A in (%*) do (
    if /I "%%A"=="modelmaker" (
        set "WITH_MODELMAKER=1"
    ) else if /I "%%A"=="noselftest" (
        set "WITH_SELFTEST=0"
    ) else (
        set "BUILD_DIR=%%A"
    )
)

if "%BUILD_DIR%"=="" set "BUILD_DIR=build-reference"
set "EXIT_CODE=0"
set "HOST_BUILD_FAILED=0"
set "WEBUI_BUILD_FAILED=0"

cd /d "%~dp0"

echo =======================================================
echo          ABDNeural (NEURONiK) - Compilacion Release
echo          Directorio de build: %BUILD_DIR%
if "%WITH_MODELMAKER%"=="1" echo          ModelMaker: INCLUIDO ^(Version.h se incrementara^)
echo =======================================================
echo.

echo [1/8] Configurando CMake...
cmake -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Fallo en la configuracion de CMake.
    set "EXIT_CODE=1"
    goto :finish
)

echo.
echo [2/8] Generando el contrato de parametros (WebPilot\generated)...
cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_ParameterExport
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Fallo al compilar el exportador del contrato.
    set "EXIT_CODE=1"
    goto :finish
)

"%BUILD_DIR%\Release\NEURONiK_ParameterExport.exe" WebPilot\generated
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Fallo al regenerar WebPilot\generated.
    set "EXIT_CODE=1"
    goto :finish
)

echo.
echo [3/8] Compilando Standalone y VST3...
cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_Standalone NEURONiK_VST3
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Fallo en la compilacion del plugin.
    set "EXIT_CODE=1"
    goto :finish
)

echo.
REM El orden IMPORTA: la WebUI (out/) va ANTES que el host. El host EMBIBE el
REM snapshot de out/ en el enlace (juce_add_binary_data sobre WebPilot/out/*);
REM compilar el host antes dejaba dentro el bundle de la pasada ANTERIOR.
echo.
echo [4/8] Exportando la WebUI del piloto...
if not exist "WebPilot\node_modules" goto :no_webui

pushd WebPilot
call pnpm build
if !ERRORLEVEL! neq 0 (
    popd
    echo [ERROR] Fallo la exportacion de la WebUI del piloto. Sin WebUI nueva no hay
    echo         selftest honesto: corria contra WebPilot\out ANTERIOR (staleness).
    set "WEBUI_BUILD_FAILED=1"
    set "EXIT_CODE=1"
    goto :modelmaker
)
popd
echo [OK] WebUI del piloto exportada en WebPilot\out
goto :pilot_host

:no_webui
echo [INFO] WebPilot\node_modules no existe, se omite la exportacion.
echo        Para habilitarla: cd WebPilot ^&^& pnpm install --ignore-workspace

:pilot_host
echo.
echo [5/8] Compilando el host del piloto WebPilot (embibe la WebUI recien exportada)...
cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_WebPilotHost
if !ERRORLEVEL! neq 0 (
    echo [AVISO] No se pudo compilar el host del piloto. El plugin sigue siendo valido.
    set HOST_BUILD_FAILED=1
)

:modelmaker
echo.
echo [6/8] Herramienta ModelMaker...
if "%WITH_MODELMAKER%"=="0" goto :no_modelmaker

cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_ModelMaker
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Fallo en la compilacion de ModelMaker.
    set "EXIT_CODE=1"
    goto :finish
)

echo [INFO] Version de ModelMaker en Source\ModelMaker\Version.h:
findstr /R "NEURONIK_MODELMAKER_VERSION" Source\ModelMaker\Version.h
echo [AVISO] Ese fichero esta versionado en git: revisa 'git status' y descarta el
echo         incremento si no forma parte de lo que quieres commitear.
goto :tests

:no_modelmaker
echo [INFO] Omitido a proposito: compilar ModelMaker incrementa Source\ModelMaker\Version.h.
echo        Para incluirlo: build.bat modelmaker

:tests
echo.
echo [7/8] Compilando y ejecutando la suite de pruebas...
cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_DSPReferenceTest NEURONiK_MidiChannelFilterTest NEURONiK_VelocityCurveTest NEURONiK_LfoSyncTest NEURONiK_ParameterDescriptorTest NEURONiK_PresetRoundTripTest NEURONiK_StatePersistenceTest NEURONiK_ParameterBridgeTest NEURONiK_BridgeProtocolContractTest
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Fallo al compilar las pruebas.
    set "EXIT_CODE=1"
    goto :finish
)

ctest --test-dir "%BUILD_DIR%" -C Release --output-on-failure
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Alguna prueba ha fallado. Revisa la salida de arriba.
    set "EXIT_CODE=1"
    goto :finish
)

echo.
echo [8/8] Selftest bidireccional del bridge del piloto...
if "%WITH_SELFTEST%"=="0" goto :finish

REM Solo si el host compilo y la WebUI existe: sin pagina que cargar no hay E2E.
set "PILOT_HOST=%BUILD_DIR%\NEURONiK_WebPilotHost_artefacts\Release\NEURONiK Web Pilot.exe"
if "!HOST_BUILD_FAILED!"=="1" (
    echo [AVISO] El host del piloto NO recompilo en esta pasada: el enlace borro el exe
    echo         anterior. Selftest omitido para no dar un OK enganoso.
    set "EXIT_CODE=1"
    goto :finish
)
if not exist "%PILOT_HOST%" (
    echo [AVISO] Host del piloto no disponible, selftest omitido.
    goto :finish
)
if "!WEBUI_BUILD_FAILED!"=="1" (
    echo [AVISO] La WebUI del piloto no se pudo exportar en esta pasada: el selftest
    echo         se omitiria contra WebPilot\out ANTERIOR. Exporta de nuevo con
    echo         cd WebPilot ^&^& pnpm build y relanza build.bat.
    goto :finish
)
if not exist "WebPilot\out\index.html" (
    echo [AVISO] WebPilot\out no existe, selftest omitido.
    goto :finish
)

"%PILOT_HOST%" --selftest
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] El selftest del bridge fallo: alguna direccion no se movio.
    set "EXIT_CODE=1"
    goto :finish
)
echo [OK] Bridge verificado: nativo-^>JS y JS-^>nativo.

echo.
echo =======================================================
echo  [EXITO] Compilacion y pruebas completadas.
echo =======================================================
echo  Standalone:        %BUILD_DIR%\NEURONiK_artefacts\Release\Standalone\NEURONiK.exe
echo  VST3:              %BUILD_DIR%\NEURONiK_artefacts\Release\VST3\NEURONiK.vst3
echo  Host del piloto:   %BUILD_DIR%\NEURONiK_WebPilotHost_artefacts\Release\NEURONiK Web Pilot.exe
if "%WITH_MODELMAKER%"=="1" echo  ModelMaker:        %BUILD_DIR%\Release\NEURONiK_ModelMaker.exe
echo.
echo  Copia de seguridad de builds anteriores: "Versiones compiladas"

:finish
echo.
echo =======================================================
if "%EXIT_CODE%"=="0" (
    echo  RESULTADO: OK
) else (
    echo  RESULTADO: CON ERRORES ^(codigo %EXIT_CODE%^)
)
echo =======================================================
echo.
pause
exit /b %EXIT_CODE%
