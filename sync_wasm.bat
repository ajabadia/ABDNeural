@echo off
REM ============================================================================
REM  NEURONiK - Sincroniza los artefactos WASM del DSP hacia la WebUI
REM
REM  Copia build-wasm/neuronik_dsp.{js,wasm} -> WebUI/public/worklet/.
REM  Ejecutar despues de build_wasm.bat cuando cambie el DSP (Fase 5):
REM  el AudioWorklet sirve el binario desde la exportacion estatica.
REM
REM  Uso:  sync_wasm.bat     (desde ABDNeural/)
REM ============================================================================

setlocal
cd /d "%~dp0"

node WebUI\scripts\sync-wasm.mjs
if errorlevel 1 (
    echo [ERROR] Fallo la sincronizacion WASM -^> WebUI. Revisa el mensaje de arriba.
    exit /b 1
)

echo [OK] Artefactos WASM sincronizados en WebUI\public\worklet
endlocal
