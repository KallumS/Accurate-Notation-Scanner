#include "Check.h"
#include "Shapes.h"

using namespace ans;

namespace
{
PathCommand move (double x, double y) { PathCommand c; c.kind = PathCommand::Move; c.p[0] = { x, y }; return c; }
PathCommand line (double x, double y) { PathCommand c; c.kind = PathCommand::Line; c.p[0] = { x, y }; return c; }
PathCommand close() { PathCommand c; c.kind = PathCommand::Close; return c; }
}

TEST ("A stroked line is a segment with the stroke's width")
{
    PageInk page;
    DrawnPath p;
    p.stroked = true; p.lineWidth = 0.5;
    p.commands = { move (10, 20), line (10, 40) };
    addPath (page, p);
    CHECK_EQ (page.segments.size(), size_t (1));
    CHECK (page.segments[0].vertical());
    CHECK_EQ (page.segments[0].thickness, 0.5);
}

TEST ("A filled thin rectangle is a segment along its length")
{
    PageInk page;
    DrawnPath p;
    p.filled = true;
    p.commands = { move (100, 10), line (100.4, 10), line (100.4, 30), line (100, 30), close() };
    addPath (page, p);
    CHECK_EQ (page.segments.size(), size_t (1));
    const auto& s = page.segments[0];
    CHECK (s.vertical());
    CHECK (std::abs (s.thickness - 0.4) < 1e-9);
    CHECK (std::abs (s.length() - 20) < 1e-9);
}

TEST ("A beam's parallelogram keeps its slope and upright thickness")
{
    PageInk page;
    DrawnPath p;
    p.filled = true;
    p.commands = { move (0, 10), line (20, 15), line (20, 17.5), line (0, 12.5), close() };
    addPath (page, p);
    CHECK_EQ (page.segments.size(), size_t (1));
    const auto& s = page.segments[0];
    CHECK (std::abs (s.thickness - 2.5) < 1e-9);
    CHECK (std::abs (s.dy() / s.dx() - 0.25) < 1e-9);
}

TEST ("A shape filled and then outlined is kept once")
{
    PageInk page;
    DrawnPath p;
    p.filled = true;
    p.commands = { move (0, 10), line (20, 10), line (20, 12), line (0, 12), close() };
    addPath (page, p);
    p.filled = false; p.stroked = true; p.lineWidth = 0.4;
    addPath (page, p);
    removeDuplicates (page);
    CHECK_EQ (page.segments.size(), size_t (1));
    CHECK (std::abs (page.segments[0].thickness - 2.4) < 1e-9);
}
