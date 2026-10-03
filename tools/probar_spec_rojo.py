"""Comprueba de EXTREMO A EXTREMO que la regresion visual se pone roja.

No basta con que el guard sepa decir que no: lo que importa es que
`visual.spec.js` FRENE por el, con su nombre y su motivo, en vez de capturar un
lienzo al que le falta un mueble y darlo por bueno.

El truco es NO tocar el `node_modules` de verdad (que es del otro hilo) ni
escribir en ABDSharedAssets: se copia el support a un `WebUI/` de mentira, se le
cambia la raiz que lee el guard, y se corre un spec minimo que hace lo mismo que
el `beforeEach` del spec real.
"""
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEBUI = os.path.join(RAIZ, "WebUI")

sys.path.insert(0, os.path.join(RAIZ, "tools"))
from webui_de_mentira import monta  # noqa: E402

# El spec de mentira: el mismo `expect` que el spec real, contra la raiz falsa.
SPEC = """import { expect, test } from '@playwright/test';
import { compruebaPaqueteCompartido, mensajeDelGuard } from './support/sharedPackage.js';

test('el guard frena la captura cuando al hermano le falta algo', async () => {
  const guard = await compruebaPaqueteCompartido(RAIZ_FALSA);
  expect(guard.ok, mensajeDelGuard(guard)).toBe(true);
});
"""


def main():
    if not os.path.isdir(WEBUI):
        raise SystemExit(f"no encuentro {WEBUI}")

    temporales = []
    try:
        # 1. El WebUI de mentira, con el paquete incompleto.
        base = tempfile.mkdtemp(prefix="webui-rojo-")
        temporales.append(base)
        raiz = monta(os.path.join(base, "WebUI"))
        print(f"arbol de mentira: {raiz}")
        print("   envelope.css presente:",
              os.path.isfile(os.path.join(raiz, "node_modules", "@abdsynths", "shared",
                                          "styles", "components", "envelope.css")))

        # 2. El support del guard, tal cual (sin tocar el original).
        e2e = os.path.join(raiz, "e2e")
        os.makedirs(os.path.join(e2e, "support"))
        shutil.copyfile(
            os.path.join(WEBUI, "e2e", "support", "sharedPackage.js"),
            os.path.join(e2e, "support", "sharedPackage.js"),
        )

        # 3. El spec minimo, con la raiz falsa inyectada.
        spec = SPEC.replace("RAIZ_FALSA", repr(raiz.replace("\\", "/")))
        io.open(os.path.join(e2e, "rojo.spec.js"), "w", encoding="utf-8", newline="\n").write(spec)

        # 4. Correrlo con el Playwright de verdad.
        # El CLI de Playwright por ruta ABSOLUTA: `npx` no esta en el PATH que
        # hereda Python en Windows, y el arbol de mentira no tiene node_modules
        # propios de donde lanzar el binario de la suite.
        cli = os.path.join(WEBUI, "node_modules", "@playwright", "test", "cli.js")
        if not os.path.isfile(cli):
            raise SystemExit(f"no encuentro el CLI de Playwright en {cli}")

        resultado = subprocess.run(
            ["node", cli, "test", "e2e/rojo.spec.js", "--reporter=list"],
            cwd=raiz,
            capture_output=True,
            text=True,
            timeout=300,
        )
        salida = resultado.stdout + resultado.stderr

        paso = "1 passed" in salida
        fallo = "1 failed" in salida
        nombra_el_fichero = "envelope.css" in salida
        nombra_el_paquete = "PAQUETE COMPARTIDO" in salida

        print()
        print("=" * 72)
        print("1 passed  ->", "SI" if paso else "no")
        print("1 failed  ->", "SI" if fallo else "no")
        print("el motivo nombra el fichero que falta ->", "SI" if nombra_el_fichero else "no")
        print("el motivo nombra el paquete        ->", "SI" if nombra_el_paquete else "no")
        print("=" * 72)
        if not salida.strip():
            print("SIN SALIDA de playwright")
        else:
            # Solo el trozo del fallo, que es lo que se quiere ver.
            for linea in salida.splitlines():
                if any(m in linea for m in ("Error", "PAQUETE", "envelope.css", "failed", "passed")):
                    print("  |", linea.strip()[:200])

        problemas = 0
        if paso:
            print("FALLO: la suite paso en verde con el paquete incompleto.")
            problemas += 1
        if not fallo:
            print("FALLO: la suite no se puso roja.")
            problemas += 1
        if not nombra_el_fichero:
            print("FALLO: el motivo no dice QUE falta.")
            problemas += 1
        if not nombra_el_paquete:
            print("FALLO: el motivo no dice DE QUE va.")
            problemas += 1

        print()
        if problemas:
            print(f"{problemas} PROBLEMA(S)")
            return 1
        print("la regresion visual frena en rojo, con el motivo, cuando al hermano le falta algo")
        return 0
    finally:
        for d in temporales:
            shutil.rmtree(d, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
