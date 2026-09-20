/**
 * Espectral — visualizador de los 64 parciales del motor.
 * ========================================================
 *
 * El frame de telemetría trae `spectral[64]` (0..1 por parcial, ya decimado a
 * ~15 Hz y value-diffed: un motor en reposo no emite). Como los anillos, es
 * PINTURA y no estado: vive fuera del ciclo setState y se alimenta por
 * suscripción al canal (store.onTelemetry), no por el paint de snapshots.
 *
 * Sin suscripción (tests, panel sin página) queda en reposo: barras al mínimo.
 * La señal se acota con signo y NaN tratado como silencio, igual que el resto
 * de consumidores del canal.
 */

const BAR_COUNT = 64;
const REST_HEIGHT = 4; // % — la barra nunca desaparece del todo (guía visual)

export function createSpectral({ onFrame = null } = {}) {
  const element = document.createElement('div');
  element.className = 'spectral';
  element.dataset.visual = 'spectral';
  element.setAttribute('aria-label', 'Espectral de parciales en vivo');
  element.setAttribute('role', 'img');

  const bars = [];

  for (let index = 0; index < BAR_COUNT; index += 1) {
    const bar = document.createElement('span');

    bar.className = 'spectral__bar';
    bar.style.height = `${REST_HEIGHT}%`;
    element.append(bar);
    bars.push(bar);
  }

  let unsubscribe = null;

  function handleFrame(frame) {
    const spectral = Array.isArray(frame?.spectral) ? frame.spectral : [];
    let anyAboveRest = false;

    for (let index = 0; index < BAR_COUNT; index += 1) {
      const value = Number(spectral[index]);

      // NaN/undefined = sin dato esta frame: silencio, no veneno.
      const safe = Number.isFinite(value) ? Math.min(1, Math.max(0, value)) : 0;

      bars[index].style.height = `${Math.round(REST_HEIGHT + safe * (100 - REST_HEIGHT))}%`;

      if (safe > 0.02) anyAboveRest = true;
    }

    element.dataset.active = String(anyAboveRest);
  }

  if (typeof onFrame === 'function') unsubscribe = onFrame(handleFrame);

  return {
    element,

    /** Contrato de vista: los snapshots no mueven las barras (canal aparte). */
    paint() {},

    destroy() {
      unsubscribe?.();
      unsubscribe = null;
      element.remove();
    },
  };
}
