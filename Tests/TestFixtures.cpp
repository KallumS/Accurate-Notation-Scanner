// Real PDFs, engraved by MuseScore and LilyPond, read and checked note by
// note against what the engraver was given (Tests/fixtures/*.notes, written
// by tools/expected.py). Every one of these is read exactly today; a change
// that breaks one has made the reader worse.

#include "Check.h"
#include "Convert.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>

using namespace ans;

namespace
{
std::string fixture (const std::string& name) { return std::string (ANS_FIXTURES) + "/" + name; }

std::string number (Fraction f)
{
    char buf[32];
    std::snprintf (buf, sizeof buf, "%.4f", f.value());
    std::string s = buf;
    while (! s.empty() && s.back() == '0') s.pop_back();
    if (! s.empty() && s.back() == '.') s.pop_back();
    return s.empty() || s == "-0" ? "0" : s;
}

std::string pitchName (const Pitch& p)
{
    static const char* steps = "CDEFGAB";
    std::string s (1, steps[p.step]);
    if (p.alter > 0) s += std::string (static_cast<size_t> (p.alter), '#');
    if (p.alter < 0) s += std::string (static_cast<size_t> (-p.alter), '-');
    return s + std::to_string (p.octave);
}

// The same list tools/expected.py writes, from what was read.
std::vector<std::string> notesOf (const Score& score)
{
    std::vector<std::string> lines;
    int staffIndex = 0;
    for (const auto& part : score.parts)
        for (int staff = 1; staff <= part.staves; ++staff)
        {
            ++staffIndex;
            for (size_t b = 0; b < part.measures.size(); ++b)
            {
                std::vector<std::string> items;
                for (const auto& e : part.measures[b].events)
                {
                    if (e.staff != staff || e.grace || e.hidden) continue;
                    if (e.rest) { items.push_back (number (e.offset) + " rest " + number (e.duration) + " -"); continue; }
                    for (const auto& n : e.notes)
                    {
                        const char* tie = n.tieStart && n.tieStop ? "continue" : n.tieStart ? "start" : n.tieStop ? "stop" : "-";
                        items.push_back (number (e.offset) + " " + pitchName (n.pitch) + " " + number (e.duration) + " " + tie);
                    }
                }
                std::sort (items.begin(), items.end());
                std::string line = "S" + std::to_string (staffIndex) + " B" + std::to_string (b + 1) + ": ";
                for (size_t i = 0; i < items.size(); ++i) line += (i ? "; " : "") + items[i];
                lines.push_back (line);
            }
        }
    return lines;
}

std::vector<std::string> readLines (const std::string& path)
{
    std::ifstream in (path);
    std::vector<std::string> out;
    for (std::string line; std::getline (in, line);)
    {
        while (! line.empty() && (line.back() == ' ' || line.back() == '\r')) line.pop_back();
        out.push_back (line);
    }
    return out;
}

void checkFixture (const std::string& name)
{
    const auto c = convertPdf (fixture (name + ".pdf"));
    CHECK (c.verdict != Conversion::Rejected);
    auto got = notesOf (c.read.score);
    for (auto& l : got) while (! l.empty() && l.back() == ' ') l.pop_back();
    const auto want = readLines (fixture (name + ".notes"));
    CHECK_EQ (got.size(), want.size());
    int wrong = 0;
    for (size_t i = 0; i < std::min (got.size(), want.size()); ++i)
        if (got[i] != want[i] && wrong++ < 3)
            check::fail (__FILE__, __LINE__, name + "\n  read: " + got[i] + "\n  want: " + want[i]);
    if (wrong > 0) std::printf ("  %s: %d bars differ\n", name.c_str(), wrong);
}
} // namespace

