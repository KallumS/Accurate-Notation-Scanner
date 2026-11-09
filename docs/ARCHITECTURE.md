# Architecture

```
PDF ──MuPDF──▶ PageInk ──Layout──▶ staves, systems, parts, barlines
 (PdfReader)   glyphs (SMuFL names), segments, curves, texts, pictures
                       │
                       ▼ Reader
        heads ─ stems ─ beams/flags ─ dots ─ accidentals ─▶ chords and rests
        clefs, key and time signatures, ties, slurs, tuplets, dynamics,
        repeats and first/second endings (voltas)
        voices and onsets (a time grid shared by all staves of a system)
        the check: bars add up, voices fit, ties land, heads on lines
                       │
                       ▼
                Score + Issues ──MusicXml──▶ .musicxml
                       │      └──Midi (perform, playOrder)──▶ .mid, playback
                       │
                       ▼
        app: list of files, the page with marked bars, the issue list
```

## The core (`Source/Core`, no JUCE)

- **PdfReader** runs each page through a MuPDF device that records instead
  of painting. Text marked invisible (a scan's OCR layer) is skipped; white
  ink is skipped.
- **Glyphs** names each character. SMuFL fonts by code point
  (`SmuflNames.inc`, generated from SMuFL's `glyphnames.json`), LilyPond's
  Emmentaler by glyph name. Letter-mapped legacy fonts are recognised and
  refused (0002).
- **Shapes** turns paths into `Segment`s (centre line + thickness: stems,
  beams, barlines, staff and ledger lines alike, however drawn) and
  `Curve`s (ties, slurs). Fill-then-stroke duplicates are merged.
- **Layout** finds staves (five thin, evenly spaced, equally long lines),
  joins them into systems (a line, bracket or brace at their left), makes a
  braced group one part, and finds barlines: upright strokes spanning a
  staff exactly with no notehead at either end (pieces of one line joined).
- **Reader** does the music, staff by staff and bar by bar (see the
  comments at the top of `Reader.h`), then the whole-score passes: tie
  consistency, multi-bar rests expanded, pickup bar, the bar check.
- **MusicXml** writes partwise MusicXML 4.0 with exact divisions.
- **Instruments** names each part's instrument from its printed label
  (Noterator's table) and how it is written against how it sounds (0013).
- **Chords** is ScaleView Pro's chord reader; **Harmony** uses it to name
  the chords each bar outlines - an analysis, never written into the
  MusicXML (0011).
- **AutoCC** is AutoCC's controller-curve engine, run by Midi for the
  sustaining instruments (0012).
- **Midi** turns the Score into sound: `playOrder()` follows repeats and
  endings, `perform()` makes the notes (ties joined, dynamics, an instrument
  per part), `toMidiFile()` writes them; the app's player plays the same
  performance (0008, 0010).
- **Convert** is the one call the app and the command line use; it also
  writes the report JSON.

## The app (`Source/App`, JUCE 8)

One window (`MainComponent`): files on the left (converted two at a time on
a thread pool), the chosen file's name, summary and instruments, the bars
that need a look, then the page rendered by MuPDF with them outlined (and
the chords, with Chords on). A toolbar that folds into rows, and a menu bar
(File, View with Interface Size, Play). `Theme` holds the family's colours,
focus ring and click rule; `SystemSettings` reads Increase Contrast (0014).
`--screenshot` draws the window into a PNG for checking without a screen.

## The checking

- `Tests/` - unit tests and real fixture PDFs read note by note.
- `tools/corpus.py` - engraves the music21 corpus and random scores
  (`tools/randomscore.py`) with MuseScore 3 and LilyPond, reads them back,
  compares (`tools/compare.py`). `tools/flagcheck.py` lists wrong bars that
  were not marked.
