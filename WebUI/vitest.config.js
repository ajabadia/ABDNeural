import { defineConfig } from 'vitest/config';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.dirname(fileURLToPath(import.meta.url));

export default defineConfig({
  test: {
    environment: 'jsdom',
    include: ['tests/**/*.test.js'],
    // El setup es compartido con el resto de la suite (polyfill de PointerEvent
    // para los controles): vive fuera de esta raíz y Vite necesita permiso.
    setupFiles: ['../../ABDSharedAssets/tests/setup.js'],
  },
  server: {
    fs: { allow: [root, path.resolve(root, '../..')] },
  },
});
