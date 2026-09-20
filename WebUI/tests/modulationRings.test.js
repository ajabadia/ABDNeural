/**
 * Modulation rings consumer: telemetryFrame -> Knob.setModulation.
 *
 * The mapping is NOT hard coded: index t of `frame.modulation` resolves through
 * the GENERATED table (MOD_DESTINATIONS, emitted by the C++ export tool), so
 * these tests pin the contract against the same artifact the runtime reads.
 * Semantics are the native LookAndFeel's: contribution normalised against the
 * destination parameter's range, signed (LFOs sweep backwards).
 */

import { describe, expect, it, vi } from 'vitest';

import { MOD_DESTINATIONS, PARAMETERS_BY_ID } from '../generated/parameters.generated.js';
import { createModulationRings, destinationIdFor } from '../src/ui/modulationRings.js';

function fakeKnob () { return { setModulation: vi.fn(), clearModulation: vi.fn() }; }

/** Known destination with a 0..1 range (oscLevel, index 1 of the table). */
const OSC_LEVEL = 1;

describe('modulationRings / destinationIdFor', () =>
{
    it('resolves the generated table (not a copied list)', () =>
    {
        expect(MOD_DESTINATIONS.length).toBeGreaterThanOrEqual(28);
        expect(destinationIdFor(0)).toBeNull();                       // "Off"
        expect(destinationIdFor(OSC_LEVEL)).toBe('oscLevel');
        expect(destinationIdFor(4)).toBe('morphX');
        expect(destinationIdFor(999)).toBeNull();
    });
});

describe('modulationRings / createModulationRings', () =>
{
    it('normalises the contribution against the parameter range', () =>
    {
        const knobs = new Map([['oscLevel', fakeKnob()]]);
        const rings = createModulationRings(knobs);
        const descriptor = PARAMETERS_BY_ID.oscLevel;

        // oscLevel spans 0..1: a 0.25 contribution IS a 0.25 ring.
        rings.handleFrame({ modulation: Array.from({ length: 28 }, (_, i) => (i === OSC_LEVEL ? 0.25 : 0)) });

        expect(knobs.get('oscLevel').setModulation).toHaveBeenCalledWith(0.25 / (descriptor.maxValue - descriptor.minValue));

        rings.destroy();
    });

    it('skips "Off", unknown ids and missing knobs', () =>
    {
        const rings = createModulationRings(new Map());   // no knobs mounted

        expect(() => rings.handleFrame({ modulation: new Array(28).fill(0.5) })).not.toThrow();

        rings.destroy();
    });

    it('survives a short or missing modulation array', () =>
    {
        const knobs = new Map([['oscLevel', fakeKnob()]]);
        const rings = createModulationRings(knobs);

        // No array: nothing to fan out.
        expect(() => rings.handleFrame({})).not.toThrow();
        expect(knobs.get('oscLevel').setModulation).not.toHaveBeenCalled();

        // A SHORT array is legitimate: indexes beyond its length are skipped,
        // the ones present still drive their destination (index 1 = oscLevel).
        expect(() => rings.handleFrame({ modulation: [0.1, 0.2] })).not.toThrow();
        expect(knobs.get('oscLevel').setModulation).toHaveBeenCalledWith(0.2);

        rings.destroy();
    });

    it('destroy clears every ring it touched', () =>
    {
        const knob = fakeKnob();
        const knobs = new Map([['oscLevel', knob]]);
        const rings = createModulationRings(knobs);

        rings.handleFrame({ modulation: Array.from({ length: 28 }, (_, i) => (i === OSC_LEVEL ? 0.5 : 0)) });
        rings.destroy();

        expect(knob.clearModulation).toHaveBeenCalled();
    });
});
