@echo off
setlocal EnableExtensions EnableDelayedExpansion
REM ============================================================================
REM  NEURONiK - Compilacion WebAssembly (Fase 5)
REM
REM  Compila el DSP real (sin port) sobre la frontera de la Fase 1:
REM    JS -> NeuronikWasmBridge (extern "C") -> DspEngineFacade -> motor
REM
REM  Artefactos: build-wasm/neuronik_dsp.js + neuronik_dsp.wasm (ES6, MODULARIZE)
REM  Requiere: emsdk (autodetectado en C:\emsdk) y Visual Studio (vcvars64).
REM
REM  NOTA: se necesita el entorno de Visual Studio ANTES de emsdk para que el
REM  bootstrap de juceaide (cross-compile) encuentre MSVC y no un MinGW del
REM  PATH (JUCE lo rechaza). Por eso vcvars64 va primero.
REM
REM  QUE GENERA TAMBIEN, Y POR QUE ESTA AQUI. Ademas del binario, esta pasada
REM  regenera WebUI\generated\gp-layout.generated.js --la firma del layout-- y
REM  comprueba que el binario y esa tabla cuadran antes de dar el build por bueno.
REM  Son dos artefactos de la MISMA pasada porque los dos se leen del arbol: si
REM  uno se regenera en una vuelta y el otro en otra, ambos son correctos por
REM  separado y se contradicen, y el unico sintoma es un aviso en la linea de
REM  audio. Recogerlo aqui y no en un segundo comando es justo el punto: un paso
REM  que hay que recordar es un paso que se olvida.
REM
REM  Y AVISA SI EL ARBOL DEL LAYOUT ESTA SUCIO. Antes de regenerar la firma mira
REM  Source/ y WebUI/generated/ y, si hay cambios sin commitear, los lista. No
REM  detiene el build, y no por descuido: la firma y el `.wasm` los lee el mismo
REM  arbol en esta misma pasada, asi que los artefactos que salen ACUERDAN entre
REM  si. El aviso va de otra cosa --artefactos nuevos junto a codigo viejo si se
REM  commitean por separado--, que es problema de quien commitea, no de quien
REM  compila. Detener aqui impediria compilar con cambios a medias, que es como
REM  se trabaja la mayor parte del tiempo.
REM
REM  Uso:  build_wasm.bat            -> compila, valida y sincroniza; PAUSA final
REM        build_wasm.bat nopause   -> sin pausa (para automatizacion)
REM  Cada pasada deja ademas wasm-last-run.log (log espejo de la consola).
REM ============================================================================
REM ---- Log espejo: relanza el script internamente y teed consola+fichero ----

REM      (mismo patron que build.bat: deja wasm-last-run.log en la raiz, asi el
REM      resultado queda en disco aunque la ventana se cierre sin querer)
if not "%~1"=="--internal-log" (
    powershell -NoProfile -Command "& cmd /c '\"%~f0\" --internal-log %*' 2>&1 | Tee-Object -Variable out; $out | Out-File -FilePath 'wasm-last-run.log' -Encoding utf8; exit $LASTEXITCODE"
    exit /b !ERRORLEVEL!
)

REM ---- Argumentos -------------------------------------------------------------
set "NOPAUSE=0"
for %%A in (%*) do (
    if /I "%%A"=="nopause" set "NOPAUSE=1"
)

REM --- 1. Localizar emsdk -----------------------------------------------------
set "EMSDK_ROOT="
if defined EMSDK set "EMSDK_ROOT=%EMSDK%"
if not defined EMSDK_ROOT if exist "C:\emsdk\emsdk_env.bat" set "EMSDK_ROOT=C:\emsdk"
if not defined EMSDK_ROOT (
    echo [ERROR] emsdk no encontrado: define EMSDK o instala en C:\emsdk
    goto :fail
)

