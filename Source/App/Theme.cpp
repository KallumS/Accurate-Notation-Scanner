#include "Theme.h"

namespace ans::theme
{

LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, ground);
    setColour (juce::TextButton::buttonColourId, control);
    setColour (juce::TextButton::textColourOffId, ink);
    setColour (juce::TextButton::textColourOnId, ink);
    setColour (juce::ListBox::backgroundColourId, sunken);
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff585F6B));
    setColour (juce::Label::textColourId, text);
    setColour (juce::TooltipWindow::backgroundColourId, sunken);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::AlertWindow::backgroundColourId, ground);
    setColour (juce::AlertWindow::textColourId, text);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    juce::Colour c = ! b.isEnabled() ? control.withAlpha (0.35f) : down ? controlHeld : over ? controlHover : control;
    if (b.getToggleState()) c = accent;
    g.setColour (c);
    g.fillRoundedRectangle (r, 4.0f);
    if (b.hasKeyboardFocus (false))
    {
        g.setColour (text);
        g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 1.5f);
    }
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int height)
{
    return juce::Font (juce::FontOptions (std::min (15.0f, height * 0.55f)));
}

} // namespace ans::theme
