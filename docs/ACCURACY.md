# How accurate it is

Measured on 2026-11-09 by engraving large collections of real music into
PDFs, reading the PDFs back with the app, and comparing every note with what
the engraver was given. Nothing here was hand-picked: each collection was
run whole.

## How the truth is known

Each piece starts as a notation file. MuseScore 3 engraves it into a PDF
and, from the same layout, exports the MusicXML of exactly what it printed.
That MusicXML is the truth. The app reads only the PDF; then
`tools/compare.py` compares, bar by bar, every note's onset, spelled pitch
(F♯ is not G♭), length and tie, and every rest.

A note counts as right only if all of those match. Voices and beaming are
not compared (they are layout choices, not music).

`tools/bigcorpus.py` runs a whole collection; `tools/flagcheck.py` counts the
bars that were wrong **and not marked** - the errors a user would not be
told about, which matter most (decision 0002).

## Results

| Collection | Pieces read | Notes | Notes exactly right | Bars exactly right |
| --- | ---: | ---: | ---: | ---: |
| OpenScore **Lieder** (songs: voice and piano, 1820-1930) | 1,332 | 1,469,361 | **88.9%** | 84.1% |
| OpenScore **String Quartets** | 119 movements | 1,369,369 | **96.4%** | 94.7% |
| DCML **Mozart piano sonatas** | 58 movements | 121,607 | **96.4%** | 94.0% |
| DCML **Beethoven string quartets** | 70 movements | 277,897 | **99.4%** | 98.7% |
| music21: all 410 **Bach chorales** | 410 | 115,942 | **99.6%** | 99.7% |
| music21: **Palestrina** masses and motets | 150 | 88,963 | **99.98%** | 99.9% |
| music21: **Monteverdi** madrigals | 49 | 50,852 | **99.9%** | 99.8% |
| music21: **Trecento** (14th-century Italian) | 102 | 85,226 | **96.1%** | 98.6% |
| music21: folk tunes (Ryan's Mammoth Collection) | 150 | 17,187 | **99.8%** | 99.5% |
| music21: other (quartets, Lieder, piano, concerto) | 66 | 189,249 | **97.8%** | 98.5% |
| **All** | **2,506** | **3,785,653** | **94.0%** | |

Per piece in the Lieder: the median song is 95.4% right, 298 songs are 99%
or better, 116 are exact.

### What it marks, and what it does not

| Collection | Bars wrong | of which marked for you to check | not marked |
| --- | ---: | ---: | ---: |
| Lieder | 22,849 | 79% | 4,820 (2.0% of all bars) |
| String Quartets | 7,192 | 57% | 3,092 (1.0% of all bars) |
| Mozart sonatas | 843 | 81% | 160 (1.0%) |
| Beethoven quartets | 612 | 43% | 346 (0.5%) |
| music21 corpus | 1,374 | 47% | 732 (0.4%) |

The unmarked errors, sampled (1,046 bars from 200 songs):

| Kind | Share | What it means |
| --- | ---: | --- |
| A tie missing or extra | 41% | Every pitch and rhythm right; one note tied (or not) to the next. A slur between two equal notes is drawn exactly like a tie. |
| Notes extra | 22% | Mostly cross-staff writing in piano parts (a run passing from one staff to the other) read into the wrong part. |
| Notes missing | 12% | Notes the reader could not place (unusual stems, cue-size passages). |
| Wrong pitch | 7% | Almost all a knock-on of a tie: a tie carries an accidental over the barline. Since 2026-11-09 such bars are marked whenever the tie is the only reason for the accidental. |
| Other | 18% | Rests, onsets of a second voice starting mid-bar, lengths. |

### Files reported "every bar checked out"

In the music21 corpus, 773 pieces were reported clean and 742 of them (96%)
matched exactly.

Of the Lieder, 145 songs were reported clean; 94 matched exactly. The 51
others differed in 1,124 of their 44,082 notes (2.5%): mostly ties, and
rests in parts hidden for a system. In the Beethoven quartets the tie rule
above cut the "clean but different" movements from 17 to 2.

## What made the difference

Fixes found by these runs, each measured before and after:

| Fix | Effect |
| --- | --- |
| Bravura's optional glyphs (MuseScore's "oversized" noteheads) | Lieder sample 46% → 89%: noteheads in a third of the songs were not recognised |
| Staves hidden when empty: parts matched by name, abbreviation, staves and clefs | songs whose first system is piano alone stopped losing the singer |
| Triplets whose "3" was left off, read only when the bar adds up no other way | Lieder sample 89% → 92% |
| Time signature shared across parts | a part first shown later no longer defaults to 4/4 |
| Ties that carry an accidental are marked | wrong pitches no longer pass silently |

