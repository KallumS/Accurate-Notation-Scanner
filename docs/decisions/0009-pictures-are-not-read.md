# 0009 - Pictures (PNG, JPEG) are not read

- **Date:** 2026-11-09
- **Status:** Accepted (reaffirms 0001)

## Context

The user asked whether the app could read JPEG and PNG files. A picture
has no music-font characters and no drawn lines - only pixels - so reading
one means recognising shapes, which is the guessing 0001 set out to avoid.

A first attempt was made for clean, computer-made pictures: staff lines
from long runs of dark pixels, staff lines lifted out, every other shape
matched against the music fonts (Bravura, Leland, MuseScore 3's, LilyPond's)
drawn at the staff's size, then the same reader and checks. On a first
MuseScore chorale at 150 dpi it found clefs, noteheads, accidentals, stems
and barlines, but missed half notes, some barlines and a staff whose line
touched a beam, matched letters in the lyrics as rests and accents, and
read 8% of notes right. Getting it near the PDF reader's accuracy would be
a long project of its own.

## Decision

The user decided to drop it. Pictures are refused, as before, with the
reason. The attempt was not committed.

## Consequences

- If it is taken up again, the lessons: clefs must be searched for where
  clefs sit (at each staff's start), not taken from connected shapes; a
  staff line thinner than a pixel is pale, so lines need a lighter
  threshold than symbols; line centres must be measured to a fraction of a
  pixel or the layout's spacing check fails; every symbol kind needs a
  "where it can be" rule or lyrics turn into rests; MuseScore's fonts for
  pictures were Leland (MuseScore 4) and MScore (MuseScore 3).
