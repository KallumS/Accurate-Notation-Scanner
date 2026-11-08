#include "Reader.h"

#include "Layout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <optional>
#include <set>

namespace ans
{

namespace
{
// ===========================================================================
// What the glyph names mean

int restType (const std::string& n)
{
    static const std::map<std::string, int> t = {
        { "restMaxima", -3 }, { "restLonga", -2 }, { "restDoubleWhole", -1 }, { "restDoubleWholeLegerLine", -1 },
        { "restWhole", 0 }, { "restWholeLegerLine", 0 }, { "restHalf", 1 }, { "restHalfLegerLine", 1 },
        { "restQuarter", 2 }, { "restQuarterOld", 2 }, { "restQuarterZ", 2 },
        { "rest8th", 3 }, { "rest16th", 4 }, { "rest32nd", 5 }, { "rest64th", 6 }, { "rest128th", 7 },
        { "rest256th", 8 }, { "rest512th", 9 }, { "rest1024th", 10 } };
    const auto it = t.find (n);
    return it == t.end() ? 99 : it->second;
}

int flagCount (const std::string& n)
{
    static const std::map<std::string, int> t = {
        { "flag8thUp", 1 }, { "flag8thDown", 1 }, { "flag16thUp", 2 }, { "flag16thDown", 2 },
        { "flag32ndUp", 3 }, { "flag32ndDown", 3 }, { "flag64thUp", 4 }, { "flag64thDown", 4 },
        { "flag128thUp", 5 }, { "flag128thDown", 5 }, { "flag256thUp", 6 }, { "flag256thDown", 6 },
        { "flag512thUp", 7 }, { "flag512thDown", 7 }, { "flag1024thUp", 8 }, { "flag1024thDown", 8 } };
    const auto it = t.find (n);
    return it == t.end() ? 0 : it->second;
}

int accidentalValue (const std::string& n)
{
    if (n == "accidentalFlat") return -1;
    if (n == "accidentalNatural") return 0;
    if (n == "accidentalSharp") return 1;
    if (n == "accidentalDoubleSharp" || n == "accidentalSharpSharp") return 2;
    if (n == "accidentalDoubleFlat") return -2;
    return 99;
}

int timeDigit (const std::string& n)
{
    if (n.size() == 8 && n.rfind ("timeSig", 0) == 0 && n[7] >= '0' && n[7] <= '9') return n[7] - '0';
    return -1;
}

int tupletDigit (const std::string& n)
{
    if (n.size() == 7 && n.rfind ("tuplet", 0) == 0 && n[6] >= '0' && n[6] <= '9') return n[6] - '0';
    return -1;
}

bool isClefName (const std::string& n)
{
    return n.find ("Clef") != std::string::npos && n.find ("TabClef") == std::string::npos
        && (n.rfind ("gClef", 0) == 0 || n.rfind ("fClef", 0) == 0 || n.rfind ("cClef", 0) == 0
            || n.rfind ("unpitchedPercussionClef", 0) == 0);
}

// The clef a glyph shows, given the staff step its origin sits on.
Clef clefFor (const std::string& n, double step)
{
    Clef c;
    c.line = static_cast<int> (std::lround (step / 2.0)) + 1;
    if (n.rfind ("gClef", 0) == 0) c.sign = ClefSign::G;
    else if (n.rfind ("fClef", 0) == 0) c.sign = ClefSign::F;
    else if (n.rfind ("cClef", 0) == 0) c.sign = ClefSign::C;
    else { c.sign = ClefSign::Percussion; c.line = 3; }
    if (n.find ("8vb") != std::string::npos) c.octave = -1;
    else if (n.find ("8va") != std::string::npos) c.octave = 1;
    else if (n.find ("15mb") != std::string::npos) c.octave = -2;
    else if (n.find ("15ma") != std::string::npos) c.octave = 2;
    return c;
}

// Diatonic number (octave * 7 + step) of the bottom line under a clef.
int bottomLineDiatonic (const Clef& c)
{
    int ref = 32, refStep = (c.line - 1) * 2;          // G4 on the clef's line
    if (c.sign == ClefSign::F) ref = 24;                // F3
    else if (c.sign == ClefSign::C) ref = 28;           // C4
    else if (c.sign == ClefSign::Percussion) { ref = 32; refStep = 2; } // read as a treble clef
    return ref - refStep + 7 * c.octave;
}

std::string articulationFor (const std::string& n)
{
    auto has = [&] (const char* s) { return n.find (s) != std::string::npos; };
    if (n.rfind ("fermata", 0) == 0) return has ("Below") ? "fermata:inverted" : "fermata:upright";
    if (n.rfind ("artic", 0) == 0)
    {
        if (has ("TenutoStaccato")) return "articulations:detached-legato";
        if (has ("AccentStaccato")) return "articulations:accent+staccato";
        if (has ("TenutoAccent")) return "articulations:accent+tenuto";
        if (has ("MarcatoStaccato")) return "articulations:strong-accent+staccato";
        if (has ("Staccatissimo")) return "articulations:staccatissimo";
        if (has ("Staccato")) return "articulations:staccato";
        if (has ("SoftAccent")) return "articulations:soft-accent";
        if (has ("Accent")) return "articulations:accent";
        if (has ("Tenuto")) return "articulations:tenuto";
        if (has ("Marcato")) return "articulations:strong-accent";
        if (has ("Stress")) return "articulations:stress";
        return {};
    }
    if (n == "ornamentTrill") return "ornaments:trill-mark";
    if (n == "ornamentTurn") return "ornaments:turn";
    if (n == "ornamentTurnInverted") return "ornaments:inverted-turn";
    if (n == "ornamentMordent") return "ornaments:mordent";
    if (n == "ornamentShortTrill") return "ornaments:inverted-mordent";
    if (n == "stringsUpBow") return "technical:up-bow";
    if (n == "stringsDownBow") return "technical:down-bow";
    if (n == "stringsHarmonic") return "technical:harmonic";
    if (n == "breathMarkComma") return "articulations:breath-mark";
    if (n == "caesura" || n == "caesuraCurved") return "articulations:caesura";
    return {};
}

std::string dynamicLetters (const std::string& n)
{
    static const std::map<std::string, std::string> t = {
        { "dynamicPiano", "p" }, { "dynamicMezzo", "m" }, { "dynamicForte", "f" }, { "dynamicRinforzando", "r" },
        { "dynamicSforzando", "s" }, { "dynamicZ", "z" }, { "dynamicNiente", "n" },
        { "dynamicPPPPPP", "pppppp" }, { "dynamicPPPPP", "ppppp" }, { "dynamicPPPP", "pppp" }, { "dynamicPPP", "ppp" },
        { "dynamicPP", "pp" }, { "dynamicMP", "mp" }, { "dynamicMF", "mf" }, { "dynamicPF", "pf" },
        { "dynamicFF", "ff" }, { "dynamicFFF", "fff" }, { "dynamicFFFF", "ffff" }, { "dynamicFFFFF", "fffff" },
        { "dynamicFFFFFF", "ffffff" }, { "dynamicFortePiano", "fp" }, { "dynamicForzando", "fz" },
        { "dynamicSforzando1", "sf" }, { "dynamicSforzandoPiano", "sfp" }, { "dynamicSforzandoPianissimo", "sfpp" },
        { "dynamicSforzato", "sfz" }, { "dynamicSforzatoPiano", "sfzp" }, { "dynamicSforzatoFF", "sffz" },
        { "dynamicRinforzando1", "rf" }, { "dynamicRinforzando2", "rfz" } };
    const auto it = t.find (n);
    return it == t.end() ? std::string() : it->second;
}

bool isKnownDynamic (const std::string& s)
{
    static const std::set<std::string> ok = { "p", "pp", "ppp", "pppp", "ppppp", "pppppp", "f", "ff", "fff", "ffff",
        "fffff", "ffffff", "mp", "mf", "sf", "sfp", "sfpp", "fp", "rf", "rfz", "sfz", "sffz", "fz", "n", "pf", "sfzp" };
    return ok.count (s) > 0;
}

Fraction baseDuration (int type)
{
    if (type >= 0) return Fraction (4, 1LL << type);
    return Fraction (4LL << (-type), 1);
}

Fraction dotted (Fraction base, int dots)
{
    Fraction total = base, add = base;
    for (int i = 0; i < dots; ++i) { add = add / Fraction (2); total += add; }
    return total;
}

// ===========================================================================
// Pieces of a page, put together

struct Head
{
    int glyph = -1;
    double x0 = 0, x1 = 0, y = 0;
    double step = 0;
    int line = 0;
    bool small = false;
    HeadKind kind = HeadKind::Black;
    std::string shape;
    int staff = -1;
    std::vector<int> stems;
};

struct Stem
{
    int segment = -1;
    double x = 0, y0 = 0, y1 = 0;
    int dir = 0;                         // 1 up, -1 down
    double freeY = 0;
    std::vector<int> heads;
    std::vector<std::vector<int>> levels; // beams, outermost first; each level may be several segments
    int flags = 0;
    bool slash = false;
    int staff = -1;
};

struct Item
{
    bool rest = false;
    std::vector<int> heads;
    int stem = -1;
    int glyph = -1;                     // a rest's glyph
    int staff = -1;
    double x = 0, x1 = 0, y0 = 0, y1 = 0;
    int type = 2;
    int dots = 0;
    bool small = false;
    bool slash = false;
    int dir = 0;
    Tuplet tuplet;
    std::vector<std::string> articulations;
    int bar = -1;                       // within its system
    int voice = 1;
    bool grace = false;
    bool measureRest = false;
    Fraction duration, offset;
    std::vector<Note> notes;
    std::vector<int> beams;
    std::vector<std::pair<int, bool>> slurs; // number, start?
};

struct TieLink { int from = -1, to = -1; bool toNextSystem = false, fromLastSystem = false; };

struct StaffBar
{
    std::vector<Event> events;
    std::vector<Direction> directions;
    std::vector<ClefChange> clefs;
    std::vector<double> clefX;           // where each clef change stands
    std::optional<int> key;
    std::optional<TimeSignature> time;
    Fraction content;
    bool hasNotes = false;
    int multiRest = 0;
    Box box;
    int page = 0;
    std::vector<std::pair<Issue::Level, std::string>> problems;
};

struct SystemRead
{
    int page = 0;
    int parts = 0;
    std::vector<int> partOfStaff;
    std::vector<std::vector<StaffBar>> staves;   // [staff in system][bar]
    int bars = 0;
    std::vector<Barline> leftBar, rightBar;      // per bar
    std::vector<bool> hasLeft;
    std::vector<std::string> staffNames;
    std::vector<Clef> openingClef;               // per staff
};

// What carries from one system to the next, per staff of a part.
struct StaffState
{
    Clef clef;
    bool clefKnown = false;
    int key = 0;
    TimeSignature time;
    bool timeKnown = false;
    struct OpenTie { int line; Pitch pitch; };
    std::vector<OpenTie> openTies;
    std::vector<OpenTie> openSlurTies;
};

// ===========================================================================

class PageReader
{
public:
    PageReader (const PageInk& p, const PageLayout& l) : page (p), layout (l) {}

    void prepare();
    SystemRead readSystem (int systemIndex, std::vector<StaffState>& state, const std::vector<int>& stateIndexOfStaff);

    const PageInk& page;
    const PageLayout& layout;

    std::vector<int> glyphStaff;
    std::vector<Head> heads;
    std::vector<Stem> stems;
    std::vector<int> beamSegs;
    std::vector<int> ledgerSegs;
    std::vector<bool> segmentTaken;
    std::vector<Item> items;
    std::vector<int> headItem;           // per head: its item (first)
    std::vector<bool> glyphUsed;
    std::vector<TieLink> ties;
    int slurCounter = 0;

private:
    double spaceNear (double x, double y) const;
    int nearestStaff (double x, double y, bool restrictToX = true) const;
    int staffForHead (const Glyph& g) const;
    bool hasLedger (const Staff& s, int step, double x0, double x1) const;

