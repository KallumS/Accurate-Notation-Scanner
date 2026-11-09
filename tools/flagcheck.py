#!/usr/bin/env python3
"""For each piece read in corpus-out: are all the bars that differ from the
truth among the bars the reader marked? Prints the bars that are wrong and
unmarked - the silent errors.

    python3 tools/flagcheck.py corpus-out musescore
"""
import json, os, subprocess, sys
root, eng = sys.argv[1], sys.argv[2]
here = os.path.dirname(os.path.abspath(__file__))
silent = total_wrong = 0
for f in sorted(os.listdir(os.path.join(root, eng))):
    if not f.endswith(".read.musicxml"):
        continue
    name = f[:-len(".read.musicxml")]
    truth = os.path.join(root, eng, name + ".truth.musicxml")
    if not os.path.exists(truth):
        truth = os.path.join(root, "src", name + ".musicxml")
        if not os.path.exists(truth):
            truth = os.path.join(root, "random", name + ".musicxml")
    rep = json.load(open(os.path.join(root, eng, name + ".report.json")))
    marked = {i["bar"] for i in rep["issues"] if i["level"] != "info"}
    r = subprocess.run([sys.executable, os.path.join(here, "compare.py"), truth, os.path.join(root, eng, f), "--json", "--verbose"], capture_output=True, text=True)
    try:
        res = json.loads(r.stdout)
    except Exception:
        continue
    wrong = set()
    for p in res["problems"]:
        if " bar " in p and p.startswith("part"):
            wrong.add(int(p.split(" bar ")[1].split(":")[0]))
    total_wrong += len(wrong)
    unmarked = sorted(b for b in wrong if b not in marked and b - 1 not in marked and b + 1 not in marked)
    if unmarked:
        silent += len(unmarked)
        print(f"{name}: wrong but unmarked bars {unmarked[:12]}")
print(f"wrong bars {total_wrong}, of which unmarked (not even next to a mark) {silent}")
