<#
.SYNOPSIS
    Master Management Script for NEURONiK Synthesizer.
    Consolidates build, clean, test, and package tasks.

.DESCRIPTION
    Un script que escribe en el repo no se puede probar sin ensuciarlo. Este tiene
    doseffects sobre ficheros del arbol:

      - `-Task build`bumpea la version: escribe `build_no.txt` e
        `Source/Core/BuildVersion.h`. El segundo esta VERSIONADO y lleva un timestamp,
        asi que cada build deja el repo modificado con algo que no es un cambio real.
      - `-Task clean` borra `build_neuronik/` entero, con `Remove-Item -Recurse -Force`.

    `-Check` cubre los dos y no toca nada:

      - Con `-Task build`, dice que numero pondria y que ficheros dejaria
        modificados, y sale 1 si lo que hay en disco no es lo que este script
        produjo -que es el unico fallo comprobable, porque el numero y el timestamp
        cambian por diseno.
      - Con `-Task clean`, dice QUE BORRARIA y cuanto ocupa, y sale 0. Borrar no se
        puede "verificar": se avisa, y quien quiera borrar, lo borra a proposito.

    Lo que `-Check` NO hace, y conviene no creer sin mirar:

      - No compila. Ni cmake ni el resto del build se ejecutan con `-Check`; lo que
        se comprueba son los ficheros, no el motor, que tiene su propio CI.
      - No dice si el numero de version "esta al dia". Este script INCREMENTA, no
        genera: el 28 vale si el bump no se ha hecho y vale si se ha hecho y se ha
        commiteado. Lo unico que dice es que haria ahora, y eso se lee en voz alta
        para que no se confunda con un veredicto.

.EXAMPLE
    .\Scripts\manage.ps1 -Task build -Config Release
    .\Scripts\manage.ps1 -Task clean
    .\Scripts\manage.ps1 -Task test
    .\Scripts\manage.ps1 -Task build -Check
    .\Scripts\manage.ps1 -Task clean -Check
#>

param (
    # SIN `ValidateSet`, y a proposito. `ValidateSet` deja pasar el valor pero hace
    # que PowerShell se queje EN INGLES y salga con 1, que es el codigo de un error
    # de ejecucion. Un `-Task deploy` en un pipeline no es un fallo: es una llamada
    # equivocada, y merece su propio codigo (2) y un mensaje que diga quais son las
    # que existen. Se valida mas abajo, en el flujo principal.
    [Parameter(Mandatory = $false)]
    [string]$task,

    [Parameter(Mandatory = $false)]
    [string]$config = "Release",

    [Parameter(Mandatory = $false)]
    [switch]$FullClean,

    [Parameter(Mandatory = $false)]
    [string]$Target = "NEURONiK_Standalone",

    # NO ESCRIBE NADA. Mira lo que se escribiria o se borraria, y lo dice.
    [Parameter(Mandatory = $false)]
    [switch]$Check,

    [Parameter(Mandatory = $false)]
    [Alias("h", "?")]
    [switch]$Help
)

$ErrorActionPreference = "Stop"

# Las funciones de PowerShell no heredan el ambito del script, asi que `-Check` se
# guarda a nivel de script para que `Update-BuildVersion` y `Invoke-TaskClean` -que
# no reciben parametros- puedan leerlo. Sin esto, el flag se quedaria en el ambito
# principal y las funciones harian exactamente lo que harian sin el.
$script:Check = $Check

# --- Configuration ---
$ProjectRoot = Get-Item $PSScriptRoot\..
$BuildDir = "$($ProjectRoot.FullName)\build_neuronik"
$AppName = "NEURONiK"

# Try to find JUCE via environment variable first, or fallback to C:\JUCE
$JuceDir = $env:JUCE_PATH
if (-not $JuceDir -and (Test-Path "C:\JUCE")) { $JuceDir = "C:\JUCE" }

