#!/usr/bin/env python3
"""selftest_pre_commit_scripts.py: el hook de los scripts, con lo que avisa.

QUE COMPRUEBA, Y POR QUE SON DOS COSAS

  1. LA LOGICA. Cada caso monta un repo temporal con un script y pregunta al
     hook si avisa o se calla. Repos temporales porque probarlo en el de verdad
     significaria dejar un commit de prueba con un script roto, que es un rojo
     que nadie ha pedido.

  2. LA INTERFAZ. Un `git commit` de verdad, con core.hooksPath apuntando a una
     copia de los hooks, y comprobando que git PARA el commit y deja pasar el
     mismo fichero arreglado.

La segunda existe porque la primera no la ve. MEDIDO el 2026-10-03: con el
hook escrito en Python y `#!/usr/bin/env python3`, los casos de la logica
pasaban y el commit se bloqueaba SIEMPRE, con "Python was not found", en vez de
avisar de lo que estaba mal.

LO QUE ESTA FUERA, Y POR QUE

  · Que el hook este INSTALADO. Es `git config core.hooksPath Scripts/hooks`,
    que es configuracion local de cada clon y no se commitea. Este script lo
    comprueba e imprime el aviso, porque no hay forma de que un test lo note
    desde el repo.

  · Que el hook entienda PowerShell. No puede: solo mira bytes. Lo que si
    puede es llamar al parser de verdad, y eso hace `caso_de_parseo`, que
    pregunta al PowerShell de la maquina por los .ps1 versionados.

QUE ROMPIA UN .ps1, Y POR QUE. MEDIDO el 2026-10-04 en `init_nexus.ps1`, que
daba TerminatorExpectedAtEndOfString en la linea 401. NO era un caracter
prohibido en si: `[Parser]::ParseFile` NO lee UTF-8, lee con la ANSI del
sistema, que aqui es cp1252. Sin BOM, los tres bytes de un `═` (U+2550) se
leen como tres caracteres, y uno de ellos es un 0x22: la comilla de cierre del
`Write-Host`. De ahi el error. Leido como UTF-8 de verdad, ese mismo fichero
da 0 errores.

O sea que el fallo es de CODIFICACION, no de contenido: por eso poner un BOM
lo tapaba. El BOM no era la solucion, era el parche, porque ademas dejaba los
283 caracteres fuera de la lista que el pre-commit exige a los .ps1. Sustituidos
por su equivalente ASCII, que quita las dos cosas a la vez. Por eso este caso
parsea con UTF-8 forzado, que es como lo leeria un PowerShell moderno, y no
con el `ParseFile` de la ANSI local, que aqui daria 0 errores con un fichero
roto y no comprobaria nada.

USO

    python Scripts/selftest_pre_commit_scripts.py

Sale 0 si todo pasa, 1 con lo que no.
"""

import os
import shutil
import subprocess
import sys
import tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
HOOKS = os.path.join(RAIZ, "Scripts", "hooks")
HOOK = os.path.join(HOOKS, "pre-commit")

CRLF = b"\r\n"
LF = b"\n"
VT = b"\x0b"

# Los que tienen que avisar. Con el numero de codigo a proposito, porque una
# "o" cirilica escrita a mano en un fuente es indistinguible de una normal, y
# este fichero tiene que poder decir "esto es cirilico" sin mirar los bytes.
TILDE = "á".encode("utf-8")        # U+00E1, NO esta en la lista de .ps1
CJK = chr(0x5730).encode("utf-8")   # U+5730, por codigo y no a mano
CIRILICO = "о".encode("utf-8")     # U+043E

