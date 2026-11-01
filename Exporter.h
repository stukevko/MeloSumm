#pragma once

#include "Model.h"
#include "Player.h"

namespace melosumm
{

/** trackMask: Bit 0 = Melodie, Bit 1 = Akkorde, Bit 2 = Bass */
namespace Exporter
{
    constexpr int kAll = 0b111;

    juce::File exportFolder();
    juce::String fileBaseName (const Pattern& p, double bpm, int trackMask);

    bool writeMidi (const Pattern& p, double bpm, int trackMask, const juce::File& dest);

    /** Rendert die Loop zweimal und gibt den zweiten Durchlauf zurueck,
        damit Hall- und Ausklang-Fahnen nahtlos in den Loop-Anfang laufen. */
    juce::AudioBuffer<float> renderLoop (const Pattern& p, double bpm, PlayerSettings settings,
                                         int trackMask, double sampleRate);

    bool writeWav (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::File& dest);

    /** Erzeugt eine eindeutige Datei im Export-Ordner. */
    juce::File makeExportFile (const juce::String& baseName, const juce::String& extension);
}

} // namespace melosumm