function Show-Help {
    Write-Host "Usage: .\Scripts\manage.ps1 -Task <Task> [-Config <Config>] [-FullClean] [-Check] [-Help]" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Tasks:" -ForegroundColor White
    Write-Host "  build     - Configures and builds the project (incremental by default)."
    Write-Host "  clean     - Removes the build directory (closes app if running)."
    Write-Host "  test      - Runs unit tests using ctest."
    Write-Host "  sign      - Not implemented."
    Write-Host ""
    Write-Host "Flags:" -ForegroundColor White
    Write-Host "  -Config      - Configuration to build (Release [default] or Debug)."
    Write-Host "  -FullClean   - When used with 'build', performs a full wipe of the build folder first."
    Write-Host "  -Check       - NO writes anything. Says what it would write or delete, and exits 1 if the"
    Write-Host "                 generated files in the repo are not the ones this script produces."
    Write-Host "  -Help / -h   - Shows this help message."
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Cyan
    Write-Host "  .\Scripts\manage.ps1 -Task build"
    Write-Host "  .\Scripts\manage.ps1 -Task build -Config Debug -FullClean"
    Write-Host "  .\Scripts\manage.ps1 -Task build -Check"
    Write-Host "  .\Scripts\manage.ps1 -Help"
}

function Show-Header {
    Write-Host "=========================================" -ForegroundColor Cyan
    Write-Host "  NEURONiK Synthesizer Management Script   " -ForegroundColor Cyan
    Write-Host "=========================================" -ForegroundColor Cyan
    Write-Host "Task: $task | Config: $config"
    if ($FullClean) { Write-Host "Mode: Full Clean Build" -ForegroundColor Yellow }
    if ($script:Check) { Write-Host "Mode: CHECK (no se escribe nada)" -ForegroundColor Yellow }
    Write-Host ""
}

function Stop-AppProcess {
    if ($script:Check) { return }
    $proc = Get-Process $AppName -ErrorAction SilentlyContinue
    if ($proc) {
        Write-Host "[INFO] Closing running instance of $AppName..." -ForegroundColor Yellow
        $proc | Stop-Process -Force
        Start-Sleep -Seconds 1 # Give OS time to release file locks
    }
}

function Find-VSPath {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath
        return $path
    }
    return $null
}

function Find-CMake {
    # 1. Try PATH
    if (Get-Command cmake -ErrorAction SilentlyContinue) { return "cmake" }

    # 2. Try vswhere
    $vsPath = Find-VSPath
    if ($vsPath) {
        $potentialCMake = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path $potentialCMake) { return $potentialCMake }
    }

    # 3. Try standard paths
    $PotentialPaths = @(
        "${env:ProgramFiles}\CMake\bin\cmake.exe",
        "C:\Program Files\CMake\bin\cmake.exe"
    )

    foreach ($path in $PotentialPaths) {
        if (Test-Path $path) { return $path }
    }
    return $null
}

function Find-VSVars {
    $vsPath = Find-VSPath
    if ($vsPath) {
        $vsvars = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"
        if (Test-Path $vsvars) { return $vsvars }
    }

    $PotentialPaths = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"
    )

    foreach ($path in $PotentialPaths) {
        if (Test-Path $path) { return $path }
    }
    return $null
}

