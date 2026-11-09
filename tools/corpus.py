#!/usr/bin/env python3
"""The accuracy check: engrave known music, read it back, compare.

    python3 tools/corpus.py [--sets bach,real,random] [--count N]
                            [--engravers musescore,lilypond] [--out corpus-out]
                            [--scanner build-core/notation-scan]

For every piece:
  1. its MusicXML (from music21's corpus, or tools/randomscore.py)
  2. engraved to PDF by MuseScore 3 and by LilyPond. MuseScore also writes
     back the MusicXML of what it engraved, which is the truth for its PDF;
     for LilyPond the source MusicXML is the truth.
  3. read by notation-scan
  4. compared note by note with tools/compare.py

Prints a line per piece and the totals. Needs mscore3, lilypond,
musicxml2ly and music21 (pip install music21).
"""
import argparse, json, os, shutil, subprocess, sys, warnings
from concurrent.futures import ThreadPoolExecutor
warnings.filterwarnings("ignore")

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

REAL = [
    "joplin/maple_leaf_rag.mxl", "schumann_robert/dichterliebe_no2.xml", "schubert/Lindenbaum.xml",
    "schoenberg/opus19/movement2.mxl", "schoenberg/opus19/movement6.mxl", "mozart/k155/movement1.mxl",
    "haydn/opus1no1/movement1.mxl", "beach/prayer_of_a_tired_child.musicxml", "handel/rinaldo/Lascia_chio_pianga.mxl",
    "verdi/laDonnaEMobile.mxl", "weber/concertino_clarinet.mxl", "liliuokalani/aloha_oe.mxl",
    "johnson_j_r/lift_every_voice.mxl", "corelli/opus3no1/1grave.xml", "cpebach/h186.mxl",
    "schumann_clara/polonaise_op1n1.mxl", "schumann_robert/opus41no1/movement1.mxl", "monteverdi/madrigal.3.1.mxl",
    "webern/webern_dormi_jesu_op_16_no_2.mxl", "leadSheet/fosterBrownHair.mxl", "bach/bwv67.4.xml",
]


