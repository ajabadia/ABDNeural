@echo off
setlocal EnableExtensions
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
REM ============================================================================

REM --- 1. Localizar emsdk -----------------------------------------------------
set "EMSDK_ROOT="
if defined EMSDK set "EMSDK_ROOT=%EMSDK%"
if not defined EMSDK_ROOT if exist "C:\emsdk\emsdk_env.bat" set "EMSDK_ROOT=C:\emsdk"
if not defined EMSDK_ROOT (
    echo [ERROR] emsdk no encontrado: define EMSDK o instala en C:\emsdk
    exit /b 1
)

REM --- 2. Localizar Visual Studio y preparar entorno ---------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSROOT="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSROOT=%%i"
)
if not defined VSROOT (
    echo [ERROR] Visual Studio no encontrado via vswhere
    exit /b 1
)
set "VCVARS=%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo [ERROR] No existe %VCVARS%
    exit /b 1
)

echo [1/5] Preparando entorno Visual Studio + emsdk ...
call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] vcvars64 fallo
    exit /b 1
)
REM NOTA: no usamos emsdk_env.bat porque bajo Git Bash (MSYSTEM definido) solo
REM imprime exports de sh en vez de hacer set. Montamos el PATH a mano:
set "EMSDK=%EMSDK_ROOT%"
set "PATH=%EMSDK_ROOT%\upstream\emscripten;%EMSDK_ROOT%;%PATH%"
for /d %%D in ("%EMSDK_ROOT%\node\*") do set "EMSDK_NODE=%%D\bin\node.exe"
for /d %%D in ("%EMSDK_ROOT%\python\*") do set "EMSDK_PYTHON=%%D\python.exe"
for /d %%D in ("%EMSDK_ROOT%\node\*") do set "PATH=%%D\bin;%PATH%"
where cl.exe >nul 2>&1 || (echo [ERROR] cl.exe no esta en PATH tras vcvars64 & exit /b 1)
where emcmake >nul 2>&1 || (echo [ERROR] emcmake no esta en PATH: falta %EMSDK_ROOT%\upstream\emscripten & exit /b 1)

REM --- 3. Configurar y compilar ------------------------------------------------
echo [2/5] Configurando (emcmake + Ninja) ...
rem Generador Ninja: el generador Visual Studio no soporta el compilador
rem em++ del toolchain Emscripten. El entorno de vcvars ya esta armado,
rem asi que el bootstrap de juceaide encuentra MSVC sin problema.
if not exist build-wasm mkdir build-wasm
pushd .
emcmake cmake -S wasm -B build-wasm -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :fail
popd

echo [3/5] Compilando ...
cmake --build build-wasm --config Release
if errorlevel 1 goto :fail

REM --- 4. Smoke test Node ------------------------------------------------------
echo [4/5] Referencia nativa + test de paridad WASM contra nativo ...
REM La referencia nativa ejecuta los MISMOS 4 escenarios que el test Node
REM re-ejecuta sobre el modulo WASM: si difieren, el DSP o la frontera han
REM cambiado por un lado y no por el otro.
cmake --build build-reference --config Release --target NEURONiK_WasmParityTest
if errorlevel 1 goto :fail
"%~dp0build-reference\Release\NEURONiK_WasmParityTest.exe" "%~dp0build-wasm\parity-native.json"
if errorlevel 1 goto :fail
node "%~dp0Tests\neuronik_wasm_parity.mjs" "%~dp0build-wasm\neuronik_dsp.js" "%~dp0build-wasm\parity-native.json"
if errorlevel 1 goto :fail

echo [5/5] Smoke test Node del modulo WASM ...
node "%~dp0Tests\neuronik_wasm_smoke.mjs" "%~dp0build-wasm\neuronik_dsp.js"
if errorlevel 1 goto :fail

echo.
echo =======================================================
echo  [EXITO] WASM compilado y validado (paridad + smoke)
echo    build-wasm\neuronik_dsp.js
echo    build-wasm\neuronik_dsp.wasm
echo    build-wasm\parity-native.json  (referencia nativa)
echo =======================================================
exit /b 0

:fail
popd 2>nul
echo.
echo [FALLO] Build WASM abortado. Revisa el error de arriba.
exit /b 1
