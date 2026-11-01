#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace melosumm
{

enum TrackId { kMelody = 0, kChords = 1, kBass = 2, kNumTracks = 3 };

/** Eine Note in Beats (Viertelnoten). */
struct NoteEvent
{
    double start  = 0.0;
    double length = 0.5;
    int    pitch  = 60;
    int    velocity = 100;

    double end() const noexcept { return start + length; }
};

/** Eine erkannte Note direkt aus der Analyse, noch nicht quantisiert. */
struct RawNote
{
    double startSec = 0.0;
    double endSec   = 0.0;
    float  midi     = 60.0f;   // Tonhoehe mit Cent-Anteil
    float  level    = 0.0f;    // mittlere Lautstaerke (linear)
};

struct AnalysisResult
{
    std::vector<RawNote> notes;
    double bpm = 120.0;          // Tempo, mit dem aufgenommen wurde
    double durationSec = 0.0;
    double beatOffsetSec = 0.0;  // Zeit, die Beat 0 entspricht
};

struct ChordLabel
{
    double start = 0.0, length = 4.0;
    juce::String name;
};

/** Alles, was abgespielt / exportiert wird. Nach dem Bauen unveraenderlich. */
struct Pattern
{
    std::array<std::vector<NoteEvent>, kNumTracks> tracks;
    std::vector<ChordLabel> chords;
    double lengthBeats = 16.0;
    int    keyRoot = 0;
    bool   minor = false;
    int    scaleIndex = 0;
    juce::String keyName;

    struct Event
    {
        double beat;
        int track, pitch, velocity;
        bool on;
    };
    std::vector<Event> events;   // sortiert nach beat (Note-Off vor Note-On)

    void buildEvents();
    bool isEmpty() const noexcept;
};

// ------------------------------------------------------------------ Skalen
struct ScaleDef
{
    const char* name;
    std::vector<int> steps;
    bool minorFamily;
};

const std::vector<ScaleDef>& getScales();     // Index 0 = Dur, 1 = Moll ...
int  scaleIndexMajor();
int  scaleIndexMinor();

/** 7-toenige Skala fuer die Akkordbildung (Pentatonik/Blues/Chromatisch -> Dur/Moll). */
std::vector<int> harmonyScaleFor (int scaleIndex);

bool isInScale (int pitch, int root, const std::vector<int>& steps);
int  snapToScale (float midi, int root, const std::vector<int>& steps);

/** Krumhansl-Schmuckler Tonarterkennung. */
struct KeyGuess { int root = 0; bool minor = false; float confidence = 0.0f; };
KeyGuess detectKey (const std::vector<NoteEvent>& notes);

/** Ergebnis aus "Auto"-Einstellungen: rootParam/scaleParam = -1 bedeutet automatisch. */
struct ResolvedKey { int root = 0; int scaleIndex = 0; };
ResolvedKey resolveKey (const std::vector<NoteEvent>& notes, int rootParam, int scaleParam);

juce::String pitchClassName (int pc);                 // "C", "C#", ...
juce::String noteName (int midi);                     // FL-Studio-Zaehlung: 60 = C5
juce::String keyDisplayName (int root, int scaleIndex);

} // namespace melosumm
