# 0012 - AutoCC's controller curves in playback and MIDI export

- **Date:** 2026-11-09
- **Status:** Accepted (adds to 0008); how notes are fed changed the same
  day, at the user's request, to Noterator's way (below)

## Context

The user asked for AutoCC's functions (KallumS/AutoCC) in the playback.
AutoCC turns notes into the controller curves a player rides by hand on
orchestral sample libraries: CC1 dynamics, CC11 expression, CC7 volume,
CC21 aux, with per-family shapes fitted to Lorne Balfe's mock-ups.

## Decision

- `AutoCC.*` is the JSFX's engine ported: its presets, the Gated envelope,
  the Legato trigger, the 0.4 s rule that carries a line on, 5 ms ticks.
- Strings, brass, woodwinds and voices get it (by their General MIDI
  program, from the part's name); piano, harp, guitar and percussion do not
  - their notes do not swell.
- AutoCC's gesture is first key down to last key up, and in Legato only
  those two moments act. Notes written joined are one gesture (as a player
  overlapping the keys gives it); a rest ends it. The app's notes are played
  95% long so repeated notes are heard, which would otherwise end every
  gesture after its first note.
- The printed dynamic sets AutoCC's Dynamic: the same steps the app uses for
  note velocity, from mf (f +14, ff +28, p -24, pp -36). AutoCC says the
  start level is the written dynamic; this table is the app's choice.
- The MIDI file carries all four lanes, for a DAW and sample libraries. The
  Mac's General MIDI player is sent CC11 and CC7 only: there CC1 is the
  modulation wheel, which adds vibrato rather than loudness.

## Consequences

- Tests take their numbers from AutoCC's README and preset table.
- Checked against the JSFX itself, run through ysfx (the EEL2 compiler
  REAPER uses; `tools/autocc/`): 1,300 random note sequences - lines,
  chords, overlaps, gaps either side of 0.4 s, every preset, several
  Dynamic settings - give exactly the same CC values at the same samples.

## Changed (2026-11-09): fed as Noterator feeds it

The user asked for the app to work as Noterator does. Every written note is
now a key of its own for its written length (Noterator's `AutoCC.cpp` and
`Perform.cpp`), so notes written end to end let go and AutoCC's 0.4 s rule
carries the line on at the level the first note reached; and the printed
dynamic is no longer AutoCC's Dynamic - it reaches AutoCC through velocity
only, as in Noterator. The two points above about gestures of joined notes
and the dynamic table are replaced by this. The engine itself is unchanged
and still matches the JSFX sample for sample; Noterator's port samples
every 10 ms, so values can land a few milliseconds apart between the two
apps, never differently shaped.
