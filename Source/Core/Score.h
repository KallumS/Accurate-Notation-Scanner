/*
    Score - the music as written on the page, ready to be saved as MusicXML.

    It keeps what the engraver showed, not what it sounds like: the spelling
    of every note, which notes carry accidentals, ties, dots, tuplets,
    voices, clefs. Durations are exact fractions of a quarter note, so a
    triplet is exactly a third and nothing is rounded.
*/

#pragma once

#include "Ink.h"

#include <cstdlib>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

namespace ans
{

struct Fraction
{
    long long n = 0, d = 1;

    Fraction() = default;
    Fraction (long long num, long long den = 1) : n (num), d (den) { normalise(); }

    void normalise()
    {
        if (d < 0) { n = -n; d = -d; }
        const long long g = std::gcd (n < 0 ? -n : n, d);
        if (g > 1) { n /= g; d /= g; }
        if (n == 0) d = 1;
    }
    friend Fraction operator+ (Fraction a, Fraction b) { return { a.n * b.d + b.n * a.d, a.d * b.d }; }
    friend Fraction operator- (Fraction a, Fraction b) { return { a.n * b.d - b.n * a.d, a.d * b.d }; }
    friend Fraction operator* (Fraction a, Fraction b) { return { a.n * b.n, a.d * b.d }; }
    friend Fraction operator/ (Fraction a, Fraction b) { return { a.n * b.d, a.d * b.n }; }
    Fraction& operator+= (Fraction b) { return *this = *this + b; }
    Fraction& operator-= (Fraction b) { return *this = *this - b; }
    friend bool operator== (Fraction a, Fraction b) { return a.n == b.n && a.d == b.d; }
    friend bool operator!= (Fraction a, Fraction b) { return ! (a == b); }
    friend bool operator< (Fraction a, Fraction b) { return a.n * b.d < b.n * a.d; }
    friend bool operator> (Fraction a, Fraction b) { return b < a; }
    friend bool operator<= (Fraction a, Fraction b) { return ! (b < a); }
    friend bool operator>= (Fraction a, Fraction b) { return ! (a < b); }
    double value() const { return static_cast<double> (n) / static_cast<double> (d); }
    std::string str() const { return d == 1 ? std::to_string (n) : std::to_string (n) + "/" + std::to_string (d); }
};

enum class ClefSign { G, F, C, Percussion, Tab };

struct Clef
{
    ClefSign sign = ClefSign::G;
    int line = 2;          // staff line the clef sits on, 1 = bottom
    int octave = 0;        // -1: sounds an octave lower (treble 8vb), +1 higher
    friend bool operator== (const Clef& a, const Clef& b) { return a.sign == b.sign && a.line == b.line && a.octave == b.octave; }
    friend bool operator!= (const Clef& a, const Clef& b) { return ! (a == b); }
};

struct TimeSignature
{
    int beats = 4, beatType = 4;
    enum Symbol { Numbers, Common, Cut } symbol = Numbers;
    Fraction length() const { return Fraction (beats * 4, beatType); }
    friend bool operator== (const TimeSignature& a, const TimeSignature& b) { return a.beats == b.beats && a.beatType == b.beatType && a.symbol == b.symbol; }
};

struct Pitch
{
    int step = 0;      // 0 = C ... 6 = B
    int octave = 4;    // scientific: middle C is C4
    int alter = 0;     // -2..2 semitones
};

struct Note
{
    Pitch pitch;
    bool accidentalShown = false;
    int accidental = 0;            // what is shown: -2 double flat ... 0 natural ... 2 double sharp
    bool cautionary = false;       // in brackets
    bool tieStart = false, tieStop = false;
    std::string notehead;          // "", "x", "diamond", "triangle", "slash"
    int line = 0;                  // staff position, half-spaces above the bottom line (for checking)
};

struct Tuplet
{
    int actual = 0, normal = 0;    // 3 in the time of 2: actual 3, normal 2
    bool start = false, stop = false;
    bool bracket = false;
    bool active() const { return actual > 0; }
};

struct Event
{
    bool rest = false;
    bool measureRest = false;      // a whole rest filling the bar
    bool hidden = false;           // not printed: a bar left empty on the page
    bool grace = false, graceSlash = false;
    int type = 2;                  // log2 of the note value: -1 breve, 0 whole, 1 half, 2 quarter, 3 eighth...
    int dots = 0;
    Tuplet tuplet;
    Fraction duration;             // in quarter notes; zero for a grace note
    Fraction offset;               // from the start of the bar
    int voice = 1;                 // within its staff, 1-based
    int staff = 1;                 // within its part, 1-based
    int stem = 0;                  // 1 up, -1 down, 0 none
    std::vector<int> beams;        // per beam level: 0 none, 1 begin, 2 continue, 3 end, 4 forward hook, 5 backward hook
    std::vector<Note> notes;       // empty for a rest
    std::vector<std::string> articulations; // MusicXML names: "staccato", "accent", "tenuto", "fermata"...
    double x = 0;                  // where it is on the page
};

struct Direction
{
    Fraction offset;
    int staff = 1;
    bool below = true;
    std::string dynamics;          // "p", "mf", "sfz"...
    std::string words;
};

struct ClefChange
{
    Fraction offset;
    int staff = 1;
    Clef clef;
};

enum class BarStyle { Regular, LightLight, LightHeavy, HeavyLight, Heavy, Dashed };

struct Barline
{
    BarStyle style = BarStyle::Regular;
    bool repeat = false;           // forward on a left barline, backward on a right one
};

// Where a bar is on the page, per staff: for showing what was read.
struct PlaceOnPage
{
    int page = 0;
    Box box;
};

struct Measure
{
    std::string number;
    bool implicit = false;         // a pickup: not counted
    std::optional<int> key;        // fifths, when it changes (or is first given) here
    std::optional<TimeSignature> time;
    std::vector<ClefChange> clefs; // offset 0 entries are the bar's starting clefs
    std::vector<Event> events;     // staff by staff, voice by voice, in time order
    std::vector<Direction> directions;
    Barline left, right;
    bool hasLeft = false;
    int multiRest = 0;             // the first bar of a multi-bar rest: how many bars
    std::vector<PlaceOnPage> places;
};

struct Part
{
    std::string name, abbreviation;
    int staves = 1;
    std::vector<Measure> measures;
};

struct Score
{
    std::string title, composer;
    std::vector<Part> parts;
};

// Something the reader could not be sure of, or had to leave out. Every one
// is shown to the user, with where it is on the page.
struct Issue
{
    enum Level { Info, Warning, Error } level = Warning;
    int page = -1;
    Box box;
    int measureIndex = -1;         // 0-based bar in the score, or -1
    int part = -1;                 // 0-based, or -1
    std::string message;
};

} // namespace ans
