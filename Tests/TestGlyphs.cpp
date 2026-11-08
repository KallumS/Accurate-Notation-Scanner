#include "Check.h"
#include "Glyphs.h"

using namespace ans;

TEST ("SMuFL code points name the symbol")
{
    CHECK_EQ (std::string (glyphs::smuflName (0xE0A4)), std::string ("noteheadBlack"));
    CHECK_EQ (std::string (glyphs::smuflName (0xE050)), std::string ("gClef"));
    CHECK_EQ (glyphs::name ("ABCDEF+Bravura", 0xE262, "").name, std::string ("accidentalSharp"));
    CHECK_EQ (glyphs::name ("Leland", 0xE4E5, "").name, std::string ("restQuarter"));
}

TEST ("Ordinary letters in a text font are text")
{
    CHECK (glyphs::name ("Times-Roman", 'A', "A").name.empty());
    CHECK (! glyphs::name ("Times-Roman", 'A', "A").musicFontUnread);
}

TEST ("LilyPond's Emmentaler names map to SMuFL names")
{
    CHECK_EQ (glyphs::name ("MZOTFV+Emmentaler-20", 0xFFFD, "noteheads.s2").name, std::string ("noteheadBlack"));
    CHECK_EQ (glyphs::name ("Emmentaler-20", 0, "clefs.F_change").name, std::string ("fClefChange"));
    CHECK_EQ (glyphs::name ("Emmentaler-Brace", 0, "brace212").name, std::string ("brace"));
    CHECK (glyphs::name ("Emmentaler-20", 0, "space").name.empty());
}

TEST ("Every LilyPond mapping is a real SMuFL name")
{
    for (const char* n : { "noteheads.s0", "noteheads.s1", "noteheads.sM1", "rests.0", "rests.0o", "rests.3", "rests.M1",
                           "accidentals.flatflat", "clefs.C", "clefs.percussion", "timesig.C22", "zero", "nine", "dots.dot",
                           "flags.u3", "flags.d10", "flags.ugrace", "scripts.staccato", "scripts.sforzato", "scripts.dfermata",
                           "scripts.prall", "scripts.espr", "scripts.caesura.curved", "f", "z", "pedal.Ped", "brackettips.up",
                           "noteheads.s2xcircle", "noteheads.s0slash", "scripts.uportato", "scripts.open", "clefs.tenorG" })
    {
        const auto m = glyphs::fromLilyPond (n);
        if (! glyphs::isSmuflName (m)) check::fail (__FILE__, __LINE__, std::string (n) + " -> '" + m + "'");
    }
}

TEST ("Letter-mapped music fonts are recognised, not read")
{
    const auto n = glyphs::name ("Maestro", 'q', "q");
    CHECK (n.name.empty());
    CHECK (n.musicFontUnread);
}
