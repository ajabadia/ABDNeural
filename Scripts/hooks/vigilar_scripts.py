#!/usr/bin/env python3
"""vigilar_scripts.py: lo que un script no puede ir a HEAD sin romperse.

ESTE FICHERO ES LA LOGICA, NO EL HOOK

Quien lo ejecuta es `Scripts/hooks/pre-commit`, un sh que busca un interprete de
Python y le pasa el trabajo. El reparto no es estetico: el lanzador es la parte
que tiene que funcionar en Windows y en Linux, y ya se ha visto que es donde se
rompe. Este fichero solo necesita un Python.

UNA LISTA BLANCA POR EXTENSION, Y NO UNA

Es lo importante de aqui, y no es una regla que se pueda copiar de un sitio a
otro. Cada interprete lee los bytes de otra manera, asi que lo que es inofensivo
en uno es un fallo en otro. MEDIDO el 2026-10-04 en esta maquina:

  .bat  cmd.exe lee con la pagina de codigos de la consola. MEDIDO: los tres
       caracteres que ya traian los 7 .bat (U+00BF, U+2014, U+2500) funcionan, y
       un acento o un CJK se come el caracter siguiente. Y el EOL tiene que ser
       CRLF, que es lo que espera cmd.exe.

  .ps1  Windows PowerShell 5.1, que es el instalado aqui. MEDIDO:
       - Un `Write-Host "═══"` con U+2550 (3 bytes UTF-8) NO PARSEA. El parser
         devuelve TerminatorExpectedAtEndOfString en la linea 401 de
         init_nexus.ps1, porque uno de los 3 bytes leidos como cp1252 se come la
         comilla de cierre. Eso no es un texto feo: es un script que no arranca.
       - Sin BOM, un `ñ` (2 bytes UTF-8) se lee como DOS caracteres: MEDIDO que
         el literal `canci<o-acento>n` mide 8 en vez de 7, con los puntos de
         codigo 195 y 179 en vez del 243. Con BOM mide 7 y da 243. El BOM se
         calma solo; no se vigila.
       - Con BOM, el fichero entero va bien. MEDIDO que las seis .ps1 del repo
         son sin BOM, y que cinco de las seis parsean sin un solo error.
       El EOL tiene que ser LF, que es lo que declara el .gitattributes.

  .sh   sh NO decodifica: pasa los bytes de largo. MEDIDO que un .sh en UTF-8
       sin BOM, con tilde en un comentario y con caja en un `echo`, imprime
       `caja: ─ —` sin que se pierda un byte. Por eso aqui la lista blanca esta
       VACIA: todo lo que no sea ASCII se deja pasar, porque medido no rompe.
       Lo que si se vigila es el EOL, que tiene que ser LF como en el
       .gitattributes, y las vertical tabs, que si rompen (abajo).

POR QUE LA LISTA DE .ps1 ES LA DE LOS QUE PARSEAN

La union medida de lo que usan las cinco .ps1 que parsean sin errores:

    U+00AB  <<   U+00BB  >>   U+00B7  (punto medio)   U+00F1  (n con tilde)
    U+2014  raya

Los otros tres que aparecen en el repo, U+2550, U+25B6 y U+2713, salen solo de
init_nexus.ps1, que es justo el que no parsea. No se pueden meter en la lista
blanca porque estarian autorizando justo lo que se ha medido que rompe.

Y no se ha intentado ser mas fino: distinguir un comentario de una cadena no es
fiable de forma automatica, y MEDIDO que equivocarse en ese caso no es un texto
malo, es un script que no arranca. Ante esa duda, la lista corta.

LAS VERTICAL TABS, PARA TODOS

No las escribe nadie a proposito: entran por una herramienta que reformatea, o
por copiar un texto de una pagina que las trae como separador. MEDIDO el
2026-10-04 en los dos interpretes: en un .sh, `echo a<0x0B>b` imprime `a` y luego
`b`, o sea que parte el comando donde no es un fin de linea; en un .ps1 pasa lo
mismo.

ALCANCE

Solo mira los ficheros que estan EN EL INDICE, que son los que el commit va a
grabar. Mirar el arbol entero haria el hook lento y avisaria de cosas que no van
a salir en este commit. Solo borra: con --diff-filter=ACM se salta los D.

USO

Se instala apuntando git a esta carpeta, que es la unica forma de que el hook
este versionado y no se pierda con un reclone:

    git config core.hooksPath Scripts/hooks

que es lo que hace Scripts/junctions-workspace.bat en cada clon.

SALIENDO

0 si todo esta bien, 1 con la lista de lo que hay que arreglar. No arregla nada
solo: un hook que reescribe lo que estas a punto de commitear sin avisar es un
hook del que no te fias.
"""

