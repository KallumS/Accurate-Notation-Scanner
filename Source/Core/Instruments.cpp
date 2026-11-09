#include "Instruments.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

namespace ans::instruments
{

namespace
{
Instrument make (std::string id, std::string name, std::string shortName, std::string family, int low, int high,
                 int sweetLow, int sweetHigh, int program, Shape shape, int octave = 0, int transposition = 0, std::string inName = {})
{
    Instrument i;
    i.id = std::move (id); i.name = std::move (name); i.shortName = std::move (shortName); i.family = std::move (family);
    i.low = low; i.high = high; i.sweetLow = sweetLow; i.sweetHigh = sweetHigh;
    i.program = program; i.shape = shape;
    i.octave = octave; i.transposition = transposition; i.transposedName = std::move (inName);
    return i;
}

// Noterator's Instruments.cpp build(), the fields this app uses, unchanged.
std::vector<Instrument> build()
{
    using S = Shape;
    std::vector<Instrument> list {
        make ("vln1", "Violin I", "Vln. I", "Strings", 55, 100, 67, 91, 40, S::Strings),
        make ("vln2", "Violin II", "Vln. II", "Strings", 55, 96, 60, 84, 40, S::Strings),
        make ("vla", "Viola", "Vla.", "Strings", 48, 88, 52, 76, 41, S::Strings),
        make ("vc", "Cello", "Vc.", "Strings", 36, 81, 40, 67, 42, S::Strings),
        make ("cb", "Double Bass", "Cb.", "Strings", 28, 60, 28, 50, 43, S::Strings, 12),
        make ("picc", "Piccolo", "Picc.", "Woodwind", 74, 108, 79, 100, 72, S::Woodwinds, -12),
        make ("fl", "Flute", "Fl.", "Woodwind", 60, 96, 67, 91, 73, S::Woodwinds),
        make ("ob", "Oboe", "Ob.", "Woodwind", 58, 91, 62, 84, 68, S::Woodwinds),
        make ("eh", "English Horn", "E.H.", "Woodwind", 52, 81, 55, 74, 69, S::Woodwinds, 0, 7, "in F"),
        make ("cl", "Clarinet", "Cl.", "Woodwind", 50, 91, 55, 84, 71, S::Woodwinds, 0, 2, "in Bb"),
        make ("bcl", "Bass Clarinet", "B. Cl.", "Woodwind", 38, 74, 40, 65, 71, S::Woodwinds, 0, 2, "in Bb"),
        make ("bsn", "Bassoon", "Bsn.", "Woodwind", 34, 76, 38, 67, 70, S::Woodwinds),
        make ("cbsn", "Contrabassoon", "Cbsn.", "Woodwind", 22, 53, 26, 48, 70, S::Woodwinds, 12),
        make ("asax", "Alto Saxophone", "A. Sax.", "Woodwind", 49, 80, 53, 76, 65, S::Woodwinds, 0, 9, "in Eb"),
        make ("tsax", "Tenor Saxophone", "T. Sax.", "Woodwind", 44, 75, 48, 70, 66, S::Woodwinds, 12, 2, "in Bb"),
        make ("bsax", "Baritone Saxophone", "Bari. Sax.", "Woodwind", 36, 69, 40, 62, 67, S::Woodwinds, 12, 9, "in Eb"),
        make ("hn", "Horn", "Hn.", "Brass", 35, 77, 48, 72, 60, S::Brass, 0, 7, "in F"),
        make ("tpt", "Trumpet", "Tpt.", "Brass", 52, 82, 60, 79, 56, S::Brass, 0, 2, "in Bb"),
        make ("tbn", "Trombone", "Tbn.", "Brass", 40, 72, 45, 67, 57, S::Brass),
        make ("btbn", "Bass Trombone", "B. Tbn.", "Brass", 34, 65, 36, 60, 57, S::Brass),
        make ("tuba", "Tuba", "Tba.", "Brass", 28, 58, 31, 53, 58, S::Brass),
        make ("timp", "Timpani", "Timp.", "Percussion", 40, 56, 40, 56, 47, S::None),
        make ("glock", "Glockenspiel", "Glock.", "Percussion", 79, 108, 79, 103, 9, S::None, -24),
        make ("xyl", "Xylophone", "Xyl.", "Percussion", 65, 108, 67, 100, 13, S::None, -12),
        make ("mar", "Marimba", "Mar.", "Percussion", 45, 96, 48, 88, 12, S::None),
        make ("vib", "Vibraphone", "Vib.", "Percussion", 53, 89, 55, 84, 11, S::None),
        make ("harp", "Harp", "Hp.", "Keys & Harp", 23, 102, 36, 84, 46, S::None),
        make ("cel", "Celesta", "Cel.", "Keys & Harp", 60, 108, 67, 96, 8, S::None, -12),
        make ("pno", "Piano", "Pno.", "Keys & Harp", 21, 108, 43, 84, 0, S::None),
        make ("sop", "Soprano", "S.", "Voices", 60, 81, 64, 77, 52, S::Neutral),
        make ("alto", "Alto", "A.", "Voices", 53, 74, 57, 69, 52, S::Neutral),
        make ("ten", "Tenor", "T.", "Voices", 48, 69, 50, 65, 52, S::Neutral, 12),
        make ("bass", "Bass", "B.", "Voices", 40, 64, 43, 60, 52, S::Neutral),
        make ("gtr", "Guitar", "Gtr.", "Band", 40, 83, 45, 76, 24, S::None, 12),
    };
    // Noterator's first clef for each: treble unless named here
    for (auto& i : list)
        for (const auto& [id, c] : std::initializer_list<std::pair<const char*, char>> {
                 { "vla", 'C' }, { "vc", 'F' }, { "cb", 'F' }, { "bcl", 'F' }, { "bsn", 'F' }, { "cbsn", 'F' }, { "tbn", 'F' },
                 { "btbn", 'F' }, { "tuba", 'F' }, { "timp", 'F' }, { "bass", 'F' } })
            if (i.id == id) i.clef = c;
    return list;
}

// Names, then abbreviations, for each instrument. Longest are tried first,
// so "bass clarinet" wins over "clarinet" and "corno inglese" over "corno".
struct Alias { const char* text; const char* id; bool abbreviation; };
const std::vector<Alias>& aliases()
{
    static const std::vector<Alias> list = [] {
        std::vector<Alias> a;
        auto names = [&a] (const char* id, std::initializer_list<const char*> words) { for (auto* w : words) a.push_back ({ w, id, false }); };
        auto shorts = [&a] (const char* id, std::initializer_list<const char*> words) { for (auto* w : words) a.push_back ({ w, id, true }); };
        names ("vln1", { "violin", "violins", "violino", "violini", "violine", "violinen", "violon", "violons", "fiddle" });
        shorts ("vln1", { "vn", "vns", "vln", "vlns", "vl", "vi", "viol" });
        names ("vla", { "viola", "violas", "viole", "bratsche", "bratschen", "alto violin" });
        shorts ("vla", { "vla", "vlas", "va", "br" });
        names ("vc", { "violoncello", "violoncelli", "violoncelle", "violoncelles", "cello", "cellos", "celli" });
        shorts ("vc", { "vc", "vcs", "vcl", "vlc", "vcll" });
        names ("cb", { "double bass", "double basses", "contrabass", "contrabasses", "contrabbasso", "contrabbassi", "kontrabass",
                       "kontrabasse", "contrebasse", "contrebasses", "string bass", "basses", "bassi" });
        shorts ("cb", { "cb", "kb", "db", "cbs" });
        names ("picc", { "piccolo", "piccolos", "ottavino", "kleine flote", "petite flute", "piccolo flute" });
        shorts ("picc", { "picc", "pic", "ott" });
        names ("fl", { "flute", "flutes", "flauto", "flauti", "flote", "floten", "grosse flote", "grande flute" });
        shorts ("fl", { "fl", "fls", "fll", "gr fl" });
        names ("ob", { "oboe", "oboes", "oboi", "hautbois", "hoboe", "hoboen" });
        shorts ("ob", { "ob", "obs", "hb" });
        names ("eh", { "english horn", "cor anglais", "corno inglese", "englischhorn", "englisch horn" });
        shorts ("eh", { "eh", "ca", "eng hn", "c ingl", "ehn" });
        names ("bcl", { "bass clarinet", "clarinetto basso", "clarinette basse", "bassklarinette", "clarone" });
        shorts ("bcl", { "bcl", "b cl", "bass cl", "bkl", "cl b" });
        names ("cl", { "clarinet", "clarinets", "clarinetto", "clarinetti", "clarinette", "clarinettes", "klarinette", "klarinetten" });
        shorts ("cl", { "cl", "cls", "clar", "kl", "klar", "clt" });
        names ("cbsn", { "contrabassoon", "contrabassoons", "contrafagotto", "kontrafagott", "contrebasson", "double bassoon" });
        shorts ("cbsn", { "cbsn", "cfg", "kfg", "cbn", "c fag" });
        names ("bsn", { "bassoon", "bassoons", "fagotto", "fagotti", "fagott", "fagotte", "basson", "bassons" });
        shorts ("bsn", { "bsn", "bsns", "fg", "fag", "bn" });
        names ("asax", { "alto saxophone", "alto sax", "saxophone alto", "altsaxophon" });
        shorts ("asax", { "asax", "a sax" });
        names ("tsax", { "tenor saxophone", "tenor sax", "saxophone tenor", "tenorsaxophon" });
        shorts ("tsax", { "tsax", "t sax" });
        names ("bsax", { "baritone saxophone", "baritone sax", "bari sax", "saxophone baryton", "baritonsaxophon" });
        shorts ("bsax", { "bsax", "bari sax", "bar sax" });
        names ("hn", { "horn", "horns", "french horn", "french horns", "corno", "corni", "cor", "cors", "waldhorn", "horner" });
        shorts ("hn", { "hn", "hns", "hr", "cr" });
        names ("tpt", { "trumpet", "trumpets", "tromba", "trombe", "trompete", "trompeten", "trompette", "trompettes",
                        "clarino", "clarini", "cornet", "cornets", "cornetto", "cornet a pistons" });
        shorts ("tpt", { "tpt", "tpts", "trp", "tr", "trpt", "tromp", "crt" });
        names ("btbn", { "bass trombone", "trombone basso", "bassposaune", "trombone basse" });
        shorts ("btbn", { "b tbn", "btbn", "b trb", "bpos" });
        names ("tbn", { "trombone", "trombones", "tromboni", "posaune", "posaunen" });
        shorts ("tbn", { "tbn", "tbns", "trb", "trbn", "pos" });
        names ("tuba", { "tuba", "tubas", "tube", "basstuba", "bass tuba" });
        shorts ("tuba", { "tba", "tb" });
        names ("timp", { "timpani", "timpano", "pauken", "pauke", "timbales", "kettledrums" });
        shorts ("timp", { "timp", "pk", "tymp", "timb" });
        names ("glock", { "glockenspiel", "campanelli", "jeu de timbres" });
        shorts ("glock", { "glock", "glk" });
        names ("xyl", { "xylophone", "xilofono", "xylophon" });
        shorts ("xyl", { "xyl", "xil" });
        names ("mar", { "marimba", "marimbaphone" });
        shorts ("mar", { "mar", "mrb" });
        names ("vib", { "vibraphone", "vibrafono", "vibraphon" });
        shorts ("vib", { "vib", "vibr" });
        names ("harp", { "harp", "harps", "arpa", "arpe", "harfe", "harfen", "harpe", "harpes" });
        shorts ("harp", { "hp", "hrp", "hf" });
        names ("cel", { "celesta", "celeste" });
        shorts ("cel", { "cel", "cels" });
        names ("pno", { "piano", "pianoforte", "klavier", "piano forte", "fortepiano" });
        shorts ("pno", { "pno", "pf", "pft", "pfte", "klav" });
        names ("sop", { "soprano", "sopran", "sopranos", "soprani" });
        shorts ("sop", { "sopr", "sop" });
        names ("alto", { "alto", "alt", "contralto", "altos", "alti" });
        names ("ten", { "tenor", "tenore", "tenors", "tenori", "tenore" });
        shorts ("ten", { "ten" });
        names ("bass", { "bass", "basso", "bariton", "baritone", "baritono" });
        names ("gtr", { "guitar", "guitars", "gitarre", "chitarra", "guitare" });
        shorts ("gtr", { "gtr", "git", "guit" });
        std::stable_sort (a.begin(), a.end(), [] (const Alias& x, const Alias& y) { return std::string (x.text).size() > std::string (y.text).size(); });
        return a;
    }();
    return list;
}

// Lower case, accents off, signs to letters, everything else a space.
std::string normalise (const std::string& s)
{
    static const std::pair<const char*, const char*> folds[] = {
        { "\xc3\xa4", "a" }, { "\xc3\xb6", "o" }, { "\xc3\xbc", "u" }, { "\xc3\x84", "a" }, { "\xc3\x96", "o" }, { "\xc3\x9c", "u" },
        { "\xc3\xa9", "e" }, { "\xc3\xa8", "e" }, { "\xc3\xaa", "e" }, { "\xc3\x89", "e" }, { "\xc3\xa0", "a" }, { "\xc3\xa2", "a" },
        { "\xc3\xa7", "c" }, { "\xc3\xae", "i" }, { "\xc3\xb4", "o" }, { "\xc3\xbb", "u" }, { "\xc3\x9f", "ss" },
        { "\xe2\x99\xad", "b" }, { "\xe2\x99\xaf", "#" }, { "\xe2\x99\xae", "" },
    };
    std::string t = s;
    for (const auto& [from, to] : folds)
        for (size_t at = t.find (from); at != std::string::npos; at = t.find (from, at))
            t.replace (at, std::string (from).size(), to);
    std::string out;
    for (char c : t)
    {
        const auto u = static_cast<unsigned char> (c);
        if (std::isalpha (u)) out += static_cast<char> (std::tolower (u));
        else if (c == '#') out += '#';
        else if (std::isdigit (u)) out += c;
        else out += ' ';
    }
    // single spaces, padded
    std::string squeezed = " ";
    for (char c : out)
        if (c != ' ' || squeezed.back() != ' ') squeezed += c;
    if (squeezed.back() != ' ') squeezed += ' ';
    return squeezed;
}

// The pitch class of a key named after "in" / "en": English, German, Italian, French.
int keyPitch (const std::string& word, const std::string& next, int& used)
{
    used = 1;
    static const std::pair<const char*, int> solfege[] = { { "do", 0 }, { "ut", 0 }, { "re", 2 }, { "mi", 4 }, { "fa", 5 },
                                                            { "sol", 7 }, { "la", 9 }, { "si", 11 } };
    static const std::pair<const char*, int> german[] = { { "c", 0 }, { "cis", 1 }, { "des", 1 }, { "d", 2 }, { "dis", 3 }, { "es", 3 },
                                                          { "e", 4 }, { "f", 5 }, { "fis", 6 }, { "ges", 6 }, { "g", 7 }, { "gis", 8 },
                                                          { "as", 8 }, { "a", 9 }, { "ais", 10 }, { "b", 10 }, { "h", 11 } };
    auto alteration = [&] (const std::string& w) {
        if (w == "b" || w == "flat" || w == "bemolle" || w == "bemol" || w == "moll") return -1;
        if (w == "#" || w == "sharp" || w == "diesis" || w == "diese") return 1;
        return 0;
    };
    for (const auto& [n, pc] : solfege)
        if (word == n)
        {
            const int alt = alteration (next);
            if (alt != 0) used = 2;
            return (pc + alt + 12) % 12;
        }
    // English with a sign: "bb", "eb", "f#"
    if (word.size() == 2 && word[0] >= 'a' && word[0] <= 'g' && (word[1] == 'b' || word[1] == '#'))
    {
        static const int letters[7] = { 9, 11, 0, 2, 4, 5, 7 };
        return (letters[word[0] - 'a'] + (word[1] == '#' ? 1 : -1) + 12) % 12;
    }
    if (word.size() == 1 && word[0] >= 'a' && word[0] <= 'h' && alteration (next) != 0 && word != "b" && word != "h")
    {
        static const int letters[7] = { 9, 11, 0, 2, 4, 5, 7 };
        used = 2;
        return (letters[word[0] - 'a'] + alteration (next) + 12) % 12;
    }
    if (word == "b" && alteration (next) == -1) { used = 2; return 10; }
    for (const auto& [n, pc] : german)
        if (word == n) return pc;   // a lone "b" is German B, B-flat: no clarinet is in B natural
    return -1;
}

std::vector<std::string> words (const std::string& padded)
{
    std::vector<std::string> out;
    std::string w;
    for (char c : padded)
    {
        if (c == ' ') { if (! w.empty()) out.push_back (w); w.clear(); }
        else w += c;
    }
    return out;
}
} // namespace

const std::vector<Instrument>& all()
{
    static const std::vector<Instrument> list = build();
    return list;
}

const Instrument* byId (const std::string& id)
{
    for (const auto& i : all())
        if (i.id == id) return &i;
    return nullptr;
}

Identified identify (const std::string& label)
{
    Identified out;
    const auto text = normalise (label);
    if (text.size() <= 2) return out;
    const auto w = words (text);

    // "in B", "en Fa": the key, and what is left of the label before it
    std::string before = text;
    int keyPc = -1;
    bool basso = false;
    for (size_t i = 0; i + 1 < w.size(); ++i)
    {
        if (w[i] != "in" && w[i] != "en") continue;
        int used = 1;
        const int pc = keyPitch (w[i + 1], i + 2 < w.size() ? w[i + 2] : std::string(), used);
        if (pc < 0) continue;
        keyPc = pc;
        out.keyName = w[i + 1];
        for (size_t k = i + 2; k < w.size(); ++k) basso = basso || w[k] == "basso" || w[k] == "tief";
        std::string b = " ";
        for (size_t k = 0; k < i; ++k) b += w[k] + " ";
        before = b;
        break;
    }

    // a single letter is a voice only when it is the whole label
    const auto plain = words (before);
    if (plain.size() == 1 && plain[0].size() == 1)
    {
        static const std::pair<char, const char*> voices[] = { { 's', "sop" }, { 'a', "alto" }, { 't', "ten" }, { 'b', "bass" } };
        for (const auto& [c, id] : voices)
            if (plain[0][0] == c) out.instrument = byId (id);
        return out;
    }
    for (const auto& a : aliases())
    {
        const std::string needle = std::string (" ") + a.text + " ";
        if (before.find (needle) == std::string::npos) continue;
        out.instrument = byId (a.id);
        out.fullName = ! a.abbreviation;
        break;
    }
    if (out.instrument == nullptr) return out;

    // the second violins: "Violino II", "2nd Violin", "Violinen 2"
    if (out.instrument->id == "vln1")
        for (const auto& x : plain)
            if (x == "ii" || x == "2" || x == "2nd" || x == "second" || x == "secondo" || x == "secondi" || x == "zweite" || x == "seconds")
                out.instrument = byId ("vln2");

    out.transposition = out.instrument->transposition;
    if (keyPc >= 0)
    {
        const auto& id = out.instrument->id;
        int t = 0;
        bool transposes = true;
        if (id == "hn")
        {
            t = (12 - keyPc) % 12;               // horns sound below, in every key
            if (t == 0) t = 12;                  // horn in C: C basso, an octave down
            if (basso && t < 12) t += 12;
        }
        else if (id == "cl" || id == "bcl" || id == "tpt")
            t = keyPc >= 9 ? 12 - keyPc : -keyPc;   // in Bb or A below; in C, D, Eb, E, F above
        else if (id == "eh" || id == "asax" || id == "bsax" || id == "tsax")
            t = (12 - keyPc) % 12;
        else
            transposes = false;                  // timpani "in D-A" is a tuning, not a transposition
        if (transposes)
        {
            out.keyStated = true;
            out.transposition = t;
        }
    }
    return out;
}

int diatonicFor (int semitones)
{
    static constexpr int steps[12] = { 0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6 };
    const int s = ((semitones % 12) + 12) % 12;
    return steps[s] + 7 * static_cast<int> (std::floor (semitones / 12.0));
}

} // namespace ans::instruments
