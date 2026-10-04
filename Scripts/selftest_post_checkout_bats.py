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
    los clones de Linux y macOS y para los runners del CI, y por eso los dos
    ultimos casos de este script solo corren en un sistema con bits de permiso:
    miden el hook de ESTE repo, que es la mitad que el banco de repos
    temporales no puede ver.

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

# El bit que git quiere en un hook, tal y como lo escribe `git ls-files -s`.
# El MISMO numero, y no una copia cogida de otro sitio: revisar_checkout.py lo
# llama MODO_EJECUTABLE y define el contrato de lo que git ejecuta.
MODO_EJECUTABLE = "100755"

# Sin extension es lo que git ejecuta: un hook con `.py` no es un hook, es un
# fichero. Los dos de aqui son `pre-commit` y `post-checkout`.
LANZADORES = ("pre-commit", "post-checkout")


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


def caso_hook_del_repo_real():
    """El hook de ESTE repo: su modo en el indice, y el bit en el disco.

    POR QUE HACE FALTA CUANDO EL BANCO DE ARRIBA YA MIDE EL 100755 Y EL 100644

    Porque `con_hooks_copiados` hace `git update-index --chmod=+x` sobre todos
    los lanzadores antes de cada caso. O sea que el banco PARTE SIEMPRE con el
    bit puesto y acaba poniendolo el mismo: no puede notar si al lanzador del
    repo de verdad se le hubiera perdido, porque nunca mira el de verdad.

    Y no es una comprobacion que sobre: el aviso del bit +x esta escrito para
    el otro lado, y lo dice revisar_checkout.py: MEDIDO que en git for Windows un
    hook en 100644 SE EJECUTA IGUAL, porque ahi X_OK es "existe" y no "tiene el
    bit", con `core.fileMode=false` que es su defecto. En un clon de Linux o
    macOS, y en los runners que usa el CI, el bit SI se comprueba, y ahi un hook
    commiteado como 100644 llega sin el y git se lo salta SIN DECIR NADA: no hay
    error ni mensaje, el hook simplemente no corre, y un .bat con LF se cuela
    hasta el build.

    SON DOS MITADES DISTINTAS, Y POR ESO SON DOS CASOS:

      · el INDICE, que es lo que viaja con el commit. No depende del sistema, asi
        que este caso corre en todos lados, Windows incluido.
      · el DISCO, que es lo que hace que git lo ejecute. Solo existe en un
        sistema con bits de permiso, asi que en Windows no se mide: `os.access`
        daria bueno siempre y el caso pasaria sin mirar nada. Por eso se salta y
        no se cuenta, en vez de contarse como un verde.
    """
    print("")
    print("El hook de este repo, no uno de mentira:")

    salida = git(RAIZ, "ls-files", "-s", "--", "Scripts/hooks")
    modos = {}
    if salida.returncode == 0:
        for linea in (salida.stdout or "").splitlines():
            # Formato: "100755 <sha> 0<TAB>ruta". El modo es el primer campo.
            partes = linea.split("\t", 1)
            if len(partes) != 2:
                continue
            campos = partes[0].split()
            if campos:
                modos[os.path.basename(partes[1])] = campos[0]

    fallos = 0
    for nombre in LANZADORES:
        modo = modos.get(nombre)
        bien = modo == MODO_EJECUTABLE
        if not bien:
            fallos += 1
        print("  [%s] %-44s %s" % ("PASA" if bien else "FALLA",
                                   "%s commiteado como %s" % (nombre, modo),
                                   MODO_EJECUTABLE if bien else
                                   "NO; git se lo va a saltar"))
        if not bien:
            print("        git update-index --chmod=+x Scripts/hooks/%s" % nombre)
    return fallos == 0, fallos


def caso_bit_en_el_disco():
    """El bit del indice ha llegado al disco. Solo en un sistema que lo tiene.

    MEDIDO el 2026-10-04 en git for Windows: `os.access(ruta, os.X_OK)` da
    bueno para cualquier fichero que exista, porque ahi el permiso de ejecucion
    no se consulta a ningun sitio. Por eso este caso no se ejecuta en Windows: no
    seria una comprobacion, seria un verde fijo que ademas taparia el caso de al
    lado. En su lugar se SALTADO, y no cuenta para el total, que es lo que
    distingue "no hay nada que mirar aqui" de "lo he mirado y esta bien".

    Es la mitad que el resto del banco no cubre: los repos temporales se montan
    con `copytree` + `chmod` explicitos, y aqui lo que se mira es lo que ha
    hecho un checkout de verdad con lo que hay commiteado.
    """
    print("")
    if os.name == "nt":
        print("  [SALTADO] el bit de ejecutable en el disco: en Windows X_OK es")
        print("            'existe' y no 'tiene el bit'. Se mide en Linux y macOS.")
        return None

    print("  El bit del indice, llegado al disco de este repo:")
    fallos = 0
    for nombre in LANZADORES:
        ruta = os.path.join(HOOKS, nombre)
        ejecutable = os.path.exists(ruta) and os.access(ruta, os.X_OK)
        if not ejecutable:
            fallos += 1
        print("  [%s] %-44s %s" % ("PASA" if ejecutable else "FALLA",
                                   "%s ejecutable en el disco" % nombre,
                                   "os.access(X_OK)" if ejecutable else "NO lo es"))
        if not ejecutable:
            print("        chmod +x Scripts/hooks/%s" % nombre)
            print("        y commitea el modo: git update-index --chmod=+x Scripts/hooks/%s" % nombre)
    return True, fallos


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

    # El hook del repo de verdad, que los casos de arriba no pueden ver porque
    # todos parten de repos temporales con el bit puesto a mano.
    indice_ok, _ = caso_hook_del_repo_real()
    resultados.append(indice_ok)

    disco = caso_bit_en_el_disco()
    if disco is not None:
        disco_ok, _ = disco
        resultados.append(disco_ok)

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