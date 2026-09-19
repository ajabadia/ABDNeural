'use client';

import { useEffect, useMemo, useRef, useState } from 'react';

// Shared keyboard (ABDSharedCode/MidiKeyboard, single source — no local fork).
// The CSS imports are side-effectful on purpose (Next.js injects them globally).
import { createKeyboard } from '@abdsynths/midi-keyb';
import '@abdsynths/midi-keyb/keyboard.css';

import { ParamChoice, ParamKnob, ParamSlider, ParamToggle } from '../lib/controls.jsx';
import { displayText, realFromNormalized } from '../lib/paramValue.js';
import { useParameterControls } from '../lib/useParameterControls.js';
import {
  PILOT_PARAMETER_IDS,
  UNROUTED_PARAMETERS,
  contractSummary,
  describePilotControls,
  getDescriptor,
} from '../lib/parameters.js';
import { engineIndexFromNormalized } from '../lib/audioParams.js';
import {
  isAudioEngineReady,
  onAudioEngineChange,
  panicWorklet,
  pushEngineToWorklet,
  pushMidiToWorklet,
  pushModelsToWorklet,
  pushParamsToWorklet,
  startAudioEngine,
} from '../lib/audioWorkletEngine.js';

// Bound at module scope: these come from the generated C++ contract, not from
// hand written constants, so a range change in the plugin cannot be missed here.
const controls = describePilotControls();
const summary = contractSummary();
const contractLine =
  `${summary.total} parameters · ${summary.implemented} wired to the DSP · ` +
  `${summary.uiOnly} UI only · ${summary.notRouted} not routed` +
  (UNROUTED_PARAMETERS.length > 0 ? ` · ${UNROUTED_PARAMETERS.length} not in the layout` : '');

/**
 * The baseline parameter keeps a native range input ON PURPOSE: the host's
 * --selftest drives `document.querySelector('input[type=range]')` and that
 * must be masterLevel. Everything else renders through the shared family.
 */
const BASELINE_PARAMETER_ID = 'masterLevel';

/** Phase 7: the GENERAL screen mirrors the plugin's own GENERAL tab (the
 *  ParameterPanel the host shows natively below the page). IDs straight from
 *  Source/UI/ParameterPanel.cpp; descriptors straight from the contract. */
const GENERAL_PARAMETER_IDS = [
  'engineType',
  'envAttack', 'envDecay', 'envSustain', 'envRelease',
  'unisonDetune', 'unisonSpread',
  'randomStrength',
  'freezeResonator', 'freezeFilter', 'freezeEnvelopes',
];

/** One hook owns BOTH tabs' state, so a preset load or a native edit is
 *  reflected wherever you are looking. PILOT ids stay first: the bridge tab
 *  keeps the declaration order it always had. */
const SCREEN_PARAMETER_IDS = [...PILOT_PARAMETER_IDS, ...GENERAL_PARAMETER_IDS];

/** Tabs live INSIDE one page on purpose: the embedded snapshot serves by
 *  basename, so a second route would collide with index.html (see the 404
 *  snapshot bug in the HANDOFF). */
const TABS = [
  { id: 'bridge', label: 'BRIDGE' },
  { id: 'general', label: 'GENERAL' },
  { id: 'keys', label: 'KEYS', 'data-tab': 'keys' },
];

function displayValue(control, realValue) {
  // A unit-less 0..1 range reads better as a percentage in the pilot UI.
  if (control.kind === 'float' && control.min === 0 && control.max === 1)
    return `${Math.round(realValue * 100)}%`;

  if (control.kind === 'float')
    return `${realValue.toFixed(2)}${control.unit ? ` ${control.unit}` : ''}`;

  return realValue.toFixed(2);
}

/** Envelope shape from the four REAL (denormalised) values. Pure SVG, no
 *  assets: attack/decay/release share the width by time proportion (with a
 *  floor so ultra-fast values stay visible) and sustain gets a fixed width
 *  because it is a level, not a time. */
