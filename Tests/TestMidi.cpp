// The score as it sounds, and as a MIDI file.
#include "Check.h"
#include "Convert.h"
#include "Midi.h"

#include <functional>
#include <map>

#include <fstream>
#include <sstream>

using namespace ans;

TEST ("Written pitches become MIDI note numbers")
{
    CHECK_EQ (midiPitch ({ 0, 4, 0 }), 60);      // middle C
    CHECK_EQ (midiPitch ({ 5, 4, 0 }), 69);      // A4
    CHECK_EQ (midiPitch ({ 3, 4, 1 }), 66);      // F#4
    CHECK_EQ (midiPitch ({ 6, 3, -1 }), 58);     // B-flat 3
}

TEST ("Part names choose instruments")
{
    CHECK_EQ (programFor ("Violin I"), 40);
    CHECK_EQ (programFor ("Vc."), 42);
    CHECK_EQ (programFor ("Viola"), 41);
    CHECK_EQ (programFor ("Piano"), 0);
    CHECK_EQ (programFor ("Soprano"), 52);
    CHECK_EQ (programFor ("Clarinet in B-flat"), 71);
}

TEST ("Every sounding note of a read score is played once, ties joined")
{
    const auto c = convertPdf (std::string (ANS_FIXTURES) + "/ms_haydn_opus1no1_movement1.pdf");
    const auto perf = perform (c.read.score);
    // the truth's notes that start a sound: not rests, not the end of a tie
    std::ifstream in (std::string (ANS_FIXTURES) + "/ms_haydn_opus1no1_movement1.notes");
    // per bar (lines are "S1 B12: ..."), then counted as often as the bar is played
    std::map<int, int> soundingInBar;
    for (std::string line; std::getline (in, line);)
    {
        const auto colon = line.find (':');
        const int bar = std::stoi (line.substr (line.find (" B") + 2, colon)) - 1;
        std::stringstream items (line.substr (colon + 1));
        for (std::string item; std::getline (items, item, ';');)
        {
            std::stringstream f (item);
            std::string onset, pitch, length, tie;
            f >> onset >> pitch >> length >> tie;
            if (pitch.empty() || pitch == "rest" || tie == "stop" || tie == "continue") continue;
            ++soundingInBar[bar];
        }
    }
    // The order the bars are played in, from MuseScore's own MusicXML of this
    // score (written out here, not taken from the code under test): a pickup
    // and bars 1-24 twice, bars 25-63 twice, then 64-65.
    std::vector<int> order;
    for (int pass = 0; pass < 2; ++pass) for (int b = 0; b <= 24; ++b) order.push_back (b);
    for (int pass = 0; pass < 2; ++pass) for (int b = 25; b <= 63; ++b) order.push_back (b);
    order.push_back (64);
    order.push_back (65);
    CHECK (perf.barPlayed == order);
    int sounding = 0, graceNotes = 0;   // grace notes are played, though the truth's list leaves them out
    for (int b : order)
    {
        sounding += soundingInBar[b];
        for (const auto& part : c.read.score.parts)
            for (const auto& e : part.measures[static_cast<size_t> (b)].events)
                if (e.grace) graceNotes += static_cast<int> (e.notes.size());
    }
    CHECK_EQ (static_cast<int> (perf.notes.size()), sounding + graceNotes);
    CHECK (perf.length() > 0);
    CHECK_EQ (perf.barStart.size(), perf.barPlayed.size() + 1);
}

TEST ("A MIDI file has a header, a tempo track and a track per part")
{
    const auto c = convertPdf (std::string (ANS_FIXTURES) + "/ms_random005.pdf");
    const auto file = toMidiFile (c.read.score, perform (c.read.score));
    CHECK (file.rfind ("MThd", 0) == 0);
    const int tracks = (static_cast<unsigned char> (file[10]) << 8) | static_cast<unsigned char> (file[11]);
    CHECK_EQ (tracks, static_cast<int> (c.read.score.parts.size()) + 1);
    int found = 0;
    for (size_t at = file.find ("MTrk"); at != std::string::npos; at = file.find ("MTrk", at + 4)) ++found;
    CHECK_EQ (found, tracks);
}

TEST ("The tempo comes from the metronome mark")
{
    // MuseScore printed "quarter = 100" over the Beach song (decision: tempo read from the page)
    const auto c = convertPdf (std::string (ANS_FIXTURES) + "/ms_beach_prayer_of_a_tired_child.pdf");
    CHECK (c.read.score.quartersPerMinute > 0);
}

namespace
{
ans::Score barsWith (int n, std::function<void (std::vector<ans::Measure>&)> mark)
{
    ans::Score s;
    s.parts.resize (1);
    s.parts[0].measures.resize (static_cast<size_t> (n));
    mark (s.parts[0].measures);
    return s;
}

void ending (ans::Measure& first, ans::Measure& last, const char* number, bool closed)
{
    first.left.ending = ans::Barline::Ending::Start;
    first.left.endingNumber = number;
    last.right.ending = closed ? ans::Barline::Ending::Stop : ans::Barline::Ending::Discontinue;
    last.right.endingNumber = number;
}
} // namespace

TEST ("Repeats are played twice, and first and second endings in turn")
{
    // |: 0 1 | 2 :| 3 |   (2 is the first ending, 3 the second) then 4 |: 5 :|
    const auto s = barsWith (6, [] (auto& m) {
        ending (m[2], m[2], "1", true);
        m[2].right.repeat = true;
        ending (m[3], m[3], "2", false);
        m[5].left.repeat = true;
        m[5].right.repeat = true;
    });
    CHECK (ans::playOrder (s) == (std::vector<int> { 0, 1, 2, 0, 1, 3, 4, 5, 5 }));

    // endings "1., 2." and "3.": three times round
    const auto t = barsWith (3, [] (auto& m) {
        ending (m[1], m[1], "1, 2", true);
        m[1].right.repeat = true;
        ending (m[2], m[2], "3", false);
    });
    CHECK (ans::playOrder (t) == (std::vector<int> { 0, 1, 0, 1, 0, 2 }));

    // no repeats: as written
    CHECK (ans::playOrder (barsWith (3, [] (auto&) {})) == (std::vector<int> { 0, 1, 2 }));
}
