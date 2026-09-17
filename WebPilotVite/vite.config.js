import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// Raíz local (a diferencia de Turbopack, Vite resuelve bien los symlinks de
// node_modules hacia el workspace anidado de WebPilot sin elevar la raíz).
export default defineConfig({
  plugins: [react()],
  base: './',
  // El public/ compartido vive en WebPilot (assets como bender.png del wheel):
  // por defecto Vite lo busca relativo a esta raíz, donde no existe.
  publicDir: path.resolve(fileURLToPath(new URL('.', import.meta.url)), '../WebPilot/public'),
  build: {
    // Salida DIRECTA a WebPilot/out: es la ruta que consume el host (snapshot
    // embebido) y el selftest, y build.bat la da como buena en ambos motores.
    // emptyOutDir deja out/ solo con ficheros del motor activo.
    outDir: path.resolve(fileURLToPath(new URL('.', import.meta.url)), '../WebPilot/out'),
    emptyOutDir: true,
    assetsDir: 'assets',
  },
});