def sources(sets, count, outdir):
    from music21 import corpus
    src = os.path.join(outdir, "src")
    os.makedirs(src, exist_ok=True)
    out = []
    if "bach" in sets:
        paths = sorted(str(p) for p in corpus.getComposer("bach") if str(p).endswith(".mxl"))
        for p in paths[:: max(1, len(paths) // count)][:count]:
            out.append(("bach_" + os.path.basename(p).replace(".mxl", ""), p))
    if "real" in sets:
        for rel in REAL[:count if "bach" not in sets else len(REAL)]:
            out.append((rel.replace("/", "_").rsplit(".", 1)[0], str(corpus.getWork(rel))))
    result = []
    for name, path in out:
        dst = os.path.join(src, name + ".musicxml")
        if not os.path.exists(dst):
            try:
                s = corpus.parse(path)
                s.write("musicxml", fp=dst)
            except Exception as e:
                print(f"skip {name}: {e}", file=sys.stderr)
                continue
        result.append((name, dst))
    if "random" in sets:
        rnd = os.path.join(outdir, "random")
        if not os.path.isdir(rnd) or len(os.listdir(rnd)) < count:
            subprocess.run([sys.executable, os.path.join(HERE, "randomscore.py"), rnd, str(count)], check=True,
                           stdout=subprocess.DEVNULL)
        for f in sorted(os.listdir(rnd))[:count]:
            result.append((f.rsplit(".", 1)[0], os.path.join(rnd, f)))
    return result


def engrave_musescore(pieces, outdir):
    d = os.path.join(outdir, "musescore")
    os.makedirs(d, exist_ok=True)
    jobs = []
    for name, src in pieces:
        pdf, truth = os.path.join(d, name + ".pdf"), os.path.join(d, name + ".truth.musicxml")
        if not (os.path.exists(pdf) and os.path.exists(truth)):
            jobs.append({"in": src, "out": [pdf, truth]})
    if jobs:
        job = os.path.join(d, "job.json")
        json.dump(jobs, open(job, "w"))
        env = dict(os.environ, QT_QPA_PLATFORM="offscreen")
        subprocess.run(["mscore3", "-j", job], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=3600)
    return [(n, os.path.join(d, n + ".pdf"), os.path.join(d, n + ".truth.musicxml")) for n, _ in pieces]


def engrave_lilypond_one(args):
    name, src, d = args
    pdf = os.path.join(d, name + ".pdf")
    if os.path.exists(pdf):
        return
    ly = os.path.join(d, name + ".ly")
    subprocess.run(["musicxml2ly", "--no-beaming", "-o", ly, src], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["lilypond", "-s", "-o", os.path.join(d, name), ly], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600)


def engrave_lilypond(pieces, outdir):
    d = os.path.join(outdir, "lilypond")
    os.makedirs(d, exist_ok=True)
    with ThreadPoolExecutor(8) as ex:
        list(ex.map(engrave_lilypond_one, [(n, s, d) for n, s in pieces]))
    return [(n, os.path.join(d, n + ".pdf"), s) for n, s in pieces]


def check_one(args):
    scanner, name, pdf, truth = args
    if not os.path.exists(pdf) or not os.path.exists(truth):
        return name, None, "not engraved"
    read = pdf[:-4] + ".read.musicxml"
    report = pdf[:-4] + ".report.json"
    r = subprocess.run([scanner, pdf, "-o", read, "--report", report, "-q"], capture_output=True, text=True)
    if r.returncode == 2 or not os.path.exists(read):
        return name, None, "rejected: " + (r.stderr.strip()[:200])
    c = subprocess.run([sys.executable, os.path.join(HERE, "compare.py"), truth, read, "--json", "--verbose"],
                       capture_output=True, text=True)
    try:
        res = json.loads(c.stdout)
    except Exception:
        return name, None, "compare failed: " + c.stderr.strip()[-300:]
    rep = json.load(open(report))
    res["flagged"] = rep.get("barsWithIssues", 0)
    res["verdict"] = rep.get("verdict")
    return name, res, ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sets", default="bach,real,random")
    ap.add_argument("--count", type=int, default=10)
    ap.add_argument("--engravers", default="musescore,lilypond")
    ap.add_argument("--out", default=os.path.join(ROOT, "corpus-out"))
    ap.add_argument("--scanner", default=os.path.join(ROOT, "build-core", "notation-scan"))
    ap.add_argument("--only", default="")
    ap.add_argument("--verbose", action="store_true")
    a = ap.parse_args()

    pieces = sources(a.sets.split(","), a.count, a.out)
    if a.only:
        pieces = [p for p in pieces if a.only in p[0]]
    grand = {"matched": 0, "total": 0, "extra": 0, "bars_same": 0, "bars": 0}
    for eng in a.engravers.split(","):
        triples = engrave_musescore(pieces, a.out) if eng == "musescore" else engrave_lilypond(pieces, a.out)
        with ThreadPoolExecutor(8) as ex:
            results = list(ex.map(check_one, [(a.scanner, n, p, t) for n, p, t in triples]))
        tot = {"matched": 0, "total": 0, "extra": 0, "bars_same": 0, "bars": 0}
        print(f"== {eng}")
        for name, res, err in results:
            if res is None:
                print(f"  {name:50s} {err}")
                continue
            for k in tot:
                tot[k] += res[k]
            pct = 100.0 * res["matched"] / res["total"] if res["total"] else 100
            mark = "OK " if res["matched"] == res["total"] and res["extra"] == 0 else "   "
            print(f"  {mark}{name:47s} notes {res['matched']:5d}/{res['total']:5d} ({pct:6.2f}%) extra {res['extra']:4d}"
                  f"  bars {res['bars_same']:3d}/{res['bars']:3d}  flagged {res['flagged']:3d}  {res['verdict']}")
            if a.verbose:
                for p in res["problems"][:8]:
                    print("        " + p[:300])
        pct = 100.0 * tot["matched"] / tot["total"] if tot["total"] else 100
        print(f"  TOTAL notes {tot['matched']}/{tot['total']} ({pct:.2f}%), extra {tot['extra']}, bars {tot['bars_same']}/{tot['bars']} identical")
        for k in grand:
            grand[k] += tot[k]


if __name__ == "__main__":
    main()
