#!/usr/bin/env python3
"""Scores with transposing instruments, to check that the app tells a part
written transposed from one written at concert pitch.

Writes MusicXML for a small orchestra - piccolo, flute, oboe, clarinets in
B-flat and A, alto saxophone, horn in F, trumpet in B-flat, violin, viola,
cello and double bass - in one of two forms:

  transposed   each transposing part written transposed (its own key
               signature), with <transpose>, as a conductor's score prints it;
  concert      every part written at concert pitch, in the score's key, with
               no <transpose>, as a "C score" prints it - the octave
               instruments (piccolo, double bass) still written in their
               octave, as they always are.

MuseScore engraves them (tools/bigcorpus.py musescore); its own MusicXML of
what it printed is the truth for each part's transposition.

    python3 tools/transposingscore.py out_dir count [seed]
"""
import os, random, sys

STEPS = ["C", "D", "E", "F", "G", "A", "B"]
NATURAL = [0, 2, 4, 5, 7, 9, 11]
# name, abbreviation, written above sounding (semitones), diatonic steps, octave, clef (sign, line, octave mark), written range
PARTS = [
    ("Piccolo", "Picc.", 0, 0, -1, ("G", 2, 0), (62, 84)),
    ("Flute", "Fl.", 0, 0, 0, ("G", 2, 0), (62, 86)),
    ("Oboe", "Ob.", 0, 0, 0, ("G", 2, 0), (60, 82)),
    ("Clarinet in B♭", "Cl.", 2, 1, 0, ("G", 2, 0), (58, 84)),
    ("Clarinet in A", "Cl.", 3, 2, 0, ("G", 2, 0), (58, 84)),
    ("Alto Saxophone", "A. Sax.", 9, 5, 0, ("G", 2, 0), (60, 81)),
    ("Horn in F", "Hn.", 7, 4, 0, ("G", 2, 0), (57, 77)),
    ("Trumpet in B♭", "Tpt.", 2, 1, 0, ("G", 2, 0), (60, 79)),
    ("Violin", "Vln.", 0, 0, 0, ("G", 2, 0), (57, 88)),
    ("Viola", "Vla.", 0, 0, 0, ("C", 3, 0), (50, 74)),
    ("Violoncello", "Vc.", 0, 0, 0, ("F", 4, 0), (38, 62)),
    ("Double Bass", "Cb.", 0, 0, 1, ("F", 4, 0), (40, 60)),
]


def scale(fifths):
    """The major scale of a key signature, as (step, alter) for each pitch class used."""
    sharps = "FCGDAEB"
    alters = {s: 0 for s in STEPS}
    for i in range(abs(fifths)):
        alters[sharps[i] if fifths > 0 else sharps[::-1][i]] += 1 if fifths > 0 else -1
    return alters


def note_xml(midi_target, alters, rng):
    """A scale note near midi_target: (step, alter, octave)."""
    for delta in [0, 1, -1, 2, -2, 3, -3]:
        m = midi_target + delta
        for i, s in enumerate(STEPS):
            for octv in range(1, 8):
                if 12 * (octv + 1) + NATURAL[i] + alters[s] == m:
                    return s, alters[s], octv
    raise ValueError(midi_target)


def write(path, concert, key, seed, bars=4):
    rng = random.Random(seed)
    parts = []
    out = ['<?xml version="1.0" encoding="UTF-8"?>',
           '<!DOCTYPE score-partwise PUBLIC "-//Recordare//DTD MusicXML 4.0 Partwise//EN" "http://www.musicxml.org/dtds/partwise.dtd">',
           '<score-partwise version="4.0">', '<part-list>']
    for i, (name, abbr, *_rest) in enumerate(PARTS):
        out += [f'<score-part id="P{i+1}"><part-name>{name}</part-name><part-abbreviation>{abbr}</part-abbreviation></score-part>']
    out.append('</part-list>')
    for i, (name, abbr, semis, dia, octv, (sign, line, mark), (lo, hi)) in enumerate(PARTS):
        written_semis = (0 if concert else semis)
        # a part written a major second up has two sharps more; a minor third up, three flats more...
        fifths = key if concert or not semis else key + {2: 2, 3: -3, 7: 1, 9: 3}[semis]
        alters = scale(fifths)
        out.append(f'<part id="P{i+1}">')
        for b in range(bars):
            out.append(f'<measure number="{b+1}">')
            if b == 0:
                out.append('<attributes><divisions>1</divisions>'
                           f'<key><fifths>{fifths}</fifths></key><time><beats>4</beats><beat-type>4</beat-type></time>'
                           f'<clef><sign>{sign}</sign><line>{line}</line></clef>')
                if written_semis or octv:
                    out.append(f'<transpose><diatonic>{-dia if written_semis else 0}</diatonic><chromatic>{-written_semis}</chromatic>'
                               + (f'<octave-change>{-octv}</octave-change>' if octv else '') + '</transpose>')
                out.append('</attributes>')
            for _ in range(4):
                s, a, o = note_xml(rng.randint(lo, hi), alters, rng)
                out.append(f'<note><pitch><step>{s}</step>' + (f'<alter>{a}</alter>' if a else '') +
                           f'<octave>{o}</octave></pitch><duration>1</duration><type>quarter</type></note>')
            out.append('</measure>')
        out.append('</part>')
    out.append('</score-partwise>')
    open(path, 'w', encoding='utf-8').write('\n'.join(out))


if __name__ == "__main__":
    d = sys.argv[1]
    count = int(sys.argv[2])
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    os.makedirs(d, exist_ok=True)
    rng = random.Random(seed)
    for n in range(count):
        key = rng.randint(-3, 3)
        for form in ("transposed", "concert"):
            write(os.path.join(d, f"orchestra{n:02d}_{form}.musicxml"), form == "concert", key, seed * 100 + n)
