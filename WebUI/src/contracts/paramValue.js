/**
 * Value plumbing for parameter-bound controls, on top of the generated contract.
 *
 * PORTADO de `WebPilot/lib/paramValue.js` SIN cambios de comportamiento; sólo
 * cambia la ruta del import (el adaptador vive ahora en ./parameters.js).
 *
 * The wire (ParameterBridge) and the store carry NORMALISED 0..1 values;
 * controls display and edit REAL units. These helpers keep every wrapper honest:
 *
 *   - props in:  normalised (state/bridge) -> real (what the control displays);
 *   - edits out: real (control change) -> normalised (what the wire carries);
 *   - conversions reuse the host's NormalisableRange math (lib/parameters.js)
 *     and snap to the declared interval, so a wrapper can never emit a value
 *     the plugin would reject.
 */

import {
  describeControl,
  formatValue,
  fromNormalized,
  snapToInterval,
  toNormalized,
} from './parameters.js';

/**
 * View-model for a parameter-bound control, straight from the contract.
 * Returns null for unknown IDs so callers can render an explicit error instead
 * of a control with invented bounds.
 */
export function describeParam(id) {
  return describeControl(id);
}

/** Normalised (wire/state) -> real (control display/edit space). */
export function realFromNormalized(control, normalized) {
  return fromNormalized(descriptorOf(control), normalized);
}

/** Real (control edit) -> normalised, clamped to the range and interval-snapped. */
export function normalizedFromReal(control, realValue) {
  const descriptor = descriptorOf(control);

  return toNormalized(descriptor, snapToInterval(descriptor, realValue));
}

/** Display text for the readout next to the label. */
export function displayText(control, realValue) {
  // A unit-less 0..1 range reads better as a percentage in the UI.
  if (control.kind === 'float' && control.min === 0 && control.max === 1)
    return `${Math.round(realValue * 100)}%`;

  return formatValue(descriptorOf(control), realValue);
}

/**
 * A `choice` control resolves its current option index from the normalised
 * value: index N maps linearly to N/(count-1), the same encoding the APVTS
 * uses for discrete parameters.
 */
export function choiceIndexFromNormalized(control, normalized) {
  const count = Math.max(control.options.length, 1);

  return clampInt(Math.round(normalized * (count - 1)), 0, count - 1);
}

/** The normalised value of a `choice` index. */
export function normalizedFromChoiceIndex(control, index) {
  const count = Math.max(control.options.length, 1);

  return clampInt(index, 0, count - 1) / (count - 1);
}

/**
 * Descriptor behind a control view-model. describeControl() view-models carry
 * the bounds themselves (min/max/step/skew...), so rebuild a NormalisableRange-
 * shaped descriptor from them; that keeps this module decoupled from the raw
 * generated map while using exactly the same math.
 */
function descriptorOf(control) {
  return {
    id: control.id,
    minValue: control.min ?? 0,
    maxValue: control.max ?? 1,
    interval: control.step || 0,
    skew: control.skew ?? 1,
    symmetricSkew: Boolean(control.symmetricSkew),
    defaultValue: control.defaultValue ?? (control.min ?? 0),
    kind: control.kind ?? 'float',
    choices: control.options ?? [],
    unit: control.unit ?? '',
  };
}

function clampInt(value, low, high) {
  return Math.min(high, Math.max(low, value));
}
