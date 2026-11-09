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
| music21 corpus (chorales, madrigals, quartets, songs, Lieder) and random stress scores | 101 | 34,525 | 98.5% | 97.9% |
| **All** | **1,680** | **3,272,759** | **93.3%** | |

(music21's whole corpus - all 410 Bach chorales, Palestrina, Trecento,
folk tunes - is being added; see the end.)

Per piece in the Lieder: the median song is 95.4% right, 298 songs are 99%
or better, 116 are exact.

### What it marks, and what it does not

| Collection | Bars wrong | of which marked for you to check | not marked |
| --- | ---: | ---: | ---: |
| Lieder | 22,849 | 79% | 4,820 (2.0% of all bars) |
| String Quartets | 7,192 | 57% | 3,092 (1.0% of all bars) |
| Mozart sonatas | 843 | 81% | 160 (1.0%) |
| Beethoven quartets | 612 | 43% | 346 (0.5%) |

The unmarked errors, sampled (1,046 bars from 200 songs):

| Kind | Share | What it means |
| --- | ---: | --- |
| A tie missing or extra | 41% | Every pitch and rhythm right; one note tied (or not) to the next. A slur between two equal notes is drawn exactly like a tie. |
| Notes extra | 22% | Mostly cross-staff writing in piano parts (a run passing from one staff to the other) read into the wrong part. |
| Notes missing | 12% | Notes the reader could not place (unusual stems, cue-size passages). |
| Wrong pitch | 7% | Almost all a knock-on of a tie: a tie carries an accidental over the barline. Since 2026-11-09 such bars are marked whenever the tie is the only reason for the accidental. |
| Other | 18% | Rests, onsets of a second voice starting mid-bar, lengths. |

### Files reported "every bar checked out"

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

## Not yet tested

- **LilyPond's own scores (the Mutopia Project, ~1,900 pieces).** The plan
  was to engrave each with LilyPond and use LilyPond's MIDI as the truth
  (pitch, onset, length of every note). LilyPond 2.24 removed its safe mode,
  so engraving third-party `.ly` files runs code from them; the session's
  sandbox refused that without the user's say-so. `tools/bigcorpus.py
  lilypond` is ready for it.
- **MuseScore 4, Dorico, Sibelius, Finale PDFs.** Only MuseScore 3 and
  LilyPond engravings were available. Dorico, Finale 27+ and Sibelius with
  Bravura/Petaluma use the same SMuFL symbols, so the reading is the same;
  their layouts differ in ways not yet measured.
- 110 Lieder, 59 quartet files: MuseScore 3.2 could not open them (saved
  by a newer MuseScore). They are not counted above.
