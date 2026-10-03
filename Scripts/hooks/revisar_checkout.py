#!/usr/bin/env python3
"""revisar_checkout.py: como ha quedado el arbol despues de un checkout.

ESTE FICHERO ES LA LOGICA, NO EL HOOK

Quien lo ejecuta es `Scripts/hooks/post-checkout`, un sh que busca un interprete
de Python, exactamente igual que hace el de pre-commit. Mismo reparto y mismo
motivo: el lanzador tiene que arrancar en Windows y en Linux, y es la parte que
se rompe.

QUE COMPRUEBA, Y SOLO DOS COSAS

  1. Los .bat con LF sin CR en el arbol de trabajo.

  2. Los lanzadores del hook (los ficheros SIN extension de la carpeta de
     hooks) sin el bit de ejecutable en el indice.

POR QUE SOLO ESAS DOS, Y NO TODAS LAS DE pre-commit

Porque pre-commit ya garantiza que lo que hay en HEAD esta limpio: si un .bat
llego al indice con LF, con vertical tabs o con un caracter raro, el commit se
paró. Un checkout no puede traer de HEAD nada que pre-commit no haya dejado
pasar. Lo UNICO que un checkout puede estropear por el camino son los finales
de linea del arbol de trabajo, que dependen de como este configurada ESTA
maquina, y los permisos, que tambien.

Asi que repetir aqui la lista de caracteres permitidos seria trabajo que no
puede dar ningun aviso nuevo. si un .bat con un caracter raro llega a HEAD, es
porque pre-commit no estaba puesto en el clon que lo commiteo, y eso ya es otro
problema, con su propio aviso.

SOBRE EL BIT DE EJECUTABLE, QUE NO ES LO QUE PARECE

MEDIDO el 2026-10-04 en git for Windows: un hook versionado como 100644 SE
EJECUTA IGUAL. El mensaje sale, el hook corre, y no hay ni aviso ni problema:

    $ git checkout rama
    Already on 'rama'
    SE EJECUTO EL HOOK

Porque git.exe delega en el emulador de rutas POSIX, y ahi X_OK es "existe", no
"tiene el bit". Con `core.fileMode=false`, que es el default en Windows, el bit
ni se rastrea.

El aviso es, por tanto, para el otro lado: un clon de Linux o macOS, y los
runners de ubuntu que usa el CI, donde el bit SI se comprueba. Ahi un hook
commiteado como 100644 llega sin el bit y git se lo salta SIN DECIR NADA: no
hay error, no hay mensaje, simplemente el hook no corre. Y entonces un .bat con
LF pasa, que es justo lo que este hook existe para que no pase.

Por eso lo que se mira es el MODO EN EL INDICE (lo que viaja con el commit) y no
el bit del disco: el del disco en Windows no existe, y el del indice es el que
decide en la maquina que importa.

Si el `core.hooksPath` de este clon apunta FUERA del repositorio, no hay modo
en el indice que mirar: no hay nada commiteado. En ese caso se mira el disco,
con `os.access(X_OK)`, que en Windows siempre da bueno -que es justo lo medido
arriba- y en Linux y macOS da la respuesta buena.

SALIENDO

Siempre 0. post-checkout no puede parar un checkout -git ya ha cambiado de rama
cuando este script se ejecuta- y un codigo distinto de 0 se traduce en un
aviso de git que no va a ningun sitio. Este avisa; quien para es pre-commit.

USO

    git config core.hooksPath Scripts/hooks

que es lo que instala Scripts/junctions-workspace.bat en cada clon.
"""

import os
import subprocess
import sys

# El bit que git quiere en un hook, tal y como lo escribe `git ls-files -s`.
MODO_EJECUTABLE = "100755"


