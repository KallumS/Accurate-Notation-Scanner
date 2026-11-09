#include "Theme.h"

namespace ans::theme
{

void useIncreasedContrast (bool on)
{
    increasedContrast = on;
    textDim = juce::Colour (on ? 0xffDDE1E7 : 0xffB0B6C0);
    warnText = juce::Colour (on ? 0xffFFC2BC : 0xffFF9E96);
    rule = juce::Colour (on ? 0xff7A828E : 0xff3A404A);
}

juce::Rectangle<float> focusRing (juce::Graphics& g, juce::Component& c, juce::Rectangle<float> r, float corner)
{
    if (! c.hasKeyboardFocus (true)) return r;
    g.setColour (text);
    g.drawRoundedRectangle (r.reduced (1.0f), corner + 2.0f, 2.0f);
    return r.reduced (4.0f);
}

void clicksKeepFocus (juce::Component& c)
{
    if (dynamic_cast<juce::ListBox*> (&c) != nullptr) return;
    if (auto* port = dynamic_cast<juce::Viewport*> (&c)) port->setWantsKeyboardFocus (false);
    if (dynamic_cast<juce::Button*> (&c) != nullptr) c.setMouseClickGrabsKeyboardFocus (false);
    for (auto* child : c.getChildren()) clicksKeepFocus (*child);
}

LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, ground);
    setColour (juce::TextButton::buttonColourId, control);
    setColour (juce::TextButton::textColourOffId, ink);
    setColour (juce::TextButton::textColourOnId, ink);
    setColour (juce::ListBox::backgroundColourId, sunken);
    setColour (juce::ScrollBar::thumbColourId, scrollGrab);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TooltipWindow::backgroundColourId, sunken);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::AlertWindow::backgroundColourId, ground);
    setColour (juce::AlertWindow::textColourId, text);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = focusRing (g, b, b.getLocalBounds().toFloat()).reduced (0.5f);
    juce::Colour c = ! b.isEnabled() ? control.withAlpha (0.35f) : down ? controlHeld : over ? controlHover : control;
    if (b.getToggleState()) c = accent;
    g.setColour (c);
    g.fillRoundedRectangle (r, 4.0f);
    // "on" is not told by the yellow alone: an ink bar under the label
    if (b.getToggleState())
    {
        g.setColour (ink);
        const float w = std::min (r.getWidth() * 0.4f, 36.0f);
        g.fillRoundedRectangle (r.getCentreX() - w * 0.5f, r.getBottom() - 6.0f, w, 2.5f, 1.0f);
    }
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int height)
{
    return juce::Font (juce::FontOptions (std::min (15.0f, height * 0.55f)));
}

} // namespace ans::theme
