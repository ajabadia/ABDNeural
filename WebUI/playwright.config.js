import { defineConfig, devices } from '@playwright/test';

/**
 * El puerto del servidor de `dist`. Es una variable de entorno y no un numero
 * fijo por una razon medida: `NEURONiK_WebUiLocalModeE2e` y
 * `NEURONiK_WebUiVisualRegression` son dos tests de ctest que se pueden LANZAR A
 * LA VEZ (`ctest -j6`, que es como corre la casa), y los dos montan su servidor
 * con `--strictPort`: con el mismo puerto, el segundo se come un EADDRINUSE y
 * falla en 13 s con un error que no habla de pintura. Cada test lleva su puerto
 * (5236 el smoke, 5239 la regresion visual) y asi conviven.
 */
const E2E_PORT = Number(process.env.NEURONIK_E2E_PORT ?? 5236);

/**
 * E2E de navegador REAL para la WebUI (Chromium de verdad, no jsdom).
 *
 * Para qué: el vitest prueba la pagina contra el DOM simulado y los tests de node
 * (`Tests/*.mjs`) prueban el DSP leyendo el WASM a mano, pero NINGUNO de los dos
 * arranca el motor por el camino que usa el usuario (SOUND ON -> AudioWorklet ->
 * AudioContext). Lo que se verifica a mano en el navegador -y hasta hoy solo estaba
 * escrito en HANDOFF- se automatiza aqui: `e2e/localMode.spec.js` recorre el smoke del
 * MODO LOCAL (SOUND ON, ranura de modelo, nota, pad) con aserciones sobre el DOM y
 * sobre los mensajes que la pagina manda al worklet.
 *
 * Sirve `dist` (no `src`): es el artefacto que embebe el plugin, asi que el comando
 * del servidor CONSTRUYE antes de servir. Sin ese `npm run build` el E2E mediria un
 * bundle viejo -la leccion ya esta escrita en HANDOFF- y pasaria (o fallaria) por
 * motivos que no son del codigo actual.
 *
 * `--mute-audio` NO es cosmetico: sin el, Chromium crea un AudioContext 'running'
 * cuyo reloj NO avanza (ni headless ni con ventana, medido), el grafo no se tira y el
 * worklet no procesa: voz 0, meter 0 y anillo quieto aunque el badge diga ON.
 * `--autoplay-policy` deja arrancar el contexto sin depender de la politica de autoplay.
 *
 * REGRESION VISUAL (mismo patron que el hermano ABDMS2000): las referencias
 * viven en `e2e/snapshots/` y las compara `toHaveScreenshot` con el umbral de
 * mas abajo. Seeparar el spec es lo que permite correrlo solo
 * (`pnpm test:visual`) y, sobre todo, REGENERAR las referencias sin tocar las
 * del smoke: `--update-snapshots` reescribe SOLO las de `visual.spec.js`.
 *
 * El umbral (100 px y 0.2 de diferencia por pixel) es el del hermano, y esta vez
 * se nota POR QUE: el lienzo es de diseño fijo y se escala con `transform` para
 * caber en el editor (mountFitStage), asi que si el viewport no es el tamaño de
 * diseño la referencia seria una foto reescalada y cada pixel de texto
 * anti-aliasea distinto. Por eso `e2e/visual.spec.js` fija el viewport al SSOT
 * (CANVAS de sections.js) y exige escala 1 antes de capturar: la referencia es
 * 1:1 y el umbral solo tiene que absorber el ruido de rasterizado, no un
 * reescalado.
 *
 * Las referencias se generaron en Chromium/Windows, que es donde hay que
 * compararlas: entre sistemas operativos cambia el rasterizado de las fuentes
 * del sistema (la pagina no carga ninguna webfont, usa `system-ui`), y un
 * baseline de Windows sobre un runner Linux fallaria por eso y no por el codigo.
 *
 * Uso:
 *   pnpm test:e2e          (equivalente a: npx playwright test)
 *   pnpm test:visual       (solo la regresion visual)
 *   npx playwright test e2e/visual.spec.js --update-snapshots   (tras un cambio INTENCIONAL)
 *   npx playwright test --headed     (con ventana, para mirar)
 *   npx playwright test --ui
 *
 * En una maquina nueva hace falta el navegador: `npx playwright install chromium`.
 */
