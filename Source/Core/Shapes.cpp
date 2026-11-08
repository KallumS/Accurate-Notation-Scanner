#include "Shapes.h"

#include <algorithm>
#include <cmath>

namespace ans
{

namespace
{
double dist (Point a, Point b) { return std::hypot (a.x - b.x, a.y - b.y); }
Point mid (Point a, Point b) { return { (a.x + b.x) * 0.5, (a.y + b.y) * 0.5 }; }

Segment orient (Point a, Point b, double thickness, bool dashed)
{
    Segment s;
    const bool mostlyHorizontal = std::abs (b.x - a.x) >= std::abs (b.y - a.y);
    if ((mostlyHorizontal && b.x < a.x) || (! mostlyHorizontal && b.y < a.y)) std::swap (a, b);
    s.a = a; s.b = b; s.thickness = thickness; s.dashed = dashed;
    return s;
}

// A closed four-cornered shape: a rectangle or a beam's parallelogram.
// Its centre line joins the middles of the two short sides; its thickness
// is the length of a short side (for a beam, its height measured upright).
Segment fromQuad (const Point q[4], double extra, bool dashed)
{
    const double e0 = dist (q[0], q[1]), e1 = dist (q[1], q[2]), e2 = dist (q[2], q[3]), e3 = dist (q[3], q[0]);
    if (e0 + e2 >= e1 + e3)
        return orient (mid (q[1], q[2]), mid (q[3], q[0]), (e1 + e3) * 0.5 + extra, dashed);
    return orient (mid (q[0], q[1]), mid (q[2], q[3]), (e0 + e2) * 0.5 + extra, dashed);
}

void addSubpath (PageInk& page, const DrawnPath& path, const std::vector<PathCommand>& sub)
{
    if (sub.empty()) return;

    bool hasCurves = false, closed = false;
    std::vector<Point> pts;
    Box box;
    for (const auto& c : sub)
    {
        switch (c.kind)
        {
            case PathCommand::Move:
            case PathCommand::Line: pts.push_back (c.p[0]); break;
            case PathCommand::CurveTo: hasCurves = true; pts.push_back (c.p[2]); break;
            case PathCommand::Close: closed = true; break;
        }
        const int n = c.kind == PathCommand::CurveTo ? 3 : (c.kind == PathCommand::Close ? 0 : 1);
        for (int i = 0; i < n; ++i) box.add ({ c.p[i].x, c.p[i].y, c.p[i].x + 1e-6, c.p[i].y + 1e-6 });
    }
    if (pts.empty()) return;

    if (pts.size() > 1 && dist (pts.front(), pts.back()) < 1e-3) { closed = true; pts.pop_back(); }
    // Drop corners repeated in place.
    std::vector<Point> corners;
    for (const auto& p : pts)
        if (corners.empty() || dist (corners.back(), p) > 1e-3) corners.push_back (p);
    if (corners.size() > 1 && dist (corners.front(), corners.back()) < 1e-3) corners.pop_back();

    const double extra = path.stroked ? path.lineWidth : 0.0;

    if (hasCurves)
    {
        if (sub.size() <= 14)
        {
            Curve c;
            c.box = box;
            c.filled = path.filled;
            c.left = c.right = corners.front();
            for (const auto& p : corners)
            {
                if (p.x < c.left.x) c.left = p;
                if (p.x > c.right.x) c.right = p;
            }
            page.curves.push_back (c);
        }
        else
            page.shapes.push_back ({ box, static_cast<int> (sub.size()) });
        return;
    }

    const bool area = path.filled || closed;
    if (area && corners.size() == 4)
    {
        const Point q[4] = { corners[0], corners[1], corners[2], corners[3] };
        page.segments.push_back (fromQuad (q, extra, path.dashed));
        return;
    }
    if (area && corners.size() > 4)
    {
        // A thin many-cornered shape (a rounded bar) is still a bar.
        const double w = box.width(), h = box.height();
        if (std::min (w, h) <= 0.35 * std::max (w, h))
        {
            if (w >= h) page.segments.push_back (orient ({ box.x0, box.cy() }, { box.x1, box.cy() }, h + extra, path.dashed));
            else page.segments.push_back (orient ({ box.cx(), box.y0 }, { box.cx(), box.y1 }, w + extra, path.dashed));
        }
        else
            page.shapes.push_back ({ box, static_cast<int> (sub.size()) });
        return;
    }
    if (path.stroked)
    {
        for (size_t i = 0; i + 1 < corners.size(); ++i)
            page.segments.push_back (orient (corners[i], corners[i + 1], path.lineWidth, path.dashed));
        if (closed && corners.size() > 2)
            page.segments.push_back (orient (corners.back(), corners.front(), path.lineWidth, path.dashed));
        return;
    }
    if (path.filled && corners.size() >= 3)
        page.shapes.push_back ({ box, static_cast<int> (sub.size()) });
}

bool near (double a, double b, double tol) { return std::abs (a - b) <= tol; }
} // namespace

void addPath (PageInk& page, const DrawnPath& path)
{
    if (! path.filled && ! path.stroked) return;
    std::vector<PathCommand> sub;
    for (const auto& c : path.commands)
    {
        if (c.kind == PathCommand::Move && ! sub.empty())
        {
            addSubpath (page, path, sub);
            sub.clear();
        }
        sub.push_back (c);
    }
    addSubpath (page, path, sub);
}

void removeDuplicates (PageInk& page)
{
    auto& s = page.segments;
    std::sort (s.begin(), s.end(), [] (const Segment& p, const Segment& q) {
        if (p.a.x != q.a.x) return p.a.x < q.a.x;
        return p.a.y < q.a.y;
    });
    std::vector<Segment> kept;
    kept.reserve (s.size());
    for (const auto& seg : s)
    {
        bool dup = false;
        for (size_t k = kept.size(); k-- > 0;)
        {
            auto& o = kept[k];
            if (seg.a.x - o.a.x > 0.05) break;
            if (near (seg.a.y, o.a.y, 0.05) && near (seg.b.x, o.b.x, 0.05) && near (seg.b.y, o.b.y, 0.05))
            {
                o.thickness = std::max (o.thickness, seg.thickness);
                dup = true;
                break;
            }
        }
        if (! dup) kept.push_back (seg);
    }
    s.swap (kept);

    auto& c = page.curves;
    std::vector<Curve> keptCurves;
    for (const auto& cur : c)
    {
        bool dup = false;
        for (auto& o : keptCurves)
            if (near (cur.box.x0, o.box.x0, 0.3) && near (cur.box.x1, o.box.x1, 0.3)
                && near (cur.box.y0, o.box.y0, 0.3) && near (cur.box.y1, o.box.y1, 0.3)) { dup = true; o.filled = o.filled || cur.filled; break; }
        if (! dup) keptCurves.push_back (cur);
    }
    c.swap (keptCurves);
}

} // namespace ans
