<#
.SYNOPSIS
    Master Management Script for CZ-5000 Emulator.
    Consolidates build, clean, test, and package tasks.

.EXAMPLE
    .\scripts\manage.ps1 -task build -config Release
    .\scripts\manage.ps1 -task clean
    .\scripts\manage.ps1 -task test
#>

param (
    [Parameter(Mandatory = $true)]
    [ValidateSet("build", "clean", "test", "gm", "sign")]
    [string]$task,

    [Parameter(Mandatory = $false)]
    [ValidateSet("Release", "Debug")]
    [string]$config = "Release"
)

$ErrorActionPreference = "Stop"

# --- Configuration ---
$ProjectRoot = Get-Item $PSScriptRoot\..
$BuildDir = "$($ProjectRoot.FullName)\build"
$JuceDir = "C:\JUCE"

function Show-Header {
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  CZ-5000 Emulator Management Script    " -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "Task: $task | Config: $config"
    Write-Host ""
}

function Find-CMake {
    $PotentialPaths = @(
        "cmake",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\CMake\bin\cmake.exe"
    )

    foreach ($path in $PotentialPaths) {
        if ($path -eq "cmake") {
            if (Get-Command cmake -ErrorAction SilentlyContinue) { return "cmake" }
        }
        elseif (Test-Path $path) {
            return $path
        }
    }
    return $null
}

function Find-VSVars {
    $PotentialPaths = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\18\Insiders\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    )

    foreach ($path in $PotentialPaths) {
        if (Test-Path $path) {
            return $path
        }
    }
    return $null
}

function Invoke-TaskBuild {
    Write-Host "Starting Build Process..." -ForegroundColor Green
    
    # Check JUCE
    if (!(Test-Path "$JuceDir\CMakeLists.txt")) {
        Write-Error "JUCE not found at $JuceDir"
    }

    # Find CMake
    $CMakePath = Find-CMake
    if (!$CMakePath) { throw "CMake not found. Please install CMake or run from VS Developer Command Prompt." }
    Write-Host "Found CMake: $CMakePath"

    # Initialize Environment
    $vsVars = Find-VSVars
    $GeneratorParams = @("-B", $BuildDir)
    
    # If not VS 18, use explicit generator for VS 2022
    if (!$vsVars -or $vsVars -notmatch "18") {
        Write-Host "Using Visual Studio 17 2022 Generator..."
        $GeneratorParams += "-G", "Visual Studio 17 2022", "-A", "x64"
    }
    else {
        Write-Host "Found VS 18+, letting CMake auto-detect generator..."
    }

    if (!(Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir
    }

    Write-Host "Configuring CMake..."
    & $CMakePath @GeneratorParams
    if ($LASTEXITCODE -ne 0) { throw "CMake Configuration Failed" }

    Write-Host "Building Project ($config)..."
    & $CMakePath --build $BuildDir --config $config
    if ($LASTEXITCODE -ne 0) { throw "Build Failed" }

    # Copy Standalone executable to root
    $StandalonePath = "$BuildDir\CZ101Emulator_artefacts\$config\Standalone\CZ-101 Emulator.exe"
    if (Test-Path $StandalonePath) {
        Copy-Item $StandalonePath "$($ProjectRoot.FullName)\CZ101Emulator.exe" -Force
        Write-Host "Standalone executable copied to root: CZ101Emulator.exe" -ForegroundColor Green
    }

    Write-Host "Build Completed Successfully!" -ForegroundColor Green
}

function Invoke-TaskClean {
    Write-Host "Cleaning Build directory..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Path $BuildDir -Recurse -Force
        Write-Host "Cleaned $BuildDir"
    }
    else {
        Write-Host "Build directory does not exist. Nothing to clean."
    }
}

function Invoke-TaskTest {
    Write-Host "Running Tests..." -ForegroundColor Cyan
    if (!(Test-Path $BuildDir)) {
        Write-Error "Build directory not found. Please run build task first."
    }
    Set-Location $BuildDir
    ctest -C $config --output-on-failure
    Set-Location $ProjectRoot
}

function Invoke-TaskGM {
    Write-Host "Running Golden Master Validation..." -ForegroundColor Magenta
    $GMExec = "$BuildDir\CZ101GoldenMaster_artefacts\$config\CZ101GoldenMaster.exe"
    if (!(Test-Path $GMExec)) {
        Write-Error "GoldenMaster executable not found. Please build first."
    }
    & $GMExec
}

function Invoke-TaskSign {
    Write-Host "Signing Plugin (Not implemented in this stub)..." -ForegroundColor Magenta
    # Reuse sign_plugin.ps1 logic here if needed
}

# --- Main Flow ---
Show-Header

try {
    switch ($task) {
        "build" { Invoke-TaskBuild }
        "clean" { Invoke-TaskClean }
        "test" { Invoke-TaskTest }
        "gm" { Invoke-TaskGM }
        "sign" { Invoke-TaskSign }
    }
}
catch {
    Write-Host "Error: $_" -ForegroundColor Red
    exit 1
}