function AdsrGraph({ attack, decay, sustain, release }) {
  const total = Math.max(attack + decay + release, 1e-3);
  const attackWidth = Math.max((attack / total) * 72, 5);
  const decayWidth = Math.max((decay / total) * 72, 5);
  const sustainWidth = 18;
  const releaseWidth = Math.max(100 - attackWidth - decayWidth - sustainWidth, 5);

  const y = (level) => 100 - level * 100;
  const peak = attackWidth;
  const sustainEnd = attackWidth + decayWidth + sustainWidth;

  const fill =
    `0,100 ${peak},${y(1)} ${peak + decayWidth},${y(sustain)} ` +
    `${sustainEnd},${y(sustain)} 100,100`;
  const line = `M 0 100 L ${peak} ${y(1)} L ${peak + decayWidth} ${y(sustain)}` +
    ` L ${sustainEnd} ${y(sustain)} L 100 100`;

  return (
    <svg className="adsr" viewBox="0 0 100 100" preserveAspectRatio="none" role="img"
      aria-label={`Envelope: attack ${attack.toFixed(2)}s, sustain ${Math.round(sustain * 100)}%, release ${release.toFixed(2)}s`}>
      <polygon className="adsr__fill" points={fill} />
      <path className="adsr__line" d={line} vectorEffect="non-scaling-stroke" />
    </svg>
  );
}

/**
 * KEYS tab: the SHARED virtual keyboard (@abdsynths/midi-keyb, the same
 * component ABDMS2000 mounts) + the plugin's pitch/mod wheels. Key/wheel
 * input goes out through the bridge MIDI messages; midiNoteState coming back
 * is applied by the host-driven feedback API (no user-input echo).
 */
