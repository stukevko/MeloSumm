#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Model.h"

class MeloSummProcessor;

namespace melosumm::ui
{

/** Zeigt Melodie, Akkorde und Bass. Die Melodie kann direkt bearbeitet werden:
    Klick = Note setzen, Ziehen = verschieben, rechter Rand = Laenge,
    Rechtsklick / Doppelklick = loeschen, Mausrad = scrollen, Strg+Mausrad = zoomen. */
class PianoRoll : public juce::Component
{
public:
    explicit PianoRoll (MeloSummProcessor& p);

    void refresh();
    void setPlayhead (double beat, bool playing);
    void setTrackVisible (int track, bool visible);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::Rectangle<float> gridArea() const;
    float beatToX (double beat) const;
    double xToBeat (float x) const;
    float pitchToY (int pitch) const;
    int yToPitch (float y) const;
    float rowHeight() const;
    juce::Rectangle<float> noteRect (const NoteEvent& n, int displayPitch) const;
    int hitTestMelody (juce::Point<float> p, bool& onRightEdge) const;
    double snap (double beat) const;
    void autoFitPitchRange();

    MeloSummProcessor& proc;
    std::shared_ptr<const Pattern> pattern;
    std::vector<NoteEvent> work;   // Melodie (untransponiert) waehrend der Bearbeitung
    std::array<bool, kNumTracks> visible { { true, true, true } };

    int lowestPitch = 48;
    int numRows = 36;
    bool userScrolled = false;

    double playBeat = 0.0;
    bool isPlaying = false;

    enum class Drag { None, Move, Resize };
    Drag drag = Drag::None;
    int dragIndex = -1;
    NoteEvent dragOrig;
    double dragStartBeat = 0.0;
    int dragStartPitch = 0;
    int lastPreviewPitch = -1;
    bool changed = false;
    double lastLength = 0.5;
    std::vector<NoteEvent> before;

    static constexpr float keyWidth = 48.0f;
    static constexpr float rulerHeight = 20.0f;
    static constexpr float chordLaneHeight = 22.0f;
};

} // namespace melosumm::ui
