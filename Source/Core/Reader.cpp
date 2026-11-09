#include "Reader.h"

#include "Layout.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
    int tremoloWith = -1;                // the other stem of a two-note tremolo
    int tremoloStrokes = 0;
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
    int tremolo = 0;                    // strokes of a two-note tremolo
    int tremoloType = 0;                // 2 its first note, 3 its second (Event's)
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

// A volta bracket over bars of one system.
struct VoltaRead
{
    int first = 0, last = 0;       // bars of the system
    std::string number, text;      // empty when it continues one from the system before
    bool closed = false;           // a hook down at its right end
    bool toEnd = false;            // runs on to the system's end, open
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
    std::vector<VoltaRead> voltas;
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
    void findTremolos();
    void buildItems();
    void findDots();
    void findTies();
    bool nextTo (int a, int b) const;
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
    findTremolos();
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
        // The same head drawn twice (a unison shared by two voices) is one head.
        bool twice = false;
        for (const auto& o : heads)
            twice = twice || (page.glyphs[static_cast<size_t> (o.glyph)].name == g.name
                              && std::abs (page.glyphs[static_cast<size_t> (o.glyph)].origin.x - g.origin.x) < 0.1
                              && std::abs (page.glyphs[static_cast<size_t> (o.glyph)].origin.y - g.origin.y) < 0.1);
        if (twice) { glyphUsed[i] = true; continue; }
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

// A two-note tremolo: beam strokes standing between two stems and touching
// neither. The two notes share the time of one: each sounds half its value.
void PageReader::findTremolos()
{
    std::set<int> used;
    for (const auto& st : stems)
        for (const auto& level : st.levels)
            for (int s : level) used.insert (s);
    std::map<std::pair<int, int>, std::vector<double>> strokes;   // (left stem, right stem) -> stroke heights
    for (size_t i = 0; i < page.segments.size(); ++i)
    {
        if (used.count (static_cast<int> (i))) continue;
        const auto& s = page.segments[i];
        const double sp = spaceNear (s.a.x, s.a.y);
        if (s.thickness < 0.22 * sp || s.thickness > 0.85 * sp || s.length() < 0.8 * sp || s.dashed) continue;
        if (std::abs (s.dy()) > 0.6 * std::abs (s.dx())) continue;
        int left = -1, right = -1;
        bool touches = false;
        for (size_t k = 0; k < stems.size() && ! touches; ++k)
        {
            const auto& st = stems[k];
            const double y = s.yAt (st.x);
            const bool alongside = y > st.y0 - sp && y < st.y1 + sp;
            if (! alongside) continue;
            if (st.x > s.minX() - 0.3 * sp && st.x < s.maxX() + 0.3 * sp) { touches = true; continue; }
            if (st.x < s.minX() && s.minX() - st.x < 3.5 * sp && (left < 0 || st.x > stems[static_cast<size_t> (left)].x)) left = static_cast<int> (k);
            if (st.x > s.maxX() && st.x - s.maxX() < 3.5 * sp && (right < 0 || st.x < stems[static_cast<size_t> (right)].x)) right = static_cast<int> (k);
        }
        if (touches || left < 0 || right < 0 || stems[static_cast<size_t> (left)].staff != stems[static_cast<size_t> (right)].staff) continue;
        // centred between them, more or less
        const double gapL = s.minX() - stems[static_cast<size_t> (left)].x, gapR = stems[static_cast<size_t> (right)].x - s.maxX();
        if (std::abs (gapL - gapR) > 1.0 * sp) continue;
        auto& ys = strokes[{ left, right }];
        const double y = (s.a.y + s.b.y) * 0.5;
        if (std::none_of (ys.begin(), ys.end(), [&] (double o) { return std::abs (o - y) < 0.3 * sp; })) ys.push_back (y);
    }
    for (const auto& [pair, ys] : strokes)
    {
        auto& a = stems[static_cast<size_t> (pair.first)];
        auto& b = stems[static_cast<size_t> (pair.second)];
        if (a.tremoloWith >= 0 || b.tremoloWith >= 0 || ! a.levels.empty() || ! b.levels.empty() || ys.size() > 4) continue;
        a.tremoloWith = pair.second; b.tremoloWith = pair.first;
        a.tremoloStrokes = b.tremoloStrokes = static_cast<int> (ys.size());
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

    // Two-note tremolos: each note sounds half its value (MusicXML's 2:1).
    for (auto& it : items)
    {
        if (it.rest || it.stem < 0) continue;
        const auto& st = stems[static_cast<size_t> (it.stem)];
        if (st.tremoloWith < 0 || it.tuplet.active()) continue;
        it.tuplet.actual = 2;
        it.tuplet.normal = 1;
        it.tremolo = st.tremoloStrokes;
        it.tremoloType = stems[static_cast<size_t> (st.tremoloWith)].x > st.x ? 2 : 3;
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

// Whether two heads are of neighbouring notes: nothing of the same voice
// (same stem direction, or a rest) stands between them.
bool PageReader::nextTo (int a, int b) const
{
    const auto& A = heads[static_cast<size_t> (a)];
    const auto& B = heads[static_cast<size_t> (b)];
    const int ia = headItem[static_cast<size_t> (a)], ib = headItem[static_cast<size_t> (b)];
    const int dir = ia >= 0 ? items[static_cast<size_t> (ia)].dir : 0;
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (static_cast<int> (i) == ia || static_cast<int> (i) == ib) continue;
        const auto& it = items[i];
        if (it.staff != A.staff || it.small) continue;
        if (it.x <= A.x1 || it.x >= B.x0) continue;
        if (it.rest || it.dir == 0 || dir == 0 || it.dir == dir) return false;
    }
    return true;
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
        // Heads near either end, on whichever staff (a tie above a staff
        // can be nearer the staff above).
        for (size_t h = 0; h < heads.size(); ++h)
        {
            const auto& hd = heads[h];
            const auto& hs = layout.staves[static_cast<size_t> (hd.staff)];
            if (hs.system != staff.system) continue;
            const double hsp = hs.space;
            if (std::abs (hd.y - cv.left.y) <= 1.3 * hsp && cv.left.x >= hd.x0 - 0.2 * hsp && cv.left.x <= hd.x1 + 1.3 * hsp)
                starts.push_back (static_cast<int> (h));
            if (std::abs (hd.y - cv.right.y) <= 1.3 * hsp && cv.right.x >= hd.x0 - 1.3 * hsp && cv.right.x <= hd.x1 + 0.2 * hsp)
                ends.push_back (static_cast<int> (h));
        }
        for (int a : starts)
            for (int b : ends)
            {
                const auto& A = heads[static_cast<size_t> (a)];
                const auto& B = heads[static_cast<size_t> (b)];
                if (A.staff != B.staff || A.line != B.line || B.x0 <= A.x1 - 0.1 * sp || A.small != B.small) continue;
                if (startUsed[static_cast<size_t> (a)] || endUsed[static_cast<size_t> (b)]) continue;
                if (! nextTo (a, b)) continue;
                const double cost = std::abs (A.y - cv.left.y) + std::abs (B.y - cv.right.y)
                                  + std::abs (cv.left.x - A.x1) * 0.3 + std::abs (cv.right.x - B.x0) * 0.3;
                if (cost < bestCost) { bestCost = cost; bestA = a; bestB = b; }
            }
        if (std::getenv ("ANS_DEBUG_TIES") != nullptr)
        {
            std::fprintf (stderr, "curve %zu (%.1f,%.1f)-(%.1f,%.1f) starts:", c, cv.left.x, cv.left.y, cv.right.x, cv.right.y);
            for (int a : starts) std::fprintf (stderr, " %d(x%.1f l%d)", a, heads[static_cast<size_t> (a)].x0, heads[static_cast<size_t> (a)].line);
            std::fprintf (stderr, " ends:");
            for (int b : ends) std::fprintf (stderr, " %d(x%.1f l%d)", b, heads[static_cast<size_t> (b)].x0, heads[static_cast<size_t> (b)].line);
            std::fprintf (stderr, " -> %d %d\n", bestA, bestB);
        }
        if (bestA >= 0 && std::getenv ("ANS_DEBUG_TIES") != nullptr)
        {
            const auto& A = heads[static_cast<size_t> (bestA)]; const auto& B = heads[static_cast<size_t> (bestB)];
            std::fprintf (stderr, "TIE p%d L dx %.2f dy %.2f  R dx %.2f dy %.2f  h %.2f w %.2f\n", page.number + 1,
                          (cv.left.x - A.x1) / sp, (cv.left.y - A.y) / sp, (cv.right.x - B.x0) / sp, (cv.right.y - B.y) / sp,
                          cv.box.height() / sp, cv.box.width() / sp);
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
        if (cv.right.x >= sys.x1 - 2.5 * sp)
        {
            for (int a : starts)
                if (! startUsed[static_cast<size_t> (a)] && heads[static_cast<size_t> (a)].x1 < cv.right.x)
                {
                    if (heads[static_cast<size_t> (a)].small) continue;
                    bool beyond = false;   // its note is its voice's last in this system
                    const int ia = headItem[static_cast<size_t> (a)];
                    const int dir = ia >= 0 ? items[static_cast<size_t> (ia)].dir : 0;
                    for (const auto& it : items)
                        beyond = beyond || (it.staff == heads[static_cast<size_t> (a)].staff && ! it.small && it.x > heads[static_cast<size_t> (a)].x1 - 0.1 * sp
                                            && (it.rest || it.dir == 0 || dir == 0 || it.dir == dir));
                    if (beyond) continue;
                    ties.push_back ({ a, -1, true, false });
                    startUsed[static_cast<size_t> (a)] = true;
                    used.insert (static_cast<int> (c));
                    break;
                }
            if (used.count (static_cast<int> (c))) continue;
        }
        {
            for (int b : ends)
            {
                if (endUsed[static_cast<size_t> (b)]) continue;
                bool earlier = false;   // the first note on its line in this system
                for (size_t h = 0; h < heads.size(); ++h)
                    earlier = earlier || (heads[h].staff == heads[static_cast<size_t> (b)].staff && heads[h].x1 < cv.left.x + 0.1 * sp && heads[h].line == heads[static_cast<size_t> (b)].line);
                if (earlier) continue;
                if (heads[static_cast<size_t> (b)].small) continue;
                bool firstThing = true; // the curve starts before every note of its voice
                const int ib = headItem[static_cast<size_t> (b)];
                const int dirB = ib >= 0 ? items[static_cast<size_t> (ib)].dir : 0;
                for (const auto& it : items)
                    firstThing = firstThing && ! (it.staff == heads[static_cast<size_t> (b)].staff && it.x <= cv.left.x && ! it.small
                                                  && (it.rest || it.dir == 0 || dirB == 0 || it.dir == dirB));
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

// A volta's label: "1.", "2.", "1., 2.", "1.-3.", "1. und 3. Strophe" -> its
// numbers ("1, 2, 3"), or "" when the text is not one. Without a full stop
// ("1") it must be upright: an italic digit by a bracket is a tuplet's.
std::string voltaNumbers (const std::string& text, bool italic = false)
{
    if (text.size() > 40) return {};
    std::vector<int> numbers;
    bool range = false, stop = false;
    size_t i = 0;
    while (i < text.size())
    {
        const char c = text[i];
        if (std::isdigit (static_cast<unsigned char> (c)))
        {
            int n = 0;
            while (i < text.size() && std::isdigit (static_cast<unsigned char> (text[i]))) n = n * 10 + (text[i++] - '0');
            if (n < 1 || n > 9) return {};
            if (range && ! numbers.empty())
                for (int k = numbers.back() + 1; k < n; ++k) numbers.push_back (k);
            range = false;
            numbers.push_back (n);
            continue;
        }
        if (c == '.') stop = true;
        else if (c == '-') range = true;
        else if (static_cast<unsigned char> (c) == 0xE2 && i + 2 < text.size()) { range = true; i += 3; continue; }   // an en dash
        else if (std::isalpha (static_cast<unsigned char> (c))) range = false;   // "und", "Strophe", "time"
        else if (c != ',' && c != ' ' && c != '&' && c != '+') return {};
        ++i;
    }
    if (numbers.empty() || (! stop && italic)) return {};
    std::string out;
    for (size_t k = 0; k < numbers.size(); ++k)
    {
        if (k > 0 && numbers[k] <= numbers[k - 1]) return {};
        out += (k ? ", " : "") + std::to_string (numbers[k]);
    }
    return out;
}

// Volta brackets above the staves of a system: a thin line with a hook down
// at its left end and its number under it; the hook at the right end is
// there when the ending closes. A line with neither number nor left hook at
// the start of a system carries on the bracket from the system before.
std::vector<VoltaRead> findVoltas (const PageInk& page, const PageLayout& layout, const System& sys, const std::vector<BarRange>& ranges)
{
    std::vector<VoltaRead> out;
    if (ranges.empty()) return out;
    auto barOf = [&] (double x) {
        for (size_t r = 0; r < ranges.size(); ++r)
            if (x >= ranges[r].x0 && x < ranges[r].x1) return static_cast<int> (r);
        return -1;
    };
    double above = -1e9;   // the bottom of the staff above
    for (int si : sys.staves)
    {
        const auto& st = layout.staves[static_cast<size_t> (si)];
        const double sp = st.space;
        const double yTop = std::max (above + sp, st.top() - 14 * sp), yBottom = st.top() - 0.8 * sp;
        above = st.bottom();
        auto hookAt = [&] (double x, double y) {
            for (const auto& v : page.segments)
                if (v.vertical() && v.thickness < 0.35 * sp && std::abs (v.a.x - x) < 0.35 * sp && std::abs (v.minY() - y) < 0.35 * sp
                    && v.maxY() - y > 0.8 * sp && v.maxY() - y < 5 * sp)
                    return true;
            return false;
        };
        auto insideStaff = [&] (double y) {
            for (const auto& o : layout.staves)
                if (y > o.top() - 0.3 * o.space && y < o.bottom() + 0.3 * o.space) return true;
            return false;
        };
        auto throughHead = [&] (const Segment& h) {
            for (const auto& g : page.glyphs)
                if (headKind (g.name) != HeadKind::None && g.box.x1 > h.minX() && g.box.x0 < h.maxX()
                    && g.box.y0 - 0.5 * sp < h.a.y && g.box.y1 + 0.5 * sp > h.a.y)
                    return true;
            return false;
        };
        for (const auto& h : page.segments)
        {
            if (! h.horizontal (0.005) || h.dashed || h.thickness > 0.35 * sp || h.length() < 1.5 * sp) continue;
            const double y = (h.a.y + h.b.y) * 0.5;
            if (y < yTop || y > yBottom || h.minX() < sys.x0 - sp || h.maxX() > sys.x1 + sp || insideStaff (y)) continue;
            const bool leftHook = hookAt (h.minX(), y), rightHook = hookAt (h.maxX(), y);
            // the label: the words under the line from its left end, joined
            const Text* label = nullptr;
            for (const auto& t : page.texts)
                if (t.box.x0 > h.minX() - 0.5 * sp && t.box.x0 < h.minX() + 3 * sp && t.box.y0 > y - 0.6 * sp && t.box.cy() > y && t.box.y1 < y + 5 * sp
                    && (label == nullptr || t.box.x0 < label->box.x0))
                    label = &t;
            std::string labelText;
            bool labelItalic = false;
            if (label != nullptr)
            {
                labelText = label->text;
                labelItalic = label->italic;
                for (const Text* last = label;;)
                {
                    const Text* next = nullptr;
                    for (const auto& t : page.texts)
                        if (&t != last && t.box.x0 >= last->box.x1 - 0.1 && t.box.x0 - last->box.x1 < 2 * sp && t.box.x1 < h.maxX()
                            && std::abs (t.box.y1 - last->box.y1) < 0.4 * sp && (next == nullptr || t.box.x0 < next->box.x0))
                            next = &t;
                    if (next == nullptr) break;
                    labelText += " " + next->text;
                    last = next;
                }
                if (voltaNumbers (labelText, labelItalic).empty()) label = nullptr;
            }
            VoltaRead v;
            v.first = barOf (h.minX() + 0.5 * sp);
            v.last = barOf (h.maxX() - 0.5 * sp);
            if (v.first < 0 || v.last < v.first) continue;
            v.closed = rightHook;
            v.toEnd = ! rightHook && h.maxX() > ranges.back().x1 - 1.5 * sp;
            if (label != nullptr && leftHook)
            {
                v.number = voltaNumbers (labelText, labelItalic);
                v.text = labelText;
            }
            else if (label != nullptr || leftHook || v.first != 0 || h.length() < 3 * sp || h.minX() < sys.x0 + 2 * sp || throughHead (h) || insideStaff (y))
                continue;   // not a bracket carried on: a ledger line, a stray line
            bool seen = false;
            for (const auto& o : out) seen = seen || (o.first == v.first && o.last == v.last) || (v.number.empty() && o.number.empty());
            if (std::getenv ("ANS_DEBUG_VOLTAS") != nullptr)
                std::fprintf (stderr, "volta p%d y%.1f x%.1f-%.1f bars %d-%d of %zu (end %.1f) \"%s\" closed %d toEnd %d hookL %d\n", page.number, y, h.minX(), h.maxX(), v.first, v.last, ranges.size(), ranges.back().x1, v.number.c_str(), v.closed, v.toEnd, leftHook);
            if (! seen) out.push_back (v);
        }
    }
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
            if (! music && kept.empty() && r + 1 < ranges.size())
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

    out.voltas = findVoltas (page, layout, sys, ranges);

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
                    {
                        if (heads[static_cast<size_t> (h)].line != a.line) continue;
                        bool taken = false;   // two heads on one line (B-flat and B) take one each
                        for (const auto& o : accs) taken = taken || (o.attached && o.head == h);
                        if (! taken) { match = h; break; }
                    }
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
                        const int alterWithoutTie = alter;
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
                        // Only the tie says this note keeps its accidental. A slur
                        // between the same two notes looks the same, and would
                        // make it a different note: say so.
                        if (tiedIn && alter != alterWithoutTie)
                            bar.problems.push_back ({ Issue::Warning, "A tie carries an accidental into this bar. If the curve is a slur rather than a tie, the note is a semitone different; check it." });
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
        for (const auto& ca : clefs) st.clef = ca.clef;

        // A curve between two different pitches on one line (F to F-flat) is a slur, not a tie.
        for (const auto& t : ties)
        {
            if (t.from < 0 || t.to < 0 || heads[static_cast<size_t> (t.from)].staff != si) continue;
            const int ia = headItem[static_cast<size_t> (t.from)], ib = headItem[static_cast<size_t> (t.to)];
            if (ia < 0 || ib < 0) continue;
            Note* na = nullptr;
            Note* nb = nullptr;
            for (auto& n : items[static_cast<size_t> (ia)].notes) if (n.line == heads[static_cast<size_t> (t.from)].line) na = &n;
            for (auto& n : items[static_cast<size_t> (ib)].notes) if (n.line == heads[static_cast<size_t> (t.to)].line) nb = &n;
            if (na == nullptr || nb == nullptr || na->pitch.alter == nb->pitch.alter) continue;
            na->tieStart = false;
            nb->tieStop = false;
        }   // the next system goes on from the last clef

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

    // --- a time grid for the system: in engraved music, notes sounding
    // together stand in one column, whatever their staff. Staves holding a
    // single voice that fills its bar give each column's beat.
    std::vector<std::vector<std::pair<double, Fraction>>> grid (ranges.size());
    for (int k = 0; k < nStaves; ++k)
    {
        const int si = sys.staves[static_cast<size_t> (k)];
        const double sp = layout.staves[static_cast<size_t> (si)].space;
        TimeSignature time = startTime[static_cast<size_t> (k)];
        for (size_t r = 0; r < ranges.size(); ++r)
        {
            if (out.staves[static_cast<size_t> (k)][r].time) time = *out.staves[static_cast<size_t> (k)][r].time;
            std::vector<int> list;
            for (size_t i = 0; i < items.size(); ++i)
                if (items[i].staff == si && items[i].bar == static_cast<int> (r) && ! items[i].small) list.push_back (static_cast<int> (i));
            std::sort (list.begin(), list.end(), [&] (int a, int b) { return items[static_cast<size_t> (a)].x < items[static_cast<size_t> (b)].x; });
            bool together = false;
            for (size_t a = 0; a + 1 < list.size(); ++a)
                together = together || std::abs (items[static_cast<size_t> (list[a])].x - items[static_cast<size_t> (list[a + 1])].x) < 0.7 * sp;
            if (list.empty()) continue;
            if (together)
            {
                // two voices: the upper one (stems up, rests high) may still fill the bar
                const double mid = layout.staves[static_cast<size_t> (si)].middle();
                std::vector<int> upper;
                for (int ii : list)
                {
                    const auto& it = items[static_cast<size_t> (ii)];
                    if (it.rest ? (it.y0 + it.y1) * 0.5 < mid - 0.5 * sp : it.dir == 1) upper.push_back (ii);
                }
                list = upper;
                bool still = false;
                for (size_t a = 0; a + 1 < list.size(); ++a)
                    still = still || std::abs (items[static_cast<size_t> (list[a])].x - items[static_cast<size_t> (list[a + 1])].x) < 0.7 * sp;
                if (still || list.empty()) continue;
            }
            Fraction t;
            std::vector<std::pair<double, Fraction>> cols;
            for (int ii : list)
            {
                const auto& it = items[static_cast<size_t> (ii)];
                cols.push_back ({ (it.x + it.x1) * 0.5, t });
                t += itemDuration (it);
            }
            if (t != time.length()) continue;
            for (auto& c : cols) grid[r].push_back (c);
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
                // the H-bar's extent: its number stands centred just above the staff
                double hx0 = ranges[r].x0, hx1 = ranges[r].x1;
                for (const auto& sgm : page.segments)
                    if (sgm.horizontal (0.01) && sgm.thickness > 0.7 * sp && sgm.length() > 2 * sp && std::abs ((sgm.a.y + sgm.b.y) * 0.5 - staff.middle()) < 0.6 * sp
                        && sgm.minX() >= ranges[r].x0 && sgm.maxX() <= ranges[r].x1) { hx0 = sgm.minX(); hx1 = sgm.maxX(); }
                std::string number;
                std::vector<std::pair<double, int>> ds;
                for (size_t i = 0; i < page.glyphs.size(); ++i)
                {
                    const auto& g = page.glyphs[i];
                    const int d = timeDigit (g.name);
                    if (d < 0 || glyphStaff[i] != si || g.box.cx() < hx0 - sp || g.box.cx() > hx1 + sp) continue;
                    if (g.box.cy() > staff.top() || g.box.y1 < staff.top() - 4 * sp) continue;
                    ds.push_back ({ g.box.x0, d });
                    glyphUsed[i] = true;
                }
                std::sort (ds.begin(), ds.end());
                for (auto& d : ds) number += static_cast<char> ('0' + d.second);
                for (const auto& t : page.texts)
                    if (number.empty() && t.box.cx() >= ranges[r].x0 && t.box.cx() < ranges[r].x1 && t.box.cy() < staff.top() && t.box.cy() > staff.top() - 4 * sp
                        && ! t.text.empty() && std::all_of (t.text.begin(), t.text.end(), [] (char c) { return c >= '0' && c <= '9'; }))
                        number = t.text;
                bar.multiRest = number.empty() || number.size() > 3 ? 1 : std::max (1, std::stoi (number));
                if (number.empty()) bar.problems.push_back ({ Issue::Warning, "A multi-bar rest without a readable number was read as one bar." });
                Event e;
                e.rest = true; e.measureRest = true; e.type = 0; e.duration = length;
                bar.events.push_back (e);
                bar.content = length;
                bar.hasNotes = true;
                continue;
            }
            if (list.empty()) continue;
            bar.hasNotes = true;

            if (std::getenv ("ANS_DEBUG_ITEMS") != nullptr)
            {
                std::fprintf (stderr, "page %d staff %d bar %zu:\n", page.number + 1, si, r + 1);
                for (int ii : list)
                {
                    const auto& it = items[static_cast<size_t> (ii)];
                    std::fprintf (stderr, "  x%.1f-%.1f %s type%d dots%d dir%d small%d stem%d levels%d lines:",
                                  it.x, it.x1, it.rest ? "REST" : "note", it.type, it.dots, it.dir, it.small, it.stem,
                                  it.stem >= 0 ? static_cast<int> (stems[static_cast<size_t> (it.stem)].levels.size()) : -1);
                    for (int h : it.heads) std::fprintf (stderr, " %d", heads[static_cast<size_t> (h)].line);
                    std::fprintf (stderr, "\n");
                }
            }
            // grace notes: small, and followed by a full-size note
            for (int ii : list)
            {
                auto& it = items[static_cast<size_t> (ii)];
                it.grace = it.small && ! it.rest;
                it.duration = it.grace ? Fraction (0) : itemDuration (it);
            }



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
                    else
                    {
                        // no stem (a whole note): with the voice it stands above or
                        // below at the same moment, else by its place on the staff
                        const double cy = (it.y0 + it.y1) * 0.5;
                        int side = 0;
                        for (int o : normals)
                        {
                            const auto& other = items[static_cast<size_t> (o)];
                            if (o == ii || (! other.rest && other.dir == 0) || std::abs (other.x - it.x) >= 0.7 * sp) continue;
                            if (cy < other.y0) side = side == -1 ? 2 : 1;          // above what sounds with it
                            else if (cy > other.y1) side = side == 1 ? 2 : -1;     // below it
                        }
                        if (side == 1) up.push_back (ii);
                        else if (side == -1) down.push_back (ii);
                        else (cy < mid ? up : down).push_back (ii);
                    }
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

            // Triplets whose "3" was left off (engravers print it on the first
            // few only). Only when a voice overflows its bar, only for beamed
            // groups of three (or six) equal notes with no tuplet, and only if
            // that makes the voice fill the bar exactly.
            for (auto& v : voices)
            {
                Fraction sum;
                for (int ii : v) if (! items[static_cast<size_t> (ii)].grace) sum += items[static_cast<size_t> (ii)].duration;
                if (sum <= length) continue;
                auto sharesBeam = [&] (int a, int b) {
                    const auto& A = items[static_cast<size_t> (a)];
                    const auto& B = items[static_cast<size_t> (b)];
                    if (A.stem < 0 || B.stem < 0) return false;
                    const auto& la = stems[static_cast<size_t> (A.stem)].levels;
                    const auto& lb = stems[static_cast<size_t> (B.stem)].levels;
                    if (la.empty() || lb.empty()) return false;
                    for (int x : la.front()) for (int y : lb.front()) if (x == y) return true;
                    return false;
                };
                std::vector<std::vector<int>> groups;
                std::vector<int> notes;
                for (int ii : v) if (! items[static_cast<size_t> (ii)].grace) notes.push_back (ii);
                for (size_t i = 0; i < notes.size(); ++i)
                {
                    if (! groups.empty() && i > 0 && sharesBeam (notes[i - 1], notes[i])) groups.back().push_back (notes[i]);
                    else groups.push_back ({ notes[i] });
                }
                std::vector<std::vector<int>> candidates;
                for (const auto& g : groups)
                {
                    if (g.size() != 3 && g.size() != 6) continue;
                    bool equal = true;
                    for (int ii : g)
                    {
                        const auto& it = items[static_cast<size_t> (ii)];
                        equal = equal && ! it.rest && ! it.tuplet.active() && it.dots == 0 && it.type == items[static_cast<size_t> (g.front())].type;
                    }
                    if (equal) candidates.push_back (g);
                }
                if (candidates.empty()) continue;
                Fraction trial = sum;
                for (const auto& g : candidates)
                    for (int ii : g) trial -= items[static_cast<size_t> (ii)].duration * Fraction (1, 3);
                if (trial != length) continue;
                for (const auto& g : candidates)
                    for (size_t k2 = 0; k2 < g.size(); ++k2)
                    {
                        auto& it = items[static_cast<size_t> (g[k2])];
                        it.tuplet.actual = static_cast<int> (g.size()) == 6 ? 6 : 3;
                        it.tuplet.normal = static_cast<int> (g.size()) == 6 ? 4 : 2;
                        it.tuplet.start = k2 == 0;
                        it.tuplet.stop = k2 + 1 == g.size();
                        it.duration = itemDuration (it);
                    }
                bar.problems.push_back ({ Issue::Info, "Triplets without a printed 3 were read as triplets: the bar only adds up that way." });
            }

            // Onsets: each voice in turn; a second voice lines up with the first by position.
            std::vector<std::pair<double, Fraction>> columns;
            Fraction longest;
            for (size_t v = 0; v < voices.size(); ++v)
            {
                Fraction t;
                bool firstAnchored = false, firstSeen = false;
                for (int ii : voices[v])
                {
                    auto& it = items[static_cast<size_t> (ii)];
                    if (! it.grace && ! it.measureRest)
                    {
                        // a gap in this voice: its next note stands in a later column
                        bool anchored = false;
                        const double cx = (it.x + it.x1) * 0.5;
                        for (const auto& c : grid[r])
                            if (std::abs (c.first - cx) < 0.7 * sp) { anchored = true; if (c.second > t) t = c.second; break; }
                        if (v > 0)
                            for (const auto& c : columns)
                                if (std::abs (c.first - it.x) < 0.6 * sp) { anchored = true; if (c.second > t) t = c.second; break; }
                        if (! firstSeen) { firstAnchored = anchored; firstSeen = true; }
                    }
                    it.offset = t;
                    it.voice = static_cast<int> (v) + 1;
                    if (! it.grace)
                    {
                        if (v == 0) columns.push_back ({ it.x, t });
                        t += it.duration;
                    }
                }
                if (t > longest) longest = t;
                if (v > 0 && t < length && firstSeen && ! firstAnchored)
                    bar.problems.push_back ({ Issue::Warning, "Voice " + std::to_string (v + 1) + " does not fill the bar and its first note could not be lined up with another part; check where it starts." });
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
                    e.tremolo = it.tremolo;
                    e.tremoloType = it.tremoloType;
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

    // 8va and 15ma lines are not read yet: mark the bars where one starts.
    for (int k = 0; k < nStaves; ++k)
    {
        const int si = sys.staves[static_cast<size_t> (k)];
        const auto& staff = layout.staves[static_cast<size_t> (si)];
        std::vector<Box> marks;
        for (size_t i = 0; i < page.glyphs.size(); ++i)
        {
            const auto& n = page.glyphs[i].name;
            if (glyphStaff[i] == si && (n.rfind ("ottava", 0) == 0 || n.rfind ("quindicesima", 0) == 0 || n.rfind ("ventiduesima", 0) == 0))
                marks.push_back (page.glyphs[i].box);
        }
        for (const auto& t : page.texts)
        {
            const auto& x = t.text;
            if (! (x.rfind ("8v", 0) == 0 || x.rfind ("15m", 0) == 0 || x.rfind ("22m", 0) == 0)) continue;
            if (nearestStaff (t.box.cx(), t.box.cy()) != si) continue;
            marks.push_back (t.box);
        }
        for (const auto& b : marks)
        {
            const int bar = barOf (b.cx());
            if (bar >= 0)
                out.staves[static_cast<size_t> (k)][static_cast<size_t> (bar)].problems.push_back (
                    { Issue::Error, "An octave line (8va or similar) starts here; it is not read yet, so the notes under it are an octave off." });
        }
        (void) staff;
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

// One part of one system, as it shows: for matching to the score's parts
// when systems show different staves (empty staves hidden).
struct SystemPart
{
    int staves = 1;
    std::vector<int> clefs;     // per staff: 0 G, 1 F, 2 C, 3 other, -1 none
    std::string name;           // the label left of it, full or abbreviated
    std::vector<std::string> aliases;   // every label it has been seen with
};

std::string squeeze (const std::string& s)
{
    std::string out;
    for (char c : s)
        if (std::isalnum (static_cast<unsigned char> (c))) out += static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
    return out;
}

// "Pno." and "Piano", "Vc." and "Violoncello": the short one's letters, in
// order, starting with the same letter.
bool sameName (const std::string& a, const std::string& b)
{
    const auto x = squeeze (a), y = squeeze (b);
    if (x.empty() || y.empty()) return false;
    if (x == y) return true;
    const auto& shortOne = x.size() <= y.size() ? x : y;
    const auto& longOne = x.size() <= y.size() ? y : x;
    if (shortOne[0] != longOne[0]) return false;
    size_t k = 0;
    for (char c : longOne)
        if (k < shortOne.size() && c == shortOne[k]) ++k;
    return k == shortOne.size();
}

double matchCost (const SystemPart& a, const SystemPart& b)
{
    if (a.staves != b.staves) return 50.0;
    double c = 0.0;
    for (size_t i = 0; i < a.clefs.size() && i < b.clefs.size(); ++i)
        if (a.clefs[i] != b.clefs[i]) c += 1.0;   // a clef can change between systems
    if (! a.name.empty() && ! b.name.empty())
    {
        bool same = sameName (a.name, b.name);
        for (const auto& alias : b.aliases) same = same || sameName (a.name, alias);
        if (same) return 0.0;
        c += 2.0;   // "Cello" and "Vc." share no letters: a different name counts, but does not decide
    }
    return c;
}

std::vector<SystemPart> systemParts (const PageInk& page, const PageLayout& layout, const System& sys)
{
    std::vector<SystemPart> out;
    for (size_t k = 0; k < sys.staves.size(); ++k)
    {
        if (k == 0 || sys.partOfStaff[k] != sys.partOfStaff[k - 1]) out.push_back ({ 0, {}, {}, {} });
        auto& part = out.back();
        ++part.staves;
        const auto& staff = layout.staves[static_cast<size_t> (sys.staves[k])];
        int clef = -1;
        double bestX = 1e9;
        for (const auto& g : page.glyphs)
        {
            if (! isClefName (g.name) || g.box.x0 > staff.x0 + 8 * staff.space || g.box.x0 < staff.x0 - staff.space) continue;
            if (g.origin.y < staff.top() - staff.space || g.origin.y > staff.bottom() + staff.space) continue;
            if (g.box.x0 < bestX)
            {
                bestX = g.box.x0;
                clef = g.name.rfind ("gClef", 0) == 0 ? 0 : g.name.rfind ("fClef", 0) == 0 ? 1 : g.name.rfind ("cClef", 0) == 0 ? 2 : 3;
            }
        }
        part.clefs.push_back (clef);
    }
    // labels: text left of the part's staves
    size_t k = 0;
    for (auto& part : out)
    {
        const auto& top = layout.staves[static_cast<size_t> (sys.staves[k])];
        const auto& bottom = layout.staves[static_cast<size_t> (sys.staves[k + static_cast<size_t> (part.staves) - 1])];
        std::vector<const Text*> words;
        for (const auto& t : page.texts)
            if (t.box.x1 < top.x0 - 0.3 * top.space && t.box.cy() > top.top() - 1.5 * top.space && t.box.cy() < bottom.bottom() + 1.5 * bottom.space)
                words.push_back (&t);
        std::sort (words.begin(), words.end(), [] (const Text* a, const Text* b) { return std::abs (a->box.cy() - b->box.cy()) > 2 ? a->box.cy() < b->box.cy() : a->box.x0 < b->box.x0; });
        for (const auto* t : words) part.name += (part.name.empty() ? "" : " ") + t->text;
        k += static_cast<size_t> (part.staves);
    }
    return out;
}

// The first tempo on a page: a metronome mark ("quarter = 120"), else a
// tempo word. In quarter notes per minute; 0 if there is none.
double tempoOn (const PageInk& page)
{
    for (const auto& g : page.glyphs)
    {
        if (g.name.rfind ("metNote", 0) != 0) continue;
        double unit = 0;
        if (g.name.find ("Quarter") != std::string::npos) unit = 1;
        else if (g.name.find ("Half") != std::string::npos) unit = 2;
        else if (g.name.find ("Whole") != std::string::npos) unit = 4;
        else if (g.name.find ("8th") != std::string::npos) unit = 0.5;
        else if (g.name.find ("16th") != std::string::npos) unit = 0.25;
        if (unit == 0) continue;
        for (const auto& d : page.glyphs)
            if (d.name == "metAugmentationDot" && d.box.x0 >= g.box.x1 - 1 && d.box.x0 < g.box.x1 + g.size * 0.3 && std::abs (d.origin.y - g.origin.y) < g.size * 0.3)
                unit *= 1.5;
        // the number after it, on the same line
        const Text* best = nullptr;
        for (const auto& t : page.texts)
        {
            if (t.box.x0 < g.box.x1 - 1 || t.box.x0 > g.box.x1 + g.size * 2.5) continue;
            if (std::abs (t.box.y1 - g.origin.y) > g.size * 0.5 && std::abs (t.box.cy() - g.box.cy()) > g.size * 0.5) continue;
            if (t.text.find_first_of ("0123456789") == std::string::npos) continue;
            if (best == nullptr || t.box.x0 < best->box.x0) best = &t;
        }
        if (best == nullptr) continue;
        const auto digits = best->text.substr (best->text.find_first_of ("0123456789"));
        const double bpm = std::atof (digits.c_str());
        if (bpm >= 20 && bpm <= 400) return bpm * unit;
    }
    static const std::pair<const char*, double> words[] = {
        { "prestissimo", 200 }, { "presto", 168 }, { "vivace", 156 }, { "allegro", 132 }, { "allegretto", 112 },
        { "moderato", 108 }, { "andantino", 96 }, { "andante", 84 }, { "adagietto", 72 }, { "adagio", 66 },
        { "larghetto", 60 }, { "lento", 56 }, { "largo", 50 }, { "grave", 44 } };
    for (const auto& t : page.texts)
    {
        std::string w;
        for (char c : t.text) w += static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
        for (const auto& [name, q] : words)
            if (w.rfind (name, 0) == 0) return q;
    }
    return 0;
}

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
    std::vector<SystemPart> known;                       // the score's parts, as first seen
    std::vector<std::vector<StaffState>> partState;      // per part, per staff
    int globalBar = 0;
    struct OpenVolta { int bar = -1; std::string number; } openVolta, pendingVolta;   // a bracket running on past a system's end
    std::vector<Part>& parts = result.score.parts;

    for (const auto& page : pages)
    {
        const auto layout = findLayout (page);
        auto verdict = judgePage (page, layout);
        verdict.quarterTurns = page.quarterTurns;
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

        if (page.unnamedMusicGlyphs > 0)
            result.issues.push_back ({ Issue::Warning, page.number, { 0, 0, page.width, page.height }, -1, -1,
                                      std::to_string (page.unnamedMusicGlyphs) + " music symbols on this page could not be named and were left out; check the page." });

        if (result.score.quartersPerMinute <= 0) result.score.quartersPerMinute = tempoOn (page);

        PageReader reader (page, layout);
        reader.prepare();

        for (size_t s = 0; s < layout.systems.size(); ++s)
        {
            const auto& sys = layout.systems[s];

            // This system's parts: how many staves, their clefs, their label.
            std::vector<SystemPart> here = systemParts (page, layout, sys);
            if (std::getenv ("ANS_DEBUG_PARTS") != nullptr)
            {
                std::fprintf (stderr, "page %d system %zu:", page.number + 1, s);
                for (const auto& h : here) std::fprintf (stderr, " [%d staves, clef %d, '%s']", h.staves, h.clefs.empty() ? -9 : h.clefs[0], h.name.c_str());
                std::fprintf (stderr, "\n");
            }

            // Match them to the score's parts: by label where there is one,
            // then by staves and clefs, in order. A part new to the score
            // (shown only once it plays) is added where it stands.
            std::vector<int> partOf (here.size(), -1);
            {
                const size_t n = here.size(), mCount = known.size();
                const double inf = 1e9;
                std::vector<std::vector<double>> cost (n + 1, std::vector<double> (mCount + 1, inf));
                std::vector<std::vector<int>> step (n + 1, std::vector<int> (mCount + 1, 0));
                cost[0][0] = 0;
                for (size_t i = 0; i <= n; ++i)
                    for (size_t j = 0; j <= mCount; ++j)
                    {
                        if (cost[i][j] >= inf) continue;
                        if (i < n && j < mCount)
                        {
                            const double c = cost[i][j] + matchCost (here[i], known[j]);
                            if (c < cost[i + 1][j + 1]) { cost[i + 1][j + 1] = c; step[i + 1][j + 1] = 1; }
                        }
                        if (i < n && cost[i][j] + 3.0 < cost[i + 1][j]) { cost[i + 1][j] = cost[i][j] + 3.0; step[i + 1][j] = 2; }      // new part
                        if (j < mCount && cost[i][j] + 0.2 < cost[i][j + 1]) { cost[i][j + 1] = cost[i][j] + 0.2; step[i][j + 1] = 3; } // part not shown
                    }
                std::vector<std::pair<int, int>> path;
                for (size_t i = n, j = mCount; i > 0 || j > 0;)
                {
                    const int st = step[i][j];
                    if (st == 1) { path.push_back ({ static_cast<int> (i - 1), static_cast<int> (j - 1) }); --i; --j; }
                    else if (st == 2) { path.push_back ({ static_cast<int> (i - 1), -1 }); --i; }
                    else { --j; }
                }
                std::reverse (path.begin(), path.end());
                // insert new parts, keeping order
                int lastMatched = -1;
                int inserted = 0;
                for (auto& [i, j] : path)
                {
                    if (j >= 0) { partOf[static_cast<size_t> (i)] = j + inserted; lastMatched = j + inserted; continue; }
                    const int at = lastMatched + 1;
                    known.insert (known.begin() + at, here[static_cast<size_t> (i)]);
                    partState.insert (partState.begin() + at, std::vector<StaffState> (static_cast<size_t> (here[static_cast<size_t> (i)].staves)));
                    Part fresh;
                    fresh.staves = here[static_cast<size_t> (i)].staves;
                    for (int b = 0; b < globalBar; ++b)
                    {
                        Measure m;
                        m.number = std::to_string (b + 1);
                        m.notShown = true;
                        if (! parts.empty()) { m.left = parts.front().measures[static_cast<size_t> (b)].left; m.hasLeft = parts.front().measures[static_cast<size_t> (b)].hasLeft; m.right = parts.front().measures[static_cast<size_t> (b)].right; }
                        fresh.measures.push_back (m);
                    }
                    parts.insert (parts.begin() + at, fresh);
                    for (auto& is : result.issues) if (is.part >= at) ++is.part;
                    partOf[static_cast<size_t> (i)] = at;
                    lastMatched = at;
                    ++inserted;
                }
                // a known part remembers every label it is seen with
                for (size_t i = 0; i < n; ++i)
                {
                    if (partOf[i] < 0 || here[i].name.empty()) continue;
                    auto& k = known[static_cast<size_t> (partOf[i])];
                    if (k.name.empty()) k.name = here[i].name;
                    else if (std::find (k.aliases.begin(), k.aliases.end(), here[i].name) == k.aliases.end()) k.aliases.push_back (here[i].name);
                }
            }

            // Ambiguous: a part not shown here could have been one that is.
            bool ambiguous = false;
            for (size_t j = 0; j < known.size(); ++j)
            {
                if (std::find (partOf.begin(), partOf.end(), static_cast<int> (j)) != partOf.end()) continue;
                for (size_t i = 0; i < here.size(); ++i)
                    if (here[i].name.empty() && known[j].name.empty() && matchCost (here[i], known[j]) < 0.01) ambiguous = true;
            }
            if (ambiguous)
                result.issues.push_back ({ Issue::Warning, page.number, { sys.x0, layout.staves[static_cast<size_t> (sys.staves.front())].top(), sys.x1,
                                            layout.staves[static_cast<size_t> (sys.staves.back())].bottom() }, globalBar, -1,
                                          "Some parts are not shown in this system and the staves have no names; they were matched to parts by clef and order, which may be wrong." });

            // Each staff: its part and its place in it.
            std::vector<int> partIndex, staffInPart;
            {
                std::vector<int> seen (known.size(), 0);
                size_t staffK = 0;
                for (size_t i = 0; i < here.size(); ++i)
                    for (int st = 0; st < here[i].staves; ++st, ++staffK)
                    {
                        const int p = partOf[i];
                        int inPart = seen[static_cast<size_t> (p)]++;
                        if (inPart >= parts[static_cast<size_t> (p)].staves) inPart = parts[static_cast<size_t> (p)].staves - 1;
                        partIndex.push_back (p);
                        staffInPart.push_back (inPart);
                    }
            }

            // Read, with each staff's running state.
            std::vector<StaffState> flat;
            std::vector<int> flatIndex;
            for (size_t k = 0; k < sys.staves.size(); ++k)
            {
                flat.push_back (partState[static_cast<size_t> (partIndex[k])][static_cast<size_t> (staffInPart[k])]);
                flatIndex.push_back (static_cast<int> (k));
            }
            auto read = reader.readSystem (static_cast<int> (s), flat, flatIndex);
            if (std::getenv ("ANS_DEBUG_PARTS") != nullptr)
            {
                std::fprintf (stderr, "  -> %d bars; staves to parts:", read.bars);
                for (size_t k = 0; k < partIndex.size(); ++k) std::fprintf (stderr, " %d.%d", partIndex[k], staffInPart[k]);
                std::fprintf (stderr, "\n");
            }
            for (size_t k = 0; k < sys.staves.size(); ++k)
                partState[static_cast<size_t> (partIndex[k])][static_cast<size_t> (staffInPart[k])] = flat[k];

            for (size_t k = 0; k < sys.staves.size(); ++k)
            {
                auto& part = parts[static_cast<size_t> (partIndex[k])];
                const auto name = partNameFrom (read.staffNames[k]);
                if (part.name.empty() && ! name.empty() && name.size() > 2 && name.back() != '.') part.name = name;
            }

            std::vector<bool> shown (parts.size(), false);
            for (int p : partIndex) shown[static_cast<size_t> (p)] = true;

            for (int b = 0; b < read.bars; ++b)
            {
                for (size_t p = 0; p < parts.size(); ++p)
                {
                    Measure m;
                    m.number = std::to_string (globalBar + 1);
                    m.left = read.leftBar[static_cast<size_t> (b)];
                    m.hasLeft = read.hasLeft[static_cast<size_t> (b)];
                    m.right = read.rightBar[static_cast<size_t> (b)];
                    m.notShown = ! shown[p];
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

            // Voltas, on every part's barlines.
            const int firstBar = globalBar - read.bars;
            auto setEnding = [&] (int bar, bool left, Barline::Ending type, const std::string& number, const std::string& text) {
                for (auto& part : parts)
                {
                    if (bar < 0 || static_cast<size_t> (bar) >= part.measures.size()) continue;
                    auto& m = part.measures[static_cast<size_t> (bar)];
                    auto& b = left ? m.left : m.right;
                    b.ending = type; b.endingNumber = number; b.endingText = text;
                    if (left) m.hasLeft = true;
                }
            };
            for (const auto& v : read.voltas)
            {
                std::string number = v.number, text = v.text;
                if (number.empty())
                {
                    // carries on the open bracket from the system before
                    if (openVolta.bar < 0 || v.first != 0) continue;
                    number = openVolta.number;
                    setEnding (openVolta.bar, false, Barline::Ending::None, {}, {});
                }
                else
                    setEnding (firstBar + v.first, true, Barline::Ending::Start, number, text);
                setEnding (firstBar + v.last, false, v.closed ? Barline::Ending::Stop : Barline::Ending::Discontinue, number, {});
                if (v.toEnd) { pendingVolta.bar = firstBar + v.last; pendingVolta.number = number; }
            }
            openVolta = pendingVolta;
            pendingVolta = {};
        }
    }

    // A part that starts later (or is hidden at first) still needs its
    // clef, key and time at its first bar.
    for (auto& part : parts)
    {
        if (part.measures.empty() || parts.empty()) continue;
        auto& first = part.measures.front();
        for (int st = 1; st <= part.staves; ++st)
        {
            bool has = false;
            for (const auto& c : first.clefs) has = has || (c.staff == st && c.offset == Fraction (0));
            if (has) continue;
            for (const auto& m : part.measures)
            {
                bool found = false;
                for (const auto& c : m.clefs)
                    if (c.staff == st) { ClefChange cc = c; cc.offset = Fraction (0); first.clefs.insert (first.clefs.begin(), cc); found = true; break; }
                if (found) break;
            }
        }
        if (! first.key)
            for (const auto& m : part.measures) if (m.key) { first.key = m.key; break; }
        if (! first.time)
        {
            // from whichever part shows it first
            for (size_t b = 0; b < part.measures.size() && ! first.time; ++b)
                for (const auto& other : parts)
                    if (b < other.measures.size() && other.measures[b].time) { first.time = other.measures[b].time; break; }
        }
    }

    // A multi-bar rest stands for that many bars (the same in every part).
    if (! parts.empty())
    {
        const size_t bars = parts.front().measures.size();
        std::vector<int> count (bars, 1), newIndex (bars, 0);
        for (const auto& part : parts)
            for (size_t b = 0; b < part.measures.size() && b < bars; ++b) count[b] = std::max (count[b], part.measures[b].multiRest);
        int next = 0;
        for (size_t b = 0; b < bars; ++b) { newIndex[b] = next; next += count[b]; }
        for (auto& part : parts)
        {
            std::vector<Measure> expanded;
            for (size_t b = 0; b < part.measures.size(); ++b)
            {
                auto m = part.measures[b];
                const int n = b < bars ? count[b] : 1;
                const Barline right = m.right;
                if (n > 1) { m.right = Barline {}; m.multiRest = n; }
                expanded.push_back (m);
                for (int extra = 1; extra < n; ++extra)
                {
                    Measure rest;
                    rest.places = m.places;
                    for (const auto& e : m.events)
                        if (e.measureRest) rest.events.push_back (e);
                    if (extra + 1 == n) rest.right = right;
                    expanded.push_back (rest);
                }
            }
            part.measures.swap (expanded);
            for (size_t b = 0; b < part.measures.size(); ++b) part.measures[b].number = std::to_string (b + 1);
        }
        for (auto& is : result.issues)
            if (is.measureIndex >= 0 && static_cast<size_t> (is.measureIndex) < bars) is.measureIndex = newIndex[static_cast<size_t> (is.measureIndex)];
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

    // Every tie must land: a start needs a stop on the same pitch at the next
    // note of its staff, and a stop needs that start. Half a slur at a
    // system break looks like half a tie; this tells them apart.
    for (auto& part : parts)
        for (int staff = 1; staff <= part.staves; ++staff)
        {
            struct Ref { Note* note; Fraction at, end; };
            std::vector<Ref> notes;
            Fraction barStart;
            for (auto& m : part.measures)
            {
                Fraction longest;
                for (auto& e : m.events)
                {
                    if (e.offset + e.duration > longest) longest = e.offset + e.duration;
                    if (e.staff != staff || e.grace || e.rest) continue;
                    for (auto& n : e.notes) notes.push_back ({ &n, barStart + e.offset, barStart + e.offset + e.duration });
                }
                barStart += longest;
            }
            auto samePitch = [] (const Note& a, const Note& b) {
                return a.pitch.step == b.pitch.step && a.pitch.octave == b.pitch.octave && a.pitch.alter == b.pitch.alter;
            };
            for (auto& r : notes)
            {
                if (! r.note->tieStart) continue;
                bool found = false;
                for (auto& o : notes)
                    if (o.at == r.end && o.note->tieStop && samePitch (*o.note, *r.note)) found = true;
                if (! found) r.note->tieStart = false;
            }
            for (auto& r : notes)
            {
                if (! r.note->tieStop) continue;
                bool found = false;
                for (auto& o : notes)
                    if (o.end == r.at && o.note->tieStart && samePitch (*o.note, *r.note)) found = true;
                if (! found) r.note->tieStop = false;
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
                for (const auto& part : parts)
                    if (b < part.measures.size() && part.measures[b].time) { t = *part.measures[b].time; break; }
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
                auto& m = parts[p].measures[b];
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
                if (! hasEvents && m.notShown)
                {
                    // the part was not shown in this system: it rests
                    for (int st = 1; st <= parts[p].staves; ++st)
                    {
                        Event e;
                        e.rest = true; e.measureRest = true; e.type = 0; e.duration = length; e.staff = st;
                        m.events.push_back (e);
                    }
                    continue;
                }
                if (! hasEvents)
                {
                    Event e;
                    e.rest = true; e.measureRest = true; e.hidden = true; e.type = 0; e.duration = length;
                    parts[p].measures[b].events.push_back (e);
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
