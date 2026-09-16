/**
 * Adapter between the generated NEURONiK parameter contract and the WebUI.
 *
 * The descriptors in `generated/parameters.generated.js` are emitted from the
 * real APVTS layout by the C++ tool `NEURONiK_ParameterExport`, so IDs, ranges,
 * intervals, skews, defaults and choice lists always match the plugin.
 *
 * This module adds the behaviour a UI needs on top of that raw data:
 *   - lookup by ID;
 *   - clamping and interval snapping;
 *   - 0..1 conversion using JUCE's NormalisableRange math (including skew);
 *   - display formatting;
 *   - default state and contract validation for a set of parameters.
 *
 * Nothing here talks to the audio engine yet: the pilot still keeps its state
 * locally. When the JUCE bridge lands, these are the values it will send.
 */

import {
  CONTRACT_SUMMARY,
  PARAMETERS,
  PARAMETERS_BY_ID,
  UNROUTED_PARAMETER_IDS,
  UNROUTED_PARAMETERS,
} from '../generated/parameters.generated.js';

export {
  CONTRACT_SUMMARY,
  PARAMETERS,
  PARAMETERS_BY_ID,
  UNROUTED_PARAMETER_IDS,
  UNROUTED_PARAMETERS,
};

/** Parameters exercised by the pilot screen. */
export const PILOT_PARAMETER_IDS = ['masterLevel', 'morphX', 'morphY', 'engineType'];

/** Skew comparison tolerance, mirroring JUCE's exactlyEqual() usage. */
const SKEW_EPSILON = 1e-6;

const clamp = (value, low, high) => Math.min(high, Math.max(low, value));
const clamp01 = (value) => clamp(value, 0, 1);

/** @returns {object|null} Descriptor for a parameter ID, or null when unknown. */
export function getDescriptor(id) {
  return PARAMETERS_BY_ID[id] ?? null;
}

/** Clamp a value into its declared range. Non-numeric input falls back to the default. */
export function clampToRange(descriptor, value) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric)) return descriptor.defaultValue;
  return clamp(numeric, descriptor.minValue, descriptor.maxValue);
}

/** Snap a value to the descriptor interval when it declares one. */
export function snapToInterval(descriptor, value) {
  const clamped = clampToRange(descriptor, value);
  if (!descriptor.interval) return clamped;

  const steps = Math.round((clamped - descriptor.minValue) / descriptor.interval);
  return clampToRange(descriptor, descriptor.minValue + steps * descriptor.interval);
}

/** Convert a real value to 0..1 the same way JUCE's NormalisableRange does. */
export function toNormalized(descriptor, value) {
  const { minValue: start, maxValue: end, skew, symmetricSkew } = descriptor;
  if (end === start) return 0;

  const proportion = clamp01((clampToRange(descriptor, value) - start) / (end - start));
  if (Math.abs(skew - 1) < SKEW_EPSILON) return proportion;
  if (!symmetricSkew) return proportion ** skew;

  const distanceFromMiddle = 2 * proportion - 1;
  return (
    (1 + Math.abs(distanceFromMiddle) ** skew * Math.sign(distanceFromMiddle || 1)) / 2
  );
}

/** Convert a 0..1 value back to its real value, mirroring NormalisableRange. */
export function fromNormalized(descriptor, normalized) {
  const { minValue: start, maxValue: end, skew, symmetricSkew } = descriptor;
  let proportion = clamp01(Number.isFinite(Number(normalized)) ? Number(normalized) : 0);

  if (!symmetricSkew) {
    if (Math.abs(skew - 1) >= SKEW_EPSILON && proportion > 0) proportion = Math.exp(Math.log(proportion) / skew);
    return start + (end - start) * proportion;
  }

  let distanceFromMiddle = 2 * proportion - 1;
  if (Math.abs(skew - 1) >= SKEW_EPSILON && distanceFromMiddle !== 0) {
    distanceFromMiddle = Math.exp(Math.log(Math.abs(distanceFromMiddle)) / skew) * Math.sign(distanceFromMiddle);
  }

  return start + ((end - start) / 2) * (1 + distanceFromMiddle);
}

