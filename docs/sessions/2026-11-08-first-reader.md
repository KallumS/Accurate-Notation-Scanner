# 2026-11-08 - The first reader, the app, the check

**Asked:** a Mac app that converts PDF scores to MusicXML, close to 100%
accurate on computer-made scores, ignoring scanned and handwritten ones. The
user pointed at Noterator and Starting Blocks Notation as related work.

## Route

1. Probed MuseScore and LilyPond PDFs with PyMuPDF: music glyphs come as
   SMuFL code points (MuseScore) or named Emmentaler glyphs (LilyPond);
   lines as strokes or thin filled rectangles. Hypothesis confirmed.
2. Chose C++ with MuPDF to match the family (Noterator: JUCE, CMake, CI
   DMG), reusing Noterator's `Xml.*` and test runner.
3. Core: reader, glyph names, shapes, layout, reader, MusicXML writer,
   command line. First Bach chorale: 100% from both engravers.
4. Built the corpus check, then fixed what it showed, roughly in order:
   beams lying on a staff line swallowed as staff line; a neighbour's head
   taken onto a stem; LilyPond's octave-clef "8" in a text font; split bars
   at repeats; slurs between equal notes read as ties (neighbours only,
   equal pitch, a whole-score tie check); ties found by staff instead of by
   notehead; multi-bar rests; a second voice starting mid-bar (a time grid
   shared across staves); unisons drawn twice; barlines drawn in pieces;
   the system's opening bracket taken for a barline; a beam taken for a
   staff line; augmented unisons.
5. The app, built and screenshotted under Xvfb; fixture tests; CI;
   `build-mac.command`; docs.

## What looked broken and was not

- Most of LilyPond's low scores: musicxml2ly's conversion (empty bars after
  repeats, dropped ties, beams at 60 degrees, a 300-page Weber).
- "Missing" chord symbols in the Foster lead sheet: compare.py counted them.

## Numbers at the end

MuseScore: 34,001 / 34,525 notes (98.5%); 6,867 / 7,017 bars identical;
44 wrong bars not marked (see 0002). Linux CI green; the Mac job was
cancelled before starting on its first run (runner not given).

## Next

- The 44 unmarked bars: chord ties across a line break in two-voice piano
  writing; rests between piano staves; Clara Schumann's high runs.
- Lyrics and chord symbols; 8va lines read rather than marked.
- Outlined-glyph PDFs (match against Bravura's outlines from Starting
  Blocks Notation) and the legacy Opus/Maestro fonts.

## Later the same night: large corpora

Asked to test against the largest collections with known notes. Ran, with
`tools/bigcorpus.py` (MuseScore 3.2 engraving each file and exporting what it
printed as the truth): OpenScore Lieder (1,332 songs), OpenScore String
Quartets (119 movements), DCML Mozart sonatas (58) and Beethoven quartets
(70), and music21's corpus (927). 2,506 pieces, 3.79 million notes, 94.0%
exactly right; results in `docs/ACCURACY.md`.

What the big runs found, in order of size:

- A third of the Lieder used Bravura's oversized noteheads (U+F4BE, an
  optional glyph outside SMuFL's core table): no notes were read at all.
  Font-specific optional glyphs are now named (`SmuflOptional.inc`), and
  any music symbol that cannot be named is reported.
- "Hide empty staves": a song opening with the piano alone lost the singer.
  Parts are now matched across systems by label (and abbreviation), staves
  and clefs, with new parts added where they first appear.
- Triplets with the "3" left off; time signatures for parts shown late.
- A slur between D-flat and D read as a tie, carrying the flat: bars where
  only a tie carries an accidental are now marked.

Blocked here, and why: newer MuseScore (3.6, 4) and running LilyPond on the
Mutopia Project's files both count as running outside code in this sandbox;
the user can allow them. MuseScore 3.5+ files were opened by marking copies
as the older format. The from-source MuPDF build now passes on Linux CI; the
Mac CI job is cancelled by GitHub before it starts, every time.
