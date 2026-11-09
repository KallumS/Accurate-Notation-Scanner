// Instruments named by the labels printed in real scores. Expected answers are
// the standard ones of the orchestration literature (what each name means,
// which way and how far each instrument in each key sounds from its written
// note), written here by hand.

#include "Check.h"
#include "../Source/Core/Instruments.h"

#include <string>

using namespace ans::instruments;

namespace
{
std::string idOf (const std::string& label)
{
    const auto r = identify (label);
    return r.instrument == nullptr ? "-" : r.instrument->id;
}
int transposition (const std::string& label) { return identify (label).transposition; }
} // namespace

TEST ("Instruments: names in English, Italian, German and French, full and abbreviated")
{
    CHECK_EQ (idOf ("Violin I"), std::string ("vln1"));
    CHECK_EQ (idOf ("Violino I."), std::string ("vln1"));
    CHECK_EQ (idOf ("Violino II."), std::string ("vln2"));
    CHECK_EQ (idOf ("2nd Violins"), std::string ("vln2"));
    CHECK_EQ (idOf ("Vl. II"), std::string ("vln2"));
    CHECK_EQ (idOf ("Viola."), std::string ("vla"));
    CHECK_EQ (idOf ("Bratsche"), std::string ("vla"));
    CHECK_EQ (idOf ("Violoncello."), std::string ("vc"));
    CHECK_EQ (idOf ("Vc."), std::string ("vc"));
    CHECK_EQ (idOf ("Contrabbasso"), std::string ("cb"));
    CHECK_EQ (idOf ("Kontrabass"), std::string ("cb"));
    CHECK_EQ (idOf ("Flauto"), std::string ("fl"));
    CHECK_EQ (idOf ("Fl\xc3\xb6te"), std::string ("fl"));        // Flöte
    CHECK_EQ (idOf ("Hautbois"), std::string ("ob"));
    CHECK_EQ (idOf ("Corno inglese"), std::string ("eh"));
    CHECK_EQ (idOf ("Bass Clarinet in B\xe2\x99\xad"), std::string ("bcl"));
    CHECK_EQ (idOf ("Fagotti"), std::string ("bsn"));
    CHECK_EQ (idOf ("Contrafagotto"), std::string ("cbsn"));
    CHECK_EQ (idOf ("2 H\xc3\xb6rner in F"), std::string ("hn"));  // Hörner
    CHECK_EQ (idOf ("Trombe in D"), std::string ("tpt"));
    CHECK_EQ (idOf ("Posaune"), std::string ("tbn"));
    CHECK_EQ (idOf ("Pauken"), std::string ("timp"));
    CHECK_EQ (idOf ("Arpa"), std::string ("harp"));
    CHECK_EQ (idOf ("Pianoforte"), std::string ("pno"));
    CHECK_EQ (idOf ("Tenore"), std::string ("ten"));
    CHECK_EQ (idOf ("S."), std::string ("sop"));
    // a label that names nothing this table has names nothing
    CHECK_EQ (idOf ("Singstimme"), std::string ("-"));
    CHECK_EQ (idOf ("Allegro"), std::string ("-"));
}

TEST ("Instruments: a full name is told from an abbreviation")
{
    CHECK (identify ("Violino I.").fullName);
    CHECK (identify ("Viola.").fullName);
    CHECK (! identify ("Vl.").fullName);
    CHECK (! identify ("Fag.").fullName);
}

TEST ("Instruments: the key an instrument is in gives its transposition (written above sounding)")
{
    CHECK_EQ (transposition ("Clarinet in B\xe2\x99\xad"), 2);    // sounds a major second lower
    CHECK_EQ (transposition ("Klarinette in B"), 2);                 // German B is B-flat
    CHECK_EQ (transposition ("Clarinetto in La"), 3);                // A: a minor third lower
    CHECK_EQ (transposition ("Clarinet in E\xe2\x99\xad"), -3);    // the small E-flat clarinet sounds a minor third higher
    CHECK_EQ (transposition ("Clarinet in C"), 0);
    CHECK_EQ (transposition ("Horn in F"), 7);                       // a perfect fifth lower
    CHECK_EQ (transposition ("Corni in Es"), 9);                     // E-flat: a major sixth lower
    CHECK_EQ (transposition ("Corno in D"), 10);                     // a minor seventh lower
    CHECK_EQ (transposition ("Corni in C"), 12);                     // C basso: an octave lower
    CHECK_EQ (transposition ("Trumpet in B flat"), 2);
    CHECK_EQ (transposition ("Trombe in D"), -2);                    // a major second higher
    CHECK_EQ (transposition ("Trompette en Fa"), -5);                // a perfect fourth higher
    CHECK_EQ (transposition ("Cor anglais"), 7);                     // always in F
    CHECK (identify ("Clarinetto in La").keyStated);
    CHECK (! identify ("Clarinet").keyStated);                       // not stated: the table's B-flat, not read
    CHECK (! identify ("Timpani in D, A").keyStated);                // a tuning, not a transposition
}
