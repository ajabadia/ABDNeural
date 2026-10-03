"""Prueba el guard del paquete compartido en los tres casos que tiene que ver.

No toca el arbol real: arma un `WebUI/` de mentira con un `node_modules` y un
paquete `@abdsynths/shared`(alpha) sinteticos, y llama al guard contra el. Es la
forma de comprobar que el guard ROJA cuando tiene que roja sin tener que mover el
enlace de verdad, que es del otro hilo.

Los tres casos:
  1. COMPLETO   -> ok, sin avisos.
  2. INCOMPLETO -> le falta un fichero de los que la WebUI importa (aquí el
     `styles/components/envelope.css`, que es exactamente el que en este repo
     existe en disco y no en ningun commit).
  3. AUSENTE    -> no hay paquete enlazado.
"""
import io
import json
import os
import shutil
import subprocess
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEBUI = os.path.join(RAIZ, "WebUI")

# Lo que el guard exige: las rutas y los simbolos de EXPORTACIONES.
RUTAS = [
    "styles/tokens.css",
    "styles/components/widgets.css",
    "styles/components/backgrounds.css",
    "styles/components/fx.css",
    "styles/components/envelope.css",
    "contracts/s950_patch_fields.json",
]

SIMBOLOS = [
    "Knob", "NumberBox", "S950_ENCODINGS", "S950_GROUPS", "Segmented", "Select",
    "Slider", "ThemeSwitcher", "Toggle", "WAVEFORM_GLYPHS", "WAVEFORM_NAMES",
    "XYPad", "buildS950Catalogue", "createDrawer", "createEnvelopeCurve",
    "createLcdPanel", "formatS950Name", "isS950Bipolar", "mountFitStage",
]

# Un `src/` minimo que importe lo mismo que el de verdad, para que la comprobacion
# de cobertura del guard no avise.
SRC = """import '@abdsynths/shared/styles/tokens.css';
import '@abdsynths/shared/styles/components/widgets.css';
import '@abdsynths/shared/styles/components/backgrounds.css';
import '@abdsynths/shared/styles/components/fx.css';
import '@abdsynths/shared/styles/components/envelope.css';
import contract from '@abdsynths/shared/contracts/s950_patch_fields.json';
import {
  Knob, NumberBox, S950_ENCODINGS, S950_GROUPS, Segmented, Select, Slider,
  ThemeSwitcher, Toggle, WAVEFORM_GLYPHS, WAVEFORM_NAMES, XYPad,
  buildS950Catalogue, createDrawer, createEnvelopeCurve, createLcdPanel,
  formatS950Name, isS950Bipolar, mountFitStage,
} from '@abdsynths/shared/components';

export { Knob, contract, isS950Bipolar, mountFitStage };
"""


def arma_arbol(base, con_paquete=True, rutas_faltantes=()):
    """Monta un WebUI/ de mentira. `rutas_faltantes` son rutas a NO crear."""
    os.makedirs(os.path.join(base, "src"))
    io.open(os.path.join(base, "src", "app.js"), "w", encoding="utf-8", newline="\n").write(SRC)
    io.open(os.path.join(base, "package.json"), "w", encoding="utf-8", newline="\n").write(
        json.dumps({"name": "webui-falsa", "private": True})
    )

    if not con_paquete:
        return

    destino = os.path.join(base, "node_modules", "@abdsynths", "shared")
    os.makedirs(os.path.join(destino, "components"))
    os.makedirs(os.path.join(destino, "styles", "components"))
    os.makedirs(os.path.join(destino, "contracts"))

    for ruta in RUTAS:
        if ruta in rutas_faltantes:
            continue
        completa = os.path.join(destino, ruta.replace("/", os.sep))
        os.makedirs(os.path.dirname(completa), exist_ok=True)
        if ruta.endswith(".json"):
            io.open(completa, "w", encoding="utf-8", newline="\n").write("{}\n")
        else:
            io.open(completa, "w", encoding="utf-8", newline="\n").write("/* vacio */\n")

    # El barrel: un `export` por simbolo, que es lo que el guard mira.
    lineas = ["// barrel de mentira"]
    for s in SIMBOLOS:
        lineas.append(f"export const {s} = {{}};")
    io.open(
        os.path.join(destino, "components", "index.js"), "w", encoding="utf-8", newline="\n"
    ).write("\n".join(lineas) + "\n")

    io.open(
        os.path.join(destino, "package.json"), "w", encoding="utf-8", newline="\n"
    ).write(json.dumps({"name": "@abdsynths/shared", "version": "0.0.0-falso"}, indent=2) + "\n")


CORREDOR = """
import('file://{guard}').then(async (m) => {{
  const g = await m.compruebaPaqueteCompartido({raiz!r});
  console.log(JSON.stringify({{ ok: g.ok, motivo: g.motivo ?? null, avisos: g.avisos ?? [] }}));
}}).catch((e) => {{ console.log(JSON.stringify({{ ok: false, excepcion: e.message }})); }});
"""


def ejecuta(base, nombre):
    guard = os.path.join(WEBUI, "e2e", "support", "sharedPackage.js").replace("\\", "/")
    script = CORREDOR.format(guard=guard, raiz=base.replace("\\", "/"))
    salida = subprocess.run(
        ["node", "-e", script], capture_output=True, text=True, cwd=WEBUI, timeout=180
    )
    lineas = [l for l in salida.stdout.splitlines() if l.strip().startswith("{")]
    if not lineas:
        return {"ok": None, "motivo": "SIN SALIDA: " + (salida.stderr or "")[:200]}
    return json.loads(lineas[-1])


def main():
    fallos = 0

    print("=" * 72)
    print("1. COMPLETO  -> tiene que VERDE")
    with tempfile.TemporaryDirectory() as base:
        arma_arbol(base, con_paquete=True)
        r = ejecuta(base, "completo")
        print("   ok =", r["ok"])
        if r["ok"] is not True:
            print("   FALLO: se esperaba verde y salio", r)
            fallos += 1
        if r.get("avisos"):
            print("   AVISOS:", r["avisos"])

    print("=" * 72)
    print("2. INCOMPLETO (falta styles/components/envelope.css) -> tiene que ROJO")
    with tempfile.TemporaryDirectory() as base:
        arma_arbol(base, con_paquete=True, rutas_faltantes={"styles/components/envelope.css"})
        r = ejecuta(base, "incompleto")
        print("   ok =", r["ok"])
        print("   motivo =", (r.get("motivo") or "")[:220])
        if r["ok"] is not False:
            print("   FALLO: se esperaba rojo y salio", r)
            fallos += 1
        if r["ok"] is False and "envelope.css" not in (r.get("motivo") or ""):
            print("   FALLO: el motivo no nombra el fichero que falta")
            fallos += 1

    print("=" * 72)
    print("3. AUSENTE (no hay paquete enlazado) -> tiene que ROJO")
    with tempfile.TemporaryDirectory() as base:
        arma_arbol(base, con_paquete=False)
        r = ejecuta(base, "ausente")
        print("   ok =", r["ok"])
        print("   motivo =", (r.get("motivo") or "")[:220])
        if r["ok"] is not False:
            print("   FALLO: se esperaba rojo y salio", r)
            fallos += 1

    print("=" * 72)
    if fallos:
        print(f"{fallos} FALLO(S)")
        return 1
    print("los tres casos se comportan como deben")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
