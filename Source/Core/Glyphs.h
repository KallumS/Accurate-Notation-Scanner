/*
    Glyphs - naming a font character as a music symbol.

    Every name used after the PDF reader is a SMuFL name ("noteheadBlack",
    "gClef", "rest8th"), whichever font the score was set in:

    - SMuFL fonts (Bravura, Leland, Petaluma, Finale Maestro, MuseScore's
      own...) put each symbol at a fixed code point, so the code point names
      it. This covers MuseScore, Dorico, Finale 27 and later, and Sibelius
      scores set in Bravura or Petaluma.
    - LilyPond's Emmentaler (and Gonville) carry their own glyph names
      ("noteheads.s2"), which are translated here.

    Older fonts that put music on ordinary letters (Maestro, Opus, Petrucci)
    are recognised as music fonts but not read: a guess at what "q" means
    would be a guess, and this app does not guess (decision 0002).
*/

#pragma once

#include <string>

namespace ans::glyphs
{

// The SMuFL name at a code point, or "".
const char* smuflName (unsigned codepoint);

// True when the name is a SMuFL glyph name.
bool isSmuflName (const std::string& name);

// LilyPond's Emmentaler glyph name ("noteheads.s2") as a SMuFL name, or "".
std::string fromLilyPond (const std::string& glyphName);

struct Naming
{
    std::string name;            // the SMuFL name, or "" for ordinary text
    bool musicFontUnread = false; // a music font we recognise but cannot read
};

// Names one character from what the PDF says about it: its font's name,
// its Unicode value (0 or 0xFFFD when unknown) and the font's own name for
// the glyph (may be empty).
Naming name (const std::string& fontName, unsigned unicode, const std::string& glyphName);

// The font's name without a subset prefix ("ABCDEF+Bravura" -> "Bravura").
std::string baseFontName (const std::string& fontName);

} // namespace ans::glyphs
