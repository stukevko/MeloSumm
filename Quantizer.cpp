#include "Quantizer.h"
#include <algorithm>
#include <cmath>

namespace melosumm
{

const juce::StringArray& gridNames()
{
    static const juce::StringArray names { "Aus", "1/4", "1/8", "1/8 Triole", "1/16", "1/16 Triole", "1/32" };
    return names;
}

double gridBeatsForIndex (int index)
{
    switch (index)
    {
        case 1: return 1.0;
        case 2: return 0.5;
        case 3: return 1.0 / 3.0;
        case 4: return 0.25;
        case 5: return 1.0 / 6.0;
        case 6: return 0.125;
        default: return 0.0;
    }
}

std::vector<NoteEvent> quantizeMelody (const AnalysisResult& analysis, const QuantizeSettings& s)
{
    std::vector<NoteEvent> out;
    if (analysis.notes.empty())
        return out;

    const double beatsPerSec = analysis.bpm / 60.0;

    float maxLevel = 1.0e-9f;
    for (const auto& r : analysis.notes)
        maxLevel = std::max (maxLevel, r.level);

    for (const auto& r : analysis.notes)
    {
        NoteEvent n;
        n.start  = (r.startSec - analysis.beatOffsetSec) * beatsPerSec;
        n.length = std::max (0.01, (r.endSec - r.startSec) * beatsPerSec);
        n.pitch  = juce::jlimit (0, 127, (int) std::lround (r.midi));
        n.velocity = juce::jlimit (40, 127, (int) std::lround (64.0 + 58.0 * std::sqrt (r.level / maxLevel)));
        out.push_back (n);
    }

    // ------------------------------------------------ Tonhoehe auf Skala
    if (s.snapToScale)
    {
        const auto key = resolveKey (out, s.keyRoot, s.scaleIndex);
        const auto& steps = getScales()[(size_t) key.scaleIndex].steps;
        for (size_t i = 0; i < out.size(); ++i)
            out[i].pitch = juce::jlimit (0, 127, snapToScale (analysis.notes[i].midi, key.root, steps));
    }

    // ------------------------------------------------ Zeit quantisieren
    const double g = s.gridBeats;
    const double str = juce::jlimit (0.0, 1.0, (double) s.strength);
    if (g > 0.0 && str > 0.0)
    {
        for (auto& n : out)
        {
            const double qs = std::round (n.start / g) * g;
            double qe = std::round (n.end() / g) * g;
            if (qe <= qs) qe = qs + g;
            const double ns = n.start + (qs - n.start) * str;
            const double ne = n.end() + (qe - n.end()) * str;
            n.start = ns;
            n.length = std::max (g * 0.5, ne - ns);
        }
    }

    // ------------------------------------------------ einstimmig machen
    std::stable_sort (out.begin(), out.end(), [] (const NoteEvent& a, const NoteEvent& b) { return a.start < b.start; });

    std::vector<NoteEvent> mono;
    for (const auto& n : out)
    {
        if (! mono.empty() && std::abs (mono.back().start - n.start) < 1.0e-6)
        {
            // gleicher Startpunkt: laengere Note behalten
            if (n.length > mono.back().length) mono.back() = n;
            continue;
        }
        mono.push_back (n);
    }
    for (size_t i = 0; i + 1 < mono.size(); ++i)
        if (mono[i].end() > mono[i + 1].start)
            mono[i].length = std::max (0.01, mono[i + 1].start - mono[i].start);

    // ------------------------------------------------ Start auf Takt 1
    if (s.trimLeadingBars && ! mono.empty())
    {
        const double shift = std::floor (std::max (0.0, mono.front().start) / 4.0) * 4.0;
        for (auto& n : mono)
            n.start -= shift;
    }

    // Noten vor Beat 0 (z. B. durch Vorziehen beim Quantisieren) leicht korrigieren
    mono.erase (std::remove_if (mono.begin(), mono.end(), [] (const NoteEvent& n) { return n.end() <= 0.0; }), mono.end());
    for (auto& n : mono)
        if (n.start < 0.0) { n.length += n.start; n.start = 0.0; }

    return mono;
}

} // namespace melosumm
