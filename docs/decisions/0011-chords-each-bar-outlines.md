# 0011 - The chords each bar outlines: ScaleView Pro's reader, an analysis apart from the reading

- **Date:** 2026-11-09
- **Status:** Accepted

## Context

The user asked for the chords each bar outlines, and to use ScaleView Pro's
detection (KallumS/ScaleView-for-Reaper) for it. ScaleView names a set of
held notes; a bar of written music also has passing notes, neighbours and
arpeggios, and often more than one chord.

## Decision

- `Chords.*` is ScaleView Pro's `detectChord`, ported rule for rule. Its
  tests are ScaleView's own 117 cases and its two sweeps; run against the
  Lua itself (texlua), the port gives the same name for all 52,800 three-
  to five-note voicings in every bass position, in eight keys.
- `Harmony.*` decides what is named: each half bar (each beat in a bar of
  three), all parts together, with passing and neighbour notes left out as
  Open Music Theory defines them ("Embellishing tones", from the user's
  documents): a step from the notes on both sides, joined to both, off the
  beat. Every other note in that part of the bar is named, the lowest as
  the bass, spelled for the key signature's major key. Halves with the same
  name are one chord.
- It is an analysis, not something read. It is never written into the
  MusicXML; the app shows it on request (Chords button) and
  `notation-scan --chords` prints it.

## How it was chosen - measured, on the DCML Mozart sonatas

All 54 movements, the app reading the PDFs, compared beat by beat with
the analysts' labels (root, and root with triad quality):

| What is named | Root | Root + quality |
| --- | ---: | ---: |
| every note in the bar | 52.4% | 47.6% |
| every note in the half bar | 56.1% | 50.3% |
| half bar, notes held for a quarter as long as the longest | 57.6% | 50.8% |
| half bar, passing + neighbour notes left out, every other note (chosen) | 58.6% | 53.3% |
| + escape tones, appoggiaturas, incomplete neighbours left out | 56.2-56.9% | 48.8-50.0% |
| leaving notes out wherever ScaleView's cost goes down | 53.1-55.8% | 48.2-51.6% |
| the analysts' own chord notes, per beat (a ceiling) | 71.6% | |
| the analysts' chord notes, whole chords | 88.8% | 86.4% |

The C++ as built, straight from the PDFs: **59.7% root, 54.3% root and
quality**. The gap to the ceilings is mostly function against sound: the
analysts write V(64) where the notes are a C chord over G, and name a
diminished seventh by its function, not its bass.

## Consequences

- Names are literal: a melody's chromatic neighbour off the beat that is
  not joined both ways stays in (Cadd#9). That is ScaleView's naming of
  what sounds, not a mistake in it.
- The key signature is read as its major key (the mode is not printed);
  it only spells and settles ScaleView's draws.
