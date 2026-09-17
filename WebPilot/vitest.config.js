import { defineConfig } from 'vitest/config';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const dirname = path.dirname(fileURLToPath(import.meta.url));

export default defineConfig({
  test: {
    environment: 'jsdom',
    include: ['tests/**/*.test.js', 'tests/**/*.test.jsx'],
    setupFiles: ['../../ABDSharedAssets/tests/setup.js'],
  },
  server: {
    fs: {
      // The shared setup lives outside the WebPilot root (suite layout) —
      // allow Vite/Vitest to serve it.
      allow: [dirname, path.resolve(dirname, '../../ABDSharedAssets')],
    },
  },
});
