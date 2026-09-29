/**
 * Pagina de prueba de la AGUJA en MODO NAVEGADOR (motor WASM, sin host).
 *
 * Por que existe: en el PLUGIN la aguja vive del canal bridge
 * (store.onTelemetry -> frame.envelopes) y la cubre la direccion AGUJA del
 * selftest del arnes (BridgeSelftest.h, fase 1-3 + PANIC). En el navegador el
 * MISMO par viaja por el meter del worklet (onWorkletEnvelopeLevels) y hasta
 * hoy no tenia verificacion visual: esta pagina monta las DOS vistas de
 * produccion (curvas del lienzo y bloques del cajon = las cuatro agujas),
 * arranca el motor WASM y las alimenta por el MISMO camino que app.js.
 *
 * Todo lo que usa es PRODUCCION: las vistas del catalogo (envelopeViews),
 * los view-models del contrato (describeControl sobre el generado), el
 * snapshot por defecto del contrato (defaultNormalizedState) y el controlador
 * del motor (audioWorkletEngine). Cero logicas gemelas: si el cable de app.js
 * cambia de canal, esta pagina se queda corta y el E2E lo diria.
 *
 * Lectura para el test: window.__needles() devuelve las cuatro agujas
 * { visible, y, level } con el nivel derivado del 'd' del path con la MISMA
 * escala que el arnes (viewBox 100x48, PAD 2: level = (H - PAD - y)/(H - 2*PAD)).
 * window.__probeReady = true cuando el montaje termino.
 *
 * El boton "ADSR: amp 0.8 / filtro 0.2" empuja el ADSR por el canal de
 * VoiceParams (`neuronik:voice`), el mismo que app.js: las dos envolventes con
 * sustains distintos, para que el E2E pueda exigir que las needles MIDAN
 * valores distintos y no solo que se muevan. Ademas REPINTA las dos vistas con
 * ese mismo estado, que es lo que permite la referencia visual de las agujas
 * SOSTENIDAS (`visual.spec.js`): la aguja tiene que caer en la altura del sustain
 * de SU propia curva.
 */

import {
  onWorkletEnvelopeLevels,
  pushMidiToWorklet,
  pushVoiceToWorklet,
  startAudioEngine,
} from '../src/audio/audioWorkletEngine.js';
import {
  describeControl,
  defaultNormalizedState,
  getDescriptor,
  toNormalized,
} from '../src/contracts/parameters.js';
import { BANDS } from '../src/contracts/sections.js';
import {
  createEnvelopeBlocks,
  createEnvelopeCurves,
} from '../src/ui/envelopeViews.js';
import '../src/styles/main.css';

/** Las fichas del contrato (BANDS es un array de ARRAYS de fichas). */
const sections = BANDS.flat();
const envSection = sections.find((section) => section.id === 'envelopes');
const matrixSection = sections.find((section) => section.id === 'modMatrix');

const status = document.querySelector('#status');

function setStatus(text, state) {
  if (!status) return;
  status.textContent = text;
  status.dataset.status = state;
}

