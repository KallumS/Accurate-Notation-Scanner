// The chord reader is ScaleView Pro's, ported. These are ScaleView's own tests:
// every expect() in its tests/test_scaleview_pro.lua (Tests/ScaleViewCases.inc,
// written by tools/scaleview_cases.py, the expected names untouched), and the
// two properties its suite checks over every voicing.

#include "Check.h"
#include "../Source/Core/Chords.h"

#include <string>
#include <vector>

using namespace ans;

namespace
{
struct Case { std::vector<int> notes; const char* root; const char* scale; const char* want; };
const Case cases[] = {
#include "ScaleViewCases.inc"
};

std::string notesOf (const std::vector<int>& n)
{
    std::string s;
    for (int x : n) s += (s.empty() ? "" : " ") + std::to_string (x);
    return s;
}

// Every set of `size` pitch classes from C upwards, as ScaleView's sweep has it.
void sweep (int start, int left, std::vector<int>& set, std::vector<std::vector<int>>& out)
{
    if (left == 0) { std::vector<int> notes; for (int pc : set) notes.push_back (60 + pc); out.push_back (notes); return; }
    for (int pc = start; pc <= 11; ++pc) { set.push_back (pc); sweep (pc + 1, left - 1, set, out); set.pop_back(); }
}
} // namespace

TEST ("Chords: every case in ScaleView Pro's own test suite")
{
    int failed = 0;
    for (const auto& c : cases)
    {
        const auto key = std::string (c.root).empty() ? chords::Scale {} : chords::Scale::named (c.root, c.scale);
        const auto got = chords::name (c.notes, key);
        if (got != c.want && failed++ < 15)
            check::fail (__FILE__, __LINE__, notesOf (c.notes) + " in " + c.root + " " + c.scale + ": got " + got + ", ScaleView says " + c.want);
    }
    CHECK_EQ (failed, 0);
    CHECK (sizeof (cases) / sizeof (cases[0]) > 100);
}

TEST ("Chords: every three- and four-note voicing gets a chord name (ScaleView 6g)")
{
    std::vector<std::vector<int>> all;
    std::vector<int> set;
    sweep (0, 3, set, all);
    sweep (0, 4, set, all);
    int unnamed = 0;
    for (const auto& n : all)
        if (! chords::isChordSymbol (chords::name (n))) ++unnamed;
    CHECK_EQ (all.size(), size_t (220 + 495));
    CHECK_EQ (unnamed, 0);
}

TEST ("Chords: no key reads draws as C major does (ScaleView 6h)")
{
    std::vector<std::vector<int>> all;
    std::vector<int> set;
    sweep (0, 3, set, all);
    sweep (0, 4, set, all);
    const auto cMajor = chords::Scale::named ("C", "Major");
    int differ = 0;
    for (const auto& n : all)
        if (chords::name (n) != chords::name (n, cMajor)) ++differ;
    CHECK_EQ (differ, 0);
}
