@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM  ABDNeural (NEURONiK) - Compilacion Release
REM
REM  Uso:  build.bat                    -> plugin + contrato + WebUI + tests + selftest
REM        build.bat <directorio>       -> usa otro directorio de build
REM        build.bat modelmaker         -> incluye la herramienta ModelMaker
REM        build.bat build modelmaker   -> build limpio incluyendo ModelMaker
REM        build.bat noselftest         -> omite el E2E del bridge (paso 9)
REM        build.bat tests              -> modo rapido: solo contrato + suite de pruebas
REM        build.bat nextui             -> WebUI con Next.js en vez de Vite (referencia)
REM        build.bat nowasm             -> omite el WASM del worklet (puede quedar viejo)
REM
REM  ModelMaker queda fuera por defecto a proposito: su target arrastra
REM  'UpdateVersion', que incrementa Source\ModelMaker\Version.h (fichero
REM  versionado) en cada compilacion.
REM
REM  El paso 9 abre brevemente la ventana del host del piloto y ejecuta el
REM  selftest bidireccional del bridge (nativo->JS y JS->nativo, sobre el
REM  canal real de WebView2). Exit code != 0 si alguna direccion no se mueve.
REM
REM  El script siempre termina con PAUSA, incluso si algo falla.
REM  Cada pasada deja ademas build-last-run.log (log espejo de la consola),
REM  asi si la ventana se cierra sin querer el resultado queda en disco.
REM ============================================================================

REM ---- Log espejo: relanza el script internamente y teed consola+fichero ----
if not "%~1"=="--internal-log" (
    powershell -NoProfile -Command "& cmd /c '\"%~f0\" --internal-log %*' 2>&1 | Tee-Object -Variable out; $out | Out-File -FilePath 'build-last-run.log' -Encoding utf8; exit $LASTEXITCODE"
    exit /b !ERRORLEVEL!
)

set "BUILD_DIR="
set "WITH_MODELMAKER=0"
set "WITH_SELFTEST=1"
set "TESTS_ONLY=0"
set "WITH_NEXTUI=0"
set "WITH_WASM=1"

for %%A in (%*) do (
    if /I "%%A"=="--internal-log" (
        rem bandera del envoltorio de log: ignorar
    ) else if /I "%%A"=="modelmaker" (
        set "WITH_MODELMAKER=1"
    ) else if /I "%%A"=="noselftest" (
        set "WITH_SELFTEST=0"
    ) else if /I "%%A"=="tests" (
        set "TESTS_ONLY=1"
        set "WITH_SELFTEST=0"
    ) else if /I "%%A"=="nextui" (
        set "WITH_NEXTUI=1"
    ) else if /I "%%A"=="nowasm" (
        set "WITH_WASM=0"
    ) else (
        set "BUILD_DIR=%%A"
    )
)

if "%BUILD_DIR%"=="" set "BUILD_DIR=build-reference"
set "EXIT_CODE=0"
set "HOST_BUILD_FAILED=0"
set "WEBUI_BUILD_FAILED=0"

cd /d "%~dp0"

echo === Sesion: %DATE% %TIME% ===
echo =======================================================
echo          ABDNeural (NEURONiK) - Compilacion Release
echo          Directorio de build: %BUILD_DIR%
if "%WITH_MODELMAKER%"=="1" echo          ModelMaker: INCLUIDO ^(Version.h se incrementara^)
if "%TESTS_ONLY%"=="1" echo          Modo: SOLO TESTS
if "%TESTS_ONLY%"=="0" echo          Modo: COMPLETO
echo =======================================================
echo.

REM Reconfigurar solo la primera vez: cmake -S -B cuesta ~4s y no hace falta
REM en cada pasada (los cambios en CMakeLists.txt los detecta MSBuild solo,
REM via ZERO_CHECK). Borrar build\CMakeCache.txt fuerza una reconfiguracion.
if exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [1/10] CMake ya configurado ^(se omite la reconfiguracion^)...
) else (
    echo [1/10] Configurando CMake...
    cmake -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release
    if !ERRORLEVEL! neq 0 (
        echo.
        echo [ERROR] Fallo en la configuracion de CMake.
        set "EXIT_CODE=1"
        goto :finish
    )
)

echo.
echo [2/10] Generando el contrato de parametros (WebPilot\generated)...
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

if "%TESTS_ONLY%"=="1" goto :tests

