#include "MusicXml.h"

#include "Instruments.h"

#include "Xml.h"

#include <algorithm>
#include <map>
#include <numeric>

namespace ans
{

namespace
{
const char* stepName (int step) { static const char* n[] = { "C", "D", "E", "F", "G", "A", "B" }; return n[((step % 7) + 7) % 7]; }

const char* typeName (int type)
{
    switch (type)
    {
        case -3: return "maxima";
        case -2: return "long";
        case -1: return "breve";
        case 0: return "whole";
        case 1: return "half";
        case 2: return "quarter";
        case 3: return "eighth";
        case 4: return "16th";
        case 5: return "32nd";
        case 6: return "64th";
        case 7: return "128th";
        case 8: return "256th";
        case 9: return "512th";
        default: return "1024th";
    }
}

const char* accidentalName (int a)
{
    switch (a)
    {
        case -2: return "flat-flat";
        case -1: return "flat";
        case 1: return "sharp";
        case 2: return "double-sharp";
        default: return "natural";
    }
}

const char* barStyleName (BarStyle s)
{
    switch (s)
    {
        case BarStyle::LightLight: return "light-light";
        case BarStyle::LightHeavy: return "light-heavy";
        case BarStyle::HeavyLight: return "heavy-light";
        case BarStyle::Heavy: return "heavy";
        case BarStyle::Dashed: return "dashed";
        case BarStyle::Regular: break;
    }
    return "regular";
}

const char* beamName (int mark)
{
    switch (mark)
    {
        case 1: return "begin";
        case 2: return "continue";
        case 3: return "end";
        case 4: return "forward hook";
        case 5: return "backward hook";
        default: return nullptr;
    }
}

void writeClef (xml::Writer& w, const Clef& c, int number, int staves)
{
    xml::Writer::Attributes a;
    if (staves > 1) a.push_back ({ "number", std::to_string (number) });
    w.open ("clef", a);
    switch (c.sign)
    {
        case ClefSign::G: w.leaf ("sign", "G"); break;
        case ClefSign::F: w.leaf ("sign", "F"); break;
        case ClefSign::C: w.leaf ("sign", "C"); break;
        case ClefSign::Percussion: w.leaf ("sign", "percussion"); break;
        case ClefSign::Tab: w.leaf ("sign", "TAB"); break;
    }
    if (c.sign != ClefSign::Percussion) w.leaf ("line", c.line);
    if (c.octave != 0) w.leaf ("clef-octave-change", c.octave);
    w.close();
}

long long lcm (long long a, long long b) { return a / std::gcd (a, b) * b; }

std::vector<std::string> split (const std::string& s, char sep)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : s)
    {
        if (c == sep) { out.push_back (cur); cur.clear(); }
        else cur += c;
    }
    out.push_back (cur);
    return out;
}
} // namespace

long long divisionsFor (const Score& score)
{
    long long d = 1;
    for (const auto& p : score.parts)
        for (const auto& m : p.measures)
        {
            for (const auto& e : m.events) { d = lcm (d, e.duration.d); d = lcm (d, e.offset.d); }
            for (const auto& x : m.directions) d = lcm (d, x.offset.d);
            for (const auto& c : m.clefs) d = lcm (d, c.offset.d);
        }
    return d;
}