if (!envSection || !matrixSection) {
  setStatus('contrato sin ficha ENVELOPES/modMatrix', 'error');
} else {
  // View-models del contrato (los mismos que el panel y app.js construyen) y
  // snapshot por defecto: las curvas nacen pintadas con los defaults.
  const controls = envSection.ids.map(describeControl).filter(Boolean);
  const routeControls = matrixSection.ids.map(describeControl).filter(Boolean);
  const snapshot = defaultNormalizedState([...envSection.ids, ...matrixSection.ids]);

  // Las DOS vistas: el lienzo (dos curvas) y el cajon (dos bloques con su
  // curva encima). Cuatro agujas en el DOM, las mismas clases que pinta el
  // arnes en el plugin.
  const canvasView = createEnvelopeCurves({ controls, routeControls, onTelemetry: null });
  const drawerView = createEnvelopeBlocks({ controls, ids: envSection.ids, routeControls, onTelemetry: null });

  document.querySelector('#canvas-mount').append(canvasView.element);
  document.querySelector('#drawer-mount').append(drawerView.element);

  canvasView.paint(snapshot);
  drawerView.paint(snapshot);

  // EL FEED, identico al de app.js (mismo orden de vistas, mismo reparto del
  // par envelopes=[amp, filter]): el meter del worklet manda el par, aqui se
  // pinta la aguja de SU curva en las DOS vistas. No hay pushTelemetryFrame
  // porque esta pagina no monta las barras ni el espectral — solo las agujas.
  onWorkletEnvelopeLevels(([amp, filter]) => {
    for (const view of [canvasView, drawerView]) {
      view.needleFor('env')?.setLevel(amp);
      view.needleFor('filter')?.setLevel(filter);
    }
  });

  // Lectura del test: las cuatro agujas con la escala del arnes.
  const readNeedle = (host, prefix) => {
    const column = host.querySelector(`[data-envelope="${prefix}"]`);
    const needle = column?.querySelector('.envelope-curve__level');

    if (!needle) return { visible: false, y: -1, level: 0 };

    const d = needle.getAttribute('d') ?? '';
    const match = d.match(/[Ll]\s*([0-9.]+)\s*,\s*([0-9.]+)/);
    const y = match ? Number(match[2]) : -1;
    const H = 48;
    const pad = 2;
    const visible = needle.dataset.visible === 'true';

    return { visible, y, level: visible && y >= 0 ? (H - pad - y) / (H - pad * 2) : 0 };
  };

  window.__needles = () => ({
    canvasAmp: readNeedle(canvasView.element, 'env'),
    canvasFilter: readNeedle(canvasView.element, 'filter'),
    blocksAmp: readNeedle(drawerView.element, 'env'),
    blocksFilter: readNeedle(drawerView.element, 'filter'),
  });

  // SOUND ON (gesto del usuario: el clic deja resume() el contexto).
  document.querySelector('#start')?.addEventListener('click', async () => {
    setStatus('cargando el motor WASM...', 'loading');
    const state = await startAudioEngine();

    setStatus(state.status === 'ready'
      ? `motor local: ON · ${state.sampleRate} Hz`
      : `motor: ${state.status}${state.error ? ` (${state.error})` : ''}`,
    state.status);

    // El snapshot del contrato al motor recien arrancado, como hace el syncEngine
    // de app.js al recibir el 'ready': sin esto el motor se queda en los
    // defaults de C++ hasta que alguien mueva un knob.
    if (state.status === 'ready') pushVoiceToWorklet(snapshot);
  });

  // La nota, por el MISMO mensaje que el teclado de la pagina manda en app.js.
  document.querySelector('#note-on')?.addEventListener('click', () => {
    pushMidiToWorklet({ kind: 'noteOn', note: 60, velocity: 0.9 });
  });
  document.querySelector('#note-off')?.addEventListener('click', () => {
    pushMidiToWorklet({ kind: 'noteOff', note: 60 });
  });

  // EL ADSR POR SU CANAL (`neuronik:voice`, el mismo pushVoiceToWorklet que
  // llama app.js en cada sync): las dos envolventes con sustains MUY distintos
  // (0.8 la de amp, 0.2 la de filtro) y tiempos de release largos para que las
  // needles de las dos vistas se queden arriba el tiempo suficiente. Es lo que
  // convierte la aguja en una prueba del cable y no solo del dibujo: si el
  // motor no recibiera el ADSR, las dos needles cairian al 0.7 de C++ y a la
  // vez, que es justo lo que el spec ya no admite.
  //
  // El MISMO estado se repinta en las dos vistas antes de empujarlo, para que la
  // foto de referencia muestre la aguja EN LA ALTURA de su propio sustain (y no
  // sobre una curva que sea la del contrato por defecto). Asi la referencia
  // tambien caza un error de PINTADO: si la escala de la aguja o el alto de la
  // curva se mueven, la foto se aparta aunque el motor siga bien.
  const ADSR_STATE = {
    ...snapshot,
    envAttack: toNormalized(getDescriptor('envAttack'), 0.01),
    envDecay: toNormalized(getDescriptor('envDecay'), 0.05),
    envSustain: toNormalized(getDescriptor('envSustain'), 0.8),
    envRelease: toNormalized(getDescriptor('envRelease'), 2.0),
    filterAttack: toNormalized(getDescriptor('filterAttack'), 0.01),
    filterDecay: toNormalized(getDescriptor('filterDecay'), 0.05),
    filterSustain: toNormalized(getDescriptor('filterSustain'), 0.2),
    filterRelease: toNormalized(getDescriptor('filterRelease'), 2.0),
  };

  document.querySelector('#voice-adsr')?.addEventListener('click', () => {
    canvasView.paint(ADSR_STATE);
    drawerView.paint(ADSR_STATE);
    pushVoiceToWorklet(ADSR_STATE);
    setStatus('ADSR pushed: amp 0.8 / filtro 0.2', 'ready');
  });

  window.__probeReady = true;
}
