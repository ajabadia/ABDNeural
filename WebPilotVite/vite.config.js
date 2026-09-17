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
    // Salida junto a WebPilot/out para que el swap A/B sea un `mv`.
    outDir: path.resolve(fileURLToPath(new URL('.', import.meta.url)), '../WebPilot/out-vite'),
    emptyOutDir: true,
    assetsDir: 'assets',
  },
});
