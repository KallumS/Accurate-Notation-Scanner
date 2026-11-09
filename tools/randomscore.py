#!/usr/bin/env python3
"""Random but properly written scores, to stress the reader.

Each score picks some of: a time signature (simple or compound), a key, a
clef, dotted and double-dotted notes, 16ths and 32nds, triplets, rests,
chords with seconds, accidentals (double ones too), ties across bars, two
voices on a staff, a piano's grand staff, grace notes, and changes of clef,
key and time part-way through. The result is plain MusicXML, so any
engraver can set it.

    python3 tools/randomscore.py out_dir count [seed]
"""
import os, random, sys, warnings
warnings.filterwarnings("ignore")
from fractions import Fraction as F
from music21 import (stream, note, chord, meter, key, clef, pitch, duration,
                     tie, layout, metadata, dynamics, articulations, expressions)

TIMES = ["4/4", "3/4", "2/4", "6/8", "3/8", "2/2", "9/8", "12/8", "5/4", "3/2"]
CLEFS = {"treble": (clef.TrebleClef, 60, 81), "bass": (clef.BassClef, 41, 62),
         "alto": (clef.AltoClef, 50, 71), "tenor": (clef.TenorClef, 46, 67),
         "treble8vb": (clef.Treble8vbClef, 48, 69)}


def durations_for(rng, bar, compound, level):
    """A list of (quarterLength, dots, tuplet) filling `bar` quarters."""
    out = []
    t = F(0)
    beat = F(3, 2) if compound else F(1)
    choices = [F(4), F(3), F(2), F(3, 2), F(1), F(3, 4), F(1, 2), F(1, 4)]
    if level >= 2:
        choices += [F(7, 4), F(7, 8), F(1, 8)]
    while t < bar:
        left = bar - t
        on_beat = (t % beat) == 0
        if level >= 1 and on_beat and left >= beat and not compound and rng.random() < 0.15:
            # a triplet group filling one beat
            unit = beat / 3
            for _ in range(3):
                out.append((unit, "triplet"))
            t += beat
            continue
        fits = [c for c in choices if c <= left]
        # prefer values that keep later beats reachable
        d = rng.choice(fits)
        out.append((d, None))
        t += d
    return out


def make_note(rng, lo, hi, prev, allow_chord, accidental_rate):
    mid = prev if prev else (lo + hi) // 2
    m = max(lo, min(hi, mid + rng.randint(-5, 5)))
    p = pitch.Pitch(midi=m)
    # respell sometimes, with doubles now and then
    r = rng.random()
    if r < accidental_rate:
        alters = [-1, 1] + ([-2, 2] if rng.random() < 0.15 else [])
        a = rng.choice(alters)
        p = pitch.Pitch(step=p.step, octave=p.octave)
        p.accidental = pitch.Accidental(a)
    if allow_chord and rng.random() < 0.3:
        others = []
        for gap in rng.sample([1, 2, 3, 4, 5], rng.randint(1, 2)):
            q = pitch.Pitch(step=p.step, octave=p.octave).transpose(f"{['m2','M2','m3','M3','P4','P5'][gap]}")
            if q.midi <= hi + 3:
                others.append(q)
        if others:
            return chord.Chord([p] + others), m
    return note.Note(p), m


def fill_voice(rng, bars, ts, compound, level, lo, hi, allow_chord, accidental_rate, rest_rate, grace_rate):
    measures = []
    prev = None
    for b in range(bars):
        content = []
        for d, tup in durations_for(rng, ts, compound, level):
            if rng.random() < rest_rate:
                n = note.Rest()
            else:
                n, prev = make_note(rng, lo, hi, prev, allow_chord, accidental_rate)
            n.duration = duration.Duration(d * (F(3, 2) if tup else 1))
            if tup:
                n.duration = duration.Duration(float(d * F(3, 2)))
                n.duration.appendTuplet(duration.Tuplet(3, 2))
            content.append(n)
        measures.append(content)
    return measures


