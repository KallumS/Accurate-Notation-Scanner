# 0008 - Playback and MIDI export, from the read score

- **Date:** 2026-11-09
- **Status:** Accepted

## Context

The user asked to hear a converted score with the Mac's built-in sounds and
to export MIDI. Noterator already plays through Apple's General MIDI set
(the DLSMusicDevice Audio Unit every Mac has).

## Decision

- `Source/Core/Midi.*` turns the read `Score` into a performance: every
  note with start and length in quarter notes, ties joined, grace notes
  short before their note, loudness from the dynamics, an instrument per
  part from its name ("Vc." is a cello), the tempo from the printed
  metronome mark, else a tempo word, else 100. The same performance is
  written as a type-1 MIDI file (tempo track + a track per part).
- `Source/App/Player.*` plays it through Apple's DLS synth (as Noterator);
  where that is missing, a small built-in tone stands in. The bar being
  played is lit on the page and the pages turn with it. Space plays/stops.
- `notation-scan --midi out.mid` exports from the command line.

## Consequences

- Pitches are as written: a transposing instrument sounds at written pitch.
  *Changed by 0013: at concert pitch where the page shows the part transposed.*
- Repeats are played once (voltas are not read yet). *Changed by 0010: repeats
  and first/second endings are now played.*
- Tested on Linux with a silent PulseAudio sink and a screenshot
  (`--play seconds`); Apple's synth is only exercised on a Mac.
