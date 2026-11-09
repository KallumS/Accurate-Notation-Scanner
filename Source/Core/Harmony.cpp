#include "Harmony.h"

#include "Chords.h"
#include "Midi.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace ans
{

namespace
{
struct Sounding
{
    int bar = 0;
    Fraction onset, length;     // in the bar
    Fraction at;                // from the start of the piece
    int pitch = 0;              // MIDI
    bool embellishing = false;
};

// One note of a melodic line: a single-note event, or a held note's
// continuation joined onto it.
struct LineNote { size_t sounding; Fraction at, end; int pitch; int bar; Fraction onset; };

Fraction beatOf (const TimeSignature& t)
{
    if (t.beatType == 8 && t.beats % 3 == 0) return Fraction (3, 2);
    return Fraction (4, t.beatType);
}
} // namespace

std::vector<OutlinedChord> outlineChords (const Score& score)
{
    std::vector<OutlinedChord> out;
    if (score.parts.empty()) return out;
    const size_t bars = score.parts.front().measures.size();

    // Each bar's time signature, key, length (its longest voice, so a pickup
    // is short) and start.
    std::vector<TimeSignature> time (bars);
    std::vector<int> key (bars, 0);
    std::vector<Fraction> length (bars), start (bars);
    {
        TimeSignature t;
        int k = 0;
        Fraction at;
        for (size_t b = 0; b < bars; ++b)
        {
            for (const auto& part : score.parts)
            {
                if (b >= part.measures.size()) continue;
                if (part.measures[b].time) t = *part.measures[b].time;
                if (part.measures[b].key && &part == &score.parts.front()) k = *part.measures[b].key;
            }
            time[b] = t;
            key[b] = k;
            Fraction longest;
            for (const auto& part : score.parts)
                if (b < part.measures.size())
                    for (const auto& e : part.measures[b].events)
                        if (! e.grace && e.offset + e.duration > longest) longest = e.offset + e.duration;
            length[b] = longest > Fraction (0) ? longest : t.length();
            start[b] = at;
            at += length[b];
        }
    }

    // Every sounding note, and each line's notes in order.
    std::vector<Sounding> notes;
    std::map<std::tuple<size_t, int, int>, std::vector<LineNote>> lines;   // (part, staff, voice)
    for (size_t p = 0; p < score.parts.size(); ++p)
        for (size_t b = 0; b < bars && b < score.parts[p].measures.size(); ++b)
            for (const auto& e : score.parts[p].measures[b].events)
            {
                if (e.grace || e.hidden) continue;
                auto& line = lines[{ p, e.staff, e.voice }];
                const Fraction at = start[b] + e.offset;
                if (e.rest)
                {
                    line.push_back ({ SIZE_MAX, at, at + e.duration, -1, static_cast<int> (b), e.offset });
                    continue;
                }
                for (const auto& n : e.notes)
                    notes.push_back ({ static_cast<int> (b), e.offset, e.duration, at, midiPitch (n.pitch), false });
                if (e.notes.size() == 1)
                {
                    const int pitch = midiPitch (e.notes.front().pitch);
                    // a tied note goes on sounding: one note of the line
                    if (e.notes.front().tieStop && ! line.empty() && line.back().pitch == pitch && line.back().end == at)
                        line.back().end = at + e.duration;
                    else
                        line.push_back ({ notes.size() - 1, at, at + e.duration, pitch, static_cast<int> (b), e.offset });
                }
                else
                    line.push_back ({ SIZE_MAX, at, at + e.duration, -2, static_cast<int> (b), e.offset });   // a chord: not a line note
            }

    // Passing and neighbour notes (Open Music Theory, "Embellishing tones"),
    // off the beat, joined to the notes on both sides.
    for (auto& [id, line] : lines)
    {
        std::sort (line.begin(), line.end(), [] (const LineNote& a, const LineNote& c) { return a.at < c.at; });
        for (size_t k = 1; k + 1 < line.size(); ++k)
        {
            const auto& a = line[k - 1];
            const auto& x = line[k];
            const auto& c = line[k + 1];
            if (x.sounding == SIZE_MAX || a.pitch < 0 || c.pitch < 0) continue;
            if (a.end != x.at || x.end != c.at) continue;
            if ((x.onset / beatOf (time[static_cast<size_t> (x.bar)])).d == 1) continue;   // on the beat: kept
            const int in = x.pitch - a.pitch, outI = c.pitch - x.pitch;
            const bool stepIn = in != 0 && std::abs (in) <= 2, stepOut = outI != 0 && std::abs (outI) <= 2;
            const bool passing = stepIn && stepOut && (in > 0) == (outI > 0);
            const bool neighbour = stepIn && stepOut && a.pitch == c.pitch;
            if (passing || neighbour) notes[x.sounding].embellishing = true;
        }
    }

    // Each part of each bar, named.
    std::vector<std::vector<const Sounding*>> inBar (bars);
    for (const auto& n : notes)
        if (! n.embellishing) inBar[static_cast<size_t> (n.bar)].push_back (&n);
    for (size_t b = 0; b < bars; ++b)
    {
        const Fraction beat = beatOf (time[b]);
        const Fraction len = length[b];
        const long long beats = (len / beat).n / std::max (1LL, (len / beat).d);
        const Fraction step = (beats > 1 && beats % 2 == 0) ? len / Fraction (2) : beat;
        const auto keyScale = chords::Scale::majorKeyOf (key[b]);
        std::string previous;
        for (Fraction a; a < len; a += step)
        {
            const Fraction z = std::min (len, a + step);
            std::vector<int> chosen;
            std::set<int> pcs;
            for (const auto* n : inBar[b])
            {
                const Fraction overlap = std::min (n->onset + n->length, z) - std::max (n->onset, a);
                if (overlap <= Fraction (0)) continue;
                chosen.push_back (n->pitch);
                pcs.insert (n->pitch % 12);
            }
            if (pcs.size() < 2) continue;
            const auto name = chords::name (chosen, keyScale);
            if (! chords::isChordSymbol (name) || name == previous) continue;
            out.push_back ({ static_cast<int> (b), a, name });
            previous = name;
        }
    }
    return out;
}

} // namespace ans