## LilyPond's own scores: the Mutopia Project

All 1,930 `.ly` files in the Mutopia Project archive were engraved with
LilyPond 2.24 (old files brought up to date with `convert-ly`); 1,584 gave
one PDF and one MIDI file. LilyPond's MIDI is the truth: every note's pitch
and start. It is a looser truth than MuseScore's MusicXML:

- **Lengths do not compare.** The MIDI plays staccato notes short and
  articulates others, so "right pitch, in sequence" is the fair measure.
- **Repeats.** Many files ask the MIDI to play the repeats; the read score
  is then played the same way, by the app's own MIDI export (0010).
- **Not the page.** 97 files make their MIDI from a separate, transposed
  copy of the music (Bach organ works doubled in octaves for a "registration",
  transposing instruments at concert pitch). They are left out. Chord
  symbols, which LilyPond's MIDI also plays, are silenced before engraving.
- Old sources brought up to date by `convert-ly` do not always engrave as
  their author meant (a symphony printed with its barlines missing); those
  pages are read as printed.

| | |
| --- | ---: |
| Pieces read (of 1,487) | 1,473 (1 refused, 13 could not be compared) |
| Notes | 1,352,906 |
| **Notes with the right pitch, in sequence** | **76.7%** |
| Median piece | 97.5% |
| Pieces 99% or better | 581 |
| Pieces under 3,000 notes (1,394) | 86.0% |
| Pieces of 3,000 notes or more (79: symphonies, concertos, opera) | 57.2% |
| Pieces reported "every bar checked out" (392) | 96.3% |

| Composer | Pieces | Notes | Right pitch, in sequence |
| --- | ---: | ---: | ---: |
| Beethoven | 66 | 260,970 | 51.4% |
| Bach | 299 | 221,015 | 91.4% |
| Schubert | 47 | 122,900 | 60.2% |
| Verdi | 20 | 56,000 | 84.2% |
| Mozart | 36 | 51,623 | 76.7% |
| Chopin | 34 | 40,977 | 78.8% |
| Handel | 47 | 36,076 | 98.4% |
| Traditional (folk dances, tunes) | 120 | 32,778 | 91.9% |
| Joplin | 13 | 27,199 | 98.4% |
| Sor (guitar) | 69 | 26,647 | 81.0% |

What the Mutopia run changed in the reader:

| Fix | Effect |
| --- | --- |
| Pages drawn sideways (LilyPond's landscape paper) are turned upright | 90 of the first 91 refusals were this; refusals 99 → 1 |
| First and second endings (voltas) are read and played (0010) | a Swedish dance 41.9% → 100% |
| LilyPond's digits in fonts with no Unicode table read by glyph name | volta numbers, fingerings |
| A whole note joins the voice it sounds with | organ chorales' long notes no longer overflow the lower voice |
| Two-note tremolos (MuseScore) | the 115 Lieder with tremolos 86.8% → 89.4% |

Large orchestral scores are the weak spot: many staves, transposing
instruments, divided parts, cue notes. Tremolos in LilyPond's drawing
(slashes across a stem) and D.C./D.S. jumps are not read yet.

## Not yet tested

- **MuseScore 4, Dorico, Sibelius, Finale PDFs.** Only MuseScore 3 and
  LilyPond engravings were available. Dorico, Finale 27+ and Sibelius with
  Bravura/Petaluma use the same SMuFL symbols, so the reading is the same;
  their layouts differ in ways not yet measured.
- 110 Lieder, 59 quartet files: MuseScore 3.2 could not open them (saved
  by a newer MuseScore). They are not counted above.
