#include "Layout.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace ans
{

HeadKind headKind (const std::string& n)
{
    if (n.rfind ("notehead", 0) != 0) return HeadKind::None;
    if (n.find ("Parenthes") != std::string::npos || n.find ("Null") != std::string::npos
        || n.find ("Cluster") != std::string::npos || n.find ("Bracket") != std::string::npos)
        return HeadKind::None;
    if (n == "noteheadSlashHorizontalEnds" || n == "noteheadSlashVerticalEnds") return HeadKind::Black;
    if (n.find ("DoubleWhole") != std::string::npos) return HeadKind::Breve;
    if (n.find ("Whole") != std::string::npos) return HeadKind::Whole;
    if (n.find ("Half") != std::string::npos) return HeadKind::Half;
    if (n.find ("Black") != std::string::npos || n == "noteheadCircleX" || n == "noteheadXOrnate") return HeadKind::Black;
    return HeadKind::None;
}

namespace
{
struct Line
{
    double y, x0, x1;
    std::vector<int> parts;   // segment indices
};

double overlap (const Line& a, const Line& b)
{
    return std::min (a.x1, b.x1) - std::max (a.x0, b.x0);
}

std::vector<Line> horizontalLines (const PageInk& page)
{
    std::vector<Line> raw;
    for (size_t i = 0; i < page.segments.size(); ++i)
    {
        const auto& s = page.segments[i];
        if (! s.horizontal (0.005) || s.thickness > 3.0 || s.length() < 2.0) continue;
        raw.push_back ({ (s.a.y + s.b.y) * 0.5, s.minX(), s.maxX(), { static_cast<int> (i) } });
    }
    std::sort (raw.begin(), raw.end(), [] (const Line& a, const Line& b) {
        if (std::abs (a.y - b.y) > 0.15) return a.y < b.y;
        return a.x0 < b.x0;
    });
    // Join pieces of one line (MuseScore draws a staff line bar by bar).
    std::vector<Line> merged;
    for (const auto& l : raw)
    {
        bool joined = false;
        for (size_t k = merged.size(); k-- > 0;)
        {
            auto& m = merged[k];
            if (l.y - m.y > 0.15) break;
            if (std::abs (l.y - m.y) <= 0.15 && l.x0 <= m.x1 + 1.5 && l.x1 >= m.x0 - 1.5)
            {
                m.x0 = std::min (m.x0, l.x0);
                m.x1 = std::max (m.x1, l.x1);
                m.parts.push_back (l.parts.front());
                joined = true;
                break;
            }
        }
        if (! joined) merged.push_back (l);
    }
    std::sort (merged.begin(), merged.end(), [] (const Line& a, const Line& b) { return a.y < b.y; });
    return merged;
}

std::vector<Staff> findStaves (const PageInk& page, std::vector<int>& lineSegments)
{
    std::vector<Line> lines;
    for (auto& l : horizontalLines (page))
        if (l.x1 - l.x0 >= 30.0) lines.push_back (l);

    std::vector<bool> used (lines.size(), false);
    std::vector<Staff> staves;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (used[i]) continue;
        for (size_t k = i + 1; k < lines.size(); ++k)
        {
            if (used[k]) continue;
            const double gap = lines[k].y - lines[i].y;
            if (gap < 1.5) continue;
            if (gap > 25.0) break;
            if (overlap (lines[i], lines[k]) < 0.6 * std::min (lines[i].x1 - lines[i].x0, lines[k].x1 - lines[k].x0)) continue;

            size_t found[5] = { i, k, 0, 0, 0 };
            bool ok = true;
            for (int n = 2; n < 5 && ok; ++n)
            {
                const double want = lines[i].y + gap * n;
                ok = false;
                for (size_t j = k + 1; j < lines.size(); ++j)
                {
                    if (lines[j].y > want + gap * 0.15) break;
                    if (! used[j] && std::abs (lines[j].y - want) <= gap * 0.15
                        && overlap (lines[i], lines[j]) >= 0.6 * std::min (lines[i].x1 - lines[i].x0, lines[j].x1 - lines[j].x0))
                    {
                        found[n] = j;
                        ok = true;
                        break;
                    }
                }
            }
            if (! ok) continue;

            Staff s;
            s.page = page.number;
            s.x0 = 1e9; s.x1 = -1e9;
            for (int n = 0; n < 5; ++n)
            {
                const auto& l = lines[found[n]];
                s.lines[n] = l.y;
                s.x0 = std::min (s.x0, l.x0);
                s.x1 = std::max (s.x1, l.x1);
                used[found[n]] = true;
                for (int seg : l.parts) lineSegments.push_back (seg);
            }
            s.space = (s.lines[4] - s.lines[0]) / 4.0;
            staves.push_back (s);
            break;
        }
    }
    std::sort (staves.begin(), staves.end(), [] (const Staff& a, const Staff& b) { return a.top() < b.top(); });
    return staves;
}

bool joined (const PageInk& page, const Staff& a, const Staff& b)
{
    const double sp = a.space;
    const double left = std::min (a.x0, b.x0) - 6.0 * sp;
    const double right = std::max (a.x1, b.x1) + sp;
    for (const auto& s : page.segments)
    {
        if (! s.vertical (0.05)) continue;
        if (s.minX() < left || s.maxX() > right) continue;
        if (s.minY() <= a.bottom() + 0.3 * sp && s.maxY() >= b.top() - 0.3 * sp) return true;
    }
    for (const auto& g : page.glyphs)
    {
        if (g.name != "brace" && g.name != "bracket" && g.name != "bracketTop" && g.name != "bracketBottom") continue;
        if (g.box.x0 < left || g.box.x1 > a.x0 + sp) continue;
        if (g.box.y0 <= a.bottom() && g.box.y1 >= b.top()) return true;
    }
    for (const auto& sh : page.shapes)
    {
        if (sh.box.x0 < left || sh.box.x1 > a.x0 + sp) continue;
        if (sh.box.y0 <= a.bottom() && sh.box.y1 >= b.top()) return true;
    }
    return false;
}

bool braced (const PageInk& page, const Staff& a, const Staff& b)
{
    const double sp = a.space;
    for (const auto& g : page.glyphs)
    {
        if (g.name != "brace") continue;
        if (g.box.x1 > a.x0 + sp || g.box.x0 < a.x0 - 6 * sp) continue;
        if (g.box.y0 <= a.middle() && g.box.y1 >= b.middle()) return true;
    }
    // A brace drawn as an outline: tall, narrow, curved, left of the staves.
    for (const auto& sh : page.shapes)
    {
        if (sh.box.x1 > a.x0 + 0.5 * sp || sh.box.x0 < a.x0 - 6 * sp) continue;
        if (sh.box.width() > 2.5 * sp) continue;
        if (sh.box.y0 <= a.middle() && sh.box.y1 >= b.middle() && sh.commands >= 8) return true;
    }
    return false;
}

// A notehead touching a vertical stroke at one of its ends makes it a stem.
bool touchesNotehead (const PageInk& page, const Segment& s, double sp)
{
    const double x = (s.a.x + s.b.x) * 0.5;
    for (const auto& g : page.glyphs)
    {
        if (headKind (g.name) == HeadKind::None) continue;
        if (x < g.box.x0 - 0.15 * sp || x > g.box.x1 + 0.15 * sp) continue;
        if (std::abs (g.origin.y - s.minY()) <= 0.6 * sp || std::abs (g.origin.y - s.maxY()) <= 0.6 * sp) return true;
    }
    return false;
}

struct BarStroke
{
    double x, thickness;
    bool dashed;
    int segment;
};

std::vector<BarColumn> columnsFor (const PageInk& page, const std::vector<Staff>& staves, const System& sys,
                                   std::vector<int>& barlineSegments)
{
    // Strokes that span whole staves, per staff.
    std::vector<std::vector<BarStroke>> perStaff (sys.staves.size());
    const Staff& first = staves[static_cast<size_t> (sys.staves.front())];
    const Staff& last = staves[static_cast<size_t> (sys.staves.back())];
    const double sp = first.space;

    for (size_t i = 0; i < page.segments.size(); ++i)
    {
        const auto& s = page.segments[i];
        if (! s.vertical (0.03) || s.thickness > 1.2 * sp) continue;
        if (s.minY() < first.top() - 0.3 * sp || s.maxY() > last.bottom() + 0.3 * sp) continue;
        const double x = (s.a.x + s.b.x) * 0.5;
        if (x < sys.x0 - 0.6 * sp || x > sys.x1 + 0.6 * sp) continue;

        bool topOk = false, bottomOk = false;
        for (int si : sys.staves)
        {
            const auto& st = staves[static_cast<size_t> (si)];
            topOk = topOk || std::abs (s.minY() - st.top()) <= 0.3 * sp;
            bottomOk = bottomOk || std::abs (s.maxY() - st.bottom()) <= 0.3 * sp;
        }
        if (! topOk || ! bottomOk) continue;
        if (touchesNotehead (page, s, sp)) continue;

        bool any = false;
        for (size_t k = 0; k < sys.staves.size(); ++k)
        {
            const auto& st = staves[static_cast<size_t> (sys.staves[k])];
            if (s.minY() <= st.top() + 0.3 * sp && s.maxY() >= st.bottom() - 0.3 * sp)
            {
                perStaff[k].push_back ({ x, s.thickness, s.dashed, static_cast<int> (i) });
                any = true;
            }
        }
        if (any) barlineSegments.push_back (static_cast<int> (i));
    }

    // Strokes close together are one barline (thin-thick, thin-thin...).
    struct Col { double x0, x1; std::vector<BarStroke> strokes; int staves = 0; };
    std::vector<Col> cols;
    for (auto& list : perStaff)
    {
        std::sort (list.begin(), list.end(), [] (const BarStroke& a, const BarStroke& b) { return a.x < b.x; });
        std::vector<Col> here;
        for (const auto& st : list)
        {
            if (! here.empty() && st.x - here.back().x1 <= 1.2 * sp)
            {
                here.back().x1 = st.x;
                here.back().strokes.push_back (st);
            }
            else
                here.push_back ({ st.x, st.x, { st }, 1 });
        }
        for (auto& h : here)
        {
            bool merged = false;
            for (auto& c : cols)
                if (std::abs (c.x0 - h.x0) <= 0.8 * sp && std::abs (c.x1 - h.x1) <= 0.8 * sp)
                {
                    ++c.staves;
                    if (h.strokes.size() > c.strokes.size()) c.strokes = h.strokes;
                    merged = true;
                    break;
                }
            if (! merged) cols.push_back (h);
        }
    }

    double thin = 1e9;
    for (auto& c : cols)
        for (auto& s : c.strokes) thin = std::min (thin, s.thickness);

    std::vector<BarColumn> out;
    const int need = static_cast<int> ((sys.staves.size() + 1) / 2);
    for (auto& c : cols)
    {
        if (c.staves < need) continue;
        // The line that opens the system is not a barline between bars.
        if (c.x0 <= sys.x0 + 0.5 * sp && c.strokes.size() == 1) continue;

        BarColumn b;
        b.x0 = c.x0 - c.strokes.front().thickness * 0.5;
        b.x1 = c.x1 + c.strokes.back().thickness * 0.5;
        std::vector<bool> heavy;
        bool dashed = false;
        for (auto& s : c.strokes)
        {
            heavy.push_back (s.thickness > thin * 1.8 && s.thickness > 0.25 * sp);
            dashed = dashed || s.dashed;
        }
        if (dashed) b.style = BarStyle::Dashed;
        else if (heavy.size() == 1) b.style = heavy[0] ? BarStyle::Heavy : BarStyle::Regular;
        else if (heavy.size() == 2)
        {
            if (! heavy[0] && ! heavy[1]) b.style = BarStyle::LightLight;
            else if (! heavy[0] && heavy[1]) b.style = BarStyle::LightHeavy;
            else if (heavy[0] && ! heavy[1]) b.style = BarStyle::HeavyLight;
            else b.style = BarStyle::Heavy;
        }
        else
            b.style = BarStyle::LightHeavy; // thin-thick-thin: a repeat both ways; dots decide

        // Repeat dots: two dots either side of the middle line, beside the strokes.
        for (const auto& g : page.glyphs)
        {
            if (g.name != "repeatDot" && g.name != "augmentationDot" && g.name != "repeatDots") continue;
            const double gx = g.box.cx();
            if (gx < b.x0 - 1.6 * sp || gx > b.x1 + 1.6 * sp) continue;
            bool inStaff = false;
            for (int si : sys.staves)
            {
                const auto& st = staves[static_cast<size_t> (si)];
                const double step = st.stepAt (g.box.cy());
                inStaff = inStaff || (step > 2.0 && step < 6.0 && g.box.cy() > st.top() && g.box.cy() < st.bottom());
            }
            if (! inStaff) continue;
            if (gx < b.x0) b.dotsBefore = true;
            else if (gx > b.x1) b.dotsAfter = true;
        }
        out.push_back (b);
    }
    std::sort (out.begin(), out.end(), [] (const BarColumn& a, const BarColumn& b) { return a.x0 < b.x0; });
    return out;
}
} // namespace

