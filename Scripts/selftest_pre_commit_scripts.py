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

  · Que PowerShell 5.1 de verdad no parsee un .ps1 con `═` dentro de un
    `Write-Host`. MEDIDO el 2026-10-04 en `init_nexus.ps1` linea 401, que da
    TerminatorExpectedAtEndOfString, y aqui no se vuelve a medir porque es una
    conclusion ya sacada, no una regla del hook.

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

    # --- .ps1: LF, y una lista blanca medida sobre las que parsean ---
    ("ps1 limpio, ASCII y LF", [pista("x.ps1", b"Write-Output 'hola'" + LF)], False),
    ("ps1 con los permitidos",
     [pista("x.ps1", b"# " + ANGULO + PUNTO + ANGULO + LF
            + b"Write-Output '" + RAYA + N_TILDE + b"'" + LF)], False),
    ("ps1 con la caja, que tampoco esta en su lista",
     [pista("x.ps1", b"# " + CAJA + LF)], True),
    ("ps1 con CRLF, que aqui no vale", [pista("x.ps1", b"Write-Output 'hola'" + CRLF)], True),
    ("ps1 con vertical tab",
     [pista("x.ps1", b"Write-Output 'a" + VT + b"b'" + LF)], True),
    ("ps1 con doble caja, que rompe el parser",
     [pista("x.ps1", b'Write-Output "' + DOBLE * 3 + b'"' + LF)], True),
    ("ps1 con triangulo, fuera de la lista",
     [pista("x.ps1", b"# " + TRIANGULO + LF)], True),
    ("ps1 con tilde, que no esta en la lista",
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

    buenos = sum(1 for r in logica if r) + (1 if interfaz else 0)
    total = len(logica) + 1
    print("")
    print("%d de %d" % (buenos, total))
    return 0 if buenos == total else 1


if __name__ == "__main__":
    sys.exit(main())