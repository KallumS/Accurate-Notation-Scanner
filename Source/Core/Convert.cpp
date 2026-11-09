#include "Convert.h"

#include "MusicXml.h"
#include "PdfReader.h"

#include <cstdio>
#include <set>

namespace ans
{

namespace
{
std::string plural (int n, const char* one, const char* many)
{
    return std::to_string (n) + " " + (n == 1 ? one : many);
}

std::string jsonString (const std::string& s)
{
    std::string out = "\"";
    for (char c : s)
    {
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char> (c) < 0x20)
                {
                    char buf[8];
                    std::snprintf (buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                }
                else
                    out += c;
        }
    }
    return out + "\"";
}

std::string number (double v)
{
    char buf[32];
    std::snprintf (buf, sizeof buf, "%.2f", v);
    return buf;
}
} // namespace

Conversion convertPdf (const std::string& path)
{
    Conversion c;
    const auto pdf = readPdf (path);
    c.pages = static_cast<int> (pdf.pages.size());
    if (! pdf.error.empty() && pdf.pages.empty())
    {
        c.error = pdf.error;
        return c;
    }
    c.read = readScore (pdf.pages);
    if (c.read.score.title.empty() && ! pdf.title.empty()) c.read.score.title = pdf.title;

    if (! c.read.hasMusic())
    {
        int scanned = 0, unreadable = 0;
        std::string reason;
        for (const auto& p : c.read.pages)
        {
            if (p.kind == PageVerdict::Scanned) ++scanned;
            if (p.kind == PageVerdict::Unreadable) { ++unreadable; if (reason.empty()) reason = p.reason; }
        }
        if (scanned > 0 && unreadable == 0)
            c.error = "This PDF is a scan \u2014 a picture of the page \u2014 so it was not converted. "
                      "Only scores made on a computer can be read exactly.";
        else if (unreadable > 0)
            c.error = reason;
        else
            c.error = "No music was found in this PDF.";
        return c;
    }

    c.musicXml = toMusicXml (c.read.score);
    bool errors = false;
    for (const auto& i : c.read.issues)
        errors = errors || i.level != Issue::Info;
    c.verdict = errors ? Conversion::WithIssues : Conversion::Clean;
    return c;
}

std::string Conversion::summary() const
{
    if (verdict == Rejected) return error;
    const auto& s = read.score;
    int scanned = 0;
    for (const auto& p : read.pages) if (p.kind == PageVerdict::Scanned) ++scanned;
    std::string out = "Read " + plural (read.barsRead, "bar", "bars") + " in " + plural (static_cast<int> (s.parts.size()), "part", "parts") + ".";
    if (verdict == Clean) out += " Every bar checked out.";
    else if (read.barsWithIssues > 0)
        out += " " + plural (read.barsWithIssues, "bar needs", "bars need") + " a look \u2014 " + (read.barsWithIssues == 1 ? "it is" : "they are") + " marked.";
    else
        out += " Some things need a look - see the list.";
    if (scanned > 0) out += " " + plural (scanned, "scanned page was", "scanned pages were") + " skipped.";
    return out;
}

std::string reportJson (const Conversion& c)
{
    std::string j = "{\n";
    j += "  \"verdict\": " + jsonString (c.verdict == Conversion::Clean ? "clean" : c.verdict == Conversion::WithIssues ? "with-issues" : "rejected") + ",\n";
    j += "  \"summary\": " + jsonString (c.summary()) + ",\n";
    j += "  \"pages\": " + std::to_string (c.pages) + ",\n";
    j += "  \"bars\": " + std::to_string (c.read.barsRead) + ",\n";
    j += "  \"barsWithIssues\": " + std::to_string (c.read.barsWithIssues) + ",\n";
    j += "  \"title\": " + jsonString (c.read.score.title) + ",\n";
    j += "  \"parts\": [";
    for (size_t p = 0; p < c.read.score.parts.size(); ++p)
        j += (p ? ", " : "") + jsonString (c.read.score.parts[p].name);
    j += "],\n  \"issues\": [\n";
    for (size_t i = 0; i < c.read.issues.size(); ++i)
    {
        const auto& is = c.read.issues[i];
        const char* level = is.level == Issue::Error ? "error" : is.level == Issue::Warning ? "warning" : "info";
        j += "    { \"level\": " + jsonString (level) + ", \"page\": " + std::to_string (is.page + 1)
           + ", \"bar\": " + std::to_string (is.measureIndex + 1) + ", \"part\": " + std::to_string (is.part + 1)
           + ", \"box\": [" + number (is.box.x0) + ", " + number (is.box.y0) + ", " + number (is.box.x1) + ", " + number (is.box.y1) + "]"
           + ", \"message\": " + jsonString (is.message) + " }" + (i + 1 < c.read.issues.size() ? "," : "") + "\n";
    }
    j += "  ]\n}\n";
    return j;
}

} // namespace ans
