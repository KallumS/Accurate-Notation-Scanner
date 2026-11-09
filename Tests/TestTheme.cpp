// The window's colours against Apple's guidelines, as the family reads them
// (Good Idea docs/apple-hig-review.md, Noterator's decision 0048): text at
// least 7:1 on what it sits on - the target for small text in a dark window -
// and anything to see and grab at least 3:1. The colours are read from
// Source/App/Theme.h itself, so the test checks what the app draws.

#include "Check.h"

#include <cmath>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>

namespace
{
std::map<std::string, unsigned> colours (bool increased)
{
    std::ifstream in (std::string (ANS_SOURCE) + "/App/Theme.h");
    std::stringstream ss;
    ss << in.rdbuf();
    std::map<std::string, unsigned> out;
    const std::regex re (R"(juce::Colour\s+(\w+)\s*\{\s*0xff([0-9A-Fa-f]{6})\s*\})");
    const auto text = ss.str();
    for (std::sregex_iterator it (text.begin(), text.end(), re), end; it != end; ++it)
        out[(*it)[1]] = static_cast<unsigned> (std::stoul ((*it)[2], nullptr, 16));
    if (increased)
    {
        // Theme.cpp's useIncreasedContrast
        std::ifstream cin (std::string (ANS_SOURCE) + "/App/Theme.cpp");
        std::stringstream cs;
        cs << cin.rdbuf();
        const std::regex on (R"((\w+) = juce::Colour \(on \? 0xff([0-9A-Fa-f]{6}))");
        const auto ct = cs.str();
        for (std::sregex_iterator it (ct.begin(), ct.end(), on), end; it != end; ++it)
            out[(*it)[1]] = static_cast<unsigned> (std::stoul ((*it)[2], nullptr, 16));
    }
    return out;
}

double luminance (unsigned rgb)
{
    auto channel = [] (unsigned v) {
        const double c = v / 255.0;
        return c <= 0.03928 ? c / 12.92 : std::pow ((c + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel ((rgb >> 16) & 255) + 0.7152 * channel ((rgb >> 8) & 255) + 0.0722 * channel (rgb & 255);
}

double contrast (unsigned a, unsigned b)
{
    const double x = luminance (a) + 0.05, y = luminance (b) + 0.05;
    return x > y ? x / y : y / x;
}
} // namespace

TEST ("Window colours: every text colour is at least 7:1 on the ground, with and without Increase Contrast")
{
    for (bool increased : { false, true })
    {
        auto c = colours (increased);
        CHECK (c.count ("ground") && c.count ("text") && c.count ("textDim") && c.count ("warnText") && c.count ("good"));
        for (const char* role : { "text", "textDim", "warnText", "good" })
            if (contrast (c[role], c["ground"]) < 7.0)
                check::fail (__FILE__, __LINE__, std::string (role) + " is " + std::to_string (contrast (c[role], c["ground"])) + ":1 on the ground");
        // the words on a button: ink on the control grey and on the yellow
        CHECK (contrast (c["ink"], c["control"]) >= 7.0);
        CHECK (contrast (c["ink"], c["accent"]) >= 7.0);
    }
}

TEST ("Window colours: what is drawn but not read (the scroll bar) is at least 3:1, and the house red never writes words")
{
    auto c = colours (false);
    CHECK (contrast (c["scrollGrab"], c["ground"]) >= 3.0);
    // the house red is under 7:1, which is why it only marks (Theme.h says so)
    CHECK (contrast (c["warn"], c["ground"]) < 7.0);
}
