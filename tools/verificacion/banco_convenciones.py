#!/usr/bin/env python3
"""banco_convenciones.py: prueba que los dos verificadores AVISAN.

POR QUE HACE FALTA

Un verificador que solo se ha visto en verde no prueba nada: puede que nunca
llegue a fallar, y mientras todos los ficheros del repo cumplan, eso no se
distingue de un script que no mira. Este banco monta ficheros con cada fallo,
los pasa por `convenciones.py` y comprueba que los ve; y monta ficheros limpios
y comprueba que NO los ve, que es la otra mitad y la que se olvida.

TAMBIEN PRUEBA QUE DETECTAR_TILDES NO INVENTE

Ese script decide el grupo de tildes de cada fichero contando palabras. Si
cuenta de mas, dira que un `.bat` ASCII lleva tildes, y ese es justo el error
que hace dano. Por eso el banco le pasa los `.bat` del repo, cuyo grupo ya
esta medido a mano, y comprueba que ninguno sale como CON. Y le pasa un texto
lleno de tildes que tiene que salir como CON.

Los caracteres se escriben con ch() y no a mano, por dos razones que las dos
han pasado en este banco. La primera, que un escape escrito a mano es
indistinguible de un error de tecleo. La segunda, y mas cara, que se
escribieron las palabras del caso SIN la tilde que el caso queria
llevar: un banco que no detecta el fallo que dice detectar esta probando
lo contrario, y el verificador sale inocente cuando el culpable es el banco.
un `\\x5730` escrito a mano es indistinguible de un error de tecleo, y MEDIDO el
2026-10-03 en este mismo banco, uno de ellos se leyo como la letra `W` seguida
de un cero, el fichero de prueba salio sin ideogramas y el verificador parecio
fallar cuando era el banco el que estaba mal.

USO

    python tools/verificacion/banco_convenciones.py

Sale 0 si todos los casos pasan, 1 con los que no.
"""

import io
import os
import shutil
import subprocess
import sys
import tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(os.path.dirname(AQUI))
CONVENCIONES = os.path.join(AQUI, "convenciones.py")
TILDES = os.path.join(AQUI, "detectar_tildes.py")

VT = bytes([11])


def ch(*codigos):
    """Los caracteres por su numero, y no escritos a mano."""
    return "".join(chr(c) for c in codigos)


# (nombre con su extension, contenido, el verificador DEBE quejarse)
CASOS = [
    ("lf_en_bat.bat", b"@echo off\nREM con LF\n", True),
    ("vertical_tab_en_bat.bat", b"@echo off\r\nREM a" + VT + b"b\r\n", True),
    ("crlf_en_sh.sh", b"#!/bin/sh\necho hola\r\n", True),
    ("cjk.md", ("# Titulo\n\n" + ch(0x5730, 0x4E00) + "\n").encode("utf-8"), True),
    ("vallas_impares.md", b"# Titulo\n\n```js\ncode\n", True),
    ("tilde_en_bat.bat", ("@echo off" + chr(13) + chr(10) + "REM aqu" + ch(0xED) + " est" + ch(0xE1) + chr(13) + chr(10)).encode("utf-8"), True),
    ("ansi_en_bat.bat", b"@echo off\r\nREM \xA1\xE9\r\n", True),
    # Los .ps1 tambien son ASCII PURO. MEDIDO el 2026-10-04: PowerShell 5.1
    # sin BOM lee con la ANSI del sistema, y una enye llega partida en dos.
    # Antes los .ps1 tenian lista blanca en el hook y aqui no se comprobaba
    # nada, que es como se colaron 66 caracteres.
    ("tilde_en_ps1.ps1", ("Write-Output 'asi" + chr(0xF1) + "'" + chr(10)).encode("utf-8"), True),
    ("angulo_en_ps1.ps1", ("# " + chr(0xAB) + chr(0xBB) + chr(10)).encode("utf-8"), True),
    ("raya_en_ps1.ps1", ("Write-Output 'a" + chr(0x2014) + "b'" + chr(10)).encode("utf-8"), True),
    # Los cuatro que tienen que estar callados.
    ("bat_limpio.bat", b"@echo off\r\nREM ok\r\n", False),
    ("ps1_limpio.ps1", b"Write-Output 'hola'\n", False),
    ("sh_limpio.sh", b"#!/bin/sh\necho ok\n", False),
    ("md_limpio.md", b"# Titulo\n\n```\ncode\n```\n", False),
]


