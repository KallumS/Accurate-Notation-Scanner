// notation-scan: the converter without the window.
//
//   notation-scan score.pdf                  writes score.musicxml beside it
//   notation-scan score.pdf -o out.musicxml  writes there
//   notation-scan score.pdf --report r.json  also writes the report
//   notation-scan score.pdf --stdout         prints the MusicXML
//
// Exit status: 0 every bar checked out, 1 converted with things to look at,
// 2 not converted (a scan, no music, or not a PDF).

#include "Convert.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

int main (int argc, char** argv)
{
    std::string input, output, report;
    bool toStdout = false, quiet = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if ((a == "-o" || a == "--output") && i + 1 < argc) output = argv[++i];
        else if (a == "--report" && i + 1 < argc) report = argv[++i];
        else if (a == "--stdout") toStdout = true;
        else if (a == "-q" || a == "--quiet") quiet = true;
        else if (a == "-h" || a == "--help")
        {
            std::printf ("usage: notation-scan score.pdf [-o out.musicxml] [--report report.json] [--stdout] [-q]\n");
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