# Los que cada interprete SI aguanta, MEDIDOS. Y los que en .bat ya no aguanta
# nadie: los .bat son ASCII puro desde el 2026-10-04, cuando se quitaron los 102
# caracteres no ASCII que traian. Por eso invertida y caja salen aqui como
# "esto tiene que avisar", y no como "esto tiene que pasar".
INVERTIDA = "¿".encode("utf-8")    # U+00BF, fuera de ASCII en .bat
RAYA = "—".encode("utf-8")         # U+2014, en la lista de .ps1
CAJA = "─".encode("utf-8")          # U+2500, fuera de ASCII en .bat
DOBLE = "═".encode("utf-8")        # U+2550, rompe el parser de .ps1
TRIANGULO = "▶".encode("utf-8")    # U+25B6, fuera de la de .ps1
ANGULO = "«".encode("utf-8")       # U+00AB, en la lista de .ps1
PUNTO = "·".encode("utf-8")        # U+00B7, en la lista de .ps1
N_TILDE = "ñ".encode("utf-8")      # U+00F1, en la lista de .ps1
COHETE = chr(0x1F680).encode("utf-8")
PALOMITA = "✓".encode("utf-8")     # U+2713, fuera de la lista de .ps1


def pista(nombre, cuerpo):
    return (nombre, cuerpo)


def limpio(extra=b""):
    return b"REM prueba" + CRLF + extra


CASOS = [
    # --- .bat: lo que ya habia, sin cambios ---
    ("bat limpio, ASCII y CRLF", [pista("x.bat", limpio())], False),
    ("bat con los que antes estaban permitidos, y ya no",
     [pista("x.bat", limpio(b"REM " + INVERTIDA + CRLF + b"REM " + RAYA + CRLF
                            + b"REM " + CAJA + CRLF))], True),
    ("bat con LF sin CR", [pista("x.bat", limpio().replace(CRLF, LF))], True),
    ("bat con vertical tab", [pista("x.bat", b"@echo off" + CRLF + b"REM a" + VT + b"b" + CRLF)], True),
    ("bat con tilde", [pista("x.bat", b"@echo off" + CRLF + b"REM est" + TILDE + CRLF)], True),
    ("bat con CJK", [pista("x.bat", b"@echo off" + CRLF + b"REM " + CJK + CRLF)], True),
    ("bat con cirilico", [pista("x.bat", b"@echo off" + CRLF + b"REM pel" + CIRILICO + CRLF)], True),
    ("bat en ANSI, que no decodifica como UTF-8",
     [pista("x.bat", b"@echo off" + CRLF + b"REM " + bytes([0xA1, 0xE9]) + CRLF)], True),

    # --- .ps1: LF y ASCII PURO, y ya sin lista blanca. MEDIDO el 2026-10-04:
    # PowerShell 5.1 sin BOM lee con la ANSI del sistema, con lo que una
    # enye llega partida en dos y un angulo frances se ve como un caracter
    # de control. La lista que los excusaba dejaba en pie justo lo que no
    # se lee bien, asi que los cinco se sustituyeron por ASCII y la lista se
    # va.
    ("ps1 limpio, ASCII y LF", [pista("x.ps1", b"Write-Output 'hola'" + LF)], False),
    ("ps1 con los que antes estaban permitidos, y ya no",
     [pista("x.ps1", b"# " + ANGULO + PUNTO + ANGULO + LF
            + b"Write-Output '" + RAYA + N_TILDE + b"'" + LF)], True),
    ("ps1 con la caja, que tampoco estaba permitida",
     [pista("x.ps1", b"# " + CAJA + LF)], True),
    ("ps1 con CRLF, que aqui no vale", [pista("x.ps1", b"Write-Output 'hola'" + CRLF)], True),
    ("ps1 con vertical tab",
     [pista("x.ps1", b"Write-Output 'a" + VT + b"b'" + LF)], True),
    ("ps1 con doble caja, que rompe el parser",
     [pista("x.ps1", b'Write-Output "' + DOBLE * 3 + b'"' + LF)], True),
    ("ps1 con triangulo, que no es ASCII",
     [pista("x.ps1", b"# " + TRIANGULO + LF)], True),
    ("ps1 con palomita, que no es ASCII",
     [pista("x.ps1", b"# " + PALOMITA + LF)], True),
    ("ps1 con la doble caja dentro de un Write-Host, como estaba init_nexus.ps1",
     [pista("x.ps1", b'Write-Host "' + DOBLE * 67 + b'"' + LF)], True),
    ("ps1 con tilde, que no es ASCII",
     [pista("x.ps1", b"Write-Output 'est" + TILDE + b"'" + LF)], True),
    ("ps1 con CJK", [pista("x.ps1", b"Write-Output '" + CJK + b"'" + LF)], True),

    # --- .sh: LF y vertical tabs. El UTF-8 se deja pasar, y eso es lo medido ---
    ("sh limpio, ASCII y LF", [pista("x.sh", b"echo hola" + LF)], False),
    ("sh con CRLF, que aqui no vale", [pista("x.sh", b"echo hola" + CRLF)], True),
    ("sh con vertical tab", [pista("x.sh", b"echo a" + VT + b"b" + LF)], True),
    ("sh con UTF-8: tilde, caja y cohete, y se deja pasar",
     [pista("x.sh", b"#!/bin/sh" + LF + b"# acenti: canci" + N_TILDE + LF
            + b"echo '" + CAJA + COHETE + b"'" + LF)], False),

    # --- los que no son de ningun tipo ---
    ("sin ningun script en el indice", [], False),
    ("fichero que no se vigila", [("notas.txt", b"uno\ndos\n")], False),
    ("los tres a la vez, los tres limpios",
     [pista("a.bat", limpio()), pista("a.ps1", b"Write-Output 'h'" + LF),
      pista("a.sh", b"echo h" + LF)], False),
]


