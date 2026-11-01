#include "PitchAnalyzer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>

namespace melosumm
{

namespace
{
    float medianOf (std::vector<float> v)
    {
        if (v.empty()) return 0.0f;
        const size_t mid = v.size() / 2;
        std::nth_element (v.begin(), v.begin() + (long) mid, v.end());
        return v[mid];
    }
}

std::vector<float> PitchAnalyzer::toAnalysisRate (const float* samples, int numSamples, double sampleRate)
{
    if (samples == nullptr || numSamples <= 0 || sampleRate <= 0.0)
        return {};

    std::vector<float> in (samples, samples + numSamples);

    juce::IIRFilter hp, lp1, lp2, lp3;
    hp.setCoefficients  (juce::IIRCoefficients::makeHighPass (sampleRate, 55.0));
    const double lpFreq = std::min (4500.0, sampleRate * 0.45);
    lp1.setCoefficients (juce::IIRCoefficients::makeLowPass (sampleRate, lpFreq, 0.54));
    lp2.setCoefficients (juce::IIRCoefficients::makeLowPass (sampleRate, lpFreq, 1.31));
    lp3.setCoefficients (juce::IIRCoefficients::makeLowPass (sampleRate, lpFreq, 0.71));
    hp.processSamples  (in.data(), numSamples);
    lp1.processSamples (in.data(), numSamples);
    lp2.processSamples (in.data(), numSamples);
    lp3.processSamples (in.data(), numSamples);

    if (std::abs (sampleRate - analysisRate) < 0.5)
        return in;

    const double ratio = sampleRate / analysisRate;
    const int outN = (int) std::floor ((double) numSamples / ratio) - 4;
    if (outN <= 0)
        return {};

    std::vector<float> out ((size_t) outN);
    juce::LagrangeInterpolator interp;
    interp.process (ratio, in.data(), out.data(), outN);
    return out;
}

AnalysisResult PitchAnalyzer::analyze (const std::vector<float>& audio,
                                       const Settings& settings,
                                       double bpm,
                                       const std::atomic<bool>* cancel)
{
    const double fs = analysisRate;
    constexpr int W = 480;       // 40 ms Fenster
    constexpr int H = 60;        // 5 ms Schritt
    constexpr int tauMax = 300;  // 40 Hz
    constexpr int tauMin = 8;    // 1500 Hz

    AnalysisResult res;
    res.bpm = bpm;
    const int N = (int) audio.size();
    res.durationSec = N / fs;

    if (N < W + tauMax + H * 4)
        return res;

    const int F = (N - W - tauMax) / H + 1;
    const float sens = juce::jlimit (0.0f, 1.0f, settings.sensitivity);

    std::vector<float> db ((size_t) F, -120.0f), midi ((size_t) F, 0.0f);
    std::vector<char> voiced ((size_t) F, 0);

    // ---------------------------------------------------------- Lautstaerke
    for (int i = 0; i < F; ++i)
    {
        const float* x = audio.data() + (size_t) i * H;
        double sum = 0.0;
        for (int j = 0; j < W; ++j)
            sum += (double) x[j] * x[j];
        const double rms = std::sqrt (sum / W);
        db[(size_t) i] = (float) (20.0 * std::log10 (rms + 1.0e-9));
    }

    std::vector<float> sortedDb (db);
    std::sort (sortedDb.begin(), sortedDb.end());
    const float peak = sortedDb[(size_t) std::min (F - 1, (int) (F * 0.97))];
    const float gate = std::max (peak - (18.0f + sens * 30.0f), -66.0f);
    const float yinThresh = 0.10f + sens * 0.14f;

    // ---------------------------------------------------------- YIN
    std::vector<double> d ((size_t) tauMax + 2, 0.0), cmnd ((size_t) tauMax + 2, 1.0);

    for (int i = 0; i < F; ++i)
    {
        if (cancel != nullptr && cancel->load())
            return res;

        if (db[(size_t) i] < gate)
            continue;

        const float* x = audio.data() + (size_t) i * H;

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0.0;
            const float* y = x + tau;
            for (int j = 0; j < W; ++j)
            {
                const double diff = (double) x[j] - y[j];
                s += diff * diff;
            }
            d[(size_t) tau] = s;
        }

        double running = 0.0;
        cmnd[0] = 1.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += d[(size_t) tau];
            cmnd[(size_t) tau] = running > 0.0 ? d[(size_t) tau] * tau / running : 1.0;
        }

