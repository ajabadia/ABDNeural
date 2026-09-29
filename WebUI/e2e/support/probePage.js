/**
 * Donde vive la pagina de la aguja (`needle-probe/`).
 *
 * NO sale de `baseURL`: es el servidor de DEV (el segundo `webServer` de
 * `playwright.config.js`), no el preview de `dist`, porque la pagina es de
 * PRUEBA —no entra en el bundle que embebe el plugin— y necesita los fuentes
 * sueltos. Los dos specs que la usan (el suyo y el bloque de agujas de la
 * regresion visual) la leen de aqui para no tener el puerto escrito dos veces.
 *
 * El puerto sale de `NEURONIK_E2E_PROBE_PORT` (5237 por defecto) y lo puede
 * fijar ctest para que este spec tenga el suyo, igual que `E2E_PORT` con el
 * preview de `dist`: los tests corren en paralelo (`ctest -j6`) y con
 * `--strictPort` dos en el mismo se comen un EADDRINUSE. `NEEDLE_PROBE_URL`
 * manda sobre las dos, para apuntar a un servidor ya levantado a mano.
 */
const PROBE_PORT = Number(process.env.NEURONIK_E2E_PROBE_PORT ?? 5237);

export const PROBE_URL =
  process.env.NEEDLE_PROBE_URL ?? `http://localhost:${PROBE_PORT}/needle-probe/`;
