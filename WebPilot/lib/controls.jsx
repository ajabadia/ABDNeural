'use client';

/**
 * React wrappers over the shared control family (ABDSharedAssets).
 *
 * The shared controls are framework-agnostic classes with imperative DOM
 * (mount -> callbacks -> destroy); these wrappers own the React side of the
 * deal:
 *
 *   - mount the control into a host div exactly once (StrictMode-safe);
 *   - bridge React props -> control.setValue (programmatic, no onChange echo);
 *   - bridge control.onChange/onDragStart/onDragEnd -> callbacks (stable refs);
 *   - destroy on unmount (the shared control removes its DOM and listeners).
 *
 * The controls know nothing about JUCE, the bridge or the contract; these
 * wrappers + lib/paramValue.js do that mapping. `ParamChoice` renders a native
 * select (discrete parameters have no reason to drag).
 */

import { useEffect, useRef } from 'react';

import { Knob, Slider, Toggle } from '@abdsynths/shared/components';

/**
 * Mount one shared control imperatively and keep it in sync with React.
 *
 * @param {Function} ControlClass  shared control class (Knob, Slider, Toggle).
 * @param {object}   props
 *   domId         id for the host element.
 *   value         normalised 0..1 (or boolean for Toggle).
 *   onChange      (normalised) => void — user edits only.
 *   onGesture     (phase: 'begin'|'end') => void — drag lifecycle.
 *   ...rest       forwarded verbatim to the shared constructor (skin, label...).
 * @returns {object} ref for the host element.
 */
export function useSharedControl(ControlClass, props) {
  const hostRef = useRef(null);
  const controlRef = useRef(null);

  // Latest callbacks without resubscribing on every render.
  const handlersRef = useRef({});
  handlersRef.current = {
    onChange: props.onChange,
    onGesture: props.onGesture,
  };

  const { domId, onChange, onGesture, value, ...controlOptions } = props;

  useEffect(() => {
    const control = new ControlClass(hostRef.current, {
      ...controlOptions,
      value,
      onChange: (next) => handlersRef.current.onChange?.(next),
      onDragStart: () => handlersRef.current.onGesture?.('begin'),
      onDragEnd: () => handlersRef.current.onGesture?.('end'),
    });

    controlRef.current = control;

    return () => {
      control.destroy();
      controlRef.current = null;
    };
    // Mount once per control instance: re-running would rebuild it mid-drag.
    // Prop updates flow through the setValue effect below.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [ControlClass]);

  // Programmatic updates: control.setValue does NOT fire onChange, so
  // React state -> control can never loop back into React state.
  useEffect(() => {
    controlRef.current?.setValue(value);
  }, [value]);

  return hostRef;
}

/**
 * Shared skeleton for the parameter-bound wrappers: resolve the view-model,
 * guard unknown IDs visibly, and translate real-units edits back to the
 * normalised wire value through lib/paramValue.
 */
function ParamControlShell({
  id,
  control,
  value,
  onChange,
  onGesture,
  label,
  render,
}) {
  if (!control)
    return <p className="param-error">unknown parameter "{id}"</p>;

  const labelNode = label ?? control.label;

  return (
    <div className="param-control" data-param-id={id} data-dsp-status={control.dspStatus}>
      {render(labelNode)}
    </div>
  );
}

/** Parameter-bound knob (shared family, contract-driven). */
export function ParamKnob({
  id,
  control,
  value,
  skin = 'vector',
  onChange,
  onGesture,
  size = 64,
  ...knobOptions
}) {
  return (
    <ParamControlShell
      id={id}
      control={control}
      value={value}
      onChange={onChange}
      label={knobOptions.label ?? control?.label}
      render={(label) => (
        <KnobHost
          control={control}
          value={value}
          skin={skin}
          onChange={onChange}
          onGesture={onGesture}
          size={size}
          label={label}
          {...knobOptions}
        />
      )}
    />
  );
}

function KnobHost({ control, value, skin, onChange, onGesture, size, label, ...rest }) {
  const hostRef = useSharedControl(Knob, {
    value,
    skin,
    label,
    size,
    onChange,
    onGesture,
    ...rest,
  });

  return <div className="param-control__widget" ref={hostRef} />;
}

/** Parameter-bound slider (shared family, contract-driven). */
export function ParamSlider({
  id,
  control,
  value,
  skin = 'vector',
  orientation = 'horizontal',
  length = 220,
  onChange,
  onGesture,
  ...sliderOptions
}) {
  return (
    <ParamControlShell
      id={id}
      control={control}
      value={value}
      onChange={onChange}
      label={sliderOptions.label ?? control?.label}
      render={(label) => (
        <SliderHost
          value={value}
          skin={skin}
          orientation={orientation}
          length={length}
          onChange={onChange}
          onGesture={onGesture}
          label={label}
          {...sliderOptions}
        />
      )}
    />
  );
}

function SliderHost({
  value,
  skin,
  orientation,
  length,
  onChange,
  onGesture,
  label,
  ...rest
}) {
  const hostRef = useSharedControl(Slider, {
    value,
    skin,
    orientation,
    length,
    label,
    onChange,
    onGesture,
    ...rest,
  });

  return <div className="param-control__widget" ref={hostRef} />;
}

/**
 * Parameter-bound toggle (bool parameters, latched). The boolean models as
 * 0/1 on the wire; the shell only passes it through.
 */
export function ParamToggle({
  id,
  control,
  value,
  skin = 'vector',
  onChange,
  ...toggleOptions
}) {
  const boolValue = Boolean(value);
  const flip = () => onChange?.(!boolValue, 1 - (boolValue ? 1 : 0));

  return (
    <ParamControlShell
      id={id}
      control={control}
      value={boolValue}
      onChange={onChange}
      label={toggleOptions.label ?? control?.label}
      render={(label) => (
        <ToggleHost
          value={boolValue}
          skin={skin}
          label={label}
          onChange={flip}
          {...toggleOptions}
        />
      )}
    />
  );
}

function ToggleHost({ value, skin, label, onChange, ...rest }) {
  const hostRef = useSharedControl(Toggle, {
    value,
    skin,
    label,
    onChange: (next) => onChange(next),
    ...rest,
  });

  return <div className="param-control__widget" ref={hostRef} />;
}

/**
 * Choice parameters: a native select is the honest control for a discrete
 * value (no dragging, keyboard/screen-reader friendly). Index N lives on the
 * wire as N/(count-1) — the APVTS encoding for discrete parameters.
 */
export function ParamChoice({ id, control, value, onChange }) {
  if (!control)
    return <p className="param-error">unknown parameter "{id}"</p>;

  const count = Math.max(control.options.length, 1);
  const index = Math.round(value * (count - 1));

  function select(event) {
    const nextIndex = Number(event.target.value);

    onChange?.(nextIndex / (count - 1), control.options[nextIndex]);
  }

  return (
    <div className="param-control" data-param-id={id} data-dsp-status={control.dspStatus}>
      <label className="param-control__label">
        <span>{control.label}</span>
        <select value={index} onChange={select}>
          {control.options.map((option, optionIndex) => (
            <option key={option} value={optionIndex}>
              {option}
            </option>
          ))}
        </select>
      </label>
    </div>
  );
}
