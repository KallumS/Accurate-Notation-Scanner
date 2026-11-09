#include "Midi.h"

#include "AutoCC.h"
#include "Instruments.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>

namespace ans
{

int midiPitch (const Pitch& p)
{
    static const int semis[7] = { 0, 2, 4, 5, 7, 9, 11 };
    return std::clamp ((p.octave + 1) * 12 + semis[((p.step % 7) + 7) % 7] + p.alter, 0, 127);
}

int Performance::barAt (double q) const
{
    if (barStart.size() < 2) return 0;
    const auto it = std::upper_bound (barStart.begin(), barStart.end(), q);
    const int i = static_cast<int> (it - barStart.begin()) - 1;
    const int slot = std::clamp (i, 0, static_cast<int> (barStart.size()) - 2);
    return static_cast<size_t> (slot) < barPlayed.size() ? barPlayed[static_cast<size_t> (slot)] : slot;
}

namespace
{
std::vector<int> endingNumbers (const std::string& s)
{
    std::vector<int> out;
    int n = 0;
    bool digits = false;
    for (char c : s + ",")
    {
        if (std::isdigit (static_cast<unsigned char> (c))) { n = n * 10 + (c - '0'); digits = true; }
        else if (digits) { out.push_back (n); n = 0; digits = false; }
    }
    return out;
}
} // namespace

std::vector<int> playOrder (const Score& score)
{
    std::vector<int> order;
    if (score.parts.empty()) return order;
    const auto& ms = score.parts.front().measures;
    const int n = static_cast<int> (ms.size());

    // the ending each bar is in, by its numbers, and where that ending stops
    std::vector<std::vector<int>> endingOf (static_cast<size_t> (n));
    std::vector<int> endingEnd (static_cast<size_t> (n), -1);
    for (int b = 0; b < n; ++b)
    {
        if (ms[static_cast<size_t> (b)].left.ending != Barline::Ending::Start) continue;
        const auto numbers = endingNumbers (ms[static_cast<size_t> (b)].left.endingNumber);
        int e = b;
        while (e < n && ms[static_cast<size_t> (e)].right.ending == Barline::Ending::None
               && (e == b || ms[static_cast<size_t> (e)].left.ending != Barline::Ending::Start))
            ++e;
        if (e >= n || (e > b && ms[static_cast<size_t> (e)].left.ending == Barline::Ending::Start)) e = std::max (b, e - 1);
        for (int k = b; k <= e; ++k) { endingOf[static_cast<size_t> (k)] = numbers; endingEnd[static_cast<size_t> (k)] = e; }
    }

    int start = 0, pass = 1;
    std::map<int, int> jumps;   // backward repeats taken, per bar
    const size_t limit = static_cast<size_t> (n) * 10 + 10;
    for (int b = 0; b < n && order.size() < limit;)
    {
        const auto& m = ms[static_cast<size_t> (b)];
        if (m.left.repeat && b != start) { start = b; pass = 1; }
        const auto& numbers = endingOf[static_cast<size_t> (b)];
        if (! numbers.empty() && std::find (numbers.begin(), numbers.end(), pass) == numbers.end())
        {
            // not this pass's ending: skip it, unless no later ending is
            if (b == 0 || ms[static_cast<size_t> (b)].left.ending == Barline::Ending::Start)
            {
                bool later = false;
                for (int k = endingEnd[static_cast<size_t> (b)] + 1; k < n && ! endingOf[static_cast<size_t> (k)].empty(); k = endingEnd[static_cast<size_t> (k)] + 1)
                    later = later || std::find (endingOf[static_cast<size_t> (k)].begin(), endingOf[static_cast<size_t> (k)].end(), pass) != endingOf[static_cast<size_t> (k)].end();
                if (later) { b = endingEnd[static_cast<size_t> (b)] + 1; continue; }
            }
        }
        order.push_back (b);
        if (m.right.repeat)
        {
            int times = 2;
            for (int k : numbers) times = std::max (times, k + 1);
            if (jumps[b] + 1 < times) { ++jumps[b]; ++pass; b = start; continue; }
            start = b + 1; pass = 1;
        }
        else if (! numbers.empty() && b == endingEnd[static_cast<size_t> (b)] && (b + 1 >= n || endingOf[static_cast<size_t> (b + 1)].empty()))
        {
            start = b + 1; pass = 1;   // past the last ending
        }
        ++b;
    }
    return order;
}

namespace
{
std::string lower (std::string s)
{
    for (auto& c : s) c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
    return s;
}

bool has (const std::string& s, const char* word) { return s.find (word) != std::string::npos; }

int velocityFor (const std::string& dynamic)
{
    static const std::map<std::string, int> v = {
        { "pppppp", 16 }, { "ppppp", 20 }, { "pppp", 25 }, { "ppp", 32 }, { "pp", 42 }, { "p", 54 }, { "mp", 66 },
        { "mf", 78 }, { "f", 92 }, { "ff", 106 }, { "fff", 116 }, { "ffff", 122 }, { "fp", 92 }, { "sf", 104 },
        { "sfz", 108 }, { "sffz", 116 }, { "fz", 104 }, { "rf", 100 }, { "rfz", 104 }, { "sfp", 104 } };
    const auto it = v.find (dynamic);
    return it == v.end() ? -1 : it->second;
}
} // namespace

int programFor (const std::string& partName)
{
    const auto n = lower (partName);
    struct Rule { const char* words[6]; int program; };
    static const Rule rules[] = {
        { { "violoncell", "cello", "vc", "vlc", nullptr }, 42 },
        { { "contrabass", "double bass", "kontrabass", "cb.", nullptr }, 43 },
        { { "viola", "vla", "bratsche", nullptr }, 41 },
        { { "violin", "vln", "vl.", "violon", "geige", nullptr }, 40 },
        { { "piccolo", "picc", nullptr }, 72 },
        { { "flute", "flöte", "flauto", "fl.", nullptr }, 73 },
        { { "oboe", "ob.", "hautbois", nullptr }, 68 },
        { { "english horn", "cor anglais", nullptr }, 69 },
        { { "clarinet", "cl.", "klarinette", "clarinetto", nullptr }, 71 },
        { { "bassoon", "fagott", "fagotto", "bsn", "fg.", nullptr }, 70 },
        { { "horn", "corno", "cor.", "hn", nullptr }, 60 },
        { { "trumpet", "trompete", "tromba", "tpt", nullptr }, 56 },
        { { "trombone", "posaune", "trb", "tbn", nullptr }, 57 },
        { { "tuba", nullptr }, 58 },
        { { "timpani", "pauken", "timp", nullptr }, 47 },
        { { "harp", "harfe", "arpa", nullptr }, 46 },
        { { "organ", "orgel", "organo", nullptr }, 19 },
        { { "harpsichord", "cembalo", "clavecin", nullptr }, 6 },
        { { "guitar", "gitarre", "chitarra", "guitare", nullptr }, 24 },
        { { "soprano", "alto", "tenor", "bass", "voice", "voce" }, 52 },
        { { "choir", "chor", "coro", "s.", "a.", "t." }, 52 },
        { { "singstimme", "gesang", "chant", "canto", "mezzo", "baritone" }, 52 },
        { { "piano", "pno", "klavier", "pianoforte", nullptr }, 0 },
    };
    for (const auto& r : rules)
        for (const char* w : r.words)
        {
            if (w == nullptr) break;
            if (has (n, w)) return r.program;
        }
    return 0;
}

Performance perform (const Score& score)
{
    Performance out;
    out.quartersPerMinute = score.quartersPerMinute > 0 ? score.quartersPerMinute : 100.0;
    if (score.parts.empty()) { out.barStart.push_back (0); return out; }

    // Where each bar starts: the longest voice in any part (a pickup is short).
    const size_t bars = score.parts.front().measures.size();
    out.barPlayed = playOrder (score);
    double t = 0;
    for (int played : out.barPlayed)
    {
        const auto b = static_cast<size_t> (played);
        out.barStart.push_back (t);
        double longest = 0;
        for (const auto& part : score.parts)
        {
            if (b >= part.measures.size()) continue;
            for (const auto& e : part.measures[b].events)
                if (! e.grace) longest = std::max (longest, (e.offset + e.duration).value());
        }
        t += longest;
    }
    out.barStart.push_back (t);

    // Channels: one per part, skipping the drum channel; programs from names.
    for (size_t p = 0; p < score.parts.size(); ++p)
    {
        int ch = static_cast<int> (p % 15);
        if (ch >= 9) ++ch;
        out.channelOfPart.push_back (ch);
        const auto* inst = instruments::byId (score.parts[p].instrument);
        out.programOfPart.push_back (inst != nullptr ? inst->program : programFor (score.parts[p].name));
    }

    for (size_t p = 0; p < score.parts.size(); ++p)
    {
        const auto& part = score.parts[p];
        std::map<int, int> velocityOfStaff;   // the dynamic in force, per staff
        struct Open { size_t index; int pitch; int staff; };
        std::vector<Open> open;                // notes tied on into the next
        for (size_t slot = 0; slot < out.barPlayed.size(); ++slot)
        {
            const auto b = static_cast<size_t> (out.barPlayed[slot]);
            if (b >= part.measures.size() || b >= bars) continue;
            const auto& m = part.measures[b];
            const double barT = out.barStart[slot];
            std::vector<const Event*> events;
            for (const auto& e : m.events) events.push_back (&e);
            std::stable_sort (events.begin(), events.end(), [] (const Event* a, const Event* c) { return a->offset < c->offset; });
            for (const auto* e : events)
            {
                if (e->rest || e->hidden) continue;
                // the dynamic marked at or before this note
                for (const auto& d : m.directions)
                    if (d.staff == e->staff && d.offset <= e->offset && ! d.dynamics.empty())
                    {
                        const int v = velocityFor (d.dynamics);
                        if (v > 0) velocityOfStaff[d.staff] = v;
                    }
                const int velocity = velocityOfStaff.count (e->staff) ? velocityOfStaff[e->staff] : 80;
                const int dynamicLevel = velocityOfStaff.count (e->staff) ? velocityOfStaff[e->staff] - velocityFor ("mf") : 0;
                double start = barT + e->offset.value();
                double length = e->duration.value();
                if (e->grace)
                {
                    length = 0.125;
                    start = std::max (0.0, start - length);
                }
                for (const auto& n : e->notes)
                {
                    // at concert pitch where the page writes the part transposed
                    const int pitch = std::clamp (midiPitch (n.pitch) - part.transpose, 0, 127);
                    if (n.tieStop && ! e->grace)
                    {
                        auto it = std::find_if (open.begin(), open.end(), [&] (const Open& o) { return o.pitch == pitch && o.staff == e->staff; });
                        if (it != open.end())
                        {
                            auto& held = out.notes[it->index];
                            held.length = start + length - held.start;
                            held.written = held.length;
                            if (! n.tieStart) open.erase (it);
                            continue;
                        }
                    }
                    PlayedNote pn;
                    pn.part = static_cast<int> (p);
                    pn.channel = out.channelOfPart[p];
                    pn.pitch = pitch;
                    pn.start = start;
                    pn.length = std::max (0.05, length * (e->grace ? 1.0 : 0.95));
                    pn.written = std::max (0.05, length);
                    pn.velocity = velocity;
                    pn.dynamic = dynamicLevel;
                    pn.bar = static_cast<int> (b);
                    out.notes.push_back (pn);
                    if (n.tieStart && ! e->grace) open.push_back ({ out.notes.size() - 1, pitch, e->staff });
                }
            }
        }
    }
    std::stable_sort (out.notes.begin(), out.notes.end(), [] (const PlayedNote& a, const PlayedNote& b) { return a.start < b.start; });

    // AutoCC's curves, part by part, for the instruments that sustain.
    for (size_t p = 0; p < score.parts.size(); ++p)
    {
        int family = sustainFamily (out.programOfPart[p]);
        if (const auto* inst = instruments::byId (score.parts[p].instrument))
        {
            using S = instruments::Shape;
            family = inst->shape == S::Strings ? 1 : inst->shape == S::Brass ? 2 : inst->shape == S::Woodwinds ? 3 : inst->shape == S::Neutral ? 4 : 0;
        }
        if (family == 0) continue;
        autocc::Settings settings;
        settings.preset = family == 1 ? autocc::Preset::Strings : family == 2 ? autocc::Preset::Brass
                        : family == 3 ? autocc::Preset::Woodwinds : autocc::Preset::Default;
        // AutoCC's gesture is first key down to last key up, and in its
        // Legato mode only those two moments act. Notes written joined (each
        // starting before the line before it has ended) are one gesture, as a
        // player overlapping the keys would give it; a rest ends it. So one key
        // per gesture, with the first note's velocity, is exactly what AutoCC
        // would make of the line played legato.
        std::vector<autocc::KeyEvent> keys;
        int lastDynamic = 0;
        double gestureEnd = -1;
        for (const auto& n : out.notes)
        {
            if (n.part != static_cast<int> (p)) continue;
            const double on = out.seconds (n.start), off = out.seconds (n.start + n.written);
            if (keys.empty() || on > gestureEnd + 1e-6)
            {
                if (! keys.empty()) keys.push_back ({ gestureEnd, 60, 0, false });
                keys.push_back ({ on, 60, n.velocity, true });
                gestureEnd = off;
            }
            else
                gestureEnd = std::max (gestureEnd, off);
            if (n.dynamic != lastDynamic || settings.dynamic.empty())
            {
                settings.dynamic.push_back ({ on, std::clamp (n.dynamic, -64, 64) });
                lastDynamic = n.dynamic;
            }
        }
        if (! keys.empty()) keys.push_back ({ gestureEnd, 60, 0, false });
        if (keys.empty()) continue;
        for (const auto& c : autocc::render (keys, settings))
            out.controls.push_back ({ static_cast<int> (p), out.channelOfPart[p], out.quarters (c.seconds), c.cc, c.value });
    }
    std::stable_sort (out.controls.begin(), out.controls.end(), [] (const PlayedControl& a, const PlayedControl& b) { return a.start < b.start; });
    return out;
}

int sustainFamily (int program)
{
    if ((program >= 40 && program <= 45) || (program >= 48 && program <= 51)) return 1;   // strings, not harp or timpani
    if (program >= 56 && program <= 63) return 2;
    if (program >= 64 && program <= 79) return 3;
    if (program >= 52 && program <= 54) return 4;
    return 0;
}

namespace
{
void put32 (std::string& s, uint32_t v) { for (int i = 3; i >= 0; --i) s += static_cast<char> ((v >> (8 * i)) & 0xff); }
void put16 (std::string& s, uint16_t v) { s += static_cast<char> (v >> 8); s += static_cast<char> (v & 0xff); }
void putVar (std::string& s, uint32_t v)
{
    char buf[5];
    int n = 0;
    buf[n++] = static_cast<char> (v & 0x7f);
    while ((v >>= 7) != 0) buf[n++] = static_cast<char> (0x80 | (v & 0x7f));
    while (n > 0) s += buf[--n];
}

struct TrackEvent { uint32_t tick; int order; std::string bytes; };

std::string track (std::vector<TrackEvent> events)
{
    std::stable_sort (events.begin(), events.end(), [] (const TrackEvent& a, const TrackEvent& b) {
        if (a.tick != b.tick) return a.tick < b.tick;
        return a.order < b.order;
    });
    std::string body;
    uint32_t last = 0;
    for (const auto& e : events)
    {
        putVar (body, e.tick - last);
        body += e.bytes;
        last = e.tick;
    }
    putVar (body, 0);
    body += "\xff\x2f";
    body += '\0';
    std::string out = "MTrk";
    put32 (out, static_cast<uint32_t> (body.size()));
    return out + body;
}

std::string meta (int type, const std::string& data)
{
    std::string s;
    s += static_cast<char> (0xff);
    s += static_cast<char> (type);
    putVar (s, static_cast<uint32_t> (data.size()));
    return s + data;
}
} // namespace

std::string toMidiFile (const Score& score, const Performance& perf)
{
    const int division = 480;
    auto tick = [&] (double q) { return static_cast<uint32_t> (std::lround (std::max (0.0, q) * division)); };

    std::string file = "MThd";
    put32 (file, 6);
    put16 (file, 1);
    put16 (file, static_cast<uint16_t> (1 + score.parts.size()));
    put16 (file, division);

    // Track 0: title, tempo, time signatures.
    std::vector<TrackEvent> conductor;
    if (! score.title.empty()) conductor.push_back ({ 0, 0, meta (0x03, score.title) });
    const uint32_t usPerQuarter = static_cast<uint32_t> (std::lround (60000000.0 / perf.quartersPerMinute));
    std::string tempo;
    tempo += static_cast<char> ((usPerQuarter >> 16) & 0xff);
    tempo += static_cast<char> ((usPerQuarter >> 8) & 0xff);
    tempo += static_cast<char> (usPerQuarter & 0xff);
    conductor.push_back ({ 0, 1, meta (0x51, tempo) });
    if (! score.parts.empty())
        for (size_t slot = 0; slot < perf.barPlayed.size() && slot < perf.barStart.size(); ++slot)
        {
            const auto b = static_cast<size_t> (perf.barPlayed[slot]);
            if (b >= score.parts.front().measures.size()) continue;
            const auto& m = score.parts.front().measures[b];
            if (! m.time) continue;
            int log2 = 0;
            for (int d = m.time->beatType; d > 1; d /= 2) ++log2;
            std::string ts;
            ts += static_cast<char> (m.time->beats);
            ts += static_cast<char> (log2);
            ts += static_cast<char> (24);
            ts += static_cast<char> (8);
            conductor.push_back ({ tick (perf.barStart[slot]), 2, meta (0x58, ts) });
        }
    file += track (conductor);

    for (size_t p = 0; p < score.parts.size(); ++p)
    {
        std::vector<TrackEvent> ev;
        const int ch = perf.channelOfPart[p];
        ev.push_back ({ 0, 0, meta (0x03, score.parts[p].name.empty() ? "Part " + std::to_string (p + 1) : score.parts[p].name) });
        std::string program;
        program += static_cast<char> (0xc0 | ch);
        program += static_cast<char> (perf.programOfPart[p]);
        ev.push_back ({ 0, 1, program });
        for (const auto& n : perf.notes)
        {
            if (n.part != static_cast<int> (p)) continue;
            std::string on, off;
            on += static_cast<char> (0x90 | ch); on += static_cast<char> (n.pitch); on += static_cast<char> (n.velocity);
            off += static_cast<char> (0x80 | ch); off += static_cast<char> (n.pitch); off += static_cast<char> (0);
            ev.push_back ({ tick (n.start), 3, on });
            ev.push_back ({ tick (n.start + n.length), 2, off });   // a note off before a note on at the same time
        }
        for (const auto& c : perf.controls)
        {
            if (c.part != static_cast<int> (p)) continue;
            std::string cc;
            cc += static_cast<char> (0xb0 | ch); cc += static_cast<char> (c.controller); cc += static_cast<char> (c.value);
            ev.push_back ({ tick (c.start), 2, cc });   // before a note starting with it
        }
        file += track (ev);
    }
    return file;
}

} // namespace ans
