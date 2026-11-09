# 0013 - Instruments are read from the part names; transposing parts play at concert pitch when the page shows them transposed

- **Date:** 2026-11-09
- **Status:** Accepted (changes 0008's "pitches are as written")

## Context

Big orchestral scores read worse than they are partly because a clarinet
in B-flat or a horn in F is written at one pitch and sounds at another, and
the app played and exported every part as written. The user asked for the
instruments to be identified, pointing to Noterator, Miderator and
Orchestration Helper.

## Decision

- `Instruments.*` holds Noterator's instrument table, its numbers
  unchanged: ranges, sweet spots, General MIDI sound, how each is written
  against how it sounds, AutoCC shape. Labels are read in English,
  Italian, German and French, full or abbreviated, with Noterator's own
  part-name words and Orchestration Helper's aliases among them; the key an
  instrument is "in" is read from the label ("Klarinette in B", "Corni in
  Es", "Trompette en Fa"). A label naming nothing in the table names
  nothing: no instrument is assumed.
- Whether a transposing part is written transposed is **read from the
  page**, not assumed: its key signature against the parts that do not
  transpose. Two sharps above the strings for a clarinet in B-flat:
  transposed. The strings' own key: a concert-pitch ("C") score. Horns and
  trumpets with no key signature and a stated key: transposed, by the old
  custom. Anything else: left as written, and a note says it could not be
  told.
- Octave instruments (double bass, piccolo, contrabassoon, tenor voice,
  guitar) take their octave only when the page uses their usual clef with
  no octave mark; a clef with an 8 has already moved the notes.
- Playback and MIDI export sound at concert pitch where the page shows a
  transposition. The MusicXML keeps the written notes and adds
  `<transpose>` (Noterator's convention) and the instrument's name and
  General MIDI program, which is how Noterator reads them.

## Consequences

- Checked on MuseScore's engravings of the same orchestra written both
  ways (`tools/transposingscore.py`): every part's transposition, 96 of 96,
  agrees with MuseScore's own MusicXML. Two of them are fixtures.
- The key signature is the only evidence used; a transposed part in a piece
  with no non-transposing part to compare with is played as written.
- Not yet used: the ranges (a note far outside an instrument's range is a
  likely misread, and could be marked).