    void findHeads();
    void findStems();
    void findBeams();
    void findFlags();
    void buildItems();
    void findDots();
    void findTies();
    void findSlurs (const std::set<int>& tieCurves);
    void findArticulations();
};

double PageReader::spaceNear (double x, double y) const
{
    const int s = nearestStaff (x, y, false);
    return s >= 0 ? layout.staves[static_cast<size_t> (s)].space : 5.0;
}

int PageReader::nearestStaff (double x, double y, bool restrictToX) const
{
    int best = -1;
    double bestD = 1e18;
    for (size_t i = 0; i < layout.staves.size(); ++i)
    {
        const auto& s = layout.staves[i];
        if (restrictToX && (x < s.x0 - 5 * s.space || x > s.x1 + 3 * s.space)) continue;
        const double d = y < s.top() ? s.top() - y : (y > s.bottom() ? y - s.bottom() : 0.0);
        if (d < bestD) { bestD = d; best = static_cast<int> (i); }
    }
    return best;
}

bool PageReader::hasLedger (const Staff& s, int step, double x0, double x1) const
{
    const double y = s.yOfStep (step);
    for (int i : ledgerSegs)
    {
        const auto& seg = page.segments[static_cast<size_t> (i)];
        if (std::abs ((seg.a.y + seg.b.y) * 0.5 - y) > 0.25 * s.space) continue;
        if (seg.maxX() < x0 || seg.minX() > x1) continue;
        return true;
    }
    return false;
}

int PageReader::staffForHead (const Glyph& g) const
{
    const double y = g.origin.y, x = g.box.cx();
    int above = -1, below = -1;
    for (size_t i = 0; i < layout.staves.size(); ++i)
    {
        const auto& s = layout.staves[i];
        if (x < s.x0 - 5 * s.space || x > s.x1 + 3 * s.space) continue;
        if (y >= s.top() - 0.3 * s.space && y <= s.bottom() + 0.3 * s.space) return static_cast<int> (i);
        if (s.bottom() < y && (above < 0 || s.bottom() > layout.staves[static_cast<size_t> (above)].bottom())) above = static_cast<int> (i);
        if (s.top() > y && (below < 0 || s.top() < layout.staves[static_cast<size_t> (below)].top())) below = static_cast<int> (i);
    }
    if (above < 0) return below;
    if (below < 0) return above;
    const auto& A = layout.staves[static_cast<size_t> (above)];
    const auto& B = layout.staves[static_cast<size_t> (below)];
    // Staves in different systems: the nearer one.
    const double dA = y - A.bottom(), dB = B.top() - y;
    if (A.system != B.system) return dA <= dB ? above : below;
    const bool needA = A.stepAt (y) <= -1.5, needB = B.stepAt (y) >= 9.5;
    if (! needA && needB) return above;
    if (needA && ! needB) return below;
    const bool ledA = hasLedger (A, -2, g.box.x0, g.box.x1);
    const bool ledB = hasLedger (B, 10, g.box.x0, g.box.x1);
    if (ledA && ! ledB) return above;
    if (ledB && ! ledA) return below;
    return dA <= dB ? above : below;
}

void PageReader::prepare()
{
    segmentTaken.assign (page.segments.size(), false);
    for (int i : layout.staffLineSegments) segmentTaken[static_cast<size_t> (i)] = true;
    for (int i : layout.barlineSegments) segmentTaken[static_cast<size_t> (i)] = true;

    // Ledger lines: short thin horizontal strokes on a line position outside a staff.
    for (size_t i = 0; i < page.segments.size(); ++i)
    {
        if (segmentTaken[i]) continue;
        const auto& s = page.segments[i];
        if (! s.horizontal (0.01)) continue;
        const double y = (s.a.y + s.b.y) * 0.5;
        const int st = nearestStaff (s.a.x, y);
        if (st < 0) continue;
        const auto& staff = layout.staves[static_cast<size_t> (st)];
        const double sp = staff.space;
        if (s.thickness > 0.45 * sp || s.length() < 0.6 * sp || s.length() > 5.0 * sp) continue;
        const double step = staff.stepAt (y);
        if (step > -1.0 && step < 9.0) continue;
        if (std::abs (step / 2.0 - std::round (step / 2.0)) > 0.12) continue;
        ledgerSegs.push_back (static_cast<int> (i));
        segmentTaken[i] = true;
    }

    glyphStaff.assign (page.glyphs.size(), -1);
    glyphUsed.assign (page.glyphs.size(), false);
    for (size_t i = 0; i < page.glyphs.size(); ++i)
    {
        const auto& g = page.glyphs[i];
        glyphStaff[i] = headKind (g.name) != HeadKind::None ? staffForHead (g) : nearestStaff (g.box.cx(), g.box.cy());
    }

    findHeads();
    findStems();
    findBeams();
    findFlags();
    buildItems();
    findDots();
    findTies();
    findArticulations();
}

void PageReader::findHeads()
{
    for (size_t i = 0; i < page.glyphs.size(); ++i)
    {
        const auto& g = page.glyphs[i];
        const auto kind = headKind (g.name);
        if (kind == HeadKind::None || glyphStaff[i] < 0) continue;
        const auto& staff = layout.staves[static_cast<size_t> (glyphStaff[i])];
        Head h;
        h.glyph = static_cast<int> (i);
        h.x0 = g.box.x0;
        h.x1 = std::max (g.box.x1, g.box.x0 + g.advance * 0.9);
        h.y = g.origin.y;
        h.step = staff.stepAt (g.origin.y);
        h.line = static_cast<int> (std::lround (h.step));
        h.kind = kind;
        h.small = g.size < 0.8 * 4.0 * staff.space;
        h.staff = glyphStaff[i];
        if (g.name.find ("noteheadX") == 0) h.shape = "x";
        else if (g.name.find ("Diamond") != std::string::npos) h.shape = "diamond";
        else if (g.name.find ("Triangle") != std::string::npos) h.shape = "triangle";
        else if (g.name.find ("Slash") != std::string::npos) h.shape = "slash";
        heads.push_back (h);
        glyphUsed[i] = true;
    }
}

void PageReader::findStems()
{
    for (size_t i = 0; i < page.segments.size(); ++i)
    {
        if (segmentTaken[i]) continue;
        const auto& s = page.segments[i];
        if (! s.vertical (0.05)) continue;
        const double x = (s.a.x + s.b.x) * 0.5;
        const double sp = spaceNear (x, s.minY());
        if (s.thickness > 0.4 * sp || s.length() < 0.9 * sp) continue;

        Stem st;
        st.segment = static_cast<int> (i);
        st.x = x; st.y0 = s.minY(); st.y1 = s.maxY();
        const double tol = 0.35 * sp + s.thickness * 0.5;
        std::vector<int> cand;
        for (size_t h = 0; h < heads.size(); ++h)
        {
            const auto& hd = heads[h];
            if (std::abs (x - hd.x1) > tol && std::abs (x - hd.x0) > tol) continue;
            if (hd.y < st.y0 - 0.6 * sp || hd.y > st.y1 + 0.6 * sp) continue;
            cand.push_back (static_cast<int> (h));
        }
        if (cand.empty()) continue;
        double dTop = 1e9, dBot = 1e9;
        for (int h : cand)
        {
            dTop = std::min (dTop, std::abs (heads[static_cast<size_t> (h)].y - st.y0));
            dBot = std::min (dBot, std::abs (heads[static_cast<size_t> (h)].y - st.y1));
        }
        if (std::min (dTop, dBot) > 0.7 * sp) continue;   // no head at either end: not a stem
        st.dir = dBot <= dTop ? 1 : -1;
        st.freeY = st.dir == 1 ? st.y0 : st.y1;
        bool small = true;
        for (int h : cand) small = small && heads[static_cast<size_t> (h)].small;
        const double minFromFree = (small ? 0.9 : 1.4) * sp;
        // Heads sit left of an up-stem and right of a down-stem. One on the
        // other side is only the displaced head of a second.
        std::vector<int> usual, other;
        for (int h : cand)
        {
            const auto& hd = heads[static_cast<size_t> (h)];
            if (std::abs (hd.y - st.freeY) < minFromFree) continue;
            const bool leftOfStem = std::abs (x - hd.x1) <= std::abs (x - hd.x0);
            ((leftOfStem == (st.dir == 1)) ? usual : other).push_back (h);
        }
        st.heads = usual;
        for (int h : other)
        {
            bool second = false;
            for (int u : usual)
                second = second || (heads[static_cast<size_t> (u)].staff == heads[static_cast<size_t> (h)].staff
                                    && std::abs (heads[static_cast<size_t> (u)].line - heads[static_cast<size_t> (h)].line) == 1);
            if (second) st.heads.push_back (h);
        }
        if (st.heads.empty()) continue;
        st.staff = heads[static_cast<size_t> (st.heads.front())].staff;
        segmentTaken[i] = true;
        const int index = static_cast<int> (stems.size());
        for (int h : st.heads) heads[static_cast<size_t> (h)].stems.push_back (index);
        stems.push_back (st);
    }
}

// A head both on its own stem's usual side and beside a neighbour's stem
// belongs to its own.
static void keepOwnStems (std::vector<Head>& heads, std::vector<Stem>& stems)
{
    for (size_t h = 0; h < heads.size(); ++h)
    {
        auto& hd = heads[h];
        if (hd.stems.size() < 2) continue;
        std::vector<int> usual;
        for (int s : hd.stems)
        {
            const auto& st = stems[static_cast<size_t> (s)];
            const bool leftOfStem = std::abs (st.x - hd.x1) <= std::abs (st.x - hd.x0);
            if (leftOfStem == (st.dir == 1)) usual.push_back (s);
        }
        if (usual.empty() || usual.size() == hd.stems.size()) continue;
        for (int s : hd.stems)
        {
            if (std::find (usual.begin(), usual.end(), s) != usual.end()) continue;
            auto& list = stems[static_cast<size_t> (s)].heads;
            list.erase (std::remove (list.begin(), list.end(), static_cast<int> (h)), list.end());
        }
        hd.stems = usual;
    }
}

void PageReader::findBeams()
{
    keepOwnStems (heads, stems);

    for (size_t i = 0; i < page.segments.size(); ++i)
    {
        if (segmentTaken[i]) continue;
        const auto& s = page.segments[i];
        if (s.vertical (0.2)) continue;
        if (std::abs (s.dy()) > 4.0 * std::abs (s.dx())) continue;
        const double sp = spaceNear (s.a.x, s.a.y);
        if (s.thickness < 0.22 * sp || s.thickness > 0.85 * sp || s.length() < 0.5 * sp) continue;
        beamSegs.push_back (static_cast<int> (i));
    }

    for (auto& st : stems)
    {
        const double sp = layout.staves[static_cast<size_t> (st.staff)].space;
        double attachedEnd = st.dir == 1 ? st.y1 : st.y0;
        // the head nearest the free end bounds where beams can be
        for (int h : st.heads)
        {
            const double y = heads[static_cast<size_t> (h)].y;
            if (st.dir == 1) attachedEnd = std::min (attachedEnd, y);
            else attachedEnd = std::max (attachedEnd, y);
        }
        struct Cand { int seg; double y, dist; };
        std::vector<Cand> cands;
        for (int bi : beamSegs)
        {
            const auto& b = page.segments[static_cast<size_t> (bi)];
            if (st.x < b.minX() - 0.3 * sp || st.x > b.maxX() + 0.3 * sp) continue;
            const double yb = b.yAt (std::clamp (st.x, b.minX(), b.maxX()));
            if (yb < st.y0 - 0.6 * sp || yb > st.y1 + 0.6 * sp) continue;
            if (std::abs (yb - attachedEnd) < 0.6 * sp) continue;
            if (st.dir == 1 && yb > attachedEnd) continue;
            if (st.dir == -1 && yb < attachedEnd) continue;
            cands.push_back ({ bi, yb, std::abs (yb - st.freeY) });
        }
        std::sort (cands.begin(), cands.end(), [] (const Cand& a, const Cand& b) { return a.dist < b.dist; });
        double lastDist = -1;
        for (const auto& c : cands)
        {
            if (! st.levels.empty() && std::abs (c.dist - lastDist) < 0.25 * sp)
            {
                st.levels.back().push_back (c.seg);
                continue;
            }
            const double limit = st.levels.empty() ? 0.75 * sp : lastDist + 1.15 * sp;
            if (c.dist > limit) break;
            st.levels.push_back ({ c.seg });
            lastDist = c.dist;
        }
    }
}

void PageReader::findFlags()
{
    for (size_t i = 0; i < page.glyphs.size(); ++i)
    {
        const auto& g = page.glyphs[i];
        const int count = flagCount (g.name);
        const bool slash = g.name.rfind ("graceNoteSlash", 0) == 0 || g.name.rfind ("graceNoteAcciaccatura", 0) == 0;
        if (count == 0 && ! slash) continue;
        const double sp = spaceNear (g.origin.x, g.origin.y);
        int best = -1;
        double bestD = 1e9;
        for (size_t s = 0; s < stems.size(); ++s)
        {
            const auto& st = stems[s];
            const double dx = std::abs (st.x - g.origin.x);
            if (dx > 0.7 * sp) continue;
            double dy;
            if (slash) dy = (g.box.cy() >= st.y0 - sp && g.box.cy() <= st.y1 + sp) ? 0.0 : 1e9;
            else dy = std::abs (st.freeY - g.origin.y);
            if (dy > 1.5 * sp) continue;
            if (dx + dy < bestD) { bestD = dx + dy; best = static_cast<int> (s); }
        }
        if (best < 0) continue;
        glyphUsed[i] = true;
        if (count > 0) stems[static_cast<size_t> (best)].flags = std::max (stems[static_cast<size_t> (best)].flags, count);
        if (slash) stems[static_cast<size_t> (best)].slash = true;
    }
}

void PageReader::buildItems()
{
    headItem.assign (heads.size(), -1);

    auto finishChord = [&] (Item& it) {
        it.x = 1e9; it.x1 = -1e9; it.y0 = 1e9; it.y1 = -1e9;
        int black = 0, half = 0, whole = 0, breve = 0;
        bool small = true;
        for (int h : it.heads)
        {
            const auto& hd = heads[static_cast<size_t> (h)];
            it.x1 = std::max (it.x1, hd.x1);
            it.y0 = std::min (it.y0, hd.y);
            it.y1 = std::max (it.y1, hd.y);
            small = small && hd.small;
            switch (hd.kind)
            {
                case HeadKind::Black: ++black; break;
                case HeadKind::Half: ++half; break;
                case HeadKind::Whole: ++whole; break;
                case HeadKind::Breve: ++breve; break;
                case HeadKind::None: break;
            }
        }
        // Where the chord stands: the left edge of its heads on the stem's
        // usual side (a second's displaced head does not move it).
        for (int h : it.heads)
        {
            const auto& hd = heads[static_cast<size_t> (h)];
            bool usual = true;
            if (it.stem >= 0)
            {
                const auto& st = stems[static_cast<size_t> (it.stem)];
                usual = st.dir == 1 ? hd.x0 < st.x - 0.2 * (hd.x1 - hd.x0) : hd.x0 >= st.x - 0.5 * (hd.x1 - hd.x0);
            }
            if (usual) it.x = std::min (it.x, hd.x0);
        }
        if (it.x > 1e8)
            for (int h : it.heads) it.x = std::min (it.x, heads[static_cast<size_t> (h)].x0);
        it.small = small;
        it.staff = heads[static_cast<size_t> (it.heads.front())].staff;

        HeadKind kind = HeadKind::Black;
        if (breve >= std::max ({ black, half, whole })) kind = HeadKind::Breve;
        if (whole >= std::max ({ black, half, breve })) kind = HeadKind::Whole;
        if (half >= std::max ({ black, whole, breve })) kind = HeadKind::Half;
        if (black >= std::max ({ half, whole, breve })) kind = HeadKind::Black;
        int beams = 0;
        if (it.stem >= 0)
        {
            const auto& st = stems[static_cast<size_t> (it.stem)];
            beams = std::max (static_cast<int> (st.levels.size()), st.flags);
            it.dir = st.dir;
            it.slash = st.slash;
        }
        switch (kind)
        {
            case HeadKind::Black: it.type = 2 + beams; break;
            case HeadKind::Half: it.type = 1; break;
            case HeadKind::Whole: it.type = 0; break;
            case HeadKind::Breve: it.type = -1; break;
            case HeadKind::None: break;
        }
    };

    for (size_t s = 0; s < stems.size(); ++s)
    {
        Item it;
        it.stem = static_cast<int> (s);
        it.heads = stems[s].heads;
        std::sort (it.heads.begin(), it.heads.end(), [&] (int a, int b) { return heads[static_cast<size_t> (a)].y > heads[static_cast<size_t> (b)].y; });
        finishChord (it);
        const int index = static_cast<int> (items.size());
        for (int h : it.heads)
            if (headItem[static_cast<size_t> (h)] < 0) headItem[static_cast<size_t> (h)] = index;
        items.push_back (it);
    }

    // Heads with no stem: whole notes and breves, grouped by column.
    std::vector<int> loose;
    for (size_t h = 0; h < heads.size(); ++h)
        if (heads[h].stems.empty()) loose.push_back (static_cast<int> (h));
    std::sort (loose.begin(), loose.end(), [&] (int a, int b) { return heads[static_cast<size_t> (a)].x0 < heads[static_cast<size_t> (b)].x0; });
    std::vector<bool> done (heads.size(), false);
    for (int h : loose)
    {
        if (done[static_cast<size_t> (h)]) continue;
        const auto& hd = heads[static_cast<size_t> (h)];
        const double w = hd.x1 - hd.x0;
        Item it;
        for (int k : loose)
        {
            const auto& o = heads[static_cast<size_t> (k)];
            if (done[static_cast<size_t> (k)] || o.staff != hd.staff) continue;
            if (o.x0 - hd.x0 > 1.15 * w || o.x0 < hd.x0 - 0.01) continue;
            it.heads.push_back (k);
            done[static_cast<size_t> (k)] = true;
        }
        std::sort (it.heads.begin(), it.heads.end(), [&] (int a, int b) { return heads[static_cast<size_t> (a)].y > heads[static_cast<size_t> (b)].y; });
        finishChord (it);
        const int index = static_cast<int> (items.size());
        for (int k : it.heads) headItem[static_cast<size_t> (k)] = index;
        items.push_back (it);
    }

    // Rests.
    for (size_t i = 0; i < page.glyphs.size(); ++i)
    {
        const auto& g = page.glyphs[i];
        const int type = restType (g.name);
        if (type == 99 || glyphStaff[i] < 0) continue;
        const auto& staff = layout.staves[static_cast<size_t> (glyphStaff[i])];
        Item it;
        it.rest = true;
        it.glyph = static_cast<int> (i);
        it.staff = glyphStaff[i];
        it.x = g.box.x0; it.x1 = g.box.x1;
        it.y0 = g.box.y0; it.y1 = g.box.y1;
        it.type = type;
        it.small = g.size < 0.8 * 4.0 * staff.space;
        items.push_back (it);
        glyphUsed[i] = true;
    }
}

void PageReader::findDots()
{
    struct DotRef { int glyph; double x, y; };
    std::vector<DotRef> dots;
    for (size_t i = 0; i < page.glyphs.size(); ++i)
        if (page.glyphs[i].name == "augmentationDot" && glyphStaff[i] >= 0)
            dots.push_back ({ static_cast<int> (i), page.glyphs[i].box.cx(), page.glyphs[i].box.cy() });
    std::sort (dots.begin(), dots.end(), [] (const DotRef& a, const DotRef& b) { return a.x < b.x; });

    // Repeat dots come in pairs beside a barline; they are not durations.
    std::vector<bool> repeatDot (dots.size(), false);
    for (size_t a = 0; a < dots.size(); ++a)
    {
        const int st = glyphStaff[static_cast<size_t> (dots[a].glyph)];
        const auto& staff = layout.staves[static_cast<size_t> (st)];
        const double sp = staff.space;
        const double step = staff.stepAt (dots[a].y);
        if (std::abs (step - 3) > 0.4 && std::abs (step - 5) > 0.4) continue;
        bool nearBar = false;
        const auto& sys = layout.systems[static_cast<size_t> (staff.system)];
        for (const auto& b : sys.bars)
            nearBar = nearBar || (dots[a].x > b.x0 - 1.6 * sp && dots[a].x < b.x1 + 1.6 * sp);
        if (! nearBar) continue;
        for (size_t b = 0; b < dots.size(); ++b)
        {
            if (b == a || std::abs (dots[b].x - dots[a].x) > 0.2 * sp) continue;
            const double other = staff.stepAt (dots[b].y);
            if (std::abs (other + step - 8) < 0.5) { repeatDot[a] = repeatDot[b] = true; }
        }
    }

    std::vector<int> owner (dots.size(), -1), chain (dots.size(), 0);
    for (size_t d = 0; d < dots.size(); ++d)
    {
        if (repeatDot[d]) { glyphUsed[static_cast<size_t> (dots[d].glyph)] = true; continue; }
        const int st = glyphStaff[static_cast<size_t> (dots[d].glyph)];
        const double sp = layout.staves[static_cast<size_t> (st)].space;
        // A second dot follows a first.
        for (size_t e = 0; e < d; ++e)
        {
            if (owner[e] < 0 || std::abs (dots[e].y - dots[d].y) > 0.2 * sp) continue;
            const double gap = dots[d].x - dots[e].x;
            if (gap > 0.2 * sp && gap < 1.1 * sp) { owner[d] = owner[e]; chain[d] = chain[e] + 1; }
        }
        if (owner[d] < 0)
        {
            int best = -1;
            double bestX = -1e9;
            for (size_t i = 0; i < items.size(); ++i)
            {
                const auto& it = items[i];
                if (it.staff != st) continue;
                const double reach = (it.small ? 1.6 : 2.4) * sp;
                if (it.x1 > dots[d].x + 0.1 * sp || dots[d].x - it.x1 > reach) continue;
                bool level = false;
                if (it.rest) level = dots[d].y >= it.y0 - 0.6 * sp && dots[d].y <= it.y1 + 0.6 * sp;
                else
                    for (int h : it.heads) level = level || std::abs (heads[static_cast<size_t> (h)].y - dots[d].y) <= 0.8 * sp;
                if (! level) continue;
                if (it.x1 > bestX) { bestX = it.x1; best = static_cast<int> (i); }
            }
            if (best < 0) continue;
            owner[d] = best;
            chain[d] = 1;
        }
        glyphUsed[static_cast<size_t> (dots[d].glyph)] = true;
        auto& it = items[static_cast<size_t> (owner[d])];
        it.dots = std::max (it.dots, chain[d]);
    }
}

void PageReader::findTies()
{
    std::set<int> used;
    std::vector<bool> startUsed (heads.size(), false), endUsed (heads.size(), false);
    for (size_t c = 0; c < page.curves.size(); ++c)
    {
        const auto& cv = page.curves[c];
        const int st = nearestStaff (cv.left.x, cv.left.y);
        if (st < 0) continue;
        const auto& staff = layout.staves[static_cast<size_t> (st)];
        const double sp = staff.space;
        if (cv.box.height() > 2.5 * sp && cv.box.height() > 0.4 * cv.box.width()) continue;

        int bestA = -1, bestB = -1;
        double bestCost = 1e9;
        std::vector<int> starts, ends;
        for (size_t h = 0; h < heads.size(); ++h)
        {
            const auto& hd = heads[h];
            if (hd.staff != st) continue;
            if (std::abs (hd.y - cv.left.y) <= 1.3 * sp && cv.left.x >= hd.x0 - 0.2 * sp && cv.left.x <= hd.x1 + 1.3 * sp)
                starts.push_back (static_cast<int> (h));
            if (std::abs (hd.y - cv.right.y) <= 1.3 * sp && cv.right.x >= hd.x0 - 1.3 * sp && cv.right.x <= hd.x1 + 0.2 * sp)
                ends.push_back (static_cast<int> (h));
        }
        for (int a : starts)
            for (int b : ends)
            {
                const auto& A = heads[static_cast<size_t> (a)];
                const auto& B = heads[static_cast<size_t> (b)];
                if (A.line != B.line || B.x0 <= A.x1 - 0.1 * sp) continue;
                if (startUsed[static_cast<size_t> (a)] || endUsed[static_cast<size_t> (b)]) continue;
                const double cost = std::abs (A.y - cv.left.y) + std::abs (B.y - cv.right.y)
                                  + std::abs (cv.left.x - A.x1) * 0.3 + std::abs (cv.right.x - B.x0) * 0.3;
                if (cost < bestCost) { bestCost = cost; bestA = a; bestB = b; }
            }
        if (bestA >= 0)
        {
            ties.push_back ({ bestA, bestB, false, false });
            startUsed[static_cast<size_t> (bestA)] = endUsed[static_cast<size_t> (bestB)] = true;
            used.insert (static_cast<int> (c));
            continue;
        }
        // A tie running off the end of a system, or in from the start of one.
        const auto& sys = layout.systems[static_cast<size_t> (staff.system)];
        if (cv.right.x >= sys.x1 - 2.5 * sp && cv.box.width() < 6 * sp)
        {
            for (int a : starts)
                if (! startUsed[static_cast<size_t> (a)] && heads[static_cast<size_t> (a)].x1 < cv.right.x)
                {
                    bool beyond = false;   // no head on that line further right
                    for (size_t h = 0; h < heads.size(); ++h)
                        beyond = beyond || (heads[h].staff == st && heads[h].line == heads[static_cast<size_t> (a)].line && heads[h].x0 > heads[static_cast<size_t> (a)].x1 && heads[h].x0 < cv.right.x);
                    if (beyond) continue;
                    ties.push_back ({ a, -1, true, false });
                    startUsed[static_cast<size_t> (a)] = true;
                    used.insert (static_cast<int> (c));
                    break;
                }
            if (used.count (static_cast<int> (c))) continue;
        }
        if (cv.box.width() < 6 * sp)
        {
            for (int b : ends)
            {
                if (endUsed[static_cast<size_t> (b)]) continue;
                bool earlier = false;   // the first note on its line in this system
                for (size_t h = 0; h < heads.size(); ++h)
                    earlier = earlier || (heads[h].staff == st && heads[h].x1 < cv.left.x + 0.1 * sp && heads[h].line == heads[static_cast<size_t> (b)].line);
                if (earlier) continue;
                bool firstThing = true; // nothing but clef/key/time before the curve
                for (const auto& it : items)
                    firstThing = firstThing && ! (it.staff == st && it.x1 < cv.left.x - 0.1 * sp);
                if (! firstThing) continue;
                ties.push_back ({ -1, b, false, true });
                endUsed[static_cast<size_t> (b)] = true;
                used.insert (static_cast<int> (c));
                break;
            }
        }
    }
    findSlurs (used);
}

void PageReader::findSlurs (const std::set<int>& tieCurves)
{
    for (size_t c = 0; c < page.curves.size(); ++c)
    {
        if (tieCurves.count (static_cast<int> (c))) continue;
        const auto& cv = page.curves[c];
        const int st = nearestStaff (cv.left.x, cv.left.y);
        if (st < 0) continue;
        const double sp = layout.staves[static_cast<size_t> (st)].space;
        if (cv.box.width() < 1.5 * sp) continue;
        auto nearestItem = [&] (Point p) {
            int best = -1;
            double bestD = 2.5 * sp;
            for (size_t i = 0; i < items.size(); ++i)
            {
                const auto& it = items[i];
                if (it.rest || it.staff != st) continue;
                const double dx = p.x < it.x ? it.x - p.x : (p.x > it.x1 ? p.x - it.x1 : 0.0);
                double dy = 1e9;
                for (int h : it.heads) dy = std::min (dy, std::abs (heads[static_cast<size_t> (h)].y - p.y));
                if (it.stem >= 0) dy = std::min (dy, std::abs (stems[static_cast<size_t> (it.stem)].freeY - p.y));
                const double d = dx + 0.5 * dy;
                if (dx < 1.5 * sp && dy < 4 * sp && d < bestD) { bestD = d; best = static_cast<int> (i); }
            }
            return best;
        };
        const int a = nearestItem (cv.left), b = nearestItem (cv.right);
        if (a < 0 && b < 0) continue;
        if (a == b) continue;
        const int number = ++slurCounter;
        if (a >= 0) items[static_cast<size_t> (a)].slurs.push_back ({ number, true });
        if (b >= 0) items[static_cast<size_t> (b)].slurs.push_back ({ number, false });
    }
}

void PageReader::findArticulations()
{
    for (size_t i = 0; i < page.glyphs.size(); ++i)
    {
        const auto& g = page.glyphs[i];
        const auto art = articulationFor (g.name);
        if (art.empty() || glyphStaff[i] < 0) continue;
        const int st = glyphStaff[i];
        const double sp = layout.staves[static_cast<size_t> (st)].space;
        int best = -1;
        double bestD = 1e9;
        const bool fermata = art.rfind ("fermata", 0) == 0;
        for (size_t k = 0; k < items.size(); ++k)
        {
            const auto& it = items[k];
            if (it.staff != st || (it.rest && ! fermata)) continue;
            const double cx = g.box.cx();
            const double dx = cx < it.x ? it.x - cx : (cx > it.x1 ? cx - it.x1 : 0.0);
            if (dx > 1.0 * sp) continue;
            double top = it.y0, bottom = it.y1;
            if (it.stem >= 0)
            {
                top = std::min (top, stems[static_cast<size_t> (it.stem)].y0);
                bottom = std::max (bottom, stems[static_cast<size_t> (it.stem)].y1);
            }
            const double dy = g.box.cy() < top ? top - g.box.cy() : (g.box.cy() > bottom ? g.box.cy() - bottom : 0.0);
            if (dy > (fermata ? 8.0 : 4.0) * sp) continue;
            const double d = dx * 2 + dy;
            if (d < bestD) { bestD = d; best = static_cast<int> (k); }
        }
        if (best < 0) continue;
        glyphUsed[i] = true;
        items[static_cast<size_t> (best)].articulations.push_back (art);
    }
}

// ===========================================================================
// One system

struct BarRange { double x0, x1; };

std::vector<BarRange> barRanges (const System& sys)
{
    std::vector<BarRange> out;
    double start = sys.x0;
    for (const auto& b : sys.bars)
    {
        out.push_back ({ start, b.x() });
        start = b.x();
    }
    if (sys.x1 - start > 1.0) out.push_back ({ start, sys.x1 });
    return out;
}

Fraction itemDuration (const Item& it)
{
    Fraction d = dotted (baseDuration (it.type), it.dots);
    if (it.tuplet.active()) d = d * Fraction (it.tuplet.normal, it.tuplet.actual);
    return d;
}

SystemRead PageReader::readSystem (int systemIndex, std::vector<StaffState>& state, const std::vector<int>& stateIndexOfStaff)
{
    const auto& sys = layout.systems[static_cast<size_t> (systemIndex)];
    SystemRead out;
    out.page = page.number;
    out.parts = sys.parts;
    out.partOfStaff = sys.partOfStaff;

    auto ranges = barRanges (sys);
    const int nStaves = static_cast<int> (sys.staves.size());

    // Bars with nothing but signatures in them are not bars: the space
    // before a start-repeat at the opening of a system, or the courtesy
    // signatures after the last barline.
    auto hasMusicIn = [&] (double x0, double x1) {
        for (const auto& it : items)
        {
            if (it.x < x0 || it.x >= x1) continue;
            for (int si : sys.staves)
                if (it.staff == si) return true;
        }
        // a multi-bar rest
        for (size_t i = 0; i < page.glyphs.size(); ++i)
            if (page.glyphs[i].name == "restHBar" && page.glyphs[i].box.cx() >= x0 && page.glyphs[i].box.cx() < x1) return true;
        for (size_t i = 0; i < page.segments.size(); ++i)
        {
            const auto& s = page.segments[i];
            if (! s.horizontal (0.01) || s.minX() < x0 || s.maxX() > x1) continue;
            for (int si : sys.staves)
            {
                const auto& st = layout.staves[static_cast<size_t> (si)];
                if (s.thickness > 0.7 * st.space && std::abs ((s.a.y + s.b.y) * 0.5 - st.middle()) < 0.6 * st.space && s.length() > 2 * st.space) return true;
            }
        }
        return false;
    };
    std::vector<int> barColumnLeft;   // the BarColumn at each kept bar's left (-1: the system's start)
    {
        auto columnAt = [&] (double x) {
            for (size_t c = 0; c < sys.bars.size(); ++c)
                if (std::abs (sys.bars[c].x() - x) < 0.5) return static_cast<int> (c);
            return -1;
        };
        std::vector<BarRange> kept;
        std::vector<int> left;
        int carriedLeft = -2;
        for (size_t r = 0; r < ranges.size(); ++r)
        {
            const bool music = hasMusicIn (ranges[r].x0, ranges[r].x1);
            if (! music && r == 0 && ranges.size() > 1)
            {
                // The opening before a start-repeat: part of the first bar.
                carriedLeft = columnAt (ranges[r].x1);
                ranges[r + 1].x0 = ranges[r].x0;
                continue;
            }
            if (! music && r + 1 == ranges.size() && r > 0) continue;   // courtesy signatures
            kept.push_back (ranges[r]);
            left.push_back (carriedLeft != -2 ? carriedLeft : columnAt (ranges[r].x0));
            carriedLeft = -2;
        }
        ranges = kept;
        barColumnLeft = left;
    }
    out.bars = static_cast<int> (ranges.size());
    out.staves.assign (static_cast<size_t> (nStaves), std::vector<StaffBar> (ranges.size()));
    out.leftBar.assign (ranges.size(), {});
    out.rightBar.assign (ranges.size(), {});
    out.hasLeft.assign (ranges.size(), false);

    // Barline styles and repeats.
    for (size_t r = 0; r < ranges.size(); ++r)
    {
        const int leftCol = barColumnLeft[r];
        if (leftCol >= 0 && static_cast<size_t> (leftCol) < sys.bars.size())
        {
            const auto& b = sys.bars[static_cast<size_t> (leftCol)];
            if (b.dotsAfter)
            {
                out.hasLeft[r] = true;
                out.leftBar[r].repeat = true;
                out.leftBar[r].style = BarStyle::HeavyLight;
            }
        }
        // the column at this range's right edge
        for (const auto& b : sys.bars)
            if (std::abs (b.x() - ranges[r].x1) < 0.5)
            {
                out.rightBar[r].style = b.style;
                if (b.dotsBefore) { out.rightBar[r].repeat = true; out.rightBar[r].style = BarStyle::LightHeavy; }
                if (b.dotsAfter && r + 1 < ranges.size())
                {
                    out.hasLeft[r + 1] = true;
                    out.leftBar[r + 1].repeat = true;
                    out.leftBar[r + 1].style = BarStyle::HeavyLight;
                    if (! b.dotsBefore) out.rightBar[r].style = BarStyle::Regular;
                }
            }
    }

    auto barOf = [&] (double x) {
        for (size_t r = 0; r < ranges.size(); ++r)
            if (x >= ranges[r].x0 - 0.01 && x < ranges[r].x1) return static_cast<int> (r);
        return -1;
    };

    std::vector<TimeSignature> startTime;
    for (int k = 0; k < nStaves; ++k) startTime.push_back (state[static_cast<size_t> (stateIndexOfStaff[static_cast<size_t> (k)])].time);
    out.openingClef.assign (static_cast<size_t> (nStaves), Clef {});

    for (int k = 0; k < nStaves; ++k)
    {
        const int si = sys.staves[static_cast<size_t> (k)];
        const auto& staff = layout.staves[static_cast<size_t> (si)];
        const double sp = staff.space;
        auto& st = state[static_cast<size_t> (stateIndexOfStaff[static_cast<size_t> (k)])];
        auto& bars = out.staves[static_cast<size_t> (k)];
        for (size_t r = 0; r < ranges.size(); ++r)
        {
            bars[r].page = page.number;
            bars[r].box = { ranges[r].x0, staff.top() - 2 * sp, ranges[r].x1, staff.bottom() + 2 * sp };
        }

        // --- clefs, by position
        struct ClefAt { double x; Clef clef; };
        std::vector<ClefAt> clefs;
        for (size_t i = 0; i < page.glyphs.size(); ++i)
        {
            const auto& g = page.glyphs[i];
            if (glyphStaff[i] != si || ! isClefName (g.name)) continue;
            Clef c = clefFor (g.name, staff.stepAt (g.origin.y));
            // LilyPond prints an octave clef's "8" as a small digit of its own.
            for (size_t j = 0; j < page.glyphs.size(); ++j)
            {
                const auto& d = page.glyphs[j];
                if (timeDigit (d.name) != 8) continue;
                if (d.size > 0.75 * 4.0 * sp) continue;
                if (d.box.cx() < g.box.x0 - 0.3 * sp || d.box.cx() > g.box.x1 + 0.3 * sp) continue;
                if (d.box.y0 >= g.box.y1 - 0.5 * sp && d.box.y0 <= g.box.y1 + 1.5 * sp) { c.octave = -1; glyphUsed[j] = true; }
                else if (d.box.y1 <= g.box.y0 + 0.5 * sp && d.box.y1 >= g.box.y0 - 1.5 * sp) { c.octave = 1; glyphUsed[j] = true; }
            }
            // ... or as a number in a text font ("8", "15").
            for (const auto& t : page.texts)
            {
                if (t.text != "8" && t.text != "15") continue;
                if (t.box.cx() < g.box.x0 - 0.3 * sp || t.box.cx() > g.box.x1 + 0.3 * sp) continue;
                const int by = t.text == "8" ? 1 : 2;
                if (t.box.y0 >= g.box.y1 - 0.7 * sp && t.box.y0 <= g.box.y1 + 1.5 * sp) c.octave = -by;
                else if (t.box.y1 <= g.box.y0 + 0.7 * sp && t.box.y1 >= g.box.y0 - 1.5 * sp) c.octave = by;
            }
            clefs.push_back ({ g.box.x0, c });
            glyphUsed[i] = true;
        }
        std::sort (clefs.begin(), clefs.end(), [] (const ClefAt& a, const ClefAt& b) { return a.x < b.x; });
        bool openingClef = ! clefs.empty() && clefs.front().x < (ranges.empty() ? sys.x1 : ranges.front().x0 + 4 * sp);
        if (! openingClef && ! st.clefKnown)
            bars.empty() ? void() : bars[0].problems.push_back ({ Issue::Warning, "No clef at the start of the staff; read as a treble clef." });
        if (openingClef) { st.clef = clefs.front().clef; st.clefKnown = true; }
        out.openingClef[static_cast<size_t> (k)] = st.clef;
        auto clefAt = [&] (double x) {
            Clef c = st.clef;
            for (const auto& ca : clefs)
                if (ca.x <= x) c = ca.clef;
            return c;
        };

        // --- time signatures, by bar
        {
            std::vector<int> digits;
            for (size_t i = 0; i < page.glyphs.size(); ++i)
            {
                const auto& g = page.glyphs[i];
                if (glyphStaff[i] != si || glyphUsed[i]) continue;
                const int d = timeDigit (g.name);
                const bool symbol = g.name == "timeSigCommon" || g.name == "timeSigCutCommon";
                if (d < 0 && ! symbol) continue;
                if (g.size < 0.75 * 4.0 * sp) continue;
                if (g.box.cy() < staff.top() - 0.3 * sp || g.box.cy() > staff.bottom() + 0.3 * sp) continue;
                digits.push_back (static_cast<int> (i));
            }
            std::sort (digits.begin(), digits.end(), [&] (int a, int b) { return page.glyphs[static_cast<size_t> (a)].box.x0 < page.glyphs[static_cast<size_t> (b)].box.x0; });
            std::vector<bool> taken (digits.size(), false);
            for (size_t a = 0; a < digits.size(); ++a)
            {
                if (taken[a]) continue;
                const auto& g = page.glyphs[static_cast<size_t> (digits[a])];
                // a column: everything overlapping it horizontally
                double cx0 = g.box.x0, cx1 = g.box.x1;
                std::vector<size_t> col;
                for (size_t b = a; b < digits.size(); ++b)
                {
                    const auto& o = page.glyphs[static_cast<size_t> (digits[b])];
                    if (o.box.x0 > cx1 + 0.15 * sp) break;
                    col.push_back (b);
                    cx1 = std::max (cx1, o.box.x1);
                    cx0 = std::min (cx0, o.box.x0);
                }
                TimeSignature ts;
                std::string upper, lower;
                bool symbolSeen = false;
                for (size_t b : col)
                {
                    taken[b] = true;
                    const auto& o = page.glyphs[static_cast<size_t> (digits[b])];
                    glyphUsed[static_cast<size_t> (digits[b])] = true;
                    if (o.name == "timeSigCommon") { ts = { 4, 4, TimeSignature::Common }; symbolSeen = true; continue; }
                    if (o.name == "timeSigCutCommon") { ts = { 2, 2, TimeSignature::Cut }; symbolSeen = true; continue; }
                    const char digit = static_cast<char> ('0' + timeDigit (o.name));
                    (o.box.cy() < staff.middle() ? upper : lower) += digit;
                }
                if (! symbolSeen)
                {
                    if (upper.empty() || lower.empty())
                    {
                        const int bar = barOf (cx0);
                        if (bar >= 0) bars[static_cast<size_t> (bar)].problems.push_back ({ Issue::Warning, "A number on the staff that is not a whole time signature was ignored." });
                        continue;
                    }
                    ts.beats = std::stoi (upper);
                    ts.beatType = std::stoi (lower);
                }
                const int bar = barOf (cx0);
                if (bar < 0) continue;   // courtesy signature at the end of the system
                bars[static_cast<size_t> (bar)].time = ts;
            }
        }

        // --- key signatures: accidentals before a bar's first note that belong to no note
        struct Acc { int glyph; double x0, x1; int line; int value; bool attached = false; int head = -1; };
        std::vector<Acc> accs;
        for (size_t i = 0; i < page.glyphs.size(); ++i)
        {
            const auto& g = page.glyphs[i];
            if (glyphStaff[i] != si) continue;
            const int v = accidentalValue (g.name);
            if (v == 99) continue;
            accs.push_back ({ static_cast<int> (i), g.box.x0, g.box.x1, static_cast<int> (std::lround (staff.stepAt (g.origin.y))), v });
        }
        std::sort (accs.begin(), accs.end(), [] (const Acc& a, const Acc& b) { return a.x0 < b.x0; });

        // Attach accidentals to noteheads: right to left from each chord.
        std::vector<int> staffItems;
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].staff == si && barOf (items[i].x) >= 0) staffItems.push_back (static_cast<int> (i));
        std::sort (staffItems.begin(), staffItems.end(), [&] (int a, int b) { return items[static_cast<size_t> (a)].x < items[static_cast<size_t> (b)].x; });

