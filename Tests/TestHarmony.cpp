// Chords outlined by bars. Each expectation is the music-theory answer for
// the notes written here by hand (Open Music Theory, "Triads and seventh
// chords" and "Embellishing tones"), not anything the code printed.

#include "Check.h"
#include "../Source/Core/Harmony.h"

#include <string>
#include <vector>

using namespace ans;

namespace
{
// "C4", "F#5", "Bb3" -> a pitch
Pitch p (const std::string& s)
{
    static const std::string letters = "CDEFGAB";
    Pitch out;
    out.step = static_cast<int> (letters.find (s[0]));
    size_t i = 1;
    while (i < s.size() && (s[i] == '#' || s[i] == 'b')) out.alter += s[i++] == '#' ? 1 : -1;
    out.octave = std::stoi (s.substr (i));
    return out;
}

Event chord (Fraction at, Fraction length, std::vector<std::string> pitches, int voice = 1, int staff = 1)
{
    Event e;
    e.offset = at;
    e.duration = length;
    e.voice = voice;
    e.staff = staff;
    for (const auto& s : pitches) { Note n; n.pitch = p (s); e.notes.push_back (n); }
    return e;
}

// A line of equal notes from the start of a bar.
void line (Measure& m, Fraction each, std::vector<std::string> pitches, int voice, int staff = 1)
{
    Fraction at;
    for (const auto& s : pitches) { m.events.push_back (chord (at, each, { s }, voice, staff)); at += each; }
}

Score oneBar (int fifths = 0, TimeSignature t = {})
{
    Score s;
    s.parts.resize (1);
    s.parts[0].staves = 2;
    s.parts[0].measures.resize (1);
    s.parts[0].measures[0].key = fifths;
    s.parts[0].measures[0].time = t;
    return s;
}

std::string names (const std::vector<OutlinedChord>& c)
{
    std::string out;
    for (const auto& x : c) out += (out.empty() ? "" : " | ") + std::to_string (x.bar) + "@" + x.offset.str() + " " + x.name;
    return out;
}
} // namespace

TEST ("Harmony: a held C major triad is C, once for the bar")
{
    auto s = oneBar();
    s.parts[0].measures[0].events.push_back (chord (0, 4, { "C3", "E3", "G3" }, 1, 2));
    line (s.parts[0].measures[0], Fraction (1, 2), { "C5", "E5", "G5", "E5", "C5", "E5", "G5", "E5" }, 1);
    CHECK_EQ (names (outlineChords (s)), std::string ("0@0 C"));
}

TEST ("Harmony: passing and neighbour notes off the beat do not change the chord")
{
    // over C E G: C D E F G F E D, then C in the next bar (D, F, F, D pass
    // between chord notes off the beat). This first ended the line on the
    // last D; with nothing after it that D is not a passing note by Open Music
    // Theory's definition (a stable tone either side), so the line now goes
    // on to the C it was meant to reach - corrected 2026-11-09.
    auto s = oneBar();
    s.parts[0].measures.resize (2);
    s.parts[0].measures[0].events.push_back (chord (0, 4, { "C3", "E3", "G3" }, 1, 2));
    line (s.parts[0].measures[0], Fraction (1, 2), { "C5", "D5", "E5", "F5", "G5", "F5", "E5", "D5" }, 1);
    s.parts[0].measures[1].events.push_back (chord (0, 4, { "C3", "E3", "G3" }, 1, 2));
    s.parts[0].measures[1].events.push_back (chord (0, 4, { "C5" }));
    CHECK_EQ (names (outlineChords (s)), std::string ("0@0 C | 1@0 C"));
    // E F E: a neighbour
    auto t = oneBar();
    t.parts[0].measures[0].events.push_back (chord (0, 4, { "C3", "G3" }, 1, 2));
    line (t.parts[0].measures[0], Fraction (1, 2), { "E5", "F5", "E5", "F5", "E5", "F5", "E5", "G5" }, 1);
    CHECK_EQ (names (outlineChords (t)), std::string ("0@0 C"));
}

TEST ("Harmony: a chord change halfway through the bar gives two chords")
{
    // I then V7 in C: C E G, then G B D F over G
    auto s = oneBar();
    s.parts[0].measures[0].events.push_back (chord (0, 2, { "C3", "E4", "G4" }));
    s.parts[0].measures[0].events.push_back (chord (2, 2, { "G2", "B3", "D4", "F4" }));
    CHECK_EQ (names (outlineChords (s)), std::string ("0@0 C | 0@2 G7"));
}

TEST ("Harmony: an arpeggiated bass outlines its chord, inversions named by the lowest note")
{
    // Alberti bass E G C G under a held E: first inversion of C, C/E
    auto s = oneBar();
    line (s.parts[0].measures[0], Fraction (1, 2), { "E3", "G3", "C4", "G3", "E3", "G3", "C4", "G3" }, 1, 2);
    s.parts[0].measures[0].events.push_back (chord (0, 4, { "E5" }));
    CHECK_EQ (names (outlineChords (s)), std::string ("0@0 C/E"));
    // The same bass under a held C: C sounds in two octaves, and ScaleView reads
    // a doubled root of the key as root position (its README: "Play E G C with
    // another C on top and you get C, not C/E"; its decision 0003). This first
    // expected C/E here, which contradicted that rule - corrected 2026-11-09.
    auto t = oneBar();
    line (t.parts[0].measures[0], Fraction (1, 2), { "E3", "G3", "C4", "G3", "E3", "G3", "C4", "G3" }, 1, 2);
    t.parts[0].measures[0].events.push_back (chord (0, 4, { "C5" }));
    CHECK_EQ (names (outlineChords (t)), std::string ("0@0 C"));
}

TEST ("Harmony: names are spelled for the key signature")
{
    // Gb Bb Db with six flats is Gb, not F#
    auto s = oneBar (-6);
    s.parts[0].measures[0].events.push_back (chord (0, 4, { "Gb3", "Bb3", "Db4" }));
    CHECK_EQ (names (outlineChords (s)), std::string ("0@0 Gb"));
}

TEST ("Harmony: three beats in a bar are named beat by beat")
{
    // 3/4: C | F/C | G/B... written as C E G, C F A, B D G
    TimeSignature three;
    three.beats = 3;
    auto s = oneBar (0, three);
    s.parts[0].measures[0].events.push_back (chord (0, 1, { "C3", "E4", "G4" }));
    s.parts[0].measures[0].events.push_back (chord (1, 1, { "C3", "F4", "A4" }));
    s.parts[0].measures[0].events.push_back (chord (2, 1, { "B2", "D4", "G4" }));
    CHECK_EQ (names (outlineChords (s)), std::string ("0@0 C | 0@1 F/C | 0@2 G/B"));
}

TEST ("Harmony: a single line with no chord in it outlines nothing")
{
    auto s = oneBar();
    s.parts[0].measures[0].events.push_back (chord (0, 4, { "C4" }));
    CHECK_EQ (outlineChords (s).size(), size_t (0));
}