        int found = -1;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < yinThresh)
            {
                while (tau + 1 <= tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau])
                    ++tau;
                found = tau;
                break;
            }
        }

        if (found < 0)
        {
            int best = tauMin;
            for (int tau = tauMin; tau <= tauMax; ++tau)
                if (cmnd[(size_t) tau] < cmnd[(size_t) best])
                    best = tau;
            if (cmnd[(size_t) best] < std::min (0.42, yinThresh * 1.8))
                found = best;
        }

        if (found < 0)
            continue;

        double betterTau = found;
        if (found > tauMin && found < tauMax)
        {
            const double s0 = cmnd[(size_t) found - 1], s1 = cmnd[(size_t) found], s2 = cmnd[(size_t) found + 1];
            const double denom = s0 - 2.0 * s1 + s2;
            if (std::abs (denom) > 1.0e-12)
                betterTau = found + juce::jlimit (-1.0, 1.0, 0.5 * (s0 - s2) / denom);
        }

        const double f = fs / betterTau;
        if (f < 45.0 || f > 1500.0)
            continue;

        midi[(size_t) i] = (float) (69.0 + 12.0 * std::log2 (f / 440.0));
        voiced[(size_t) i] = 1;
    }

    // ---------------------------------------------------------- Aufraeumen
    // Sehr kurze stimmhafte Inseln entfernen
    for (int i = 0; i < F;)
    {
        if (! voiced[(size_t) i]) { ++i; continue; }
        int j = i;
        while (j < F && voiced[(size_t) j]) ++j;
        if (j - i < 3)
            for (int k = i; k < j; ++k) voiced[(size_t) k] = 0;
        i = j;
    }

    // Oktavfehler korrigieren
    {
        std::vector<float> fixedMidi (midi);
        std::vector<float> win;
        for (int i = 0; i < F; ++i)
        {
            if (! voiced[(size_t) i]) continue;
            win.clear();
            for (int k = std::max (0, i - 15); k <= std::min (F - 1, i + 15); ++k)
                if (voiced[(size_t) k]) win.push_back (midi[(size_t) k]);
            const float m = medianOf (win);
            const float diff = midi[(size_t) i] - m;
            if (diff > 9.0f && diff < 15.0f)        fixedMidi[(size_t) i] -= 12.0f;
            else if (diff < -9.0f && diff > -15.0f) fixedMidi[(size_t) i] += 12.0f;
        }
        midi.swap (fixedMidi);
    }

    // Median-Glaettung (5) innerhalb stimmhafter Abschnitte
    {
        std::vector<float> sm (midi);
        std::vector<float> win;
        for (int i = 0; i < F; ++i)
        {
            if (! voiced[(size_t) i]) continue;
            win.clear();
            for (int k = i - 2; k <= i + 2; ++k)
                if (k >= 0 && k < F && voiced[(size_t) k]) win.push_back (midi[(size_t) k]);
            sm[(size_t) i] = medianOf (win);
        }
        midi.swap (sm);
    }

    // ---------------------------------------------------------- Segmentierung
    struct Seg { int a = 0, b = 0; bool hardStart = false; };
    std::vector<Seg> segs;

    const int minFrames = std::max (3, (int) std::round (settings.minNoteMs * 0.001 * fs / H));
    const float onsetDb = 9.0f - sens * 3.0f;

    bool open = false;
    Seg cur;
    std::vector<float> curVals;
    int devCount = 0;
    int lastOnset = -1000;

    auto closeAt = [&] (int b)
    {
        cur.b = b;
        if (cur.b > cur.a)
            segs.push_back (cur);
        open = false;
        curVals.clear();
        devCount = 0;
    };

    for (int i = 0; i < F; ++i)
    {
        if (! voiced[(size_t) i])
        {
            if (open) closeAt (i);
            continue;
        }

        if (! open)
        {
            open = true;
            cur = {};
            cur.a = i;
            curVals.clear();
            curVals.push_back (midi[(size_t) i]);
            devCount = 0;
            continue;
        }

        // Neuer Einsatz (z. B. "da-da-da" auf gleicher Tonhoehe)?
        float minPrev = db[(size_t) i];
        for (int k = std::max (cur.a, i - 6); k < i; ++k)
            minPrev = std::min (minPrev, db[(size_t) k]);

        if (i - cur.a >= minFrames && i - lastOnset >= minFrames && db[(size_t) i] - minPrev > onsetDb)
        {
            closeAt (i);
            open = true;
            cur = {};
            cur.a = i;
            cur.hardStart = true;
            curVals.push_back (midi[(size_t) i]);
            lastOnset = i;
            continue;
        }

        // Tonhoehenwechsel?
        const int skip = (int) curVals.size() > 8 ? (int) curVals.size() / 5 : 0;
        const float ref = medianOf (std::vector<float> (curVals.begin() + skip, curVals.end()));
        const float dev = std::abs (midi[(size_t) i] - ref);

        if (dev > 0.75f) ++devCount; else devCount = 0;

        if (devCount >= 4)
        {
            const int splitAt = i - 3;
            closeAt (splitAt);
            open = true;
            cur = {};
            cur.a = splitAt;
            cur.hardStart = true;
            for (int k = splitAt; k <= i; ++k)
                curVals.push_back (midi[(size_t) k]);
            continue;
        }

        curVals.push_back (midi[(size_t) i]);
    }
    if (open) closeAt (F);

    // ---------------------------------------------------------- Noten bauen
    auto notePitch = [&] (const Seg& s)
    {
        const int len = s.b - s.a;
        const int from = len >= 6 ? s.a + len / 5 : s.a;
        std::vector<float> v;
        for (int k = from; k < s.b; ++k)
            v.push_back (midi[(size_t) k]);
        return medianOf (v);
    };

    // Kleine Aussetzer innerhalb derselben Note zusammenfuehren
    std::vector<Seg> merged;
    for (const auto& s : segs)
    {
        if (! merged.empty())
        {
            auto& p = merged.back();
            const int gap = s.a - p.b;
            if (gap > 0 && gap <= 5 && std::lround (notePitch (p)) == std::lround (notePitch (s)))
            {
                p.b = s.b;
                continue;
            }
        }
        merged.push_back (s);
    }

    const double hopSec = H / fs;
    const double halfWin = (W * 0.5) / fs;

    for (const auto& s : merged)
    {
        if (s.b - s.a < minFrames)
            continue;

        RawNote n;
        n.startSec = s.a * hopSec + halfWin - hopSec * 0.5;
        n.endSec   = (s.b - 1) * hopSec + halfWin + hopSec * 0.5;
        n.midi     = notePitch (s);

        double lvl = 0.0;
        for (int k = s.a; k < s.b; ++k)
            lvl += std::pow (10.0, db[(size_t) k] / 20.0);
        n.level = (float) (lvl / (s.b - s.a));

        res.notes.push_back (n);
    }

    return res;
}

} // namespace melosumm
