#!/usr/bin/env python3
"""selftest_pre_commit_bats.py: el hook de los .bat, con lo que tiene que avisar.

QUE COMPRUEBA, Y POR QUE SON DOS COSAS

  1. LA LOGICA. Cada caso monta un repo temporal con un .bat y pregunta al hook
     si avisa o se calla. Repos temporales porque probarlo en el de verdad
     significaria dejar un commit de prueba con un .bat roto, que es un rojo
     que nadie ha pedido.

  2. LA INTERFAZ. Un `git commit` de verdad, con core.hooksPath apuntando a una
     copia de los hooks, y comprobando que git PARA el commit con el .bat en LF
     y deja pasar el mismo fichero arreglado.

La segunda existe porque la primera no la ve. MEDIDO el 2026-10-03: con el
hook escrito en Python y `#!/usr/bin/env python3`, los 11 casos de la logica
pasaban y el commit se bloqueaba SIEMPRE, con "Python was not found", en vez de
avisar de los .bat. El hook era correcto y no servia para nada, y solo se
supo haciendo un commit de verdad.

LO QUE ESTA FUERA, Y POR QUE

  · Que el hook este INSTALADO. Es `git config core.hooksPath Scripts/hooks`,
    que es configuracion local de cada clon y no se commitea. Sin eso el hook
    es un fichero correcto que nadie ejecuta. Este script lo comprueba e imprime
    el aviso, porque no hay forma de que un test lo note desde el repo.

USO

    python Scripts/selftest_pre_commit_bats.py

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
VT = b"\x0b"

# Los tres caracteres que los .bat del repo ya traen y que el hook deja pasar.
INVERTIDA = "¿".encode("utf-8")   # U+00BF
RAYA = "—".encode("utf-8")       # U+2014
CAJA = "─".encode("utf-8")        # U+2500

# Los que tienen que avisar. Con el numero de codigo a proposito, porque una
# "o" cirilica escrita a mano en un fuente es indistinguible de una normal, y
# este fichero tiene que poder decir "esto es cirilico" sin mirar los bytes.
TILDE = "á".encode("utf-8")       # U+00E1
CJK = chr(0x5730).encode("utf-8")   # U+5730, por codigo y no a mano, sin el caracter aqui
CIRILICO = "о".encode("utf-8")     # U+043E


def bat_limpio():
    return b"@echo off" + CRLF + b"REM prueba" + CRLF


def pista(nombre="x.bat", cuerpo=None):
    if cuerpo is None:
        cuerpo = bat_limpio()
    return (nombre, cuerpo)


# (nombre, ficheros a poner en el indice, el hook DEBE avisar)
CASOS = [
    ("bat limpio, ASCII y CRLF", [pista()], False),
    (
        "bat con los tres permitidos",
        [pista(cuerpo=b"@echo off" + CRLF + b"REM " + INVERTIDA + CRLF
               + b"REM " + RAYA + CRLF + b"REM " + CAJA + CRLF)],
        False,
    ),
    ("bat con LF sin CR", [pista(cuerpo=bat_limpio().replace(CRLF, b"\n"))], True),
    ("bat con vertical tab", [pista(cuerpo=b"@echo off" + CRLF + b"REM a" + VT + b"b" + CRLF)], True),
    ("bat con tilde", [pista(cuerpo=b"@echo off" + CRLF + b"REM est" + TILDE + CRLF)], True),
    ("bat con CJK", [pista(cuerpo=b"@echo off" + CRLF + b"REM " + CJK + CRLF)], True),
    ("bat con cirilico", [pista(cuerpo=b"@echo off" + CRLF + b"REM pel" + CIRILICO + CRLF)], True),
    ("bat en ANSI, que no decodifica como UTF-8", [pista(cuerpo=b"@echo off" + CRLF + b"REM " + bytes([0xA1, 0xE9]) + CRLF)], True),
    ("dos bats, los dos limpios", [pista(nombre="a.bat"), pista(nombre="b.bat")], False),
    ("sin ningun bat en el indice", [], False),
    ("fichero que no es bat, no se vigila", [("notas.txt", b"uno\ndos\n")], False),
]


def git(d, *args):
    return subprocess.run(["git"] + list(args), cwd=d, capture_output=True, text=True)


def como_se_llama_el_hook():
    """Como lo ejecuta git: con sh en Windows, directo si ya es ejecutable."""
    if os.name == "nt":
        return ["sh", HOOK]
    return [HOOK]


def repo_nuevo(hooks_en=None):
    d = tempfile.mkdtemp(prefix="selftest_bats_")
    git(d, "init", "-q")
    git(d, "config", "user.email", "t@t.t")
    git(d, "config", "user.name", "t")
    if hooks_en:
        git(d, "config", "core.hooksPath", hooks_en)
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
        print("  [%s] %-38s rc=%d (se esperaba %s)"
              % ("PASA" if bien else "FALLA", nombre, r.returncode,
                 "aviso" if debe_avisar else "silencio"))
        if not bien:
            for linea in (r.stderr or "").rstrip().splitlines()[:8]:
                print("        | " + linea)
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
        # El hooksPath va DESPUES de copiar: hasta que no esta, git ni mira en
        # esa carpeta, y el commit pasa sin que el hook opine nada. Ponerlo
        # antes de copiar daria un fallo que parece del hook y es del montaje.
        git(d, "config", "core.hooksPath", hooks)

        # Con el hook ya puesto, un commit de un .bat con LF tiene que parar.
        with open(os.path.join(d, "malo.bat"), "wb") as f:
            f.write(b"@echo off\nREM con LF solo\n")
        git(d, "add", "-A")
        r1 = git(d, "commit", "-m", "no deberia pasar")
        bloquea = r1.returncode != 0
        print("  [%s] git commit con .bat en LF -> rc=%d (se esperaba que parara)"
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
            print("        esta en la historia pese al hook. El log real:")
            for linea in log.rstrip().splitlines():
                print("        | " + linea)
            return False

        # El mismo fichero arreglado tiene que pasar, o el hook seria un muro.
        with open(os.path.join(d, "malo.bat"), "wb") as f:
            f.write(b"@echo off" + CRLF + b"REM ya con CRLF" + CRLF)
        git(d, "add", "-A")
        r2 = git(d, "commit", "-m", "bat arreglado")
        print("  [%s] git commit con .bat en CRLF -> rc=%d (se esperaba que pasara)"
              % ("PASA" if r2.returncode == 0 else "FALLA", r2.returncode))
        if r2.returncode != 0:
            print("        " + (r2.stdout + r2.stderr).strip()[:400])
            return False
        return True
    finally:
        shutil.rmtree(d, ignore_errors=True)


def hook_instalado():
    return git(RAIZ, "config", "--get", "core.hooksPath").stdout.strip()


def main():
    for necesario in (HOOK, os.path.join(HOOKS, "vigilar_bats.py")):
        if not os.path.exists(necesario):
            print("FALLA: no existe " + necesario)
            return 1

    print("Hook:   %s" % HOOK)
    print("Logica: %s" % os.path.join(HOOKS, "vigilar_bats.py"))
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
