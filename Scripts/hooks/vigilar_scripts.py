#!/usr/bin/env python3
"""vigilar_scripts.py: lo que un script no puede ir a HEAD sin romperse.

ESTE FICHERO ES LA LOGICA, NO EL HOOK

Quien lo ejecuta es `Scripts/hooks/pre-commit`, un sh que busca un interprete de
Python y le pasa el trabajo. El reparto no es estetico: el lanzador es la parte
que tiene que funcionar en Windows y en Linux, y ya se ha visto que es donde se
rompe. Este fichero solo necesita un Python.

UNA REGLA POR EXTENSION, Y NO UNA

Es lo importante de aqui, y no es una regla que se pueda copiar de un sitio a
otro. Cada interprete lee los bytes de otra manera, asi que lo que es inofensivo
en uno es un fallo en otro. MEDIDO el 2026-10-04 en esta maquina:

  .bat  cmd.exe lee con la pagina de codigos de la consola, y un caracter UTF-8
       multibyte se come el siguiente. MEDIDO el 2026-10-04 que los 102
       caracteres no ASCII que traian los tres .bat (U+00BF x3, U+2500 x94 y
       U+2014 x5) estaban todos en lineas REM, asi que se han podido quitar sin
       tocar una sola orden. MEDIDO que antes de quitarlos, los que estaban
       DENTRO de un Write-Host con comillas rompian el interprete, igual que
       en un .ps1. Por eso la lista de .bat esta ahora VACIA: ASCII puro, sin
       excepciones. Y el EOL tiene que ser CRLF, que es lo que espera cmd.exe.

  .ps1  Windows PowerShell 5.1, que es el instalado aqui. MEDIDO:
       - NO LEE UTF-8. Sin BOM lee con la pagina de codigos ANSI del sistema,
         que en esta maquina es cp1252. MEDIDO: una enye (U+00F1) llega como
         DOS caracteres, con los puntos de codigo 195 y 179 en vez del 243; y un
         U+00AB se ve como DOS caracteres de control, no como unas comillas.
       - Un `Write-Host "═══"` con U+2550 (3 bytes UTF-8) NO PARSEA. El parser
         devuelve TerminatorExpectedAtEndOfString en la linea 401 de
         init_nexus.ps1, porque uno de los 3 bytes leidos como cp1252 se come la
         comilla de cierre. Eso no es un texto feo: es un script que no arranca.
       - Y el fallo es de CODIFICACION, no del CARACTER: puesto como UTF-8 de
         verdad, ese mismo fichero da 0 errores. Un BOM lo tapaba porque cambia
         la decodificacion, pero no arregla nada: deja el resto de los
         caracteres no ASCII en pie.
       Por eso la lista de .ps1 esta ahora VACIA tambien: ASCII puro, y no por
       purismo. Los seis .ps1 del repo son ASCII puro desde el 2026-10-04 y
       parsean sin un error. El EOL tiene que ser LF, que es lo que declara el
       .gitattributes.

  .sh   sh NO decodifica: pasa los bytes de largo. MEDIDO que un .sh en UTF-8
       sin BOM, con tilde en un comentario y con caja en un `echo`, imprime
       `caja: ─ —` sin que se pierda un byte. Por eso aqui la lista blanca esta
       VACIA: todo lo que no sea ASCII se deja pasar, porque medido no rompe.
       Lo que si se vigila es el EOL, que tiene que ser LF como en el
       .gitattributes, y las vertical tabs, que si rompen (abajo).

POR QUE .ps1 NO TIENE LISTA BLANCA, Y .bat TAMPOCO

Antes los .ps1 traian una lista de cinco caracteres que usaban las cinco .ps1
que parseaban: U+00AB, U+00BB, U+00B7, U+00F1 y U+2014. MEDIDO el 2026-10-04
que esa lista era una EXCUSA, no una medicion: los cinco son precisamente los
que PowerShell 5.1 lee mal sin BOM. La enye se parte en dos, y los angulos
Franceses se ven como caracteres de control. Autorizarlos era dejar en pie
justo lo que no se lee bien, y en un Write-Host eso lo ve el usuario.

Se sustituyeron por su equivalente ASCII y la lista se fue. Los .bat ya
habian llegado a lo mismo antes, y por el mismo motivo: su lista tambien era
una excusa.

Y no se ha intentado ser mas fino, ni aqui ni ahi: distinguir un comentario de
una cadena no es fiable de forma automatica, y MEDIDO que equivocarse en ese
caso no es un texto feo, es un script que no arranca. Ante esa duda, ASCII.

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
#   permitidos None = todos pasan (medido que no rompen). Un conjunto = solo
#              esos pasan, mas el ASCII; el conjunto VACIO es ASCII puro.
REGLAS = {
    ".bat": {
        "eol": "crlf",
        # ASCII PURO, sin excepciones. MEDIDO el 2026-10-04: los 102 caracteres
        # no ASCII que traian los tres .bat estaban en lineas REM y se han
        # podido quitar sin tocar una orden. Antes habia una lista con U+00BF,
        # U+2500 y U+2014, y existia porque hooks NO existian todavia; ahora que
        # el hook esta, la lista es una excusa para no quitarlos.
        #
        # Ojo al vacio vs None: un conjunto vacio significa "ningun caracter de
        # mas pasa", y None significa "todos pasan". El de .sh es None a
        # proposito, y confundirlos dejaria a los .bat abiertos de par en par.
        "permitidos": {},
    },
    ".ps1": {
        "eol": "lf",
        # ASCII PURO, sin excepciones, y no por purismo. MEDIDO el 2026-10-04:
        # PowerShell 5.1 lee un .ps1 SIN BOM con la ANSI del sistema, no con
        # UTF-8. Una enye (U+00F1) llega como DOS caracteres, 195 y 179, y en la
        # linea 158 de update_version.ps1, que es un Write-Host que ve el
        # usuario, un U+00AB se veian DOS caracteres de control donde deberia
        # haber unas comillas. Aqui habia una lista con esos cinco caracteres de
        # excepcion, y lo que hacia era dejar en pie justo lo que PowerShell no
        # lee bien. Los cinco se sustituyeron por su equivalente ASCII el
        # 2026-10-04 y los seis .ps1 del repo son ahora ASCII puro.
        "permitidos": {},
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
        "  inocuo en uno rompe en otro, asi que la regla es POR EXTENSION y esta\n"
        "  medida, no inventada:\n"
        "\n"
        "    .bat  cmd.exe. Le pasa CRLF y ASCII PURO, sin excepciones: un\n"
        "          caracter UTF-8 multibyte se come el siguiente cuando la\n"
        "          consola lo lee con su pagina de codigos.\n"
        "    .ps1  Windows PowerShell 5.1. Le pasa LF, y ASCII PURO. MEDIDO que\n"
        "          sin BOM NO lee UTF-8, sino la pagina ANSI del sistema (cp1252\n"
        "          aqui): una enye se lee como DOS letras y un '<' frances se ve\n"
        "          como caracteres de control. Y un '=' doble (U+2550) dentro de\n"
        "          un Write-Host \"...\" hace que el fichero NO PARSEE, con\n"
        "          TerminatorExpectedAtEndOfString. Un BOM lo tapaba sin\n"
        "          arreglar nada: por eso aqui no hay lista blanca.\n"
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