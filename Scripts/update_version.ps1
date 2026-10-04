# update_version.ps1 - incrementa NEURONIK_MODELMAKER_VERSION_SUB en Version.h.
#
# USO
#   .\Scripts\update_version.ps1              # incrementa (solo si hay marca de release)
#   .\Scripts\update_version.ps1 -Check       # NO escribe. Dice si haria falta y sale 1 si es asi.
#   .\Scripts\update_version.ps1 -Help
#
# POR QUE EL FLAG NO ES OPCIONAL
#
# El fichero que escribe, `Source/ModelMaker/Version.h`, esta VERSIONADO. Cada vez
# que corre sin `-Check` deja el repo modificado con un numero nuevo dentro, y el
# siguiente `git status` lo ensena como un cambio cualquiera: imposible de distinguir
# de una edicion real, y facil de commitear sin querer. Con `-Check` se puede
# preguntar antes, que es lo unico que evita el `git checkout` a posteriori.
#
# La convencion del flag es `-Check` y no `--check` porque este fichero es
# PowerShell. Se podria poner `--check` y el script no se quejaria: PowerShell acepta
# cualquier cosa delante del guion como nombre de parametro. Eso es precisamente lo
# que no hay que hacer -meter la ortografia de Node en un script de PowerShell solo
# para que un test de Node lo reconozca- porque un `--check` que funciona en un shell
# y no en otro es un contrato que nadie puede leer.
#
# LA RUTA POR DEFECTO, Y POR QUE NO ES RELATIVA AL DIRECTORIO DE TRABAJO
#
# Antes era `..\Source\ModelMaker\Version.h`, que se resuelve contra el CWD. Corriendo
# desde la raiz del repo -que es como se llama a los scripts- resolvia a
# `ABDSynths/Source/ModelMaker/Version.h`, que no existe. Y lo peor no era que no
# encontrase el fichero: `Resolve-Path` escribe un error no terminante, `$path` se
# queda en `$null`, y el script SEGUIDO adelante con el contenido vacio, hasta el
# punto de terminar por el camino de "no he encontrado el define que incrementar" y
# salir 1 con un motivo que no era el motivo.
#
# Con `$PSScriptRoot` la ruta depende del script y no de quien lo llama. Es lo que
# hace `manage.ps1` con `$ProjectRoot`, y es la unica forma de que un `-Check` sea
# fiable desde cualquier sitio.
#
# LO QUE -CHECK PROMETE, Y LO QUE NO PUEDE PROMETER
#
# Este script NO genera un fichero: lo INCREMENTA. Eso cambia lo que un check puede
# decir, y conviene decirlo antes que fabricarlo:
#
#   - NO puede decir "el Version.h esta al dia", porque "al dia" no tiene un valor
#     derivable. El 28 esta al dia si aun no se ha corrido, y desfasado si se ha
#     corrido y el resultado no se ha commiteado. Un check que saliera en rojo
#     siempre (porque siempre falta el +1) seria un rojo perpetuo, que es como un
#     check deja de mirarse. Y uno que saliera verde siempre no miraria nada.
#
#   - SI puede decir las dos cosas que si son un fallo real y no una consecuencia
#     del incremento: que el fichero no este donde debe, y que no contenga la linea
#     que este script tiene que tocar. De ahi son los dos unicos exit 1.
#
# Asi que `-Check` es un DRY-RUN HONESTO: dice el numero que hay y el que se Pondria, y
# avisa de que subir el numero es un acto manual, no algo que un pipeline pueda
# verificar por su cuenta. Quien necesite el veredicto de "se ha bumpingado y
# commiteado", lo tiene en `git status`, que es donde esta la verdad.
#
# Cuando el nombre del parametro sea `-Check` y no `--check`, y por que no da igual:
# PowerShell acepta CUALQUIER cosa delante del guion como nombre de parametro, asi
# que `--check` tambien habria funcionado. Se escribe `-Check` porque es la forma de
# PowerShell de decir `-No writes` y no la de Node: un flag escrito en el idioma que
# no es, para que un test lo reconozca, es un contrato que el siguiente que lo lea no
# puede usar.
param (
    # Si se deja vacio, se calcula mas abajo a partir de `$PSScriptRoot`. NO lleva
    # valor por defecto aqui a proposito: en PowerShell 5.1 `$PSScriptRoot` todavia no
    # esta poblado mientras se evaluan los valores por defecto, y si el script se
    # invoca con ruta RELATIVA -`.\Scripts\update_version.ps1`, que es como se llama a
    # los scripts de este repo- sale vacio y la ruta queda en `\..\Source\...`,
    # que no existe. En el cuerpo del script si esta poblado siempre.
    [string]$VersionFile,

    # NO ESCRIBE NADA. Calcula el resultado y compara con lo que hay en disco.
    [Parameter(Mandatory = $false)]
    [switch]$Check,

    [Parameter(Mandatory = $false)]
    [Alias("h", "?")]
    [switch]$Help
)

