import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// ─── Raíz del grafo ─────────────────────────────────────────────────────────
// La raíz de Vite DEBE ser la raíz de la suite (D:/desarrollos/ABDSynths),
// NO este directorio: los paquetes compartidos (@abdsynths/shared,
// @abdsynths/midi-keyb) viven fuera de WebPilotVite como symlinks pnpm del
// workspace anidado de WebPilot. Si la raíz es este dir, quedan fuera del
// grafo y el build falla con "Module not found" (misma lección que Turbopack:
// root = directorio del proyecto NO sirve cuando hay symlinks externos).
const SUITE_ROOT = path.resolve(fileURLToPath(new URL('.', import.meta.url)), '../../../..');

export default defineConfig({
  root: SUITE_ROOT,
  plugins: [react()],

  build: {
    // Salida junto a WebPilot/out para que el swap A/B sea un `mv`.
    outDir: path.resolve(SUITE_ROOT, 'ABDNeural/WebPilot/out-vite'),
    emptyOutDir: true,
    // Recursos con URLs relativas: el host del piloto sirve el snapshot
    // desde la raíz virtual "/" del WebView (sin base absoluta no hay
    // fallback embebido ni navegación file://).
    assetsDir: 'assets',
  },

  // Rutas relativas en el HTML final (equivalente a `next export` sin basePath).
  base: './',

  resolve: {
    // Los CSS globales se importan desde main.jsx; nada que resolver aquí.
    preserveSymlinks: false,
  },
});
