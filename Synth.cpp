#include "Synth.h"
#include <algorithm>
#include <cmath>

namespace melosumm
{

namespace
{
    constexpr double twoPi = 6.283185307179586;

    inline float polyBlep (double t, double dt) noexcept
    {
        if (dt <= 0.0) return 0.0f;
        if (t < dt)       { t /= dt;               return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt;    return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    inline float osc (Wave w, double ph, double inc, double pm) noexcept
    {
        switch (w)
        {
            case Wave::Sine:     return (float) std::sin (twoPi * ph + pm);
            case Wave::Saw:      return (float) (2.0 * ph - 1.0) - polyBlep (ph, inc);
            case Wave::Square:
            {
                float v = ph < 0.5 ? 1.0f : -1.0f;
                v += polyBlep (ph, inc);
                double p2 = ph + 0.5; if (p2 >= 1.0) p2 -= 1.0;
                v -= polyBlep (p2, inc);
                return v;
            }
            case Wave::Triangle: return (float) (4.0 * std::abs (ph - 0.5) - 1.0);
            case Wave::None:
            default:             return 0.0f;
        }
    }

    inline void advance (double& ph, double inc) noexcept
    {
        ph += inc;
        if (ph >= 1.0) ph -= std::floor (ph);
    }

    std::vector<Preset> makePresets()
    {
        std::vector<Preset> v;
        auto add = [&] (const char* name, auto&& fn) { Preset p; p.name = name; fn (p); v.push_back (p); };

        add ("Synth Pluck", [] (Preset& p) {
            p.w1 = Wave::Saw; p.unison = 3; p.unisonDetune = 10;
            p.cutoff = 500; p.reso = 0.3f; p.filterEnvOct = 3.8f; p.fA = 0.001f; p.fD = 0.22f; p.fS = 0.0f; p.fR = 0.2f;
            p.a = 0.001f; p.d = 0.9f; p.s = 0.0f; p.r = 0.25f; p.gain = 0.45f; });
        add ("Grand Piano", [] (Preset& p) {
            p.w1 = Wave::Sine; p.fmRatio = 1.0f; p.fmIndex = 2.2f; p.fmSustainIndex = 0.25f; p.fmDecay = 0.35f;
            p.w2 = Wave::Sine; p.w2Semis = 12; p.w2Level = 0.22f; p.keytrack = 0.0f;
            p.a = 0.002f; p.d = 2.4f; p.s = 0.0f; p.r = 0.35f; p.drive = 0.15f; p.gain = 0.50f; });
        add ("E-Piano", [] (Preset& p) {
            p.w1 = Wave::Sine; p.fmRatio = 1.0f; p.fmIndex = 3.0f; p.fmSustainIndex = 0.5f; p.fmDecay = 0.5f;
            p.w2 = Wave::Sine; p.w2Semis = 31; p.w2Level = 0.06f;
            p.a = 0.002f; p.d = 3.0f; p.s = 0.15f; p.r = 0.4f; p.gain = 0.48f; });
        add ("Pluck (Gitarre)", [] (Preset& p) {
            p.karplus = true; p.ksDecaySec = 1.4f; p.ksBright = 0.75f;
            p.a = 0.001f; p.d = 4.0f; p.s = 0.0f; p.r = 0.2f; p.gain = 1.0f; });
        add ("Harfe", [] (Preset& p) {
            p.karplus = true; p.ksDecaySec = 3.0f; p.ksBright = 0.4f; p.cutoff = 6000;
            p.a = 0.001f; p.d = 5.0f; p.s = 0.0f; p.r = 0.6f; p.gain = 1.0f; });
        add ("Glocke", [] (Preset& p) {
            p.w1 = Wave::Sine; p.fmRatio = 3.5f; p.fmIndex = 5.0f; p.fmSustainIndex = 0.4f; p.fmDecay = 1.2f;
            p.a = 0.001f; p.d = 4.0f; p.s = 0.0f; p.r = 1.0f; p.gain = 0.35f; });
        add ("Marimba", [] (Preset& p) {
            p.w1 = Wave::Sine; p.fmRatio = 4.0f; p.fmIndex = 2.5f; p.fmSustainIndex = 0.0f; p.fmDecay = 0.06f;
            p.a = 0.001f; p.d = 0.7f; p.s = 0.0f; p.r = 0.15f; p.gain = 0.60f; });
        add ("Saw Lead", [] (Preset& p) {
            p.w1 = Wave::Saw; p.unison = 2; p.unisonDetune = 12;
            p.cutoff = 3000; p.reso = 0.25f; p.filterEnvOct = 1.5f; p.fD = 0.3f; p.fS = 0.3f;
            p.a = 0.005f; p.d = 0.2f; p.s = 0.85f; p.r = 0.15f; p.vibDepth = 0.15f; p.vibRate = 5.5f; p.gain = 0.28f; });
        add ("Supersaw", [] (Preset& p) {
            p.w1 = Wave::Saw; p.unison = 7; p.unisonDetune = 28; p.stereoSpread = 1.0f;
            p.cutoff = 7000; p.reso = 0.1f; p.filterEnvOct = 0.5f; p.fD = 0.4f; p.fS = 0.5f;
            p.a = 0.01f; p.d = 0.3f; p.s = 0.9f; p.r = 0.3f; p.gain = 0.24f; });
        add ("Square Lead", [] (Preset& p) {
            p.w1 = Wave::Square; p.sub = 0.35f; p.cutoff = 4000; p.reso = 0.15f; p.filterEnvOct = 1.0f; p.fD = 0.25f; p.fS = 0.4f;
            p.a = 0.005f; p.d = 0.2f; p.s = 0.9f; p.r = 0.12f; p.vibDepth = 0.12f; p.gain = 0.24f; });
        add ("Floete", [] (Preset& p) {
            p.w1 = Wave::Sine; p.w2 = Wave::Sine; p.w2Semis = 12; p.w2Level = 0.12f; p.noise = 0.05f; p.cutoff = 5000;
            p.a = 0.06f; p.d = 0.2f; p.s = 0.9f; p.r = 0.15f; p.vibDepth = 0.18f; p.vibRate = 5.0f; p.gain = 0.45f; });
        add ("Orgel", [] (Preset& p) {
            p.w1 = Wave::Sine; p.w2 = Wave::Sine; p.w2Semis = 12; p.w2Level = 0.6f; p.sub = 0.5f;
            p.a = 0.005f; p.d = 0.1f; p.s = 1.0f; p.r = 0.08f; p.drive = 0.3f; p.gain = 0.30f; });
        add ("Soft Pad", [] (Preset& p) {
            p.w1 = Wave::Saw; p.unison = 4; p.unisonDetune = 15; p.w2 = Wave::Triangle; p.w2Semis = 12; p.w2Level = 0.4f;
            p.cutoff = 1400; p.reso = 0.15f; p.filterEnvOct = 0.8f; p.fA = 0.8f; p.fD = 1.5f; p.fS = 0.5f; p.fR = 1.2f;
            p.a = 0.6f; p.d = 1.0f; p.s = 0.85f; p.r = 1.3f; p.stereoSpread = 0.9f; p.gain = 0.22f; });
        add ("Strings", [] (Preset& p) {
            p.w1 = Wave::Saw; p.unison = 5; p.unisonDetune = 18; p.cutoff = 3200; p.reso = 0.05f;
            p.a = 0.3f; p.d = 0.5f; p.s = 0.9f; p.r = 0.8f; p.vibDepth = 0.08f; p.stereoSpread = 0.9f; p.gain = 0.26f; });
        add ("Choir Pad", [] (Preset& p) {
            p.w1 = Wave::Triangle; p.unison = 5; p.unisonDetune = 14; p.cutoff = 2200; p.reso = 0.3f;
            p.a = 0.5f; p.d = 0.8f; p.s = 0.9f; p.r = 1.2f; p.vibDepth = 0.1f; p.stereoSpread = 0.9f; p.gain = 0.30f; });
        add ("Dark Keys", [] (Preset& p) {
            p.w1 = Wave::Triangle; p.w2 = Wave::Saw; p.w2Semis = 0; p.w2Level = 0.3f; p.unison = 2; p.unisonDetune = 8;
            p.cutoff = 900; p.reso = 0.2f; p.filterEnvOct = 1.2f; p.fD = 0.6f; p.fS = 0.1f;
            p.a = 0.003f; p.d = 1.6f; p.s = 0.2f; p.r = 0.5f; p.gain = 0.32f; });
        add ("Sub Bass", [] (Preset& p) {
            p.isBass = true; p.w1 = Wave::Sine; p.drive = 0.15f;
            p.a = 0.005f; p.d = 0.1f; p.s = 1.0f; p.r = 0.08f; p.gain = 0.70f; });
        add ("808", [] (Preset& p) {
            p.isBass = true; p.w1 = Wave::Sine; p.pitchEnvSemis = 12.0f; p.pitchDecay = 0.035f; p.drive = 1.0f;
            p.a = 0.001f; p.d = 2.5f; p.s = 0.0f; p.r = 0.15f; p.gain = 0.60f; });
        add ("Reese Bass", [] (Preset& p) {
            p.isBass = true; p.w1 = Wave::Saw; p.unison = 2; p.unisonDetune = 22; p.stereoSpread = 0.3f;
            p.cutoff = 700; p.reso = 0.25f; p.keytrack = 0.3f;
            p.a = 0.005f; p.d = 0.2f; p.s = 1.0f; p.r = 0.1f; p.drive = 0.4f; p.gain = 0.28f; });
        add ("Pluck Bass", [] (Preset& p) {
            p.isBass = true; p.w1 = Wave::Saw; p.sub = 0.4f; p.cutoff = 300; p.reso = 0.3f; p.filterEnvOct = 3.0f; p.fD = 0.2f; p.fS = 0.1f;
            p.a = 0.001f; p.d = 0.4f; p.s = 0.4f; p.r = 0.1f; p.gain = 0.40f; });
        add ("Square Bass", [] (Preset& p) {
            p.isBass = true; p.w1 = Wave::Square; p.cutoff = 900; p.reso = 0.2f; p.filterEnvOct = 1.5f; p.fD = 0.25f; p.fS = 0.3f;
            p.a = 0.003f; p.d = 0.2f; p.s = 0.9f; p.r = 0.1f; p.gain = 0.30f; });

        return v;
    }
}

const std::vector<Preset>& getPresets()
{
    static const std::vector<Preset> presets = makePresets();
    return presets;
}

juce::StringArray presetNames()
{
    juce::StringArray s;
    for (auto& p : getPresets()) s.add (p.name);
    return s;
}

int presetIndex (const juce::String& name)
{
    const auto& p = getPresets();
    for (int i = 0; i < (int) p.size(); ++i)
        if (p[(size_t) i].name == name) return i;
    return 0;
}

// ============================================================================ Envelope
void Envelope::setup (float a, float d, float s, float r, double sr) noexcept
{
    attackInc   = (float) (1.0 / (std::max (0.0005f, a) * sr));
    decayCoef   = (float) std::exp (-1.0 / (std::max (0.001f, d) * sr / 3.0));
    sustain     = juce::jlimit (0.0f, 1.0f, s);
    releaseCoef = (float) std::exp (-1.0 / (std::max (0.003f, r) * sr / 4.6));
}

float Envelope::next() noexcept
{
    switch (stage)
    {
        case Attack:
            value += attackInc;
            if (value >= 1.0f) { value = 1.0f; stage = Decay; }
            break;
        case Decay:
            value = sustain + (value - sustain) * decayCoef;
            if (sustain <= 0.0001f)
            {
                if (value < 1.0e-4f) { value = 0.0f; stage = Idle; }
            }
            else if (std::abs (value - sustain) < 1.0e-4f)
            {
                value = sustain; stage = Sustain;
            }
            break;
        case Sustain:
            value = sustain;
            break;
        case Release:
            value *= releaseCoef;
            if (value < 1.0e-4f) { value = 0.0f; stage = Idle; }
            break;
        case Idle:
        default:
            value = 0.0f;
            break;
    }
    return value;
}

// ============================================================================ Voice
void Voice::prepare (double sampleRate)
{
    sr = sampleRate;
    ks.assign ((size_t) (sampleRate / 25.0) + 8, 0.0f);
    amp.kill();
    fenv.kill();
}

void Voice::start (int n, int velocity, const Preset& preset, uint64_t ord)
{
    const bool wasActive = amp.isActive();
    p = &preset;
    note = n;
    order = ord;
    vel = 0.35f + 0.65f * juce::jlimit (0, 127, velocity) / 127.0f;
    freq = (float) (440.0 * std::pow (2.0, (n - 69) / 12.0));
    time = 0.0;
    released = false;
    rng = 0x9e3779b9u ^ (uint32_t) (ord * 2654435761u + (uint32_t) n);

    amp.setup (p->a, p->d, p->s, p->r, sr);
    fenv.setup (p->fA, p->fD, p->fS, p->fR, sr);
    if (! wasActive) { amp.value = 0.0f; ic1L = ic2L = ic1R = ic2R = 0.0f; }
    fenv.value = 0.0f;
    amp.noteOn();
    fenv.noteOn();

    const int U = juce::jlimit (1, kMaxUnison, p->unison);
    for (int u = 0; u < U; ++u)
    {
        const float pos = U > 1 ? (float) u / (float) (U - 1) * 2.0f - 1.0f : 0.0f;
        detMul[(size_t) u] = (float) std::pow (2.0, pos * p->unisonDetune / 1200.0);
        const float pan = pos * p->stereoSpread;
        const float ang = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        panL[(size_t) u] = std::cos (ang) * 1.41421356f;
        panR[(size_t) u] = std::sin (ang) * 1.41421356f;
        const bool randomPhase = p->w1 != Wave::Sine || U > 1;
        ph1[(size_t) u] = randomPhase ? (noiseSample() * 0.5 + 0.5) : 0.0;
        ph2[(size_t) u] = randomPhase ? (noiseSample() * 0.5 + 0.5) : 0.0;
    }
    subPh = fmPh = lfoPh = 0.0;

    if (p->karplus)
    {
        std::fill (ks.begin(), ks.end(), 0.0f);
        ksWrite = 0;
        ksPrev = ksLp = 0.0f;
        ksDelay = juce::jlimit (2.0, (double) ks.size() - 3.0, sr / freq - 0.5);
        ksDamp = (float) std::exp (std::log (0.001) / (freq * std::max (0.05f, p->ksDecaySec)));
        ksExcite = (int) (sr / freq);
    }
}

void Voice::release() noexcept
{
    released = true;
    amp.noteOff();
    fenv.noteOff();
}

void Voice::kill() noexcept
{
    released = true;
    amp.kill();
    fenv.kill();
}

void Voice::render (float* L, float* R, int numSamples) noexcept
{
    if (p == nullptr || ! amp.isActive())
        return;

    constexpr int chunk = 16;
    const int U = juce::jlimit (1, kMaxUnison, p->unison);
    const float norm = 1.0f / std::sqrt ((float) U);
    const double w2Mul = std::pow (2.0, p->w2Semis / 12.0);
    const bool useFilter = p->cutoff < 19000.0f || p->filterEnvOct != 0.0f;
    const float driveAmt = 1.0f + 3.0f * p->drive;
    const float driveNorm = 1.0f / std::tanh (driveAmt);

    for (int pos = 0; pos < numSamples; pos += chunk)
    {
        const int m = std::min (chunk, numSamples - pos);

        double semis = 0.0;
        if (p->pitchEnvSemis != 0.0f)
            semis += p->pitchEnvSemis * std::exp (-time / std::max (0.001f, p->pitchDecay));
        if (p->vibDepth > 0.0f)
        {
            const double ramp = std::min (1.0, time / 0.35);
            semis += p->vibDepth * ramp * std::sin (twoPi * lfoPh);
            lfoPh += p->vibRate * m / sr;
            if (lfoPh >= 1.0) lfoPh -= 1.0;
        }
        const double f = semis != 0.0 ? freq * std::pow (2.0, semis / 12.0) : (double) freq;
        const double baseInc = f / sr;

        float a1 = 0, a2 = 0, a3 = 0;
        if (useFilter)
        {
            const double oct = p->filterEnvOct * fenv.current() * (0.6 + 0.4 * vel) + p->keytrack * (note - 60) / 12.0;
            const double fc = juce::jlimit (20.0, sr * 0.45, (double) p->cutoff * std::pow (2.0, oct));
            const float g = (float) std::tan (juce::MathConstants<double>::pi * fc / sr);
            const float k = 2.0f - 2.0f * juce::jlimit (0.0f, 0.97f, p->reso);
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        const double fmIdx = p->fmIndex > 0.0f
                               ? p->fmSustainIndex + (p->fmIndex - p->fmSustainIndex) * std::exp (-time / std::max (0.001f, p->fmDecay))
                               : 0.0;
        const double fmInc = baseInc * p->fmRatio;

        for (int i = 0; i < m; ++i)
        {
            float l = 0.0f, r = 0.0f;

            if (p->karplus)
            {
                const int size = (int) ks.size();
                double rp = ksWrite - ksDelay;
                while (rp < 0.0) rp += size;
                const int i0 = (int) rp % size;
                const int i1 = (i0 + 1) % size;
                const float frac = (float) (rp - std::floor (rp));
                const float out = ks[(size_t) i0] + (ks[(size_t) i1] - ks[(size_t) i0]) * frac;
                float in = ksDamp * 0.5f * (out + ksPrev);
                ksPrev = out;
                if (ksExcite > 0)
                {
                    ksLp += (noiseSample() - ksLp) * (0.12f + 0.88f * p->ksBright);
                    in += ksLp * 0.8f;
                    --ksExcite;
                }
                ks[(size_t) ksWrite] = in;
                ksWrite = (ksWrite + 1) % size;
                l = r = out;
            }
            else
            {
                double pm = 0.0;
                if (fmIdx > 0.0)
                {
                    pm = fmIdx * std::sin (twoPi * fmPh);
                    advance (fmPh, fmInc);
                }
                for (int u = 0; u < U; ++u)
                {
                    const double inc = baseInc * detMul[(size_t) u];
                    float o = osc (p->w1, ph1[(size_t) u], inc, pm);
                    advance (ph1[(size_t) u], inc);
                    if (p->w2 != Wave::None)
                    {
                        const double inc2 = inc * w2Mul;
                        o += p->w2Level * osc (p->w2, ph2[(size_t) u], inc2, 0.0);
                        advance (ph2[(size_t) u], inc2);
                    }
                    l += o * panL[(size_t) u];
                    r += o * panR[(size_t) u];
                }
                l *= norm; r *= norm;

                if (p->sub > 0.0f)
                {
                    const float sv = (float) std::sin (twoPi * subPh) * p->sub;
                    advance (subPh, baseInc * 0.5);
                    l += sv; r += sv;
                }
                if (p->noise > 0.0f)
                {
                    const float nz = noiseSample() * p->noise;
                    l += nz; r += nz;
                }
            }

            if (useFilter)
            {
                {
                    const float v3 = l - ic2L;
                    const float v1 = a1 * ic1L + a2 * v3;
                    const float v2 = ic2L + a2 * ic1L + a3 * v3;
                    ic1L = 2.0f * v1 - ic1L; ic2L = 2.0f * v2 - ic2L;
                    l = v2;
                }
                {
                    const float v3 = r - ic2R;
                    const float v1 = a1 * ic1R + a2 * v3;
                    const float v2 = ic2R + a2 * ic1R + a3 * v3;
                    ic1R = 2.0f * v1 - ic1R; ic2R = 2.0f * v2 - ic2R;
                    r = v2;
                }
            }

            if (p->drive > 0.0f)
            {
                l = std::tanh (l * driveAmt) * driveNorm;
                r = std::tanh (r * driveAmt) * driveNorm;
            }

            const float e = amp.next();
            fenv.next();
            const float gg = e * vel * p->gain;
            L[pos + i] += l * gg;
            R[pos + i] += r * gg;
        }

        time += m / sr;
        if (! amp.isActive())
            break;
    }
}

// ============================================================================ TrackSynth
void TrackSynth::prepare (double sampleRate)
{
    for (auto& v : voices)
        v.prepare (sampleRate);
}

void TrackSynth::setPreset (int index) noexcept
{
    presetIdx = juce::jlimit (0, (int) getPresets().size() - 1, index);
}

void TrackSynth::noteOn (int note, int velocity) noexcept
{
    const auto& preset = getPresets()[(size_t) presetIdx];
    Voice* target = nullptr;

    for (auto& v : voices)
        if (v.isActive() && ! v.isReleased() && v.getNote() == note) { target = &v; break; }

    if (target == nullptr)
        for (auto& v : voices)
            if (! v.isActive()) { target = &v; break; }

    if (target == nullptr)
    {
        uint64_t oldest = UINT64_MAX;
        for (auto& v : voices)
            if (v.isReleased() && v.getOrder() < oldest) { oldest = v.getOrder(); target = &v; }
    }

    if (target == nullptr)
    {
        uint64_t oldest = UINT64_MAX;
        for (auto& v : voices)
            if (v.getOrder() < oldest) { oldest = v.getOrder(); target = &v; }
    }

    if (target != nullptr)
        target->start (note, velocity, preset, ++counter);
}

void TrackSynth::noteOff (int note) noexcept
{
    for (auto& v : voices)
        if (v.isActive() && ! v.isReleased() && v.getNote() == note)
            v.release();
}

void TrackSynth::allNotesOff (bool hard) noexcept
{
    for (auto& v : voices)
    {
        if (hard) v.kill();
        else if (v.isActive()) v.release();
    }
}

void TrackSynth::render (float* L, float* R, int numSamples) noexcept
{
    for (auto& v : voices)
        if (v.isActive())
            v.render (L, R, numSamples);
}

} // namespace melosumm
