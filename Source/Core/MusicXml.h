/*
    MusicXml - a Score written out as MusicXML 4.0 (partwise), the format
    Dorico, Sibelius, MuseScore, Finale, Noterator and every other notation
    program reads.
*/

#pragma once

#include "Score.h"

#include <string>

namespace ans
{

std::string toMusicXml (const Score& score);

// The divisions per quarter note that express every duration exactly.
long long divisionsFor (const Score& score);

} // namespace ans
