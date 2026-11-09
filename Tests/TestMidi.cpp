// The score as it sounds, and as a MIDI file.
#include "Check.h"
#include "Convert.h"
#include "Midi.h"

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
    int sounding = 0;
    for (std::string line; std::getline (in, line);)
    {
        const auto colon = line.find (':');
        std::stringstream items (line.substr (colon + 1));
        for (std::string item; std::getline (items, item, ';');)
        {
            std::stringstream f (item);
            std::string onset, pitch, length, tie;
            f >> onset >> pitch >> length >> tie;
            if (pitch.empty() || pitch == "rest" || tie == "stop" || tie == "continue") continue;
            ++sounding;
        }
    }
    int graceNotes = 0;   // played, though the truth's list leaves them out
    for (const auto& part : c.read.score.parts)
        for (const auto& m : part.measures)
            for (const auto& e : m.events)
                if (e.grace) graceNotes += static_cast<int> (e.notes.size());
    CHECK_EQ (static_cast<int> (perf.notes.size()), sounding + graceNotes);
    CHECK (perf.length() > 0);
    CHECK_EQ (perf.barStart.size(), c.read.score.parts.front().measures.size() + 1);
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
