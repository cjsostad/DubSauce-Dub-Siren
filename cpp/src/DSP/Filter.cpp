#include "DSP/Filter.h"
#include <cmath>
#include <algorithm>

namespace DubSiren {

// ============================================================================
// LowPassFilter Implementation (TPT state-variable filter, 12 dB/oct)
// ============================================================================

LowPassFilter::LowPassFilter(int sampleRate)
    : sampleRate(sampleRate)
    , cutoff(3000.0f)        // Same default as before
    , cutoffCurrent(3000.0f)
    , resonance(1.0f)        // Q = 1.0: gentle peak, same default as before
    , resonanceCurrent(1.0f)
    , smoothing(0.05f)       // Same smoothing rate as before
    , ic1eq(0.0f)
    , ic2eq(0.0f)
{
    // Keep tan() well away from its pole at Nyquist. 0.45 * fs = 21.6 kHz
    // at 48 kHz, so in practice this never binds below the 20 kHz clamp
    // in setCutoff, but it protects us if the sample rate ever drops.
    maxCutoff = 0.45f * static_cast<float>(sampleRate);
}

void LowPassFilter::process(const float* input, float* output, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        output[i] = processSample(input[i]);
    }
}

float LowPassFilter::processSample(float input) {
    // Smooth parameter changes to prevent zipper noise (unchanged behaviour:
    // the engine sets targets, we glide toward them each sample)
    cutoffCurrent += (cutoff - cutoffCurrent) * smoothing;
    resonanceCurrent += (resonance - resonanceCurrent) * smoothing;

    // --- TPT SVF coefficients (recomputed per sample because cutoff is
    // --- modulated per sample by the LFO in AudioEngine)
    float fc = std::min(cutoffCurrent, maxCutoff);
    float g = std::tan(PI * fc / static_cast<float>(sampleRate));
    float k = 1.0f / resonanceCurrent;   // damping = 1/Q; small k = big resonance

    float a1 = 1.0f / (1.0f + g * (g + k));
    float a2 = g * a1;
    float a3 = g * a2;

    // --- TPT SVF core (Simper). v2 is the low-pass output.
    float v3 = input - ic2eq;
    float v1 = a1 * ic1eq + a2 * v3;
    float v2 = ic2eq + a2 * ic1eq + a3 * v3;

    ic1eq = 2.0f * v1 - ic1eq;
    ic2eq = 2.0f * v2 - ic2eq;

    // Clamp state to prevent runaway values at extreme resonance
    ic1eq = clampSample(ic1eq);
    ic2eq = clampSample(ic2eq);

    return v2;
}

void LowPassFilter::setCutoff(float freq) {
    cutoff = std::clamp(freq, 20.0f, 20000.0f);
}

void LowPassFilter::setResonance(float res) {
    resonance = std::clamp(res, 0.1f, 20.0f);
}

void LowPassFilter::reset() {
    ic1eq = 0.0f;
    ic2eq = 0.0f;
    cutoffCurrent = cutoff;
    resonanceCurrent = resonance;
}

// ============================================================================
// DCBlocker Implementation (unchanged)
// ============================================================================

DCBlocker::DCBlocker()
    : xPrev(0.0f)
    , yPrev(0.0f)
    , coeff(0.995f)  // High-pass at ~10Hz @ 48kHz
{
}

void DCBlocker::process(const float* input, float* output, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        output[i] = processSample(input[i]);
    }
}

float DCBlocker::processSample(float input) {
    // First-order high-pass filter: y[n] = x[n] - x[n-1] + coeff * y[n-1]
    float output = input - xPrev + coeff * yPrev;
    xPrev = input;
    yPrev = output;
    return output;
}

void DCBlocker::reset() {
    xPrev = 0.0f;
    yPrev = 0.0f;
}

} // namespace DubSiren
