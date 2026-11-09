// ScaleView Pro's chord reader, ported from `reascripts/ScaleView Pro.lua`.
// The comments that explain each rule are ScaleView's; they are kept short
// here - the Lua has them in full, with the cases that decided them.

#include "Chords.h"

#include <algorithm>
#include <map>
#include <set>

namespace ans::chords
{

namespace
{
const char* const SHARP_NAMES[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char* const FLAT_NAMES[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
const char* const LETTERS[7] = { "C", "D", "E", "F", "G", "A", "B" };
const int LETTER_PCS[7] = { 0, 2, 4, 5, 7, 9, 11 };

// The eighteen spellings ScaleView's scale list offers: the only ones a chord
// is named on.
const std::set<std::string>& chordRootNames()
{
    static const std::set<std::string> s { "C", "C#", "Db", "D", "D#", "Eb", "E", "F", "F#", "Gb",
                                           "G", "G#", "Ab", "A", "A#", "Bb", "B", "Cb" };
    return s;
}

struct ScaleDef { const char* name; std::vector<int> intervals, letters; };
const std::vector<ScaleDef>& scaleDefs()
{
    static const std::vector<ScaleDef> d {
        { "Major", { 0, 2, 4, 5, 7, 9, 11 }, { 0, 1, 2, 3, 4, 5, 6 } },
        { "Minor (Natural)", { 0, 2, 3, 5, 7, 8, 10 }, { 0, 1, 2, 3, 4, 5, 6 } },
        { "Harmonic Minor", { 0, 2, 3, 5, 7, 8, 11 }, { 0, 1, 2, 3, 4, 5, 6 } },
        { "Ionian", { 0, 2, 4, 5, 7, 9, 11 }, { 0, 1, 2, 3, 4, 5, 6 } },
        { "Dorian", { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 2, 3, 4, 5, 6 } },
        { "Phrygian", { 0, 1, 3, 5, 7, 8, 10 }, { 0, 1, 2, 3, 4, 5, 6 } },
        { "Lydian", { 0, 2, 4, 6, 7, 9, 11 }, { 0, 1, 2, 3, 4, 5, 6 } },
        { "Mixolydian", { 0, 2, 4, 5, 7, 9, 10 }, { 0, 1, 2, 3, 4, 5, 6 } },
        { "Aeolian", { 0, 2, 3, 5, 7, 8, 10 }, { 0, 1, 2, 3, 4, 5, 6 } },
        { "Major Pentatonic", { 0, 2, 4, 7, 9 }, { 0, 1, 2, 4, 5 } },
        { "Minor Pentatonic", { 0, 3, 5, 7, 10 }, { 0, 2, 3, 4, 6 } },
        { "Major Blues", { 0, 2, 3, 4, 7, 9 }, { 0, 1, 2, 2, 4, 5 } },
        { "Minor Blues", { 0, 3, 5, 6, 7, 10 }, { 0, 2, 3, 4, 4, 6 } },
        { "Whole Tone", { 0, 2, 4, 6, 8, 10 }, { 0, 1, 2, 3, 4, 5 } },
        { "Diminished Whole-Half", { 0, 2, 3, 5, 6, 8, 9, 11 }, { 0, 1, 2, 3, 4, 5, 5, 6 } },
        { "Diminished Half-Whole", { 0, 1, 3, 4, 6, 7, 9, 10 }, { 0, 1, 2, 2, 3, 4, 5, 6 } },
    };
    return d;
}

int mod12 (int x) { return ((x % 12) + 12) % 12; }

// Spells pc on a letter ("" when that needs more than a double accidental).
std::string spellAs (int letter, int pc)
{
    letter = ((letter % 7) + 7) % 7;
    const int offset = mod12 (pc - LETTER_PCS[letter] + 6) - 6;
    static const std::map<int, const char*> acc { { -2, "bb" }, { -1, "b" }, { 0, "" }, { 1, "#" }, { 2, "x" } };
    const auto it = acc.find (offset);
    if (it == acc.end()) return {};
    return std::string (LETTERS[letter]) + it->second;
}

// What a key gives the reader: the notes in it, how each is spelled, which
// way it leans, and its first, third and fifth degrees.
struct KeyView
{
    bool set = false;
    bool active[12] = {};
    std::string names[12];
    bool usesFlats = false;
    bool tonic[12] = {};
};

KeyView viewOf (const Scale& s)
{
    KeyView k;
    for (int pc = 0; pc < 12; ++pc) k.names[pc] = SHARP_NAMES[pc];
    if (s.empty())
    {
        // ScaleView's ASSUMED_KEY and ASSUMED_TONIC: C major for the draws.
        for (int pc : { 0, 2, 4, 5, 7, 9, 11 }) k.active[pc] = true;
        for (int pc : { 0, 4, 7 }) k.tonic[pc] = true;
        return k;
    }
    k.set = true;
    const int rootPc = mod12 (LETTER_PCS[s.rootLetter] + s.rootAccidental);
    int sharps = 0, flats = 0;
    std::string spelled[12];
    bool named[12] = {};
    for (size_t d = 0; d < s.intervals.size(); ++d)
    {
        const int pc = mod12 (rootPc + s.intervals[d]);
        const auto name = spellAs (s.rootLetter + s.letters[d], pc);
        k.active[pc] = true;
        if (! name.empty())
        {
            spelled[pc] = name;
            named[pc] = true;
            if (name.find ('#') != std::string::npos || name.find ('x') != std::string::npos) ++sharps;
            if (name.find ('b', 1) != std::string::npos) ++flats;
        }
    }
    k.usesFlats = flats > sharps;
    for (int pc = 0; pc < 12; ++pc)
        k.names[pc] = named[pc] ? spelled[pc] : (k.usesFlats ? FLAT_NAMES[pc] : SHARP_NAMES[pc]);
    for (size_t degree : { size_t (0), size_t (2), size_t (4) })
        if (degree < s.intervals.size()) k.tonic[mod12 (rootPc + s.intervals[degree])] = true;
    return k;
}

std::string chordNoteName (const KeyView& k, int pc)
{
    const auto& name = k.names[pc];
    if (chordRootNames().count (name)) return name;
    return k.usesFlats ? FLAT_NAMES[pc] : SHARP_NAMES[pc];
}

// ---------------------------------------------------------------- the reader

const std::map<std::string, int>& coreRank()
{
    static const std::map<std::string, int> r {
        { "maj/P/none", 1 }, { "min/P/none", 2 },
        { "maj/P/b7", 3 }, { "min/P/b7", 4 }, { "maj/P/maj7", 5 },
        { "min/b/b7", 6 }, { "min/b/bb7", 7 }, { "min/b/none", 8 },
        { "maj/#/none", 9 },
        { "sus4/P/none", 10 }, { "sus2/P/none", 11 },
        { "min/P/maj7", 12 },
        { "maj/#/b7", 13 }, { "maj/b/b7", 14 }, { "maj/#/maj7", 15 },
        { "maj/b/maj7", 16 },
        { "sus4/P/b7", 17 }, { "sus2/P/b7", 18 },
        { "sus4/P/maj7", 19 }, { "sus2/P/maj7", 20 },
        { "none/P/b7", 30 }, { "none/P/maj7", 31 }, { "none/P/none", 34 },
        { "none/b/maj7", 41 }, { "none/b/b7", 42 },
    };
    return r;
}

int rankUnnamed (const std::string& third)
{
    if (third == "maj" || third == "min") return 25;
    if (third == "sus4" || third == "sus2") return 70;
    return 90;
}

constexpr int RANK_TWICE_ODD = 12;
constexpr int RANK_NO_FIFTH = 20;
constexpr int RANK_NO_FIFTH7 = 4;
constexpr int RANK_ELEVENTH = 6;
constexpr int COST_INVERSION = 14;
constexpr int COST_NATURAL = 1;
constexpr int COST_ADD = 2;
constexpr int COST_CLASH = 6;
constexpr int COST_ALTERED = 5;
constexpr int COST_CLASHING = 18;
constexpr int COST_SUS_EXTRA = 12;
constexpr int COST_SIXTH = 4;
constexpr int COST_MIN_SIXTH = 5;

struct Core { std::string third, fifth, seventh; bool used[12] = {}; };

Core core (const bool* has, bool preferSharpFive)
{
    Core c;
    c.used[0] = true;
    if (has[4]) { c.third = "maj"; c.used[4] = true; }
    else if (has[3]) { c.third = "min"; c.used[3] = true; }
    else if (has[5]) { c.third = "sus4"; c.used[5] = true; }
    else if (has[2]) { c.third = "sus2"; c.used[2] = true; }
    else c.third = "none";

    if (has[7]) { c.fifth = "P"; c.used[7] = true; }
    else if (has[6] && has[8] && preferSharpFive) { c.fifth = "#"; c.used[8] = true; }
    else if (has[6]) { c.fifth = "b"; c.used[6] = true; }
    else if (has[8]) { c.fifth = "#"; c.used[8] = true; }
    else c.fifth = "none";

    if (has[10]) { c.seventh = "b7"; c.used[10] = true; }
    else if (has[11]) { c.seventh = "maj7"; c.used[11] = true; }
    else if (c.third == "min" && c.fifth == "b" && has[9]) { c.seventh = "bb7"; c.used[9] = true; }
    else c.seventh = "none";
    return c;
}

std::string coreName (const std::string& third, const std::string& fifth, const std::string& seventh)
{
    static const std::map<std::string, std::string> special {
        { "min/b/none", "dim" }, { "min/b/bb7", "dim7" }, { "min/b/b7", "min7b5" },
        { "maj/#/none", "aug" }, { "maj/#/b7", "aug7" }, { "maj/#/maj7", "maj7#5" },
        { "maj/b/b7", "7b5" }, { "min/P/maj7", "minMaj7" }, { "min/none/maj7", "minMaj7" },
    };
    const auto it = special.find (third + "/" + fifth + "/" + seventh);
    if (it != special.end()) return it->second;

    const std::string base = third == "min" ? "min" : third == "sus4" ? "sus4" : third == "sus2" ? "sus2" : "";
    const std::string sev = seventh == "b7" ? "7" : seventh == "bb7" ? "dim7" : seventh == "maj7" ? (third == "min" ? "Maj7" : "maj7") : "";
    const std::string alt = fifth == "b" ? "b5" : fifth == "#" ? "#5" : "";
    std::string name = (third == "sus4" || third == "sus2") ? sev + base : base + sev;
    name += alt;
    if (name == alt && ! alt.empty()) name = "(" + alt + ")";
    return name;
}

int rankOf (const std::string& third, const std::string& fifth, const std::string& seventh)
{
    const auto& table = coreRank();
    if (fifth == "none")
    {
        const auto it = table.find (third + "/P/" + seventh);
        const int rank = it != table.end() ? it->second : rankUnnamed (third);
        return rank + (seventh == "none" ? RANK_NO_FIFTH : RANK_NO_FIFTH7);
    }
    const auto it = table.find (third + "/" + fifth + "/" + seventh);
    if (it != table.end()) return it->second;
    return rankUnnamed (third) + (seventh != "none" ? RANK_TWICE_ODD : 0);
}

void replaceFirst (std::string& s, const std::string& from, const std::string& to)
{
    const auto at = s.find (from);
    if (at != std::string::npos) s.replace (at, from.size(), to);
}

struct Reading { std::string name; int cost = 0, rank = 0; };

Reading analyseAs (const bool* has, int root, int bass, bool preferSharpFive)
{
    const auto c = core (has, preferSharpFive);
    const auto& third = c.third;
    const auto& fifth = c.fifth;
    const auto& seventh = c.seventh;
    const int rank = rankOf (third, fifth, seventh);
    std::string name = coreName (third, fifth, seventh);
    int cost = rank;

    // what the core did not take
    std::set<int> naturals;
    std::vector<std::string> altered;
    bool sixth = false, asEleventh = false;
    for (int i = 1; i <= 11; ++i)
    {
        if (! has[i] || c.used[i]) continue;
        if (i == 9)
        {
            if (seventh == "none") sixth = true;
            else naturals.insert (13);
        }
        else if (i == 1) altered.push_back ("b9");
        else if (i == 2) naturals.insert (9);
        else if (i == 3) altered.push_back ("#9");
        else if (i == 5) naturals.insert (11);
        else if (i == 6) altered.push_back ("#11");
        else if (i == 8) altered.push_back ("b13");
        else altered.push_back ("maj7");   // only 11 arrives here: a major seventh beside a flat one
    }

    if (seventh != "none" && seventh != "bb7")
    {
        bool ninth = naturals.count (9) > 0;
        for (const auto& t : altered) if (t == "b9" || t == "#9") ninth = true;
        int number = 0;
        if (ninth && naturals.count (13)) number = 13;
        else if (ninth && naturals.count (11) && third != "maj") number = 11;
        else if (naturals.count (9)) number = 9;

        if (third == "sus4" && seventh == "b7" && naturals.count (9) && (fifth == "P" || fifth == "none"))
        {
            name = "11";
            number = 11;
            asEleventh = true;
            naturals.insert (11);
            cost = RANK_ELEVENTH + (fifth == "none" ? RANK_NO_FIFTH7 : 0);
        }
        else if (number != 0)
            replaceFirst (name, "7", std::to_string (number));

        std::vector<int> spare;
        for (int degree : { 9, 11, 13 })
        {
            if (! naturals.count (degree)) continue;
            const bool implied = number != 0 && degree <= number && ! (degree == 11 && third == "maj");
            if (implied) cost += COST_NATURAL;
            else
            {
                cost += (degree == 11 && third == "maj") ? COST_CLASH : COST_ADD;
                spare.push_back (degree);
            }
        }
        if (! spare.empty())
        {
            name += "(";
            for (size_t k = 0; k < spare.size(); ++k) name += (k ? "," : "") + std::to_string (spare[k]);
            name += ")";
        }
    }
    else
    {
        if (sixth)
        {
            cost += (third == "min" && fifth == "P") ? COST_MIN_SIXTH : COST_SIXTH;
            const std::string mark = fifth == "b" ? "b5" : fifth == "#" ? "#5" : "";
            if (naturals.count (9) && (third == "maj" || third == "min"))
            {
                naturals.erase (9);
                name = (third == "min" ? "min6/9" : "6/9") + mark;
                cost += COST_ADD;
            }
            else if (third == "min") name = "min6" + mark;
            else if (third == "maj") name = fifth == "#" ? "aug6" : "6" + mark;
            else if (third == "none") name = "6" + mark;
            else name += "(add6)";
        }
        if (seventh == "bb7" && naturals.count (9))
        {
            naturals.erase (9);
            replaceFirst (name, "dim7", "dim9");
            cost += COST_ADD;
        }
        for (int degree : { 9, 11, 13 })
            if (naturals.count (degree))
            {
                cost += (degree == 11 && third == "maj") ? COST_CLASH : COST_ADD;
                name += (name.empty() ? "add" : "Add") + std::to_string (degree);
            }
    }

    const bool dominant = third == "maj" && seventh == "b7";
    const bool triad = (third == "maj" || third == "min") && fifth == "P";
    for (const auto& token : altered)
    {
        const bool plainFifth = fifth != "b" && fifth != "#";
        const bool athome = token != "maj7"
                            && ((dominant && (plainFifth || root == bass))
                                || (plainFifth && (token == "#11" || (triad && token != "b13"))));
        const std::string shown = (token == "b13" && seventh == "none") ? "b6" : token == "maj7" ? "(maj7)" : token;
        cost += athome ? COST_ALTERED : COST_CLASHING;
        name += ((name.empty() && seventh == "none") ? "add" : "") + shown;
    }

    if ((third == "sus4" || third == "sus2") && ! asEleventh)
    {
        int carried = static_cast<int> (altered.size());
        for (int degree : { 9, 11, 13 }) if (naturals.count (degree)) ++carried;
        if (sixth) ++carried;
        cost += carried * COST_SUS_EXTRA;
    }

    if (third == "none") name = name.empty() ? "5" : name + "(no3)";
    if (root != bass) cost += COST_INVERSION;
    return { name, cost, rank };
}

Reading analyse (const bool* has, int root, int bass)
{
    auto r = analyseAs (has, root, bass, false);
    if (has[6] && has[8] && ! has[7])
    {
        auto alt = analyseAs (has, root, bass, true);
        if (alt.cost < r.cost) return alt;
    }
    return r;
}
} // namespace

Scale Scale::named (const std::string& root, const std::string& scale)
{
    Scale s;
    if (root.empty() || root[0] < 'A' || root[0] > 'G') return s;
    static const int letterOf[7] = { 5, 6, 0, 1, 2, 3, 4 };   // A..G -> C-based index
    s.rootLetter = letterOf[root[0] - 'A'];
    for (size_t i = 1; i < root.size(); ++i) s.rootAccidental += root[i] == '#' ? 1 : root[i] == 'b' ? -1 : 0;
    for (const auto& d : scaleDefs())
        if (scale == d.name) { s.intervals = d.intervals; s.letters = d.letters; }
    return s;
}

Scale Scale::majorKeyOf (int fifths)
{
    static const char* const keys[15] = { "Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#" };
    if (fifths < -7 || fifths > 7) return {};
    return named (keys[fifths + 7], "Major");
}

std::string name (const std::vector<int>& midiNotes, const Scale& keyScale)
{
    if (midiNotes.empty()) return {};
    const auto key = viewOf (keyScale);
    bool classes[12] = {};
    int voices[12] = {};
    std::set<int> distinct (midiNotes.begin(), midiNotes.end());
    for (int n : distinct) { classes[mod12 (n)] = true; ++voices[mod12 (n)]; }
    const int bass = mod12 (*distinct.begin());
    int count = 0;
    for (bool b : classes) count += b ? 1 : 0;
    if (count == 1) return chordNoteName (key, bass);

    auto spellOut = [&] {
        std::string out;
        for (int pc = 0; pc < 12; ++pc)
            if (classes[pc]) out += (out.empty() ? "" : " ") + chordNoteName (key, pc);
        return out;
    };
    auto rootPosition = [&] (int root) {
        if (root == bass) return true;
        if (voices[root] < 2) return false;
        return key.tonic[root];
    };

    if (count == 2)
    {
        for (int root = 0; root < 12; ++root)
            if (classes[root] && classes[(root + 7) % 12])
                return chordNoteName (key, root) + "5" + (rootPosition (root) ? "" : "/" + chordNoteName (key, bass));
        for (int root = 0; root < 12; ++root)
        {
            if (! classes[root]) continue;
            const char* quality = classes[(root + 4) % 12] ? "maj" : classes[(root + 3) % 12] ? "min" : nullptr;
            if (quality != nullptr)
                return chordNoteName (key, root) + quality + "(no5)" + (rootPosition (root) ? "" : "/" + chordNoteName (key, bass));
        }
        return spellOut();
    }
    if (count > 7) return spellOut();

    struct Best { int root = -1; std::string name; int cost = 0, rank = 0, fit = 0; bool slash = false; } best;
    for (int root = 0; root < 12; ++root)
    {
        if (! classes[root]) continue;
        bool has[12] = {};
        for (int pc = 0; pc < 12; ++pc)
            if (classes[pc]) has[mod12 (pc - root)] = true;
        const auto r = analyse (has, root, bass);
        int fit = key.active[root] ? 100 : 0;
        for (int pc = 0; pc < 12; ++pc)
            if (classes[pc] && key.active[pc]) ++fit;
        const bool slash = root != bass;
        if (best.root < 0 || r.cost < best.cost
            || (r.cost == best.cost && fit > best.fit)
            || (r.cost == best.cost && fit == best.fit && best.slash && ! slash)
            || (r.cost == best.cost && fit == best.fit && slash == best.slash && r.rank < best.rank))
            best = { root, r.name, r.cost, r.rank, fit, slash };
    }
    std::string out = chordNoteName (key, best.root) + best.name;
    if (! rootPosition (best.root)) out += "/" + chordNoteName (key, bass);
    return out;
}

bool isChordSymbol (const std::string& name)
{
    return ! name.empty() && name.find (' ') == std::string::npos;
}

} // namespace ans::chords
