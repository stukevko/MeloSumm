#include "PianoRoll.h"
#include <algorithm>
#include "LookAndFeel.h"
#include "PluginProcessor.h"
#include <cmath>

namespace melosumm::ui
{

PianoRoll::PianoRoll (MeloSummProcessor& p) : proc (p)
{
    setOpaque (true);
    refresh();
}

void PianoRoll::refresh()
{
    pattern = proc.getPattern();
    if (drag == Drag::None)
        work = proc.getMelody();
    if (! userScrolled)
        autoFitPitchRange();
    repaint();
}

void PianoRoll::setPlayhead (double beat, bool playing)
{
    if (std::abs (beat - playBeat) > 1.0e-4 || playing != isPlaying)
    {
        playBeat = beat;
        isPlaying = playing;
        repaint();
    }
}

void PianoRoll::setTrackVisible (int track, bool v)
{
    visible[(size_t) track] = v;
    repaint();
}

//==============================================================================
juce::Rectangle<float> PianoRoll::gridArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (keyWidth).withTrimmedTop (rulerHeight + chordLaneHeight);
}

float PianoRoll::rowHeight() const
{
    return gridArea().getHeight() / (float) numRows;
}

float PianoRoll::beatToX (double beat) const
{
    const auto a = gridArea();
    const double L = pattern ? pattern->lengthBeats : 16.0;
    return a.getX() + (float) (beat / L) * a.getWidth();
}

double PianoRoll::xToBeat (float x) const
{
    const auto a = gridArea();
    const double L = pattern ? pattern->lengthBeats : 16.0;
    return (double) (x - a.getX()) / a.getWidth() * L;
}

float PianoRoll::pitchToY (int pitch) const
{
    const int top = lowestPitch + numRows - 1;
    return gridArea().getY() + (float) (top - pitch) * rowHeight();
}

int PianoRoll::yToPitch (float y) const
{
    const int top = lowestPitch + numRows - 1;
    return juce::jlimit (0, 127, top - (int) std::floor ((y - gridArea().getY()) / rowHeight()));
}

juce::Rectangle<float> PianoRoll::noteRect (const NoteEvent& n, int displayPitch) const
{
    const float x0 = beatToX (n.start);
    const float x1 = beatToX (n.end());
    return { x0, pitchToY (displayPitch), std::max (3.0f, x1 - x0), rowHeight() };
}

double PianoRoll::snap (double beat) const
{
    const double g = proc.editGridBeats();
    return std::round (beat / g) * g;
}

void PianoRoll::autoFitPitchRange()
{
    int lo = 128, hi = -1;
    const int tr = proc.getTranspose();
    for (const auto& n : work) { lo = std::min (lo, n.pitch + tr); hi = std::max (hi, n.pitch + tr); }
    if (pattern != nullptr)
        for (int t = 1; t < kNumTracks; ++t)
            if (visible[(size_t) t])
                for (const auto& n : pattern->tracks[(size_t) t]) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); }

    if (hi < 0) { lowestPitch = 48; numRows = 36; return; }

    lo -= 3; hi += 3;
    numRows = juce::jlimit (24, 88, hi - lo + 1);
    if (hi - lo + 1 < numRows)
        lo -= (numRows - (hi - lo + 1)) / 2;
    lowestPitch = juce::jlimit (0, 127 - numRows + 1, lo);
}

