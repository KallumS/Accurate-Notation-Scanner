#!/usr/bin/env python3
"""Compares two MusicXML files note by note: the truth and what was read.

    python3 tools/compare.py truth.musicxml read.musicxml [--verbose]

For every part and bar it lists each note and rest as
(onset, pitch or "rest", duration, tie) and counts what matches. Voices and
beaming are not compared (they are notation choices); pitches with their
spelling, rhythms, ties, bar lines-up and onsets are. Prints one line:

    notes 412/412 matched (100.00%), 0 extra, bars 40/40 identical

and with --json, a machine-readable summary.
"""
import difflib, json, sys, warnings
from collections import Counter
warnings.filterwarnings("ignore")
from music21 import converter, note, chord, stream


def events(path):
    s = converter.parse(path)
    parts = []
    for p in s.parts:
        bars = []
        for m in p.getElementsByClass(stream.Measure):
            items = Counter()
            for el in m.recurse().notesAndRests:
                if el.duration.isGrace:
                    continue
                off = round(float(el.getOffsetInHierarchy(m)), 4)
                dur = round(float(el.duration.quarterLength), 4)
                if isinstance(el, note.Rest):
                    if el.style.hideObjectOnPrint or getattr(el, "hideObjectOnPrint", False):
                        continue
                    items[(off, "rest", dur, "")] += 1
                    continue
                pitches = el.pitches
                for i, pt in enumerate(pitches):
                    tie = ""
                    t = el.tie if isinstance(el, note.Note) else (el[i].tie if isinstance(el, chord.Chord) else None)
                    if t is not None:
                        tie = t.type
                    items[(off, pt.nameWithOctave, dur, tie)] += 1
            bars.append(items)
        parts.append(bars)
    return parts


def main():
    truth, read = sys.argv[1], sys.argv[2]
    verbose = "--verbose" in sys.argv
    T, R = events(truth), events(read)
    total = matched = extra = 0
    bars_total = bars_same = 0
    problems = []
    if len(T) != len(R):
        problems.append(f"parts: truth {len(T)}, read {len(R)}")
    for pi in range(max(len(T), len(R))):
        tp = T[pi] if pi < len(T) else []
        rp = R[pi] if pi < len(R) else []
        if len(tp) != len(rp):
            problems.append(f"part {pi+1}: truth {len(tp)} bars, read {len(rp)}")
        # Line the bars up first, so one bar too many or too few counts once.
        key = lambda b: tuple(sorted(b.elements()))
        sm = difflib.SequenceMatcher(None, [key(b) for b in tp], [key(b) for b in rp], autojunk=False)
        pairs = []
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == "equal" or (tag == "replace" and i2 - i1 == j2 - j1):
                pairs += list(zip(range(i1, i2), range(j1, j2)))
            else:
                pairs += [(i, None) for i in range(i1, i2)] + [(None, j) for j in range(j1, j2)]
        for ti, ri in pairs:
            bi = ti if ti is not None else ri
            tb = tp[ti] if ti is not None else Counter()
            rb = rp[ri] if ri is not None else Counter()
            n = sum(tb.values())
            ok = sum((tb & rb).values())
            total += n
            matched += ok
            extra += sum((rb - tb).values())
            bars_total += 1
            if tb == rb:
                bars_same += 1
            elif verbose:
                problems.append(f"part {pi+1} bar {bi+1}: missing {sorted((tb - rb).elements())} extra {sorted((rb - tb).elements())}")
    pct = 100.0 * matched / total if total else 100.0
    line = f"notes {matched}/{total} matched ({pct:.2f}%), {extra} extra, bars {bars_same}/{bars_total} identical"
    if "--json" in sys.argv:
        print(json.dumps({"matched": matched, "total": total, "extra": extra, "bars_same": bars_same, "bars": bars_total, "problems": problems}))
    else:
        print(line)
        for p in problems[:40]:
            print("  " + p)


if __name__ == "__main__":
    main()
