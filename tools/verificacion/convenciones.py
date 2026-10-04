#!/usr/bin/env python3
"""convenciones.py: como esta escrito cada fichero del repo, y donde no lo esta.

QUE MIDE, Y POR QUE CADA COSA

  CR / LF    Los finales de linea, y cuantos hay de cada clase. Un fichero con
             los dos mezclados es el sintoma de que algo se ha editado con una
             herramienta que normaliza y otra que no, y el `git diff` ensucia
             cada linea.

  VT         Vertical tabs, 0x0B. No las escribe nadie a proposito, y en un
             `.bat` parten un comando donde no es un fin de linea.

  vallas     Las lineas de valla de codigo de un `.md`, contadas de dos en dos.
             Un numero impar es un documento que no se cierra, que es como se
             rompe el resaltado de GitHub.

  CJK        Cualquier ideograma. En este repo no hay ninguno y no debe haber
             ninguno: son los que se cuelan al copiar texto de una pagina.

  no ASCII   Cuantos bytes van por encima de 127, y cuales. En un fichero ASCII
             es cero, y en un `.bat` y un `.ps1` tambien: los dos son ASCII
             PURO, sin lista blanca. En un `.md` con tildes no.

LA LISTA DE CONVENCIONES, Y DONDE ESTA

La fuente de estas reglas es el `.gitattributes` y las cabeceras de los
ficheros. En la practica son tres:

  · los `.bat` van en CRLF y en ASCII puro, sin excepciones;
  · los `.ps1` van en LF y en ASCII puro, sin excepciones, y no porque sea
    purismo: PowerShell 5.1 sin BOM lee con la ANSI del sistema, y MEDIDO que
    una enye llega partida en dos;
  · los `.sh` y `.py` van en LF;
  · los `.md` con tildes van en LF y con vallas parejas.

Este script COMPRUEBA las mecánicas, que son objetivos y no dependen de que
alguien diga nada. La convencion de tildes NO la comprueba: esa necesita saber
cual es el grupo de cada fichero, y de eso se ocupa `detectar_tildes.py`.

LO QUE NO HACE

No arregla nada. Solo dice. Y sale 1 si encuentra algo, para que se pueda usar
como puerta en un script, no solo para mirar.

USO

    python tools/verificacion/convenciones.py --trackeados   # lo versionado
    python tools/verificacion/convenciones.py                 # el arbol entero
    python tools/verificacion/convenciones.py CMakeLists.txt  # lo que le digas
"""

import io
import os
import re
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

BINARIOS = {".png", ".jpg", ".jpeg", ".gif", ".wasm", ".exe", ".dll", ".lib",
            ".pdb", ".icns", ".ico", ".jar", ".bin", ".o", ".obj", ".a",
            ".dylib", ".so", ".ilk"}
# Los .log son salida de los scripts de build, no fuente: se escriben mezclando
# CRLF y LF y tragen texto de otras paginas. MEDIDO el 2026-10-03: sin esta
# linea, revisar el repo entero salia con rc=1 por `watch-tree.log` y cuatro
# mas, que no son ficheros que nadie pueda arreglar.
LOGS = {".log"}
FUERA = {".git", "node_modules", "build-reference", "build-wasm", "dist",
         ".freebuff", ".agents",
         # `temp_docx_extract` es el undo de un .docx: no es fuente del proyecto.
         # MEDIDO el 2026-10-04 que ya NO esta versionado (se salio del indice
         # con `git rm --cached` y esta en el .gitignore), pero sigue en el disco
         # de quien lo tenga, y revisar el ARBOL entero se lo encontraria. Se
         # queda la exclusion por eso, no porque este versionado: si vuelve a
         # aparecer en el arbol, el sitio para quejarse es el .gitignore.
         "temp_docx_extract"}
# Las extensiones que SI se revisan enteras, y en las que no decodificar como
# UTF-8 es un problema y no la prueba de que el fichero sea binario.
TEXTO = {".bat", ".cmd", ".sh", ".py", ".md", ".js", ".mjs", ".cjs", ".ts",
         ".json", ".yml", ".yaml", ".txt", ".cpp", ".h", ".hpp", ".cmake",
         ".gitignore", ".gitattributes", ".css", ".html", ".xml", ".ps1"}

# Los .bat y los .ps1 NO tienen lista blanca: ASCII PURO, sin excepciones.
#
# MEDIDO el 2026-10-04 que los 102 caracteres no ASCII que traian los tres .bat
# estaban todos en lineas REM y se han podido quitar sin tocar una sola orden.
# Antes habia una lista con U+00BF, U+2500 y U+2014, y existia porque el hook de
# pre-commit todavia no estaba: sin hook que lo impidiera, la lista era la
# unica barrera, y era una barrera blanda. MEDIDO ademas que un caracter de
# esos DENTRO de un `Write-Host "..."` no sale mal impreso, rompe el
# interprete, igual que en un .ps1.
#
# Los .ps1 llegaron a lo mismo por su cuenta. PowerShell 5.1 SIN BOM no lee
# UTF-8: lee con la ANSI del sistema, que aqui es cp1252. MEDIDO que una enye
# llega como DOS caracteres (195 y 179 en vez de 243) y que en la linea 158 de
# update_version.ps1, un Write-Host que ve el usuario, un U+00AB se veia como
# dos caracteres de control. Los cinco que estaban en la lista
# (U+00AB, U+00BB, U+00B7, U+00F1, U+2014) son justo los que no se leen bien,
# asi que eran una excusa y no una medicion. Se sustituyeron por ASCII.
ASCII_PURO = {".bat", ".ps1"}

CJK = re.compile("[\u3000-\u9fff\uff00-\uffef]")


