'use client';

import { useMemo } from 'react';

import { ParamChoice, ParamSlider } from '../lib/controls.jsx';
import { displayText, realFromNormalized } from '../lib/paramValue.js';
import { useParameterControls } from '../lib/useParameterControls.js';
import {
  UNROUTED_PARAMETERS,
  contractSummary,
  describePilotControls,
  getDescriptor,
  validateState,
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

function displayValue(control, realValue) {
  // A unit-less 0..1 range reads better as a percentage in the pilot UI.
  if (control.kind === 'float' && control.min === 0 && control.max === 1)
    return `${Math.round(realValue * 100)}%`;

  return realValue.toFixed(2);
}

export default function HomePage() {
  const {
    controls: hookControls,
    parameters,
    changeCount,
    bridgeAvailable,
    snapshotVersion,
    contractErrors,
    summary: hookSummary,
    pushParameter,
    handleChange,
    handleGesture,
  } = useParameterControls();

  const errors = useMemo(
    () => validateState(controls.map((control) => control.id), parameters),
    [parameters],
  );

  function renderControl(control) {
    const normalized = parameters[control.id];
    const descriptor = getDescriptor(control.id);
    const realValue = realFromNormalized(control, normalized);
    const divergent = control.dspStatus !== 'implemented';

    const heading = (
      <span className="control-heading">
        <span title={control.dspNote || `${control.engines} engine`}>
          {control.label}
          {divergent ? ' *' : ''}
        </span>
        <strong>{displayValue(control, realValue)}</strong>
      </span>
    );

    if (control.id === BASELINE_PARAMETER_ID) {
      return (
        <label className="control" key={control.id}>
          {heading}
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
          {heading}
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
        {heading}
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
            ? 'Connected to the JUCE host over the WebView2 channel. The native strip below binds the same APVTS parameters — move either side and watch both.'
            : 'Next.js screen driven by the generated parameter contract. No JUCE backend detected: state stays local.'}
        </p>

        <p className="contract-line">
          {contractLine}
          {bridgeAvailable ? ` · snapshot #${snapshotVersion}` : ''}
        </p>

        <div className="controls">{controls.map(renderControl)}</div>

        {controls.some((control) => control.dspStatus !== 'implemented') ? (
          <p className="contract-warning">
            * parameters marked with an asterisk are present in the plugin but not wired to the
            DSP; see the generated contract for the exact reason.
          </p>
        ) : null}

        <footer className="panel-footer">
          <span>
            Parameter updates: {changeCount}
            {errors.length > 0 ? ` · contract errors: ${errors.join(', ')}` : ''}
          </span>
          <code>{JSON.stringify(parameters)}</code>
        </footer>
      </section>
    </main>
  );
}