# Sin esto, un cmdlet que falla deja el script continuar con variables a medio
# rellenar. Con esto, para. Un script que escribe sobre un `Version.h` commiteado no
# debe ser capaz de seguir adelante sin saber lo que esta leyendo.
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrEmpty($VersionFile)) {
    $VersionFile = Join-Path $PSScriptRoot "..\Source\ModelMaker\Version.h"
}

if ($Help) {
    Write-Host "Uso: .\Scripts\update_version.ps1 [-VersionFile <ruta>] [-Check] [-Help]"
    Write-Host ""
    Write-Host "  -Check   NO escribe. Sale 1 si el Version.h commiteado no cuadra con lo que"
    Write-Host "           se generaria aqui. Sin el, sale 0 sea cual sea el estado."
    exit 0
}

# RELEASE-GATED (2026-09-20): el incremento SO ocurre cuando la build viene marcada
# como release (NEURONIK_MM_RELEASE=1; build.bat modelmaker release lo pone). Cualquier
# otra compilacion del target deja Version.h intacto: antes, cada build (incluida una
# verificacion suelta) quemaba un numero de un fichero VERSIONADO en git.
if ($env:NEURONIK_MM_RELEASE -ne "1") {
    if ($Check) {
        # Se dice en voz alta, y no con un "OK" a secas. Sin la marca de release este
        # script no haria NADA, asi que un verde aqui no significa "el Version.h esta
        # al dia" sino "no hay nada que comprobar". Son dos cosas distintas, y quien lea
        # el log en un pipeline tiene que poder distinguirlas.
        Write-Host "ModelMaker version bump: SIN MARCA DE RELEASE (NEURONIK_MM_RELEASE!=1)."
        Write-Host "Este script no incrementaria nada, asi que -Check NO HA COMPROBADO NADA."
        Write-Host "Para comprobarlo de verdad, corre con NEURONIK_MM_RELEASE=1."
        exit 0
    }
    Write-Host "ModelMaker version bump skipped (no release marker: NEURONIK_MM_RELEASE!=1)"
    exit 0
}

if (-not (Test-Path $VersionFile)) {
    Write-Error "Version file not found: $VersionFile"
    Write-Error "  Se ha buscado desde la raiz del script, no desde el directorio actual."
    Write-Error "  Pasa -VersionFile <ruta> si el fichero vive en otro sitio."
    exit 1
}
$path = Resolve-Path $VersionFile

$content = Get-Content $path
$newContent = @()
$incremented = $false
$cambia = $false
$lineaNueva = ""
$newSub = $null

foreach ($line in $content) {
    if ($line -match '#define NEURONIK_MODELMAKER_VERSION_SUB (\d+)') {
        $currentSub = [int]$matches[1]
        $newSub = $currentSub + 1
        $lineaNueva = "#define NEURONIK_MODELMAKER_VERSION_SUB $newSub"
        $line = $lineaNueva
        $incremented = $true
    }
    $newContent += $line
}

if (-not $incremented) {
    # Ni con -Check ni sin el: si no esta el define, este script no sabe que hacer y
    # no va a inventarse una linea que sobrescriba un fichero que no ha ledo bien.
    Write-Warning "Could not find version define to increment in: $path"
    exit 1
}

if ($Check) {
    # Los dos exit 1 de este script estan aqui arriba: fichero ausente y define
    # ausente. Lo que llega aqui ya se sabe que se puede tocar.
    Write-Host "DRY-RUN - $path"
    Write-Host "  numero en disco     : $currentSub"
    Write-Host "  se pondria           : $newSub"
    Write-Host "  fichero en git       : $(if (git ls-files --error-unmatch $path 2>$null) { 'si' } else { 'NO - versionado? revisalo antes de escribir' })"
    Write-Host ""
    Write-Host "-Check NO ha escrito nada, y NO es un veredicto de "desfasado"."
    Write-Host "Este script incrementa; no genera. Que el numero sea el de ahora o el de despues"
    Write-Host "depende de si el bump anterior llego a commitearse, y eso no lo puede decir un"
    Write-Host "script que lee el numero antes de incrementarlo. Si lo que quieres es saber si"
    Write-Host "el ultimo bump quedo commiteado, mira git status, no esto."
    exit 0
}

Write-Host "Incremented version to 0.1.$newSub"
$newContent | Set-Content $path -Encoding UTF8