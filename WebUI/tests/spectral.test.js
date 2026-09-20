/**
 * Spectral visualizer: 64 telemetry-driven bars, paint-not-state.
 *
 * Pinning: 64 bars always present (rest state even without a channel), a frame
 * maps spectral[i] 0..1 onto bar height with NaN treated as silence, values are
 * clamped, snapshots never freeze the bars, and destroy() unsubscribes.
 */

import { describe, expect, it, vi } from 'vitest';

import { createSpectral } from '../src/ui/spectral.js';

/** Mount + capture the pump the channel hands back (store.onTelemetry shape). */
function mount (options = {})
{
    let pump = null;
    const unsubscribe = vi.fn();
    const onFrame = options.onFrame
        ?? ((notify) => { pump = notify; return unsubscribe; });

    const visual = createSpectral({ onFrame });

    document.body.append(visual.element);

    return { visual, element: visual.element, pump, unsubscribe };
}

function heightPct (bar)
{
    return Number.parseFloat(bar.style.height);
}

describe('spectral', () =>
{
    it('renders 64 bars at rest without a channel', () =>
    {
        const { element, visual } = mount({ onFrame: null });

        expect(element.querySelectorAll('.spectral__bar')).toHaveLength(64);
        expect(heightPct(element.querySelectorAll('.spectral__bar')[0])).toBe(4);

        visual.destroy();
    });

    it('maps a frame onto bar heights, clamped, NaN as silence', () =>
    {
        const { element, visual, pump } = mount();
        const bars = element.querySelectorAll('.spectral__bar');

        visual.paint({});   // snapshots never move the bars
        expect(heightPct(bars[0])).toBe(4);

        const frame = { spectral: new Array(64).fill(0) };

        frame.spectral[0] = 1.0;    // full scale
        frame.spectral[1] = 0.5;
        frame.spectral[2] = Number.NaN;
        frame.spectral[3] = 7.0;    // clamped
        frame.spectral[4] = -2.0;   // clamped

        pump(frame);

        expect(heightPct(bars[0])).toBe(100);
        expect(heightPct(bars[1])).toBe(52);
        expect(heightPct(bars[2])).toBe(4);
        expect(heightPct(bars[3])).toBe(100);
        expect(heightPct(bars[4])).toBe(4);
        expect(element.dataset.active).toBe('true');

        visual.destroy();
    });

    it('unsubscribes on destroy (no frames after death)', () =>
    {
        const { visual, unsubscribe, pump } = mount();

        visual.destroy();
        unsubscribe.mock.results;
        expect(unsubscribe).toHaveBeenCalledTimes(1);

        // A late frame after destroy must not throw nor repaint.
        expect(() => pump?.({ spectral: new Array(64).fill(0.5) })).not.toThrow();
    });
});
