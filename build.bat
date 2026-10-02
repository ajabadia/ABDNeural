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
REM        build.bat nopause            -> sin pausa final (para automatizacion)
REM
REM  ModelMaker queda fuera por defecto a proposito: es otro entregable. Su
REM  Version.h (fichero versionado) SOLO se incrementa en builds marcadas como
REM  release: 'build.bat modelmaker release'. Una compilacion normal de
REM  verificacion (build.bat modelmaker) NO lo toca.
REM
REM  El paso 9 corre el selftest del bridge sobre el canal real de WebView2 DOS
REM  veces y sobre la MISMA pagina: en el PLUGIN (Standalone, el veredicto que
REM  manda) y en la bancada WebView2, que sirve WebUI\dist desde disco. Los dos
REM  corren las mismas DIEZ direcciones (MATRIZ abre el cajon con la matriz en
REM  uso y deja el cajon abierto) y SIN omitidos: la maquinaria de "direccion no
REM  aplicable" se fue con el piloto (ticket 8.4). Exit code != 0 si alguna
REM  direccion no se mueve.
REM
REM  Al final, :finish imprime el resumen del selftest por direccion, con UN
REM  bloque por superficie —PLUGIN (Standalone) y BANCADA (WebPilotHost)—, cada
REM  uno con su veredicto, su exit y su transcript: se distinguen porque las dos
REM  corrian el mismo arnes sobre la misma pagina y sus fallos no se parecen en
REM  nada (la bancada no escribe en el log del plugin, asi que antes su veredicto
REM  solo aparecia en linea durante el paso 9 y el resumen final solo pintaba el
REM  del plugin). La superficie que no se ejecuto en la pasada se dice con su
REM  motivo, en vez de callar. Todo parsea Scripts\selftest_summary.ps1 y todo
REM  cae tambien en build-last-run.log.
REM
REM  El script siempre termina con PAUSA, incluso si algo falla (build.bat
REM  nopause la omite para correr automatizado: CI, agentes, una sola pasada).
REM  Cada pasada deja ademas build-last-run.log (log espejo de la consola),
REM  asi si la ventana se cierra sin querer el resultado queda en disco.
REM ============================================================================

REM ---- Log espejo: relanza el script internamente y teed consola+fichero ----
if not "%~1"=="--internal-log" (
    powershell -NoProfile -Command "& cmd /c '\"%~f0\" --internal-log %*' 2>&1 | Tee-Object -Variable out; $out | Out-File -FilePath 'build-last-run.log' -Encoding utf8; exit $LASTEXITCODE"
    exit /b !ERRORLEVEL!
)

REM ---- Argumentos ----
set "NOPAUSE=0"
set "BUILD_DIR="
set "WITH_MODELMAKER=0"
set "MM_RELEASE=0"
set "WITH_SELFTEST=1"
set "TESTS_ONLY=0"
set "WITH_WASM=1"

REM ---- Estado de las dos superficies del selftest ----
REM Se inicializan AQUI, no en el paso 9: cualquier `goto :finish` anterior
REM (una compilacion que cae, un ctest en rojo) llega al resumen final, y sin
REM estos valores el bloque de cada superficie saldria con el motivo vacio —
REM "no se ejecuto ()" en vez de decir por que.
set "PLUGIN_RAN=0"
set "PILOT_RAN=0"
set "PLUGIN_SKIP=la pasada no llego al paso 9 del selftest ^(o se pidio noselftest^)"
set "PILOT_SKIP=la pasada no llego al paso 9 del selftest ^(o se pidio noselftest^)"

