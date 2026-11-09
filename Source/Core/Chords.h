/*
    Chords - naming a set of notes as a chord symbol.

    This is ScaleView Pro's chord reader (KallumS/ScaleView-for-Reaper,
    `reascripts/ScaleView Pro.lua`, `detectChord`), ported rule for rule. A
    chord is read rather than looked up: the third, fifth and seventh above
    each candidate root give its quality from a small table; whatever is
    left is described as sixths, ninths, elevenths and thirteenths. Every
    note is tried as the root, each reading is costed, the cheapest wins, and
    the key only settles a genuine draw. The bass after a slash is the lowest
    note, except that a root doubled in two octaves that is a first, third or
    fifth degree of the key reads as root position.

    Keep it alike with the Lua: a change to a rule goes there first, with its
    test, and is copied here. Its tests are copied into Tests/TestChords.cpp
    with the names ScaleView's own suite expects.
*/

#pragma once

#include <string>
#include <vector>

namespace ans::chords
{

// A scale, as ScaleView's scale list has them: a root spelled by letter and
// accidental, and the scale's intervals with the letter each degree takes.
struct Scale
{
    int rootLetter = 0;              // 0 = C ... 6 = B
    int rootAccidental = 0;          // -1 flat, 1 sharp
    std::vector<int> intervals;      // semitones above the root
    std::vector<int> letters;        // letters above the root letter, per degree

    // "C", "F#", "Bb" with "Major", "Minor (Natural)", "Harmonic Minor"... as
    // ScaleView names them. An unknown name gives an empty scale.
    static Scale named (const std::string& root, const std::string& scale);
    // The major key a key signature of this many fifths (flats negative) shows.
    static Scale majorKeyOf (int fifths);
    bool empty() const { return intervals.empty(); }
};

// The chord the notes make (MIDI note numbers, any order, repeats allowed),
// named for the key. With no key, ScaleView assumes C major for its draws.
// One note is its name; two notes are a chord only as a bare fifth or a
// third ("C5", "Cmaj(no5)"), otherwise the note names; more than seven
// different notes are a cluster and read out as note names too.
std::string name (const std::vector<int>& midiNotes, const Scale& key = {});

// True when name() gave a chord symbol rather than a list of notes.
bool isChordSymbol (const std::string& name);

} // namespace ans::chords