REM El orden IMPORTA, y ahora va de abajo arriba: el .wasm lo produce la WASM,
REM la WebUI lo copia a su dist (publicDir = WebPilot\public) y el PLUGIN lo
REM EMBIBE (juce_add_binary_data sobre WebUI\dist/*). Compilar el plugin antes
REM dejaba dentro el bundle de la pasada ANTERIOR.
echo.
echo [3/10] Compilando el DSP a WebAssembly (worklet + paridad + smoke)...
if "%WITH_WASM%"=="0" goto :no_wasm
REM build_wasm.bat compila el DSP a WASM, valida la paridad nativo<->WASM, corre
REM el smoke y SINCRONIZA WebPilot\public\worklet (que la exportacion de la WebUI
REM copia a out/ y el host embebe). Se le pasa --internal-log nopause para no
REM anidar su tee (va al log de esta pasada) ni quedarse en su pausa final.
call "%~dp0build_wasm.bat" --internal-log nopause
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Fallo la compilacion/validacion WASM. El worklet de la WebUI
    echo         quedaria DESACTUALIZADO, asi que se aborta. Para omitirlo a
    echo         proposito: build.bat nowasm
    set "EXIT_CODE=1"
    goto :finish
)
echo [OK] WASM compilado, validado y sincronizado en WebPilot\public\worklet.
goto :wasm_done

:no_wasm
echo [INFO] Omitido por flag nowasm: el worklet puede quedar desactualizado.

:wasm_done
REM La interfaz del plugin es WebUI\dist y va EMBEBIDA en el binario, asi que su
REM exportacion tiene que ir ANTES de compilar el plugin (y DESPUES de la WASM:
REM el publicDir de la WebUI es WebPilot\public, donde sync-wasm deja el .wasm).
echo [4/10] Exportando la WebUI del plugin (WebUI\dist)...
if not exist "WebUI\node_modules" goto :no_plugin_webui

pushd WebUI
call pnpm build
if !ERRORLEVEL! neq 0 (
    popd
    echo [ERROR] Fallo la exportacion de la WebUI del plugin. Sin WebUI\dist nueva
    echo         el plugin embebiria la ANTERIOR, o ninguna.
    set "EXIT_CODE=1"
    goto :finish
)
popd
echo [OK] WebUI del plugin exportada en WebUI\dist.
goto :plugin_webui_done

:no_plugin_webui
echo [ERROR] WebUI\node_modules no existe: el plugin necesita su interfaz.
echo         Ejecuta: cd WebUI ^&^& pnpm install
set "EXIT_CODE=1"
goto :finish

:plugin_webui_done
REM Guard: la pagina embebida tiene que llevar el worklet RECIEN compilado. Si el
REM publicDir de la WebUI no lo copio, el plugin sonaria con un DSP VIEJO y el
REM sintoma es mudo (misma clase de fallo que cubre el guard de WebPilot\out).
if "%WITH_WASM%"=="1" if not exist "WebUI\dist\worklet\neuronik_dsp.wasm" (
    echo [ERROR] WebUI\dist\worklet\neuronik_dsp.wasm no existe: la interfaz
    echo         embebida no traeria worklet. Revisa el publicDir de la WebUI.
    set "EXIT_CODE=1"
    goto :finish
)
if "%WITH_WASM%"=="1" if exist "build-wasm\neuronik_dsp.wasm" if exist "WebUI\dist\worklet\neuronik_dsp.wasm" (
    for %%F in ("build-wasm\neuronik_dsp.wasm") do set "WSRC_SIZE=%%~zF"
    for %%F in ("WebUI\dist\worklet\neuronik_dsp.wasm") do set "WDST_SIZE=%%~zF"
    if not "!WSRC_SIZE!"=="!WDST_SIZE!" (
        echo [ERROR] WebUI\dist\worklet\neuronik_dsp.wasm no coincide con build-wasm:
        echo         el worklet embebido seria un DSP VIEJO.
        set "EXIT_CODE=1"
        goto :finish
    )
)

echo.
echo [5/10] Compilando Standalone y VST3 (embiben la WebUI recien exportada)...
cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_Standalone NEURONiK_VST3
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Fallo en la compilacion del plugin.
    set "EXIT_CODE=1"
    goto :finish
)

REM Motor por defecto: Vite (A/B de la Fase 6: -45%% de bundle, build 4x mas
REM rapido, misma pagina y mismo selftest). Next queda detras de `nextui`.
REM Ambos motores VACIAN out/ al empezar: no se mezclan restos de motor.
echo.
echo [6/10] Exportando la WebUI del piloto...
if not exist "WebPilot\node_modules" goto :no_webui

pushd WebPilot
if "%WITH_NEXTUI%"=="1" goto :webui_next

echo [INFO] Motor WebUI: Vite ^(@abdsynths/web-pilot-vite^)
call pnpm --filter @abdsynths/web-pilot-vite build
if !ERRORLEVEL! neq 0 (
    popd
    echo [ERROR] Fallo la exportacion de la WebUI con Vite. Sin WebUI nueva no hay
    echo         selftest honesto: corria contra WebPilot\out ANTERIOR \^(staleness\^).
    set "WEBUI_BUILD_FAILED=1"
    set "EXIT_CODE=1"
    goto :modelmaker
)
popd
echo [OK] WebUI del piloto exportada con Vite en WebPilot\out
goto :pilot_host

:webui_next
echo [INFO] Motor WebUI: Next.js ^(referencia, flag nextui^)
call pnpm build
if !ERRORLEVEL! neq 0 (
    popd
    echo [ERROR] Fallo la exportacion de la WebUI con Next. Sin WebUI nueva no hay
    echo         selftest honesto: corria contra WebPilot\out ANTERIOR \^(staleness\^).
    set "WEBUI_BUILD_FAILED=1"
    set "EXIT_CODE=1"
    goto :modelmaker
)
popd
echo [OK] WebUI del piloto exportada con Next en WebPilot\out
goto :pilot_host

:no_webui
echo [INFO] WebPilot\node_modules no existe, se omite la exportacion.
echo        Para habilitarla: cd WebPilot ^&^& pnpm install --ignore-workspace

:pilot_host
REM Guard de staleness del worklet: el .wasm embebido debe ser EXACTAMENTE el
REM recien compilado. sync-wasm.mjs lo dejo en WebPilot\public\worklet y Vite lo
REM copia a out/ (publicDir apunta a WebPilot/public). Si esa copia falla, la
REM WebUI sonaria con un DSP VIEJO y el sintoma es mudo. Comparacion por tamano.
if "%WITH_WASM%"=="1" if exist "build-wasm\neuronik_dsp.wasm" if exist "WebPilot\out\worklet\neuronik_dsp.wasm" (
    for %%F in ("build-wasm\neuronik_dsp.wasm") do set "WSRC_SIZE=%%~zF"
    for %%F in ("WebPilot\out\worklet\neuronik_dsp.wasm") do set "WDST_SIZE=%%~zF"
    if not "!WSRC_SIZE!"=="!WDST_SIZE!" (
        echo [ERROR] WebPilot\out\worklet\neuronik_dsp.wasm no coincide con build-wasm:
        echo         el worklet embebido seria un DSP VIEJO. Revisa el publicDir de
        echo         Vite y la exportacion de la WebUI.
        set "EXIT_CODE=1"
        goto :finish
    )
)
echo.
echo [7/10] Compilando el host del piloto WebPilot (embibe la WebUI recien exportada)...
cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_WebPilotHost
if !ERRORLEVEL! neq 0 (
    echo [AVISO] No se pudo compilar el host del piloto. El plugin sigue siendo valido.
    set HOST_BUILD_FAILED=1
)

:modelmaker
echo.
echo [8/10] Herramienta ModelMaker...
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
echo [9/10] Compilando y ejecutando la suite de pruebas...
REM La lista debe cubrir TODOS los tests registrados en ctest: si falta uno,
REM ctest falla al no encontrar el ejecutable (no se construye solo).
cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_DSPReferenceTest NEURONiK_MidiPortTest NEURONiK_MidiChannelFilterTest NEURONiK_VelocityCurveTest NEURONiK_LfoSyncTest NEURONiK_ParameterDescriptorTest NEURONiK_PresetRoundTripTest NEURONiK_StatePersistenceTest NEURONiK_ParameterBridgeTest NEURONiK_BridgeProtocolContractTest NEURONiK_DspReverbParityTest NEURONiK_DspReverbJucePolicyTest NEURONiK_DspEffectsParityTest NEURONiK_AudioBufferParityTest ABDShared_DspCore_Tests
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
if "%WITH_SELFTEST%"=="0" goto :finish
echo [10/10] Selftest bidireccional del bridge del piloto...
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
echo  WebUI embebida:    WebUI\dist  ^(dentro del Standalone y del VST3^)
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