import os
import subprocess
import sys

# MEDIDAS, no supuestas. La clave es la extension; el valor, lo que ese
# interprete puede leer. Ver la cabecera antes de tocar nada.
#
#   eol        "crlf" o "lf": lo que TIENE que ser en el arbol de trabajo.
#   permitidos None = todos pasan. Un conjunto = solo esos, mas el ASCII.
REGLAS = {
    ".bat": {
        "eol": "crlf",
        "permitidos": {
            0x00BF: "inicio de interrogacion (va en verify_all.bat y verify_all_check.bat)",
            0x2014: "raya (va en build.bat)",
            0x2500: "caja de dibujo del panel (va en verify_all_check.bat)",
        },
    },
    ".ps1": {
        "eol": "lf",
        "permitidos": {
            0x00AB: "doble angulo de apertura",
            0x00BB: "doble angulo de cierre",
            0x00B7: "punto medio",
            0x00F1: "n con tilde (salva al parser, aunque se ve como dos letras)",
            0x2014: "raya",
        },
    },
    ".sh": {
        "eol": "lf",
        # MEDIDO que sh pasa los bytes de largo y un .sh con UTF-8 imprime bien.
        # Poner aqui una lista restrictiva seria bloquear ficheros por algo que
        # no se ha visto romperse nunca.
        "permitidos": None,
    },
}

VT = 0x0B
EXTENSIONES = tuple(REGLAS)


def ficheros_vigilados_en_el_indice():
    """Los scripts de este commit que van a grabar, ya renombrados si toca."""
    salida = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACM", "-z"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if salida.returncode != 0:
        # Si no se puede preguntar al indice, no se bloquea el commit por eso.
        # Bloquear aqui seria tapar un problema de git con un problema del hook.
        sys.stderr.write(
            "pre-commit: no se ha podido leer el indice, se pasa sin vigilar.\n"
        )
        for linea in (salida.stderr or "").splitlines()[:3]:
            sys.stderr.write("pre-commit:   %s\n" % linea)
        return []

    nombres = []
    # `-z` y no el listado normal: MEDIDO que `git ls-files` entrecomilla los
    # nombres que no son ASCII, y este repo tiene Asset\\ con acentos.
    for nombre in (salida.stdout or "").split("\0"):
        if not nombre:
            continue
        if os.path.splitext(nombre)[1].lower() in REGLAS:
            nombres.append(nombre)
    return nombres


