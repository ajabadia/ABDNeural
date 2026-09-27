/**
 * Contract guard for the WebUI entry point (src/app.js).
 *
 * Two things the host depends on and that are easy to break while moving code
 * around:
 *
 *   1. the ORDER of the boot: the panel must be in the DOM before the store
 *      announces the page, because the host times "panel in DOM" and
 *      "page ready" (`window.__pilotReady`) separately and reports both;
 *   2. the UI stays framework free — that was the whole point of the vanilla
 *      decision (ROADMAP, Fase 8), and it is what keeps the bundle at a
 *      fraction of the React pilot's.
 *
 * The DOM-level contract with the host (first range input = masterLevel, footer
 * <code> as JSON, `data-tab="keys"`) is pinned in tests/panel.test.js, against
 * the real DOM instead of against the source text.
 */

import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { describe, expect, it } from 'vitest';

const here = dirname(fileURLToPath(import.meta.url));
const read = (...segments) => readFileSync(join(here, ...segments), 'utf8');

describe('WebUI entry contract', () => {
  const app = read('../src/app.js');
  const panel = read('../src/ui/panel.js');
  const visuals = read('../src/ui/visuals.js');
  const store = read('../src/contracts/paramStore.js');
  const bridge = read('../src/bridge/bridgeCore.js');
  const html = read('../index.html');

  it('keeps masterLevel as the baseline parameter the host selftest drives', () => {
    expect(app).toContain("BASELINE_PARAMETER_ID = 'masterLevel'");
    // The input itself is a native range input (see buildBaselineControl).
    expect(panel).toContain("slider.type = 'range'");
    expect(panel).toContain('slider.min = \'0\'');
    expect(panel).toContain('slider.max = \'1\'');
  });

  it('mounts the panel BEFORE announcing the page to the host', () => {
    // Anchor on the CALLS (with their semicolon): the module doc mentions
    // `store.start()` in prose, and indexOf would find that first.
    const mounted = app.indexOf('root.append(panel.element);');
    const started = app.indexOf('store.start();');

    expect(mounted).toBeGreaterThan(-1);
    expect(started).toBeGreaterThan(-1);
    expect(mounted).toBeLessThan(started);
  });

  it('the store owns every screen id (the 70 of the canvas)', () => {
    expect(app).toContain('createParameterStore({ ids: SCREEN_PARAMETER_IDS })');
  });

  it('builds the single canvas from the section layout SSOT', () => {
    expect(app).toContain("import { BANDS, CANVAS, SECTION_ACTIONS, SECTION_VISUALS } from './contracts/sections.js'");
    expect(app).toContain('const bands = BANDS.map');
    expect(app).toContain('baselineId: BASELINE_PARAMETER_ID');

    // Y el lienzo de diseño ENCAJA en el viewport del editor: sin el ajuste, una
    // ventana baja corta por abajo el pie y la franja de teclado.
    // ...y el mecanismo vive en el PAQUETE (no copia local): infraestructura
    // de pagina compartida por la suite.
    expect(app).toContain("import { mountFitStage } from '@abdsynths/shared/components';");
    expect(app).toContain("mountFitStage(root, { width: CANVAS.width, height: CANVAS.height })");
  });

  it('pushes cell edits to the store in NORMALISED units, with their gesture', () => {
    expect(app).toContain('onChange: (id, normalized) => store.handleChange(id, normalized)');
    expect(app).toContain('onGesture: (id, phase) => store.handleGesture(id, phase)');
  });

  it('resolves card actions against their own catalogue and routes them to the store', () => {
    // El boton de ficha llega al panel ya resuelto (mismo trato que los controles)
    // y su accion la ejecuta el store, que es quien habla con el host.
    expect(app).toContain('action: section.action ? SECTION_ACTIONS[section.action] ?? null : null');
    expect(app).toContain("if (id === 'randomize') store.randomize();");
  });

  it('las rutas de ENVOLVENTES abren la MATRIZ: opener tardio cableado en app.js', () => {
    // La vista se fabrica antes del panel (orden fijado por el selftest del host);
    // el cable es un setRouteOpener que app.js hace cuando ya tiene el panel.
    expect(app).toContain('canvasEnvCurvesView?.setRouteOpener');
    expect(app).toContain("panel.openDrawerRoute('modMatrix', slot)");
    // El opener del CAJON (IR A LA RUTA) pide la vuelta: cerrar la matriz
    // reabre ENVOLVENTES. La del lienzo, no (el usuario nunca entro alli).
    expect(app).toContain("panel.openDrawerRoute('modMatrix', slot, { returnTo: 'envelopes' })");
    // El metodo del panel existe y es el dueño del gesto (abre + resalta + retorno).
    expect(panel).toContain('function openDrawerRoute(sectionId, slot, { returnTo = null } = {})');
    expect(panel).toContain('openDrawerRoute,');
    expect(visuals).toContain('curves.setRouteOpener(options.onOpenRoute ?? null);');
  });

  it('monta las vistas de ficha (curva ADSR, resumen de la matriz) aparte de las celdas', () => {
    expect(app).toContain("import { createVisual } from './ui/visuals.js'");
    expect(app).toContain('visualSpec.parameterIds.map(describeControl)');
    expect(app).toContain('visual: visualSpec');
    expect(app).toContain('createVisual(visualSpec.id, visualControls, {');

    // La FABRICA vive en su modulo porque la usan la pagina y la suite del panel:
    // cuando estaba escrita dentro del test, el harness montaba una curva ADSR para
    // cualquier vista declarada (y el resumen de la matriz añadio una segunda).
    // DISENO 9.x: ENVOLVENTES es compuesta desde su retiro de la curva única —
    // lienzo (curvas + rutas de matriz) y cajón (bloques con knobs) en un módulo.
    expect(visuals).toContain("if (visualId === 'envelope-curves') {");
    expect(visuals).toContain('onTelemetry: options.onTelemetry ?? null,');
    expect(visuals).toContain("if (visualId === 'envelope-blocks') {");
    expect(visuals).toContain('const blocks = createEnvelopeBlocks({');
    expect(visuals).toContain('routeControls: options.routeControls ?? [],');
    expect(visuals).toContain('blocks.setRouteOpener(options.onOpenRoute ?? null);');
    // El resumen de la matriz: filas botón con opener tardío (gesto de rutas).
    expect(visuals).toContain("if (visualId === 'mod-summary') {");
    expect(visuals).toContain('summary.setRouteOpener(options.onOpenRoute ?? null);');
    // La vista MODELOS es compuesta desde 8.3: pad dibujado + ranuras, montadas
    // por la misma fabrica (un solo punto de comportamiento, dos mitades).
    expect(visuals).toContain("if (visualId === 'model-slots') {");
    expect(visuals).toContain('createModelSlots({ onLoad: options.onLoad ?? null })');
    expect(visuals).toContain('createXyPad({ onEdit: options.onEdit ?? null })');
  });

  it('las ranuras de modelo A–D piden la carga al store: host O fichero local', () => {
    // DOS caminos, una sola puerta (el store): con host el dialogo lo abre el
    // host nativo (`loadModel` via cable); sin host, el input de fichero local
    // (el handler `requestLocalFile` que app.js le entrega al store).
    expect(app).toContain('onLoad: (slot) => store.loadModel(slot, {');
    expect(app).toContain('requestLocalFile: (nextSlot) =>');
    expect(app).toContain('await store.loadLocalModel(file, localModelSlot);');
    expect(app).toContain('store.seedLocalModels(emptyLocalModels());');
    expect(app).toContain('store.setLocalModelReady(true);');
    expect(store).toContain('transport?.sendLoadModel(slot);');
    expect(store).toContain('requestLocalFile(slot);');
    expect(bridge).toContain("emit({ action: 'loadModel', slot });");

    // Y el panel le pasa el ESTADO entero a las vistas: las ranuras no son parametros
    // (`state.models`) y se habilitan segun haya a quien pedirle la carga.
    expect(panel).toContain('for (const visual of visuals) visual.paint(parameters, state);');
  });

  it('la matriz de modulacion se edita en el cajon y no en la rejilla del lienzo', () => {
    // El panel monta las celdas de una ficha de cajon DENTRO del cajon (y el lienzo
    // se queda con el resumen): si alguien devuelve la matriz al lienzo, cae aqui.
    expect(panel).toContain("import { createDrawer } from '@abdsynths/shared/components'");
    expect(panel).toContain('const drawer = section.drawer ? drawerFor(section, context) : null;');
    expect(panel).toContain('(slotOf?.get(control.id) ?? body).append(cell.element);');
    expect(panel).toContain('context.drawers.set(section.id, drawer);');
    // Abrir el cajon es estado de VISTA: no pasa por el store ni por el host.
    expect(panel).toContain('trigger.addEventListener(\'click\', () => drawer.open());');
  });

  it('mounts the shared keyboard with the DUAL MIDI path (bridge + worklet)', () => {
    expect(app).toContain("import { mountKeyboard } from './ui/keyboard.js'");
    expect(app).toContain('store.sendMidiNoteOn(note, velocity)');
    expect(app).toContain('pushMidiToWorklet({ kind: \'noteOn\', note, velocity })');
    expect(app).toContain('store.sendMidiNoteOff(note)');
    expect(app).toContain('pushMidiToWorklet({ kind: \'noteOff\', note })');
    expect(app).toContain('store.sendMidiPitchBend(value)');
    expect(app).toContain('pushMidiToWorklet({ kind: \'pitchBend\', value })');
    expect(app).toContain('onModWheel: store.sendMidiModWheel');
  });

  it('wires the audio policy: owner from the bridge state, worklet behind a button', () => {
    expect(app).toContain("import { audioOwnerFor } from './audio/policy.js'");
    expect(app).toContain('owner = audioOwnerFor(state.bridgeAvailable)');
    expect(app).toContain('panel.paintAudio({ owner, ...engineSnapshot })');
    expect(app).toContain('startAudioEngine()');
  });

  it('syncs state to the worklet through ONE path (page edits and native snapshots)', () => {
    expect(app).toContain('if (!isAudioEngineReady()) return;');
    expect(app).toContain('pushParamsToWorklet(state.parameters)');
    expect(app).toContain('pushEngineToWorklet(index)');
  });

  it('feeds controls normalised values and pushes normalised edits back', () => {
    expect(app).toContain('parameterStore.handleChange(id, Number(slider.value))');
    expect(app).toContain("parameterStore.handleGesture(id, 'begin')");
    expect(app).toContain("parameterStore.handleGesture(id, 'end')");
  });

  it('drives the wheels from the host MIDI view (host-driven feedback)', () => {
    expect(app).toContain('keyboard.setMidiState(state.midiState)');
  });

  it('index.html loads app.js as a module and has no framework root', () => {
    expect(html).toContain('<script type="module" src="./src/app.js"></script>');
    expect(html).not.toContain('react');
  });

  it('is framework free: no React anywhere in the entry point', () => {
    expect(app).not.toMatch(/from\s+['"]react/);
    expect(app).not.toMatch(/createRoot|renderHook|useState/);
    expect(panel).not.toMatch(/from\s+['"]react/);
  });

  it('takes its theming from the shared SSOT, not from local copies', () => {
    expect(app).toContain("'@abdsynths/shared/styles/tokens.css'");
    expect(app).toContain("'@abdsynths/shared/styles/components/widgets.css'");
  });

  it('mounts the shared theme switcher (dark = current, light = suite) in the header', () => {
    // El selector es UNIVERSAL (@abdsynths/shared); los temas son de la suite.
    // Sin persistencia: cada carga empieza en el tema oscuro (selftest/paridad).
    expect(panel).toContain("import { ThemeSwitcher } from '@abdsynths/shared/components'");
    expect(panel).toContain("new ThemeSwitcher(audioRow");
    expect(panel).toContain("{ id: 'dark', label: 'Dark' }");
    expect(panel).toContain("{ id: 'light', label: 'Light' }");
  });

  it('paints the shared tintable background on the page root', () => {
    expect(html).toContain('<body class="abd-theme-bg">');
    expect(app).toContain("'@abdsynths/shared/styles/components/backgrounds.css'");
  });
});
