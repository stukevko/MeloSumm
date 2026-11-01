#include "Arranger.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace melosumm
{

const juce::StringArray& chordStyleNames()
{
    static const juce::StringArray n { "Halten (Pad)", "Stabs", "Achtel-Pulse", "Arpeggio hoch 1/16",
                                       "Arpeggio auf-ab 1/16", "Arpeggio 1/8", "Piano gebrochen", "Offbeat" };
    return n;
}

const juce::StringArray& bassPatternNames()
{
    static const juce::StringArray n { "Halten", "Halbe", "Viertel", "Achtel", "Oktaven",
                                       "Synkopiert", "808 Trap", "Walking" };
    return n;
}

const juce::StringArray& complexityNames()
{
    static const juce::StringArray n { "Dreiklaenge", "Septakkorde", "add9", "Volle 9er" };
    return n;
}

const juce::StringArray& chordRateNames()
{
    static const juce::StringArray n { "2 pro Takt", "1 pro Takt", "Alle 2 Takte" };
    return n;
}

double chordRateBeatsForIndex (int index)
{
    switch (index)
    {
        case 0:  return 2.0;
        case 2:  return 8.0;
        default: return 4.0;
    }
}

namespace
{
    struct Candidate
    {
        int degree = 0;
        bool borrowed = false;
        int rootPc = 0;
        std::vector<int> pcs;   // Grundton zuerst
        juce::String name;
        std::vector<int> triadPcs;
    };

    juce::String qualitySuffix (int third, int fifth, int seventh, int complexity, int ninth)
    {
        const bool major = third == 4 && fifth == 7;
        const bool minor = third == 3 && fifth == 7;
        const bool dim   = third == 3 && fifth == 6;
        const bool aug   = third == 4 && fifth == 8;

        if (complexity == 0 || (complexity == 2 && ninth != 2))
        {
            if (minor) return "m";
            if (dim)   return "dim";
            if (aug)   return "aug";
            return "";
        }
        if (complexity == 2)
        {
            if (minor) return "m(add9)";
            if (dim)   return "dim";
            if (aug)   return "aug(add9)";
            return "add9";
        }

        juce::String s;
        if (major)      s = seventh == 11 ? "maj7" : "7";
        else if (minor) s = seventh == 10 ? "m7" : "m(maj7)";
        else if (dim)   s = seventh == 10 ? "m7b5" : "dim7";
        else if (aug)   s = "aug(maj7)";

        if (complexity == 3 && ninth == 2 && ! dim)
        {
            if (s == "maj7") s = "maj9";
            else if (s == "7") s = "9";
            else if (s == "m7") s = "m9";
        }
        return s;
    }

    std::vector<Candidate> buildCandidates (int keyRoot, const std::vector<int>& steps, int complexity)
    {
        std::vector<Candidate> c;
        auto add = [&] (int degree, bool borrowed, int r, int third, int fifth, int seventh, int ninth)
        {
            Candidate k;
            k.degree = degree;
            k.borrowed = borrowed;
            k.rootPc = (keyRoot + r) % 12;
            const int R = k.rootPc;
            k.triadPcs = { R, (R + third) % 12, (R + fifth) % 12 };

            switch (complexity)
            {
                case 1:  k.pcs = { R, (R + third) % 12, (R + fifth) % 12, (R + seventh) % 12 }; break;
                case 2:  k.pcs = ninth == 2 ? std::vector<int> { R, (R + third) % 12, (R + fifth) % 12, (R + 2) % 12 }
                                            : k.triadPcs; break;
                case 3:  k.pcs = (ninth == 2 && ! (third == 3 && fifth == 6))
                                     ? std::vector<int> { R, (R + third) % 12, (R + seventh) % 12, (R + 2) % 12 }
                                     : std::vector<int> { R, (R + third) % 12, (R + fifth) % 12, (R + seventh) % 12 };
                         break;
                default: k.pcs = k.triadPcs; break;
            }
            k.name = pitchClassName (R) + qualitySuffix (third, fifth, seventh, complexity, ninth);
            c.push_back (k);
        };

        for (int d = 0; d < 7; ++d)
        {
            const int r = steps[(size_t) d];
            auto iv = [&] (int off) { return (steps[(size_t) ((d + off) % 7)] - r + 12) % 12; };
            add (d, false, r, iv (2), iv (4), iv (6), iv (1));
        }

        // In Moll zusaetzlich die Dur-Dominante (harmonisch Moll)
        const bool minorKey = steps[2] == 3;
        if (minorKey && steps[6] == 10)
            add (4, true, steps[4], 4, 7, 10, 2);

        return c;
    }

    double degreePrior (int degree, bool borrowed, bool minorKey)
    {
        if (borrowed) return 0.05;
        static const double majorPrior[7] = { 0.35, 0.10, 0.00, 0.25, 0.25, 0.25, -0.40 };
        static const double minorPrior[7] = { 0.35, -0.35, 0.15, 0.20, 0.05, 0.25, 0.20 };
        return minorKey ? minorPrior[degree] : majorPrior[degree];
    }

    std::vector<int> voiceChord (const std::vector<int>& pcs, int target, const std::vector<int>& prev)
    {
        std::vector<int> best;
        double bestScore = std::numeric_limits<double>::max();
        const int n = (int) pcs.size();

        for (int inv = 0; inv < n; ++inv)
        {
            for (int p0 = target - 16; p0 <= target + 4; ++p0)
            {
                if (((p0 % 12) + 12) % 12 != pcs[(size_t) inv])
                    continue;

                std::vector<int> v { p0 };
                for (int k = 1; k < n; ++k)
                {
                    const int pc = pcs[(size_t) ((inv + k) % n)];
                    int p = v.back() + 1;
                    while (((p % 12) + 12) % 12 != pc) ++p;
                    v.push_back (p);
                }

                double mean = 0.0;
                for (int p : v) mean += p;
                mean /= (double) v.size();

                double score = std::abs (mean - target) * 0.6;
                if (! prev.empty())
                {
                    double vl = 0.0;
                    for (int p : v)
                    {
                        int dmin = 99;
                        for (int q : prev) dmin = std::min (dmin, std::abs (p - q));
                        vl += dmin;
                    }
                    score += vl * 0.5;
                }
                if (v.back() - v.front() > 14) score += 2.0;

                if (score < bestScore) { bestScore = score; best = v; }
            }
        }
        return best;
    }

    struct BassEl { double off, len; int tone; int vel; };

    std::vector<BassEl> bassElements (int pattern)
    {
        switch (pattern)
        {
            case 1: return { { 0, 1.9, 0, 105 }, { 2, 1.9, 0, 95 } };
            case 2: return { { 0, .9, 0, 105 }, { 1, .9, 0, 90 }, { 2, .9, 0, 98 }, { 3, .9, 0, 90 } };
            case 3: { std::vector<BassEl> e; for (int i = 0; i < 8; ++i) e.push_back ({ i * .5, .45, 0, i % 2 ? 85 : 105 }); return e; }
            case 4: { std::vector<BassEl> e; for (int i = 0; i < 8; ++i) e.push_back ({ i * .5, .45, i % 2, i % 2 ? 88 : 108 }); return e; }
            case 5: return { { 0, 1.25, 0, 110 }, { 1.5, .5, 0, 90 }, { 2, 1.0, 2, 100 }, { 3.5, .5, 1, 92 } };
            case 6: return { { 0, 2.5, 0, 115 }, { 2.75, .6, 0, 100 }, { 3.5, .5, 1, 95 } };
            case 7: return { { 0, .95, 0, 105 }, { 1, .95, 3, 92 }, { 2, .95, 2, 98 }, { 3, .95, 4, 90 } };
            default: return {};
        }
    }
}

ArrangeResult arrange (const std::vector<NoteEvent>& melody, const ArrangeSettings& s)
{
    ArrangeResult out;

    const auto steps = harmonyScaleFor (s.scaleIndex);
    const bool minorKey = steps[2] == 3;
    const auto cands = buildCandidates (s.keyRoot, steps, s.complexity);
    const int C = (int) cands.size();

    const double L = std::max (1.0, s.loopLength);
    const double rate = std::max (1.0, s.chordRate);
    const int S = std::max (1, (int) std::ceil (L / rate - 1.0e-6));

    juce::Random rng (s.variation * 7919 + 17);

    // ------------------------------------------------ Melodie-Passung je Abschnitt
    std::vector<std::vector<double>> score ((size_t) S, std::vector<double> ((size_t) C, 0.0));
    std::vector<bool> hasMelody ((size_t) S, false);

    for (int seg = 0; seg < S; ++seg)
    {
        const double t0 = seg * rate, t1 = std::min (L, t0 + rate);
        double totalW = 0.0;
        std::vector<double> raw ((size_t) C, 0.0);

        for (const auto& n : melody)
        {
            const double ov = std::min (t1, n.end()) - std::max (t0, n.start);
            if (ov <= 0.0) continue;

            double w = ov;
            const double posInSeg = n.start - t0;
            if (posInSeg >= -1.0e-6 && std::fmod (posInSeg + 1.0e-6, 2.0) < 0.01) w *= 1.6;   // betonte Zaehlzeit
            if (posInSeg >= -1.0e-6 && posInSeg < 0.01) w *= 1.3;                            // Akkordanfang
            totalW += w;

            const int pc = ((n.pitch % 12) + 12) % 12;
            const bool inScale = isInScale (n.pitch, s.keyRoot, steps);

            for (int c = 0; c < C; ++c)
            {
                const auto& k = cands[(size_t) c];
                if (pc == k.rootPc)                                          raw[(size_t) c] += w * 1.25;
                else if (std::find (k.triadPcs.begin(), k.triadPcs.end(), pc) != k.triadPcs.end()) raw[(size_t) c] += w * 1.0;
                else if (std::find (k.pcs.begin(), k.pcs.end(), pc) != k.pcs.end())               raw[(size_t) c] += w * 0.55;
                else if (inScale)                                            raw[(size_t) c] -= w * 0.3;
                else                                                         raw[(size_t) c] -= w * 0.7;
            }
        }

        hasMelody[(size_t) seg] = totalW > 0.0;
        for (int c = 0; c < C; ++c)
        {
            double v = totalW > 0.0 ? 2.2 * raw[(size_t) c] / totalW : 0.0;
            v += degreePrior (cands[(size_t) c].degree, cands[(size_t) c].borrowed, minorKey);
            if (seg == 0 && cands[(size_t) c].degree == 0 && ! cands[(size_t) c].borrowed) v += 0.45;
            if (s.variation > 0) v += (rng.nextDouble() - 0.5) * 1.1;
            score[(size_t) seg][(size_t) c] = v;
        }
    }

    auto transition = [&] (int a, int b, int seg)
    {
        const auto& A = cands[(size_t) a];
        const auto& B = cands[(size_t) b];
        double t = 0.0;
        if (a == b) t -= hasMelody[(size_t) seg] ? 0.35 : 0.15;
        const int mv = ((B.rootPc - A.rootPc) % 12 + 12) % 12;
        if (mv == 5) t += 0.22;                 // Quartschritt aufwaerts / Quintfall
        if (mv == 8 || mv == 9) t += 0.08;      // Terzfall
        if (A.degree == 4 && B.degree == 0) t += 0.3;  // Dominante -> Tonika
        if (A.degree == 6 && B.degree == 0 && ! minorKey) t += 0.15;
        return t;
    };

    // ------------------------------------------------ Viterbi
    std::vector<std::vector<double>> dp ((size_t) S, std::vector<double> ((size_t) C, 0.0));
    std::vector<std::vector<int>> back ((size_t) S, std::vector<int> ((size_t) C, 0));

    for (int c = 0; c < C; ++c)
        dp[0][(size_t) c] = score[0][(size_t) c];

    for (int seg = 1; seg < S; ++seg)
    {
        for (int c = 0; c < C; ++c)
        {
            double best = -1.0e18; int arg = 0;
            for (int p = 0; p < C; ++p)
            {
                const double v = dp[(size_t) seg - 1][(size_t) p] + transition (p, c, seg);
                if (v > best) { best = v; arg = p; }
            }
            dp[(size_t) seg][(size_t) c] = best + score[(size_t) seg][(size_t) c];
            back[(size_t) seg][(size_t) c] = arg;
        }
    }

    // Letzter Akkord soll gut zurueck zum ersten fuehren (Loop)
    int last = 0;
    {
        double best = -1.0e18;
        for (int c = 0; c < C; ++c)
        {
            double v = dp[(size_t) S - 1][(size_t) c];
            if (S > 1)
            {
                const int deg = cands[(size_t) c].degree;
                if (deg == 4 || deg == 3 || (minorKey && deg == 6)) v += 0.15;
            }
            if (v > best) { best = v; last = c; }
        }
    }

    std::vector<int> chosen ((size_t) S, 0);
    chosen[(size_t) S - 1] = last;
    for (int seg = S - 1; seg > 0; --seg)
        chosen[(size_t) seg - 1] = back[(size_t) seg][(size_t) chosen[(size_t) seg]];

    // ------------------------------------------------ Noten erzeugen
    const int chordTarget = 50 + s.chordRegister * 8;   // 50 / 58 / 66
    const int bassLow = 31 + (s.bassRegister - 1) * 12;
    std::vector<int> prevVoicing;

    const float hz = juce::jlimit (0.0f, 1.0f, s.humanize);
    auto hv = [&] (int v) { return juce::jlimit (1, 127, v + (int) std::lround ((rng.nextFloat() - 0.5f) * 30.0f * hz)); };
    auto ht = [&] (double t) { return t <= 0.0 ? t : std::max (0.0, t + (rng.nextDouble() - 0.5) * 0.04 * hz); };

    auto addNote = [&] (std::vector<NoteEvent>& dst, double start, double len, int pitch, int vel, double segEnd)
    {
        if (start >= segEnd - 1.0e-6) return;
        len = std::min (len, segEnd - start);
        if (len <= 0.01) return;
        dst.push_back ({ ht (start), len, juce::jlimit (0, 127, pitch), hv (vel) });
    };

    for (int seg = 0; seg < S; ++seg)
    {
        const auto& k = cands[(size_t) chosen[(size_t) seg]];
        const double t0 = seg * rate, t1 = std::min (L, t0 + rate);

        // Akkord-Beschriftung
        if (! out.labels.empty() && out.labels.back().name == k.name
            && std::abs (out.labels.back().start + out.labels.back().length - t0) < 1.0e-6)
            out.labels.back().length += (t1 - t0);
        else
            out.labels.push_back ({ t0, t1 - t0, k.name });

        const auto v = voiceChord (k.pcs, chordTarget, prevVoicing);
        prevVoicing = v;
        const int n = (int) v.size();

        std::vector<int> arpUp (v);
        for (int p : v) arpUp.push_back (p + 12);

        switch (s.chordStyle)
        {
            case 1: // Stabs
                for (double bar = std::floor (t0 / 4.0) * 4.0; bar < t1; bar += 4.0)
                {
                    static const double offs[3] = { 0.0, 1.5, 3.0 };
                    static const int vels[3] = { 105, 90, 96 };
                    for (int k = 0; k < 3; ++k)
                        if (bar + offs[k] >= t0 - 1.0e-6)
                            for (int p : v) addNote (out.chords, bar + offs[k], 0.5, p, vels[k], t1);
                }
                break;

            case 2: // Achtel-Pulse
                for (double t = t0; t < t1 - 1.0e-6; t += 0.5)
                    for (int p : v) addNote (out.chords, t, 0.42, p, std::fmod (t, 1.0) < 0.01 ? 104 : 84, t1);
                break;

            case 3: case 4: case 5: // Arpeggios
            {
                const double step = s.chordStyle == 5 ? 0.5 : 0.25;
                std::vector<int> seq (arpUp);
                if (s.chordStyle == 4)
                    for (int i = (int) arpUp.size() - 2; i >= 1; --i) seq.push_back (arpUp[(size_t) i]);
                int idx = 0;
                for (double t = t0; t < t1 - 1.0e-6; t += step, ++idx)
                    addNote (out.chords, t, step * 0.95, seq[(size_t) (idx % (int) seq.size())],
                             std::fmod (t, 1.0) < 0.01 ? 100 : 82, t1);
                break;
            }

            case 6: // Piano gebrochen
            {
                for (double bar = std::floor (t0 / 4.0) * 4.0; bar < t1; bar += 4.0)
                {
                    if (bar >= t0 - 1.0e-6) addNote (out.chords, bar, 4.0, v[0] - 12, 100, t1);
                    static const int order[8] = { 0, 1, 2, 3, 2, 1, 2, 3 };
                    for (int i = 1; i < 8; ++i)
                    {
                        const double t = bar + i * 0.5;
                        if (t < t0 - 1.0e-6) continue;
                        addNote (out.chords, t, 0.5, v[(size_t) (order[i] % n)], i % 2 ? 78 : 90, t1);
                    }
                }
                break;
            }

            case 7: // Offbeat
                for (double t = t0 + 0.5; t < t1 - 1.0e-6; t += 1.0)
                    for (int p : v) addNote (out.chords, t, 0.35, p, 96, t1);
                break;

            default: // Halten
                for (int p : v) addNote (out.chords, t0, t1 - t0, p, 88, t1);
                break;
        }

        // ---------------- Bass
        const int root = bassLow + ((k.rootPc - bassLow) % 12 + 12) % 12;
        const int third = root + ((k.triadPcs[1] - k.rootPc + 12) % 12);
        const int fifth = root + 7 <= bassLow + 14 ? root + 7 : root - 5;
        const auto& nextK = cands[(size_t) chosen[(size_t) ((seg + 1) % S)]];
        const int nextRoot = bassLow + ((nextK.rootPc - bassLow) % 12 + 12) % 12;
        const int approach = nextRoot > root ? nextRoot - 1 : (nextRoot < root ? nextRoot + 1 : root + 2);

        auto toneOf = [&] (int tone)
        {
            switch (tone)
            {
                case 1: return root + 12;
                case 2: return fifth;
                case 3: return third;
                case 4: return approach;
                default: return root;
            }
        };

        if (s.bassPattern == 0)
        {
            addNote (out.bass, t0, t1 - t0, root, 105, t1);
        }
        else
        {
            const auto els = bassElements (s.bassPattern);
            for (double bar = std::floor (t0 / 4.0) * 4.0; bar < t1; bar += 4.0)
                for (const auto& e : els)
                    if (bar + e.off >= t0 - 1.0e-6)
                        addNote (out.bass, bar + e.off, e.len, toneOf (e.tone), e.vel, t1);
        }
    }

    auto sortNotes = [] (std::vector<NoteEvent>& v)
    {
        std::stable_sort (v.begin(), v.end(), [] (const NoteEvent& a, const NoteEvent& b) { return a.start < b.start; });
    };
    sortNotes (out.chords);
    sortNotes (out.bass);
    return out;
}

} // namespace melosumm
