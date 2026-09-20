param (
    [string]$VersionFile = "..\Source\ModelMaker\Version.h"
)

# RELEASE-GATED (2026-09-20): el incremento SO ocurre cuando la build viene marcada
# como release (NEURONIK_MM_RELEASE=1; build.bat modelmaker release lo pone). Cualquier
# otra compilacion del target deja Version.h intacto: antes, cada build (incluida una
# verificacion suelta) quemaba un numero de un fichero VERSIONADO en git.
if ($env:NEURONIK_MM_RELEASE -ne "1") {
    Write-Host "ModelMaker version bump skipped (no release marker: NEURONIK_MM_RELEASE!=1)"
    exit 0
}

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
