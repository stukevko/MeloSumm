#include "Widgets.h"
#include <algorithm>
#include <cmath>

namespace melosumm::ui
{

//==============================================================================
Knob::Knob (APVTS& state, const juce::String& paramId, const juce::String& labelText,
            juce::Colour colour, std::function<juce::String (double)> formatter)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (slider);

    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (uiFont (12.0f));
    label.setColour (juce::Label::textColourId, Colours::textDim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);

    if (formatter)
    {
        slider.textFromValueFunction = formatter;
        slider.updateText();
    }
    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (15));
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, r.getWidth(), 16);
    slider.setBounds (r);
}

//==============================================================================
Choice::Choice (APVTS& state, const juce::String& paramId, const juce::String& labelText)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);

    label.setText (labelText, juce::dontSendNotification);
    label.setFont (uiFont (12.0f));
    label.setColour (juce::Label::textColourId, Colours::textDim);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramId, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (15));
    box.setBounds (r.removeFromTop (juce::jmin (26, r.getHeight())));
}

//==============================================================================
Toggle::Toggle (APVTS& state, const juce::String& paramId, const juce::String& text, juce::Colour colour)
{
    button.setButtonText (text);
    button.setColour (juce::ToggleButton::tickColourId, colour);
    addAndMakeVisible (button);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, button);
}

//==============================================================================
DragExportButton::DragExportButton (const juce::String& text, juce::Colour c, std::function<juce::File()> provider)
    : juce::TextButton (text), fileProvider (std::move (provider)), colour (c)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("In FL Studio ziehen (Piano Roll, Playlist oder Channel Rack). Klick = Datei im Explorer zeigen.");
}

void DragExportButton::mouseDown (const juce::MouseEvent& e)
{
    dragStarted = false;
    juce::TextButton::mouseDown (e);
}

void DragExportButton::mouseDrag (const juce::MouseEvent& e)
{
    juce::TextButton::mouseDrag (e);
    if (dragStarted || e.getDistanceFromDragStart() < 6 || ! fileProvider)
        return;

    dragStarted = true;
    juce::MouseCursor::showWaitCursor();
    const auto file = fileProvider();
    juce::MouseCursor::hideWaitCursor();

    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

void DragExportButton::mouseUp (const juce::MouseEvent& e)
{
    const bool wasDrag = dragStarted;
    dragStarted = false;

    if (! wasDrag && e.getDistanceFromDragStart() < 6 && isMouseOver() && fileProvider)
    {
        juce::MouseCursor::showWaitCursor();
        const auto file = fileProvider();
        juce::MouseCursor::hideWaitCursor();
        if (file.existsAsFile())
            file.revealToUser();
    }
    // Klick nicht zusaetzlich als Button-Klick behandeln
    setState (juce::Button::buttonNormal);
    juce::ignoreUnused (e);
}

void DragExportButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    auto bg = Colours::panelHi;
    if (down) bg = bg.brighter (0.15f);
    else if (highlighted) bg = bg.brighter (0.08f);
    g.setColour (bg);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colour.withAlpha (highlighted ? 0.9f : 0.55f));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);

    // kleines "Ziehen"-Symbol
    auto icon = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.3f);
    g.setColour (colour);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 2; ++j)
            g.fillEllipse (icon.getX() + j * icon.getWidth() * 0.55f, icon.getY() + i * icon.getHeight() * 0.4f, 2.6f, 2.6f);

    g.setColour (Colours::text);
    g.setFont (uiFont (13.0f, true));
    g.drawFittedText (getButtonText(), r.toNearestInt().withTrimmedRight (6), juce::Justification::centred, 1);
}

//==============================================================================
void LevelMeter::setLevel (float linear)
{
    if (std::abs (linear - level) > 0.002f)
    {
        level = linear;
        repaint();
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::panelHi);
    g.fillRoundedRectangle (r, 3.0f);
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto fill = r.withWidth (r.getWidth() * frac);
    const auto c = db > -3.0f ? Colours::record : (db > -18.0f ? juce::Colour (0xffffc04d) : Colours::play);
    g.setColour (c);
    g.fillRoundedRectangle (fill, 3.0f);
}

//==============================================================================
void drawSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour colour)
{
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (area.toFloat(), 10.0f);
    g.setColour (Colours::outline.withAlpha (0.7f));
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 10.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (colour);
        g.setFont (uiFont (12.5f, true));
        g.drawText (title.toUpperCase(), area.reduced (12, 6).removeFromTop (16), juce::Justification::centredLeft);
    }
}

} // namespace melosumm::ui