        for (int ii : staffItems)
        {
            const auto& it = items[static_cast<size_t> (ii)];
            if (it.rest) continue;
            double leftEdge = 1e9;
            for (int h : it.heads) leftEdge = std::min (leftEdge, heads[static_cast<size_t> (h)].x0);
            // accidentals of this chord stand in columns to its left, each close to the next
            double frontier = leftEdge;
            for (int pass = 0; pass < 6; ++pass)
            {
                bool any = false;
                double newFrontier = frontier;
                for (auto& a : accs)
                {
                    if (a.attached) continue;
                    if (a.x1 > frontier + 0.3 * sp) continue;
                    if (frontier - a.x1 > (pass == 0 ? 1.2 : 0.7) * sp) continue;
                    int match = -1;
                    for (int h : it.heads)
                        if (heads[static_cast<size_t> (h)].line == a.line) match = h;
                    if (match < 0) continue;
                    a.attached = true;
                    a.head = match;
                    newFrontier = std::min (newFrontier, a.x0);
                    any = true;
                }
                if (! any) break;
                frontier = newFrontier;
            }
        }

        // Group the loose accidentals of each bar's opening into a key signature.
        std::vector<int> barKey (ranges.size(), 99);
        std::vector<double> keyX (ranges.size(), -1);
        for (size_t r = 0; r < ranges.size(); ++r)
        {
            double firstItem = ranges[r].x1;
            for (int ii : staffItems)
                if (barOf (items[static_cast<size_t> (ii)].x) == static_cast<int> (r)) { firstItem = std::min (firstItem, items[static_cast<size_t> (ii)].x); }
            std::vector<Acc*> run;
            for (auto& a : accs)
                if (! a.attached && a.x0 >= ranges[r].x0 - 0.5 * sp && a.x1 <= firstItem + 0.1 * sp) run.push_back (&a);
            if (run.empty()) continue;
            const Clef c = clefAt (run.front()->x0);
            const int base = bottomLineDiatonic (c);
            int naturals = 0, sharps = 0, flats = 0;
            bool orderOk = true;
            static const int sharpOrder[7] = { 3, 0, 4, 1, 5, 2, 6 };
            static const int flatOrder[7] = { 6, 2, 5, 1, 4, 0, 3 };
            for (auto* a : run)
            {
                const int letter = ((base + a->line) % 7 + 7) % 7;
                if (a->value == 0) { if (sharps + flats > 0) orderOk = false; ++naturals; continue; }
                if (a->value == 1) { if (flats > 0 || sharps >= 7 || sharpOrder[sharps] != letter) orderOk = false; ++sharps; }
                else if (a->value == -1) { if (sharps > 0 || flats >= 7 || flatOrder[flats] != letter) orderOk = false; ++flats; }
                else orderOk = false;
            }
            if (! orderOk)
            {
                bars[r].problems.push_back ({ Issue::Warning, "Accidentals at the start of the bar were not a key signature and belong to no note; they were left out." });
                for (auto* a : run) a->attached = true;
                continue;
            }
            barKey[r] = sharps > 0 ? sharps : -flats;
            keyX[r] = run.front()->x0;
            for (auto* a : run) { a->attached = true; glyphUsed[static_cast<size_t> (a->glyph)] = true; }
            (void) naturals;
        }
        // A system that opens with no key signature is in C (or A minor).
        if (! ranges.empty() && barKey[0] == 99) barKey[0] = 0;
        for (size_t r = 0; r < ranges.size(); ++r)
            if (barKey[r] != 99) bars[r].key = barKey[r];

