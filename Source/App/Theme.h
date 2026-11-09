/*
    Theme - the family's colours (Noterator's Theme.h, Good Idea's
    COLOUR.md): a dark cool-grey ground, light grey controls with dark ink,
    one yellow for what is chosen. The score itself is shown on white paper,
    as printed. Problems are marked in the house red and written in the
    lighter warnText, which is readable on the ground.

    Apple's guidelines (decision 0014, as Noterator's 0048): text is at least
    7:1 on what it sits on; with the Mac's Increase Contrast on,
    useIncreasedContrast() lifts secondary text, rules and the warning text
    further. A control the keyboard has reached wears a ring in the text
    colour (focusRing), never the yellow, which means only "on"; a control
    that is on also carries an ink bar under its label, so "on" is never
    told by colour alone.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ans
{
// Text beyond ASCII (an ellipsis, curly quotes) goes through here.
inline juce::String ui (const char* utf8) { return juce::String::fromUTF8 (utf8); }
}

namespace ans::theme
{
inline const juce::Colour accent    { 0xffFFF200 };
inline const juce::Colour control   { 0xffA9AFBA };
inline const juce::Colour controlHover { 0xffC0C6CF };
inline const juce::Colour controlHeld  { 0xff8F96A2 };
inline const juce::Colour ground    { 0xff23272E };
inline const juce::Colour sunken    { 0xff1A1D23 };
inline const juce::Colour ink       { 0xff14171C };
inline const juce::Colour text      { 0xffDDE1E7 };
inline const juce::Colour textOff   { 0xff8A919C };   // edges and a control that cannot be used; never words
inline const juce::Colour warn      { 0xffD2483F };   // marks; 3.4:1, so never text on the ground
inline const juce::Colour good      { 0xff8FD19E };   // 9.2:1 on the ground
inline const juce::Colour scrollGrab { 0xff6D7581 };  // 3.3:1 on the ground (3:1 for things to grab)

// These change with the Mac's Increase Contrast (useIncreasedContrast).
inline juce::Colour textDim   { 0xffB0B6C0 };   // secondary text, 7.3:1 on the ground
inline juce::Colour warnText  { 0xffFF9E96 };   // a problem, written: 7.5:1
inline juce::Colour rule      { 0xff3A404A };
inline bool increasedContrast = false;

// Increase Contrast on or off; the caller repaints. (Noterator's Theme.cpp.)
void useIncreasedContrast (bool on);

// A control reached with Tab is ringed in the text colour, inside its own
// bounds; returns what is left for the control itself. (Noterator's.)
juce::Rectangle<float> focusRing (juce::Graphics&, juce::Component&, juce::Rectangle<float> bounds, float corner = 4.0f);

// Clicking a button leaves the keyboard with the window, as on the Mac, so
// Space still plays; Tab reaches the buttons instead. Lists excepted: once
// clicked, they are browsed with the arrows. (Noterator's.)
void clicksKeepFocus (juce::Component&);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int height) override;
};
} // namespace ans::theme
