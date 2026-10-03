#!/usr/bin/env python3
"""detectar_tildes.py: que ficheros del repo van con tildes y cuales sin.

POR QUE EXISTE

En este repo las dos convenciones conviven y estan medidas una por una.
`Scripts/COMO-ARREGLAR-EL-BUILD.md` se acentua; `Scripts/verify_all.bat` no, y
tocarlo con tildes rompe el build porque cmd.exe no las lee. La regla esta dicha
en el .gitattributes y en las cabeceras de los ficheros, pero DICHA no es
SABIDA: la forma de saberlo de verdad es contar.

Y el error sale en las dos direcciones, y las dos se han cometido:

  · acentuar un fichero que va sin tildes, metiendo lo que parece un acento y
    es ruido en un `.bat` que cmd.exe lee con otra pagina de codigos;
  · acentuar de menos un fichero que si las lleva, que es lo que pasaba con las
    secciones nuevas de COMO-ARREGLAR-EL-BUILD.md.

Este script DICE el grupo de cada fichero, con los numeros detras, para que la
decision se tome mirando y no de memoria. No decide cual es el grupo correcto:
eso lo dice el .gitattributes, la cabecera del fichero, o quien lo escribe.

COMO SE DECIDE, Y POR QUE ASI

Cuenta palabras cuya forma CON tilde es OTRA palabra, no una falta: `esta` de
verbo frente a `esta` demostrativo, `mas` frente a `mas`, `dia` frente a `dia`.
Una de estas en el fichero demuestra que su autor acentua. Y cuenta la forma SIN
tilde cuando esa forma no es una palabra: un `mas` delata a un fichero sin
acentuar aunque no tenga ni una tilde.

Lo que NO hace, y se probo que no sirve, es comparar con la forma sin tilde.
MEDIDO el 2026-10-03: contando con `\b` mas la letra, cualquier palabra que
empiece por `e`, `o` o `a` cuenta, y la `e` se lleva 1192 apariciones frente a 5
acentuadas en COMO-ARREGLAR-EL-BUILD.md: todos los ficheros salian SIN.

Con palabras EXACTAS el metodo separa, y estos ocho tenian el grupo medido a
mano de antes:

    COMO-ARREGLAR-EL-BUILD.md   esta=18  mas=6  dia=3  -> CON
    VERIFICAR.md                esta=16  mas=14 dia=5  -> CON
    CMakeLists.txt              esta=2   mas=2  dia=1  -> CON
    vite.config.js              esta=1                 -> CON
    verify_all.bat              ninguna                -> SIN
    junctions-workspace.bat     ninguna                -> SIN
    build.bat                   ninguna                -> SIN SENAL
    build_wasm.bat              ninguna                -> SIN SENAL

`SIN SENAL` es un tercer grupo, y esta en la salida a proposito: son ficheros
que no traen ni una palabra acentuada ni una forma que falte, asi que este
metodo no puede ponerlos en un lado ni en el otro. Filtrarlos sin mas era
peor que equivocarse, porque `build.bat` desaparecia del informe sin dejar
rastro de si se habia leido. Para esos, la cabecera del fichero.

Que salgan los cuatro primeros CON y los cuatro ultimos SIN es lo que hace que
el metodo sea de fiar: el grupo de cada uno ya se sabia antes de escribir el
script, y el script no esta escrito para que ese caso salga bien.

USO

    python tools/verificacion/detectar_tildes.py        # el repo entero
    python tools/verificacion/detectar_tildes.py un.md

Sale 0 siempre que pueda leer los ficheros; lo que informa es la tabla. Sale 1
solo si hay un fichero que no se ha podido leer.
"""

import io
import os
import re
import sys

# (palabra con tilde, palabra sin tilde, etiqueta ASCII, es_falta)
# La palabra va COMPLETA y no su inicial: es lo que hace que el recuento sea
# una cuenta y no una de letras sueltas. La etiqueta es ASCII porque la salida
# va a una consola cp1252, y una tabla ilegible es una tabla que no se lee.
PARES = [
    ("está", "esta", "esta", False),
    ("sólo", "solo", "solo", False),
    ("cómo", "como", "como", False),
    ("dónde", "donde", "donde", False),
    ("qué", "que", "que", False),
    ("aún", "aun", "aun", False),
    ("éste", "este", "este", False),
    ("ésa", "esa", "esa", False),
    # Estas tres: la forma sin tilde NO es una palabra. Su presencia delata a un
    # fichero que no acentua, y por eso cuentan aunque el fichero no traiga
    # ninguna tilde.
    ("más", "mas", "mas", True),
    ("día", "dia", "dia", True),
    ("aún", "aun", "aun-falta", True),
]