        for (const auto& a : accs)
            if (! a.attached)
            {
                const int bar = barOf (a.x0);
                if (bar >= 0) bars[static_cast<size_t> (bar)].problems.push_back ({ Issue::Warning, "An accidental that belongs to no note was left out." });
            }

        // --- notes and rests into bars, with pitches
        std::map<int, std::vector<int>> itemsOfBar;
        for (int ii : staffItems) itemsOfBar[barOf (items[static_cast<size_t> (ii)].x)].push_back (ii);

        int key = st.key;
        for (size_t r = 0; r < ranges.size(); ++r)
        {
            auto& bar = bars[r];
            if (bar.key) key = *bar.key;
            if (bar.time) { st.time = *bar.time; st.timeKnown = true; }

            // Clefs in this bar after its opening become clef changes.
            for (const auto& ca : clefs)
            {
                if (ca.x < ranges[r].x0 || ca.x >= ranges[r].x1) continue;
                if (r == 0 && &ca == &clefs.front() && openingClef) continue;
                bar.clefs.push_back ({ Fraction (-1), 1, ca.clef });   // offset filled in below
                bar.clefX.push_back (ca.x);
            }

            std::map<int, int> barAlter;   // diatonic number -> alter set in this bar
            auto& list = itemsOfBar[static_cast<int> (r)];
            std::sort (list.begin(), list.end(), [&] (int a, int b) { return items[static_cast<size_t> (a)].x < items[static_cast<size_t> (b)].x; });
            for (int ii : list)
            {
                auto& it = items[static_cast<size_t> (ii)];
                it.bar = static_cast<int> (r);
                if (it.rest) continue;
                const Clef c = clefAt (it.x);
                const int base = bottomLineDiatonic (c);
                it.notes.clear();
                for (int h : it.heads)
                {
                    const auto& hd = heads[static_cast<size_t> (h)];
                    Note n;
                    n.line = hd.line;
                    n.notehead = hd.shape;
                    const int diatonic = base + hd.line;
                    n.pitch.step = ((diatonic % 7) + 7) % 7;
                    n.pitch.octave = static_cast<int> (std::floor (diatonic / 7.0));
                    int shown = 99;
                    for (const auto& a : accs)
                        if (a.head == h) shown = a.value;
                    bool tiedIn = false;
                    for (const auto& t : ties)
                        if (t.to == h) { n.tieStop = true; tiedIn = true; }
                    for (const auto& t : ties)
                        if (t.from == h) n.tieStart = true;
                    if (shown != 99)
                    {
                        n.accidentalShown = true;
                        n.accidental = shown;
                        n.pitch.alter = shown;
                        barAlter[diatonic] = shown;
                    }
                    else
                    {
                        int alter = 0;
                        static const int sharpOrder[7] = { 3, 0, 4, 1, 5, 2, 6 };
                        static const int flatOrder[7] = { 6, 2, 5, 1, 4, 0, 3 };
                        for (int k2 = 0; k2 < std::abs (key) && k2 < 7; ++k2)
                            if ((key > 0 ? sharpOrder[k2] : flatOrder[k2]) == n.pitch.step) alter = key > 0 ? 1 : -1;
                        const auto found = barAlter.find (diatonic);
                        if (found != barAlter.end()) alter = found->second;
                        if (tiedIn)
                        {
                            // A tied note keeps the pitch it was tied from.
                            for (const auto& t : ties)
                                if (t.to == h && t.from >= 0 && headItem[static_cast<size_t> (t.from)] >= 0)
                                    for (const auto& pn : items[static_cast<size_t> (headItem[static_cast<size_t> (t.from)])].notes)
                                        if (pn.line == hd.line) alter = pn.pitch.alter;
                            for (const auto& ot : st.openTies)
                                if (ot.line == hd.line && n.tieStop && ! std::any_of (ties.begin(), ties.end(), [&] (const TieLink& t) { return t.to == h && t.from >= 0; }))
                                    alter = ot.pitch.alter;
                        }
                        n.pitch.alter = alter;
                    }
                    if (std::abs (hd.step - hd.line) > 0.3)
                        bar.problems.push_back ({ Issue::Warning, "A notehead sits between a line and a space." });
                    it.notes.push_back (n);
                }
                if (it.stem < 0 && (heads[static_cast<size_t> (it.heads.front())].kind == HeadKind::Black || heads[static_cast<size_t> (it.heads.front())].kind == HeadKind::Half))
                    bar.problems.push_back ({ Issue::Warning, "A note without a stem was read as a quarter or half note." });
            }
        }
        st.key = key;
        for (const auto& ca : clefs) st.clef = ca.clef;   // the next system goes on from the last clef

