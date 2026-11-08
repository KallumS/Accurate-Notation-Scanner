#!/usr/bin/env python3
"""Writes a fixture's truth as a plain note list, one line per staff and bar:

    S<staff> B<bar>: <onset> <pitch> <duration> <tie>; ...

using the same rules as compare.py (pitches as music21 spells them, "E-4",
"F#5"; grace notes and hidden rests left out; items sorted). The C++ test
(Tests/TestFixtures.cpp) writes the same list from what it reads.

    python3 tools/expected.py truth.musicxml > Tests/fixtures/name.notes
"""
import sys, warnings
warnings.filterwarnings("ignore")
sys.path.insert(0, __import__("os").path.dirname(__file__))
from compare import events


def fmt(x):
    s = f"{x:.4f}".rstrip("0").rstrip(".")
    return s if s else "0"


for si, bars in enumerate(events(sys.argv[1])):
    for bi, items in enumerate(bars):
        row = sorted(f"{fmt(o)} {p} {fmt(d)} {t or '-'}" for (o, p, d, t), n in items.items() for _ in range(n))
        print(f"S{si + 1} B{bi + 1}: " + "; ".join(row))
