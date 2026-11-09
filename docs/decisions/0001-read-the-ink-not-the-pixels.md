# 0001 - Read what the PDF draws, not a picture of it

- **Date:** 2026-11-08
- **Status:** Accepted

## Context

The user asked for a PDF-to-MusicXML converter that is close to 100%
accurate on computer-made scores and ignores scanned or handwritten ones,
on the grounds that notation is predictable when a PDF is clean. Existing
scanners (optical music recognition) render the page to pixels and
recognise shapes, which is where their errors come from.

## Decision

Read the PDF's own content. A PDF exported from a notation program holds
every symbol as a character of a music font at an exact position, and every
line as a drawn stroke. Name the characters (SMuFL code points, LilyPond's
glyph names), measure positions against the staff lines, and build the
music from that. No image recognition anywhere. Pages that are pictures are
refused with a reason.

## Consequences

- Pitches, durations and symbols are not recognised but read; what is left
  is assembling them (which stem a head is on, which voice a note is in),
  which is checkable.
- Scans, photos and handwriting are out of scope by design.
- PDFs whose symbols were turned into outlines, and old letter-mapped music
  fonts (Opus, Maestro, Petrucci), are refused for now. Both could be added:
  outlines by matching against the fonts' known shapes (Starting Blocks
  Notation's `sb_glyphs.lua` holds Bravura's), legacy fonts by a name table.
