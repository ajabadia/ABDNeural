'use client';

import { useMemo, useState } from 'react';

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
    pushParameter,
    handleChange,
    handleGesture,
    loadPreset,
    savePreset,
  } = useParameterControls(SCREEN_PARAMETER_IDS);

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