def correr(script, argumentos):
    return subprocess.run([sys.executable, script] + list(argumentos),
                          capture_output=True, text=True, encoding="utf-8", cwd=RAIZ)


def escribir(destino, nombre, contenido):
    ruta = os.path.join(destino, nombre)
    with open(ruta, "wb") as f:
        f.write(contenido)
    return ruta


def banco_convenciones():
    print("  Cada fichero por separado, que se queje cuando toca:")
    fallos = 0
    for nombre, contenido, debe_quejarse in CASOS:
        d = tempfile.mkdtemp(prefix="banco_uno_")
        try:
            ruta = escribir(d, nombre, contenido)
            r = correr(CONVENCIONES, [ruta])
            se_quejo = r.returncode == 1
            bien = se_quejo == debe_quejarse
            if not bien:
                fallos += 1
            print("    [%s] %-24s deberia %-9s y %s"
                  % ("PASA" if bien else "FALLA", nombre,
                     "quejarse" if debe_quejarse else "callarse",
                     "se quejo" if se_quejo else "se callo"))
        finally:
            shutil.rmtree(d, ignore_errors=True)

    # Y los diez juntos: tiene que salir con rc=1, no con excepcion.
    d = tempfile.mkdtemp(prefix="banco_todos_")
    try:
        rutas = [escribir(d, n, c) for n, c, _ in CASOS]
        r = correr(CONVENCIONES, rutas)
        bien = r.returncode == 1
        if not bien:
            fallos += 1
        print("    [%s] los %d juntos salen con rc=%d (se esperaba 1)"
              % ("PASA" if bien else "FALLA", len(CASOS), r.returncode))
        if not bien:
            print("        " + (r.stderr or "").strip()[:300])
    finally:
        shutil.rmtree(d, ignore_errors=True)

    # Y el repo entero, pero SOLO lo trackeado. MEDIDO el 2026-10-03: pasandole
    # el arbol entero, el banco sale en rojo por `_fix_paso2.py`, que es un
    # temporal de otra sesion sin trackear y no es fuente de nadie. Una suite
    # que verifica el repositorio no tiene que ver bien lo que hay suelto por
    # el suelo, y si lo revisa, avisa de algo que nadie puede arreglar.
    # MEDIDO el 2026-10-03, dos cosas en esta misma llamada. La primera, que
    # iba con `.split()` y no con `.splitlines()`. La segunda, que `text=True` sin
    # `encoding` decodifica la salida con la pagina de codigos de la consola, que en
    # Windows es cp1252: los dos bytes UTF-8 de la "o" con tilde se convierten en DOS
    # caracteres, la ruta deja de existir, y el verificador dice "no se ha podido
    # leer" de un fichero que esta ahi. Con `encoding="utf-8"` la ruta llega entera.

    # Los nombres del repo tienen espacios (`DOC/1 - inicio.docx`), y `.split()`
    # parte la ruta en `DOC/1`, `-` e `inicio.docx`, que no existen. El verificador
    # decia "no se ha podido leer" de un fichero que si esta, y el culpa era el
    # que se lo pasaba. Con `.splitlines()` cada ruta llega entera.
    trackeados = subprocess.run(["git", "-c", "core.quotePath=false", "ls-files"], cwd=RAIZ,
                                capture_output=True, text=True, encoding="utf-8").stdout.splitlines()
    r = correr(CONVENCIONES, trackeados)
    bien = r.returncode == 0
    if not bien:
        fallos += 1
        print("    [FALLA] el repo entero tiene algo que arreglar:")
        for linea in r.stdout.splitlines()[:20]:
            print("        " + linea)
    print("    [%s] los %d ficheros trackeados pasan (rc=0)"
          % ("PASA" if bien else "FALLA", len(trackeados)))
    return fallos == 0