for %%A in (%*) do (
    if /I "%%A"=="--internal-log" (
        rem bandera del envoltorio de log: ignorar
    ) else if /I "%%A"=="nopause" (
        set "NOPAUSE=1"
    ) else if /I "%%A"=="modelmaker" (
        set "WITH_MODELMAKER=1"
    ) else if /I "%%A"=="release" (
        set "MM_RELEASE=1"
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
echo [2/9] Generando el contrato de parametros y el catalogo de efectos (WebUI\generated)...
cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_ParameterExport NEURONiK_FxExport
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

REM El catalogo de efectos va en el MISMO paso porque la pagina lo necesita para
REM pintar un hueco, y es el otro lado de la misma fuente de verdad que el
REM contrato de parametros: la tabla de efectos del modulo compartido
REM (DspEffects/FxDefaultCatalogue.h). Es un ejecutable aparte porque ese modulo
REM es JUCE-free y el exportador no tiene por que dejar de serlo para leer una
REM tabla de structs.
"%BUILD_DIR%\Release\NEURONiK_FxExport.exe" WebUI\generated
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Fallo al regenerar el catalogo de efectos.
    set "EXIT_CODE=1"
    goto :finish
)

if "%TESTS_ONLY%"=="1" goto :tests

REM El orden IMPORTA, y ahora va de abajo arriba: el .wasm lo produce la WASM,
REM la WebUI lo copia a su dist (publicDir = WebUI\public) y el PLUGIN lo
REM EMBIBE (juce_add_binary_data sobre WebUI\dist/*). Compilar el plugin antes
REM dejaba dentro el bundle de la pasada ANTERIOR.

REM Los enlaces del WORKSPACE (`@abdsynths/shared` y `@abdsynths/midi-keyb`) no se
REM bajan: son enlaces a los repos de al lado, y los escribe pnpm con la ruta tal
REM cual la ve el shell (POSIX), que Node lee como `D:\d\...` y no resuelve. El
REM paso 4 se para entonces con un "Rollup failed to resolve" que no habla de
REM enlaces, y los E2E de navegador se caen por el pre-transform error del dev
REM server. El script los deja como junctions —junction es el unico enlace que
REM `mklink` hace sin pedir administrador— y sale con 1 si algo se queda sin
REM resolver. Va antes del paso 3 porque el WASM sincroniza a
REM WebUI\public\worklet, que es parte del mismo arbol. Detalle medido en
REM Scripts\COMO-ARREGLAR-EL-BUILD.md (sexto atranco).
call "%~dp0Scripts\junctions-workspace.bat"
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] Los enlaces del workspace no resuelven, asi que los pasos que
    echo         usan la WebUI ^(WASM, export y E2E^) fallarian sin decir por que.
    echo         El sexto atranco de Scripts\COMO-ARREGLAR-EL-BUILD.md.
    set "EXIT_CODE=1"
    goto :finish
)

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
REM Guard por HASH (SHA256): el drift no se ve en el codigo (los .js/.wasm se versionan)
REM y solo aparece como audio viejo en la WebUI. workletSyncTest.mjs es la SSOT del hash.
node "%~dp0Tests\workletSyncTest.mjs"
if !ERRORLEVEL! neq 0 (
    echo.
    echo [ERROR] WebUI\public\worklet desincronizado de build-wasm ^(hash SHA256^). El worklet
    echo         quedaria con un DSP VIEJO y el sintoma es mudo.
    set "EXIT_CODE=1"
    goto :finish
)
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
REM Guard por HASH: dist/worklet tiene que ser el DSP recien compilado, no el de la
REM pasada anterior. Compara build-wasm <-> public/worklet <-> dist/worklet.
if "%WITH_WASM%"=="1" (
    node "%~dp0Tests\workletSyncTest.mjs"
    if !ERRORLEVEL! neq 0 (
        echo.
        echo [ERROR] WebUI\dist\worklet desincronizado ^(hash SHA256^). El worklet embebido
        echo         seria un DSP VIEJO. Ejecuta WebUI\scripts\sync-wasm.mjs y pnpm build en WebUI/.
        set "EXIT_CODE=1"
        goto :finish
    )
)
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
REM La lista sale de los add_test de CMakeLists.txt (38 + ABDShared_DspCore_Tests).
REM Para regenerarla tras anadir un test:
REM   grep "^add_test" CMakeLists.txt | cut -d: -f2- | sed "s/add_test(NAME //; s/ COMMAND.*//"
cmake --build "%BUILD_DIR%" --config Release --target NEURONiK_DSPReferenceTest NEURONiK_ModulationMatrixTest NEURONiK_ModulationDest17DriveTest NEURONiK_PresetMigrationParityTest NEURONiK_ModulationContractTest NEURONiK_MidiChannelFilterTest NEURONiK_MidiPortTest NEURONiK_DspReverbParityTest NEURONiK_DspReverbJucePolicyTest NEURONiK_FxSlotsTest NEURONiK_FxCatalogueTest NEURONiK_WasmLayoutOrderTest NEURONiK_AudioBufferParityTest NEURONiK_VelocityCurveTest NEURONiK_LfoSyncTest NEURONiK_ParameterDescriptorTest NEURONiK_PresetRoundTripTest NEURONiK_StatePersistenceTest NEURONiK_ModelSlotTest NEURONiK_MemoryBudgetTest NEURONiK_Vst3LoadTest NEURONiK_ModelMakerRoundTripTest NEURONiK_FactoryPresetAudioTest NEURONiK_SpectralAnalyzerTest NEURONiK_LeastSquaresGridTest NEURONiK_OctaveFamilyTest NEURONiK_Cz101ResidualRangesTest NEURONiK_LayerClusteringTest NEURONiK_LayerViewTest NEURONiK_GridIndicatorTest NEURONiK_TemporalAnalysisTest NEURONiK_FrameSamplerTest NEURONiK_LayerEngineTest NEURONiK_TransposableOffsetsTest NEURONiK_NeurotikBowTest NEURONiK_ParameterBridgeTest NEURONiK_ParameterRandomizerTest NEURONiK_BridgeProtocolContractTest ABDShared_DspCore_Tests
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
set "PILOT_SKIP=el plugin no llego a la bancada"
if not exist "%PLUGIN_STANDALONE%" (
    echo [AVISO] Standalone del plugin no disponible, selftest del plugin omitido.
    set "EXIT_CODE=1"
    set "PLUGIN_SKIP=no hay Standalone que ejecutar"
    set "PILOT_SKIP=el Standalone del plugin no existe"
    goto :finish
)

REM El transcript queda junto al log de la compilacion, para poder leer el detalle
REM sin depender del stdout (y ahi es donde lo busca quien depura un FAIL).
set "NEURONIK_SELFTEST_LOG=%~dp0%BUILD_DIR%\neuronik-selftest.log"
"%PLUGIN_STANDALONE%" --selftest
set "PLUGIN_ST=%ERRORLEVEL%"
set "PLUGIN_RAN=1"
if not "%PLUGIN_ST%"=="0" (
    echo.
    echo [ERROR] El selftest del plugin fallo: alguna direccion no se movio.
    echo         Detalle: %NEURONIK_SELFTEST_LOG%
    set "EXIT_CODE=1"
    set "PILOT_SKIP=el selftest del plugin fallo y la pasada se corto ahi"
    goto :finish
)
echo [OK] Plugin verificado: ver el resumen por direccion al final de esta pasada.

REM Solo si la bancada compilo y la pagina existe: sin pagina que cargar no hay E2E.
REM La bancada sirve WebUI\dist (la MISMA pagina que embebe el plugin), asi que este
REM selftest no depende de ninguna otra exportacion: lo que comprueba es lo que se
REM envia.
set "PILOT_HOST=%BUILD_DIR%\NEURONiK_WebPilotHost_artefacts\Release\NEURONiK Web Pilot.exe"
if "!HOST_BUILD_FAILED!"=="1" (
    echo [AVISO] La bancada NO recompilo en esta pasada: el enlace borro el exe anterior.
    echo         Selftest omitido para no dar un OK enganoso.
    set "EXIT_CODE=1"
    set "PILOT_SKIP=la bancada no recompilo en esta pasada"
    goto :finish
)
if not exist "%PILOT_HOST%" (
    echo [AVISO] Bancada no disponible, selftest omitido.
    set "PILOT_SKIP=no hay ejecutable de bancada"
    goto :finish
)
if not exist "WebUI\dist\index.html" (
    echo [AVISO] WebUI\dist no existe: sin pagina que cargar no hay E2E ^(y el plugin
    echo         tampoco embebio interfaz^). Selftest de la bancada omitido.
    set "PILOT_SKIP=no hay pagina en WebUI\dist que cargar"
    goto :finish
)

REM La bancada escribe su selftest en un log ACUMULATIVO con marca de tiempo, el
REM MISMO formato que el plugin (politica en Source\WebUI\SelftestLog.h), asi que
REM el resumen final lo lee igual que al del plugin. Antes era un transcript de
REM UNA pasada, reescrito desde cero y sin marcas, y por eso el resumen necesitaba
REM un modo `stdout` aparte: comparar una pasada fallida con la anterior era
REM imposible porque no quedaba rastro de la anterior.
REM
REM NO se borra antes de correr: acumular es el punto. Para empezar de cero hay
REM que borrar el fichero a mano.
set "BENCH_TRANSCRIPT=%~dp0%BUILD_DIR%\neuronik-selftest-bancada.log"
set "NEURONIK_SELFTEST_LOG_BANCADA=%BENCH_TRANSCRIPT%"

"%PILOT_HOST%" --selftest
REM El exit se lee ANTES de nada: cualquier comando posterior (type, echo) pone
REM ERRORLEVEL a 0, y leerlo despues hacia que una bancada en FAIL saliera con
REM exit 0 y la pasada se cerrara en verde.
set "PILOT_ST=%ERRORLEVEL%"
set "PILOT_RAN=1"
if not "%PILOT_ST%"=="0" (
    echo.
    echo [ERROR] El selftest de la BANCADA fallo: alguna direccion no se movio.
    echo         Detalle: %BENCH_TRANSCRIPT% ^(acumulativo: las pasadas anteriores siguen ahi^)
    set "EXIT_CODE=1"
    goto :finish
)
echo [OK] Bancada verificada: las mismas direcciones sobre la MISMA pagina del plugin.

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
REM ---- Resumen del selftest: veredicto por direccion + rutas de los logs -----
REM Los DOS transcripts son acumulativos y con marca de tiempo (politica en
REM Source\WebUI\SelftestLog.h): cada corrida anade lineas y se cierra con una
REM linea "veredicto:". El resumen parsea Scripts\selftest_summary.ps1 desde la
REM ULTIMA corrida CERRADA de cada uno: si esta pasada no llego a renovarla
REM (proceso muerto antes de terminar), el aviso de fecha lo dice en vez de
REM pintar un OK de otra pasada. Sin log o sin corrida, el resumen lo dice en
REM vez de callar. La llamada es un proceso hijo del tee: su stdout lo captura el
REM envoltorio y por eso el resumen tambien queda en build-last-run.log.
set "SELFTEST_LOG=%NEURONIK_SELFTEST_LOG%"
if "%SELFTEST_LOG%"=="" set "SELFTEST_LOG=%~dp0%BUILD_DIR%\neuronik-selftest.log"

echo.
echo =======================================================
if "%EXIT_CODE%"=="0" (
    echo  RESULTADO: OK
) else (
    echo  RESULTADO: CON ERRORES ^(codigo %EXIT_CODE%^)
)
echo =======================================================

if "%WITH_SELFTEST%"=="1" (
    echo  Resumen del selftest ^(por direccion, una superficie cada una^):
    echo.
    if "%PLUGIN_RAN%"=="1" (
        powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\selftest_summary.ps1" "%SELFTEST_LOG%" "PLUGIN ^(Standalone, exit %PLUGIN_ST%^)" log
    ) else (
        echo  PLUGIN ^(Standalone^): no se ejecuto ^(%PLUGIN_SKIP%^).
    )
    if "%PILOT_RAN%"=="1" (
        powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\selftest_summary.ps1" "%BENCH_TRANSCRIPT%" "BANCADA ^(WebPilotHost, exit %PILOT_ST%^)" log
    ) else (
        echo  BANCADA ^(WebPilotHost^): no se ejecuto ^(%PILOT_SKIP%^).
    )
) else (
    echo  Selftest omitido en esta pasada ^(noselftest / tests^).
)
echo =======================================================
if "%NOPAUSE%"=="1" exit /b %EXIT_CODE%
echo.
pause
exit /b %EXIT_CODE%
