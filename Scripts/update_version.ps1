param (
    [string]$VersionFile = "..\Source\ModelMaker\Version.h"
)

$path = Resolve-Path $VersionFile
if (-not (Test-Path $path)) {
    Write-Error "Version file not found: $VersionFile"
    exit 1
}

$content = Get-Content $path
$newContent = @()
$incremented = $false

foreach ($line in $content) {
    if ($line -match '#define NEURONIK_MODELMAKER_VERSION_SUB (\d+)') {
        $currentSub = [int]$matches[1]
        $newSub = $currentSub + 1
        $line = "#define NEURONIK_MODELMAKER_VERSION_SUB $newSub"
        $incremented = $true
        Write-Host "Incremented version to 0.1.$newSub"
    }
    $newContent += $line
}

if ($incremented) {
    $newContent | Set-Content $path -Encoding UTF8
}
else {
    Write-Warning "Could not find version define to increment."
}