function Update-BuildVersion {
    $versionFile = Join-Path $ProjectRoot.FullName "build_no.txt"
    $headerDir = Join-Path $ProjectRoot.FullName "Source/Core"
    $headerFile = Join-Path $headerDir "BuildVersion.h"

    $buildNo = 0
    if (Test-Path $versionFile) {
        $buildNo = [int](Get-Content $versionFile)
    }
    $buildNo++

    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $headerContent = @'
/* Auto-generated build version file */
#pragma once

#define NEURONIK_BUILD_VERSION "@BUILD_NO@"
#define NEURONIK_BUILD_TIMESTAMP "@TIMESTAMP@"
'@ -replace "@BUILD_NO@", $buildNo -replace "@TIMESTAMP@", $timestamp

    if ($script:Check) {
        # LO QUE SE COMPARA, Y LO QUE NO
        #
        # El numero y el timestamp cambian en cada ejecucion por diseno, asi que
        #comparar el fichero entero daria rojo siempre y no diria nada. Lo que se
        # compara es si el fichero en disco tiene la FORMA que produce este script:
        # esos dos defines, con esos nombres, en ese fichero. Si alguien lo ha
        # reescrito a mano, o ha desaparecido, eso si es un fallo real y sale 1.
        Write-Host "DRY-RUN -Update-BuildVersion"
        Write-Host "  build_no.txt       : $(([int]$buildNo - 1)) -> $buildNo (NO VERSIONADO: se regenera localmente)"
        Write-Host "  Source/Core/BuildVersion.h :"
        Write-Host "      NEURONIK_BUILD_VERSION    = $buildNo (lo que se escribiria)"
        Write-Host "      NEURONIK_BUILD_TIMESTAMP  = $timestamp (cambia en cada ejecucion, por diseno)"

        if (-not (Test-Path $headerFile)) {
            Write-Host ""
            Write-Host "DESFASADO - $headerFile no existe."
            Write-Host "Arreglo: .\Scripts\manage.ps1 -Task build, y commit de $headerFile."
            return 1
        }

        $enDisco = Get-Content $headerFile -Raw
        $falta = @()
        if ($enDisco -notmatch '#define NEURONIK_BUILD_VERSION\s+"\d+"') { $falta += "NEURONIK_BUILD_VERSION" }
        if ($enDisco -notmatch '#define NEURONIK_BUILD_TIMESTAMP\s+"[^"]+"') { $falta += "NEURONIK_BUILD_TIMESTAMP" }

        if ($falta.Count -gt 0) {
            Write-Host ""
            Write-Host "DESFASADO - a $headerFile le falta: $($falta -join ', ')"
            Write-Host "Arreglo: .\Scripts\manage.ps1 -Task build, y commit de $headerFile."
            return 1
        }

        Write-Host ""
        Write-Host "OK - $headerFile tiene la forma que produce este script."
        Write-Host "-Check NO ha escrito nada. Ojo con lo que esto NO es: no dice que el numero de"
        Write-Host "version este al dia. Este script INCREMENTA en vez de generar, asi que el numero"
        Write-Host "cambia en cada build y la linea de arriba es la que HABRIA, no la que deberia haber."
        Write-Host "Que el ultimo bump quedara commiteado se mira en git status."
        return 0
    }

    if (!(Test-Path $headerDir)) { New-Item -ItemType Directory -Path $headerDir -Force | Out-Null }

    $buildNo | Set-Content $versionFile
    $headerContent | Set-Content $headerFile -Encoding UTF8
    Write-Host "[INFO] Build #$buildNo updated at $timestamp" -ForegroundColor Cyan
    return 0
}