def git(d, *args):
    return subprocess.run(["git"] + list(args), cwd=d, capture_output=True, text=True)


def como_se_llama_el_hook():
    """Como lo ejecuta git: con sh en Windows, directo si ya es ejecutable."""
    if os.name == "nt":
        return ["sh", HOOK]
    return [HOOK]


def repo_nuevo():
    d = tempfile.mkdtemp(prefix="selftest_scripts_")
    git(d, "init", "-q")
    git(d, "config", "user.email", "t@t.t")
    git(d, "config", "user.name", "t")
    with open(os.path.join(d, "semilla.txt"), "w", encoding="utf-8") as f:
        f.write("semilla\n")
    git(d, "add", "-A")
    git(d, "commit", "-qm", "semilla")
    return d


def poner(d, ficheros):
    for nombre, cuerpo in ficheros:
        ruta = os.path.join(d, nombre)
        carpeta = os.path.dirname(ruta)
        if carpeta:
            os.makedirs(carpeta, exist_ok=True)
        with open(ruta, "wb") as f:
            f.write(cuerpo)
    if ficheros:
        git(d, "add", "-A")


def caso_de_logica(nombre, ficheros, debe_avisar):
    d = repo_nuevo()
    try:
        poner(d, ficheros)
        r = subprocess.run(como_se_llama_el_hook(), cwd=d, capture_output=True, text=True)
        aviso = r.returncode != 0
        bien = aviso == debe_avisar
        print("  [%s] %-48s %s" % ("PASA" if bien else "FALLA", nombre,
                                   "aviso" if aviso else "silencio"))
        if not bien:
            for linea in (r.stderr or "").rstrip().splitlines()[:8]:
                print("        | " + ascii(linea))
        return bien
    finally:
        shutil.rmtree(d, ignore_errors=True)