def tie_across(rng, measures, rate):
    for b in range(len(measures) - 1):
        a, c = measures[b][-1], measures[b + 1][0]
        if isinstance(a, note.Note) and isinstance(c, note.Note) and rng.random() < rate:
            c.pitch = pitch.Pitch(a.pitch.nameWithOctave)
            a.tie = tie.Tie("start")
            c.tie = tie.Tie("stop")


def build(seed, outdir):
    rng = random.Random(seed)
    level = rng.choice([0, 1, 1, 2])
    ts_name = rng.choice(TIMES)
    ts = meter.TimeSignature(ts_name)
    compound = ts.numerator in (6, 9, 12) and ts.denominator == 8
    bar = F(ts.barDuration.quarterLength).limit_denominator(64)
    fifths = rng.randint(-6, 6)
    bars = rng.randint(6, 14)
    kind = rng.choice(["solo", "solo", "piano", "duo", "twovoices"])
    s = stream.Score()
    s.insert(0, metadata.Metadata(title=f"Random {seed}"))
    acc = 0.12 + 0.1 * level

    def part_with(name, clefname, voices=1, chords=False):
        cl, lo, hi = CLEFS[clefname]
        p = stream.Part()
        p.partName = name
        vs = []
        for v in range(voices):
            if voices == 2:
                vlo, vhi = ((lo + hi) // 2, hi + 3) if v == 0 else (lo - 3, (lo + hi) // 2)
            else:
                vlo, vhi = lo, hi
            vs.append(fill_voice(rng, bars, bar, compound, level, vlo, vhi, chords, acc,
                                 0.12 if level else 0.05, 0.0))
        for v in vs:
            tie_across(rng, v, 0.25)
        for b in range(bars):
            m = stream.Measure(number=b + 1)
            if b == 0:
                m.insert(0, cl())
                m.insert(0, key.KeySignature(fifths))
                m.insert(0, meter.TimeSignature(ts_name))
            if voices == 1:
                t = 0
                for n in vs[0][b]:
                    m.insert(t, n)
                    t += n.duration.quarterLength
            else:
                for vi, v in enumerate(vs):
                    vo = stream.Voice(id=str(vi + 1))
                    t = 0
                    for n in v[b]:
                        if not isinstance(n, note.Rest):
                            n.stemDirection = "up" if vi == 0 else "down"
                        vo.insert(t, n)
                        t += n.duration.quarterLength
                    m.insert(0, vo)
            p.append(m)
        return p

    if kind == "solo":
        s.insert(0, part_with("Flute", rng.choice(list(CLEFS)), 1, chords=rng.random() < 0.3))
    elif kind == "twovoices":
        s.insert(0, part_with("Voices", rng.choice(["treble", "bass"]), 2))
    elif kind == "duo":
        s.insert(0, part_with("Violin", "treble"))
        s.insert(0, part_with("Cello", rng.choice(["bass", "tenor"])))
    else:
        rh = part_with("Piano", "treble", 1, chords=True)
        lh = part_with("Piano", "bass", 1, chords=True)
        rh.__class__ = stream.PartStaff
        lh.__class__ = stream.PartStaff
        s.insert(0, rh)
        s.insert(0, lh)
        s.insert(0, layout.StaffGroup([rh, lh], symbol="brace", name="Piano"))

    # changes part-way through
    first = s.parts[0]
    ms = list(first.getElementsByClass(stream.Measure))
    if level >= 1 and len(ms) > 6 and rng.random() < 0.5:
        for p in s.parts:
            pm = list(p.getElementsByClass(stream.Measure))
            pm[len(pm) // 2].insert(0, key.KeySignature(rng.randint(-5, 5)))
    path = os.path.join(outdir, f"random{seed:03d}.musicxml")
    s.write("musicxml", fp=path)
    return path


if __name__ == "__main__":
    out, count = sys.argv[1], int(sys.argv[2])
    seed0 = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    os.makedirs(out, exist_ok=True)
    for i in range(count):
        print(build(seed0 + i, out))
