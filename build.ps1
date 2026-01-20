param (
    [string]$BuildType = "Release"
)

$BuildDir = "build"
$LogDir = "LOGS/build"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$Timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"
$LogFile = "$LogDir/${Timestamp}_build.log"

if ($BuildType -eq "clean") {
    Write-Host "Cleaning build..."
    Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
    exit
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Set-Location $BuildDir

$CMakeConfig = if ($BuildType -eq "debug") { "Debug" } else { "Release" }

Write-Host "Configuring CMake ($CMakeConfig)..."
cmake .. -DCMAKE_BUILD_TYPE=$CMakeConfig | Tee-Object -FilePath "..\$LogFile" -Append
if ($LASTEXITCODE -ne 0) { Write-Error "CMake configuration failed"; exit 1 }

Write-Host "Building..."
cmake --build . --config $CMakeConfig | Tee-Object -FilePath "..\$LogFile" -Append
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

Write-Host "Build complete."
Set-Location ..
