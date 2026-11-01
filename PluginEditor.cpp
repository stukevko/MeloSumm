#include "PluginEditor.h"
#include <algorithm>
#include "Exporter.h"

using namespace melosumm;
using namespace melosumm::ui;

namespace
{
    juce::String utf (const char* s) { return juce::String::fromUTF8 (s); }

    // Symbole als UTF-8-Escapes, damit der Quelltext ASCII bleibt
    const char* const kDot   = "\xe2\x97\x8f";   // ●
    const char* const kStop  = "\xe2\x96\xa0";   // ■
    const char* const kPlay  = "\xe2\x96\xb6";   // ▶

    juce::String fmtPercent (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; }
}

//==============================================================================
MeloSummEditor::MeloSummEditor (MeloSummProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      roll (p),
      sensKnob      (p.apvts, ParamIDs::sensitivity, "Empfindlichkeit", Colours::accent, fmtPercent),
      minNoteKnob   (p.apvts, ParamIDs::minNote, utf ("Min. L\xc3\xa4nge"), Colours::accent,
                     [] (double v) { return juce::String (juce::roundToInt (v)) + " ms"; }),
      strengthKnob  (p.apvts, ParamIDs::qStrength, "Quantisierung", Colours::accent,
                     [] (double v) { return juce::String (juce::roundToInt (v)) + " %"; }),
      transposeKnob (p.apvts, ParamIDs::transpose, "Transponieren", Colours::accent,
                     [] (double v) { const int i = juce::roundToInt (v); return (i > 0 ? "+" : "") + juce::String (i) + " HT"; }),
      gridChoice  (p.apvts, ParamIDs::grid, "Raster"),
      loopChoice  (p.apvts, ParamIDs::loopBars, utf ("Loop-L\xc3\xa4nge")),
      keyChoice   (p.apvts, ParamIDs::keyRoot, "Tonart"),
      scaleChoice (p.apvts, ParamIDs::scale, "Skala"),
      snapToggle  (p.apvts, ParamIDs::snapScale, "Auf Tonart korrigieren"),
      trimToggle  (p.apvts, ParamIDs::trimStart, "Leere Takte am Anfang weg"),
      metroToggle   (p.apvts, ParamIDs::metronome, "Metronom"),
      countInToggle (p.apvts, ParamIDs::countIn, utf ("Vorz\xc3\xa4hler (1 Takt)")),
      syncToggle    (p.apvts, ParamIDs::syncHost, "Mit FL Studio synchron"),
      monitorToggle (p.apvts, ParamIDs::monitor, utf ("Mikro mith\xc3\xb6ren")),
      loopToggle    (p.apvts, ParamIDs::loopPlay, "Loop"),
      bpmKnob       (p.apvts, ParamIDs::bpm, "Tempo (ohne FL)", Colours::accent,
                     [] (double v) { return juce::String (v, 1) + " BPM"; }),
      melPreset  (p.apvts, ParamIDs::presetId (kMelody), "Sound"),
      chdPreset  (p.apvts, ParamIDs::presetId (kChords), "Sound"),
      bassPreset (p.apvts, ParamIDs::presetId (kBass), "Sound"),
      melVol  (p.apvts, ParamIDs::volId (kMelody), utf ("Lautst\xc3\xa4rke"), Colours::accent, fmtPercent),
      chdVol  (p.apvts, ParamIDs::volId (kChords), utf ("Lautst\xc3\xa4rke"), Colours::chords, fmtPercent),
      bassVol (p.apvts, ParamIDs::volId (kBass), utf ("Lautst\xc3\xa4rke"), Colours::bass, fmtPercent),
      melMute  (p.apvts, ParamIDs::muteId (kMelody), "Stumm", Colours::accent),
      chdMute  (p.apvts, ParamIDs::muteId (kChords), "Stumm", Colours::chords),
      bassMute (p.apvts, ParamIDs::muteId (kBass), "Stumm", Colours::bass),
      chordStyle  (p.apvts, ParamIDs::chordStyle, "Spielweise"),
      chordRate   (p.apvts, ParamIDs::chordRate, "Wechsel"),
      chordType   (p.apvts, ParamIDs::complexity, "Akkord-Typ"),
      chordReg    (p.apvts, ParamIDs::chordReg, "Lage"),
      bassPattern (p.apvts, ParamIDs::bassPattern, "Muster"),
      bassReg     (p.apvts, ParamIDs::bassReg, "Lage"),
      melMidi  ("MIDI", Colours::accent, [&p] { return p.exportMidi (1 << kMelody); }),
      melWav   ("WAV",  Colours::accent, [&p] { return p.exportWav (1 << kMelody); }),
      chdMidi  ("MIDI", Colours::chords, [&p] { return p.exportMidi (1 << kChords); }),
      chdWav   ("WAV",  Colours::chords, [&p] { return p.exportWav (1 << kChords); }),
      bassMidi ("MIDI", Colours::bass,   [&p] { return p.exportMidi (1 << kBass); }),
      bassWav  ("WAV",  Colours::bass,   [&p] { return p.exportWav (1 << kBass); }),
      masterKnob   (p.apvts, ParamIDs::master, "Master", Colours::text, fmtPercent),
      reverbKnob   (p.apvts, ParamIDs::reverb, "Hall", Colours::chords, fmtPercent),
      humanizeKnob (p.apvts, ParamIDs::humanize, "Humanize", Colours::bass, fmtPercent),
      audioToggle  (p.apvts, ParamIDs::audioOn, "Sound an (aus = nur MIDI)", Colours::play),
      midiToggle   (p.apvts, ParamIDs::midiOut, "MIDI-Out (Kanal 1/2/3)", Colours::chords),
      allMidi ("Alles als MIDI ziehen", Colours::text, [&p] { return p.exportMidi (Exporter::kAll); }),
      mixWav  ("Mix als WAV ziehen", Colours::text, [&p] { return p.exportWav (Exporter::kAll); })
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    // ------------------------------------------------ Kopfzeile
    recordBtn.setButtonText (utf (kDot) + "  AUFNAHME");
    styleButton (recordBtn, Colours::record);
    recordBtn.setTooltip (utf ("Startet die Aufnahme (mit Vorz\xc3\xa4hler). Nochmal klicken = Aufnahme beenden und analysieren."));
    recordBtn.onClick = [this]
    {
        if (proc.getRecState() == MeloSummProcessor::RecState::Idle) proc.startRecording();
        else proc.stopAll();
    };

    stopBtn.setButtonText (utf (kStop) + "  STOPP");
    styleButton (stopBtn, Colours::panelHi);
    stopBtn.onClick = [this] { proc.stopAll(); };

    playBtn.setButtonText (utf (kPlay) + "  ABSPIELEN");
    styleButton (playBtn, Colours::play.darker (0.5f));
    playBtn.setTooltip (utf ("Spielt die Loop ab. L\xc3\xa4uft FL Studio, spielt MeloSumm automatisch synchron mit."));
    playBtn.onClick = [this] { proc.togglePlay(); };

    loadBtn.setButtonText ("Audio laden");
    styleButton (loadBtn, Colours::panelHi);
    loadBtn.setTooltip (utf ("Eine Aufnahme (WAV, MP3, FLAC ...) laden und analysieren. Du kannst Dateien auch einfach aufs Plugin ziehen."));
    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (utf ("Aufnahme w\xc3\xa4hlen"), juce::File(), "*.wav;*.mp3;*.flac;*.aif;*.aiff;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) proc.loadAudioFile (f);
                              });
    };

    undoBtn.setButtonText (utf ("R\xc3\xbc" "ckg\xc3\xa4ngig"));
    styleButton (undoBtn, Colours::panelHi);
    undoBtn.setTooltip ("Strg+Z");
    undoBtn.onClick = [this] { proc.undo(); };

    redoBtn.setButtonText ("Wiederholen");
    styleButton (redoBtn, Colours::panelHi);
    redoBtn.setTooltip ("Strg+Y");
    redoBtn.onClick = [this] { proc.redo(); };

    clearBtn.setButtonText (utf ("L\xc3\xb6schen"));
    styleButton (clearBtn, Colours::panelHi);
    clearBtn.setTooltip (utf ("Melodie l\xc3\xb6schen (mit R\xc3\xbc" "ckg\xc3\xa4ngig wiederherstellbar)."));
    clearBtn.onClick = [this] { proc.clearMelody(); };

    for (auto* b : { &recordBtn, &stopBtn, &playBtn, &loadBtn, &undoBtn, &redoBtn, &clearBtn })
        addAndMakeVisible (b);

    statusLabel.setFont (uiFont (15.0f, true));
    statusLabel.setColour (juce::Label::textColourId, Colours::text);
    addAndMakeVisible (statusLabel);

    keyLabel.setFont (uiFont (15.0f, true));
    keyLabel.setJustificationType (juce::Justification::centred);
    keyLabel.setColour (juce::Label::textColourId, Colours::accent);
    keyLabel.setColour (juce::Label::backgroundColourId, Colours::panelHi);
    addAndMakeVisible (keyLabel);

    bpmLabel.setFont (uiFont (13.0f));
    bpmLabel.setJustificationType (juce::Justification::centredRight);
    bpmLabel.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (bpmLabel);

    meter.setTooltip ("Mikrofon-Pegel");
    addAndMakeVisible (meter);

    // ------------------------------------------------ Piano Roll
    addAndMakeVisible (roll);
    const char* viewNames[kNumTracks] = { "Melodie", "Akkorde", "Bass" };
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& tb = viewToggles[(size_t) t];
        tb.setButtonText (viewNames[t]);
        tb.setToggleState (true, juce::dontSendNotification);
        tb.setColour (juce::ToggleButton::tickColourId, Colours::forTrack (t));
        tb.onClick = [this, t] { roll.setTrackVisible (t, viewToggles[(size_t) t].getToggleState()); };
        addAndMakeVisible (tb);
    }
    hintLabel.setText (utf ("Klick = Note setzen   \xc2\xb7   Ziehen = verschieben   \xc2\xb7   rechter Rand = L\xc3\xa4nge   \xc2\xb7   Rechtsklick = l\xc3\xb6schen   \xc2\xb7   Mausrad = scrollen (Strg = zoomen)"),
                       juce::dontSendNotification);
    hintLabel.setFont (uiFont (12.0f));
    hintLabel.setJustificationType (juce::Justification::centredRight);
    hintLabel.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (hintLabel);

    // ------------------------------------------------ Regler
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &sensKnob, &minNoteKnob, &strengthKnob, &transposeKnob, &gridChoice, &loopChoice, &keyChoice, &scaleChoice,
             &snapToggle, &trimToggle, &metroToggle, &countInToggle, &syncToggle, &monitorToggle, &loopToggle, &bpmKnob,
             &melPreset, &chdPreset, &bassPreset, &melVol, &chdVol, &bassVol, &melMute, &chdMute, &bassMute,
             &chordStyle, &chordRate, &chordType, &chordReg, &bassPattern, &bassReg,
             &melMidi, &melWav, &chdMidi, &chdWav, &bassMidi, &bassWav,
             &masterKnob, &reverbKnob, &humanizeKnob, &audioToggle, &midiToggle, &allMidi, &mixWav })
        addAndMakeVisible (c);

    sensKnob.slider.setTooltip (utf ("H\xc3\xb6her = auch leise/unsaubere T\xc3\xb6ne werden erkannt. Niedriger = nur klare T\xc3\xb6ne."));
    minNoteKnob.slider.setTooltip (utf ("K\xc3\xbcrzere Noten werden ignoriert (gegen Wackler)."));
    strengthKnob.slider.setTooltip ("Wie stark die Noten auf das Raster gezogen werden.");
    snapToggle.button.setTooltip (utf ("Schiefe T\xc3\xb6ne werden auf die Tonleiter korrigiert."));
    syncToggle.button.setTooltip (utf ("An: Tempo und Start kommen von FL Studio. L\xc3\xa4uft FL, startet die Aufnahme beim n\xc3\xa4" "chsten Takt."));
    monitorToggle.button.setTooltip (utf ("Dein Mikrofon wird durch das Plugin ausgegeben (Kopfh\xc3\xb6rer benutzen!)."));

    variationBtn.setButtonText ("Neue Akkorde");
    styleButton (variationBtn, Colours::chords.darker (0.6f));
    variationBtn.setTooltip ("Erzeugt eine andere passende Akkordfolge.");
    variationBtn.onClick = [this] { proc.newChordVariation(); };
    addAndMakeVisible (variationBtn);

    saveBtn.setButtonText ("Alles speichern ...");
    styleButton (saveBtn, Colours::panelHi);
    saveBtn.setTooltip ("Speichert MIDI und WAV (Komplett + jede Spur einzeln) in einen Ordner.");
    saveBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (utf ("Ordner f\xc3\xbcr den Export w\xc3\xa4hlen"), Exporter::exportFolder());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto dir = fc.getResult();
                                  if (dir.isDirectory()) proc.saveAllTo (dir);
                              });
    };
    addAndMakeVisible (saveBtn);

    folderBtn.setButtonText ("Export-Ordner");
    styleButton (folderBtn, Colours::panelHi);
    folderBtn.setTooltip (utf ("\xc3\x96" "ffnet den Ordner mit allen gezogenen Dateien."));
    folderBtn.onClick = [] { Exporter::exportFolder().startAsProcess(); };
    addAndMakeVisible (folderBtn);

    setResizable (true, true);
    setResizeLimits (1240, 880, 2600, 1700);
    setSize (1300, 900);
    startTimerHz (30);
    timerCallback();
}