        // ties leaving this system
        st.openTies.clear();
        for (const auto& t : ties)
            if (t.toNextSystem && heads[static_cast<size_t> (t.from)].staff == si && headItem[static_cast<size_t> (t.from)] >= 0)
                for (const auto& n : items[static_cast<size_t> (headItem[static_cast<size_t> (t.from)])].notes)
                    if (n.line == heads[static_cast<size_t> (t.from)].line) st.openTies.push_back ({ n.line, n.pitch });
    }

    // --- tuplets
    {
        struct Mark { double cx, cy; Box box; int actual, normal; int staff; };
        std::vector<Mark> marks;
        // SMuFL tuplet digits
        for (size_t i = 0; i < page.glyphs.size(); ++i)
        {
            const int d = tupletDigit (page.glyphs[i].name);
            if (d < 0) continue;
            const auto& g = page.glyphs[i];
            bool merged = false;
            for (auto& m : marks)
                if (std::abs (m.box.cy() - g.box.cy()) < 0.3 * g.box.height() && g.box.x0 - m.box.x1 < 0.3 * g.size * 0.25)
                {
                    m.actual = m.actual * 10 + d;
                    m.box.add (g.box);
                    m.cx = m.box.cx();
                    merged = true;
                }
            if (! merged) marks.push_back ({ g.box.cx(), g.box.cy(), g.box, d, 0, glyphStaff[i] });
            glyphUsed[i] = true;
        }
        // Italic numbers in a text font: "3", "6", "3:2"
        for (const auto& t : page.texts)
        {
            if (! t.italic) continue;
            const auto& s = t.text;
            if (s.empty() || s.size() > 5) continue;
            int a = 0, n = 0;
            bool colon = false, ok = true;
            for (char ch : s)
            {
                if (ch >= '0' && ch <= '9') (colon ? n : a) = (colon ? n : a) * 10 + (ch - '0');
                else if (ch == ':' && ! colon) colon = true;
                else ok = false;
            }
            if (! ok || a < 2) continue;
            const int st = nearestStaff (t.box.cx(), t.box.cy());
            if (st < 0) continue;
            marks.push_back ({ t.box.cx(), t.box.cy(), t.box, a, n, st });
        }

        for (const auto& m : marks)
        {
            bool inSystem = false;
            for (int si : sys.staves) inSystem = inSystem || si == m.staff;
            if (! inSystem) continue;
            const double sp = layout.staves[static_cast<size_t> (m.staff)].space;
            const auto& staff = layout.staves[static_cast<size_t> (m.staff)];
            if (m.cy > staff.top() - 0.3 * sp && m.cy < staff.bottom() + 0.3 * sp && m.box.height() > 1.5 * sp) continue;

            // Its span: a bracket either side of it, or the beam under it.
            double x0 = 1e9, x1 = -1e9;
            bool bracket = false;
            for (size_t i = 0; i < page.segments.size(); ++i)
            {
                const auto& s = page.segments[i];
                if (s.thickness > 0.35 * sp || s.vertical (0.2)) continue;
                if (std::abs (s.yAt (s.maxX()) - m.cy) > 1.3 * sp && std::abs (s.yAt (s.minX()) - m.cy) > 1.3 * sp) continue;
                if (s.maxX() <= m.box.x0 + 0.3 * sp && s.maxX() >= m.box.x0 - 2.0 * sp) { x0 = std::min (x0, s.minX()); bracket = true; }
                if (s.minX() >= m.box.x1 - 0.3 * sp && s.minX() <= m.box.x1 + 2.0 * sp) { x1 = std::max (x1, s.maxX()); bracket = true; }
            }
            std::vector<int> group;
            if (bracket && x0 < 1e8 && x1 > -1e8)
            {
                for (size_t i = 0; i < items.size(); ++i)
                    if (items[i].staff == m.staff && items[i].x >= x0 - 0.8 * sp && items[i].x1 <= x1 + 0.8 * sp && ! items[i].small)
                        group.push_back (static_cast<int> (i));
            }
            else
            {
                int bestBeam = -1;
                double bestD = 3.5 * sp;
                for (int bi : beamSegs)
                {
                    const auto& b = page.segments[static_cast<size_t> (bi)];
                    if (m.cx < b.minX() - 0.5 * sp || m.cx > b.maxX() + 0.5 * sp) continue;
                    const double d = std::abs (b.yAt (std::clamp (m.cx, b.minX(), b.maxX())) - m.cy);
                    if (d < bestD) { bestD = d; bestBeam = bi; }
                }
                if (bestBeam >= 0)
                    for (size_t i = 0; i < items.size(); ++i)
                    {
                        if (items[i].stem < 0) continue;
                        for (const auto& lv : stems[static_cast<size_t> (items[i].stem)].levels)
                            for (int s : lv)
                                if (s == bestBeam && std::find (group.begin(), group.end(), static_cast<int> (i)) == group.end())
                                    group.push_back (static_cast<int> (i));
                    }
            }
            if (group.empty()) continue;
            std::sort (group.begin(), group.end(), [&] (int a, int b) { return items[static_cast<size_t> (a)].x < items[static_cast<size_t> (b)].x; });
            int normal = m.normal;
            if (normal == 0)
            {
                if (m.actual == 2) normal = 3;
                else if (m.actual == 4) normal = 3;
                else { normal = 1; while (normal * 2 < m.actual) normal *= 2; }
            }
            for (size_t k = 0; k < group.size(); ++k)
            {
                auto& it = items[static_cast<size_t> (group[k])];
                if (it.tuplet.active()) continue;
                it.tuplet.actual = m.actual;
                it.tuplet.normal = normal;
                it.tuplet.start = k == 0;
                it.tuplet.stop = k + 1 == group.size();
                it.tuplet.bracket = bracket;
            }
        }
    }