/** Human readable value, using the descriptor's own metadata. */
export function formatValue(descriptor, value) {
  if (descriptor.kind === 'choice') {
    const index = clamp(Math.round(value), 0, Math.max(descriptor.choices.length - 1, 0));
    return descriptor.choices[index] ?? `#${value}`;
  }

  if (descriptor.kind === 'bool') return value >= 0.5 ? 'On' : 'Off';

  const magnitude = Math.max(Math.abs(descriptor.minValue), Math.abs(descriptor.maxValue));
  const decimals = magnitude >= 100 ? 0 : magnitude >= 10 ? 1 : 2;
  const text = snapToInterval(descriptor, value).toFixed(decimals);

  return descriptor.unit ? `${text} ${descriptor.unit}` : text;
}

/**
 * Everything a generic control needs, straight from the contract.
 * Returns null when the ID is unknown so callers can report it instead of
 * rendering a control with invented bounds.
 */
export function describeControl(id) {
  const descriptor = getDescriptor(id);
  if (!descriptor) return null;

  return {
    id: descriptor.id,
    label: descriptor.name,
    group: descriptor.group,
    kind: descriptor.kind,
    dspStatus: descriptor.dspStatus,
    engines: descriptor.engines,
    dspNote: descriptor.dspNote,
    min: descriptor.minValue,
    max: descriptor.maxValue,
    step: descriptor.interval || (descriptor.maxValue - descriptor.minValue) / 1000,
    skew: descriptor.skew,
    symmetricSkew: descriptor.symmetricSkew,
    unit: descriptor.unit,
    options: descriptor.kind === 'float' ? [] : descriptor.choices,
    defaultValue: descriptor.kind === 'float' ? descriptor.defaultValue : descriptor.defaultChoiceIndex,
  };
}

/** Descriptors for the pilot screen, in declaration order. */
export function describePilotControls(ids = PILOT_PARAMETER_IDS) {
  return ids.map(describeControl).filter(Boolean);
}

/** Default state for a set of parameters, taken from the contract. */
export function defaultState(ids = PILOT_PARAMETER_IDS) {
  const state = {};

  for (const id of ids) {
    const descriptor = getDescriptor(id);
    if (!descriptor) continue;

    state[id] =
      descriptor.kind === 'float' ? descriptor.defaultValue : descriptor.defaultChoiceIndex;
  }

  return state;
}

/**
 * Check that the parameters a screen depends on exist and that a state holds
 * valid values for them. Used by the pilot footer to make contract problems
 * visible instead of silently rendering wrong bounds.
 */
/**
 * Parameters whose behaviour does not match what the UI implies, straight from
 * the contract: `notRouted` never reaches the DSP and `uiOnly` only drives a
 * panel action. A migrated screen can use this to warn the user instead of
 * offering a control that silently does nothing.
 */
export function divergentParameters() {
  return PARAMETERS.filter((descriptor) => descriptor.dspStatus !== 'implemented');
}

/** Contract health plus the status breakdown of a specific screen. */
export function contractSummary(ids = PILOT_PARAMETER_IDS) {
  const controls = describePilotControls(ids);

  return {
    ...CONTRACT_SUMMARY,
    screen: {
      total: controls.length,
      implemented: controls.filter((control) => control.dspStatus === 'implemented').length,
      divergent: controls.filter((control) => control.dspStatus !== 'implemented').length,
    },
  };
}

export function validateState(ids, state) {
  const errors = [];

  for (const id of ids) {
    const descriptor = getDescriptor(id);

    if (!descriptor) {
      errors.push(`unknown parameter "${id}"`);
      continue;
    }

    const value = state[id];

    if (value === undefined) {
      errors.push(`missing value for "${id}"`);
      continue;
    }

    if (descriptor.kind === 'float') {
      if (value < descriptor.minValue || value > descriptor.maxValue)
        errors.push(`"${id}" out of range: ${value}`);
    } else if (!Number.isInteger(value) || value < 0 || value >= descriptor.choices.length) {
      errors.push(`"${id}" invalid choice index: ${value}`);
    }
  }

  return errors;
}
