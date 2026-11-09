# 0010 - First and second endings are read; repeats are played

- **Date:** 2026-11-09
- **Status:** Accepted (changes the "repeats are played once" consequence of 0008)

## Context

Volta brackets (the "1." and "2." brackets over first and second endings)
were not read. The MusicXML had the repeat signs but no endings, so any
program playing it (Noterator, this app) played both endings every time.
Mutopia's MIDI files often play the repeats out, which is how this showed:
a Swedish dance scored 41.9% "right" until the comparison played the
repeats too (99.5%).

## Decision

- A volta is read from its ink, as everything else (0001): a thin line
  above a staff, a hook down at its left end, and its label under the line
  next to the hook. A hook at the right end closes it (MusicXML
  `stop`); without one it is left open (`discontinue`).
- The label is read whole: "1.", "2.", "1., 2.", "1.-3.", "1. und 3.
  Strophe", "1.u.2.". A bare digit with no full stop counts only when
  upright: an italic digit by a bracket is a tuplet's.
- A bracket that runs to the end of a system is carried on only if the
  next system starts with an unlabelled line of its own (not a staff line,
  not a ledger line through a notehead, not starting at the system's edge).
- Endings go on every part's barlines, as MusicXML expects.
- `Midi.cpp`'s `playOrder()` plays each repeat twice (or as many times as
  its endings' numbers ask) and takes each ending on its own pass. The
  app's playing-bar highlight follows the jumps. D.C., D.S., Fine and codas
  are not followed: they are words, and reading them is a later step.
- LilyPond writes volta numbers (and fingerings) in a font subset with no
  Unicode table; their glyph names ("one", "fattened.two") are read as
  digits in `PdfReader.cpp`.

## Consequences

- Checked against MuseScore's own MusicXML for 156 pieces with endings
  (Mozart, Beethoven, OpenScore Lieder, music21): 140 match bar for bar;
  most of the rest are endings hidden on the page (the truth has them, the
  ink does not), and a few are MuseScore export quirks (a "2." ending
  exported as number 1). The accuracy check (`tools/corpus.py`) is
  unchanged by it.
- A volta whose label cannot be read is left out, not guessed. It is not
  yet reported as an issue.