def prueba_negativa_del_parseo():
    """El caso de parseo tiene que FALLAR cuando hay un .ps1 roto.

    Sin esto, un caso que dice "0 errores" no se distingue de uno que no mira.
    MEDIDO el 2026-10-04: la primera version de este caso era exactamente eso,
    un falso verde. Se rompe un .ps1 de verdad en un clon temporal, se espera
    rc=1, y se deja constancia de que se ha restaurado.

    Rompe la SINTAXIS, no la codificacion a proposito: un U+2550 dentro de un
    Write-Host solo rompe cuando PowerShell lo lee con la ANSI del sistema
    (cp1252 aqui). Leido como UTF-8, que es lo que hace este caso, da 0
    errores. Por eso la prueba tiene que ser de sintaxis, que falla en las dos
    lecturas, si no estaria probando otra cosa.
    """
    d = repo_nuevo()
    try:
        # Un repo nuevo no tiene .ps1: se copia uno real del de verdad.
        origen = os.path.join(RAIZ, "init_nexus.ps1")
        if not os.path.exists(origen):
            print("  [N/A  ] no hay init_nexus.ps1 donde copiar")
            return True
        shutil.copyfile(origen, os.path.join(d, "roto.ps1"))
        # Romperlo de verdad: una llave sin cerrar. Se rompe la SINTAXIS y no la
        # codificacion, porque un U+2550 dentro de un Write-Host solo rompe
        # cuando PowerShell lo lee con la ANSI del sistema; leido en UTF-8, que
        # es lo que hace el caso, da 0 errores. Con la sintaxis rota falla en
        # las dos lecturas, que es lo que hace falta para que la prueba valga.
        with open(os.path.join(d, "roto.ps1"), "a", encoding="ascii", newline="\n") as f:
            f.write("if ($true) {\n")
        git(d, "add", "-A")

        bien = caso_de_parseo_en(d)
        print("  [%s] el caso de parseo ve un .ps1 con sintaxis rota -> %s"
              % ("PASA" if bien else "FALLA",
                 "falla, como debe" if not bien else "PASA, y no deberia"))

        # Y en el mismo repo, ya sin el roto, tiene que volver a estar bien. Se
        # deja OTRO .ps1 sano: si se quita el unico, el caso dice con razon "no
        # hay ninguno que comprobar", que es cierto pero no prueba nada.
        shutil.copyfile(origen, os.path.join(d, "sano.ps1"))
        git(d, "rm", "-q", "--cached", "roto.ps1")
        os.remove(os.path.join(d, "roto.ps1"))
        git(d, "add", "-A")
        git(d, "commit", "-qm", "fuera el roto")
        bien2 = caso_de_parseo_en(d)
        print("  [%s] y vuelve a estar bien cuando ya no hay ninguno roto"
              % ("PASA" if bien2 else "FALLA"))
        return (not bien) and bien2
    finally:
        shutil.rmtree(d, ignore_errors=True)


def caso_de_parseo():
    """El caso de parseo de verdad, sobre ESTE repo."""
    return caso_de_parseo_en(RAIZ)