REM --- 2. Localizar Visual Studio y preparar entorno ---------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSROOT="
REM  `-products *` NO es opcional: sin el, vswhere solo lista los productos con
REM  licencia completa y las Build Tools NO aparecen. Medido el 2026-10-02 en esta
REM  maquina, donde lo unico instalado son las Visual Studio Build Tools 2026: la
REM  llamada sin el flag devolvia VACIO, y con rc 0, que es lo que hace que esto
REM  parezca un "no hay Visual Studio" en vez de un "no se que versions hay".
REM
REM  Y el `for /f` va FUERA del `if exist`, y no dentro como estaba, porque DENTRO
REM  de un bloque el asterisco no llega al comando que se ejecuta: batch lo expande
REM  antes y el .bat deja de analizar entero con un "No se esperaba \Microsoft en
REM  este momento" que no senala la linea culpable. Medido en este fichero real: el
REM  mismo `for /f` con el asterisco FUERA del bloque funciona y devuelve la ruta de
REM  las Build Tools; DENTRO, el .bat ni siquiera llega a ejecutar la linea. Con
REM  `-products MSBuildProduct` en vez del asterisco no hay error de analisis pero
REM  tampoco lista nada, porque esta instalacion no se registra con ese producto.
REM
REM  Por eso aqui ya no hay ningun `if`: si el vswhere no esta, el `for /f` falla
REM  y deja VSROOT vacio, que es justo lo que mira el `if not defined` de abajo.
REM
REM  Y el vcvars64 se `call` con comillas, como estaba: la ruta de las Build Tools
REM  lleva "Program Files (x86)" con un parentesis dentro, y eso dentro de un bloque
REM  if rompe el analisis de la linea. Medido: el `if not exist "%VCVARS%"` de abajo
REM  con esa ruta dentro del bloque deja el .bat sin analizar con un "No se esperaba
REM  \Microsoft en este momento". Por eso las comprobaciones de ruta van FUERA del
REM  bloque, con el `if ... else` en una linea, que si aguanta el parentesis.
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT (
    echo [ERROR] Visual Studio no encontrado via vswhere
    goto :fail
)
set "VCVARS=%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" echo [ERROR] No existe %VCVARS% & goto :fail

echo [1/7] Preparando entorno Visual Studio + emsdk ...
call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] vcvars64 fallo
    goto :fail
)
REM NOTA: no usamos emsdk_env.bat porque bajo Git Bash (MSYSTEM definido) solo
REM imprime exports de sh en vez de hacer set. Montamos el PATH a mano:
set "EMSDK=%EMSDK_ROOT%"
set "PATH=%EMSDK_ROOT%\upstream\emscripten;%EMSDK_ROOT%;%PATH%"
for /d %%D in ("%EMSDK_ROOT%\node\*") do set "EMSDK_NODE=%%D\bin\node.exe"
for /d %%D in ("%EMSDK_ROOT%\python\*") do set "EMSDK_PYTHON=%%D\python.exe"
for /d %%D in ("%EMSDK_ROOT%\node\*") do set "PATH=%%D\bin;%PATH%"
where cl.exe >nul 2>&1 || (echo [ERROR] cl.exe no esta en PATH tras vcvars64 & goto :fail)
where emcmake >nul 2>&1 || (echo [ERROR] emcmake no esta en PATH: falta %EMSDK_ROOT%\upstream\emscripten & goto :fail)

REM --- 3. Configurar, firma y compilar ----------------------------------------
echo [2/7] Configurando (emcmake + Ninja) ...
rem Generador Ninja: el generador Visual Studio no soporta el compilador
rem em++ del toolchain Emscripten. El entorno de vcvars ya esta armado,
rem asi que el bootstrap de juceaide encuentra MSVC sin problema.
if not exist build-wasm mkdir build-wasm
pushd .
rem 2>&1 en el configure: emcmake escribe su banner informativo a stderr y,
rem dentro del log espejo (PowerShell Tee), saldria como NativeCommandError
rem falso. Fusionarlo con stdout lo deja como texto normal.
emcmake cmake -S wasm -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release 2>&1
if errorlevel 1 goto :fail
popd
echo [3/7] Regenerando la firma del layout (mismo arbol que el binario) ...
REM El binario y la tabla que lee la pagina tienen que salir del MISMO arbol.
REM Antes eran dos pasos que recordar --compilar el .wasm por un lado, regenerar
REM la firma por otro-- y el que se olvidaba no hacia ruido en ninguna parte: el
REM .wasm viejo pasaba todos los tests y el desajuste solo aparecia como un aviso
REM en la linea de audio, que es donde nadie mira. Aqui la firma se regenera
REM siempre, en la misma pasada, antes de compilar: si el arbol del layout esta
REM movido, esta tabla y el binario de abajo lo leen igual y no pueden mentir.
REM
REM Va ANTES que la compilacion de emscripten a proposito: si el exportador
REM falla, se pierde en segundos y no despues de minutos de compilacion.

