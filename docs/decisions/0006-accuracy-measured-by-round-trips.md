# 0006 - Accuracy is measured by engraving known music and reading it back

- **Date:** 2026-11-08
- **Status:** Accepted

## Decision

`tools/corpus.py`: music21's corpus (Bach chorales; quartets, Lieder, piano,
choir, concerto) and random scores built to stress the reader
(`tools/randomscore.py`) are engraved by MuseScore 3 and by LilyPond. MuseScore
also exports the MusicXML of what it engraved, which is the truth for its
PDFs; LilyPond's truth is the source. The read-back is compared note by note
(onset, spelled pitch, duration, tie) with bars aligned. Fixtures from this
set are committed as C++ tests.

## Consequences

- LilyPond's results include musicxml2ly's own conversion faults; they are
  looked at, not trusted.
- 2026-11-08: MuseScore - 40 chorales 100%, 21 real pieces 97%, 40 random
  scores 94%; overall 98.5% of 34,525 notes.
