#pragma once

#include "Common.h"
#include <cstdint>

namespace DubSiren {

/**
 * Audio oscillator with multiple waveform types and PolyBLEP anti-aliasing.
 *
 * PolyBLEP (Polynomial Band-Limited Step) is applied to discontinuous
 * waveforms to reduce aliasing artifacts.
 *
 * The Square waveform is an "S-1 voice": a variable-width pulse (default
 * 60% duty, PolyBLEP-corrected at both edges) mixed with a phase-locked
 * sawtooth layer (default 23%), modelled on the Roland S-1 dub siren
 * patch this build targets. setPulseWidth(0.5) and setSawMix(0.0)
 * restore a classic plain square.
 */
class Oscillator {
public:
    explicit Oscillator(int sampleRate = DEFAULT_SAMPLE_RATE);

    /**
     * Generate audio samples for the current waveform.
     * @param output Buffer to fill with samples
     * @param numSamples Number of samples to generate
     */
    void generate(float* output, int numSamples);

    /**
     * Generate a single sample (for sample-accurate processing)
     */
    float generateSample();

    // Parameter setters
    void setFrequency(float freq);
    void setWaveform(Waveform waveform);
    void setPulseWidth(float width);   // 0.05-0.95; duty cycle of the Square voice
    void setSawMix(float mix);         // 0.0-1.0; saw layer level in the Square voice
    void setDrift(float amount);       // 0.0-0.02; slow random pitch instability (analog grain)
    void setNoise(float level);        // 0.0-0.1; noise floor mixed into the Square voice
    void resetPhase();

    // Getters
    float getFrequency() const { return frequency; }
    Waveform getWaveform() const { return waveform; }
    float getPhase() const { return phase; }
    float getPulseWidth() const { return pulseWidth; }
    float getSawMix() const { return sawMix; }

private:
    int sampleRate;
    float frequency;
    float phase;  // Phase accumulator (0.0 to 1.0)
    Waveform waveform;
    float pulseWidth;  // Duty cycle for the Square (pulse) voice
    float sawMix;      // Saw layer mix for the Square voice
    float driftAmount; // Max relative pitch wander (e.g. 0.004 = +/-0.4%)
    float noiseLevel;  // Noise mixed into the Square voice
    float driftState;  // Slow random-walk state for pitch drift
    uint32_t rngState; // Cheap xorshift RNG state

    // Cheap white noise in [-1, 1]
    float nextNoise();

    // PolyBLEP helper function
    float polyBlep(float t, float dt) const;

    // Waveform generators
    float generateSine();
    float generatePulsePolyBlep();   // Variable-width pulse (the S-1 square)
    float generateSawPolyBlep();
    float generateTriangle();
};

} // namespace DubSiren