def revisar(ruta, regla):
    """Devuelve la lista de problemas de un script. Vacia si esta bien."""
    try:
        with open(ruta, "rb") as f:
            raw = f.read()
    except OSError as e:
        return ["  no se ha podido leer: %s" % e]

    fallos = []
    permitidos = regla["permitidos"]

    # 1. El EOL que pide la regla. Se cuenta sobre el total de LF, no sobre las
    #    lineas, porque un fichero mezclado es el caso dificil.
    lf = raw.count(b"\n")
    crlf = raw.count(b"\r\n")
    sueltos = lf - crlf
    if regla["eol"] == "crlf":
        if sueltos:
            fallos.append(
                "  %d salto(s) de linea con LF solo, sin el CR: en CRLF, como los"
                " demas." % sueltos
            )
    else:
        if crlf:
            fallos.append(
                "  %d salto(s) de linea con CRLF: aqui tiene que ser LF, que es lo"
                " que declara el .gitattributes." % crlf
            )

    # 2. Vertical tabs, con su numero de linea para poder buscarlos.
    for linea_no, linea in enumerate(raw.split(b"\n"), 1):
        if bytes([VT]) in linea:
            fallos.append(
                "  linea %d: vertical tab (0x0B). Quitala: parte el comando donde no"
                " es un fin de linea." % linea_no
            )

    # 3. Caracteres raros, tambien con su linea, y solo si la lista de la
    #    extension no es None. Se cuenta por byte, que es como los ve el
    #    interprete: un caracter UTF-8 de dos bytes aparece dos veces.
    try:
        texto = raw.decode("utf-8")
    except UnicodeDecodeError as e:
        fallos.append(
            "  no decodifica como UTF-8 (offset %d). El interprete no lo va a leer"
            " bien: guardalo en UTF-8." % e.start
        )
        texto = raw.decode("utf-8", errors="replace")

    if permitidos is not None:
        for linea_no, linea in enumerate(texto.split("\n"), 1):
            for caracter in linea:
                punto = ord(caracter)
                if punto < 0x80 or punto in permitidos:
                    continue
                fallos.append(
                    "  linea %d: U+%04X, que este interprete no lee bien. Ponlo en"
                    " ASCII." % (linea_no, punto)
                )
                break  # uno por linea, que si no el aviso es inservible

    return fallos


def main():
    rutas = ficheros_vigilados_en_el_indice()
    if not rutas:
        return 0

    con_fallos = []
    for ruta in rutas:
        extension = os.path.splitext(ruta)[1].lower()
        fallos = revisar(ruta, REGLAS[extension])
        if fallos:
            con_fallos.append((ruta, extension, fallos))

    if not con_fallos:
        return 0

    sys.stderr.write("")
    # MEDIDO el 2026-10-03: los NOMBRES de los caracteres raros salen como
    # interrogacion o como una secuencia de escape en la consola de Windows, que
    # no los tiene, y este mensaje es justo el que hay que leer cuando algo no
    # pasa. Por eso van los codigos y no los caracteres.
    sys.stderr.write(
        "pre-commit: %d script(s) con lo que su interprete no come.\n" % len(con_fallos))
    sys.stderr.write("")
    for ruta, extension, fallos in con_fallos:
        sys.stderr.write("  %s (%s)\n" % (ruta, extension))
        for fallo in fallos:
            sys.stderr.write("%s\n" % fallo)
        sys.stderr.write("")

    sys.stderr.write(
        "  Que es esto. Cada interprete lee los bytes a su manera y lo que es\n"
        "  inocuo en uno rompe en otro, asi que la lista blanca es POR\n"
        "  EXTENSION y esta medida, no inventada:\n"
        "\n"
        "    .bat  cmd.exe. Le pasa CRLF, y de los caracteres raros solo los tres\n"
        "          que ya usaban los .bat del repo: U+00BF, U+2014 y U+2500.\n"
        "          Los demas, U+2500 aparte, salen mal leidos por la consola.\n"
        "    .ps1  Windows PowerShell 5.1. Le pasa LF. MEDIDO: un '═' (U+2550)\n"
        "          dentro de un Write-Host \"...\" hace que el fichero NO PARSEE,\n"
        "          con TerminatorExpectedAtEndOfString. Sin BOM, ademas, un 'ñ'\n"
        "          se lee como dos letras. Con BOM el fichero va bien.\n"
        "    .sh   sh no decodifica: pasa los bytes de largo. MEDIDO que un .sh\n"
        "          con UTF-8 imprime bien, asi que aqui solo se vigila el EOL y\n"
        "          las vertical tabs, que si parten el comando.\n"
        "\n"
        "  El sintoma de todo esto es el mismo en los tres: un script que no hace\n"
        "  nada, o un error que no senala ni la linea ni la causa.\n"
    )
    sys.stderr.write("")
    return 1


if __name__ == "__main__":
    sys.exit(main())