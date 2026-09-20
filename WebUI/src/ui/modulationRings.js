/**
 * Anillos de modulación (telemetryFrame → Knob.setModulation).
 * ============================================================
 *
 * El frame de telemetría trae `modulation[t]`: la contribución CON SIGNO que
 * la matriz acumula sobre el destino t (source×amount por bloque de audio, ver
 * NeuronikEngine::processBlock). El índice t es el MISMO orden que la tabla de
 * destinos del preset — y esa tabla ahora viaja en el contrato generado
 * (MOD_DESTINATIONS, emitida por el exportador de descriptores), así que el
 * mapeo índice→parámetro no se copia a mano en dos sitios: se lee del artefacto.
 *
 * La contribución se normaliza contra el RANGO del parámetro destino (misma
 * matemática que el LookAndFeel nativo: (mod - start) / length, con el skew
 * ignorado a propósito: el anillo es guiño visual, no segundo edit). Un destino
 * sin parámetro ("Off"), sin descriptor o sin knob montado se ignora — un frame
 * puede llegar antes de que la ficha exista, y al revés.
 *
 * El fan-out ES el consumo: por frame, exactamente un setModulation por knob
 * afectado; los knobs sin ruta esta frame NO se tocan (su anillo anterior es
 * el último dato real que tuvieron, como el nativo, que no repinta lo quieto).
 */

import {
  MOD_DESTINATIONS,
  PARAMETERS_BY_ID,
} from '../../generated/parameters.generated.js';

/** Normalised range length per destination parameter, resolved once. */
const RANGE_BY_ID = new Map();

for (const destination of MOD_DESTINATIONS) {
  if (destination.parameterId == null) continue;

  const descriptor = PARAMETERS_BY_ID[destination.parameterId];

  if (!descriptor) continue;

  const length = descriptor.maxValue - descriptor.minValue;

  RANGE_BY_ID.set(
    destination.parameterId,
    length > 0 ? length : 1,
  );
}

/** t (route index) -> parameter id the telemetry frame drives, or null. */
export function destinationIdFor(targetIndex) {
  const destination = MOD_DESTINATIONS[targetIndex];

  return destination?.parameterId ?? null;
}

/**
 * @param {Map<string, { setModulation: Function }>} controlsById
 *        knob instances of the page (only knobs with setModulation are used).
 * @returns {{ handleFrame: Function, destroy: Function }}
 *          handleFrame consumes one telemetry frame; destroy clears every ring.
 */
export function createModulationRings(controlsById) {
  const touched = new Set();

  return {
    handleFrame(frame) {
      const modulation = Array.isArray(frame?.modulation) ? frame.modulation : [];

      for (let targetIndex = 0; targetIndex < modulation.length; targetIndex += 1) {
        const parameterId = destinationIdFor(targetIndex);

        if (parameterId == null) continue;

        const control = controlsById.get?.(parameterId);

        if (!control || typeof control.setModulation !== 'function') continue;

        const rangeLength = RANGE_BY_ID.get(parameterId) ?? 1;
        const contribution = Number(modulation[targetIndex]) || 0;

        control.setModulation(contribution / rangeLength);
        touched.add(parameterId);
      }
    },

    /** Rings are paint, not state: a fresh page must not inherit ghosts. */
    destroy() {
      for (const parameterId of touched) {
        controlsById.get?.(parameterId)?.clearModulation?.();
      }

      touched.clear();
    },
  };
}
