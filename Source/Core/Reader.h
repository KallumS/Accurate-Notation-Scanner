/*
    Reader - from the ink on the pages to a Score.

    Each page's layout (Layout.h) gives its staves and bars. Then, staff by
    staff, the symbols are put together the way a musician reads them:
    noteheads with their stems, beams, flags, dots and accidentals into
    chords; rests; clefs, key and time signatures; ties; tuplets. Where two
    voices share a staff they are told apart by stem direction and checked
    against each other.

    Every bar is then checked: its notes must add up to its time signature
    (a pickup and the bar that completes it excepted), every voice must fit,
    every note must sit on a line or a space. Whatever does not check out
    is reported as an Issue with its place on the page, never silently
    "fixed" (decision 0002).
*/

#pragma once

#include "Ink.h"
#include "Score.h"

#include <string>
#include <vector>

namespace ans
{

struct PageVerdict
{
    enum Kind { Music, Scanned, NoMusic, Unreadable } kind = NoMusic;
    std::string reason;
};

struct ReadResult
{
    Score score;
    std::vector<Issue> issues;
    std::vector<PageVerdict> pages;
    int barsRead = 0;
    int barsWithIssues = 0;
    bool hasMusic() const { return ! score.parts.empty() && barsRead > 0; }
};

ReadResult readScore (const std::vector<PageInk>& pages);

} // namespace ans