def git(*args):
    return subprocess.run(
        ["git"] + list(args),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def ruta_de_hooks():
    """Donde git busca los hooks en ESTE clon, o "" si no hay config puesta.

    Sin `core.hooksPath` los hooks estan en `.git/hooks`, que no se versiona, y
    entonces no hay nada commiteado que pueda haber perdido el bit. Se devuelve
    la cadena vacia y no se avisa de nada, que es lo correcto.
    """
    salida = git("config", "--get", "core.hooksPath")
    if salida.returncode != 0:
        return ""
    return (salida.stdout or "").strip()


def lanzadores_sin_bit_de_ejecutable(destino):
    """Los ficheros SIN extension de la carpeta de hooks y su bit, en el indice.

    "Sin extension" es la definicion de "lo que git ejecuta": un hook con
    `.py` no es un hook, es un fichero. Los de aqui se llaman pre-commit y
    post-checkout, y no llevan punto.
    """
    problemas = []
    salida = git("ls-files", "-s", "--", destino)
    if salida.returncode != 0:
        return problemas

    for linea in (salida.stdout or "").splitlines():
        # Formato: "100755 <sha> 0<TAB>ruta". El modo es el primer campo.
        partes = linea.split("\t", 1)
        if len(partes) != 2:
            continue
        campos = partes[0].split()
        if not campos:
            continue
        modo, ruta = campos[0], partes[1]
        if os.path.splitext(ruta)[1]:
            continue  # tiene extension: no lo ejecuta git como hook
        if modo == MODO_EJECUTABLE:
            continue
        problemas.append(
            ("  %s esta commiteado como %s, y git necesita %s para ejecutarlo."
             % (ruta, modo, MODO_EJECUTABLE),
             "  git update-index --chmod=+x %s" % ruta)
        )
    return problemas


def lanzadores_sin_bit_en_el_disco(destino):
    """La misma comprobacion cuando la carpeta de hooks no esta en el repo."""
    problemas = []
    try:
        nombres = sorted(os.listdir(destino))
    except OSError:
        return problemas
    for nombre in nombres:
        ruta = os.path.join(destino, nombre)
        if os.path.splitext(nombre)[1] or not os.path.isfile(ruta):
            continue
        if os.access(ruta, os.X_OK):
            continue
        problemas.append(
            ("  %s no tiene el bit de ejecutable y git se lo va a saltar."
             % ruta,
             "  chmod +x %s" % ruta)
        )
    return problemas


def bats_con_lf():
    """Los .bat del arbol de trabajo que han vuelto con LF sin CR."""
    salida = git("ls-files", "-z", "--", "*.bat")
    if salida.returncode != 0:
        sys.stderr.write(
            "post-checkout: no se han podido listar los .bat; se pasa sin revisar.\n"
        )
        return []

    # `-z` y no el listado normal: MEDIDO que `git ls-files` entrecomilla los
    # nombres que no son ASCII, y este repo tieneAsset\\ con acentos.
    rutas = [r for r in (salida.stdout or "").split("\0") if r.lower().endswith(".bat")]

    problemas = []
    for ruta in rutas:
        try:
            with open(ruta, "rb") as f:
                raw = f.read()
        except OSError as e:
            problemas.append((ruta, ["  no se ha podido leer: %s" % e]))
            continue
        lf = raw.count(b"\n")
        crlf = raw.count(b"\r\n")
        sueltos = lf - crlf
        if sueltos:
            problemas.append(
                (ruta,
                 ["  %d salto(s) de linea con LF solo, sin el CR." % sueltos,
                  "  En Windows esto lo pone la regla `*.bat text eol=crlf` del"
                  " .gitattributes. Si sigue asi, el checkout se ha hecho con"
                  " una config que pisa los atributos."]
            ))
    return problemas


def main():
    avisos = []

    for ruta, lineas in bats_con_lf():
        avisos.append(("Un .bat ha vuelto del checkout con LF", ruta, lineas))

    destino = ruta_de_hooks()
    if destino:
        # Fuera del repositorio no hay indice que consultar, y `git ls-files`
        # con una ruta absoluta devuelve error. Se distingue por el mismo criterio
        # que usa git: una ruta relativa sin `..` es del repo.
        dentro = not os.path.isabs(destino) and not destino.replace("\\", "/").startswith("../")
        if dentro:
            problemas = lanzadores_sin_bit_de_ejecutable(destino)
        else:
            problemas = lanzadores_sin_bit_en_el_disco(destino)
        if problemas:
            avisos.append((
                "Un lanzador del hook se ha quedado sin bit de ejecutable",
                "",
                [linea for par in problemas for linea in par]))

    if not avisos:
        return 0

    sys.stderr.write("\n")
    sys.stderr.write(
        "post-checkout: %d cosa(s) que un checkout ha dejado tocadas.\n" % len(avisos))
    sys.stderr.write(
        "  Esto NO ha parado nada: git ya ha cambiado de rama. Se avisa porque\n"
        "  el rojo sale mucho mas tarde, en el build, y sin senalar ni la linea\n"
        "  ni la causa.\n")
    for titulo, ruta, lineas in avisos:
        sys.stderr.write("\n  %s\n" % titulo)
        if ruta:
            sys.stderr.write("  %s\n" % ruta)
        for linea in lineas:
            sys.stderr.write("%s\n" % linea)
    sys.stderr.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())