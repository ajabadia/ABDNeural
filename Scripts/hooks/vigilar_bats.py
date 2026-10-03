#!/usr/bin/env python3
"""vigilar_bats.py: lo que un .bat no puede ir a HEAD sin romper el build.

ESTE FICHERO ES LA LOGICA, NO EL HOOK

Quien lo ejecuta es `Scripts/hooks/pre-commit`, un sh de ocho lineas que busca un
interprete de Python y le pasa el trabajo. El reparto no es estetico: el
lanzador es la parte que tiene que funcionar en Windows y en Linux, y ya se ha
visto que es donde se rompe. Este fichero solo necesita un Python.

QUE VIGILA, Y POR QUE CADA COSA

  1. LF sin CR. Un .bat con LF es un .bat que cmd.exe se come de otra manera, y
     el fallo sale como "el script no hace nada" o "goto no encuentra la
     etiqueta", no como un problema de finales de linea. No hay nada que
     depurar: el sintoma no se parece a la causa.

  2. Vertical tabs (0x0B). No las escribe nadie a proposito: entran por una
     herramienta que reformatea, o por copiar un texto de una pagina que los
     trae como separador de columna. En un .bat, el 0x0B parte el comando donde
     no es un fin de linea, y el error que sale apunta a otra linea.

  3. Caracteres fuera de la lista blanca. MEDIDO el 2026-10-03 sobre los 7 .bat
     del repo: NO son ASCII puro. Hay tres excepciones que ya estan y que
     funcionan, y estan aqui porque quitarlas seria cambiar ficheros que hoy
     van bien:

         U+00BF  (¿)   Scripts/verify_all.bat, Scripts/verify_all_check.bat
         U+2500  (─)   Scripts/verify_all_check.bat, 94 veces, la caja del panel
         U+2014  (—)   build.bat, 5 veces

     Lo que se bloquea es lo de mas alla: tildes, enye, cedilla, cirilico y CJK.
     Todos son UTF-8 multibyte, y cmd.exe lee el .bat con la pagina de codigos de
     la consola, no en UTF-8: un byte de continuacion se come el caracter
     siguiente y el mensaje que se imprime deja de ser el que se escribio.

POR QUE NO ES "ASCII PURO" Y LO SERIA MAS SIMPLE

Porque un hook que falla en HEAD no es un hook, es un obstaculo. Este se
escribio DESPUES de medir que los 7 .bat del repo ya tienen esas tres
caracteres, y la lista blanca son exactamente esos tres. Si algun dia se
quiere ASCII puro de verdad, hay que quitar primero los 101 caracteres de los
tres ficheros, y eso es otro commit con otro motivo.

ALCANCE

Solo mira los .bat que estan EN EL INDICE, que son los que el commit va a
grabar. Mirar el arbol entero haria el hook lento y avisaria de cosas que no
van a salir en este commit. Solo borra: con --diff-filter=ACM se salta los D.

USO

Se instala apuntando git a esta carpeta, que es la unica forma de que el hook
este versionado y no se pierda con un reclone:

    git config core.hooksPath Scripts/hooks

SALIENDO

0 si todo esta bien, 1 con la lista de lo que hay que arreglar. No arregla nada
solo: un hook que reescribe lo que estas a punto de commitear sin avisar es un
hook del que no te fias.
"""

import subprocess
import sys

# Los 7 .bat del repo, MEDIDOS. Ver la cabecera antes de tocarlos.
PERMITIDOS_FUERA_DE_ASCII = {
    0x00BF: "inicio de interrogacion (va en verify_all.bat y verify_all_check.bat)",
    0x2014: "raya (va en build.bat)",
    0x2500: "caja de dibujo del panel (va en verify_all_check.bat)",
}

VT = 0x0B


def ficheros_bat_en_el_indice():
    """Los .bat que este commit va a grabar, ya renombrados si toca."""
    salida = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACM"],
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
    nombres = [n for n in salida.stdout.splitlines() if n.lower().endswith(".bat")]
    return nombres


def revisar(ruta):
    """Devuelve la lista de problemas de un .bat. Vacia si esta bien."""
    try:
        with open(ruta, "rb") as f:
            raw = f.read()
    except OSError as e:
        return ["  no se ha podido leer: %s" % e]

    fallos = []

    # 1. LF sin CR. Se cuenta sobre el total de LF, no sobre las lineas.
    lf = raw.count(b"\n")
    crlf = raw.count(b"\r\n")
    sueltos = lf - crlf
    if sueltos:
        fallos.append(
            "  %d salto(s) de linea con LF solo, sin el CR: en CRLF, como los demas."
            % sueltos
        )

    # 2. Vertical tabs, con su numero de linea para poder buscarlos.
    for linea_no, linea in enumerate(raw.split(b"\n"), 1):
        if bytes([VT]) in linea:
            fallos.append(
                "  linea %d: vertical tab (0x0B). Quitala: parte el comando donde no"
                " es un fin de linea." % linea_no
            )

    # 3. Caracteres raros, tambien con su linea. Se cuenta por byte, que es como
    #    los ve cmd.exe: un caracter UTF-8 de dos bytes aparece dos veces.
    try:
        texto = raw.decode("utf-8")
    except UnicodeDecodeError as e:
        fallos.append(
            "  no decodifica como UTF-8 (offset %d). cmd.exe no lo va a leer bien:"
            " guardalo en UTF-8." % e.start
        )
        texto = raw.decode("utf-8", errors="replace")

    for linea_no, linea in enumerate(texto.split("\n"), 1):
        for caracter in linea:
            punto = ord(caracter)
            if punto < 0x80 or punto in PERMITIDOS_FUERA_DE_ASCII:
                continue
            fallos.append(
                '  linea %d: U+%04X (fuera de ASCII, y no es de los tres permitidos).'
                " Ponlo en ASCII." % (linea_no, punto)
            )
            break  # uno por linea, que si no el aviso es inservible

    return fallos


def main():
    rutas = ficheros_bat_en_el_indice()
    if not rutas:
        return 0

    con_fallos = []
    for ruta in rutas:
        fallos = revisar(ruta)
        if fallos:
            con_fallos.append((ruta, fallos))

    if not con_fallos:
        return 0

    sys.stderr.write("")
    # MEDIDO el 2026-10-03: los NOMBRES de los tres caracteres salen como
    # interrogacion o como una secuencia de escape en la consola de Windows, que
    # no los tiene, y este mensaje es justo el que hay que leer cuando algo no
    # pasa. Por eso van los codigos y no los caracteres.
    sys.stderr.write("pre-commit: %d .bat con lo que cmd.exe no come.\n" % len(con_fallos))
    sys.stderr.write("")
    for ruta, fallos in con_fallos:
        sys.stderr.write("  %s\n" % ruta)
        for fallo in fallos:
            sys.stderr.write("%s\n" % fallo)
        sys.stderr.write("")
    sys.stderr.write(
        "  Que es esto: un .bat con LF, con vertical tabs o con caracteres de mas\n"
        "  alla de los tres permitidos (U+00BF, U+2500 y U+2014) se ejecuta de otra\n"
        "  manera, y el fallo sale como 'el script no hace nada' o 'goto no encuentra la\n"
        "  etiqueta', que no senala ni la linea ni la causa. Lo de la lista blanca esta\n"
        "  medida sobre los .bat del repo; no es ASCII puro a proposito.\n"
    )
    sys.stderr.write("")
    return 1


if __name__ == "__main__":
    sys.exit(main())