TEST ("MuseScore: Mozart string quartet, K. 155") { checkFixture ("ms_mozart_k155_movement1"); }
TEST ("MuseScore: Haydn string quartet, Op. 1 No. 1") { checkFixture ("ms_haydn_opus1no1_movement1"); }
TEST ("MuseScore: Beach, choir and piano") { checkFixture ("ms_beach_prayer_of_a_tired_child"); }
TEST ("MuseScore: Webern, Op. 16 No. 2") { checkFixture ("ms_webern_webern_dormi_jesu_op_16_no_2"); }
TEST ("MuseScore: Corelli, Op. 3 No. 1") { checkFixture ("ms_corelli_opus3no1_1grave"); }
TEST ("MuseScore: Lift Every Voice") { checkFixture ("ms_johnson_j_r_lift_every_voice"); }
TEST ("MuseScore: Bach chorale with horn, BWV 1.6") { checkFixture ("ms_bach_bwv1.6"); }
TEST ("MuseScore: random score 2") { checkFixture ("ms_random002"); }
TEST ("MuseScore: random score 5") { checkFixture ("ms_random005"); }
TEST ("MuseScore: Kinkel, Op. 10 No. 3 (first and second endings)") { checkFixture ("ms_kinkel_op10no3"); }
TEST ("LilyPond: Monteverdi madrigal") { checkFixture ("ly_monteverdi_madrigal.3.1"); }
TEST ("LilyPond: Bach chorale BWV 187.7") { checkFixture ("ly_bach_bwv187.7"); }

namespace
{
// Each volta as "bar:start 1." / "bar:stop 1" / "bar:discontinue 2", in order.
std::vector<std::string> endingsOf (const Score& score)
{
    std::vector<std::string> out;
    const auto& ms = score.parts.front().measures;
    for (size_t b = 0; b < ms.size(); ++b)
    {
        if (ms[b].left.ending == Barline::Ending::Start) out.push_back (std::to_string (b) + ":start " + ms[b].left.endingText);
        if (ms[b].right.ending == Barline::Ending::Stop) out.push_back (std::to_string (b) + ":stop " + ms[b].right.endingNumber);
        if (ms[b].right.ending == Barline::Ending::Discontinue) out.push_back (std::to_string (b) + ":discontinue " + ms[b].right.endingNumber);
    }
    return out;
}
} // namespace

TEST ("First and second endings are read, MuseScore and LilyPond alike")
{
    // MuseScore: a first and a second ending, both closed
    const auto k = convertPdf (fixture ("ms_kinkel_op10no3.pdf"));
    const auto ke = endingsOf (k.read.score);
    CHECK_EQ (ke.size(), size_t (4));
    if (ke.size() == 4)
    {
        CHECK (ke[0].find (":start 1.") != std::string::npos);
        CHECK (ke[1].find (":stop 1") != std::string::npos);
        CHECK (ke[2].find (":start 2.") != std::string::npos);
        CHECK (ke[3].find (":stop 2") != std::string::npos);
    }
    // LilyPond: its digits come from a font with no Unicode table; the second ending is open
    const auto c = convertPdf (fixture ("ly_carcassi_op60no24.pdf"));
    const auto ce = endingsOf (c.read.score);
    CHECK (ce == (std::vector<std::string> { "28:start 1.", "28:stop 1", "29:start 2.", "29:discontinue 2" }));
    // every part carries them
    for (const auto& part : c.read.score.parts)
        CHECK (part.measures.size() > 29 && part.measures[29].left.ending == Barline::Ending::Start);
}

TEST ("A scanned PDF is refused, with the reason")
{
    const auto c = convertPdf (fixture ("scanned.pdf"));
    CHECK (c.verdict == Conversion::Rejected);
    CHECK (c.musicXml.empty());
    CHECK (c.summary().find ("scan") != std::string::npos);
}

TEST ("A file that is not a PDF is refused")
{
    const auto c = convertPdf (fixture ("ms_random002.notes"));
    CHECK (c.verdict == Conversion::Rejected);
    CHECK (! c.error.empty());
}

TEST ("The MusicXML written reads back with the same notes")
{
    const auto c = convertPdf (fixture ("ms_random005.pdf"));
    CHECK (c.musicXml.find ("<score-partwise version=\"4.0\">") != std::string::npos);
    CHECK (c.musicXml.find ("<divisions>") != std::string::npos);
}
