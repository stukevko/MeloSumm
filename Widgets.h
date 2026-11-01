#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"
#include <functional>

namespace melosumm::ui
{

using APVTS = juce::AudioProcessorValueTreeState;

/** Drehregler mit Beschriftung oben und Wert unten. */
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& labelText,
          juce::Colour colour = Colours::accent, std::function<juce::String (double)> formatter = {});

    void resized() override;
    juce::Slider slider;

private:
    juce::Label label;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

/** Auswahlliste mit Beschriftung. */
class Choice : public juce::Component
{
public:
    Choice (APVTS& state, const juce::String& paramId, const juce::String& labelText);
    void resized() override;
    juce::ComboBox box;

private:
    juce::Label label;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

/** Schalter (Pill-Switch). */
class Toggle : public juce::Component
{
public:
    Toggle (APVTS& state, const juce::String& paramId, const juce::String& text,
            juce::Colour colour = Colours::accent);
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::ToggleButton button;

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

/** Knopf, den man direkt in FL Studio ziehen kann (MIDI/WAV).
    Klick: Datei erzeugen und im Explorer zeigen. */
class DragExportButton : public juce::TextButton
{
public:
    DragExportButton (const juce::String& text, juce::Colour colour, std::function<juce::File()> provider);
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    std::function<juce::File()> fileProvider;
    juce::Colour colour;
    bool dragStarted = false;
};

class LevelMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setLevel (float linear);
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f;
};

void drawSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title,
                  juce::Colour colour = Colours::textDim);

} // namespace melosumm::ui
