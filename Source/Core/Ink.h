/*
    Ink - what is printed on a page, exactly as the PDF draws it.

    A computer-made score is not a picture: every notehead, clef and rest is a
    character of a music font, at an exact position, and every staff line,
    stem, beam and barline is a drawn line or shape. The PDF reader hands
    over these, unchanged, and everything after it works from them. Nothing
    is guessed from pixels (decision 0001).

    Coordinates are PDF points, x to the right and y DOWN the page (MuPDF's
    convention), with the page's top-left corner at 0,0.
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ans
{

struct Point { double x = 0, y = 0; };

struct Box
{
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    double width() const { return x1 - x0; }
    double height() const { return y1 - y0; }
    double cx() const { return (x0 + x1) * 0.5; }
    double cy() const { return (y0 + y1) * 0.5; }
    bool empty() const { return x1 <= x0 || y1 <= y0; }
    void add (const Box& b)
    {
        if (empty()) { *this = b; return; }
        x0 = std::min (x0, b.x0); y0 = std::min (y0, b.y0);
        x1 = std::max (x1, b.x1); y1 = std::max (y1, b.y1);
    }
};

// One character of a music font, already named (Glyphs.h).
struct Glyph
{
    std::string name;     // the SMuFL name: "noteheadBlack", "gClef", "accidentalSharp"...
    Point origin;         // the font's origin: for noteheads, accidentals and clefs
                          // it sits on the line or space the symbol means
    Box box;              // the glyph's own outline, tight
    double size = 0;      // font size in points; a music font's em is four staff spaces
    double advance = 0;   // advance width in points
};

// A run of ordinary text: a title, a part name, a tempo, lyrics, a tuplet number.
struct Text
{
    std::string text;     // UTF-8
    Box box;
    double size = 0;
    bool italic = false, bold = false;
    std::string font;
};

// A straight stroke of some thickness: a stroked line, or a filled rectangle
// or parallelogram (stems, barlines, beams, ledger lines, staff lines).
// a is the left (or top) end of its centre line, b the other.
struct Segment
{
    Point a, b;
    double thickness = 0;
    bool dashed = false;

    double dx() const { return b.x - a.x; }
    double dy() const { return b.y - a.y; }
    double length() const { return std::hypot (dx(), dy()); }
    bool horizontal (double tolerance = 0.02) const { return std::abs (dy()) <= tolerance * std::max (1e-9, std::abs (dx())) + 0.05; }
    bool vertical (double tolerance = 0.02) const { return std::abs (dx()) <= tolerance * std::max (1e-9, std::abs (dy())) + 0.05; }
    double minX() const { return std::min (a.x, b.x); }
    double maxX() const { return std::max (a.x, b.x); }
    double minY() const { return std::min (a.y, b.y); }
    double maxY() const { return std::max (a.y, b.y); }
    double yAt (double x) const
    {
        if (std::abs (dx()) < 1e-9) return (a.y + b.y) * 0.5;
        return a.y + (x - a.x) * dy() / dx();
    }
};

// A shape with curves in it: ties, slurs, and anything else round.
struct Curve
{
    Point left, right;    // its ends, leftmost and rightmost
    Box box;
    bool filled = false;
};

// Any other shape (a brace drawn as a path, a symbol outlined into paths).
struct Shape
{
    Box box;
    int commands = 0;
};

// A picture placed on the page. A page that is mostly picture is a scan.
struct Picture
{
    Box box;
};

struct PageInk
{
    int number = 0;                // 0-based
    double width = 0, height = 0;
    int quarterTurns = 0;          // the music was drawn turned on the page; it was read
                                   // turned back this many quarter turns clockwise
    std::vector<Glyph> glyphs;
    std::vector<Text> texts;
    std::vector<Segment> segments;
    std::vector<Curve> curves;
    std::vector<Shape> shapes;
    std::vector<Picture> pictures;
    std::vector<std::string> unknownMusicFonts; // music-looking fonts we cannot name
    int unnamedMusicGlyphs = 0;
};

} // namespace ans
