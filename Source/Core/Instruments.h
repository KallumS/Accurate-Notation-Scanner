/*
    Instruments - who a part is, from the name printed beside it.

    The table is Noterator's (KallumS/Noterator, Source/Core/Instruments.cpp),
    number for number: practical range and sweet spot (sounding), General
    MIDI program, how it is written against how it sounds, and which AutoCC
    shape suits it. Keep it alike with Noterator's, as Xml.* is.

    What this file adds is reading a printed label in any of the languages
    scores are printed in: Noterator's own part-name words, Orchestration
    Helper's aliases (KallumS/Orchestration-Helper, orchestration_data.lua),
    and the standard Italian, German and French names and abbreviations
    ("Violoncello", "Vc.", "Klarinette in B", "Cor. en Fa"), with the key an
    instrument is "in" read from the label too. A label that names no
    instrument names none: nothing is assumed.
*/

#pragma once

#include <string>
#include <vector>

namespace ans::instruments
{

enum class Shape { None, Neutral, Strings, Brass, Woodwinds };   // AutoCC's presets

struct Instrument
{
    std::string id;               // Noterator's: "vln1", "cl", "hn"...
    std::string name, shortName;  // English, as Noterator writes them
    std::string family;           // Strings, Woodwind, Brass, Percussion, Keys & Harp, Voices, Band
    int low = 0, high = 127;      // practical range, sounding (MIDI)
    int sweetLow = 0, sweetHigh = 127;
    int program = 0;              // General MIDI, 0-based
    int octave = 0;               // written = sounding + octave, always (double bass +12, piccolo -12)
    int transposition = 0;        // and + this as well, in a transposed score (clarinet in B-flat +2)
    std::string transposedName;   // "in Bb"
    Shape shape = Shape::None;
    char clef = 'G';              // the clef Noterator writes it in (its first staff): G, F or C;
                                  // the octave applies to a page in that clef with no octave mark
};

const std::vector<Instrument>& all();
const Instrument* byId (const std::string& id);

struct Identified
{
    const Instrument* instrument = nullptr;   // null: the label names no instrument this table has
    bool fullName = false;        // the label spells a name out ("Violino I."), not only an abbreviation ("Vl.")
    bool keyStated = false;       // "in B", "in A", "en Fa" was printed
    int transposition = 0;        // for that key, as Instrument::transposition; the table's when none was stated
    std::string keyName;          // "Bb", "A", "F"... as stated
};

// Who the label names: "Violino I.", "Clarinetto in La", "2 Hörner in F".
Identified identify (const std::string& label);

// MusicXML's <transpose> for writing `semitones` above sounding: the interval
// as steps of the scale (a major second is 1, a perfect fifth 4).
int diatonicFor (int semitones);

} // namespace ans::instruments
