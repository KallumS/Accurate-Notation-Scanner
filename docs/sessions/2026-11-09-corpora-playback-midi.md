# 2026-11-09 - Large corpora, Mutopia, playback, MIDI

**Asked:** test against the largest corpora with known notes; then run
Mutopia; add playback with the Mac's built-in sounds, MIDI export, and
reading JPEG/PNG (the last dropped by the user part-way).

## Route

1. `tools/bigcorpus.py`: MuseScore 3 engraves each score file to a PDF and
   exports what it printed as the truth. Ran OpenScore Lieder (1,332 songs),
   OpenScore String Quartets (119), DCML Mozart sonatas (58) and Beethoven
   quartets (70), music21's corpus (927): 2,506 pieces, 3.79 million notes,
   94.0% exact (`docs/ACCURACY.md`).
2. Fixes they forced: Bravura's optional "oversized" noteheads (a third of
   the Lieder had no notes read); parts matched across systems when empty
   staves are hidden; unprinted triplets; time signatures for late parts;
   bars marked where only a tie carries an accidental; unnamed music symbols
   reported; MuPDF fetched by a CMake script (a shell one-liner was split by
   CMake - caught by a new Linux CI job that builds MuPDF from source).
3. Mutopia (user allowed running LilyPond on its files): LilyPond's MIDI as
   the truth. Absolute-time comparison made one early misread cost every
   later note, so notes are now lined up in sequence; guitar parts are
   written an octave above how they sound, so a whole-piece octave shift is
   allowed. 40-piece sample: 81.8% exact, 86.5% right pitch. The full run of
   ~1,900 pieces was still going when the session ended.
4. MIDI export and playback (decision 0008), the playing bar lit on the
   page; checked with a silent PulseAudio sink and `--play` screenshots
   (`docs/playing.png`).
5. Pictures: a first image reader (decision 0009) - dropped by the user;
   not committed.

## What looked broken and was not

- MuseScore 3.2's own MusicXML exports are sometimes invalid XML (20
  Lieder could not be compared); the app's output parsed every time.
- Low LilyPond/Mutopia scores on guitar pieces: the page is right; the MIDI
  sounds an octave lower.

## Blocked

- Newer MuseScore (3.6/4) could not be installed in the sandbox (running a
  downloaded program); 3.5+ files were opened by marking copies 3.01.
- GitHub cancels the Mac CI job before it starts, every run.

## Next

- Finish the Mutopia run and add its numbers to `docs/ACCURACY.md`.
- Dorico, Sibelius and Finale PDFs from the user.
- Ties vs slurs, cross-staff notes, voltas (also for playback of repeats).
