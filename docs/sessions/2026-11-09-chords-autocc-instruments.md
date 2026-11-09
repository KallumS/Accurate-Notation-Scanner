# 2026-11-09 (evening) - Why orchestral scores score low, chords, AutoCC, instruments

**Asked:** why big orchestral scores read less accurately; identify the
instruments named (Noterator, Miderator, Orchestration Helper); never change
a test to make it pass; detect the chords each bar outlines with ScaleView
Pro, using the user's music theory documents; use AutoCC's functions in
playback.

## Route

1. **Orchestral scores.** Compared part by part instead of as one score,
   the 79 biggest Mutopia scores are 78.0% right, not 57.2%: LilyPond's MIDI
   is at concert pitch, the page at written pitch, and one transposing part
   spoiled every chord. A real reader bug came out of it: a score set in a
   smaller music font throughout read as all grace notes (a Beethoven
   quartet 2.4% -> 73.4%; seven Mutopia pieces).
2. **Tests.** The playback test had been counting notes over the code's own
   play order (circular). Its expectation now comes from MuseScore's
   MusicXML; the endings test from the truth file and the printed bar
   numbers. CLAUDE.md now carries the user's rule.
3. **Chords (0011).** ScaleView Pro's `detectChord` ported to `Chords.*`;
   checked with ScaleView's own 117 cases and against the Lua itself (run
   with texlua): identical on 52,800 voicings. What to name was measured on
   the DCML Mozart analyses: half bars, passing and neighbour notes left out
   as Open Music Theory (the user's documents) defines them - 59.7% of beats
   agree with the analysts' root, where their own chord notes would give
   71.6%. Shown with a Chords button; `notation-scan --chords`.
4. **AutoCC (0012).** The JSFX's engine ported to `AutoCC.*`; strings,
   brass, woodwinds and voices get its curves, one gesture per run of
   joined notes, the printed dynamic as its Dynamic. The MIDI file has all
   four lanes; the Mac's GM sounds get CC11 and CC7.
5. **Instruments (0013).** Noterator's table, names in four languages and
   abbreviations, the key an instrument is in. Transposition read from the
   key signatures; playback at concert pitch where the page is transposed;
   `<transpose>` and the instrument in the MusicXML. 96/96 parts right on
   MuseScore engravings of the same orchestra written both ways.

## Mistakes

- Four test expectations were wrong and were corrected with the reason
  written beside each (and told to the user): a doubled tonic root reads as
  root position by ScaleView's documented rule; a note with nothing after it
  is not a passing note by Open Music Theory's definition; the brass peak
  was read 50 ms into its settle; the violin's top forgot AutoCC's cap at
  127 and its 10% velocity scaling.
- The first AutoCC curves froze after each phrase's first note: notes are
  played 95% long, so every key went up between notes. Fixed by giving
  AutoCC one gesture per run of joined notes - what a legato player gives it.
- An editing script turned `\xe2...` escapes into literal characters in the
  app's source; the instrument line showed mojibake until fixed.
- Noterator already had an AutoCC port; it was found after this one was
  written. They differ on joined notes (Noterator releases between them).

## Numbers at the end

- Tests: 57, all passing. `tools/corpus.py`: unchanged (MuseScore 98.53%;
  31 unmarked wrong bars).
- Chords on Mozart: 59.7% root, 54.3% root and quality, per beat.

## AutoCC against the JSFX itself

The sandbox first refused downloading and building ysfx (code from
outside); the user then allowed it. `tools/autocc/` builds the real
AutoCC.jsfx and this port side by side: 1,300 random note sequences, every
CC value at the same sample in both.

## Next

- A note far outside its instrument's range is a likely misread: mark it.
- Reconcile the two AutoCC ports (this app's and Noterator's).
- Part matching across systems could use the instrument as well as the name.
