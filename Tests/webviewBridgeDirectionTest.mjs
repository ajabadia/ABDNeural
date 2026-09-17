/**
 * ABDNeural — ejecuta el guard de dirección del canal WebView2 sobre `Source/`.
 *
 * El guard vive en `ABDSharedCode/WebView2Bridge/testing/webviewBridgeDirection.js` y lo
 * comparten todos los hosts: comprueba que el código C++ nunca alcanza los listeners del
 * WebUI con `backend.emitEvent` (canal JS → nativo, ahí el mensaje se descarta en silencio)
 * y que el fichero que debe emitir lo hace con `emitEventIfBrowserIsVisible`.
 *
 * ABDNeural no tiene runner JS, así que se lanza con `node` directamente desde ctest.
 * Salida: exit code 0 si el canal está bien, 1 si hay algún problema (con el detalle).
 */

import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { checkBridgeDirection, formatFindings } from '../../ABDSharedCode/WebView2Bridge/testing/webviewBridgeDirection.js';

const testsDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.dirname(testsDirectory);

// `WebPilotHost.cpp` es el único que emite hacia el WebUI: la mitad positiva del guard
// falla si deja de hacerlo, que es justo el fallo silencioso que vigila.
const result = checkBridgeDirection({
  sourceRoot: path.join(repositoryRoot, 'Source'),
  emitters: ['WebPilotHost.cpp'],
});

console.log(`Bridge direction guard — ${result.scanned} ficheros C++ analizados en Source/`);

if (!result.ok) {
  console.error(formatFindings(result));
  process.exit(1);
}

console.log(formatFindings(result));