    // --- voices, timing and the check, bar by bar
    for (int k = 0; k < nStaves; ++k)
    {
        const int si = sys.staves[static_cast<size_t> (k)];
        const auto& staff = layout.staves[static_cast<size_t> (si)];
        const double sp = staff.space;
        auto& st = state[static_cast<size_t> (stateIndexOfStaff[static_cast<size_t> (k)])];
        auto& bars = out.staves[static_cast<size_t> (k)];
        TimeSignature time = startTime[static_cast<size_t> (k)];

        for (size_t r = 0; r < ranges.size(); ++r)
        {
            auto& bar = bars[r];
            if (bar.time) time = *bar.time;
            const Fraction length = time.length();

            std::vector<int> list;
            for (size_t i = 0; i < items.size(); ++i)
                if (items[i].staff == si && items[i].bar == static_cast<int> (r)) list.push_back (static_cast<int> (i));
            std::sort (list.begin(), list.end(), [&] (int a, int b) { return items[static_cast<size_t> (a)].x < items[static_cast<size_t> (b)].x; });

            // multi-bar rest
            for (size_t i = 0; i < page.glyphs.size(); ++i)
            {
                const auto& g = page.glyphs[i];
                if (glyphStaff[i] != si || g.name != "restHBar" || g.box.cx() < ranges[r].x0 || g.box.cx() >= ranges[r].x1) continue;
                bar.multiRest = 1;
                glyphUsed[i] = true;
            }
            for (const auto& s : page.segments)
                if (s.horizontal (0.01) && s.thickness > 0.7 * sp && s.length() > 2 * sp && std::abs ((s.a.y + s.b.y) * 0.5 - staff.middle()) < 0.6 * sp
                    && s.minX() >= ranges[r].x0 && s.maxX() <= ranges[r].x1)
                    bar.multiRest = 1;
            if (bar.multiRest)
            {
                std::string number;
                std::vector<std::pair<double, int>> ds;
                for (size_t i = 0; i < page.glyphs.size(); ++i)
                {
                    const auto& g = page.glyphs[i];
                    const int d = timeDigit (g.name);
                    if (d < 0 || glyphStaff[i] != si || g.box.cx() < ranges[r].x0 || g.box.cx() >= ranges[r].x1) continue;
                    if (g.box.cy() > staff.top()) continue;
                    ds.push_back ({ g.box.x0, d });
                    glyphUsed[i] = true;
                }
                std::sort (ds.begin(), ds.end());
                for (auto& d : ds) number += static_cast<char> ('0' + d.second);
                for (const auto& t : page.texts)
                    if (number.empty() && t.box.cx() >= ranges[r].x0 && t.box.cx() < ranges[r].x1 && t.box.cy() < staff.top() && t.box.cy() > staff.top() - 4 * sp
                        && ! t.text.empty() && std::all_of (t.text.begin(), t.text.end(), [] (char c) { return c >= '0' && c <= '9'; }))
                        number = t.text;
                bar.multiRest = number.empty() ? 1 : std::max (1, std::stoi (number));
                Event e;
                e.rest = true; e.measureRest = true; e.type = 0; e.duration = length;
                bar.events.push_back (e);
                bar.content = length;
                bar.hasNotes = true;
                continue;
            }
            if (list.empty()) continue;
            bar.hasNotes = true;

            // grace notes: small, and followed by a full-size note
            for (int ii : list)
            {
                auto& it = items[static_cast<size_t> (ii)];
                it.grace = it.small && ! it.rest;
                it.duration = it.grace ? Fraction (0) : itemDuration (it);
            }

            auto sumOf = [&] (const std::vector<int>& v) {
                Fraction f;
                for (int ii : v) if (! items[static_cast<size_t> (ii)].grace) f += items[static_cast<size_t> (ii)].duration;
                return f;
            };

            // A whole rest alone fills the bar, whatever its length.
            std::vector<int> normals;
            for (int ii : list) if (! items[static_cast<size_t> (ii)].grace) normals.push_back (ii);

            // One voice, or two? Two when an up-stem and a down-stem chord stand together.
            bool together = false;
            for (size_t a = 0; a < normals.size() && ! together; ++a)
                for (size_t b = a + 1; b < normals.size() && ! together; ++b)
                {
                    const auto& A = items[static_cast<size_t> (normals[a])];
                    const auto& B = items[static_cast<size_t> (normals[b])];
                    if (std::abs (A.x - B.x) < 0.7 * sp) together = true;
                }

            std::vector<std::vector<int>> voices;
            auto measureRestFix = [&] (std::vector<int>& v) {
                if (v.size() == 1)
                {
                    auto& it = items[static_cast<size_t> (v.front())];
                    if (it.rest && (it.type == 0 || (it.type == -1 && length >= Fraction (8))) && it.dots == 0)
                    {
                        it.measureRest = true;
                        it.duration = length;
                    }
                }
            };
            if (! together)
            {
                voices.push_back (normals);
                measureRestFix (voices[0]);
            }
            else
            {
                std::vector<int> up, down;
                const double mid = staff.middle();
                for (int ii : normals)
                {
                    const auto& it = items[static_cast<size_t> (ii)];
                    if (it.rest)
                    {
                        const double cy = (it.y0 + it.y1) * 0.5;
                        if (cy < mid - 0.5 * sp) up.push_back (ii);
                        else if (cy > mid + 0.5 * sp) down.push_back (ii);
                        else
                        {
                            bool upHere = false;
                            for (int o : up) upHere = upHere || std::abs (items[static_cast<size_t> (o)].x - it.x) < 0.7 * sp;
                            (upHere ? down : up).push_back (ii);
                        }
                    }
                    else if (it.dir == -1) down.push_back (ii);
                    else if (it.dir == 1) up.push_back (ii);
                    else ((it.y0 + it.y1) * 0.5 < mid ? up : down).push_back (ii);
                }
                auto byX = [&] (int a, int b) { return items[static_cast<size_t> (a)].x < items[static_cast<size_t> (b)].x; };
                std::sort (up.begin(), up.end(), byX);
                std::sort (down.begin(), down.end(), byX);
                measureRestFix (up);
                measureRestFix (down);
                voices.push_back (up);
                if (! down.empty()) voices.push_back (down);
            }

            // grace notes go with the voice of the note after them
            for (int ii : list)
            {
                auto& it = items[static_cast<size_t> (ii)];
                if (! it.grace) continue;
                int target = 0;
                double bestX = 1e9;
                for (size_t v = 0; v < voices.size(); ++v)
                    for (int o : voices[v])
                    {
                        const double x = items[static_cast<size_t> (o)].x;
                        if (x > it.x && x < bestX) { bestX = x; target = static_cast<int> (v); }
                    }
                voices[static_cast<size_t> (target)].push_back (ii);
                std::sort (voices[static_cast<size_t> (target)].begin(), voices[static_cast<size_t> (target)].end(),
                           [&] (int a, int b) { return items[static_cast<size_t> (a)].x < items[static_cast<size_t> (b)].x; });
            }

            // Onsets: each voice in turn; a second voice lines up with the first by position.
            std::vector<std::pair<double, Fraction>> columns;
            Fraction longest;
            for (size_t v = 0; v < voices.size(); ++v)
            {
                Fraction t;
                for (int ii : voices[v])
                {
                    auto& it = items[static_cast<size_t> (ii)];
                    if (v > 0 && ! it.grace)
                        for (const auto& c : columns)
                            if (std::abs (c.first - it.x) < 0.6 * sp && c.second > t) { t = c.second; break; }
                    it.offset = t;
                    it.voice = static_cast<int> (v) + 1;
                    if (! it.grace)
                    {
                        if (v == 0) columns.push_back ({ it.x, t });
                        t += it.duration;
                    }
                }
                if (t > longest) longest = t;
                if (t > length)
                    bar.problems.push_back ({ Issue::Error, "Voice " + std::to_string (v + 1) + " adds up to " + t.str() + " beats, more than the "
                                                             + std::to_string (time.beats) + "/" + std::to_string (time.beatType) + " bar holds." });
            }
            bar.content = longest;

            // Events, voice by voice.
            for (size_t v = 0; v < voices.size(); ++v)
                for (int ii : voices[v])
                {
                    auto& it = items[static_cast<size_t> (ii)];
                    Event e;
                    e.rest = it.rest;
                    e.measureRest = it.measureRest;
                    e.grace = it.grace;
                    e.graceSlash = it.grace && it.slash;
                    e.type = it.type;
                    e.dots = it.dots;
                    e.tuplet = it.tuplet;
                    e.duration = it.duration;
                    e.offset = it.offset;
                    e.voice = it.voice;
                    e.stem = it.dir;
                    e.notes = it.notes;
                    e.x = it.x;
                    for (const auto& a : it.articulations) e.articulations.push_back (a);
                    for (const auto& s : it.slurs) e.articulations.push_back (std::string ("slur:") + (s.second ? "start:" : "stop:") + std::to_string (s.first));
                    bar.events.push_back (e);
                }

            // Beams, level by level, from which beam strokes neighbours share.
            for (size_t v = 0; v < voices.size(); ++v)
            {
                std::vector<int> beamed;
                for (int ii : voices[v])
                    if (items[static_cast<size_t> (ii)].stem >= 0 && ! stems[static_cast<size_t> (items[static_cast<size_t> (ii)].stem)].levels.empty())
                        beamed.push_back (ii);
                auto shares = [&] (int a, int b, int level) {
                    const auto& la = stems[static_cast<size_t> (items[static_cast<size_t> (a)].stem)].levels;
                    const auto& lb = stems[static_cast<size_t> (items[static_cast<size_t> (b)].stem)].levels;
                    if (static_cast<size_t> (level) >= la.size()) return false;
                    for (int s : la[static_cast<size_t> (level)])
                        for (const auto& l : lb)
                            for (int t : l) if (s == t) return true;
                    return false;
                };
                for (size_t i = 0; i < beamed.size(); ++i)
                {
                    const int a = beamed[i];
                    const auto& lv = stems[static_cast<size_t> (items[static_cast<size_t> (a)].stem)].levels;
                    std::vector<int> marks;
                    for (size_t level = 0; level < lv.size(); ++level)
                    {
                        const bool left = i > 0 && shares (a, beamed[i - 1], static_cast<int> (level));
                        const bool right = i + 1 < beamed.size() && shares (a, beamed[i + 1], static_cast<int> (level));
                        int mark;
                        if (left && right) mark = 2;
                        else if (right) mark = 1;
                        else if (left) mark = 3;
                        else
                        {
                            const auto& seg = page.segments[static_cast<size_t> (lv[level].front())];
                            mark = (seg.a.x + seg.b.x) * 0.5 > stems[static_cast<size_t> (items[static_cast<size_t> (a)].stem)].x ? 4 : 5;
                        }
                        marks.push_back (mark);
                    }
                    for (auto& e : bar.events)
                        if (std::abs (e.x - items[static_cast<size_t> (a)].x) < 1e-6 && e.voice == items[static_cast<size_t> (a)].voice && ! e.rest)
                            e.beams = marks;
                }
            }

            // Clef changes: at the onset of the first note after them.
            for (size_t c = 0; c < bar.clefs.size(); ++c)
            {
                Fraction at = bar.content;
                for (int ii : list)
                    if (items[static_cast<size_t> (ii)].x > bar.clefX[c] && ! items[static_cast<size_t> (ii)].grace && items[static_cast<size_t> (ii)].offset < at)
                        at = items[static_cast<size_t> (ii)].offset;
                bar.clefs[c].offset = at;
            }
        }
        st.time = time;
    }

