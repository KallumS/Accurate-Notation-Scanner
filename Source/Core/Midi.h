/*
    Midi - the score as it sounds: every note with its start and length in
    quarter notes, its part, channel, General MIDI instrument and loudness.
    Used for playback (the app) and for MIDI export.

    Repeats are played, and first and second endings taken in turn
    (playOrder); D.C., D.S. and codas are not followed.

    Ties are joined into one note. Pitches are as written (MusicXML's
    convention; a transposing instrument is not moved). Grace notes are played
    short, just before their note. The tempo comes from the printed metronome
    mark, else from a tempo word (Allegro, Andante...), else 100.
    Dynamics set the loudness from where they stand.
*/

#pragma once

#include "Score.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ans
{

struct PlayedNote
{
    int part = 0;
    int channel = 0;
    int pitch = 60;          // MIDI note number
    double start = 0;        // quarter notes from the beginning
    double length = 1;
    int velocity = 80;
    int bar = 0;             // index of the score's bar it starts in
};

struct Performance
{
    std::vector<PlayedNote> notes;        // in start order
    std::vector<double> barStart;         // quarter notes, one per bar as played, plus the end
    std::vector<int> barPlayed;           // the score's bar at each of those
    double quartersPerMinute = 100;
    std::vector<int> programOfPart;       // General MIDI program, 0-127
    std::vector<int> channelOfPart;       // 0-15, never 9 (drums)
    double length() const { return barStart.empty() ? 0.0 : barStart.back(); }
    double seconds (double quarters) const { return quarters * 60.0 / quartersPerMinute; }
    double quarters (double seconds) const { return seconds * quartersPerMinute / 60.0; }
    int barAt (double quarters) const;    // the score's bar sounding then
};

// The score's bars in the order they are played: each repeat twice (or as
// many times as its endings' numbers ask), each ending on its own pass.
std::vector<int> playOrder (const Score& score);

Performance perform (const Score& score);

// A Standard MIDI File (type 1): a tempo track, then a track per part.
std::string toMidiFile (const Score& score, const Performance& performance);

// The General MIDI program for a part's name ("Violin I", "Vc.", "Piano").
int programFor (const std::string& partName);

// The MIDI note number of a written pitch.
int midiPitch (const Pitch& p);

} // namespace ans