PageLayout findLayout (const PageInk& page)
{
    PageLayout out;
    out.staves = findStaves (page, out.staffLineSegments);

    for (size_t i = 0; i < out.staves.size(); ++i)
    {
        const bool startNew = i == 0 || ! joined (page, out.staves[i - 1], out.staves[i]);
        if (startNew)
        {
            System s;
            s.page = page.number;
            out.systems.push_back (s);
        }
        auto& sys = out.systems.back();
        out.staves[i].system = static_cast<int> (out.systems.size() - 1);
        out.staves[i].indexInSystem = static_cast<int> (sys.staves.size());
        sys.staves.push_back (static_cast<int> (i));
    }

    for (auto& sys : out.systems)
    {
        sys.x0 = 1e9; sys.x1 = -1e9;
        for (int si : sys.staves)
        {
            sys.x0 = std::min (sys.x0, out.staves[static_cast<size_t> (si)].x0);
            sys.x1 = std::max (sys.x1, out.staves[static_cast<size_t> (si)].x1);
        }
        int part = 0;
        for (size_t k = 0; k < sys.staves.size(); ++k)
        {
            if (k > 0 && ! braced (page, out.staves[static_cast<size_t> (sys.staves[k - 1])], out.staves[static_cast<size_t> (sys.staves[k])]))
                ++part;
            sys.partOfStaff.push_back (part);
        }
        sys.parts = part + 1;
        sys.bars = columnsFor (page, out.staves, sys, out.barlineSegments);
    }
    return out;
}

} // namespace ans