    // Dynamics: letters next to each other make one marking.
    for (int k = 0; k < nStaves; ++k)
    {
        const int si = sys.staves[static_cast<size_t> (k)];
        const auto& staff = layout.staves[static_cast<size_t> (si)];
        const double sp = staff.space;
        std::vector<int> dyn;
        for (size_t i = 0; i < page.glyphs.size(); ++i)
            if (glyphStaff[i] == si && ! dynamicLetters (page.glyphs[i].name).empty()) dyn.push_back (static_cast<int> (i));
        std::sort (dyn.begin(), dyn.end(), [&] (int a, int b) { return page.glyphs[static_cast<size_t> (a)].box.x0 < page.glyphs[static_cast<size_t> (b)].box.x0; });
        std::vector<bool> done (dyn.size(), false);
        for (size_t a = 0; a < dyn.size(); ++a)
        {
            if (done[a]) continue;
            Box box = page.glyphs[static_cast<size_t> (dyn[a])].box;
            std::string word = dynamicLetters (page.glyphs[static_cast<size_t> (dyn[a])].name);
            done[a] = true;
            for (size_t b = a + 1; b < dyn.size(); ++b)
            {
                const auto& g = page.glyphs[static_cast<size_t> (dyn[b])];
                if (done[b] || g.box.x0 - box.x1 > 0.5 * sp || std::abs (g.origin.y - page.glyphs[static_cast<size_t> (dyn[a])].origin.y) > 0.3 * sp) continue;
                word += dynamicLetters (g.name);
                box.add (g.box);
                done[b] = true;
            }
            const int bar = barOf (box.cx());
            if (bar < 0) continue;
            auto& sb = out.staves[static_cast<size_t> (k)][static_cast<size_t> (bar)];
            Direction d;
            d.staff = 1;
            d.below = box.cy() > staff.middle();
            d.dynamics = isKnownDynamic (word) ? word : "other:" + word;
            // at the onset of the nearest note or rest
            double bestD = 1e9;
            for (const auto& e : sb.events)
            {
                if (e.grace) continue;
                const double dd = std::abs (e.x - box.x0);
                if (dd < bestD) { bestD = dd; d.offset = e.offset; }
            }
            sb.directions.push_back (d);
        }
    }

    // Part names, from text left of the first system's staves.
    out.staffNames.assign (static_cast<size_t> (nStaves), std::string());
    for (int k = 0; k < nStaves; ++k)
    {
        const auto& staff = layout.staves[static_cast<size_t> (sys.staves[static_cast<size_t> (k)])];
        std::vector<const Text*> words;
        for (const auto& t : page.texts)
            if (t.box.x1 < staff.x0 - 0.3 * staff.space && t.box.cy() > staff.top() - 1.5 * staff.space && t.box.cy() < staff.bottom() + 1.5 * staff.space)
                words.push_back (&t);
        std::sort (words.begin(), words.end(), [] (const Text* a, const Text* b) { return a->box.x0 < b->box.x0; });
        std::string name;
        for (const auto* t : words) name += (name.empty() ? "" : " ") + t->text;
        out.staffNames[static_cast<size_t> (k)] = name;
    }
    return out;
}

// ===========================================================================

PageVerdict judgePage (const PageInk& page, const PageLayout& layout)
{
    PageVerdict v;
    double pictureArea = 0;
    for (const auto& p : page.pictures) pictureArea += std::max (0.0, p.box.width()) * std::max (0.0, p.box.height());
    const double pageArea = std::max (1.0, page.width * page.height);
    int musicGlyphs = 0;
    for (const auto& g : page.glyphs)
        if (headKind (g.name) != HeadKind::None || restType (g.name) != 99 || isClefName (g.name)) ++musicGlyphs;

    if (! layout.staves.empty() && musicGlyphs > 0)
    {
        v.kind = PageVerdict::Music;
        return v;
    }
    if (pictureArea > 0.3 * pageArea && musicGlyphs == 0)
    {
        v.kind = PageVerdict::Scanned;
        v.reason = "This page is a picture (a scan or a photo), not computer-made notation, so it was not read.";
        return v;
    }
    if (! page.unknownMusicFonts.empty())
    {
        v.kind = PageVerdict::Unreadable;
        v.reason = "This page uses the music font \"" + page.unknownMusicFonts.front()
                 + "\", which places symbols on ordinary letters; it cannot be read without guessing, so it was not read.";
        return v;
    }
    if (! layout.staves.empty())
    {
        v.kind = PageVerdict::Unreadable;
        v.reason = "This page has staves but its symbols are drawn as shapes rather than set in a music font, so it was not read.";
        return v;
    }
    v.kind = PageVerdict::NoMusic;
    v.reason = "No music on this page.";
    return v;
}

std::string partNameFrom (const std::string& s)
{
    std::string out = s;
    while (! out.empty() && out.back() == ' ') out.pop_back();
    return out;
}
} // namespace

