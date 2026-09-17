import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    environment: 'jsdom',
    include: ['tests/**/*.test.js', 'tests/**/*.test.jsx'],
    setupFiles: ['../../ABDSharedAssets/tests/setup.js'],
  },
});
