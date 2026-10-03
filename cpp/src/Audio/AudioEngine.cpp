#include "Audio/AudioEngine.h"
#include <cmath>
#include <algorithm>

namespace DubSiren {

// Pi Waveform enum: Sine=0, Square=1, Saw=2, Triangle=3.
// Voice uses the band-limited (POLYBLEP) DaisySP shapes.
static inline uint8_t voiceWave(int piIdx) {
    switch (piIdx & 3) {
        case 0:  return daisysp::Oscillator::WAVE_SIN;
        case 1:  return daisysp::Oscillator::WAVE_POLYBLEP_SQUARE;
        case 2:  return daisysp::Oscillator::WAVE_POLYBLEP_SAW;
        default: return daisysp::Oscillator::WAVE_POLYBLEP_TRI;
    }
}
// LFO doesn't alias at LFO rates, so plain shapes are fine.
static inline uint8_t lfoWave(int piIdx) {
    switch (piIdx & 3) {
        case 0:  return daisysp::Oscillator::WAVE_SIN;
        case 1:  return daisysp::Oscillator::WAVE_SQUARE;
        case 2:  return daisysp::Oscillator::WAVE_SAW;
        default: return daisysp::Oscillator::WAVE_TRI;
    }
}

static inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

AudioEngine::AudioEngine(int sampleRate, int bufferSize)
    : sampleRate(sampleRate)
    , bufferSize(bufferSize)
    , level(0.7f)
    , baseFreq(220.0f)
    , lfoRate(4.0f)
    , lfoDepthHz(0.0f)
    , toneHz(4000.0f)
    , delaySec(0.3f)
    , feedback(0.5f)
    , oscWaveIdx(1)   // Square (Pi enum) -> band-limited square
    , lfoWaveIdx(0)   // Sine
    , pitchEnvMode(PitchEnvelopeMode::None)
    , gateOn(false)
    , amp(0.0f)
    , toneState(0.0f)
    , delCurrent(0.0f)
    , baseFreqCurrent(220.0f)
{
    const float sr = static_cast<float>(sampleRate);

    osc.Init(sr);
    osc.SetWaveform(voiceWave(oscWaveIdx.get()));
    osc.SetAmp(1.0f);

    lfo.Init(sr);
    lfo.SetWaveform(lfoWave(lfoWaveIdx.get()));
    lfo.SetFreq(lfoRate.get());
    lfo.SetAmp(0.0f);

    fbFilt.Init(sr);
    fbFilt.SetFreq(2800.0f);
    fbFilt.SetRes(0.2f);
    fbFilt.SetDrive(0.1f);

    delayLine.Init();
    delCurrent = delaySec.get() * sr;
}

void AudioEngine::process(float* output, int numFrames) {
    const float sr = static_cast<float>(sampleRate);

    // Apply current parameters on the audio thread.
    osc.SetWaveform(voiceWave(oscWaveIdx.get()));
    lfo.SetWaveform(lfoWave(lfoWaveIdx.get()));
    lfo.SetFreq(lfoRate.get());
    lfo.SetAmp(lfoDepthHz.get());

    const float fb       = feedback.get();
    const float lvl      = level.get();
    const float delTgt   = delaySec.get() * sr;
    const float toneCoef = 1.0f - std::exp(-2.0f * static_cast<float>(M_PI) * toneHz.get() / sr);
    const bool  gate     = gateOn.load();

    for (int i = 0; i < numFrames; ++i) {
        // click-free gate
        daisysp::fonepole(amp, gate ? 1.0f : 0.0f, 0.002f);

        // sine (or selected) LFO wails the pitch, additive in Hz
        // Tune 0.01f by ear on hardware; smaller is smoother, larger is faster.
        daisysp::fonepole(baseFreqCurrent, baseFreq.get(), 0.01f);
        float f = daisysp::fclamp(baseFreqCurrent + lfo.Process(), 20.0f, 8000.0f);
        osc.SetFreq(f);
        float sig = osc.Process() * amp;

        // tone: output one-pole lowpass
        daisysp::fonepole(toneState, sig, toneCoef);
        sig = toneState;

        // slewed delay time -> dub pitch-bend on the tails
        daisysp::fonepole(delCurrent, delTgt, 0.0002f);
        delayLine.SetDelay(delCurrent);

        float echo = delayLine.Read();
        fbFilt.Process(echo);
        echo = fbFilt.Low();
        delayLine.Write(std::tanh(sig + fb * echo)); // soft-clipped feedback

        float out = (sig + echo) * lvl;
        output[i * 2]     = out; // L
        output[i * 2 + 1] = out; // R
    }
}

void AudioEngine::trigger() {
    std::lock_guard<std::mutex> lock(triggerMutex);
    gateOn.store(true);
}

void AudioEngine::release() {
    std::lock_guard<std::mutex> lock(triggerMutex);
    gateOn.store(false);
}

const char* AudioEngine::cyclePitchEnvelope() {
    return "none"; // siren voice has no pitch envelope
}

// ---------------------------------------------------------------- setters
void AudioEngine::setVolume(float v)          { level.set(clampf(v, 0.0f, 1.0f)); }
void AudioEngine::setFrequency(float freq)    { baseFreq.set(clampf(freq, 20.0f, 8000.0f)); }
void AudioEngine::setWaveform(Waveform wf)    { oscWaveIdx.set(static_cast<int>(wf)); }
void AudioEngine::setWaveform(int index)      { oscWaveIdx.set(index & 3); }
void AudioEngine::setAttackTime(float)        {}
void AudioEngine::setReleaseTime(float)       {}
void AudioEngine::setLfoRate(float rate)      { lfoRate.set(clampf(rate, 0.02f, 30.0f)); }
void AudioEngine::setLfoDepth(float depth)    { lfoDepthHz.set(clampf(depth, 0.0f, 1.0f) * 1200.0f); }
void AudioEngine::setLfoPitchDepth(float)     {}
void AudioEngine::setLfoWaveform(Waveform wf) { lfoWaveIdx.set(static_cast<int>(wf)); }
void AudioEngine::setLfoWaveform(int index)   { lfoWaveIdx.set(index & 3); }
void AudioEngine::setFilterCutoff(float freq) { toneHz.set(clampf(freq, 200.0f, 16000.0f)); }
void AudioEngine::setFilterResonance(float)   {}
void AudioEngine::setDelayTime(float seconds) { delaySec.set(clampf(seconds, 0.001f, 2.5f)); }
void AudioEngine::setDelayFeedback(float f)   { feedback.set(clampf(f, 0.0f, 0.95f)); }
void AudioEngine::setDelayMix(float)          {}
void AudioEngine::setReverbSize(float)        {}
void AudioEngine::setReverbMix(float)         {}
void AudioEngine::setReverbDamping(float)     {}
void AudioEngine::setPitchEnvelopeMode(PitchEnvelopeMode m) { pitchEnvMode.set(m); }

} // namespace DubSiren
