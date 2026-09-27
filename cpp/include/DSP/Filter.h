#pragma once

#include "Common.h"

namespace DubSiren {

/**
 * Resonant state-variable low-pass filter (12 dB/octave) with parameter smoothing.
 *
 * Replaces the previous one-pole design. Implemented as a topology-preserving
 * transform (TPT / "zero delay feedback") SVF after Andrew Simper (Cytomic).
 * Chosen because it stays stable and well-behaved under fast, per-sample
 * cutoff modulation (the engine sweeps cutoff with the LFO every sample),
 * which the classic Chamberlin SVF does not at high cutoffs.
 *
 * Resonance is expressed as Q (same range as before, 0.1 - 20.0):
 *   Q ~ 0.5   : dull, over-damped
 *   Q = 0.707 : flat Butterworth response
 *   Q = 1-4   : increasingly pronounced resonant peak (classic siren/acid zone)
 *   Q > 8     : screaming, approaching self-oscillation
 *
 * Parameter smoothing prevents "zipper noise" and clicks when filter
 * parameters change rapidly (e.g., from rotary encoder adjustments).
 * Public API is unchanged from the one-pole version - drop-in replacement.
 */
class LowPassFilter {
public:
    explicit LowPassFilter(int sampleRate = DEFAULT_SAMPLE_RATE);

    /**
     * Process audio through the filter.
     * @param input Input buffer
     * @param output Output buffer (can be same as input)
     * @param numSamples Number of samples to process
     */
    void process(const float* input, float* output, int numSamples);

    /**
     * Process a single sample (for sample-accurate processing)
     */
    float processSample(float input);

    // Parameter setters
    void setCutoff(float freq);
    void setResonance(float res);
    void reset();

    // Getters
    float getCutoff() const { return cutoff; }
    float getResonance() const { return resonance; }

private:
    int sampleRate;
    float cutoff;           // Target cutoff frequency (Hz)
    float cutoffCurrent;    // Smoothed current cutoff (Hz)
    float resonance;        // Target resonance (Q)
    float resonanceCurrent; // Smoothed current resonance (Q)
    float smoothing;        // Smoothing coefficient

    // SVF integrator states (replace the old one-pole prevOutput)
    float ic1eq;
    float ic2eq;

    float maxCutoff;        // Stability ceiling, derived from sampleRate
};

/**
 * DC blocking filter to remove DC offset.
 *
 * DC offset can accumulate in feedback loops (filters, delay, reverb) and waste
 * headroom, leading to asymmetric clipping and pops. This first-order high-pass
 * filter at ~10Hz removes DC while preserving bass frequencies.
 */
class DCBlocker {
public:
    DCBlocker();

    void process(const float* input, float* output, int numSamples);
    float processSample(float input);
    void reset();

private:
    float xPrev;
    float yPrev;
    float coeff;  // High-pass at ~10Hz @ 48kHz
};

} // namespace DubSiren
