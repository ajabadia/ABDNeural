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

import { BANDS } from '../src/contracts/sections.js';

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
    // 2026-09-28: el control base es un KNOB del paquete compartido, no un
    // `<input type=range>`. Lo que el arnés necesita no es el mueble sino el
    // ANCLA (`data-baseline-control`) y el PUENTE (`baselineControl.value`): con
    // el selector viejo ("el primer input[type=range]") el arnes habria pasado a
    // apuntar a la rueda de modulacion del teclado, en silencio.
    expect(panel).toContain('wrapper.dataset.baselineControl = control.id;');
    expect(panel).toContain('wrapper.baselineControl = {');
    // El ancla se escribe en la CELDA (elemento estable), no en el dial del knob.
    expect(panel).toContain('cell--baseline');
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
    expect(visuals).toContain('onLoad: options.onLoad ?? null,');
    // OLVIDAR tambien entra por handler (no por el panel): la vista no guarda
    // estado de la memoria, solo dice a quien se lo pide.
    expect(visuals).toContain('onForget: options.onForget ?? null,');
    // El pad recibe el canal de telemetría: la divergencia página<->nativo
    // de su fila de readout vive del frame.morph (ver ui/xyPad.js). Las
    // esquinas A-D clicables piden abrir el cajón vía un opener que el panel
    // enchufa tarde (app.js), mismo patrón que setRouteOpener.
    expect(visuals).toContain('onEdit: options.onEdit ?? null,');
    expect(visuals).toContain('onTelemetry: options.onTelemetry ?? null,');
    expect(visuals).toContain('onCornerClick: (slot) => cornerOpener?.(slot),');
    expect(visuals).toContain('setCornerOpener(opener) {');
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
    // El import del paquete compartido: `createDrawer` mas `Knob` (el control
    // base es un Knob desde 2026-09-28), asi que se ancla en el uso y no en la
    // forma exacta de la linea de import.
    expect(panel).toContain("from '@abdsynths/shared/components'");
    expect(panel).toContain('createDrawer');
    expect(panel).toContain('const drawer = section.drawer ? drawerFor(section, context) : null;');
    expect(panel).toContain('(slotOf?.get(control.id) ?? body).append(cell.element);');
    expect(panel).toContain('context.drawers.set(section.id, drawer);');
    // Abrir el cajon es estado de VISTA: no pasa por el store ni por el host.
    // El gancho avisa al panel de la apertura por cuenta del usuario (cancela
    // un retorno VOLVER pendiente: ver routeBack en ui/panel.js).
    expect(panel).toContain('context.onDrawerOpenedByUser?.();');
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

  it('en MODO LOCAL el pad arranca con una ruta de la MATRIZ a Morph Z y el motor la recibe', () => {
    // Sin host el motor nace con la matriz del contrato (slot 3 en Off) y el anillo
    // del pad se queda quieto: la ruta sembrada es lo que lo hace bailar.
    expect(store).toContain('function seedLocalMorphZRoute()');
    expect(app).toContain('store.seedLocalMorphZRoute();');
    // Y el cable va DENTRO de la rama local: con host manda el APVTS.
    expect(app.indexOf('store.seedLocalMorphZRoute();'))
      .toBeGreaterThan(app.indexOf('if (!store.getState().bridgeAvailable) {'));

    // SOUND ON arranca el motor DESPUES del primer paint, y syncEngine se corta
    // sin motor: al llegar a ready hay que re-aplicar el estado, o la ruta (y los
    // modelos, y el pad) no llegan nunca al worklet recien arrancado.
    expect(app).toContain("if (engine.status === 'ready' && !wasReady) paint(store.getState());");

    // Y el guard necesita una COPIA: el objeto que llega por el canal es el MISMO
    // `audioEngineState` que se muta en el sitio, asi que guardarlo por referencia
    // deja `wasReady` en true para siempre y el re-sync no dispara NUNCA -el worklet
    // arranca, procesa y no le llega ni un `neuronik:params`-. Lo caza el E2E de
    // navegador (e2e/localMode.spec.js), que es el unico que ve la frontera de verdad.
    expect(app).toContain('engineSnapshot = { ...engine };');
  });

  it('en MODO LOCAL las ranuras de modelo se recuperan de la memoria del navegador', () => {
    // El plugin vuelve a sus ranuras por el PRESET (`modelPath<slot>`); el navegador no
    // tiene preset ni sistema de ficheros, asi que lo unico que sobrevive a un F5 es el
    // localStorage: sin esta memoria cada recarga empezaba con las cuatro EMPTY.
    expect(store).toContain('function restoreLocalModels()');
    expect(app).toContain('store.restoreLocalModels();');

    // Y SOLO en modo local: con host manda modelsState (el APVTS sabe sus rutas).
    expect(store).toContain('if (state.bridgeAvailable) return 0;');

    // El orden del arranque local es el de siempre: el shape de cuatro ranuras
    // primero, la memoria encima, y el input de fichero al final.
    const seeded = app.indexOf('store.seedLocalModels(emptyLocalModels());');
    const restored = app.indexOf('store.restoreLocalModels();');
    const ready = app.indexOf('store.setLocalModelReady(true);');

    expect(seeded).toBeGreaterThan(-1);
    expect(seeded).toBeLessThan(restored);
    expect(restored).toBeLessThan(ready);

    // Dentro de la rama local: con bridge no se toca nada de esto.
    expect(restored).toBeGreaterThan(app.indexOf('if (!store.getState().bridgeAvailable) {'));
  });

  it('syncs state to the worklet through ONE path (page edits and native snapshots)', () => {
    expect(app).toContain('if (!isAudioEngineReady()) return;');
    expect(app).toContain('pushParamsToWorklet(state.parameters)');
    expect(app).toContain('pushEngineToWorklet(index)');
    // El ADSR va en la MISMA funcion y NO dentro de pushParamsToWorklet: son
    // VoiceParams y su canal es `neuronik:voice`. Sin esta linea el motor local
    // seguia con los defaults de C++ y los ocho knobs de envolvente no se oian
    // en el navegador (fue una limitacion de la pagina de needle-probe).
    // Regex y no toContain: una linea COMENTADA con la misma llamada pasaria
    // el `toContain` y dejaria el test sin usefulness.
    expect(app).toMatch(/^\s*pushVoiceToWorklet\(state\.parameters\);$/m);
  });

  it('feeds controls normalised values and pushes normalised edits back', () => {
    // El control base ya no se cablea en app.js (era su propio bloque con el
    // `<input>` nativo): lo construye buildBaselineControl con los handlers del
    // PANEL, el mismo camino que cualquier otro control continuo. Se comprueba el
    // DESTINO del cableado mas que su forma, que es la del paquete compartido.
    // El gesto pasa por `applyValue` ANTES de empujarse: sin eso el puente que
    // lee el arnés se queda en el ultimo `paint` y contradice al dial (lo cazó
    // el smoke E2E del master). Regex y no `toContain`, por lo mismo que arriba:
    // una linea comentada con la llamada pasaria la asercion.
    expect(panel).toMatch(
      /onChange: \(value\) => \{\s*applyValue\(value\);\s*handlers\?\.onChange\?\.\(control\.id, value\);\s*\}/,
    );
    expect(panel).toContain("onDragStart: () => handlers?.onGesture?.(control.id, 'begin')");
    expect(panel).toContain("onDragEnd: () => handlers?.onGesture?.(control.id, 'end')");
    expect(app).not.toContain('function bindBaseline');
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

// EL APAGADO de la pagina. Los canales son estado de MODULO (viven mas alla del
// documento): sin devolverlos, WebView2 —que vuelve a navegar la pagina cada vez
// que se abre el editor— acumula una `paint` vieja por apertura. El `pagehide`
// es el enganche; lo que se protege aqui es que NO se pueda suscribir sin pasar
// por el registro, que es la parte que se rompe en silencio (una suscripcion
// nueva sin envolver compila igual y no hace nada).
describe('el apagado de pagina (el circuito del unsubscribe)', () => {
  const app = read('../src/app.js');

  // Una llamada a un canal: los `onWorklet*` / `onAudioEngineChange` se importan
  // pelados, y del store solo se usan los tres metodos con remover. Las
  // PROPIEDADES (`onTelemetry: store.onTelemetry`) no son llamadas y no cuentan.
  const channelCall = /(?<![\w.])(?:onWorklet[A-Za-z]+|onAudioEngineChange)\s*\(|store\.(?:onUserEdit|onTelemetry|subscribe)\s*\(/g;

  it('se engancha al ciclo de vida de la pagina con pagehide, no beforeunload', () => {
    expect(app).toContain("window.addEventListener('pagehide'");
    // Un SOLO enganche de pagina, y no `beforeunload` (que el navegador puede
    // suprimir y WebView2 no garantiza al navegar). La razon vive en el fuente,
    // asi que aqui se mira el enganche, no la prosa.
    expect(app).not.toContain("addEventListener('beforeunload'");
    expect((app.match(/window\.addEventListener\(/g) ?? []).length).toBe(1);
    // La pagina que vuelve de la CACHE (bfcache) sigue viva: apagarla la dejaria
    // muda al regresar.
    expect(app).toContain('if (event.persisted) return;');
    expect(app).toContain('teardownPage();');
  });

  it('toda suscripcion de la pagina pasa por el registro del apagado', () => {
    const calls = (app.match(channelCall) ?? []).length;
    // Uno menos: la propia declaracion de la funcion.
    const tracked = (app.match(/subscribeForPage\(/g) ?? []).length - 1;

    expect(calls).toBeGreaterThan(0);
    expect(tracked).toBe(calls);
  });

  it('cada canal cierra de verdad: el registro solo guarda funciones', () => {
    expect(app).toContain("if (typeof unsubscribe === 'function') pageSubscriptions.push(unsubscribe);");
    expect(app).toContain('pageSubscriptions.splice(0)');
  });
});

// EL CRITERIO del distintivo vivo, sobre el CONTRATO REAL (no sobre el texto):
// una ficha con cajon cuelga el dato que se mueve, en la cabecera y en el
// cajon. ENVOLVENTES vivia sin él siendo la unica con cajon cuyo numero SI se
// mueve (8/8 -> 4/8 al cambiar de motor, medido el 2026-09-27), y nadie lo
// notaba porque la excepcion no la miraba ningun test: una ficha nueva con
// cajon y sin distintivo habria entrado igual.
describe('el criterio del distintivo vivo (una cuenta, dos destinos)', () => {
  const cards = BANDS.flat();
  const withDrawer = cards.filter((card) => card.drawer);

  it('toda ficha con cajon cuelga distintivo vivo', () => {
    const missing = withDrawer
      .filter((card) => !card.drawer.liveBadge)
      .map((card) => card.id);

    expect(missing).toEqual([]);
    // El numero de fichas con cajon, para que la lista se note al anadir una.
    expect(withDrawer.map((card) => card.id).sort())
    // `fx` entra aqui el 2026-09-29, cuando EFECTOS paso a ser la quinta
    // ficha con cajon (sus cuatro modulos de hueco).
      .toEqual(['envelopes', 'fx', 'globalFull', 'lfo', 'modMatrix', 'models']);
  });

  it('el distintivo declara DE QUE se cuenta, y el chip solo si se pide', () => {
    for (const card of withDrawer) {
      const badge = card.drawer.liveBadge;

      // Modo (`active`/`touched`/`loaded`) o ids de rutas asignadas: lo que sea,
      // pero dicho. Un distintivo sin declare es un literal disfrazado.
      expect(badge.mode ?? badge.ids, card.id).toBeTruthy();
    }

    // El chip del lienzo es opcional a proposito: la MATRIZ lleva su propio
    // resumen y su dato ya esta en la ficha. Lo demas lo pide con `onCard`.
    const withChip = withDrawer
      .filter((card) => card.drawer.liveBadge.onCard)
      .map((card) => card.id)
      .sort();

    expect(withChip).toEqual(['envelopes', 'fx', 'globalFull', 'lfo', 'models']);
  });
});
