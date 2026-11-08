/*
    Shapes - a drawn PDF path, turned into the kinds of ink the reader uses.

    Programs draw the same thing different ways: MuseScore strokes a stem as a
    line, LilyPond fills a thin rectangle; a beam is a filled parallelogram
    in both. Here they all become Segments (a centre line and a thickness),
    so nothing later needs to know which program drew them. Round shapes
    become Curves (ties, slurs), anything else a Shape.
*/

#pragma once

#include "Ink.h"

#include <vector>

namespace ans
{

struct PathCommand
{
    enum Kind { Move, Line, CurveTo, Close } kind = Move;
    Point p[3];   // Move/Line: p[0]; CurveTo: two control points then the end
};

struct DrawnPath
{
    std::vector<PathCommand> commands;
    bool filled = false, stroked = false;
    double lineWidth = 0;   // already scaled to page points
    bool dashed = false;
};

// Adds what the path draws to the page.
void addPath (PageInk& page, const DrawnPath& path);

// Removes the same stroke drawn twice (a shape filled, then outlined).
void removeDuplicates (PageInk& page);

} // namespace ans
