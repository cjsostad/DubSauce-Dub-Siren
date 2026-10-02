#pragma once

#include "Common.h"
#include "daisysp.h"
#include <atomic>
#include <mutex>

namespace DubSiren {

// Max echo buffer: 2.5 s at 48 kHz — matches the Daisy build.
static constexpr size_t SIREN_MAX_DELAY = static_cast<size_t>(48000 * 2.5f);

/**
 * Dub Siren Audio Engine — Daisy voice.
 *
 * The DSP is a straight port of the Daisy Seed siren (DubSirenPod /
 * MultiEffectSiren GetSirenSample): band-limited oscillator, sine LFO added to
 * pitch in Hz, one-pole tone lowpass, and a slewed feedback delay with a dark
 * filter and tanh soft-clip. Reverb / pitch-envelope / resonance from the old
 * engine are gone, but every public setter is retained so GPIOController and
 * main compile and run unchanged — the removed features are harmless no-ops.
 */
class AudioEngine {
public:
    explicit AudioEngine(int sampleRate = DEFAULT_SAMPLE_RATE, int bufferSize = DEFAULT_BUFFER_SIZE);
    ~AudioEngine() = default;
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    void process(float* output, int numFrames);
    void trigger();
    void release();
    const char* cyclePitchEnvelope();

    // Master
    void setVolume(float volume);          // -> siren level

    // Oscillator (voice)
    void setFrequency(float freq);         // -> pitch (Hz)
    void setWaveform(Waveform wf);
    void setWaveform(int index);           // -> band-limited voice shape

    // Envelope (kept for compat; siren uses a fixed click-free gate)
    void setAttackTime(float seconds);
    void setReleaseTime(float seconds);

    // LFO (the wail)
    void setLfoRate(float rate);           // -> wail rate (Hz)
    void setLfoDepth(float depth);         // 0..1 -> wail depth in Hz
    void setLfoPitchDepth(float depth);    // no-op (wail depth is setLfoDepth)
    void setLfoWaveform(Waveform wf);
    void setLfoWaveform(int index);        // -> LFO/wail shape

    // Filter -> tone
    void setFilterCutoff(float freq);      // -> tone lowpass cutoff
    void setFilterResonance(float res);    // no-op

    // Delay
    void setDelayTime(float seconds);
    void setDelayFeedback(float feedback);
    void setDelayMix(float mix);           // no-op (dry + echo summed)

    // Reverb (no-ops; siren-only)
    void setReverbSize(float size);
    void setReverbMix(float mix);
    void setReverbDamping(float damping);

    // Pitch envelope (compat only; not used by the voice)
    void setPitchEnvelopeMode(PitchEnvelopeMode mode);

    // Getters
    float getVolume() const { return level.get(); }
    float getFrequency() const { return baseFreq.get(); }
    bool isPlaying() const { return gateOn.load(); }
    PitchEnvelopeMode getPitchEnvelopeMode() const { return pitchEnvMode.get(); }

private:
    int sampleRate;
    int bufferSize;

    // Daisy siren DSP
    daisysp::Oscillator osc;
    daisysp::Oscillator lfo;
    daisysp::Svf        fbFilt;  // fixed-dark filter in the feedback path
    daisysp::DelayLine<float, SIREN_MAX_DELAY> delayLine;

    // Thread-safe params (set from GPIO thread, applied in the audio thread)
    AudioParameter<float> level;
    AudioParameter<float> baseFreq;
    AudioParameter<float> lfoRate;
    AudioParameter<float> lfoDepthHz;
    AudioParameter<float> toneHz;
    AudioParameter<float> delaySec;
    AudioParameter<float> feedback;
    AudioParameter<int>   oscWaveIdx;
    AudioParameter<int>   lfoWaveIdx;
    AudioParameter<PitchEnvelopeMode> pitchEnvMode;

    std::atomic<bool> gateOn;

    // Audio-thread-only state
    float amp;
    float toneState;
    float delCurrent;

    std::mutex triggerMutex;
};

} // namespace DubSiren
