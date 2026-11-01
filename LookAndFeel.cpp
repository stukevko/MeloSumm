#include "LookAndFeel.h"

namespace melosumm::ui
{

juce::Font uiFont (float height, bool bold)
{
    auto opts = juce::FontOptions (height);
    if (bold) opts = opts.withStyle ("Bold");
    return juce::Font (opts);
}

MeloLookAndFeel::MeloLookAndFeel()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, Colours::bg);
    setColour (Label::textColourId, Colours::text);
    setColour (Slider::textBoxTextColourId, Colours::text);
    setColour (Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (ComboBox::backgroundColourId, Colours::panelHi);
    setColour (ComboBox::textColourId, Colours::text);
    setColour (ComboBox::outlineColourId, Colours::outline);
    setColour (ComboBox::arrowColourId, Colours::textDim);
    setColour (PopupMenu::backgroundColourId, Colours::panel);
    setColour (PopupMenu::textColourId, Colours::text);
    setColour (PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha (0.35f));
    setColour (PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (TextButton::buttonColourId, Colours::panelHi);
    setColour (TextButton::textColourOffId, Colours::text);
    setColour (TextButton::textColourOnId, juce::Colours::white);
    setColour (ToggleButton::textColourId, Colours::text);
    setColour (TooltipWindow::backgroundColourId, Colours::panelHi);
    setColour (TooltipWindow::textColourId, Colours::text);
    setColour (TooltipWindow::outlineColourId, Colours::outline);
}

void MeloLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float pos, float start, float end, juce::Slider& s)
{
    using namespace juce;
    auto bounds = Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float lineW = jmax (2.5f, radius * 0.16f);
    const float arcR = radius - lineW * 0.5f;
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);

    Path bgArc;
    bgArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (Colours::outline);
    g.strokePath (bgArc, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));

    const float angle = start + pos * (end - start);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (s.isEnabled() && std::abs (angle - from) > 0.001f)
    {
        Path valArc;
        valArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (accent);
        g.strokePath (valArc, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const float knobR = arcR - lineW * 1.3f;
    g.setColour (Colours::panelHi);
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2, knobR * 2);
    g.setColour (Colours::outline.brighter (0.2f));
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2, knobR * 2, 1.0f);

    Path pointer;
    const float pl = knobR * 0.75f;
    pointer.addRoundedRectangle (-1.5f, -knobR + 2.0f, 3.0f, pl * 0.6f, 1.5f);
    g.setColour (Colours::text);
    g.fillPath (pointer, AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void MeloLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgColour,
                                            bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = bgColour;
    if (b.getToggleState())
        c = b.findColour (juce::TextButton::buttonOnColourId);
    if (down)             c = c.brighter (0.15f);
    else if (highlighted) c = c.brighter (0.07f);
    if (! b.isEnabled())  c = c.withMultipliedAlpha (0.5f);

    g.setColour (c);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::outline.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

juce::Font MeloLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return uiFont (juce::jmin (15.0f, h * 0.48f), true);
}

void MeloLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat();
    const float h = jmin (18.0f, r.getHeight() - 4.0f);
    const float w = h * 1.8f;
    Rectangle<float> sw (r.getX() + 2.0f, r.getCentreY() - h / 2.0f, w, h);

    const bool on = b.getToggleState();
    auto accent = b.findColour (ToggleButton::tickColourId);
    g.setColour (on ? accent : Colours::outline);
    g.fillRoundedRectangle (sw, h / 2.0f);
    if (highlighted)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (sw, h / 2.0f);
    }
    const float d = h - 4.0f;
    const float kx = on ? sw.getRight() - d - 2.0f : sw.getX() + 2.0f;
    g.setColour (juce::Colours::white);
    g.fillEllipse (kx, sw.getY() + 2.0f, d, d);

    g.setColour (b.isEnabled() ? Colours::text : Colours::textDim);
    g.setFont (uiFont (13.0f));
    g.drawFittedText (b.getButtonText(), r.withLeft (sw.getRight() + 7.0f).toNearestInt(), Justification::centredLeft, 2);
}

void MeloLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                    int, int, int, int, juce::ComboBox& box)
{
    using namespace juce;
    auto r = Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.hasKeyboardFocus (true) ? Colours::accent.withAlpha (0.6f) : box.findColour (ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    Path arrow;
    const float ax = (float) width - 14.0f, ay = (float) height * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.5f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (box.findColour (ComboBox::arrowColourId));
    g.strokePath (arrow, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

juce::Font MeloLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return uiFont (juce::jmin (14.0f, box.getHeight() * 0.55f));
}

juce::Font MeloLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

juce::Font MeloLookAndFeel::getPopupMenuFont()
{
    return uiFont (14.0f);
}

} // namespace melosumm::ui
