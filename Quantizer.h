#pragma once

#include "Model.h"

namespace melosumm
{

struct QuantizeSettings
{
    double gridBeats = 0.25;     // 0 = aus
    float  strength  = 1.0f;     // 0..1
    bool   snapToScale = true;
    int    keyRoot   = -1;       // -1 = automatisch
    int    scaleIndex = -1;      // -1 = automatisch
    bool   trimLeadingBars = true;
};

/** Macht aus erkannten Rohnoten saubere, quantisierte MIDI-Noten (Beats). */
std::vector<NoteEvent> quantizeMelody (const AnalysisResult& analysis, const QuantizeSettings& settings);

/** Raster-Wert (Beats) fuer den Parameter-Index. */
double gridBeatsForIndex (int index);
const juce::StringArray& gridNames();

} // namespace melosumm