def banco_tildes():
    fallos = 0
    bats = subprocess.run(["git", "-c", "core.quotePath=false", "ls-files", "*.bat"], cwd=RAIZ,
                          capture_output=True, text=True, encoding="utf-8").stdout.splitlines()
    if not bats:
        print("    [FALLA] no hay .bat en el repo")
        return False

    r = correr(TILDES, bats)
    if r.returncode != 0:
        print("    [FALLA] detectar_tildes sale con rc=%d" % r.returncode)
        print("        " + (r.stderr or "").strip()[:300])
        return False

    # Los .bat del repo NO pueden salir como CON tildes. Salir como SIN o como
    # SIN SENAL esta bien: lo que se busca es que no invente.
    _LINEA_CON = "CON tildes"
    texto = r.stdout.split(_LINEA_CON)[1].split("SIN tildes")[0] if _LINEA_CON in r.stdout else ""
    intrusos = [b for b in bats
                if os.path.basename(b) in texto or b.replace("/", "\\") in texto]
    if intrusos:
        fallos += 1
        for b in intrusos:
            print("    [FALLA] %s sale como CON tildes" % b)
    else:
        print("    [PASA] ninguno de los %d .bat sale como CON tildes" % len(bats))

    # Y el caso contrario: un texto con tildes tiene que salir como CON.
    d = tempfile.mkdtemp(prefix="banco_tildes_")
    try:
        frase = ("Esta esta vez. Solo solo hay " + ch(0xE1) + " y " + ch(0xE9)
                 + " y " + ch(0xF3) + ". Está está más día cómo dónde qué.\n")
        ruta = escribir(d, "con_tildes.md", (frase * 4).encode("utf-8"))
        r2 = correr(TILDES, [ruta])
        bien = r2.returncode == 0 and _LINEA_CON in r2.stdout
        if not bien:
            fallos += 1
        print("    [%s] un texto con tildes sale como CON" % ("PASA" if bien else "FALLA"))
        if not bien:
            print("        " + r2.stdout.strip()[:300])
    finally:
        shutil.rmtree(d, ignore_errors=True)
    return fallos == 0


def banco_arbol():
    """El modo ARBOL tiene que saltarse lo que git ignora, y solo eso.

    MEDIDO el 2026-10-04 que salia con rc=1 por `_fix_paso2.py`, un temporal
    sin trackear de otra sesion. Saltarselo con una lista de nombres es cambiar
    el sintoma cada vez que aparece un temporal nuevo, asi que se salto lo que
    git ya ignora, y este banco monta las DOS mitades para que el salto no se
    convierta en un verde que no mira:

      - lo ignorado con CJK, que NO se puede quejar (no es fuente de nadie);
      - lo NO ignorado y sin trackear con CJK, que SI tiene que quejarse, que
        es justo el fichero que alguien esta a punto de commitear.

    Si alguna vez la segunda dejara de quejarse, esto estaria:callando ante un
    fichero que puede entrar en el repo, que es el fallo que importa.
    """
    print("  El modo ARBOL, con lo que git ignora y con lo que no:")
    fallos = 0
    d = tempfile.mkdtemp(prefix="banco_arbol_")
    try:
        subprocess.run(["git", "init", "-q", d], capture_output=True)
        escribir(d, ".gitignore", b"ignorado_*.py\n")
        cjk = ("# " + ch(0x5730, 0x4E00) + "\n").encode("utf-8")
        escribir(d, "ignorado_malo.py", cjk)
        escribir(d, "suelto_malo.py", cjk)

        r = correr(CONVENCIONES, ["--arbol", d])
        vio_suelto = "suelto_malo.py" in r.stdout
        vio_ignorado = "ignorado_malo.py" in r.stdout
        bien = r.returncode == 1 and vio_suelto and not vio_ignorado
        if not bien:
            fallos += 1
        print("    [%s] ve el suelto y no ve el ignorado (rc=%d)"
              % ("PASA" if bien else "FALLA", r.returncode))
        if not bien:
            print("        %s" % (r.stdout or r.stderr).strip()[:300])

        # Y ya sin el suelto: solo queda el ignorado, y ahi tiene que CALLARSE.
        os.remove(os.path.join(d, "suelto_malo.py"))
        r2 = correr(CONVENCIONES, ["--arbol", d])
        bien = r2.returncode == 0
        if not bien:
            fallos += 1
        print("    [%s] solo con lo ignorado se calla (rc=%d)"
              % ("PASA" if bien else "FALLA", r2.returncode))
        if not bien:
            print("        %s" % (r2.stdout or r2.stderr).strip()[:300])
    finally:
        shutil.rmtree(d, ignore_errors=True)
    return fallos == 0


def main():
    print("convenciones.py:")
    a = banco_convenciones()
    print("")
    print("modo ARBOL:")
    c = banco_arbol()
    print("")
    print("detectar_tildes.py:")
    b = banco_tildes()
    print("")
    print("Banco: %s" % ("los tres bien" if a and b and c else "ALGO FALLA"))
    return 0 if (a and b and c) else 1


if __name__ == "__main__":
    sys.exit(main())