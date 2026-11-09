// notation-scan: the converter without the window.
//
//   notation-scan score.pdf                  writes score.musicxml beside it
//   notation-scan score.pdf -o out.musicxml  writes there
//   notation-scan score.pdf --report r.json  also writes the report
//   notation-scan score.pdf --midi s.mid     also writes a MIDI file
//   notation-scan score.pdf --stdout         prints the MusicXML
//   notation-scan score.pdf --chords c.txt   also writes the chords each bar
//                                            outlines ("-" prints them), one
//                                            line per bar: number, then
//                                            onset:name in quarter notes
//
// Exit status: 0 every bar checked out, 1 converted with things to look at,
// 2 not converted (a scan, no music, or not a PDF).

#include "Convert.h"
#include "Harmony.h"
#include "Midi.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

int main (int argc, char** argv)
{
    std::string input, output, report, midi, chords;
    bool toStdout = false, quiet = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if ((a == "-o" || a == "--output") && i + 1 < argc) output = argv[++i];
        else if (a == "--report" && i + 1 < argc) report = argv[++i];
        else if (a == "--midi" && i + 1 < argc) midi = argv[++i];
        else if (a == "--chords" && i + 1 < argc) chords = argv[++i];
        else if (a == "--stdout") toStdout = true;
        else if (a == "-q" || a == "--quiet") quiet = true;
        else if (a == "-h" || a == "--help")
        {
            std::printf ("usage: notation-scan score.pdf [-o out.musicxml] [--report report.json] [--midi out.mid] [--chords out.txt|-] [--stdout] [-q]\n");
            return 0;
        }
        else input = a;
    }
    if (input.empty())
    {
        std::fprintf (stderr, "usage: notation-scan score.pdf [-o out.musicxml] [--report report.json]\n");
        return 2;
    }
    if (output.empty() && ! toStdout)
    {
        output = input;
        const auto dot = output.find_last_of ('.');
        if (dot != std::string::npos && output.find_last_of ('/') < dot) output.erase (dot);
        output += ".musicxml";
    }

    const auto c = ans::convertPdf (input);
    if (! report.empty()) std::ofstream (report) << ans::reportJson (c);

    if (c.verdict == ans::Conversion::Rejected)
    {
        std::fprintf (stderr, "%s\n", c.summary().c_str());
        return 2;
    }
    if (! midi.empty()) std::ofstream (midi, std::ios::binary) << ans::toMidiFile (c.read.score, ans::perform (c.read.score));
    if (! chords.empty())
    {
        std::string text;
        int bar = -1;
        const auto& measures = c.read.score.parts.front().measures;
        for (const auto& ch : ans::outlineChords (c.read.score))
        {
            if (ch.bar != bar)
            {
                if (bar >= 0) text += "\n";
                bar = ch.bar;
                text += "bar " + std::to_string (bar + 1) + " (" + measures[static_cast<size_t> (bar)].number + "):";
            }
            text += " " + ch.offset.str() + ":" + ch.name;
        }
        if (bar >= 0) text += "\n";
        if (chords == "-") std::fwrite (text.data(), 1, text.size(), stdout);
        else std::ofstream (chords) << text;
    }
    if (toStdout) std::fwrite (c.musicXml.data(), 1, c.musicXml.size(), stdout);
    else std::ofstream (output, std::ios::binary) << c.musicXml;

    if (! quiet)
    {
        std::fprintf (stderr, "%s\n", c.summary().c_str());
        for (const auto& i : c.read.issues)
            std::fprintf (stderr, "  %s: page %d, bar %d, part %d: %s\n",
                          i.level == ans::Issue::Error ? "problem" : i.level == ans::Issue::Warning ? "check" : "note",
                          i.page + 1, i.measureIndex + 1, i.part + 1, i.message.c_str());
    }
    return c.verdict == ans::Conversion::Clean ? 0 : 1;
}
