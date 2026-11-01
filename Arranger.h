#pragma once

#include "Model.h"

namespace melosumm
{

struct ArrangeSettings
{
    int    keyRoot = 0;
    int    scaleIndex = 0;
    double loopLength = 16.0;     // Beats
    double chordRate = 4.0;       // Beats pro Akkord
    int    complexity = 0;        // 0 Dreiklaenge, 1 Septakkorde, 2 add9, 3 sus/Mix
    int    chordStyle = 0;        // siehe chordStyleNames()
    int    chordRegister = 1;     // 0 tief, 1 mittel, 2 hoch
    int    bassPattern = 0;       // siehe bassPatternNames()
    int    bassRegister = 1;      // 0 tief, 1 mittel, 2 hoch
    int    variation = 0;         // 0 = beste Passung, >0 = Varianten
    float  humanize = 0.0f;       // 0..1
};

struct ArrangeResult
{
    std::vector<NoteEvent> chords, bass;
    std::vector<ChordLabel> labels;
};

ArrangeResult arrange (const std::vector<NoteEvent>& melody, const ArrangeSettings& settings);

const juce::StringArray& chordStyleNames();
const juce::StringArray& bassPatternNames();
const juce::StringArray& complexityNames();
const juce::StringArray& chordRateNames();
double chordRateBeatsForIndex (int index);

} // namespace melosumm