function KeysTab({ midiState, bridgeAvailable, onNoteOn, onNoteOff, onPitchBend, onModWheel, onPanic }) {
  const keyboardRef = useRef(null);
  const callbacksRef = useRef({});
  const [hasKeybed, setHasKeybed] = useState(false);

  // Latest callbacks without re-creating the keyboard.
  callbacksRef.current = { onNoteOn, onNoteOff, onPitchBend, onModWheel, onPanic };

  useEffect(() => {
    const root = document.getElementById('keys-root');
    if (!root) return undefined;

    root.innerHTML =
      '<div class="keys-strip">'
      + '<div id="pitch-wheel-container"></div>'
      + '<div id="piano-keyboard"></div>'
      + '<div id="mod-wheel-container"></div>'
      + '</div>'
      + '<div class="keys-note">QWERTY plays (A W S E D…), Z/X shifts octave, Space = panic.'
      + (bridgeAvailable ? '' : ' — LOCAL MODE: notes stay on this page.')
      + '</div>';

    const callbacks = callbacksRef.current;
    const keyboard = createKeyboard({
      containerId: 'piano-keyboard',
      wheelPitchId: 'pitch-wheel-container',
      wheelModId: 'mod-wheel-container',
      panicBtnId: null,
      onNoteOn: (note, velocity) => callbacksRef.current.onNoteOn?.(note, velocity),
      onNoteOff: (note) => callbacksRef.current.onNoteOff?.(note),
      onPitchBend: (value) => callbacksRef.current.onPitchBend?.(value),
      onModWheel: (value) => callbacksRef.current.onModWheel?.(value),
      onPanic: () => callbacksRef.current.onPanic?.(),
    });
    keyboardRef.current = keyboard;
    setHasKeybed(document.querySelector('#piano-keyboard .kbd-white-key') !== null);

    return () => {
      keyboard.destroy();
      keyboardRef.current = null;
      root.innerHTML = '';
    };
    // The keyboard is created ONCE per mount of the tab: callbacks ride refs,
    // and bridgeAvailable only matters at mount time (local vs live label).
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Host-driven feedback: the plugin's external MIDI view mirrors on the
  // wheels. Wheel setters are the SILENT API (v0.2): they move the filmstrip
  // and the readout without echoing back as user input. Held notes are not
  // re-highlighted on purpose (the shared keyboard keeps its own press state;
  // a repaint from telemetry would fight the user's finger).
  useEffect(() => {
    if (!hasKeybed) return;

    keyboardRef.current?.setPitchBend?.(midiState.pitchBend ?? 0);
    keyboardRef.current?.setModWheel?.(midiState.modWheel ?? 0);
  }, [midiState, hasKeybed]);

  // Full release path for the page's own panic button below.
  function handlePanicClick() {
    keyboardRef.current?.panic?.();
    onPanic?.();
  }

  return (
    <div className="tab-group keys-group">
      <div className="keys-toolbar">
        <button type="button" className="keys-panic" onClick={handlePanicClick}>PANIC</button>
        <span className={`keys-status${bridgeAvailable ? '' : ' keys-status--local'}`}>
          {bridgeAvailable ? 'MIDI → PLUGIN LIVE' : 'LOCAL MODE'}
        </span>
      </div>
      <div id="keys-root" />
    </div>
  );
}

/**
 * Preset bar over the bridge's preset messages (protocol v1, additive):
 * the select loads, the input + SAVE stores the current state. The host
 * answers with presetList (which refreshes both) or presetError (shown).
 * In local mode the hook's senders are no-ops and the bar stays disabled.
 */
function PresetBar({ presetState, presetError, bridgeAvailable, onLoad, onSave }) {
  const [nameInput, setNameInput] = useState('');
  const presets = presetState.presets;
  const currentKnown = presets.includes(presetState.current);

  function submitSave(event) {
    event.preventDefault();
    const name = nameInput.trim();
    if (!name) return;
    onSave(name);
    setNameInput('');
  }

  return (
    <div className="preset-block">
      <form className="preset-bar" onSubmit={submitSave}>
        <select
          aria-label="Preset"
          value={currentKnown ? presetState.current : ''}
          disabled={!bridgeAvailable || presets.length === 0}
          onChange={(event) => {
            if (event.target.value) onLoad(event.target.value);
          }}
        >
          {!currentKnown ? (
            <option value="" disabled hidden>
              {presets.length === 0 ? 'No presets yet' : 'Preset…'}
            </option>
          ) : null}
          {presets.map((name) => (
            <option key={name} value={name}>{name}</option>
          ))}
        </select>

        <input
          aria-label="New preset name"
          placeholder="New preset name"
          value={nameInput}
          maxLength={100}
          disabled={!bridgeAvailable}
          onChange={(event) => setNameInput(event.target.value)}
        />
        <button type="submit" disabled={!bridgeAvailable || nameInput.trim() === ''}>
          SAVE
        </button>
      </form>

      {presetError ? (
        <p className="preset-error" role="alert">
          preset {presetError.operation}: {presetError.detail}
        </p>
      ) : null}
    </div>
  );
}

export default function HomePage() {
  const [activeTab, setActiveTab] = useState('bridge');

  // ---- Web audio path (AudioWorklet + WASM DSP real, Fase 5) ----------------
  // El navegador exige un gesto de usuario para arrancar audio: el botón
  // SOUND ON es la puerta; el estado llega por onAudioEngineChange.
  const [audio, setAudio] = useState({ status: 'idle', error: null, sampleRate: 0 });

  useEffect(() => {
    let active = true;

    onAudioEngineChange((state) => {
      if (active)
        setAudio({ status: state.status, error: state.error, sampleRate: state.sampleRate });
    });

    return () => { active = false; };
  }, []);

  /**
   * Audio policy (ROADMAP Fase 8 / ticket 8.1): inside a JUCE host the PLUGIN
   * owns the audio and the page is only a remote control for it. Starting the
   * worklet here would leave two engines over the same parameters, which is not
   * a supported case (doubled voices, phase-y FX). The vanilla mirror of this
   * rule lives in WebUI/src/audio/policy.js; this guard dies with the pilot.
   */
  async function handleStartSound(insideHost) {
    if (insideHost) return;

    await startAudioEngine();
  }

  function audioControl(insideHost) {
    if (insideHost)
      return (
        <span
          className="status"
          title="Lo que suena es el motor del plugin; la página no arranca el worklet."
        >
          AUDIO: NATIVO
        </span>
      );

    if (audio.status === 'ready')
      return (
        <span className="status">
          AUDIO ON · {(audio.sampleRate / 1000).toFixed(1)} kHz
        </span>
      );

    if (audio.status === 'loading')
      return <span className="status">AUDIO…</span>;

    return (
      <button
        type="button"
        className="keys-panic"
        onClick={() => handleStartSound(insideHost)}
        title={audio.error ?? 'Arranca el motor DSP real en el navegador (AudioWorklet)'}
      >
        {audio.status === 'error' ? 'AUDIO ERROR — REINTENTAR' : 'SOUND ON'}
      </button>
    );
  }

  const {
    controls: hookControls,
    parameters,
    changeCount,
    bridgeAvailable,
    snapshotVersion,
    contractErrors,
    summary: hookSummary,
    presetState,
    presetError,
    midiState,
    models,
    pushParameter,
    handleChange,
    handleGesture,
    loadPreset,
    savePreset,
    sendMidiNoteOn,
    sendMidiNoteOff,
    sendMidiPitchBend,
    sendMidiModWheel,
    sendMidiPanic,
  } = useParameterControls(SCREEN_PARAMETER_IDS);

  // Sync completo al worklet en cada cambio de estado: cubre ediciones de la
  // página Y snapshots nativos (bridge) con un único camino, sin wrappers por
  // control. El motor va aparte porque reconstruye el engine (caro): sólo
  // cuando cambia su índice.
  const lastEngineIndexRef = useRef(-1);

  // Latest spectral models (bridge modelsState) for the worklet's initial push.
  const modelsRef = useRef(null);

  useEffect(() => {
    if (audio.status !== 'ready') return;

    const engineIndex =
      engineIndexFromNormalized (parameters.engineType ?? 0, getDescriptor ('engineType'));

    if (engineIndex !== lastEngineIndexRef.current) {
      lastEngineIndexRef.current = engineIndex;
      pushEngineToWorklet (engineIndex);

      // Models hang off the concrete engine: the switch rebuilt it, so every
      // slot reset to defaults until the page re-applies what the bridge sent.
      if (modelsRef.current)
        pushModelsToWorklet (modelsRef.current);
    }

    pushParamsToWorklet (parameters);
  }, [parameters, audio.status]);

  useEffect(() => {
    if (audio.status !== 'ready' || !models) return;

    // First delivery after SOUND ON: the params effect above already ran, so
    // nothing else would push the models until the next preset load.
    pushModelsToWorklet (models);
  }, [models, audio.status]);

  const bridgeControls = useMemo(
    () => hookControls.filter((control) => PILOT_PARAMETER_IDS.includes(control.id)),
    [hookControls],
  );

  const generalControls = useMemo(
    () => hookControls.filter((control) => GENERAL_PARAMETER_IDS.includes(control.id)),
    [hookControls],
  );

  const byId = useMemo(() => {
    const map = {};
    for (const control of generalControls) map[control.id] = control;
    return map;
  }, [generalControls]);

  function heading(control, normalized) {
    const realValue = realFromNormalized(control, normalized);
    const divergent = control.dspStatus !== 'implemented';

    return (
      <span className="control-heading">
        <span title={control.dspNote || `${control.engines} engine`}>
          {control.label}
          {divergent ? ' *' : ''}
        </span>
        <strong>{displayValue(control, realValue)}</strong>
      </span>
    );
  }

  function renderControl(control) {
    const normalized = parameters[control.id];

    if (control.id === BASELINE_PARAMETER_ID) {
      const realValue = realFromNormalized(control, normalized);

      return (
        <label className="control" key={control.id}>
          {heading(control, normalized)}
          <input
            type="range"
            min={control.min}
            max={control.max}
            step={control.step}
            value={realValue}
            onPointerDown={() => handleGesture(control.id, 'begin')}
            onPointerUp={() => handleGesture(control.id, 'end')}
            onChange={(event) =>
              pushParameter(control.id, Number(event.target.value), 'change')}
          />
        </label>
      );
    }

    if (control.kind === 'choice') {
      return (
        <div className="control" key={control.id}>
          {heading(control, normalized)}
          <ParamChoice
            id={control.id}
            control={control}
            value={normalized}
            onChange={(normalizedValue) => handleChange(control.id, normalizedValue)}
          />
        </div>
      );
    }

    return (
      <div className="control" key={control.id}>
        {heading(control, normalized)}
        <ParamSlider
          id={control.id}
          control={control}
          value={normalized}
          onChange={(normalizedValue) => handleChange(control.id, normalizedValue)}
          onGesture={(phase) => handleGesture(control.id, phase)}
        />
      </div>
    );
  }

  /** GENERAL tab: the family paints its own label, so there is no duplicate
   *  heading — just the wrapper, plus a real-units readout for precision. */
  function renderGeneralControl(control) {
    const normalized = parameters[control.id];
    const realValue = realFromNormalized(control, normalized);
    const divergent = control.dspStatus !== 'implemented';

    const shell = (children) => (
      <div className="control" key={control.id}>
        {children}
        <span className="control-value" title={control.dspNote || `${control.engines} engine`}>
          {displayValue(control, realValue)}{divergent ? ' *' : ''}
        </span>
      </div>
    );

    if (control.kind === 'choice') {
      return shell(
        <ParamChoice
          id={control.id}
          control={control}
          value={normalized}
          onChange={(normalizedValue) => handleChange(control.id, normalizedValue)}
        />,
      );
    }

    if (control.kind === 'bool') {
      return shell(
        <ParamToggle
          id={control.id}
          control={control}
          value={normalized}
          onChange={(normalizedValue) => handleChange(control.id, normalizedValue)}
        />,
      );
    }

    return shell(
      <ParamKnob
        id={control.id}
        control={control}
        value={normalized}
        size={56}
        onChange={(normalizedValue) => handleChange(control.id, normalizedValue)}
        onGesture={(phase) => handleGesture(control.id, phase)}
      />,
    );
  }

  const divergentCount = generalControls.filter(
    (control) => control.dspStatus !== 'implemented').length;

  return (
    <main className="page-shell">
      <section className="panel">
        <header className="panel-header">
          <div>
            <p className="eyebrow">NEURONiK / WEB PILOT</p>
            <h1>Parameter bridge</h1>
          </div>
          <span className="status">{bridgeAvailable ? 'BRIDGE LIVE' : 'LOCAL MODE'}</span>
          {audioControl(bridgeAvailable)}
        </header>

        <p className="intro">
          {bridgeAvailable
            ? 'Connected to the JUCE host over the WebView2 channel. The native panel below binds the same APVTS parameters — move either side and watch both.'
            : 'Next.js screen driven by the generated parameter contract. No JUCE backend detected: state stays local.'}
        </p>

        <p className="contract-line">
          {contractLine}
          {bridgeAvailable ? ` · snapshot #${snapshotVersion}` : ''}
        </p>

        <PresetBar
          presetState={presetState}
          presetError={presetError}
          bridgeAvailable={bridgeAvailable}
          onLoad={loadPreset}
          onSave={savePreset}
        />

        <div className="tabs" role="tablist" aria-label="Screens">
          {TABS.map((tab) => (
            <button
              key={tab.id}
              type="button"
              role="tab"
              aria-selected={activeTab === tab.id}
              data-tab={tab['data-tab'] ?? tab.id}
              className={`tab${activeTab === tab.id ? ' tab--active' : ''}`}
              onClick={() => setActiveTab(tab.id)}
            >
              {tab.label}
            </button>
          ))}
        </div>

        {activeTab === 'bridge' ? (
          <>
            <div className="controls">{bridgeControls.map(renderControl)}</div>

            {bridgeControls.some((control) => control.dspStatus !== 'implemented') ? (
              <p className="contract-warning">
                * parameters marked with an asterisk are present in the plugin but not wired to the
                DSP; see the generated contract for the exact reason.
              </p>
            ) : null}
          </>
        ) : (
          <>
            <div className="tab-group">
              <h2>ENGINE</h2>
              <div className="knob-grid">
                {['engineType', 'freezeResonator', 'freezeFilter', 'freezeEnvelopes']
                  .map((id) => byId[id])
                  .filter(Boolean)
                  .map(renderGeneralControl)}
              </div>
            </div>

            <div className="tab-group">
              <h2>AMPLITUDE ENVELOPE</h2>
              <AdsrGraph
                attack={realFromNormalized(byId.envAttack, parameters.envAttack)}
                decay={realFromNormalized(byId.envDecay, parameters.envDecay)}
                sustain={realFromNormalized(byId.envSustain, parameters.envSustain)}
                release={realFromNormalized(byId.envRelease, parameters.envRelease)}
              />
              <div className="knob-grid">
                {['envAttack', 'envDecay', 'envSustain', 'envRelease']
                  .map((id) => byId[id])
                  .filter(Boolean)
                  .map(renderGeneralControl)}
              </div>
            </div>

            <div className="tab-group">
              <h2>SPECTRAL UNISON</h2>
              <div className="knob-grid">
                {['unisonDetune', 'unisonSpread']
                  .map((id) => byId[id])
                  .filter(Boolean)
                  .map(renderGeneralControl)}
              </div>
            </div>

            <div className="tab-group">
              <h2>RANDOM</h2>
              <p className="tab-note">
                The RANDOM button lives on the native panel; this strength knob shapes how far it
                pushes every parameter.
              </p>
              <div className="knob-grid">
                {['randomStrength'].map((id) => byId[id]).filter(Boolean).map(renderGeneralControl)}
              </div>
            </div>

            {divergentCount > 0 ? (
              <p className="contract-warning">
                * {divergentCount} GENERAL control{divergentCount === 1 ? ' is' : 's are'} UI-only
                (freeze toggles, random strength): they drive panel behaviour, not the DSP directly.
              </p>
            ) : null}
          </>
        )}

        {activeTab === 'keys' ? (
          <KeysTab
            midiState={midiState}
            bridgeAvailable={bridgeAvailable}
            onNoteOn={(note, velocity) => {
              // Dual path: bridge nativo (si vive) + motor WASM local.
              sendMidiNoteOn (note, velocity);
              pushMidiToWorklet ({ kind: 'noteOn', note, velocity });
            }}
            onNoteOff={(note) => {
              sendMidiNoteOff (note);
              pushMidiToWorklet ({ kind: 'noteOff', note });
            }}
            onPitchBend={(value) => {
              sendMidiPitchBend (value);
              pushMidiToWorklet ({ kind: 'pitchBend', value });
            }}
            onModWheel={sendMidiModWheel}
            onPanic={() => {
              // Native side of the page panic: stop EVERY sounding note in the
              // plugin (page, hardware and DAW alike) — midiPanic action.
              sendMidiPanic();
              panicWorklet();
            }}
          />
        ) : null}

        <footer className="panel-footer">
          <span>
            Parameter updates: {changeCount}
            {contractErrors.length > 0 ? ` · contract errors: ${contractErrors.join(', ')}` : ''}
          </span>
          <code>{JSON.stringify(parameters)}</code>
        </footer>
      </section>
    </main>
  );
}
