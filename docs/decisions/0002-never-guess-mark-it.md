# 0002 - Never guess; mark it

- **Date:** 2026-11-08
- **Status:** Accepted

## Context

A converter that is right 98% of the time and silent about the other 2% is
worse than one that says where it is unsure: the user has to proof-read
everything. Engraved notation carries its own checks - bars add up, ties
join equal pitches, notes sit on lines or spaces.

## Decision

Every bar is checked after reading. Anything that does not check out
becomes an Issue with its page and box, shown outlined on the page in the
app and listed. The reader does not adjust durations to make a bar fit,
invent accidentals, or pick the likelier of two readings without saying so.
A whole file is "checked out" only when nothing was marked.

## Consequences

- The measure of progress is two numbers: notes matched, and wrong bars
  that were not marked (`tools/flagcheck.py`). The second matters more.
- On 2026-11-08: of about 5,900 bars from MuseScore PDFs, 116 differ from
  the source and 44 of those were not marked (mostly piano: ties on chords
  across a line break, rests between the two staves; and a slur between two
  equal notes, which is drawn exactly like a tie).
