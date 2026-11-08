/*
    Layout - the page's frame: staves, systems, parts, and the barlines that
    cut each system into bars.

    Staves are found from their lines: five straight lines, evenly spaced,
    side by side. Staves joined by a line, a bracket or a brace at their left
    form a system; a brace makes its staves one part (a piano). Barlines are
    upright strokes that run exactly from a staff's top line to its bottom
    line (or across several staves) with no notehead at either end - which
    is what tells them from stems.
*/

#pragma once

#include "Ink.h"
#include "Score.h"

#include <vector>

namespace ans
{

struct Staff
{
    int page = 0;
    double lines[5] = { 0, 0, 0, 0, 0 };  // y of each line, top first
    double space = 0;                     // distance between lines
    double x0 = 0, x1 = 0;
    int system = -1;                      // index into PageLayout::systems
    int indexInSystem = 0;

    double top() const { return lines[0]; }
    double bottom() const { return lines[4]; }
    double middle() const { return lines[2]; }
    // Half-spaces above the bottom line: 0 bottom line, 1 first space, 8 top line.
    double stepAt (double y) const { return (bottom() - y) / (space * 0.5); }
    double yOfStep (double step) const { return bottom() - step * space * 0.5; }
};

struct BarColumn
{
    double x0 = 0, x1 = 0;                // left and right edges of the strokes
    BarStyle style = BarStyle::Regular;
    bool dotsBefore = false;              // repeat dots on the left: end of a repeat
    bool dotsAfter = false;               // on the right: start of a repeat
    double x() const { return (x0 + x1) * 0.5; }
};

struct System
{
    int page = 0;
    std::vector<int> staves;              // indices into PageLayout::staves, top first
    std::vector<int> partOfStaff;         // same order: which part (within the system) each staff is
    int parts = 0;
    std::vector<BarColumn> bars;          // left to right, not counting the system's opening line
    double x0 = 0, x1 = 0;
};

struct PageLayout
{
    std::vector<Staff> staves;
    std::vector<System> systems;
    std::vector<int> barlineSegments;     // indices into PageInk::segments used as barlines
    std::vector<int> staffLineSegments;   // ... and as staff lines
};

PageLayout findLayout (const PageInk& page);

// Whether a glyph name is a notehead, and its kind.
enum class HeadKind { None, Black, Half, Whole, Breve };
HeadKind headKind (const std::string& glyphName);

} // namespace ans