ReadResult readScore (const std::vector<PageInk>& pages)
{
    ReadResult result;
    std::vector<StaffState> state;
    std::vector<int> staffBase;                 // first state index of each part
    std::vector<int> stavesOfPart;
    bool structureKnown = false;
    int globalBar = 0;

    struct PartBars { std::vector<Measure> measures; };
    std::vector<Part>& parts = result.score.parts;

    for (const auto& page : pages)
    {
        const auto layout = findLayout (page);
        auto verdict = judgePage (page, layout);
        result.pages.push_back (verdict);
        if (verdict.kind != PageVerdict::Music)
        {
            if (verdict.kind != PageVerdict::NoMusic)
                result.issues.push_back ({ Issue::Error, page.number, { 0, 0, page.width, page.height }, -1, -1, verdict.reason });
            continue;
        }

        // Title and composer: the first page's text above its first staff.
        if (result.score.title.empty() && ! layout.staves.empty())
        {
            const double top = layout.staves.front().top() - 2 * layout.staves.front().space;
            const Text* biggest = nullptr;
            for (const auto& t : page.texts)
                if (t.box.y1 < top && (biggest == nullptr || t.size > biggest->size + 0.1)) biggest = &t;
            if (biggest != nullptr && biggest->size > 12.0)
            {
                std::vector<const Text*> line;
                for (const auto& t : page.texts)
                    if (t.box.y1 < top && std::abs (t.size - biggest->size) < 0.2 && std::abs (t.box.cy() - biggest->box.cy()) < biggest->size * 0.6)
                        line.push_back (&t);
                std::sort (line.begin(), line.end(), [] (const Text* a, const Text* b) { return a->box.x0 < b->box.x0; });
                for (const auto* t : line) result.score.title += (result.score.title.empty() ? "" : " ") + t->text;
                std::vector<const Text*> right;
                for (const auto& t : page.texts)
                    if (t.box.y1 < top && t.box.x1 > page.width * 0.6 && t.box.y0 > biggest->box.y1 && t.size < biggest->size)
                        right.push_back (&t);
                if (! right.empty())
                {
                    const double y = right.front()->box.cy();
                    std::sort (right.begin(), right.end(), [] (const Text* a, const Text* b) { return a->box.x0 < b->box.x0; });
                    for (const auto* t : right)
                        if (std::abs (t->box.cy() - y) < t->size * 0.6)
                            result.score.composer += (result.score.composer.empty() ? "" : " ") + t->text;
                }
            }
        }

        PageReader reader (page, layout);
        reader.prepare();

        for (size_t s = 0; s < layout.systems.size(); ++s)
        {
            const auto& sys = layout.systems[s];
            if (! structureKnown)
            {
                stavesOfPart.assign (static_cast<size_t> (sys.parts), 0);
                for (int p : sys.partOfStaff) ++stavesOfPart[static_cast<size_t> (p)];
                int base = 0;
                for (int n : stavesOfPart) { staffBase.push_back (base); base += n; }
                state.assign (static_cast<size_t> (base), StaffState {});
                parts.assign (static_cast<size_t> (sys.parts), Part {});
                for (size_t p = 0; p < parts.size(); ++p) parts[p].staves = stavesOfPart[p];
                structureKnown = true;
            }

            // Which part and staff each staff of this system is.
            std::vector<int> stateIndex, partIndex, staffInPart;
            const bool sameShape = sys.parts == static_cast<int> (parts.size()) && [&] {
                std::vector<int> count (static_cast<size_t> (sys.parts), 0);
                for (int p : sys.partOfStaff) ++count[static_cast<size_t> (p)];
                return count == stavesOfPart;
            }();
            {
                std::vector<int> seen (parts.size() + 8, 0);
                for (size_t k = 0; k < sys.staves.size(); ++k)
                {
                    int p = sameShape ? sys.partOfStaff[k] : std::min (static_cast<int> (k), static_cast<int> (parts.size()) - 1);
                    int inPart = sameShape ? seen[static_cast<size_t> (p)]++ : 0;
                    if (inPart >= stavesOfPart[static_cast<size_t> (p)]) inPart = stavesOfPart[static_cast<size_t> (p)] - 1;
                    partIndex.push_back (p);
                    staffInPart.push_back (inPart);
                    stateIndex.push_back (staffBase[static_cast<size_t> (p)] + inPart);
                }
            }
            if (! sameShape)
                result.issues.push_back ({ Issue::Error, page.number, { sys.x0, layout.staves[static_cast<size_t> (sys.staves.front())].top(), sys.x1,
                                            layout.staves[static_cast<size_t> (sys.staves.back())].bottom() }, globalBar, -1,
                                          "This system has a different set of staves from the first one (hidden empty staves?); its staves were matched to parts in order, which may be wrong." });

            auto read = reader.readSystem (static_cast<int> (s), state, stateIndex);

            if (globalBar == 0)
                for (size_t k = 0; k < sys.staves.size(); ++k)
                {
                    auto& part = parts[static_cast<size_t> (partIndex[k])];
                    const auto name = partNameFrom (read.staffNames[k]);
                    if (part.name.empty() && ! name.empty()) part.name = name;
                }

            for (int b = 0; b < read.bars; ++b)
            {
                for (size_t p = 0; p < parts.size(); ++p)
                {
                    Measure m;
                    m.number = std::to_string (globalBar + 1);
                    m.left = read.leftBar[static_cast<size_t> (b)];
                    m.hasLeft = read.hasLeft[static_cast<size_t> (b)];
                    m.right = read.rightBar[static_cast<size_t> (b)];
                    parts[p].measures.push_back (m);
                }
                for (size_t k = 0; k < sys.staves.size(); ++k)
                {
                    auto& sb = read.staves[k][static_cast<size_t> (b)];
                    const int p = partIndex[k];
                    const int staffNo = staffInPart[k] + 1;
                    auto& m = parts[static_cast<size_t> (p)].measures.back();
                    if (sb.key && staffNo == 1) m.key = sb.key;
                    if (sb.time && staffNo == 1) m.time = sb.time;
                    if (sb.multiRest > 0) m.multiRest = std::max (m.multiRest, sb.multiRest);
                    for (auto& e : sb.events)
                    {
                        e.staff = staffNo;
                        m.events.push_back (e);
                    }
                    for (auto& d : sb.directions) { d.staff = staffNo; m.directions.push_back (d); }
                    for (auto& c : sb.clefs) { c.staff = staffNo; m.clefs.push_back (c); }
                    m.places.push_back ({ sb.page, sb.box });
                    for (const auto& pr : sb.problems)
                        result.issues.push_back ({ pr.first, sb.page, sb.box, globalBar, p, pr.second });
                }
                ++globalBar;
            }

            // The clef each staff starts the system with.
            if (read.bars > 0)
                for (size_t k = 0; k < sys.staves.size(); ++k)
                {
                    auto& m = parts[static_cast<size_t> (partIndex[k])].measures[static_cast<size_t> (globalBar - read.bars)];
                    ClefChange c;
                    c.offset = Fraction (0);
                    c.staff = staffInPart[k] + 1;
                    c.clef = read.openingClef[k];
                    m.clefs.insert (m.clefs.begin(), c);
                }
        }
    }

    // Ties across systems, timings and the bar check, once all bars are in.
    for (size_t p = 0; p < parts.size(); ++p)
    {
        auto& part = parts[p];
        int key = 0;
        TimeSignature time;
        bool timeSet = false;
        std::vector<Clef> clef (static_cast<size_t> (part.staves));
        std::vector<bool> clefSet (static_cast<size_t> (part.staves), false);
        for (size_t b = 0; b < part.measures.size(); ++b)
        {
            auto& m = part.measures[b];
            // Only changes are kept: a clef or key restated at a new system is not news.
            if (m.key)
            {
                if (b > 0 && *m.key == key) m.key.reset();
                else key = *m.key;
            }
            if (m.time)
            {
                if (timeSet && *m.time == time) m.time.reset();
                else { time = *m.time; timeSet = true; }
            }
            std::vector<ClefChange> kept;
            for (const auto& c : m.clefs)
            {
                const auto s = static_cast<size_t> (std::max (0, c.staff - 1));
                if (s >= clef.size()) continue;
                if (clefSet[s] && clef[s] == c.clef) continue;
                clef[s] = c.clef;
                clefSet[s] = true;
                kept.push_back (c);
            }
            m.clefs = kept;
        }
        if (! timeSet && ! part.measures.empty())
        {
            part.measures.front().time = TimeSignature {};
            result.issues.push_back ({ Issue::Warning, -1, {}, 0, static_cast<int> (p), "No time signature was found; 4/4 was assumed." });
        }
    }

    // The bar check, and the pickup.
    if (! parts.empty())
    {
        const size_t bars = parts.front().measures.size();
        std::vector<TimeSignature> timeOf (bars);
        {
            TimeSignature t;
            for (size_t b = 0; b < bars; ++b)
            {
                if (parts.front().measures[b].time) t = *parts.front().measures[b].time;
                timeOf[b] = t;
            }
        }
        bool pickup = false;
        for (size_t b = 0; b < bars; ++b)
        {
            const Fraction length = timeOf[b].length();
            Fraction most;
            bool any = false, mismatch = false;
            for (const auto& part : parts)
            {
                if (b >= part.measures.size()) continue;
                const auto& m = part.measures[b];
                std::map<std::pair<int, int>, Fraction> voiceEnd;
                for (const auto& e : m.events)
                {
                    if (e.grace) continue;
                    auto& end = voiceEnd[{ e.staff, e.voice }];
                    if (e.offset + e.duration > end) end = e.offset + e.duration;
                }
                for (const auto& [k, end] : voiceEnd)
                {
                    any = true;
                    if (most != Fraction (0) && end != most && k.second == 1) mismatch = true;
                    if (end > most) most = end;
                }
            }
            if (! any) continue;
            const bool first = b == 0, last = b + 1 == bars;
            if (first && most < length && ! mismatch)
            {
                pickup = true;
                for (auto& part : parts)
                    if (! part.measures.empty()) { part.measures[0].implicit = true; part.measures[0].number = "0"; }
                continue;
            }
            // A bar split in two around a repeat (3 beats, then 1) is how it
            // was written, when every part agrees and the halves make a bar.
            auto lengthOf = [&] (size_t bar) {
                Fraction most;
                for (const auto& part : parts)
                {
                    if (bar >= part.measures.size()) continue;
                    for (const auto& e : part.measures[bar].events)
                        if (! e.grace && e.voice == 1 && e.offset + e.duration > most) most = e.offset + e.duration;
                }
                return most;
            };
            if (! mismatch && most < length && most > Fraction (0))
            {
                const Fraction before = b > 0 ? lengthOf (b - 1) : Fraction (0);
                const Fraction after = b + 1 < bars ? lengthOf (b + 1) : Fraction (0);
                if ((b > 0 && before + most == length && timeOf[b - 1].length() == length && ! (b == 1 && pickup))
                    || (after + most == length && b + 1 < bars && timeOf[b + 1].length() == length))
                    continue;
            }
            for (size_t p = 0; p < parts.size(); ++p)
            {
                if (b >= parts[p].measures.size()) continue;
                const auto& m = parts[p].measures[b];
                if (m.multiRest > 0) continue;
                std::map<std::pair<int, int>, Fraction> voiceEnd;
                bool hasEvents = false;
                for (const auto& e : m.events)
                {
                    if (e.grace) continue;
                    hasEvents = true;
                    auto& end = voiceEnd[{ e.staff, e.voice }];
                    if (e.offset + e.duration > end) end = e.offset + e.duration;
                }
                if (! hasEvents)
                {
                    result.issues.push_back ({ Issue::Warning, m.places.empty() ? -1 : m.places.front().page, m.places.empty() ? Box {} : m.places.front().box,
                                              static_cast<int> (b), static_cast<int> (p), "This bar has no notes or rests in it." });
                    continue;
                }
                for (const auto& [k, end] : voiceEnd)
                {
                    if (k.second != 1 || end == length) continue;
                    if (last && pickup && end < length) continue;
                    const auto& place = static_cast<size_t> (k.first - 1) < m.places.size() ? m.places[static_cast<size_t> (k.first - 1)] : PlaceOnPage {};
                    result.issues.push_back ({ Issue::Error, place.page, place.box, static_cast<int> (b), static_cast<int> (p),
                                              "The notes add up to " + end.str() + " quarter notes but the time signature ("
                                              + std::to_string (timeOf[b].beats) + "/" + std::to_string (timeOf[b].beatType) + ") needs " + length.str() + "." });
                }
            }
        }
        // renumber after a pickup
        if (pickup)
            for (auto& part : parts)
                for (size_t b = 1; b < part.measures.size(); ++b) part.measures[b].number = std::to_string (b);
        result.barsRead = static_cast<int> (bars);
    }

    // De-duplicate issues and count bars with problems.
    std::set<int> barsWithErrors;
    for (const auto& i : result.issues)
        if (i.level != Issue::Info && i.measureIndex >= 0) barsWithErrors.insert (i.measureIndex);
    result.barsWithIssues = static_cast<int> (barsWithErrors.size());
    return result;
}

} // namespace ans
