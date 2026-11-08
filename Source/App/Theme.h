/*
    Theme - the family's colours (Noterator's Theme.h, Good Idea's
    COLOUR.md): a dark cool-grey ground, light grey controls with dark ink,
    one yellow for what is chosen. The score itself is shown on white paper,
    as printed. Problems are marked in the house red and written in the
    lighter warnText, which is readable on the ground.
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
inline const juce::Colour textDim   { 0xffB0B6C0 };
inline const juce::Colour textOff   { 0xff8A919C };
inline const juce::Colour rule      { 0xff3A404A };
inline const juce::Colour warn      { 0xffD2483F };
inline const juce::Colour warnText  { 0xffFF9E96 };
inline const juce::Colour good      { 0xff8FD19E };

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int height) override;
};
} // namespace ans::theme
