import { defineConfig } from 'vite';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.dirname(fileURLToPath(import.meta.url));

// Raíz del repo (ABDNeural/). El servidor de desarrollo solo sirve ficheros bajo
// `fs.allow`; esta UI importa DOS cosas de fuera de su propia carpeta:
//   - el contrato generado, que vive en WebPilot/generated (una sola copia,
//     escrita por NEURONiK_ParameterExport en el paso 2/9 de build.bat);
//   - los tokens/widgets de @abdsynths/shared, que es un paquete del workspace.
const repoRoot = path.resolve(root, '..');

export default defineConfig({
  // Rutas relativas: el host embebe la página y no sirve desde una raíz web.
  base: './',
  // Los assets compartidos (bender.png del wheel, etc.) y el worklet siguen
  // viviendo en WebPilot/public — una sola copia para todas las UIs.
  publicDir: path.resolve(repoRoot, 'WebPilot/public'),
  server: {
    fs: { allow: [root, repoRoot, path.resolve(repoRoot, '../ABDSharedAssets')] },
  },
  build: {
    // Salida propia. OJO: WebPilot/out sigue siendo lo que embebe el host del
    // piloto y lo que sirve start.bat; el cambio de motor de UI es un paso
    // deliberado del ticket 8.2, no un efecto colateral de compilar esto.
    outDir: 'dist',
    emptyOutDir: true,
    assetsDir: 'assets',
  },
});
