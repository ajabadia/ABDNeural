# Resumen del selftest para el final de build.bat (llamado con -File desde
# :finish).
#
#   selftest_summary.ps1 <log> [etiqueta] [modo]
#
# Argumentos:
#   <log>      ruta del transcript que hay que resumir.
#   [etiqueta] que superficie se esta resumiendo; se imprime como cabecera para
#              que, cuando el resumen final trae las dos, se sepa cual es cual.
#              Por defecto "PLUGIN (Standalone)".
#   [modo]     "log" (defecto) o "stdout".
#                - log:    transcript ACUMULATIVO, con marca de tiempo. Lo usan
#                          las DOS superficies: cada corrida anade lineas, abre
#                          con una cabecera "==== CORRIDA ..." y se cierra con
#                          "[selftest] veredicto: ...". Que sea acumulativo es lo
#                          que permite comparar dos pasadas cuando una falla.
#                - stdout: transcript de UNA corrida tal cual sale de consola, sin
#                          marca de tiempo ni linea de cierre, acotado por la
#                          ULTIMA linea "RESULT:". YA NO LO USA NADIE: la bancada
#                          escribia asi antes de que su log pasara a ser
#                          acumulativo (politica en Source\WebUI\SelftestLog.h) y
#                          este modo era el precio de eso. Se conserva porque
#                          sirve para volcar una stdout cruda a mano (por ejemplo
#                  una corrida de la bancada de una maquina donde el log no se
#                          copio), no porque el build lo necesite.
#
# El resumen sale de la ULTIMA corrida cerrada; si esa corrida es vieja (esta
# pasada no la renovo, p.ej. el proceso murio antes de terminar), un aviso lo dice
# en vez de pintar un OK de otra pasada.
#
# Formato de linea de veredicto por direccion:
#   <fecha>  [selftest] DIRECCION: detalle ... -> OK|FAIL
# La DIRECCION es el texto entre "[selftest] " y el primer ":"; hay varias
# lineas por direccion y lineas informativas sin flecha (se ignoran).

$ErrorActionPreference = 'Stop'

$stLog = $args[0]
$stLabel = if ($args.Count -ge 2 -and $args[1]) { $args[1] } else { 'PLUGIN (Standalone)' }
$stStdout = ($args.Count -ge 3 -and $args[2] -eq 'stdout')

# La cabecera va SIEMPRE, incluso en los caminos de salida temprana: cuando el
# resumen final trae las dos superficies, es lo que dice cual es cual.
function Write-Header {
    Write-Output ''
    Write-Output ('  ' + $stLabel + ':')
}

Write-Header

if (-not $stLog -or -not (Test-Path -LiteralPath $stLog)) {
    Write-Output '    (log del selftest no encontrado)'
    exit 0
}

$all = Get-Content -LiteralPath $stLog

$closes = @()
for ($i = 0; $i -lt $all.Count; $i++) {
    if ($all[$i] -match '\[selftest\] veredicto:') { $closes += $i }
}

# En modo stdout la corrida no se cierra con "veredicto:" (una stdout cruda no
# lleva esa linea): se acota por la ULTIMA linea "RESULT:". Sin ella, la corrida
# esta cortada y no se resume: es mejor decirlo que pintar un OK a medias.
if ($stStdout) {
    $closes = @()
    for ($i = 0; $i -lt $all.Count; $i++) {
        if (($all[$i] -split '\[selftest\] ', 2)[1] -match '^RESULT: ') { $closes += $i }
    }
}

if ($closes.Count -eq 0) {
    if ($stStdout) {
        Write-Output '    (el transcript no tiene ninguna corrida cerrada: la superficie no llego a imprimir RESULT)'
    } else {
        Write-Output '    (el log no tiene ninguna corrida cerrada)'
    }
    exit 0
}

$endIdx = $closes[$closes.Count - 1]
$startIdx = if ($closes.Count -ge 2) { $closes[$closes.Count - 2] + 1 } else { 0 }

# La marca de tiempo que escribe el arnes va delante de "[selftest]": con ella
# se ve si la corrida es de esta pasada o del pasado del log. Solo tiene sentido
# en modo log: un volcado de stdout no la lleva.
if (-not $stStdout) {
    $stamp = ($all[$endIdx] -split '\[selftest\]')[0].Trim()
    try {
        $inv = [System.Globalization.CultureInfo]::InvariantCulture
        $runTime = [datetime]::ParseExact($stamp, 'd MMM yyyy h:mm:sstt', $inv)
        $ageMin = [int]((Get-Date) - $runTime).TotalMinutes
        if ($ageMin -ge 120) {
            Write-Output ('  AVISO: la ultima corrida del log es de hace ' + $ageMin + ' min: esta pasada no la renovo.')
        }
    } catch {
        Write-Output '  (no pude verificar la fecha de la ultima corrida)'
    }
}

# Una pasada por la corrida: RESULT cierra el veredicto global, las lineas con
# flecha dan su direccion (agrupadas por primera aparicion, en orden).
$order = @()
$dirs = @{}
$resultLine = $null

for ($i = $startIdx; $i -le $endIdx; $i++) {
    $body = ($all[$i] -split '\[selftest\] ', 2)[1]
    if ($null -eq $body) { continue }

    if ($body -match '^RESULT: (.+)$') { $resultLine = $Matches[1].Trim(); continue }

    $isFail = $body -match ' -> FAIL\b'
    $isOk = $body -match ' -> OK\b'
    if (-not ($isFail -or $isOk)) { continue }

    $label = ($body -split ':', 2)[0].Trim()
    if (-not $dirs.ContainsKey($label)) {
        $dirs[$label] = @()
        $order += $label
    }
    if ($isFail) {
        $detail = ($body -split ' -> FAIL', 2)[1].Trim(' ', ':')

        # Muchas lineas de fallo ACABAN en "-> FAIL": el motivo esta antes, en la
        # cola de la linea. Sin este recorte, el resumen decia "[FAIL] AGUJA" y
        # nada mas, que es justo el caso para el que se separan las superficies.
        if (-not $detail) {
            $head = ($body -replace '\s*->\s*FAIL\s*$', '').Trim()
            $detail = if ($head.Length -gt 110) { '...' + $head.Substring($head.Length - 110) } else { $head }
        }

        $dirs[$label] = @($dirs[$label]) + $detail
    }
}

if ($order.Count -eq 0) {
    Write-Output '    (la ultima corrida no tiene direcciones registradas)'
} else {
    foreach ($name in $order) {
        $fails = $dirs[$name]
        if ($fails.Count -eq 0) {
            Write-Output ('    [OK  ] ' + $name)
        } else {
            Write-Output ('    [FAIL] ' + $name)
            foreach ($d in $fails) { Write-Output ('           ' + $d) }
        }
    }
}

if ($null -ne $resultLine) {
    Write-Output ('    Veredicto: ' + $resultLine)
} else {
    Write-Output '    Veredicto: SIN RESULT (la corrida no cerro)'
}

Write-Output ('    Transcript: ' + $stLog)
exit 0
