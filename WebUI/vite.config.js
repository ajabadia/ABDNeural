import { defineConfig } from 'vite';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.dirname(fileURLToPath(import.meta.url));

// Raíz del repo (ABDNeural/). El servidor de desarrollo solo sirve ficheros bajo
// `fs.allow`; esta UI importa DOS cosas de fuera de su propia carpeta:
//   - el contrato generado, que vive en WebUI/generated (una sola copia, escrita
//     por NEURONiK_ParameterExport en el paso 2/9 de build.bat);
//   - los tokens/widgets de @abdsynths/shared, que es un paquete del workspace.
const repoRoot = path.resolve(root, '..');

export default defineConfig({
  // Rutas relativas: el host embebe la página y no sirve desde una raíz web.
  base: './',
  // Los assets compartidos (bender.png del wheel, etc.) y el worklet viven en
  // WebUI/public — UNA sola copia, la de esta pagina. Hasta el ticket 8.4 este
  // publicDir apuntaba a WebPilot/public (el piloto era el dueno de los assets
  // compartidos); la retirada los mudo aqui y este camino es el que los copia a
  // `dist/` (y de ahi los embebe el plugin).
  publicDir: path.resolve(root, 'public'),
  server: {
    fs: { allow: [root, repoRoot, path.resolve(repoRoot, '../ABDSharedAssets')] },
  },
  build: {
    // Salida propia: `WebUI/dist` es lo que embebe el PLUGIN (juce_add_binary_data),
    // lo que sirve la bancada WebView2 y lo que sirve start.bat. Ya no hay una segunda
    // exportacion a la que apuntar: el piloto se retiro (ticket 8.4).
    outDir: 'dist',
    emptyOutDir: true,
    assetsDir: 'assets',
  },
});