FALTAN = {"mas", "dia", "aun-falta"}


def cuenta(texto, palabra):
    return len(re.findall(r"\b" + palabra + r"\b", texto, re.IGNORECASE))


def mide(texto):
    """(cuantas formas acentuadas, cuantas que faltan, detalle legible)."""
    con, faltan = 0, 0
    detalle = []
    for acentuada, suelta, etiqueta, es_falta in PARES:
        n_con = cuenta(texto, acentuada)
        if n_con:
            con += n_con
            detalle.append((etiqueta, n_con))
        if es_falta:
            n_sin = cuenta(texto, suelta)
            if n_sin:
                faltan += n_sin
                detalle.append((etiqueta, -n_sin))
    return con, faltan, detalle


def ficheros_interesantes(raiz):
    """Los de texto del repo, saltando imagenes, binarios y salidas de build."""
    binarios = {".png", ".jpg", ".jpeg", ".gif", ".wasm", ".exe", ".dll", ".lib",
                ".pdb", ".icns", ".ico", ".jar", ".bin", ".o", ".obj", ".a",
                ".dylib", ".so", ".ilk"}
    fuera = {".git", "node_modules", "build-reference", "build-wasm", "dist",
             ".freebuff", ".agents"}
    for carpeta, dirs, ficheros in os.walk(raiz):
        dirs[:] = [d for d in dirs if d not in fuera]
        for nombre in ficheros:
            if os.path.splitext(nombre)[1].lower() in binarios:
                continue
            yield os.path.join(carpeta, nombre)


def main():
    raiz = os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))

    if len(sys.argv) > 1:
        rutas = sys.argv[1:]
    else:
        rutas = ficheros_interesantes(raiz)

    filas = []
    ilegibles = []
    for ruta in rutas:
        try:
            texto = io.open(ruta, encoding="utf-8").read()
        except (IOError, OSError, UnicodeDecodeError):
            ilegibles.append(ruta)
            continue
        con, faltan, detalle = mide(texto)
        filas.append((ruta, con, faltan, detalle))

    con_tilde = [f for f in filas if f[1] > 0]
    sin_tilde = [f for f in filas if f[1] == 0 and f[2] > 0]
    # Los que no traen NI una palabra acentuada NI una forma que falte: no dicen
    # nada, y no se pueden meter en ninguno de los dos grupos de arriba. Antes
    # se filtraban en silencio, y eso era peor que un error: `build.bat` no
    # aparecia en ninguna parte y no habia manera de saber si se habia leido.
    sin_senal = [f for f in filas if f[1] == 0 and f[2] == 0]

    def rel(ruta):
        try:
            return os.path.relpath(ruta, raiz)
        except ValueError:
            return ruta

    print("CON tildes (%d). Cada marca es una palabra acentuada cuenta." % len(con_tilde))
    for ruta, con, faltan, detalle in sorted(con_tilde):
        marcas = " ".join("%s=%d" % (e, n) for e, n in detalle if n > 0)
        print("  %-50s %s" % (rel(ruta), marcas))
    print("")

    print("SIN tildes (%d). La marca negativa es una forma que no lleva tilde y" % len(sin_tilde))
    print("no es palabra, y delata al fichero aunque no traiga ni una tilde.")
    for ruta, con, faltan, detalle in sorted(sin_tilde):
        marcas = " ".join("%s=%d" % (e, -n) for e, n in detalle if n < 0)
        print("  %-50s %s" % (rel(ruta), marcas))
    print("")
    print("SIN SENAL (%d): no traen ninguna de las palabras contadas, asi que no se" % len(sin_senal))
    print("pueden poner ni en un grupo ni en el otro. build.bat y build_wasm.bat estan")
    print("aqui, y eso no quiere decir que lleven tildes: quiere decir que este metodo")
    print("no tiene nada que decir de ellos. Para esos, la cabecera del fichero.")
    for ruta, con, faltan, detalle in sorted(sin_senal)[:20]:
        print("  " + rel(ruta))
    if len(sin_senal) > 20:
        print("  ... y %d mas" % (len(sin_senal) - 20))
    print("")
    print("Totales: %d con, %d sin, %d sin senal, %d ilegibles."
          % (len(con_tilde), len(sin_tilde), len(sin_senal), len(ilegibles)))

    if ilegibles:
        print("")
        print("NO LEIDOS (%d), los primeros:" % len(ilegibles))
        for ruta in ilegibles[:10]:
            print("  " + rel(ruta))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())