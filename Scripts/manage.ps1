<#
.SYNOPSIS
    Master Management Script for NEXUS Synthesizer.
    Consolidates build, clean, test, and package tasks.

.EXAMPLE
    .\Scripts\manage.ps1 -task build -config Release
    .\Scripts\manage.ps1 -task clean
    .\Scripts\manage.ps1 -task test
#>

param (
    [Parameter(Mandatory = $true)]
    [ValidateSet("build", "clean", "test", "sign")]
    [string]$task,

    [Parameter(Mandatory = $false)]
    [ValidateSet("Release", "Debug")]
    [string]$config = "Release"
)

$ErrorActionPreference = "Stop"

# --- Configuration ---
$ProjectRoot = Get-Item $PSScriptRoot\..
$BuildDir = "$($ProjectRoot.FullName)\build"
# Try to find JUCE in common locations if not set
$JuceDirCandidate = "C:\JUCE" 
if (Test-Path $JuceDirCandidate) { $JuceDir = $JuceDirCandidate }

function Show-Header {
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  NEXUS Synthesizer Management Script   " -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "Task: $task | Config: $config"
    Write-Host ""
}

function Find-CMake {
    $PotentialPaths = @(
        "cmake",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
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
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"
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
    
    # Find CMake
    $CMakePath = Find-CMake
    if (!$CMakePath) { throw "CMake not found. Please install CMake or run from VS Developer Command Prompt." }
    Write-Host "Found CMake: $CMakePath"

    # Initialize Environment
    $vsVars = Find-VSVars
    $GeneratorParams = @("-B", $BuildDir)
    
    # Simple logic to detect generator based on available VS
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
    & $CMakePath @GeneratorParams
    if ($LASTEXITCODE -ne 0) { throw "CMake Configuration Failed" }

    Write-Host "Building Project ($config)..."
    & $CMakePath --build $BuildDir --config $config
    if ($LASTEXITCODE -ne 0) { throw "Build Failed" }

    # Copy Standalone executable to root
    # Adjust path based on JUCE defaults: build/NEXUS_artefacts/Release/Standalone/NEXUS.exe
    $StandalonePath = "$BuildDir\NEXUS_artefacts\$config\Standalone\NEXUS.exe"
    if (Test-Path $StandalonePath) {
        Copy-Item $StandalonePath "$($ProjectRoot.FullName)\NEXUS.exe" -Force
        Write-Host "Standalone executable copied to root: NEXUS.exe" -ForegroundColor Green
    }
    else {
        Write-Warning "Standalone executable not found at expected path: $StandalonePath"
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

function Invoke-TaskSign {
    Write-Host "Signing Plugin (Not implemented in this stub)..." -ForegroundColor Magenta
}

# --- Main Flow ---
Show-Header

try {
    switch ($task) {
        "build" { Invoke-TaskBuild }
        "clean" { Invoke-TaskClean }
        "test" { Invoke-TaskTest }
        "sign" { Invoke-TaskSign }
    }
}
catch {
    Write-Host "Error: $_" -ForegroundColor Red
    exit 1
}
