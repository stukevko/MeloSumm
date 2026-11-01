#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"
#include "Widgets.h"
#include "PianoRoll.h"

class MeloSummEditor : public juce::AudioProcessorEditor,
                       public juce::FileDragAndDropTarget,
                       private juce::Timer
{
public:
    explicit MeloSummEditor (MeloSummProcessor&);
    ~MeloSummEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { fileHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { fileHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void styleButton (juce::TextButton& b, juce::Colour c);

    MeloSummProcessor& proc;
    melosumm::ui::MeloLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // Kopfzeile
    juce::TextButton recordBtn, stopBtn, playBtn, loadBtn, undoBtn, redoBtn, clearBtn;
    juce::Label statusLabel, keyLabel, bpmLabel;
    melosumm::ui::LevelMeter meter;

    // Piano Roll
    melosumm::ui::PianoRoll roll;
    std::array<juce::ToggleButton, melosumm::kNumTracks> viewToggles;
    juce::Label hintLabel;

    // Erkennung
    melosumm::ui::Knob sensKnob, minNoteKnob, strengthKnob, transposeKnob;
    melosumm::ui::Choice gridChoice, loopChoice, keyChoice, scaleChoice;
    melosumm::ui::Toggle snapToggle, trimToggle;

    // Aufnahme
    melosumm::ui::Toggle metroToggle, countInToggle, syncToggle, monitorToggle, loopToggle;
    melosumm::ui::Knob bpmKnob;

    // Spuren
    melosumm::ui::Choice melPreset, chdPreset, bassPreset;
    melosumm::ui::Knob melVol, chdVol, bassVol;
    melosumm::ui::Toggle melMute, chdMute, bassMute;
    melosumm::ui::Choice chordStyle, chordRate, chordType, chordReg, bassPattern, bassReg;
    juce::TextButton variationBtn;
    melosumm::ui::DragExportButton melMidi, melWav, chdMidi, chdWav, bassMidi, bassWav;

    // Fusszeile
    melosumm::ui::Knob masterKnob, reverbKnob, humanizeKnob;
    melosumm::ui::Toggle audioToggle, midiToggle;
    melosumm::ui::DragExportButton allMidi, mixWav;
    juce::TextButton saveBtn, folderBtn;

    // Bereiche fuer paint()
    juce::Rectangle<int> headerArea, detectArea, recordArea, trackAreas[3], footerArea;

    std::unique_ptr<juce::FileChooser> chooser;
    int lastPatternVersion = -1;
    int lastViewKey = -1;
    bool fileHover = false;
    int blink = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeloSummEditor)
};
