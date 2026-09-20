@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM  ABDNeural (NEURONiK) - Compilacion Release
REM
REM  Uso:  build.bat                    -> plugin + contrato + WebUI + tests + selftest
REM        build.bat <directorio>       -> usa otro directorio de build
REM        build.bat modelmaker         -> incluye la herramienta ModelMaker
REM        build.bat modelmaker release -> ModelMaker marcado RELEASE (incrementa su Version.h)
REM        build.bat build modelmaker   -> build limpio incluyendo ModelMaker
REM        build.bat noselftest         -> omite el E2E del bridge (paso 9)
REM        build.bat tests              -> modo rapido: solo contrato + suite de pruebas
REM        build.bat nowasm             -> omite el WASM del worklet (puede quedar viejo)
REM
REM  ModelMaker queda fuera por defecto a proposito: es otro entregable. Su
REM  Version.h (fichero versionado) SOLO se incrementa en builds marcadas como
REM  release: 'build.bat modelmaker release'. Una compilacion normal de
REM  verificacion (build.bat modelmaker) NO lo toca.
REM
REM  El paso 9 corre el selftest del bridge sobre el canal real de WebView2 DOS
REM  veces y sobre la MISMA pagina: en el PLUGIN (Standalone, el veredicto que
REM  manda) y en la bancada WebView2, que sirve WebUI\dist desde disco. Los dos
REM  corren las mismas SEIS direcciones — MATRIZ (abre el cajon con la matriz en
REM  uso y deja el cajon abierto), nativo->JS, JS->nativo, GENERAL, MIDI y MODELOS
REM  A-D — y SIN omitidos: la maquinaria de "direccion no aplicable" se fue con el
REM  piloto (ticket 8.4). Exit code != 0 si alguna direccion no se mueve.
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
set "MM_RELEASE=0"
set "WITH_SELFTEST=1"
set "TESTS_ONLY=0"
set "WITH_WASM=1"

for %%A in (%*) do (
    if /I "%%A"=="--internal-log" (
        rem bandera del envoltorio de log: ignorar
    ) else if /I "%%A"=="modelmaker" (
        set "WITH_MODELMAKER=1"
    ) else if /I "%%A"=="release" (
        set "MM_RELEASE=1""
    ) else if /I "%%A"=="noselftest" (
        set "WITH_SELFTEST=0"
    ) else if /I "%%A"=="tests" (
        set "TESTS_ONLY=1"
        set "WITH_SELFTEST=0"
    ) else if /I "%%A"=="nowasm" (
        set "WITH_WASM=0"
    ) else (
        set "BUILD_DIR=%%A"
    )
)

if "%BUILD_DIR%"=="" set "BUILD_DIR=build-reference"
set "EXIT_CODE=0"
set "HOST_BUILD_FAILED=0"

cd /d "%~dp0"

echo === Sesion: %DATE% %TIME% ===
echo =======================================================
echo          ABDNeural (NEURONiK) - Compilacion Release
echo          Directorio de build: %BUILD_DIR%
if "%WITH_MODELMAKER%"=="1" if "%MM_RELEASE%"=="1" echo          ModelMaker: INCLUIDO, marcado RELEASE ^(Version.h se incrementara^)
if "%WITH_MODELMAKER%"=="1" if "%MM_RELEASE%"=="0" echo          ModelMaker: INCLUIDO, sin marca release ^(Version.h NO se toca^)
if "%TESTS_ONLY%"=="1" echo          Modo: SOLO TESTS
if "%TESTS_ONLY%"=="0" echo          Modo: COMPLETO
echo =======================================================
echo.

REM Reconfigurar solo la primera vez: cmake -S -B cuesta ~4s y no hace falta
REM en cada pasada (los cambios en CMakeLists.txt los detecta MSBuild solo,
REM via ZERO_CHECK). Borrar build\CMakeCache.txt fuerza una reconfiguracion.
if exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [1/9] CMake ya configurado ^(se omite la reconfiguracion^)...
) else (
    echo [1/9] Configurando CMake...
    cmake -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release
    if !ERRORLEVEL! neq 0 (
        echo.
        echo [ERROR] Fallo en la configuracion de CMake.
        set "EXIT_CODE=1"
        goto :finish
    )
)