REM --- AVISO: el arbol del layout esta sucio ------------------------------
REM NO es un fallo, y esa decision es deliberada. Aqui la firma y el `.wasm`
REM los leen los DOS del mismo arbol y en esta misma pasada, asi que los
REM artefactos que salgan ACUERDAN entre si: el desajuste que este paso evita
REM no puede ocurrir dentro de una sola ejecucion. Lo que un arbol sucio puede
REM producir es otra cosa --artefactos nuevos junto a codigo viejo, si alguien
REM los commitea por separado-- y eso lo decide quien commitea, no quien
REM compila. FALLAR aqui impediria compilar con cambios a medias, que es
REM exactamente como se trabaja la mayor parte del tiempo.
REM
REM El ambito es el mismo que usa el vigilante: lo que alimenta el layout
REM (Source/) y lo que el exportador escribe (WebUI/generated/).
set "LAYOUT_DIRTY="
for /f "usebackq delims=" %%L in (`git status --porcelain -- Source WebUI/generated 2^>nul`) do set "LAYOUT_DIRTY=1"
if defined LAYOUT_DIRTY (
    echo.
    echo   [AVISO] El arbol del layout tiene cambios SIN COMMITEAR:
    git status --porcelain -- Source WebUI/generated
    echo     La firma y el .wasm saldran de ESTE arbol, asi que los dos
    echo     artefactos ACUERDAN entre si y el build es coherente. Lo que no
    echo     debe pasar es commitear el .wasm y gp-layout.generated.js sin el
    echo     codigo que los produjo.
    echo.
)
cmake --build build-reference --config Release --target NEURONiK_LayoutExport
if errorlevel 1 goto :fail
REM La ruta del exportador es ABSOLUTA: este script se puede lanzar desde
REM cualquier directorio, y una ruta relativa escribiria la tabla en el sitio
REM de quien lo lanzo en vez de en el repositorio.
"%~dp0build-reference\Release\NEURONiK_LayoutExport.exe" "%~dp0WebUI\generated"
if errorlevel 1 goto :fail

echo [4/7] Compilando ...
cmake --build build-wasm --config Release
if errorlevel 1 goto :fail

REM --- 4. Paridad y smoke test Node -------------------------------------------
echo [5/7] Referencia nativa + test de paridad WASM contra nativo ...
REM La referencia nativa ejecuta los MISMOS 5 escenarios que el test Node
REM re-ejecuta sobre el modulo WASM: si difieren, el DSP o la frontera han
REM cambiado por un lado y no por el otro.
cmake --build build-reference --config Release --target NEURONiK_WasmParityTest
if errorlevel 1 goto :fail
"%~dp0build-reference\Release\NEURONiK_WasmParityTest.exe" "%~dp0build-wasm\parity-native.json"
if errorlevel 1 goto :fail
node "%~dp0Tests\neuronik_wasm_parity.mjs" "%~dp0build-wasm\neuronik_dsp.js" "%~dp0build-wasm\parity-native.json"
if errorlevel 1 goto :fail

echo [6/7] Smoke test Node del modulo WASM ...
node "%~dp0Tests\neuronik_wasm_smoke.mjs" "%~dp0build-wasm\neuronik_dsp.js"
if errorlevel 1 goto :fail

REM --- 6. Sincronizar los artefactos que sirve la WebUI ------------------------
REM Sin este paso el worklet de WebUI\public\worklet se queda en el DSP de la
REM pasada anterior: el drift no se ve en el codigo (los .js/.wasm se versionan)
REM y solo aparece como audio viejo en la WebUI. Ocurrio dos veces; ahora es
REM imposible por construccion.
echo [7/7] Sincronizando artefactos con WebUI\public\worklet ...
node "%~dp0WebUI\scripts\sync-wasm.mjs"
if errorlevel 1 goto :fail

REM Guard por HASH (SHA256): el drift no se ve en el codigo y solo aparece como
REM audio viejo en la WebUI. Compara build-wasm <-> public/worklet (y dist si existe).
node "%~dp0Tests\workletSyncTest.mjs"
if errorlevel 1 goto :fail

REM Cierre: la firma del layout contra el binario YA sincronizado, que es el
REM que carga el AudioWorklet. Va al final y no al principio a proposito: hasta
REM que sync-wasm.mjs ha copiado, el binario de public\worklet es el de la
REM pasada anterior y compararlo aqui solo daria ruido. Aqui ya estan las tres
REM mitades de la misma pasada --la tabla que leen la pagina, el binario nuevo
REM y el arbol del que salieron-- asi que si algo se desajusta, el build falla
REM en vez de dejar un aviso en la linea de audio. El mismo test que corre
REM como NEURONiK_WasmLayoutFingerprint en ctest, para que compilar y testear
REM no puedan discrepar.
node "%~dp0Tests\wasmLayoutFingerprintTest.mjs"
if errorlevel 1 goto :fail

echo.
echo =======================================================
echo  [EXITO] WASM compilado y validado (firma + paridad + smoke + sync)
echo    build-wasm\neuronik_dsp.js
echo    build-wasm\neuronik_dsp.wasm
echo    build-wasm\parity-native.json  (referencia nativa)
echo    WebUI\public\worklet\      (sincronizado)
echo    WebUI\generated\gp-layout.generated.js  (firma, del mismo arbol)
echo =======================================================
set "EXIT_CODE=0"
goto :finish

:fail
popd 2>nul
echo.
echo =======================================================
echo  RESULTADO: CON ERRORES  (build WASM abortado)
echo =======================================================
set "EXIT_CODE=1"

:finish
echo.
echo  Log espejo: wasm-last-run.log
if not "%NOPAUSE%"=="1" pause
exit /b %EXIT_CODE%
