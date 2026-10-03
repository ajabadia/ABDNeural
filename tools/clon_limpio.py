"""Clon LIMPIO de ABDSharedAssets y comprueba que el import de ABDNeural resuelve.

Es la prueba que importa: el arbol de trabajo de al lado tiene ficheros que no
estan en ningun commit, asi que un `require.resolve` ahi sale verde sin decir
nada. Lo que vale es un clon de lo que esta ahora en `origin/main`, que es lo
que vera el runner de CI.

No clona ABDNeural entero (pesa demasiado y aqui no hace falta): clona el
hermano, y comprueba contra el las rutas y simbolos que la WebUI importa, leidos
del arbol real de ABDNeural.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEBUI = os.path.join(RAIZ, "WebUI")
HERMANO = "https://github.com/ajabadia/ABDSharedAssets.git"

# Las rutas que la WebUI importa del paquete, y el prefijo del import.
PREFIJO = re.compile(r"@abdsynths/shared/([^'\"`\s;,)]+)")


def rutas_que_importa_la_webui():
    """Las rutas del paquete que la WebUI importa de verdad, leidas de `src/`."""
    encontradas = set()
    for base, _dirs, archivos in os.walk(os.path.join(WEBUI, "src")):
        for nombre in archivos:
            if not nombre.endswith(".js"):
                continue
            texto = open(os.path.join(base, nombre), encoding="utf-8").read()
            for m in PREFIJO.finditer(texto):
                encontradas.add(m.group(1))
    return sorted(encontradas)


def simbolos_del_barrel(fuente_webui):
    """Los simbolos que la WebUI pide al barrel, con la llave de cierre bien puesta."""
    patron = re.compile(
        r"import\s*\{(?P<nombres>[^}]*)\}\s*from\s*'@abdsynths/shared/components'", re.S
    )
    nombres = set()
    for base, _dirs, archivos in os.walk(osaje := os.path.join(fuente_webui, "src")):
        for nombre in archivos:
            if not nombre.endswith(".js"):
                continue
            texto = open(os.path.join(base, nombre), encoding="utf-8").read()
            for m in patron.finditer(texto):
                for parte in m.group("nombres").split(","):
                    if parte.strip():
                        nombres.add(parte.strip())
    return sorted(nombres), osaje


def main():
    rutas = rutas_que_importa_la_webui()
    simbolos, _ = simbolos_del_barrel(WEBUI)
    print(f"la WebUI importa {len(rutas)} rutas y {len(simbolos)} simbolos del paquete")

    base = tempfile.mkdtemp(prefix="abdshared-limpio-")
    try:
        destino = os.path.join(base, "ABDSharedAssets")
        print(f"clonando {HERMANO} ...")
        clon = subprocess.run(
            ["git", "clone", "--depth", "1", HERMANO, destino],
            capture_output=True, text=True, timeout=600,
        )
        if clon.returncode != 0:
            print("FALLO el clon:", clon.stderr[-400:])
            return 1

        cabeza = subprocess.run(
            ["git", "log", "-1", "--pretty=%h %s"], cwd=destino,
            capture_output=True, text=True,
        ).stdout.strip()
        print(f"clonado en {cabeza}")

        # 1. Las rutas de FICHERO.
        faltan = []
        for ruta in rutas:
            completa = os.path.join(destino, ruta.replace("/", os.sep))
            if not os.path.exists(completa):
                faltan.append(ruta)
        print()
        print(f"rutas presentes: {len(rutas) - len(faltan)}/{len(rutas)}")
        if faltan:
            print("  FALTAN:", faltan)

        # 2. Los simbolos del barrel, importando el indice de verdad.
        barrel = os.path.join(destino, "components", "index.js")
        if not os.path.isfile(barrel):
            print("  FALTA el barrel components/index.js")
            return 1

        # La ruta tiene que ir como URL `file://`: a pelo, una ruta absoluta de
        # Windows es un SyntaxError de Node y la prueba daria falso negativo
        # (medido).
        url_barrel = "file:///" + barrel.replace("\\", "/").lstrip("/")
        codigo = (
            "import * as m from '{barrel}';\n"
            "const pedidos = {pedidos};\n"
            "const faltan = pedidos.filter((n) => !(n in m));\n"
            "console.log(JSON.stringify({{ total: pedidos.length, faltan }}));\n"
        ).format(barrel=url_barrel,
                 pedidos="[" + ",".join(f'"{s}"' for s in simbolos) + "]")

        salida = subprocess.run(
            ["node", "--input-type=module", "-e", codigo],
            cwd=destino, capture_output=True, text=True, timeout=300,
        )
        if salida.returncode != 0:
            print("  FALLO al importar el barrel:"); print(salida.stderr[:1200])
            return 1

        import json
        info = json.loads(salida.stdout.strip().splitlines()[-1])
        print(f"simbolos del barrel presentes: {info['total'] - len(info['faltan'])}/{info['total']}")
        if info["faltan"]:
            print("  FALTAN:", info["faltan"])

        print()
        if faltan or info["faltan"]:
            print("HAY FICHEROS O SIMBOLOS SIN SUBIR")
            return 1
        print("el clon limpio tiene todo lo que la WebUI importa de este repositorio")
        return 0
    finally:
        shutil.rmtree(base, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