std::string toMusicXml (const Score& score)
{
    xml::Writer w ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
                   "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\" \"http://www.musicxml.org/dtds/partwise.dtd\">\n");
    const long long div = divisionsFor (score);
    auto ticks = [&] (Fraction f) { return f.n * (div / f.d); };

    auto writeEnding = [&] (const Barline& b) {
        if (b.ending == Barline::Ending::None) return;
        const char* type = b.ending == Barline::Ending::Start ? "start" : b.ending == Barline::Ending::Stop ? "stop" : "discontinue";
        if (b.ending == Barline::Ending::Start && ! b.endingText.empty())
            w.leaf ("ending", b.endingText, { { "number", b.endingNumber }, { "type", type } });
        else
            w.empty ("ending", { { "number", b.endingNumber }, { "type", type } });
    };

    w.open ("score-partwise", { { "version", "4.0" } });
    if (! score.title.empty())
    {
        w.open ("work");
        w.leaf ("work-title", score.title);
        w.close();
    }
    w.open ("identification");
    if (! score.composer.empty()) w.leaf ("creator", score.composer, { { "type", "composer" } });
    w.open ("encoding");
    w.leaf ("software", "Accurate Notation Scanner");
    w.close();
    w.close();

    w.open ("part-list");
    for (size_t p = 0; p < score.parts.size(); ++p)
    {
        w.open ("score-part", { { "id", "P" + std::to_string (p + 1) } });
        w.leaf ("part-name", score.parts[p].name.empty() ? "Part " + std::to_string (p + 1) : score.parts[p].name);
        // the instrument, as Noterator names it, and its General MIDI sound
        if (const auto* inst = instruments::byId (score.parts[p].instrument))
        {
            const std::string id = "P" + std::to_string (p + 1) + "-I1";
            w.open ("score-instrument", { { "id", id } });
            w.leaf ("instrument-name", inst->name);
            w.close();
            w.open ("midi-instrument", { { "id", id } });
            w.leaf ("midi-program", inst->program + 1);
            w.close();
        }
        w.close();
    }
    w.close();

    for (size_t p = 0; p < score.parts.size(); ++p)
    {
        const auto& part = score.parts[p];
        w.open ("part", { { "id", "P" + std::to_string (p + 1) } });
        int slurBase = 0;
        for (size_t b = 0; b < part.measures.size(); ++b)
        {
            const auto& m = part.measures[b];
            xml::Writer::Attributes ma { { "number", m.number } };
            if (m.implicit) ma.push_back ({ "implicit", "yes" });
            w.open ("measure", ma);

            if (m.hasLeft)
            {
                w.open ("barline", { { "location", "left" } });
                w.leaf ("bar-style", barStyleName (m.left.style));
                writeEnding (m.left);
                if (m.left.repeat) w.empty ("repeat", { { "direction", "forward" } });
                w.close();
            }

            std::vector<const ClefChange*> openingClefs, laterClefs;
            for (const auto& c : m.clefs) (c.offset == Fraction (0) ? openingClefs : laterClefs).push_back (&c);

            if (b == 0 || m.key || m.time || ! openingClefs.empty() || m.multiRest > 1)
            {
                w.open ("attributes");
                if (b == 0) w.leaf ("divisions", div);
                if (m.key)
                {
                    w.open ("key");
                    w.leaf ("fifths", *m.key);
                    w.close();
                }
                if (m.time)
                {
                    xml::Writer::Attributes ta;
                    if (m.time->symbol == TimeSignature::Common) ta.push_back ({ "symbol", "common" });
                    if (m.time->symbol == TimeSignature::Cut) ta.push_back ({ "symbol", "cut" });
                    w.open ("time", ta);
                    w.leaf ("beats", m.time->beats);
                    w.leaf ("beat-type", m.time->beatType);
                    w.close();
                }
                if (b == 0 && part.staves > 1) w.leaf ("staves", part.staves);
                for (const auto* c : openingClefs) writeClef (w, c->clef, c->staff, part.staves);
                // written above sounding: Noterator's <transpose>, as it writes and reads it
                if (b == 0 && part.transpose != 0)
                {
                    const int octaves = part.transpose / 12, rest = part.transpose - 12 * octaves;
                    w.open ("transpose");
                    w.leaf ("diatonic", -instruments::diatonicFor (rest));
                    w.leaf ("chromatic", -rest);
                    if (octaves != 0) w.leaf ("octave-change", -octaves);
                    w.close();
                }
                if (m.multiRest > 1)
                {
                    w.open ("measure-style");
                    w.leaf ("multiple-rest", m.multiRest);
                    w.close();
                }
                w.close();
            }

            // Staff by staff, voice by voice.
            std::map<std::pair<int, int>, std::vector<const Event*>> lanes;
            for (const auto& e : m.events) lanes[{ e.staff, e.voice }].push_back (&e);
            for (auto& [k, list] : lanes)
                std::stable_sort (list.begin(), list.end(), [] (const Event* a, const Event* c) { return a->offset < c->offset; });

            Fraction pos;
            bool firstLane = true;
            std::vector<bool> directionDone (m.directions.size(), false);
            std::vector<bool> clefDone (laterClefs.size(), false);
            int lastStaff = 0;
            auto writeDirections = [&] (int staff, Fraction at, bool flushAll) {
                for (size_t i = 0; i < m.directions.size(); ++i)
                {
                    const auto& d = m.directions[i];
                    if (directionDone[i] || d.staff != staff || (! flushAll && d.offset > at)) continue;
                    directionDone[i] = true;
                    w.open ("direction", { { "placement", d.below ? "below" : "above" } });
                    w.open ("direction-type");
                    if (! d.dynamics.empty())
                    {
                        w.open ("dynamics");
                        if (d.dynamics.rfind ("other:", 0) == 0) w.leaf ("other-dynamics", d.dynamics.substr (6));
                        else w.empty (d.dynamics);
                        w.close();
                    }
                    else
                        w.leaf ("words", d.words);
                    w.close();
                    if (d.offset != pos) w.leaf ("offset", ticks (d.offset - pos));
                    if (part.staves > 1) w.leaf ("staff", d.staff);
                    w.close();
                }
            };
            auto writeClefChanges = [&] (int staff, Fraction at, bool flushAll) {
                for (size_t i = 0; i < laterClefs.size(); ++i)
                {
                    if (clefDone[i] || laterClefs[i]->staff != staff || (! flushAll && laterClefs[i]->offset > at)) continue;
                    clefDone[i] = true;
                    w.open ("attributes");
                    writeClef (w, laterClefs[i]->clef, staff, part.staves);
                    w.close();
                }
            };

            for (const auto& [key, list] : lanes)
            {
                const int staff = key.first, voice = key.second;
                const int voiceNumber = (staff - 1) * 4 + voice;
                if (! firstLane && pos > Fraction (0))
                {
                    w.open ("backup");
                    w.leaf ("duration", ticks (pos));
                    w.close();
                    pos = Fraction (0);
                }
                const bool directionsHere = staff != lastStaff;
                lastStaff = staff;
                firstLane = false;
                for (size_t i = 0; i < list.size(); ++i)
                {
                    const Event& e = *list[i];
                    if (! e.grace && e.offset > pos)
                    {
                        w.open ("forward");
                        w.leaf ("duration", ticks (e.offset - pos));
                        w.leaf ("voice", voiceNumber);
                        if (part.staves > 1) w.leaf ("staff", staff);
                        w.close();
                        pos = e.offset;
                    }
                    if (directionsHere && ! e.grace)
                    {
                        writeClefChanges (staff, pos, false);
                        writeDirections (staff, pos, false);
                    }

                    const size_t noteCount = e.rest ? 1 : e.notes.size();
                    for (size_t n = 0; n < noteCount; ++n)
                    {
                        w.open ("note", e.hidden ? xml::Writer::Attributes { { "print-object", "no" } } : xml::Writer::Attributes {});
                        if (e.grace) w.empty ("grace", e.graceSlash ? xml::Writer::Attributes { { "slash", "yes" } } : xml::Writer::Attributes {});
                        if (n > 0) w.empty ("chord");
                        const Note* note = e.rest ? nullptr : &e.notes[n];
                        if (e.rest)
                        {
                            if (e.measureRest) w.empty ("rest", { { "measure", "yes" } });
                            else w.empty ("rest");
                        }
                        else
                        {
                            w.open ("pitch");
                            w.leaf ("step", stepName (note->pitch.step));
                            if (note->pitch.alter != 0) w.leaf ("alter", note->pitch.alter);
                            w.leaf ("octave", note->pitch.octave);
                            w.close();
                        }
                        if (! e.grace) w.leaf ("duration", ticks (e.duration));
                        if (note != nullptr && note->tieStop) w.empty ("tie", { { "type", "stop" } });
                        if (note != nullptr && note->tieStart) w.empty ("tie", { { "type", "start" } });
                        w.leaf ("voice", voiceNumber);
                        if (! e.measureRest)
                        {
                            w.leaf ("type", typeName (e.type));
                            for (int d = 0; d < e.dots; ++d) w.empty ("dot");
                        }
                        if (note != nullptr && note->accidentalShown)
                            w.leaf ("accidental", accidentalName (note->accidental),
                                    note->cautionary ? xml::Writer::Attributes { { "cautionary", "yes" } } : xml::Writer::Attributes {});
                        if (e.tuplet.active())
                        {
                            w.open ("time-modification");
                            w.leaf ("actual-notes", e.tuplet.actual);
                            w.leaf ("normal-notes", e.tuplet.normal);
                            w.close();
                        }
                        if (! e.rest && e.stem != 0) w.leaf ("stem", e.stem > 0 ? "up" : "down");
                        if (note != nullptr && ! note->notehead.empty()) w.leaf ("notehead", note->notehead);
                        if (part.staves > 1) w.leaf ("staff", staff);
                        if (n == 0)
                            for (size_t level = 0; level < e.beams.size(); ++level)
                                if (const char* bn = beamName (e.beams[level]))
                                    w.leaf ("beam", bn, { { "number", std::to_string (level + 1) } });

                        // notations
                        std::vector<std::string> arts, orns, techs, fermatas, slurs;
                        if (n == 0)
                            for (const auto& a : e.articulations)
                            {
                                const auto parts = split (a, ':');
                                if (parts[0] == "articulations") for (const auto& x : split (parts[1], '+')) arts.push_back (x);
                                else if (parts[0] == "ornaments") orns.push_back (parts[1]);
                                else if (parts[0] == "technical") techs.push_back (parts[1]);
                                else if (parts[0] == "fermata") fermatas.push_back (parts[1]);
                                else if (parts[0] == "slur") slurs.push_back (a);
                            }
                        const bool tied = note != nullptr && (note->tieStart || note->tieStop);
                        const bool tuplet = n == 0 && e.tuplet.active() && (e.tuplet.start || e.tuplet.stop);
                        const bool tremolo = n == 0 && e.tremolo > 0 && e.tremoloType > 0;
                        if (tied || tuplet || tremolo || ! arts.empty() || ! orns.empty() || ! techs.empty() || ! fermatas.empty() || ! slurs.empty())
                        {
                            w.open ("notations");
                            if (note != nullptr && note->tieStop) w.empty ("tied", { { "type", "stop" } });
                            if (note != nullptr && note->tieStart) w.empty ("tied", { { "type", "start" } });
                            for (const auto& s : slurs)
                            {
                                const auto bits = split (s, ':');
                                const int number = (std::stoi (bits[2]) + slurBase) % 6 + 1;
                                w.empty ("slur", { { "type", bits[1] }, { "number", std::to_string (number) } });
                            }
                            if (tuplet)
                            {
                                if (e.tuplet.start) w.empty ("tuplet", { { "type", "start" }, { "bracket", e.tuplet.bracket ? "yes" : "no" } });
                                if (e.tuplet.stop) w.empty ("tuplet", { { "type", "stop" } });
                            }
                            if (! arts.empty())
                            {
                                w.open ("articulations");
                                for (const auto& a : arts) w.empty (a);
                                w.close();
                            }
                            if (! orns.empty() || tremolo)
                            {
                                w.open ("ornaments");
                                for (const auto& a : orns) w.empty (a);
                                if (tremolo)
                                    w.leaf ("tremolo", e.tremolo, { { "type", e.tremoloType == 1 ? "single" : e.tremoloType == 2 ? "start" : "stop" } });
                                w.close();
                            }
                            if (! techs.empty())
                            {
                                w.open ("technical");
                                for (const auto& a : techs) w.empty (a);
                                w.close();
                            }
                            for (const auto& f : fermatas) w.empty ("fermata", { { "type", f } });
                            w.close();
                        }
                        w.close();
                    }
                    if (! e.grace) pos += e.duration;
                }
                if (directionsHere)
                {
                    writeClefChanges (staff, pos, true);
                    writeDirections (staff, pos, true);
                }
            }
            for (size_t i = 0; i < m.directions.size(); ++i)
                if (! directionDone[i]) writeDirections (m.directions[i].staff, pos, true);
            for (size_t i = 0; i < laterClefs.size(); ++i)
                if (! clefDone[i]) writeClefChanges (laterClefs[i]->staff, pos, true);

            if (m.right.style != BarStyle::Regular || m.right.repeat || m.right.ending != Barline::Ending::None)
            {
                w.open ("barline", { { "location", "right" } });
                w.leaf ("bar-style", barStyleName (m.right.style));
                writeEnding (m.right);
                if (m.right.repeat) w.empty ("repeat", { { "direction", "backward" } });
                w.close();
            }
            w.close();
            (void) slurBase;
        }
        w.close();
    }
    w.close();
    return w.str();
}

} // namespace ans