MeloSummEditor::~MeloSummEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MeloSummEditor::styleButton (juce::TextButton& b, juce::Colour c)
{
    b.setColour (juce::TextButton::buttonColourId, c);
    b.setColour (juce::TextButton::buttonOnColourId, c.brighter (0.2f));
}

//==============================================================================
void MeloSummEditor::timerCallback()
{
    ++blink;
    statusLabel.setText (proc.getStatusText(), juce::dontSendNotification);

    const auto st = proc.getRecState();
    if (st == MeloSummProcessor::RecState::Idle)
    {
        recordBtn.setButtonText (utf (kDot) + "  AUFNAHME");
        recordBtn.setColour (juce::TextButton::buttonColourId, Colours::record.darker (0.35f));
    }
    else
    {
        recordBtn.setButtonText (utf (kStop) + "  AUFNAHME BEENDEN");
        const bool on = (blink / 10) % 2 == 0;
        recordBtn.setColour (juce::TextButton::buttonColourId, on ? Colours::record : Colours::record.darker (0.5f));
    }
    recordBtn.setEnabled (! proc.isAnalyzing());

    const bool playing = proc.isInternalPlaying();
    playBtn.setButtonText (playing ? utf (kStop) + "  STOPP" : utf (kPlay) + "  ABSPIELEN");
    playBtn.setColour (juce::TextButton::buttonColourId, playing ? Colours::play.darker (0.2f) : Colours::play.darker (0.55f));

    undoBtn.setEnabled (proc.canUndo());
    redoBtn.setEnabled (proc.canRedo());

    meter.setLevel (proc.getInputLevel());
    roll.setPlayhead (proc.getPlayPosition(), proc.isPlayingNow());

    bpmLabel.setText (juce::String (proc.getCurrentBpm(), 1) + " BPM" + (proc.isHostTempo() ? " (FL)" : " (manuell)"),
                      juce::dontSendNotification);

    const int v = proc.getPatternVersion();
    if (v != lastPatternVersion)
    {
        lastPatternVersion = v;
        roll.refresh();
        auto p = proc.getPattern();
        keyLabel.setText (p != nullptr && ! proc.getMelody().empty() ? p->keyName : juce::String ("Tonart: -"),
                          juce::dontSendNotification);
    }
    else
    {
        // Raster/Transponieren aendern die Ansicht, ohne ein neues Pattern zu erzeugen
        const int viewKey = proc.getTranspose() * 1000 + juce::roundToInt (proc.editGridBeats() * 96.0);
        if (viewKey != lastViewKey)
        {
            lastViewKey = viewKey;
            roll.refresh();
        }
    }
}

