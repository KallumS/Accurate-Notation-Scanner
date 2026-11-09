#include "Midi.h"

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
    return std::clamp (i, 0, static_cast<int> (barStart.size()) - 2);
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
    double t = 0;
    for (size_t b = 0; b < bars; ++b)
    {
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
        out.programOfPart.push_back (programFor (score.parts[p].name));
    }

    for (size_t p = 0; p < score.parts.size(); ++p)
    {
        const auto& part = score.parts[p];
        std::map<int, int> velocityOfStaff;   // the dynamic in force, per staff
        struct Open { size_t index; int pitch; int staff; };
        std::vector<Open> open;                // notes tied on into the next
        for (size_t b = 0; b < part.measures.size() && b < bars; ++b)
        {
            const auto& m = part.measures[b];
            const double barT = out.barStart[b];
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
                double start = barT + e->offset.value();
                double length = e->duration.value();
                if (e->grace)
                {
                    length = 0.125;
                    start = std::max (0.0, start - length);
                }
                for (const auto& n : e->notes)
                {
                    const int pitch = midiPitch (n.pitch);
                    if (n.tieStop && ! e->grace)
                    {
                        auto it = std::find_if (open.begin(), open.end(), [&] (const Open& o) { return o.pitch == pitch && o.staff == e->staff; });
                        if (it != open.end())
                        {
                            auto& held = out.notes[it->index];
                            held.length = start + length - held.start;
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
                    pn.velocity = velocity;
                    pn.bar = static_cast<int> (b);
                    out.notes.push_back (pn);
                    if (n.tieStart && ! e->grace) open.push_back ({ out.notes.size() - 1, pitch, e->staff });
                }
            }
        }
    }
    std::stable_sort (out.notes.begin(), out.notes.end(), [] (const PlayedNote& a, const PlayedNote& b) { return a.start < b.start; });
    return out;
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
        for (size_t b = 0; b < score.parts.front().measures.size() && b < perf.barStart.size(); ++b)
        {
            const auto& m = score.parts.front().measures[b];
            if (! m.time) continue;
            int log2 = 0;
            for (int d = m.time->beatType; d > 1; d /= 2) ++log2;
            std::string ts;
            ts += static_cast<char> (m.time->beats);
            ts += static_cast<char> (log2);
            ts += static_cast<char> (24);
            ts += static_cast<char> (8);
            conductor.push_back ({ tick (perf.barStart[b]), 2, meta (0x58, ts) });
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
        file += track (ev);
    }
    return file;
}

} // namespace ans
