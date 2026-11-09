# 2026-11-09 (later) - Mutopia in full, endings, sideways pages, tremolos

**Asked:** carry on with the work from before the logs were written (not
the JPEG/PNG reader): finish Mutopia and fix what it shows.

## Route

1. Mutopia's MIDI often plays the repeats (`\unfoldRepeats`). A Swedish
   dance read "clean" scored 41.9%: the MIDI was twice as long. The check now
   also compares the score with its repeats played. music21's
   `expandRepeats()` hung on a Paganini caprice and killed a 1,584-piece run
   at the very end (the summary was written only at the end); the harness
   now keeps each result as it comes (`--resume`) and uses the app's own
   MIDI export for the repeats.
2. That showed first/second endings were never read: the MusicXML had the
   repeat signs but both endings played every time. Voltas are now read
   from their ink and the player follows repeats and endings (decision
   0010). Checked against MuseScore's truth for 156 pieces with endings: 140
   match bar for bar; most others are endings hidden on the page.
   LilyPond's volta numbers come from a font subset with no Unicode table
   ("fattened.two"), now read by glyph name.
3. 99 of the first Mutopia pieces were refused as "no music": 90 of 91
   checked were scores drawn sideways on an upright page (LilyPond's
   `'landscape` paper). Pages whose music glyphs all face a quarter turn
   away are read again turned upright; the app turns the picture the same
   way so the marks line up (screenshot checked). Refusals 99 → 1.
4. Organ chorales then showed whole notes put in the wrong voice (by
   height on the staff); they now join the voice they sound with.
5. Truth problems found and handled, not "fixed" in the reader: chord
   symbols play in LilyPond's MIDI (silenced before engraving); 97 files
   build their MIDI from a transposed copy (organ registrations doubled in
   octaves) and are left out of the totals.
6. Two-note tremolos (MuseScore: strokes floating between two stems) were
   marked bars; now read as MusicXML 2:1 with `<tremolo>`. The 115 Lieder
   with tremolos: 86.8% → 89.4% of notes exact.

## Numbers at the end

- Mutopia, whole archive: 1,473 pieces, 1,352,906 notes, **76.7%** right
  pitch in sequence, median piece 97.5%, 581 pieces ≥ 99%. Pieces under
  3,000 notes 86.0%; the 79 big ones (symphonies, concertos) 57.2%. Same
  pieces as the first full run: 72.0% → 75.7% before the truth clean-up.
- `tools/corpus.py --sets bach,real,random --count 40`: unchanged by every
  step (MuseScore 98.53%; 31 unmarked wrong bars).
- Tests: 32, all passing; new fixtures `ms_kinkel_op10no3` (endings,
  OpenScore CC0) and `ly_carcassi_op60no24` (LilyPond endings, Mutopia CC
  BY-SA), listed in `Tests/fixtures/SOURCES.md`.

## Mistakes

- Continuation lines for voltas were first accepted too freely: ledger
  lines and the staff lines of the staff above passed as "the bracket
  carries on". Now: not a staff line, not through a notehead, not starting
  at the system's edge, at least three spaces long.
- A label like "1. 2. 3." is three words in the PDF; reading word by word
  took "2." alone. Labels are now joined along their line first.

## Next

- Orchestral scores: the 79 big Mutopia pieces at 57%.
- LilyPond's single-note tremolo (slashes across a stem) and its two-note
  form; D.C./D.S./Fine for playback.
- A volta whose label cannot be read is skipped silently; it should be
  marked.
- Dorico, Sibelius and Finale PDFs from the user.