//==============================================================================
void MeloSummEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);

    // Logo
    auto logo = headerArea.withWidth (190);
    g.setFont (uiFont (28.0f, true));
    juce::ColourGradient grad (Colours::accent, (float) logo.getX(), 0.0f, Colours::chords, (float) logo.getRight(), 0.0f, false);
    g.setGradientFill (grad);
    g.drawText ("MeloSumm", logo.withTrimmedBottom (14), juce::Justification::centredLeft);
    g.setColour (Colours::textDim);
    g.setFont (uiFont (11.5f));
    g.drawText (utf ("Summen \xe2\x86\x92 MIDI \xe2\x86\x92 Beat"), logo.withTrimmedTop (34), juce::Justification::centredLeft);

    drawSection (g, detectArea, "Erkennung");
    drawSection (g, recordArea, "Aufnahme & Wiedergabe");
    drawSection (g, trackAreas[0], "Melodie", Colours::accent);
    drawSection (g, trackAreas[1], "Akkorde", Colours::chords);
    drawSection (g, trackAreas[2], "Bass", Colours::bass);
    drawSection (g, footerArea, {});
}

void MeloSummEditor::paintOverChildren (juce::Graphics& g)
{
    if (! fileHover) return;
    g.setColour (Colours::bg.withAlpha (0.75f));
    g.fillAll();
    g.setColour (Colours::accent);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (12.0f), 14.0f, 3.0f);
    g.setFont (uiFont (26.0f, true));
    g.drawText ("Loslassen zum Analysieren", getLocalBounds(), juce::Justification::centred);
}

void MeloSummEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    constexpr int gap = 10;

    // ------------------------------------------------ Kopfzeile
    headerArea = r.removeFromTop (54);
    {
        auto h = headerArea;
        h.removeFromLeft (200);
        recordBtn.setBounds (h.removeFromLeft (200).reduced (0, 6));
        h.removeFromLeft (8);
        stopBtn.setBounds (h.removeFromLeft (100).reduced (0, 6));
        h.removeFromLeft (8);
        playBtn.setBounds (h.removeFromLeft (140).reduced (0, 6));

        clearBtn.setBounds (h.removeFromRight (84).reduced (0, 10));
        h.removeFromRight (6);
        redoBtn.setBounds (h.removeFromRight (100).reduced (0, 10));
        h.removeFromRight (6);
        undoBtn.setBounds (h.removeFromRight (100).reduced (0, 10));
        h.removeFromRight (6);
        loadBtn.setBounds (h.removeFromRight (100).reduced (0, 10));
        h.removeFromRight (14);

        auto info = h.removeFromRight (juce::jmin (210, juce::jmax (150, h.getWidth() - 12)));
        keyLabel.setBounds (info.removeFromTop (30).reduced (0, 2));
        auto lower = info;
        meter.setBounds (lower.removeFromLeft (50).withSizeKeepingCentre (50, 8));
        bpmLabel.setBounds (lower);
    }

    r.removeFromTop (4);
    statusLabel.setBounds (r.removeFromTop (26));
    r.removeFromTop (6);

    // ------------------------------------------------ Fusszeile
    footerArea = r.removeFromBottom (90);
    {
        auto f = footerArea.reduced (12, 6);
        masterKnob.setBounds (f.removeFromLeft (72));
        reverbKnob.setBounds (f.removeFromLeft (72));
        humanizeKnob.setBounds (f.removeFromLeft (72));
        f.removeFromLeft (14);
        auto toggles = f.removeFromLeft (230);
        audioToggle.setBounds (toggles.removeFromTop (toggles.getHeight() / 2).reduced (0, 4));
        midiToggle.setBounds (toggles.reduced (0, 4));

        auto btnArea = f.withSizeKeepingCentre (f.getWidth(), 40);
        folderBtn.setBounds (btnArea.removeFromRight (130));
        btnArea.removeFromRight (8);
        saveBtn.setBounds (btnArea.removeFromRight (160));
        btnArea.removeFromRight (8);
        mixWav.setBounds (btnArea.removeFromRight (190));
        btnArea.removeFromRight (8);
        allMidi.setBounds (btnArea.removeFromRight (200));
    }
    r.removeFromBottom (gap);

    // ------------------------------------------------ Spuren
    auto tracks = r.removeFromBottom (186);
    r.removeFromBottom (gap);
    {
        const int w = tracks.getWidth() - 2 * gap;
        trackAreas[0] = tracks.removeFromLeft ((int) (w * 0.27f));
        tracks.removeFromLeft (gap);
        trackAreas[2] = tracks.removeFromRight ((int) (w * 0.29f));
        tracks.removeFromRight (gap);
        trackAreas[1] = tracks;

        auto layoutDrags = [] (juce::Rectangle<int> row, juce::Component& a, juce::Component& b)
        {
            const int w2 = (row.getWidth() - 8) / 2;
            a.setBounds (row.removeFromLeft (w2));
            row.removeFromLeft (8);
            b.setBounds (row);
        };

        // Melodie
        {
            auto a = trackAreas[0].reduced (12, 8).withTrimmedTop (22);
            auto drags = a.removeFromBottom (34);
            layoutDrags (drags.withTrimmedLeft (0), melMidi, melWav);
            a.removeFromBottom (6);
            melVol.setBounds (a.removeFromRight (84));
            a.removeFromRight (8);
            melPreset.setBounds (a.removeFromTop (44));
            a.removeFromTop (6);
            melMute.setBounds (a.removeFromTop (26));
        }
        // Akkorde
        {
            auto a = trackAreas[1].reduced (12, 8).withTrimmedTop (22);
            auto bottom = a.removeFromBottom (34);
            a.removeFromBottom (6);
            chdVol.setBounds (a.removeFromRight (84));
            a.removeFromRight (8);
            auto row1 = a.removeFromTop (44);
            a.removeFromTop (6);
            auto row2 = a.removeFromTop (44);
            const int cw = (row1.getWidth() - 16) / 3;
            chdPreset.setBounds (row1.removeFromLeft (cw)); row1.removeFromLeft (8);
            chordStyle.setBounds (row1.removeFromLeft (cw)); row1.removeFromLeft (8);
            chordRate.setBounds (row1);
            chordType.setBounds (row2.removeFromLeft (cw)); row2.removeFromLeft (8);
            chordReg.setBounds (row2.removeFromLeft (cw)); row2.removeFromLeft (8);
            chdMute.setBounds (row2.withTrimmedTop (16));

            variationBtn.setBounds (bottom.removeFromLeft (140));
            bottom.removeFromLeft (8);
            layoutDrags (bottom, chdMidi, chdWav);
        }
        // Bass
        {
            auto a = trackAreas[2].reduced (12, 8).withTrimmedTop (22);
            auto drags = a.removeFromBottom (34);
            layoutDrags (drags, bassMidi, bassWav);
            a.removeFromBottom (6);
            bassVol.setBounds (a.removeFromRight (84));
            a.removeFromRight (8);
            auto row1 = a.removeFromTop (44);
            a.removeFromTop (6);
            auto row2 = a.removeFromTop (44);
            const int cw = (row1.getWidth() - 8) / 2;
            bassPreset.setBounds (row1.removeFromLeft (cw)); row1.removeFromLeft (8);
            bassPattern.setBounds (row1);
            bassReg.setBounds (row2.removeFromLeft (cw)); row2.removeFromLeft (8);
            bassMute.setBounds (row2.withTrimmedTop (16));
        }
    }

    // ------------------------------------------------ Seitenleiste
    auto side = r.removeFromLeft (290);
    r.removeFromLeft (gap);
    {
        recordArea = side.removeFromBottom (156);
        side.removeFromBottom (gap);
        detectArea = side;

        auto a = detectArea.reduced (12, 8).withTrimmedTop (22);
        const int rowH = juce::jlimit (64, 84, (a.getHeight() - 2 * 44 - 3 * 6) / 2);
        auto knobs = a.removeFromTop (rowH);
        const int kw = knobs.getWidth() / 3;
        sensKnob.setBounds (knobs.removeFromLeft (kw));
        minNoteKnob.setBounds (knobs.removeFromLeft (kw));
        strengthKnob.setBounds (knobs);
        a.removeFromTop (6);

        auto row = a.removeFromTop (44);
        const int hw = (row.getWidth() - 8) / 2;
        gridChoice.setBounds (row.removeFromLeft (hw)); row.removeFromLeft (8);
        loopChoice.setBounds (row);
        a.removeFromTop (6);
        row = a.removeFromTop (44);
        keyChoice.setBounds (row.removeFromLeft (hw)); row.removeFromLeft (8);
        scaleChoice.setBounds (row);
        a.removeFromTop (6);
        row = a.removeFromTop (juce::jmin (rowH, a.getHeight()));
        transposeKnob.setBounds (row.removeFromLeft (84));
        row.removeFromLeft (8);
        snapToggle.setBounds (row.removeFromTop (row.getHeight() / 2).reduced (0, 4));
        trimToggle.setBounds (row.reduced (0, 4));

        auto b = recordArea.reduced (12, 8).withTrimmedTop (22);
        bpmKnob.setBounds (b.removeFromRight (90));
        const int th = b.getHeight() / 5;
        for (auto* t : { &syncToggle, &countInToggle, &metroToggle, &monitorToggle, &loopToggle })
            t->setBounds (b.removeFromTop (th));
    }

    // ------------------------------------------------ Piano Roll
    auto top = r.removeFromTop (26);
    for (auto& tb : viewToggles)
        tb.setBounds (top.removeFromLeft (100));
    hintLabel.setBounds (top);
    r.removeFromTop (4);
    roll.setBounds (r);
}

bool MeloSummEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z'))
    {
        if (mods.isShiftDown()) proc.redo(); else proc.undo();
        return true;
    }
    if (mods.isCommandDown() && (key.getKeyCode() == 'Y' || key.getKeyCode() == 'y'))
    {
        proc.redo();
        return true;
    }
    return false;
}

bool MeloSummEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;mp3;flac;aif;aiff;ogg"))
            return true;
    return false;
}

void MeloSummEditor::filesDropped (const juce::StringArray& files, int, int)
{
    fileHover = false;
    repaint();
    for (const auto& f : files)
    {
        juce::File file (f);
        if (file.hasFileExtension ("wav;mp3;flac;aif;aiff;ogg"))
        {
            proc.loadAudioFile (file);
            break;
        }
    }
}