echo.
echo [2/9] Generando el contrato de parametros (WebUI\generated)...
cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_ParameterExport
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Fallo al compilar el exportador del contrato.
    set "EXIT_CODE=1"
    goto :finish
)

"%BUILD_DIR%\Release\NEURONiK_ParameterExport.exe" WebUI\generated
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Fallo al regenerar WebUI\generated.
    set "EXIT_CODE=1"
    goto :finish
)

if "%TESTS_ONLY%"=="1" goto :tests

REM El orden IMPORTA, y ahora va de abajo arriba: el .wasm lo produce la WASM,
REM la WebUI lo copia a su dist (publicDir = WebUI\public) y el PLUGIN lo
REM EMBIBE (juce_add_binary_data sobre WebUI\dist/*). Compilar el plugin antes
REM dejaba dentro el bundle de la pasada ANTERIOR.
echo.
echo [3/9] Compilando el DSP a WebAssembly (worklet + paridad + smoke)...
if "%WITH_WASM%"=="0" goto :no_wasm
REM build_wasm.bat compila el DSP a WASM, valida la paridad nativo<->WASM, corre
REM el smoke y SINCRONIZA WebUI\public\worklet (que el build de la WebUI copia a
REM dist/ y el plugin embebe). Se le pasa --internal-log nopause para no anidar su
REM tee (va al log de esta pasada) ni quedarse en su pausa final.
call "%~dp0build_wasm.bat" --internal-log nopause
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Fallo la compilacion/validacion WASM. El worklet de la WebUI
    echo         quedaria DESACTUALIZADO, asi que se aborta. Para omitirlo a
    echo         proposito: build.bat nowasm
    set "EXIT_CODE=1"
    goto :finish
)
echo [OK] WASM compilado, validado y sincronizado en WebUI\public\worklet.
goto :wasm_done

:no_wasm
echo [INFO] Omitido por flag nowasm: el worklet puede quedar desactualizado.

:wasm_done
REM La interfaz del plugin es WebUI\dist y va EMBEBIDA en el binario, asi que su
REM exportacion tiene que ir ANTES de compilar el plugin (y DESPUES de la WASM:
REM el publicDir de la WebUI es WebUI\public, donde sync-wasm deja el .wasm).
echo [4/9] Exportando la WebUI del plugin (WebUI\dist)...
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
REM sintoma es mudo (el mismo sintoma que cubrian los guards del piloto).
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
echo [5/9] Compilando Standalone y VST3 (embiben la WebUI recien exportada)...
cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_Standalone NEURONiK_VST3
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Fallo en la compilacion del plugin.
    set "EXIT_CODE=1"
    goto :finish
)

echo.
echo [6/9] Compilando la bancada WebView2 (sirve WebUI\dist desde disco)...
cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_WebPilotHost
if !ERRORLEVEL! neq 0 (
    echo [AVISO] No se pudo compilar la bancada. El plugin sigue siendo valido.
    set HOST_BUILD_FAILED=1
)

:modelmaker
echo.
echo [7/9] Herramienta ModelMaker...
if "%WITH_MODELMAKER%"=="0" goto :no_modelmaker

if "%MM_RELEASE%"=="1" set "NEURONIK_MM_RELEASE=1"
cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_ModelMaker
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Fallo en la compilacion de ModelMaker.
    set "EXIT_CODE=1"
    goto :finish
)
set "NEURONIK_MM_RELEASE=0"

echo [INFO] Version de ModelMaker en Source\ModelMaker\Version.h:
findstr /R "NEURONIK_MODELMAKER_VERSION" Source\ModelMaker\Version.h
if "%MM_RELEASE%"=="1" (
    echo [INFO] Build marcada RELEASE: el incremento de Version.h forma parte de esta
    echo        pasada y es lo que hay que commitear.
) else (
    echo [INFO] Sin marca release: Version.h NO se ha tocado. Para un release:
    echo        build.bat modelmaker release
)
goto :tests

:no_modelmaker
echo [INFO] Omitido a proposito: es otro entregable. build.bat modelmaker para incluirlo.

:tests
echo.
echo [8/9] Compilando y ejecutando la suite de pruebas...
REM La lista debe cubrir TODOS los tests registrados en ctest: si falta uno,
REM ctest falla al no encontrar el ejecutable (no se construye solo).
cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_DSPReferenceTest NEURONiK_MidiPortTest NEURONiK_MidiChannelFilterTest NEURONiK_VelocityCurveTest NEURONiK_LfoSyncTest NEURONiK_ParameterDescriptorTest NEURONiK_PresetRoundTripTest NEURONiK_StatePersistenceTest NEURONiK_ModelSlotTest NEURONiK_ModelMakerRoundTripTest NEURONiK_ParameterBridgeTest NEURONiK_BridgeProtocolContractTest NEURONiK_DspReverbParityTest NEURONiK_DspReverbJucePolicyTest NEURONiK_DspEffectsParityTest NEURONiK_AudioBufferParityTest ABDShared_DspCore_Tests
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
echo [9/9] Selftest del bridge (plugin Standalone + bancada, la MISMA pagina)...
REM La superficie que SE ENVIA es el plugin: su Standalone hospeda la pagina de la
REM WebUI y corre el arnes compartido (Source/WebUI/BridgeSelftest.h), cuyo veredicto
REM es su codigo de salida. Va PRIMERO y manda. La bancada corre el MISMO arnes sobre
REM la MISMA pagina, servida desde disco.
set "PLUGIN_STANDALONE=%BUILD_DIR%\NEURONiK_artefacts\Release\Standalone\NEURONiK.exe"
if not exist "%PLUGIN_STANDALONE%" (
    echo [AVISO] Standalone del plugin no disponible, selftest del plugin omitido.
    set "EXIT_CODE=1"
    goto :finish
)

REM El transcript queda junto al log de la compilacion, para poder leer el detalle
REM sin depender del stdout (y ahi es donde lo busca quien depura un FAIL).
set "NEURONIK_SELFTEST_LOG=%~dp0%BUILD_DIR%\neuronik-selftest.log"
"%PLUGIN_STANDALONE%" --selftest
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] El selftest del plugin fallo: alguna direccion no se movio.
    echo         Detalle: %NEURONIK_SELFTEST_LOG%
    set "EXIT_CODE=1"
    goto :finish
)
echo [OK] Plugin verificado (6 direcciones): MATRIZ, nativo-^>JS, JS-^>nativo, GENERAL, MIDI y MODELOS A-D.

REM Solo si la bancada compilo y la pagina existe: sin pagina que cargar no hay E2E.
REM La bancada sirve WebUI\dist (la MISMA pagina que embebe el plugin), asi que este
REM selftest no depende de ninguna otra exportacion: lo que comprueba es lo que se
REM envia.
set "PILOT_HOST=%BUILD_DIR%\NEURONiK_WebPilotHost_artefacts\Release\NEURONiK Web Pilot.exe"
if "!HOST_BUILD_FAILED!"=="1" (
    echo [AVISO] La bancada NO recompilo en esta pasada: el enlace borro el exe anterior.
    echo         Selftest omitido para no dar un OK enganoso.
    set "EXIT_CODE=1"
    goto :finish
)
if not exist "%PILOT_HOST%" (
    echo [AVISO] Bancada no disponible, selftest omitido.
    goto :finish
)
if not exist "WebUI\dist\index.html" (
    echo [AVISO] WebUI\dist no existe: sin pagina que cargar no hay E2E ^(y el plugin
    echo         tampoco embebio interfaz^). Selftest de la bancada omitido.
    goto :finish
)

"%PILOT_HOST%" --selftest
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] El selftest del bridge fallo: alguna direccion no se movio.
    set "EXIT_CODE=1"
    goto :finish
)
echo [OK] Bancada verificada: las seis direcciones sobre la MISMA pagina del plugin.

echo.
echo =======================================================
echo  [EXITO] Compilacion y pruebas completadas.
echo =======================================================
echo  Standalone:        %BUILD_DIR%\NEURONiK_artefacts\Release\Standalone\NEURONiK.exe
echo  VST3:              %BUILD_DIR%\NEURONiK_artefacts\Release\VST3\NEURONiK.vst3
echo  Bancada WebView2:  %BUILD_DIR%\NEURONiK_WebPilotHost_artefacts\Release\NEURONiK Web Pilot.exe
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