function Invoke-TaskBuild {
    if ($script:Check) {
        # No se compila, no se busca cmake, no se cierra la app. Lo que se mira es
        # unicamente lo que este script escribe, que es lo unico suyo.
        $r = Update-BuildVersion
        if ($r -ne 0) { return $r }
        Write-Host ""
        Write-Host "-Task build con -Check NO ha compilado nada. Aqui no se comprueba el motor:"
        Write-Host "cmake y la compilacion tienen su propio sitio. Este flag mira los ficheros."
        return 0
    }

    Write-Host "Starting Build Process..." -ForegroundColor Green

    if ($FullClean) {
        Invoke-TaskClean
    }
    else {
        Stop-AppProcess
    }

    Update-BuildVersion | Out-Null

    $CMakePath = Find-CMake
    if (!$CMakePath) { throw "CMake not found. Please install CMake or run from VS Developer Command Prompt." }
    Write-Host "Found CMake: $CMakePath"

    $vsVars = Find-VSVars
    $GeneratorParams = @("-B", $BuildDir)

    if ($JuceDir) {
        Write-Host "Setting JUCE Path: $JuceDir"
        $GeneratorParams += "-DCMAKE_PREFIX_PATH=`"$JuceDir`""
        $GeneratorParams += "-DJUCE_PATH=`"$JuceDir`""
    }

    if ($vsVars -and $vsVars -match "2022") {
        Write-Host "Using Visual Studio 17 2022 Generator..."
        $GeneratorParams += "-G", "Visual Studio 17 2022", "-A", "x64"
    }
    elseif ($vsVars -and $vsVars -match "2019") {
        Write-Host "Using Visual Studio 16 2019 Generator..."
        $GeneratorParams += "-G", "Visual Studio 16 2019", "-A", "x64"
    }
    else {
        Write-Host "Letting CMake auto-detect generator..."
    }

    if (!(Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }

    Write-Host "Configuring CMake..."
    & $CMakePath @GeneratorParams "$($ProjectRoot.FullName)"
    if ($LASTEXITCODE -ne 0) { throw "CMake Configuration Failed" }

    Write-Host "Building Project ($config) Target ($Target)..."
    & $CMakePath --build $BuildDir --config $config --target $Target
    if ($LASTEXITCODE -ne 0) { throw "Build Failed" }

    Write-Host "Build Completed Successfully!" -ForegroundColor Green
    return 0
}

function Invoke-TaskClean {
    if ($script:Check) {
        if (Test-Path $BuildDir) {
            $n = @(Get-ChildItem $BuildDir -Recurse -File -ErrorAction SilentlyContinue).Count
            $mb = [math]::Round((Get-ChildItem $BuildDir -Recurse -File -ErrorAction SilentlyContinue |
                Measure-Object -Property Length -Sum).Sum / 1MB, 1)
            $plural = if ($n -eq 1) { "fichero" } else { "ficheros" }
            Write-Host "DRY-RUN -Task clean"
            Write-Host "  BORRARIA: $BuildDir"
            Write-Host "           $n $plural, $mb MB"
            Write-Host "-Check NO ha borrado nada. Sin -Check esto es un Remove-Item -Recurse -Force"
            Write-Host "sin confirmacion: si hay una build ahi dentro que te interese, este es el aviso."
        }
        else {
            Write-Host "DRY-RUN -Task clean: $BuildDir no existe, no habria nada que borrar."
        }
        return 0
    }

    Write-Host "Cleaning Build directory..." -ForegroundColor Yellow

    Stop-AppProcess

    if (Test-Path $BuildDir) {
        $maxRetries = 5
        $retryDelay = 2 # seconds
        for ($i = 1; $i -le $maxRetries; $i++) {
            try {
                Remove-Item -Path $BuildDir -Recurse -Force -ErrorAction Stop
                Write-Host "Cleaned $BuildDir"
                return 0 # Success
            }
            catch {
                if ($i -lt $maxRetries) {
                    Write-Host "Warning: Could not delete '$BuildDir'. Another process might be locking it." -ForegroundColor Yellow
                    Write-Host "Retrying in $retryDelay seconds... (Attempt $i of $maxRetries)" -ForegroundColor Yellow
                    Start-Sleep -Seconds $retryDelay
                }
                else {
                    Write-Host "Error: Failed to delete '$BuildDir' after several retries." -ForegroundColor Red
                    throw "Could not clean build directory. Please ensure no processes (like debuggers or the app itself) are using it."
                }
            }
        }
    }
    else {
        Write-Host "Build directory does not exist. Nothing to clean."
    }
    return 0
}

function Invoke-TaskTest {
    Write-Host "Running Tests..." -ForegroundColor Cyan
    if (!(Test-Path $BuildDir)) {
        Write-Error "Build directory not found. Please run build task first."
    }
    Set-Location $BuildDir
    ctest -C $config --output-on-failure
    Set-Location $ProjectRoot
    return 0
}

function Invoke-TaskSign {
    Write-Host "Signing Plugin (Not implemented in this stub)..." -ForegroundColor Magenta
    return 0
}

# --- Main Flow ---
if ($Help -or [string]::IsNullOrEmpty($task)) {
    Show-Help
    exit 0
}

Show-Header

# Sin esto, un `-Task` desconocido no hacia NADA y salia con 0: un pipeline que
# escribiera `manage.ps1 -Task deploy` creeria que habia hecho un deploy. Y el 2 es
# el codigo que usan los scripts de este repo para "me has llamado mal", distinto del
# 1 de "he intentado hacerlo y ha fallado".
$VALID_TASKS = @("build", "clean", "test", "sign")
$VALID_CONFIGS = @("Release", "Debug")

if ($task -notin $VALID_TASKS) {
    # `[Console]::Error` y no `Write-Error`: con `$ErrorActionPreference = "Stop"`,
    # `Write-Error` LANZA una excepcion, el try/catch de abajo la se traga como si
    # fuera un fallo de ejecucion y sale con 1. Aqui no ha habido un fallo: ha habido
    # una llamada equivocada, y por eso tiene su propio codigo.
    [Console]::Error.WriteLine("Tarea desconocida: '$task'. Validas: $($VALID_TASKS -join ', ').")
    Show-Help
    exit 2
}

if ($config -notin $VALID_CONFIGS) {
    [Console]::Error.WriteLine("Configuracion desconocida: '$config'. Validas: $($VALID_CONFIGS -join ', ').")
    Show-Help
    exit 2
}

try {
    $rc = 0
    switch ($task) {
        "build" { $rc = Invoke-TaskBuild }
        "clean" { $rc = Invoke-TaskClean }
        "test"  { $rc = Invoke-TaskTest }
        "sign"  { $rc = Invoke-TaskSign }
    }
    exit $rc
}
catch {
    Write-Host "Error: $_" -ForegroundColor Red
    exit 1
}