export default defineConfig({
  testDir: './e2e',
  // En serie: hay UN servidor y un unico servicio de audio de Chromium; los tests
  // comparten el mismo worklet y el mismo reloj (paralelizarlos los hace pelear).
  fullyParallel: false,
  workers: 1,
  forbidOnly: !!process.env.CI,
  retries: process.env.CI ? 1 : 0,
  reporter: [['list']],
  // El arranque incluye el calentamiento del servicio de audio (hasta ~6 s).
  timeout: 60_000,
  use: {
    baseURL: `http://localhost:${E2E_PORT}`,
    trace: 'on-first-retry',
    screenshot: 'only-on-failure',
  },
  projects: [
    {
      name: 'chromium',
      use: {
        ...devices['Desktop Chrome'],
        launchOptions: {
          args: ['--autoplay-policy=no-user-gesture-required', '--mute-audio'],
        },
      },
    },
  ],
  webServer: [
    {
      command: `npm run build && npx vite preview --port ${E2E_PORT} --strictPort`,
      url: `http://localhost:${E2E_PORT}`,
      reuseExistingServer: !process.env.CI,
      timeout: 180_000,
    },
    // La pagina de prueba de la aguja (needle-probe) se sirve desde el DEV, no
    // desde dist: es una pagina de PRUEBA (no entra en el bundle que embebe el
    // plugin) y necesita los fuentes sueltos (src/ al descubierto, sin build).
    // Puerto propio por la misma razon que E2E_PORT: ctest -j6.
    {
      command: 'npx vite --port 5237 --strictPort',
      url: 'http://localhost:5237/needle-probe/',
      reuseExistingServer: !process.env.CI,
      timeout: 120_000,
    },
  ],
  // Las REFERENCIAS de la regresion visual. Se versionan (son el patron del
  // hermano ABDMS2000): sin la referencia el test falla diciendo que la ha
  // escrito, que es justo el aviso de "hay que mirarlo antes de aceptarlo".
  snapshotDir: './e2e/snapshots',
  // El nombre lleva el test Y el argumento: renombrar un test o una captura deja
  // su referencia vieja huerfana a la vista, en vez de reutilizar la de otro.
  snapshotPathTemplate: '{testDir}/snapshots/{testFileDir}/{testName}-{arg}{ext}',
  expect: {
    toHaveScreenshot: {
      // EL UMBRAL, con los tres numeros MEDIDOS en este lienzo (no copiados del
      // hermano: alli `maxDiffPixels` era 100, y aqui se ve que es ciego):
      //
      //   - el RUIDO entre dos corridas seguidas es de 0 pixeles: el lienzo es
      //     estatico sin audio y el navegador lo fija el lockfile, asi que la
      //     suite entera pasa con `maxDiffPixels: 0`;
      //   - la regresion mas PEQUENA que se ha podido construir (el distintivo
      //     vivo `0/4` pasando de color apagado a acento) mueve 77 pixeles;
      //   - un desplazamiento de 1 px en la rejilla de la ficha mueve 206.
      //
      // 20 queda un factor 3.8 por debajo de la mas pequena y sigue respirando
      // ante un build de Chromium que se lleve un punado de pixeles de
      // rasterizado. El que absorbe de verdad el antialiasing es el
      // `threshold` de debajo (un pixel cuenta como distinto a partir del 20%
      // de diferencia). Con el 100 del hermano, la del distintivo PASABA en
      // verde, que es justo el fallo que esta suite viene a cazar. Para ser
      // absolutamente estricto, 0 tambien pasa aqui: subelo solo con un motivo
      // escrito.
      maxDiffPixels: 20,
      threshold: 0.2,
    },
  },
});
