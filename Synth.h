#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>
#include <vector>

namespace melosumm
{

enum class Wave { None, Sine, Saw, Square, Triangle };

struct Preset
{
    juce::String name;
    bool isBass = false;

    Wave  w1 = Wave::Saw;
    Wave  w2 = Wave::None;
    float w2Semis = 0.0f, w2Level = 0.0f;
    int   unison = 1;
    float unisonDetune = 0.0f;     // Cent
    float stereoSpread = 0.6f;
    float sub = 0.0f, noise = 0.0f;

    float fmRatio = 0.0f, fmIndex = 0.0f, fmSustainIndex = 0.0f, fmDecay = 0.3f;

    bool  karplus = false;
    float ksDecaySec = 1.5f, ksBright = 0.7f;

    float cutoff = 20000.0f, reso = 0.0f, filterEnvOct = 0.0f, keytrack = 0.0f;
    float fA = 0.001f, fD = 0.3f, fS = 0.0f, fR = 0.3f;

    float a = 0.005f, d = 0.3f, s = 0.8f, r = 0.2f;

    float pitchEnvSemis = 0.0f, pitchDecay = 0.05f;
    float vibDepth = 0.0f, vibRate = 5.0f;
    float drive = 0.0f;
    float gain = 0.4f;
};

const std::vector<Preset>& getPresets();
juce::StringArray presetNames();
int presetIndex (const juce::String& name);

/** Einfache Huellkurve (linearer Attack, exponentielles Decay/Release). */
struct Envelope
{
    void setup (float a, float d, float s, float r, double sr) noexcept;
    void noteOn() noexcept   { stage = Attack; }
    void noteOff() noexcept  { if (stage != Idle) stage = Release; }
    void kill() noexcept     { stage = Idle; value = 0.0f; }
    bool isActive() const noexcept { return stage != Idle; }
    float next() noexcept;
    void advance (int n) noexcept { for (int i = 0; i < n; ++i) next(); }
    float current() const noexcept { return value; }

    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float value = 0.0f;
    float attackInc = 1.0f, decayCoef = 0.0f, sustain = 1.0f, releaseCoef = 0.0f;
};

class Voice
{
public:
    void prepare (double sampleRate);
    void start (int note, int velocity, const Preset& preset, uint64_t order);
    void release() noexcept;
    void kill() noexcept;
    bool isActive() const noexcept { return amp.isActive(); }
    bool isReleased() const noexcept { return released; }
    int  getNote() const noexcept { return note; }
    uint64_t getOrder() const noexcept { return order; }

    void render (float* L, float* R, int numSamples) noexcept;

private:
    double sr = 44100.0;
    const Preset* p = nullptr;
    int note = -1;
    float vel = 1.0f, freq = 440.0f;
    double time = 0.0;
    uint64_t order = 0;
    bool released = false;

    static constexpr int kMaxUnison = 7;
    std::array<double, kMaxUnison> ph1 {}, ph2 {};
    std::array<float, kMaxUnison> detMul {}, panL {}, panR {};
    double subPh = 0.0, fmPh = 0.0, lfoPh = 0.0;
    uint32_t rng = 0x12345678u;

    Envelope amp, fenv;
    float ic1L = 0, ic2L = 0, ic1R = 0, ic2R = 0;

    std::vector<float> ks;
    int ksWrite = 0;
    float ksPrev = 0.0f, ksLp = 0.0f, ksDamp = 0.99f;
    double ksDelay = 100.0;
    int ksExcite = 0;

    float noiseSample() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (rng & 0xffffff) / 8388608.0f - 1.0f;
    }
};

class TrackSynth
{
public:
    void prepare (double sampleRate);
    void setPreset (int index) noexcept;
    int  getPreset() const noexcept { return presetIdx; }
    void noteOn (int note, int velocity) noexcept;
    void noteOff (int note) noexcept;
    void allNotesOff (bool hard) noexcept;
    void render (float* L, float* R, int numSamples) noexcept;   // addiert

private:
    static constexpr int kVoices = 16;
    std::array<Voice, kVoices> voices;
    int presetIdx = 0;
    uint64_t counter = 0;
};

} // namespace melosumm
