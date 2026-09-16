'use client';

import { useEffect, useMemo, useState } from 'react';

import {
  PARAMETERS,
  PILOT_PARAMETER_IDS,
  UNROUTED_PARAMETERS,
  contractSummary,
  defaultState,
  describePilotControls,
  formatValue,
  getDescriptor,
  validateState,
} from '../lib/parameters.js';

// Bound at module scope: these come from the generated C++ contract, not from
// hand written constants, so a range change in the plugin cannot be missed here.
const controls = describePilotControls();
const initialState = defaultState();
const summary = contractSummary();
const contractLine =
  `${summary.total} parameters · ${summary.implemented} wired to the DSP · ` +
  `${summary.uiOnly} UI only · ${summary.notRouted} not routed` +
  (UNROUTED_PARAMETERS.length > 0 ? ` · ${UNROUTED_PARAMETERS.length} not in the layout` : '');

function displayValue(control, value) {
  // A unit-less 0..1 range reads better as a percentage in the pilot UI.
  if (control.kind === 'float' && control.min === 0 && control.max === 1)
    return `${Math.round(value * 100)}%`;

  return formatValue(getDescriptor(control.id), value);
}

export default function HomePage() {
  const [parameters, setParameters] = useState(initialState);
  const [changeCount, setChangeCount] = useState(0);

  const contractErrors = useMemo(
    () => validateState(PILOT_PARAMETER_IDS, parameters),
    [parameters],
  );

  // Marker used by the WebView2 host to time the real startup of the panel.
  useEffect(() => {
    window.__pilotReady = true;

    return () => {
      window.__pilotReady = false;
    };
  }, []);

  function updateParameter(id, nextValue) {
    setParameters((current) => ({ ...current, [id]: nextValue }));
    setChangeCount((current) => current + 1);
  }

  function renderControl(control) {
    const value = parameters[control.id];

    // The contract tells us whether the DSP will react at all.
    const divergent = control.dspStatus !== 'implemented';

    const heading = (
      <span className="control-heading">
        <span title={control.dspNote || `${control.engines} engine`}>
          {control.label}
          {divergent ? ' *' : ''}
        </span>
        <strong>{displayValue(control, value)}</strong>
      </span>
    );

    if (control.kind === 'float') {
      return (
        <label className="control" key={control.id}>
          {heading}
          <input
            type="range"
            min={control.min}
            max={control.max}
            step={control.step}
            value={value}
            onChange={(event) => updateParameter(control.id, Number(event.target.value))}
          />
        </label>
      );
    }

    return (
      <label className="control" key={control.id}>
        {heading}
        <select
          value={value}
          onChange={(event) => updateParameter(control.id, Number(event.target.value))}
        >
          {control.options.map((option, index) => (
            <option key={option} value={index}>
              {option}
            </option>
          ))}
        </select>
      </label>
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
          <span className="status">STATIC EXPORT</span>
        </header>

        <p className="intro">
          Minimal Next.js screen driven by the generated parameter contract. State is still
          local: the JUCE bridge is not connected yet.
        </p>

        <p className="contract-line">{contractLine}</p>

        <div className="controls">{controls.map(renderControl)}</div>

        {controls.some((control) => control.dspStatus !== 'implemented') ? (
          <p className="contract-warning">
            * parameters marked with an asterisk are present in the plugin but not wired to the
            DSP; see the generated contract for the exact reason.
          </p>
        ) : null}

        <footer className="panel-footer">
          <span>
            Local parameter updates: {changeCount}
            {contractErrors.length > 0 ? ` · contract errors: ${contractErrors.join(', ')}` : ''}
          </span>
          <code>{JSON.stringify(parameters)}</code>
        </footer>
      </section>
    </main>
  );
}