//==============================================================================
void PianoRoll::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto bounds = getLocalBounds().toFloat();
    const auto area = gridArea();
    const float rh = rowHeight();
    const double L = pattern ? pattern->lengthBeats : 16.0;
    const int tr = proc.getTranspose();

    g.fillAll (Colour (0xff12141a));

    // ------------------------------------------------ Zeilen
    std::vector<int> steps;
    int keyRoot = 0;
    if (pattern != nullptr)
    {
        steps = getScales()[(size_t) pattern->scaleIndex].steps;
        keyRoot = pattern->keyRoot;
    }

    for (int r = 0; r < numRows; ++r)
    {
        const int pitch = lowestPitch + numRows - 1 - r;
        const float y = area.getY() + r * rh;
        const int pc = pitch % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        Colour row = black ? Colour (0xff15171e) : Colour (0xff1a1d25);
        if (! steps.empty() && ! work.empty() && isInScale (pitch, keyRoot, steps))
            row = row.interpolatedWith (Colours::accent, 0.035f);
        g.setColour (row);
        g.fillRect (area.getX(), y, area.getWidth(), rh);
        g.setColour (pc == 0 ? Colour (0xff2c3242) : Colour (0xff1f232d));
        g.drawHorizontalLine ((int) std::round (y + rh), area.getX(), area.getRight());
    }

    // ------------------------------------------------ Raster
    const double gridStep = proc.editGridBeats();
    const float pxPerStep = (float) (gridStep / L) * area.getWidth();
    const int stepsTotal = (int) std::round (L / gridStep);
    for (int i = 0; i <= stepsTotal; ++i)
    {
        const double b = i * gridStep;
        const float x = beatToX (b);
        const bool bar = std::abs (std::fmod (b + 1.0e-6, 4.0)) < 1.0e-4;
        const bool beat = std::abs (b - std::round (b)) < 1.0e-4;
        if (! bar && ! beat && pxPerStep < 7.0f) continue;
        g.setColour (bar ? Colour (0xff3a4152) : (beat ? Colour (0xff272c38) : Colour (0xff1e222b)));
        g.drawVerticalLine ((int) std::round (x), area.getY(), area.getBottom());
    }

    // ------------------------------------------------ Lineal + Akkordspur
    auto ruler = bounds.withTrimmedLeft (keyWidth).withHeight (rulerHeight);
    g.setColour (Colour (0xff181b23));
    g.fillRect (ruler);
    g.setFont (uiFont (11.0f, true));
    for (int bar = 0; bar * 4.0 < L; ++bar)
    {
        const float x = beatToX (bar * 4.0);
        g.setColour (Colours::textDim);
        g.drawText (String (bar + 1), Rectangle<float> (x + 4, ruler.getY(), 40, rulerHeight), Justification::centredLeft);
        g.setColour (Colour (0xff3a4152));
        g.drawVerticalLine ((int) std::round (x), ruler.getY() + 4, ruler.getBottom());
    }

    auto lane = bounds.withTrimmedLeft (keyWidth).withTrimmedTop (rulerHeight).withHeight (chordLaneHeight);
    g.setColour (Colour (0xff151820));
    g.fillRect (lane);
    if (pattern != nullptr && visible[kChords])
    {
        g.setFont (uiFont (12.0f, true));
        for (const auto& c : pattern->chords)
        {
            auto r = Rectangle<float> (beatToX (c.start), lane.getY() + 2, beatToX (c.start + c.length) - beatToX (c.start), lane.getHeight() - 4).reduced (1.5f, 0);
            g.setColour (Colours::chords.withAlpha (0.22f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (Colours::chords.brighter (0.4f));
            g.drawText (c.name, r.reduced (5, 0), Justification::centredLeft, true);
        }
    }

    // ------------------------------------------------ Noten
    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    auto drawNotes = [&] (const std::vector<NoteEvent>& notes, int pitchOffset, Colour c, float alpha, bool strong)
    {
        for (const auto& n : notes)
        {
            const int p = n.pitch + pitchOffset;
            if (p < lowestPitch || p >= lowestPitch + numRows) continue;
            auto r = noteRect (n, p).reduced (0.5f, 0.5f);
            const float velA = 0.55f + 0.45f * (float) n.velocity / 127.0f;
            g.setColour (c.withAlpha (alpha * velA));
            g.fillRoundedRectangle (r, std::min (3.0f, rh * 0.3f));
            if (strong)
            {
                g.setColour (c.brighter (0.5f));
                g.drawRoundedRectangle (r, std::min (3.0f, rh * 0.3f), 1.0f);
                if (r.getWidth() > 28.0f && rh >= 11.0f)
                {
                    g.setColour (Colour (0xff1a0a10));
                    g.setFont (uiFont (std::min (11.0f, rh - 2.0f), true));
                    g.drawText (noteName (p), r.reduced (3, 0), Justification::centredLeft, false);
                }
            }
        }
    };

    if (pattern != nullptr)
    {
        if (visible[kBass])   drawNotes (pattern->tracks[kBass], 0, Colours::bass, 0.55f, false);
        if (visible[kChords]) drawNotes (pattern->tracks[kChords], 0, Colours::chords, 0.45f, false);
    }
    if (visible[kMelody])
        drawNotes (work, tr, Colours::accent, 1.0f, true);

    // Bereich ausserhalb der Loop leicht abdunkeln (Noten hinter dem Loop-Ende)
    g.restoreState();

    // ------------------------------------------------ Playhead
    if (isPlaying)
    {
        const float x = beatToX (playBeat);
        g.setColour (Colours::text.withAlpha (0.85f));
        g.fillRect (x - 0.75f, ruler.getY(), 1.5f, bounds.getBottom() - ruler.getY());
        Path tri;
        tri.addTriangle (x - 5, ruler.getY(), x + 5, ruler.getY(), x, ruler.getY() + 7);
        g.fillPath (tri);
    }

    // ------------------------------------------------ Tastatur
    auto keys = bounds.withWidth (keyWidth).withTrimmedTop (rulerHeight + chordLaneHeight);
    g.setColour (Colour (0xff0d0f13));
    g.fillRect (bounds.withWidth (keyWidth).withHeight (rulerHeight + chordLaneHeight));
    for (int r = 0; r < numRows; ++r)
    {
        const int pitch = lowestPitch + numRows - 1 - r;
        const int pc = pitch % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        const float y = keys.getY() + r * rh;
        g.setColour (black ? Colour (0xff22252e) : Colour (0xffd9dce4));
        g.fillRect (keys.getX(), y, keys.getWidth() - (black ? 14.0f : 0.0f), rh - 0.5f);
        if (pc == 0 && rh >= 8.0f)
        {
            g.setColour (Colour (0xff3a3f4d));
            g.setFont (uiFont (std::min (11.0f, rh), true));
            g.drawText (noteName (pitch), Rectangle<float> (keys.getX() + 3, y, keys.getWidth() - 6, rh), Justification::centredRight);
        }
    }

    // ------------------------------------------------ Leerer Zustand
    if (work.empty() && drag == Drag::None)
    {
        g.setColour (Colours::textDim);
        g.setFont (uiFont (16.0f, true));
        g.drawFittedText ("Druecke AUFNAHME und summ deine Melodie\noder klick hier, um Noten selbst zu setzen.",
                          area.reduced (20).toNearestInt(), Justification::centred, 3);
    }

    g.setColour (Colours::outline);
    g.drawRect (bounds, 1.0f);
}

//==============================================================================
int PianoRoll::hitTestMelody (juce::Point<float> p, bool& onRightEdge) const
{
    onRightEdge = false;
    if (! visible[kMelody]) return -1;
    const int tr = proc.getTranspose();
    for (int i = (int) work.size() - 1; i >= 0; --i)
    {
        const auto r = noteRect (work[(size_t) i], work[(size_t) i].pitch + tr);
        if (r.expanded (1.0f, 0.0f).contains (p))
        {
            onRightEdge = p.x > r.getRight() - std::min (8.0f, r.getWidth() * 0.35f);
            return i;
        }
    }
    return -1;
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    if (pos.x < keyWidth)
    {
        if (pos.y >= gridArea().getY())
            proc.previewNote (yToPitch (pos.y));
        return;
    }
    if (pos.y < gridArea().getY())
        return;

    const int tr = proc.getTranspose();
    bool edge = false;
    const int idx = hitTestMelody (pos, edge);

    if (e.mods.isPopupMenu())
    {
        if (idx >= 0)
        {
            work.erase (work.begin() + idx);
            proc.setMelody (work, true);
            repaint();
        }
        return;
    }

    changed = false;
    if (idx >= 0)
    {
        dragIndex = idx;
        drag = edge ? Drag::Resize : Drag::Move;
        dragOrig = work[(size_t) idx];
    }
    else
    {
        const double g = proc.editGridBeats();
        const double L = pattern ? pattern->lengthBeats : 16.0;
        NoteEvent n;
        n.start = juce::jlimit (0.0, std::max (0.0, L - g), std::floor (xToBeat (pos.x) / g) * g);
        n.length = lastLength;
        n.pitch = juce::jlimit (0, 127, yToPitch (pos.y) - tr);
        n.velocity = 100;
        work.push_back (n);
        dragIndex = (int) work.size() - 1;
        drag = Drag::Move;
        dragOrig = n;
        changed = true;
    }

    dragStartBeat = xToBeat (pos.x);
    dragStartPitch = yToPitch (pos.y);
    lastPreviewPitch = work[(size_t) dragIndex].pitch + tr;
    proc.previewNote (lastPreviewPitch);
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::None || dragIndex < 0 || dragIndex >= (int) work.size())
        return;

    const double g = proc.editGridBeats();
    const double L = pattern ? pattern->lengthBeats : 16.0;
    const int tr = proc.getTranspose();
    auto& n = work[(size_t) dragIndex];
    const auto old = n;

    if (drag == Drag::Move)
    {
        const double delta = std::round ((xToBeat (e.position.x) - dragStartBeat) / g) * g;
        n.start = juce::jlimit (0.0, std::max (0.0, L - g), dragOrig.start + delta);
        n.pitch = juce::jlimit (0, 127, dragOrig.pitch + (yToPitch (e.position.y) - dragStartPitch));
    }
    else
    {
        const double end = std::max (n.start + g, snap (xToBeat (e.position.x)));
        n.length = end - n.start;
    }

    if (n.start != old.start || n.pitch != old.pitch || n.length != old.length)
        changed = true;

    if (n.pitch + tr != lastPreviewPitch)
    {
        lastPreviewPitch = n.pitch + tr;
        proc.previewNote (lastPreviewPitch);
    }
    repaint();
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    if (drag != Drag::None && changed && dragIndex >= 0 && dragIndex < (int) work.size())
    {
        lastLength = work[(size_t) dragIndex].length;
        proc.setMelody (work, true);
    }
    drag = Drag::None;
    dragIndex = -1;
    changed = false;
    work = proc.getMelody();
    repaint();
}

void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    bool edge = false;
    const int idx = (e.position.x > keyWidth && e.position.y > gridArea().getY()) ? hitTestMelody (e.position, edge) : -1;
    if (idx >= 0)
        setMouseCursor (edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float d = std::abs (w.deltaY) > 0.0001f ? w.deltaY : w.deltaX;
    if (std::abs (d) < 0.0001f) return;
    userScrolled = true;

    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const int centre = lowestPitch + numRows / 2;
        numRows = juce::jlimit (12, 88, numRows + (d > 0 ? -4 : 4));
        lowestPitch = centre - numRows / 2;
    }
    else
    {
        lowestPitch += d > 0 ? 2 : -2;
    }
    lowestPitch = juce::jlimit (0, 127 - numRows + 1, lowestPitch);
    repaint();
}

} // namespace melosumm::ui
