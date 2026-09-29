/**
 * El guard del reloj de audio, compartido por los specs que arrancan el motor de
 * verdad (`localMode`, `needleProbe` y el bloque de agujas de `visual`).
 *
 * Por que existe y por que NO se puede quitar: con `--mute-audio`, Chromium crea
 * un `AudioContext` 'running' cuyo reloj NO avanza (medido, ni headless ni con
 * ventana). El grafo no se tira, el worklet no procesa, y la pagina miente: el
 * badge dice ON, las voces valen 0 y las needles no se mueven. Un E2E que espera
 * ese estado se queda colgado hasta el timeout en vez de fallar con un motivo, y
 * uno que lo esquivase estaria probando un motor parado. Se SONDEA con contextos
 * de usar y tirar porque el primero tarda ~4 s y puede quedarse clavado para
 * siempre (medido).
 *
 * @returns {Promise<number>} intentos consumidos; 0 = este entorno no procesa audio.
 *   El que llama hace `test.skip(!attempts, ...)`: sin audio la prueba se dice
 *   saltada y no se genera una referencia visual de un motor parado.
 */
export async function ensureAudioClock(page, attempts = 24) {
  return page.evaluate(async (tries) => {
    for (let attempt = 1; attempt <= tries; attempt += 1) {
      const context = new AudioContext();

      await context.resume().catch(() => {});

      const before = context.currentTime;
      await new Promise((resolve) => setTimeout(resolve, 250));
      const advanced = context.currentTime - before;

      await context.close();

      if (advanced > 0) return attempt;
    }

    return 0;
  }, attempts);
}
