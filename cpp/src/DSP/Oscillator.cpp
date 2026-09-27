#include "DSP/Oscillator.h"
#include <cmath>
#include <algorithm>

namespace DubSiren {

Oscillator::Oscillator(int sampleRate)
    : sampleRate(sampleRate)
    , frequency(440.0f)
    , phase(0.0f)
    , waveform(Waveform::Sine)
    , pulseWidth(0.6f)   // S-1 patch: pulse width off-centre (~60%)
    , sawMix(4.3f)       // S-1 patch corrected: saw-dominant (127) with light pulse (29)
    , driftAmount(0.004f) // Analog grain: pitch wanders +/-0.4% like a warm 555
    , noiseLevel(0.012f)  // Analog grain: faint noise floor (~-38 dB)
    , driftState(0.0f)
    , rngState(0x9E3779B9u)
{
}

float Oscillator::nextNoise() {
    // xorshift32: fast, no allocations, good enough for audio noise
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return (static_cast<int32_t>(rngState) * (1.0f / 2147483648.0f));
}

void Oscillator::generate(float* output, int numSamples) {
    for (int i = 0; i < numSamples; ++i) {
        output[i] = generateSample();
    }
}

float Oscillator::generateSample() {
    float sample = 0.0f;

    switch (waveform) {
        case Waveform::Sine:
            sample = generateSine();
            break;
        case Waveform::Square: {
            // S-1 voice: variable-width pulse + phase-locked saw layer,
            // normalised so the sum stays within +/-1.
            float pulse = generatePulsePolyBlep();
            float saw = generateSawPolyBlep();
            sample = (pulse + sawMix * saw) / (1.0f + sawMix);
            // Analog grain: faint noise floor, filtered later by the engine's SVF
            sample += noiseLevel * nextNoise();
            break;
        }
        case Waveform::Saw:
            sample = generateSawPolyBlep();
            break;
        case Waveform::Triangle:
            sample = generateTriangle();
            break;
    }

    // Analog grain: slow random pitch drift. driftState is a heavily
    // smoothed random walk in [-1, 1]; it nudges the effective frequency
    // by up to +/-driftAmount, like component drift in a 555 circuit.
    driftState += 0.0008f * (nextNoise() - driftState);
    float driftedFreq = frequency * (1.0f + driftAmount * driftState * 20.0f);

    // Advance phase
    float dt = driftedFreq / static_cast<float>(sampleRate);
    phase += dt;
    if (phase >= 1.0f) {
        phase -= 1.0f;
    }

    return sample;
}

float Oscillator::polyBlep(float t, float dt) const {
    /**
     * Calculate PolyBLEP (Polynomial Band-Limited Step) residual.
     *
     * PolyBLEP reduces aliasing in discontinuous waveforms (square, sawtooth)
     * by applying a polynomial correction near discontinuities.
     *
     * @param t Current phase position (0.0 to 1.0)
     * @param dt Phase increment per sample (frequency / sample_rate)
     * @return The PolyBLEP residual to subtract from the naive waveform
     */

    // Check if we're within one sample of a discontinuity
    if (t < dt) {
        // Just after the discontinuity (phase recently wrapped)
        float tNorm = t / dt;  // Normalize to 0-1 range
        return tNorm + tNorm - tNorm * tNorm - 1.0f;
    } else if (t > 1.0f - dt) {
        // Just before the discontinuity (phase about to wrap)
        float tNorm = (t - 1.0f) / dt;  // Normalize to -1 to 0 range
        return tNorm * tNorm + tNorm + tNorm + 1.0f;
    }

    return 0.0f;
}

float Oscillator::generateSine() {
    // Sine wave - naturally band-limited, no anti-aliasing needed
    return std::sin(TWO_PI * phase);
}

float Oscillator::generatePulsePolyBlep() {
    /**
     * Generate a variable-width pulse wave with PolyBLEP anti-aliasing.
     *
     * Rising edge at phase = 0, falling edge at phase = pulseWidth.
     * The DC offset a non-50% duty cycle introduces (2w - 1) is removed
     * here so downstream stages see a zero-centred signal.
     */
    float dt = frequency / static_cast<float>(sampleRate);

    // Naive pulse: +1 while phase < width, -1 after
    float value = (phase < pulseWidth) ? 1.0f : -1.0f;

    // PolyBLEP correction at the rising edge (phase = 0)
    value += 2.0f * polyBlep(phase, dt);

    // PolyBLEP correction at the falling edge (phase = pulseWidth)
    float phaseShifted = phase + (1.0f - pulseWidth);
    if (phaseShifted >= 1.0f) {
        phaseShifted -= 1.0f;
    }
    value -= 2.0f * polyBlep(phaseShifted, dt);

    // Remove the duty-cycle DC offset
    value -= (2.0f * pulseWidth - 1.0f);

    return value;
}

float Oscillator::generateSawPolyBlep() {
    /**
     * Generate sawtooth wave with PolyBLEP anti-aliasing.
     *
     * PolyBLEP is applied at the phase reset (when saw jumps from +1 to -1)
     * to smooth the discontinuity and reduce aliasing.
     */
    float dt = frequency / static_cast<float>(sampleRate);

    // Naive sawtooth: ramps from -1 to +1 over one cycle
    float value = 2.0f * phase - 1.0f;

    // Apply PolyBLEP correction at the discontinuity (phase = 0)
    value -= 2.0f * polyBlep(phase, dt);

    return value;
}

float Oscillator::generateTriangle() {
    /**
     * Generate triangle wave.
     *
     * Triangle has no discontinuities in value (only in slope), so
     * aliasing is much less severe and no PolyBLEP is applied.
     */
    if (phase < 0.5f) {
        return 4.0f * phase - 1.0f;
    } else {
        return 3.0f - 4.0f * phase;
    }
}

void Oscillator::setFrequency(float freq) {
    frequency = clamp(freq, 20.0f, 20000.0f);
}

void Oscillator::setWaveform(Waveform wf) {
    waveform = wf;
}

void Oscillator::setPulseWidth(float width) {
    pulseWidth = clamp(width, 0.05f, 0.95f);
}

void Oscillator::setSawMix(float mix) {
    sawMix = clamp(mix, 0.0f, 5.0f);
}

void Oscillator::setDrift(float amount) {
    driftAmount = clamp(amount, 0.0f, 0.02f);
}

void Oscillator::setNoise(float level) {
    noiseLevel = clamp(level, 0.0f, 0.1f);
}

void Oscillator::resetPhase() {
    phase = 0.0f;
}

} // namespace DubSiren
