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
$BuildDir = "$($ProjectRoot.FullName)\build_nexus"
# Try to find JUCE in common locations if not set
$JuceDirCandidate = "C:\JUCE" 
if (Test-Path $JuceDirCandidate) { $JuceDir = $JuceDirCandidate }

function Show-Header {
    Write-Host "=========================================" -ForegroundColor Cyan
    Write-Host "  NEXUS Synthesizer Management Script   " -ForegroundColor Cyan
    Write-Host "=========================================" -ForegroundColor Cyan
    Write-Host "Task: $task | Config: $config"
    Write-Host ""
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
    if (!(Test-Path $headerDir)) { New-Item -ItemType Directory -Path $headerDir -Force | Out-Null }
    $headerFile = Join-Path $headerDir "BuildVersion.h"

    $buildNo = 0
    if (Test-Path $versionFile) {
        $buildNo = [int](Get-Content $versionFile)
    }
    $buildNo++
    $buildNo | Set-Content $versionFile

    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $headerContent = @'
/* Auto-generated build version file */
#pragma once

#define NEXUS_BUILD_VERSION "@BUILD_NO@"
#define NEXUS_BUILD_TIMESTAMP "@TIMESTAMP@"
'@ -replace "@BUILD_NO@", $buildNo -replace "@TIMESTAMP@", $timestamp

    $headerContent | Set-Content $headerFile -Encoding UTF8
    Write-Host "[INFO] Build #$buildNo updated at $timestamp" -ForegroundColor Cyan
}

function Invoke-TaskBuild {
    Write-Host "Starting Build Process..." -ForegroundColor Green
    
    # Force clean build if requested/needed to avoid cache issues with JUCE settings
    Invoke-TaskClean

    # Find CMake
    $CMakePath = Find-CMake
    if (!$CMakePath) { throw "CMake not found. Please install CMake or run from VS Developer Command Prompt." }
    Write-Host "Found CMake: $CMakePath"

    # Initialize Environment
    $vsVars = Find-VSVars
    $GeneratorParams = @("-B", $BuildDir)
    
    # Add JUCE Path if found
    if ($JuceDir) {
        Write-Host "Setting JUCE Path: $JuceDir"
        $GeneratorParams += "-DCMAKE_PREFIX_PATH=`"$JuceDir`""
        $GeneratorParams += "-DJUCE_PATH=`"$JuceDir`""
    }
    
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
    & $CMakePath @GeneratorParams "$($ProjectRoot.FullName)"
    if ($LASTEXITCODE -ne 0) { throw "CMake Configuration Failed" }

    # Build Project
    Write-Host "Building Project ($config)..."
    & $CMakePath --build $BuildDir --config $config --target NEXUS_Standalone
    if ($LASTEXITCODE -ne 0) { throw "Build Failed" }

    # Versioning
    Update-BuildVersion

    Write-Host "Build Completed Successfully!" -ForegroundColor Green
}

function Invoke-TaskClean {
    Write-Host "Cleaning Build directory..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        $maxRetries = 5
        $retryDelay = 2 # seconds
        for ($i = 1; $i -le $maxRetries; $i++) {
            try {
                Remove-Item -Path $BuildDir -Recurse -Force -ErrorAction Stop
                Write-Host "Cleaned $BuildDir"
                return # Success
            }
            catch {
                if ($i -lt $maxRetries) {
                    Write-Host "Warning: Could not delete '$BuildDir'. Another process might be locking it." -ForegroundColor Yellow
                    Write-Host "Retrying in $retryDelay seconds... (Attempt $i of $maxRetries)" -ForegroundColor Yellow
                    Start-Sleep -Seconds $retryDelay
                } else {
                    Write-Host "Error: Failed to delete '$BuildDir' after several retries." -ForegroundColor Red
                    throw "Could not clean build directory. Please ensure no processes (like debuggers or the app itself) are using it."
                }
            }
        }
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
