#include "Model.h"
#include <algorithm>
#include <cmath>

namespace melosumm
{

void Pattern::buildEvents()
{
    events.clear();
    const double L = lengthBeats;
    constexpr double eps = 1.0e-4;

    for (int t = 0; t < kNumTracks; ++t)
    {
        for (const auto& n : tracks[(size_t) t])
        {
            if (n.start >= L - eps || n.start < 0.0)
                continue;
            const double endB = std::min (n.end(), L - eps);
            if (endB <= n.start)
                continue;
            events.push_back ({ n.start, t, n.pitch, juce::jlimit (1, 127, n.velocity), true });
            events.push_back ({ endB, t, n.pitch, 0, false });
        }
    }

    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b)
    {
        if (a.beat != b.beat) return a.beat < b.beat;
        return (! a.on) && b.on; // Note-Off zuerst
    });
}

bool Pattern::isEmpty() const noexcept
{
    for (auto& t : tracks)
        if (! t.empty())
            return false;
    return true;
}

// ---------------------------------------------------------------------------
const std::vector<ScaleDef>& getScales()
{
    static const std::vector<ScaleDef> scales {
        { "Dur",              { 0, 2, 4, 5, 7, 9, 11 }, false },
        { "Moll",             { 0, 2, 3, 5, 7, 8, 10 }, true  },
        { "Harmonisch Moll",  { 0, 2, 3, 5, 7, 8, 11 }, true  },
        { "Melodisch Moll",   { 0, 2, 3, 5, 7, 9, 11 }, true  },
        { "Dorisch",          { 0, 2, 3, 5, 7, 9, 10 }, true  },
        { "Phrygisch",        { 0, 1, 3, 5, 7, 8, 10 }, true  },
        { "Lydisch",          { 0, 2, 4, 6, 7, 9, 11 }, false },
        { "Mixolydisch",      { 0, 2, 4, 5, 7, 9, 10 }, false },
        { "Pentatonik Dur",   { 0, 2, 4, 7, 9 },        false },
        { "Pentatonik Moll",  { 0, 3, 5, 7, 10 },       true  },
        { "Blues",            { 0, 3, 5, 6, 7, 10 },    true  },
        { "Chromatisch",      { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, false },
    };
    return scales;
}

int scaleIndexMajor() { return 0; }
int scaleIndexMinor() { return 1; }

std::vector<int> harmonyScaleFor (int scaleIndex)
{
    const auto& s = getScales();
    scaleIndex = juce::jlimit (0, (int) s.size() - 1, scaleIndex);
    if (s[(size_t) scaleIndex].steps.size() == 7)
        return s[(size_t) scaleIndex].steps;
    return s[(size_t) (s[(size_t) scaleIndex].minorFamily ? scaleIndexMinor() : scaleIndexMajor())].steps;
}

bool isInScale (int pitch, int root, const std::vector<int>& steps)
{
    const int pc = ((pitch - root) % 12 + 12) % 12;
    return std::find (steps.begin(), steps.end(), pc) != steps.end();
}

int snapToScale (float midi, int root, const std::vector<int>& steps)
{
    const int rounded = (int) std::lround (midi);
    if (isInScale (rounded, root, steps))
        return rounded;

    const int base = (int) std::floor (midi);
    int best = (int) std::lround (midi);
    float bestDist = 1.0e9f;
    for (int c = base - 3; c <= base + 4; ++c)
    {
        if (! isInScale (c, root, steps))
            continue;
        const float d = std::abs ((float) c - midi);
        if (d < bestDist) { bestDist = d; best = c; }
    }
    // Sauber getroffene "fremde" Toene sind meist Absicht -> nicht verbiegen
    return bestDist <= 0.8f ? best : rounded;
}

KeyGuess detectKey (const std::vector<NoteEvent>& notes)
{
    static const double majorProfile[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double minorProfile[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    double hist[12] = {};
    double total = 0.0;
    for (const auto& n : notes)
    {
        const double w = std::max (0.05, n.length);
        hist[((n.pitch % 12) + 12) % 12] += w;
        total += w;
    }
    if (total <= 0.0)
        return {};

    // Erste und letzte Note bekommen extra Gewicht (Grundton-Tendenz)
    if (! notes.empty())
    {
        hist[((notes.back().pitch % 12) + 12) % 12]  += total * 0.15;
        hist[((notes.front().pitch % 12) + 12) % 12] += total * 0.08;
    }

    auto corr = [&] (const double* profile, int root)
    {
        double mx = 0, my = 0;
        for (int i = 0; i < 12; ++i) { mx += hist[i]; my += profile[i]; }
        mx /= 12.0; my /= 12.0;
        double num = 0, dx = 0, dy = 0;
        for (int i = 0; i < 12; ++i)
        {
            const double x = hist[(i + root) % 12] - mx;
            const double y = profile[i] - my;
            num += x * y; dx += x * x; dy += y * y;
        }
        return (dx > 0 && dy > 0) ? num / std::sqrt (dx * dy) : 0.0;
    };

    // Anteil der Notendauer, der in die jeweilige Tonleiter passt
    static const int majorSteps[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int minorSteps[7] = { 0, 2, 3, 5, 7, 8, 10 };
    auto inScaleFraction = [&] (const int* steps, int root)
    {
        double in = 0.0, all = 0.0;
        for (int pc = 0; pc < 12; ++pc)
        {
            all += hist[pc];
            const int rel = (pc - root + 12) % 12;
            for (int k = 0; k < 7; ++k)
                if (steps[k] == rel) { in += hist[pc]; break; }
        }
        return all > 0.0 ? in / all : 0.0;
    };

    KeyGuess best;
    double bestScore = -2.0, second = -2.0;
    for (int r = 0; r < 12; ++r)
    {
        for (int m = 0; m < 2; ++m)
        {
            const double c = corr (m ? minorProfile : majorProfile, r)
                           + 1.5 * inScaleFraction (m ? minorSteps : majorSteps, r);
            if (c > bestScore) { second = bestScore; bestScore = c; best.root = r; best.minor = (m == 1); }
            else if (c > second) second = c;
        }
    }
    best.confidence = (float) juce::jlimit (0.0, 1.0, bestScore - second + 0.5);
    return best;
}

ResolvedKey resolveKey (const std::vector<NoteEvent>& notes, int rootParam, int scaleParam)
{
    ResolvedKey k;
    const auto guess = detectKey (notes);

    if (rootParam < 0)
    {
        k.root = guess.root;
        k.scaleIndex = scaleParam >= 0 ? scaleParam : (guess.minor ? scaleIndexMinor() : scaleIndexMajor());

        // Moll-Skala gewaehlt, aber Dur erkannt (oder umgekehrt): Paralleltonart nehmen
        if (scaleParam >= 0)
        {
            const bool wantMinor = getScales()[(size_t) scaleParam].minorFamily;
            if (wantMinor && ! guess.minor)      k.root = (guess.root + 9) % 12;
            else if (! wantMinor && guess.minor) k.root = (guess.root + 3) % 12;
        }
        return k;
    }

    k.root = rootParam % 12;
    if (scaleParam >= 0)
    {
        k.scaleIndex = scaleParam;
        return k;
    }

    // Grundton fest, Dur/Moll automatisch: welcher Modus passt besser?
    int inMajor = 0, inMinor = 0;
    const auto& maj = getScales()[(size_t) scaleIndexMajor()].steps;
    const auto& min = getScales()[(size_t) scaleIndexMinor()].steps;
    for (const auto& n : notes)
    {
        if (isInScale (n.pitch, k.root, maj)) ++inMajor;
        if (isInScale (n.pitch, k.root, min)) ++inMinor;
    }
    k.scaleIndex = (inMinor > inMajor || (inMinor == inMajor && guess.minor)) ? scaleIndexMinor() : scaleIndexMajor();
    return k;
}

juce::String pitchClassName (int pc)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((pc % 12) + 12) % 12];
}

juce::String noteName (int midi)
{
    // FL Studio: MIDI 60 = C5
    return pitchClassName (midi) + juce::String (midi / 12);
}

juce::String keyDisplayName (int root, int scaleIndex)
{
    const auto& s = getScales();
    scaleIndex = juce::jlimit (0, (int) s.size() - 1, scaleIndex);
    return pitchClassName (root) + " " + s[(size_t) scaleIndex].name;
}

} // namespace melosumm
