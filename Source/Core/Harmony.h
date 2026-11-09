/*
    Harmony - which chords each bar outlines.

    An analysis, not something read from the page: the score's notes are
    named as chords by ScaleView Pro's reader (Chords.h). It works from the
    read Score, all parts together:

    - Each bar is cut in two (or, in a bar of three beats, into beats): the
      steps Mozart's analysts' chord changes were best matched at.
    - Passing and neighbour notes are left out, as Open Music Theory defines
      them ("Embellishing tones"): a note a step from the notes on both sides
      of it in the same line, moving on in the same direction (passing) or
      coming back (neighbour), joined to both and falling off the beat.
    - Every note left that sounds in that part of the bar is named, the
      lowest as the bass. (Keeping only the longer-sounding notes was
      measured too, and did worse once the decorations were gone: it lost
      the short notes of an arpeggio.)
    - The key signature's major key spells the names and settles ScaleView's
      draws; a bar's halves with the same name are one chord.

    Measured against the DCML analysis of all 54 Mozart sonata movements
    (docs/decisions/0011): the root agrees on 58.6% of beats, where naming
    only the analysts' own chord notes would agree on 71.6% - the analysts
    name harmonic function, which the notes alone do not always show.
*/

#pragma once

#include "Score.h"

#include <string>
#include <vector>

namespace ans
{

struct OutlinedChord
{
    int bar = 0;            // index into a part's measures
    Fraction offset;        // from the start of the bar, in quarter notes
    std::string name;       // ScaleView's symbol: "C", "G7/B", "Dmin"; never a list of notes
};

// In bar order. A part of a bar with no notes, or only one or two that make
// no chord, gives nothing.
std::vector<OutlinedChord> outlineChords (const Score& score);

} // namespace ans