def revisa(ruta):
    """Lista de problemas. Vacia si el fichero cumple lo que se le pide."""
    try:
        raw = io.open(ruta, "rb").read()
    except (IOError, OSError):
        return ["no se ha podido leer"]
    ext = os.path.splitext(ruta)[1].lower()
    problemas = []

    # El filtro de carpetas vale tambien cuando le pasan ficheros a mano, que es
    # lo que hace el banco. Si solo se aplicara al recorrer el arbol, basta con
    # pasarle `temp_docx_extract\...` para saltarselo, y el banco lo hace.
    if any(parte in FUERA for parte in ruta.replace("\\", "/").split("/")):
        return []

    # Un fichero que no es de TEXTO y no decodifica como UTF-8 es binario: un
    # .docx, un .pdf. No es un problema de finales de linea y no se revisa. La
    # lista de binarios de arriba se queda corta en cuanto aparece una
    # extension nueva, y MEDIDO el 2026-10-03 asi pasaba: `DOC.docx` salia
    # con 86 vertical tabs que no son de nadie.
    if ext not in TEXTO:
        try:
            raw.decode("utf-8")
        except UnicodeDecodeError:
            return []

    crlf = raw.count(b"\r\n")
    lf = raw.count(b"\n")
    sueltos = lf - crlf
    vt = raw.count(bytes([11]))

    # 1. Finales de linea mezclados o contrarios a lo que pide la extension.
    if crlf and sueltos:
        problemas.append("finales de linea mezclados: %d CRLF y %d LF solo"
                         % (crlf, sueltos))
    elif ext == ".bat" and sueltos:
        problemas.append("%d LF sin CR en un .bat, que va en CRLF" % sueltos)
    elif ext in (".sh", ".py") and crlf:
        problemas.append("%d CRLF en un %s, que va en LF" % (crlf, ext))

    if vt:
        problemas.append("%d vertical tabs (0x0B)" % vt)

    # 2. Texto: CJK, y en los .bat lo que se salga de la lista.
    try:
        texto = raw.decode("utf-8")
    except UnicodeDecodeError as e:
        problemas.append("no decodifica como UTF-8, offset %d" % e.start)
        texto = raw.decode("utf-8", errors="replace")

    cjk = sorted(set(CJK.findall(texto)))
    if cjk:
        problemas.append("CJK: %s" % ascii("".join(cjk[:8])))

    if ext in ASCII_PURO:
        # ASCII puro, sin lista y sin excepciones. En .bat porque cmd.exe lee
        # con la pagina de codigos de la consola y un caracter UTF-8 multibyte
        # se come el que viene detras; en .ps1 porque PowerShell 5.1 sin BOM
        # hace lo mismo con la ANSI del sistema. Ver ASCII_PURO, arriba.
        raros = sorted(set(c for c in texto if ord(c) > 127))
        if raros:
            problemas.append("fuera de ASCII: %s (los %s son ASCII puro)"
                             % (ascii("".join(raros[:8])), ext))

    # 3. Vallas de un .md.
    if ext == ".md":
        vallas = len(re.findall(r"^\s*(?:```|~~~)", texto, re.M))
        if vallas % 2:
            problemas.append("%d vallas de codigo, IMPAR: el documento no cierra"
                             % vallas)
    return problemas


def ficheros(raiz):
    for carpeta, dirs, nombres in os.walk(raiz):
        dirs[:] = [d for d in dirs if d not in FUERA]
        for nombre in nombres:
            ruta = os.path.join(carpeta, nombre)
            ext = os.path.splitext(nombre)[1].lower()
            if ext in BINARIOS or ext in LOGS:
                continue
            yield ruta


def trackeados():
    """Los ficheros que git tiene versionados, preguntandolo a git.

    MEDIDO el 2026-10-03: esto se hacia al reves, pasando la lista desde un
    script de shell con `$TRACKEADOS`. El shell parte esa cadena por espacios, y
    los nombres del repo tienen espacios: 433 rutas llegaron como 473 trozos, y
    el verificador decia "no se ha podido leer" de ficheros que estaban ahi,
    con el `.gitattributes` delante. Preguntando a git desde aqui no hay shell de
    por medio y cada ruta llega entera.

    `core.quotePath=false` porque con el valor por defecto git devuelve los
    nombres con bytes de mas de 127 entrecomillados y en escapes octales, y
    `encoding="utf-8"` porque sin el la salida se decodifica con cp1252 y la "o"
    con tilde se convierte en dos caracteres que no existen.
    """
    salida = subprocess.run(
        ["git", "-c", "core.quotePath=false", "ls-files"],
        cwd=RAIZ, capture_output=True, text=True, encoding="utf-8")
    if salida.returncode != 0:
        return []
    return [r for r in salida.stdout.splitlines() if r]


def main():
    if "--trackeados" in sys.argv[1:]:
        rutas = trackeados()
    else:
        rutas = [a for a in sys.argv[1:] if not a.startswith("--")] or list(ficheros(RAIZ))
    malos = []
    for ruta in rutas:
        problemas = revisa(ruta)
        if problemas:
            malos.append((ruta, problemas))

    print("Revisados %d ficheros." % len(rutas))
    print("")
    if not malos:
        print("Ninguno incumple.")
        return 0

    for ruta, problemas in malos:
        try:
            nombre = os.path.relpath(ruta, RAIZ)
        except ValueError:
            nombre = ruta
        print("%s" % nombre)
        for problema in problemas:
            print("    %s" % problema)
        print("")
    print("%d de %d ficheros con algo que arreglar." % (len(malos), len(rutas)))
    return 1


if __name__ == "__main__":
    sys.exit(main())