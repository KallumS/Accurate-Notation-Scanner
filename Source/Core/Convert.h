/*
    Convert - one PDF in, MusicXML and a report out. What the app and the
    command-line tool both call.
*/

#pragma once

#include "Reader.h"

#include <string>

namespace ans
{

struct Conversion
{
    enum Verdict { Clean, WithIssues, Rejected } verdict = Rejected;
    std::string musicXml;      // empty when rejected
    ReadResult read;
    std::string error;         // why nothing came out, in plain words
    int pages = 0;

    // One or two sentences for a person.
    std::string summary() const;
};

Conversion convertPdf (const std::string& path);

// The report as JSON: verdict, summary, and every issue with its place.
std::string reportJson (const Conversion& c);

} // namespace ans
