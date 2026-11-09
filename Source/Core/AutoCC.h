/*
    AutoCC - controller curves for each note, the way a player rides them.

    This is AutoCC's engine (KallumS/AutoCC, `AutoCC.jsfx`), ported for the
    score instead of a live keyboard. Each note of a line starts an arc on
    four lanes - CC1 dynamics, CC11 expression, CC7 volume, CC21 aux - shaped
    for the instrument family: the note starts at the dynamic and swells
    (strings +31 over 2.2 s and hold it; brass bloom +7 at once and ease
    back; woodwinds +17 within a second), and falls back over a second once
    the last key is up. A note starting within 0.4 s of the last key-up
    carries the line on instead of swelling again. The presets are AutoCC's,
    fitted to Lorne Balfe's mock-ups (AutoCC docs/research/per-note-analysis.md).

    Ported settings: Gated envelope, Legato trigger (first note only), sustain
    pedal not read, 5 ms update interval, depth and time scale 100%. The one
    thing the score adds is AutoCC's "Dynamic" control, moved by the printed
    dynamics (Midi.cpp).

    Keep it alike with the JSFX: a change to a rule goes there first.
*/

#pragma once

#include <array>
#include <utility>
#include <vector>

namespace ans::autocc
{

enum class Preset { Default, Strings, Brass, Woodwinds };

struct Lane
{
    int cc = 0;
    int floor = 0, peak = 0, sustain = 0;
    double attackMs = 0, decayMs = 0, releaseMs = 0;
    int velocityPercent = 0;        // how much velocity scales the swell
    double attackShape = 1, releaseShape = 1;   // curve exponents
};

// AutoCC's built-in preset: lanes CC1, CC11, CC7, CC21 in that order.
std::array<Lane, 4> preset (Preset p);

struct KeyEvent
{
    double seconds = 0;
    int note = 60;
    int velocity = 100;
    bool on = true;
};

struct Control
{
    double seconds = 0;
    int cc = 0;
    int value = 0;
};

struct Settings
{
    Preset preset = Preset::Default;
    // AutoCC's Dynamic (-64..64) from a time on: moves where CC1 and CC11
    // start, peak and sustain. Sorted by time; before the first, 0.
    std::vector<std::pair<double, int>> dynamic;
};

// The CC values AutoCC sends for these keys (one MIDI channel), in time
// order: the resting level of each lane at time 0, then every change, until
// the last arc has fallen back.
std::vector<Control> render (std::vector<KeyEvent> keys, const Settings& settings);

} // namespace ans::autocc
