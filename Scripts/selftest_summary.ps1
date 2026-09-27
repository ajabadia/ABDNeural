# Resumen del selftest para el final de build.bat (llamado con -File desde
# :finish). Argumento: ruta del transcript acumulativo del selftest.
#
# El log es acumulativo: cada corrida del plugin Standalone anhade lineas y se
# cierra con una linea "[selftest] veredicto: ...". La bancada NO escribe aqui
# (su salida es solo stdout), asi que cada corrida cerrada es del plugin y esta
# completa. El resumen sale de la ULTIMA corrida cerrada; si esa corrida es
# vieja (esta pasada no la renovo, p.ej. el proceso murio antes de terminar),
# un aviso lo dice en vez de pintar un OK de otra pasada.
#
# Formato de linea de veredicto por direccion:
#   <fecha>  [selftest] DIRECCION: detalle ... -> OK|FAIL
# La DIRECCION es el texto entre "[selftest] " y el primer ":"; hay varias
# lineas por direccion y lineas informativas sin flecha (se ignoran).

$ErrorActionPreference = 'Stop'
$stLog = $args[0]

if (-not $stLog -or -not (Test-Path -LiteralPath $stLog)) {
    Write-Output '  (log del selftest no encontrado)'
    exit 0
}

$all = Get-Content -LiteralPath $stLog

$closes = @()
for ($i = 0; $i -lt $all.Count; $i++) {
    if ($all[$i] -match '\[selftest\] veredicto:') { $closes += $i }
}

if ($closes.Count -eq 0) {
    Write-Output '  (el log no tiene ninguna corrida cerrada)'
    exit 0
}

$endIdx = $closes[$closes.Count - 1]
$startIdx = if ($closes.Count -ge 2) { $closes[$closes.Count - 2] + 1 } else { 0 }

# La marca de tiempo que escribe el arnes va delante de "[selftest]": con ella
# se ve si la corrida es de esta pasada o del pasado del log.
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
        $dirs[$label] = @($dirs[$label]) + $detail
    }
}

if ($order.Count -eq 0) {
    Write-Output '  (la ultima corrida no tiene direcciones registradas)'
} else {
    foreach ($name in $order) {
        $fails = $dirs[$name]
        if ($fails.Count -eq 0) {
            Write-Output ('  [OK  ] ' + $name)
        } else {
            Write-Output ('  [FAIL] ' + $name)
            foreach ($d in $fails) { Write-Output ('         ' + $d) }
        }
    }
}

if ($null -ne $resultLine) {
    Write-Output ('  Veredicto global: ' + $resultLine)
} else {
    Write-Output '  Veredicto global: SIN RESULT (la corrida no cerro)'
}

Write-Output ('  Log del selftest: ' + $stLog)
exit 0
