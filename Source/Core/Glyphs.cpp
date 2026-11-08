#include "Glyphs.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace ans::glyphs
{

namespace
{
struct Entry { unsigned codepoint; const char* name; };
const Entry smufl[] = {
#include "SmuflNames.inc"
};

const std::unordered_map<unsigned, const char*>& byCodepoint()
{
    static const auto table = [] {
        std::unordered_map<unsigned, const char*> t;
        for (const auto& e : smufl) t.emplace (e.codepoint, e.name);
        return t;
    }();
    return table;
}

const std::unordered_set<std::string>& allNames()
{
    static const auto table = [] {
        std::unordered_set<std::string> t;
        for (const auto& e : smufl) t.insert (e.name);
        return t;
    }();
    return table;
}

std::string lower (std::string s)
{
    for (auto& c : s) c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
    return s;
}

bool contains (const std::string& s, const char* part) { return s.find (part) != std::string::npos; }

bool startsWith (const std::string& s, const char* prefix) { return s.rfind (prefix, 0) == 0; }

// Emmentaler's names for what this app reads. Everything else in the font
// (mensural and shape-note variants, accordion symbols...) is left unnamed.
const std::unordered_map<std::string, std::string>& lilyTable()
{
    static const std::unordered_map<std::string, std::string> t = {
        { "noteheads.sM1", "noteheadDoubleWhole" }, { "noteheads.sM1double", "noteheadDoubleWholeSquare" },
        { "noteheads.s0", "noteheadWhole" }, { "noteheads.s1", "noteheadHalf" }, { "noteheads.s2", "noteheadBlack" },
        { "noteheads.s0cross", "noteheadXWhole" }, { "noteheads.s1cross", "noteheadXHalf" }, { "noteheads.s2cross", "noteheadXBlack" },
        { "noteheads.s0diamond", "noteheadDiamondWhole" }, { "noteheads.s1diamond", "noteheadDiamondHalf" }, { "noteheads.s2diamond", "noteheadDiamondBlack" },
        { "noteheads.s0triangle", "noteheadTriangleUpWhole" }, { "noteheads.s1triangle", "noteheadTriangleUpHalf" }, { "noteheads.s2triangle", "noteheadTriangleUpBlack" },
        { "noteheads.s0slash", "noteheadSlashWhiteWhole" }, { "noteheads.s1slash", "noteheadSlashWhiteHalf" }, { "noteheads.s2slash", "noteheadSlashHorizontalEnds" },
        { "noteheads.s2xcircle", "noteheadCircleX" },

        { "rests.M3", "restMaxima" }, { "rests.M2", "restLonga" }, { "rests.M1", "restDoubleWhole" }, { "rests.M1o", "restDoubleWholeLegerLine" },
        { "rests.0", "restWhole" }, { "rests.0o", "restWholeLegerLine" }, { "rests.1", "restHalf" }, { "rests.1o", "restHalfLegerLine" },
        { "rests.2", "restQuarter" }, { "rests.2classical", "restQuarterOld" }, { "rests.2z", "restQuarterZ" },
        { "rests.3", "rest8th" }, { "rests.4", "rest16th" }, { "rests.5", "rest32nd" }, { "rests.6", "rest64th" },
        { "rests.7", "rest128th" }, { "rests.8", "rest256th" }, { "rests.9", "rest512th" }, { "rests.10", "rest1024th" },

        { "accidentals.sharp", "accidentalSharp" }, { "accidentals.flat", "accidentalFlat" }, { "accidentals.natural", "accidentalNatural" },
        { "accidentals.doublesharp", "accidentalDoubleSharp" }, { "accidentals.flatflat", "accidentalDoubleFlat" },
        { "accidentals.leftparen", "accidentalParensLeft" }, { "accidentals.rightparen", "accidentalParensRight" },

        { "clefs.G", "gClef" }, { "clefs.G_change", "gClefChange" },
        { "clefs.F", "fClef" }, { "clefs.F_change", "fClefChange" },
        { "clefs.C", "cClef" }, { "clefs.C_change", "cClefChange" }, { "clefs.varC", "cClef" }, { "clefs.varC_change", "cClefChange" },
        { "clefs.percussion", "unpitchedPercussionClef1" }, { "clefs.percussion_change", "unpitchedPercussionClef1" },
        { "clefs.varpercussion", "unpitchedPercussionClef2" }, { "clefs.varpercussion_change", "unpitchedPercussionClef2" },
        { "clefs.tab", "6stringTabClef" }, { "clefs.tab_change", "6stringTabClef" },
        { "clefs.tenorG", "gClef8vbCClef" }, { "clefs.tenorG_change", "gClef8vbCClef" },

        { "timesig.C44", "timeSigCommon" }, { "timesig.C22", "timeSigCutCommon" },
        { "zero", "timeSig0" }, { "one", "timeSig1" }, { "two", "timeSig2" }, { "three", "timeSig3" }, { "four", "timeSig4" },
        { "five", "timeSig5" }, { "six", "timeSig6" }, { "seven", "timeSig7" }, { "eight", "timeSig8" }, { "nine", "timeSig9" },
        { "plus", "timeSigPlus" },

        { "dots.dot", "augmentationDot" },

        { "flags.u3", "flag8thUp" }, { "flags.u4", "flag16thUp" }, { "flags.u5", "flag32ndUp" }, { "flags.u6", "flag64thUp" },
        { "flags.u7", "flag128thUp" }, { "flags.u8", "flag256thUp" }, { "flags.u9", "flag512thUp" }, { "flags.u10", "flag1024thUp" },
        { "flags.d3", "flag8thDown" }, { "flags.d4", "flag16thDown" }, { "flags.d5", "flag32ndDown" }, { "flags.d6", "flag64thDown" },
        { "flags.d7", "flag128thDown" }, { "flags.d8", "flag256thDown" }, { "flags.d9", "flag512thDown" }, { "flags.d10", "flag1024thDown" },
        { "flags.ugrace", "graceNoteSlashStemUp" }, { "flags.dgrace", "graceNoteSlashStemDown" },

        { "scripts.staccato", "articStaccatoAbove" }, { "scripts.sforzato", "articAccentAbove" }, { "scripts.tenuto", "articTenutoAbove" },
        { "scripts.ustaccatissimo", "articStaccatissimoAbove" }, { "scripts.dstaccatissimo", "articStaccatissimoBelow" },
        { "scripts.umarcato", "articMarcatoAbove" }, { "scripts.dmarcato", "articMarcatoBelow" },
        { "scripts.uportato", "articTenutoStaccatoAbove" }, { "scripts.dportato", "articTenutoStaccatoBelow" },
        { "scripts.ufermata", "fermataAbove" }, { "scripts.dfermata", "fermataBelow" },
        { "scripts.ushortfermata", "fermataShortAbove" }, { "scripts.dshortfermata", "fermataShortBelow" },
        { "scripts.ulongfermata", "fermataLongAbove" }, { "scripts.dlongfermata", "fermataLongBelow" },
        { "scripts.uverylongfermata", "fermataVeryLongAbove" }, { "scripts.dverylongfermata", "fermataVeryLongBelow" },
        { "scripts.trill", "ornamentTrill" }, { "scripts.turn", "ornamentTurn" }, { "scripts.reverseturn", "ornamentTurnInverted" },
        { "scripts.mordent", "ornamentMordent" }, { "scripts.prall", "ornamentShortTrill" },
        { "scripts.upbow", "stringsUpBow" }, { "scripts.downbow", "stringsDownBow" }, { "scripts.flageolet", "stringsHarmonic" },
        { "scripts.segno", "segno" }, { "scripts.coda", "coda" }, { "scripts.varcoda", "codaSquare" },
        { "scripts.rcomma", "breathMarkComma" }, { "scripts.caesura.straight", "caesura" }, { "scripts.caesura.curved", "caesuraCurved" },
        { "scripts.espr", "articSoftAccentAbove" }, { "scripts.open", "brassMuteOpen" }, { "scripts.stopped", "brassMuteClosed" },

        { "f", "dynamicForte" }, { "m", "dynamicMezzo" }, { "p", "dynamicPiano" }, { "r", "dynamicRinforzando" },
        { "s", "dynamicSforzando" }, { "z", "dynamicZ" }, { "n", "dynamicNiente" },

        { "brackettips.up", "bracketTop" }, { "brackettips.down", "bracketBottom" },
        { "pedal.Ped", "keyboardPedalPed" }, { "pedal.*", "keyboardPedalUp" },
    };
    return t;
}

// Fonts that hold music on ordinary letters. Recognised, not read.
bool isLetterMappedMusicFont (const std::string& lowerName)
{
    for (const char* f : { "maestro", "opus", "petrucci", "sonata", "engraver", "jazz", "reprise",
                           "inkpen", "helsinki", "norfolk", "tamburo", "toccata", "november", "golden age", "broadway copyist" })
        if (contains (lowerName, f)) return true;
    return false;
}
} // namespace

const char* smuflName (unsigned codepoint)
{
    const auto& t = byCodepoint();
    const auto it = t.find (codepoint);
    return it == t.end() ? "" : it->second;
}

bool isSmuflName (const std::string& name) { return allNames().count (name) > 0; }

std::string fromLilyPond (const std::string& glyphName)
{
    const auto& t = lilyTable();
    const auto it = t.find (glyphName);
    if (it != t.end()) return it->second;
    if (startsWith (glyphName, "brace") && glyphName.size() > 5 && std::isdigit (static_cast<unsigned char> (glyphName[5])))
        return "brace";
    return {};
}

std::string baseFontName (const std::string& fontName)
{
    const auto plus = fontName.find ('+');
    if (plus == 6) return fontName.substr (7);
    return fontName;
}

Naming name (const std::string& fontName, unsigned unicode, const std::string& glyphName)
{
    Naming out;
    const auto font = lower (baseFontName (fontName));

    const bool lily = contains (font, "emmentaler") || contains (font, "gonville") || contains (font, "feta")
                   || contains (font, "lilyjazz") || contains (font, "lilypond");
    if (lily)
    {
        out.name = fromLilyPond (glyphName);
        return out;
    }

    if (unicode >= 0xE000 && unicode <= 0xF8FF)
    {
        out.name = smuflName (unicode);
        if (! out.name.empty()) return out;
    }

    // A SMuFL font without Unicode values may still carry its glyph names.
    if (glyphName.size() == 7 && startsWith (glyphName, "uni"))
    {
        const auto cp = static_cast<unsigned> (std::strtoul (glyphName.c_str() + 3, nullptr, 16));
        if (cp >= 0xE000 && cp <= 0xF8FF) out.name = smuflName (cp);
        if (! out.name.empty()) return out;
    }
    if (! glyphName.empty() && isSmuflName (glyphName))
    {
        out.name = glyphName;
        return out;
    }

    if (isLetterMappedMusicFont (font)) out.musicFontUnread = true;
    return out;
}

} // namespace ans::glyphs
