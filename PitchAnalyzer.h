#pragma once

#include "Model.h"
#include <atomic>

namespace melosumm
{

/** Erkennt aus einer (gesummten/gesungenen/gepfiffenen) Aufnahme einzelne Noten.
    Verfahren: YIN-Tonhoehenerkennung, Lautstaerke-Gate, Oktavfehler-Korrektur,
    Segmentierung nach Tonhoehenwechsel und neuen Einsaetzen. */
class PitchAnalyzer
{
public:
    static constexpr double analysisRate = 12000.0;

    struct Settings
    {
        float  sensitivity = 0.5f;   // 0 = streng (nur klare Toene), 1 = empfindlich
        double minNoteMs   = 70.0;   // kuerzere Noten werden verworfen
    };

    /** Filtert und resampelt Mono-Audio auf analysisRate. */
    static std::vector<float> toAnalysisRate (const float* samples, int numSamples, double sampleRate);

    static AnalysisResult analyze (const std::vector<float>& audioAtAnalysisRate,
                                   const Settings& settings,
                                   double bpm,
                                   const std::atomic<bool>* cancel = nullptr);
};

} // namespace melosumm