def caso_de_parseo_en(raiz):
    """El parser de verdad del PowerShell de la maquina, sobre los .ps1 que
    git tiene versionados. El hook mira bytes; este caso pregunta a quien sabe.

    MEDIDO el 2026-10-04: antes de arreglar init_nexus.ps1, esto era
    `init_nexus.ps1 errores=1`. Ahora tiene que ser 0 en todos.

    `raiz` es un parametro porque este mismo caso se vuelve a llamar sobre un
    repo de mentira, con un .ps1 roto a proposito, para comprobar que aqui
    falla (ver `prueba_negativa_del_parseo`).

    Si no hay powershell (Linux, o un clon sin PowerShell), no se puede
    comprobar y no se cuenta como fallo: se dice y se sale bien.
    """
    ps = shutil.which("powershell") or shutil.which("pwsh")
    if not ps:
        print("  [N/A  ] no hay powershell en esta maquina: parseo sin comprobar")
        return True

    version = subprocess.run([ps, "-NoProfile", "-Command", "$PSVersionTable.PSVersion.ToString()"],
                             capture_output=True, text=True).stdout.strip()
    print("  PowerShell %s" % version)

    # Los .ps1 VERSIONADOS, y no lo que traigan clonado por debajo. Se saca la
    # lista del indice, pero se parsea el fichero DE DISCO, no el blob del
    # indice. MEDIDO el 2026-10-04: parseando el blob, un init_nexus.ps1 roto
    # en el arbol de trabajo daba "0 errores" y el caso se ponia en verde,
    # porque `git ls-files` no ve lo que todavia no esta en el indice. Un
    # editor que rompe el fichero y todavia no ha hecho `git add` es
    # justamente el caso que este tiene que coger.
    r = git(raiz, "ls-files", "-z", "--", "*.ps1")
    ficheros = [f for f in r.stdout.split("\0") if f]
    if not ficheros:
        print("  [FALLA] no hay ningun .ps1 versionado: el caso no comprueba nada")
        return False

    sin_parsear = [f for f in ficheros if not os.path.isfile(os.path.join(raiz, f))]
    if sin_parsear:
        print("  [FALLA] versionados pero no en disco: %s" % ", ".join(sin_parsear))
        return False

    # ParseInput por fichero, en un solo powershell para no pagar 6 arranques.
    #
    # Y NO ParseFile, que lee con la ANSI del sistema. MEDIDO el 2026-10-04 en
    # es-ES (cp1252): ParseFile decia "0 errores" de un init_nexus.ps1 roto,
    # porque leia los tres bytes de un U+25B6 como tres caracteres y uno era la
    # comilla de cierre, pero el parser ya habia cerrado la cadena antes. O sea:
    # con ParseFile este caso es un falso verde garantizado en Windows. Se leen
    # los BYTES y se decodifica en UTF-8 a proposito, que es como lo leeria un
    # PowerShell moderno, y con esa lectura el mismo fichero roto SI da error.
    #
    # El script va en un fichero y se llama con -File, NO con -Command: MEDIDO
    # el 2026-10-04, `-Command "bucle" ruta1 ruta2` concatena las rutas al
    # comando y las EJECUTA, en vez de dejarlas en $args. Con -Command el caso
    # daba "0 .ps1 versionados" y se ponia en verde sin comprobar nada, que es
    # justo el fallo que un caso de este tipo no puede permitirse.
    ps_script = (
        "foreach ($f in $args) {\n"
        "  $bytes = [System.IO.File]::ReadAllBytes($f)\n"
        "  $texto = [System.Text.Encoding]::UTF8.GetString($bytes)\n"
        "  if ($texto.Length -gt 0 -and [int][char]$texto[0] -eq 0xFEFF) "
        "{ $texto = $texto.Substring(1) }\n"
        "  $t = $null; $e = $null\n"
        "  $null = [System.Management.Automation.Language.Parser]::ParseInput($texto, [ref]$t, [ref]$e)\n"
        '  Write-Output ("RES|" + $e.Count + "|" + $f)\n'
        '  foreach ($x in $e) { Write-Output ("ERR -> " + $x.Message) }\n'
        "}\n"
    )
    d = tempfile.mkdtemp(prefix="selftest_parseo_")
    try:
        guion = os.path.join(d, "parsear.ps1")
        with open(guion, "w", encoding="ascii", newline="\n") as f:
            f.write(ps_script)
        rutas = [os.path.join(raiz, f) for f in ficheros]
        r2 = subprocess.run(
            [ps, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", guion] + rutas,
            capture_output=True, text=True)
    finally:
        shutil.rmtree(d, ignore_errors=True)

    bien = True
    vistos = 0
    for linea in r2.stdout.splitlines():
        if linea.startswith("ERR ->"):
            print("     " + ascii(linea.strip()))
            continue
        if not linea.startswith("RES|"):
            continue
        _, cuenta, nombre = linea.split("|", 2)
        vistos += 1
        if cuenta.strip() != "0":
            bien = False
            print("  [FALLA] %s no parsea: %s error(es)" % (nombre, cuenta.strip()))
    # El recuento es el que evita el falso verde: si powershell no ha hablado
    # de cada fichero, el caso NO pasa, se llame como se llame.
    if vistos != len(ficheros):
        print("  [FALLA] se esperaban %d .ps1 y powershell solo hablo de %d: "
              "el caso no ha comprobado nada" % (len(ficheros), vistos))
        return False
    if bien:
        print("  [PASA] los %d .ps1 versionados parsean con 0 errores" % vistos)
    return bien


def caso_de_interfaz():
    """git de verdad llamando al hook de verdad. Aqui es donde se cae lo que
    el banco de la logica no ve."""
    d = repo_nuevo()
    hooks = os.path.join(d, "hooks")
    try:
        shutil.copytree(HOOKS, hooks)
        for nombre in sorted(os.listdir(hooks)):
            if not os.path.splitext(nombre)[1]:
                git(d, "update-index", "--chmod=+x", os.path.join("hooks", nombre))
        # El hooksPath va DESPUES de copiar: hasta que no esta, git ni mira en
        # esa carpeta, y el commit pasa sin que el hook opine nada. Ponerlo
        # antes de copiar daria un fallo que parece del hook y es del montaje.
        git(d, "config", "core.hooksPath", hooks)

        # Con el hook ya puesto, un commit de un script malo tiene que parar.
        with open(os.path.join(d, "malo.ps1"), "wb") as f:
            f.write(b'Write-Output "' + DOBLE * 3 + b'"\n')
        git(d, "add", "-A")
        r1 = git(d, "commit", "-m", "no deberia pasar")
        bloquea = r1.returncode != 0
        print("  [%s] git commit con .ps1 roto -> rc=%d (se esperaba que parara)"
              % ("PASA" if bloquea else "FALLA", r1.returncode))
        if not bloquea:
            print("        el commit ha pasado: el hook no esta de camino")
            return False

        # Y el commit bloqueado no tiene que haber dejado nada en la historia.
        log = git(d, "log", "--oneline", "--all").stdout
        en_historia = "no deberia pasar" in log
        print("  [%s] el commit parado no esta en la historia"
              % ("PASA" if not en_historia else "FALLA"))
        if en_historia:
            return False

        # El mismo fichero arreglado tiene que pasar, o el hook seria un muro.
        with open(os.path.join(d, "malo.ps1"), "wb") as f:
            f.write(b"Write-Output 'hola'\n")
        git(d, "add", "-A")
        r2 = git(d, "commit", "-m", "ps1 arreglado")
        print("  [%s] git commit con .ps1 en ASCII y LF -> rc=%d (se esperaba que pasara)"
              % ("PASA" if r2.returncode == 0 else "FALLA", r2.returncode))
        return r2.returncode == 0
    finally:
        shutil.rmtree(d, ignore_errors=True)


def hook_instalado():
    return git(RAIZ, "config", "--get", "core.hooksPath").stdout.strip()


def main():
    for necesario in (HOOK, os.path.join(HOOKS, "vigilar_scripts.py")):
        if not os.path.exists(necesario):
            print("FALLA: no existe " + necesario)
            return 1

    print("Hook:   %s" % HOOK)
    print("Logica: %s" % os.path.join(HOOKS, "vigilar_scripts.py"))
    print("")
    print("La logica, caso por caso:")
    logica = [caso_de_logica(*c) for c in CASOS]

    print("")
    print("El parser de PowerShell de verdad, sobre lo versionado:")
    parseo = caso_de_parseo()

    print("")
    print("La prueba negativa: el caso de arriba tiene que FALLAR si hay un roto:")
    negativo = prueba_negativa_del_parseo()

    print("")
    print("La interfaz, con git de verdad:")
    interfaz = caso_de_interfaz()

    print("")
    instalado = hook_instalado()
    if os.path.normpath(instalado) == os.path.normpath("Scripts/hooks"):
        print("core.hooksPath = %s (instalado)" % instalado)
    else:
        print("ATENCION: core.hooksPath NO apunta a Scripts/hooks, asi que este hook")
        print("           no se ejecuta en ESTE clon. Instalar con:")
        print("               git config core.hooksPath Scripts/hooks")
        print("           Sin eso el hook es correcto y nadie lo llama.")

    buenos = (sum(1 for r in logica if r) + (1 if parseo else 0)
              + (1 if negativo else 0) + (1 if interfaz else 0))
    total = len(logica) + 3
    print("")
    print("%d de %d" % (buenos, total))
    return 0 if buenos == total else 1


if __name__ == "__main__":
    sys.exit(main())