#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace melosumm::ui
{

namespace Colours
{
    const juce::Colour bg        { 0xff101218 };
    const juce::Colour panel     { 0xff181b23 };
    const juce::Colour panelHi   { 0xff20242f };
    const juce::Colour outline   { 0xff2b3040 };
    const juce::Colour text      { 0xffe8eaf0 };
    const juce::Colour textDim   { 0xff8a91a6 };
    const juce::Colour accent    { 0xffff5a7a };   // Melodie / Hauptfarbe
    const juce::Colour chords    { 0xff7c8cff };
    const juce::Colour bass      { 0xff35d39a };
    const juce::Colour record    { 0xffff3b4e };
    const juce::Colour play      { 0xff3ddc84 };

    inline juce::Colour forTrack (int t)
    {
        return t == 0 ? accent : (t == 1 ? chords : bass);
    }
}

juce::Font uiFont (float height, bool bold = false);

class MeloLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MeloLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getPopupMenuFont() override;
};

} // namespace melosumm::ui
