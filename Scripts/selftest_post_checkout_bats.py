#!/usr/bin/env python3
"""selftest_post_checkout_bats.py: el hook de post-checkout, con lo que avisa.

QUE COMPRUEBA, Y POR QUE SON DOS COSAS

  1. LA LOGICA. Cada caso monta un repo temporal con lo que tiene que dar la
     voz y pregunta al hook. Repos temporales porque probarlo en el de verdad
     significaria dejar un checkout de prueba con .bat rotos.

  2. LA INTERFAZ. Un `git checkout` de verdad, con core.hooksPath apuntando a
     una copia de los hooks, y comprobando que git LLAMA al hook. La segunda
     existe porque la primera no la ve: un hook bien escrito al que git nunca
     llama pasa el banco entero y no vigila nada. MEDIDO el 2026-10-03 con
     pre-commit, que se escribio en Python con `#!/usr/bin/env python3` y en
     git for Windows no arrancaba nunca.

LO QUE ESTA FUERA, Y POR QUE

  · Que el hook este INSTALADO. Es `git config core.hooksPath Scripts/hooks`, que
    es configuracion local de cada clon y no se commitea. Este script lo
    comprueba e imprime el aviso, porque no hay forma de que un test lo note
    desde el repo.

  · Que un hook en 100644 no se ejecute. MEDIDO el 2026-10-04 en git for
    Windows: SI se ejecuta, porque ahi X_OK es "existe" y no "tiene el bit", y
    con `core.fileMode=false` el bit ni se rastrea. El aviso del bit +x es para
    los clones de Linux y macOS y para los runners del CI, y eso no se puede
    medir desde esta maquina sin cambiarla. Lo que SI se mide aqui es que el
    aviso salta cuando el modo del indice no es 100755, que es lo que en la otra
    maquina haria que el hook no corriera.

USO

    python Scripts/selftest_post_checkout_bats.py

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
HOOK = os.path.join(HOOKS, "post-checkout")

CRLF = b"\r\n"


def git(d, *args):
    return subprocess.run(["git"] + list(args), cwd=d, capture_output=True, text=True)


def como_se_llama_el_hook():
    """Como lo ejecuta git: con sh en Windows, directo si ya es ejecutable."""
    if os.name == "nt":
        return ["sh", HOOK]
    return [HOOK]


def repo_nuevo():
    d = tempfile.mkdtemp(prefix="selftest_checkout_")
    git(d, "init", "-q")
    git(d, "config", "user.email", "t@t.t")
    git(d, "config", "user.name", "t")
    with open(os.path.join(d, "semilla.txt"), "w", encoding="utf-8") as f:
        f.write("semilla\n")
    git(d, "add", "-A")
    git(d, "commit", "-qm", "semilla")
    return d


def con_hooks_copiados(d):
    """Copia los hooks dentro del repo y los deja TRACKEADOS.

    Trackeados y no sueltos a proposito: el aviso del bit de ejecutable mira el
    MODO EN EL INDICE, y un fichero que no esta en el indice no tiene modo. Sin
    esto el banco no estaria probando la mitad de lo que dice probar.
    """
    destino = os.path.join(d, "hooks")
    shutil.copytree(HOOKS, destino)
    git(d, "config", "core.hooksPath", "hooks")
    git(d, "add", "-f", "hooks")
    # El bit a TODOS los lanzadores, no solo al que se esta probando. MEDIDO
    # el 2026-10-04 que dejarlo solo en post-checkout hacia que el hook avisara
    # de un 100644 inventado en pre-commit, y tres casos de ocho fallaban por
    # eso y no por lo que decian. En el repo de verdad pre-commit esta en 100755,
    # y el punto de este banco es partir de ahi.
    for nombre in sorted(os.listdir(destino)):
        if not os.path.splitext(nombre)[1]:
            git(d, "update-index", "--chmod=+x", os.path.join("hooks", nombre))
    return destino


def correr(d, *extra):
    return subprocess.run(como_se_llama_el_hook() + list(extra),
                          cwd=d, capture_output=True, text=True)


def caso(nombre, montar, debe_avisar):
    d = repo_nuevo()
    try:
        montar(d)
        r = correr(d, "refs/heads/master", "refs/heads/master", "1")
        aviso = bool((r.stderr or "").strip())
        bien = aviso == debe_avisar
        print("  [%s] %-44s %s" % ("PASA" if bien else "FALLA", nombre,
                                   "aviso" if aviso else "silencio"))
        if not bien:
            for linea in (r.stderr or "").rstrip().splitlines()[:10]:
                print("        | " + linea)
        return bien
    finally:
        shutil.rmtree(d, ignore_errors=True)


def escribir(d, nombre, cuerpo):
    with open(os.path.join(d, nombre), "wb") as f:
        f.write(cuerpo)


def main():
    for necesario in (HOOK, os.path.join(HOOKS, "revisar_checkout.py")):
        if not os.path.exists(necesario):
            print("FALLA: no existe " + necesario)
            return 1

    print("Hook:   %s" % HOOK)
    print("Logica: %s" % os.path.join(HOOKS, "revisar_checkout.py"))
    print("")
    print("La logica, caso por caso:")

    resultados = []

    def caso_simple(nombre, cuerpo, debe_avisar):
        def montar(d):
            if cuerpo is not None:
                escribir(d, "algo.bat", cuerpo)
                git(d, "add", "-A")
            con_hooks_copiados(d)
            git(d, "commit", "-qm", "hooks")
        resultados.append(caso(nombre, montar, debe_avisar))

    caso_simple("nada que revisar", None, False)
    caso_simple("bat con CRLF", b"@echo off" + CRLF + b"REM ok" + CRLF, False)
    caso_simple("bat con LF sin CR", b"@echo off\nREM malo\n", True)
    caso_simple("bat con LF y CRLF mezclados",
                b"@echo off" + CRLF + b"REM a\nREM b\n", True)

    # El bit de ejecutable, que se mira en el indice.
    def montar_644(d):
        escribir(d, "algo.bat", b"@echo off" + CRLF + b"REM ok" + CRLF)
        git(d, "add", "-A")
        con_hooks_copiados(d)
        git(d, "update-index", "--chmod=-x", "hooks/post-checkout")
        git(d, "commit", "-qm", "hooks sin el bit")
    resultados.append(caso("lanzador del hook commiteado como 100644",
                           montar_644, True))

    def montar_755(d):
        escribir(d, "algo.bat", b"@echo off" + CRLF + b"REM ok" + CRLF)
        git(d, "add", "-A")
        con_hooks_copiados(d)
        git(d, "commit", "-qm", "hooks con el bit")
    resultados.append(caso("lanzador del hook commiteado como 100755",
                           montar_755, False))

    def montar_sin_hook_path(d):
        escribir(d, "algo.bat", b"@echo off" + CRLF + b"REM ok" + CRLF)
        git(d, "add", "-A")
        shutil.copytree(HOOKS, os.path.join(d, "hooks"))
        git(d, "config", "--unset", "core.hooksPath")
        git(d, "add", "-f", "hooks")
        git(d, "commit", "-qm", "hooks sin instalar")
    resultados.append(caso("sin core.hooksPath: nada que mirar del bit",
                           montar_sin_hook_path, False))

    print("")
    print("La interfaz, con git de verdad:")
    interfaz = caso_de_interfaz()

    print("")
    instalado = git(RAIZ, "config", "--get", "core.hooksPath").stdout.strip()
    if os.path.normpath(instalado) == os.path.normpath("Scripts/hooks"):
        print("core.hooksPath = %s (instalado)" % instalado)
    else:
        print("ATENCION: core.hooksPath NO apunta a Scripts/hooks, asi que este hook")
        print("           no se ejecuta en ESTE clon. Instalar con:")
        print("               git config core.hooksPath Scripts/hooks")

    buenos = sum(1 for r in resultados if r) + (1 if interfaz else 0)
    total = len(resultados) + 1
    print("")
    print("%d de %d" % (buenos, total))
    return 0 if buenos == total else 1


def caso_de_interfaz():
    """git de verdad llamando al hook de verdad con un checkout de verdad.

    Aqui se cae lo que el banco de la logica no ve: que git llame al hook. Un
    hook al que git no llama se calla siempre y el banco entero lo aprueba.
    """
    d = repo_nuevo()
    try:
        escribir(d, "algo.bat", b"@echo off" + CRLF + b"REM ok" + CRLF)
        git(d, "add", "-A")
        con_hooks_copiados(d)
        git(d, "commit", "-qm", "hooks")

        # Rama con el .bat en LF, para que el checkout tenga algo que avisar.
        git(d, "checkout", "-qb", "rota")
        escribir(d, "algo.bat", b"@echo off\nREM con LF\n")
        git(d, "add", "-A")
        git(d, "commit", "-qm", "rota")
        git(d, "checkout", "-q", "master")

        r = git(d, "checkout", "rota")
        salida = (r.stdout or "") + (r.stderr or "")
        aviso = "post-checkout" in salida
        print("  [%s] git checkout llama al hook y el aviso sale"
              % ("PASA" if aviso else "FALLA"))
        if not aviso:
            print("        git no ha dicho nada. La salida real:")
            for linea in salida.rstrip().splitlines()[:10]:
                print("        | " + linea)
            return False

        # Y el checkout NO puede haber fallado por culpa del hook: post-checkout
        # devuelve siempre 0 a proposito.
        bien = r.returncode == 0 and git(d, "rev-parse", "--abbrev-ref", "HEAD").stdout.strip() == "rota"
        print("  [%s] el checkout se completo y no quedo en la rama rota"
              % ("PASA" if bien else "FALLA"))
        if not bien:
            print("        rc=%d, HEAD=%s" % (
                r.returncode,
                git(d, "rev-parse", "--abbrev-ref", "HEAD").stdout.strip()))
            return False
        return True
    finally:
        shutil.rmtree(d, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())