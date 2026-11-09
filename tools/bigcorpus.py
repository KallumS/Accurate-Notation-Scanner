#!/usr/bin/env python3
"""The accuracy check at scale, for corpora that come as score files.

  MuseScore files (.mscx, .mscz, .mxl, .musicxml, .krn, .abc...):
    python3 tools/bigcorpus.py musescore OUT FILE_OR_DIR... [--limit N] [--jobs 8]
  Each is engraved by MuseScore 3 into a PDF and the MusicXML of what it
  engraved (the truth), read back, and compared note by note.

  LilyPond sources (.ly, e.g. the Mutopia Project):
    python3 tools/bigcorpus.py lilypond OUT FILE_OR_DIR... [--limit N]
  Each is engraved by LilyPond (in its safe mode) into a PDF and a MIDI file;
  the MIDI is the truth: every note's pitch, onset and length.

Results go to OUT/results.jsonl (one line per piece) and a summary is
printed: notes matched, bars identical, and - the number that matters
most (decision 0002) - bars that were wrong but not marked.
"""
import argparse, glob, json, os, shutil, subprocess, sys, tempfile, warnings
from concurrent.futures import ThreadPoolExecutor
warnings.filterwarnings("ignore")

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SCANNER = os.environ.get("ANS_SCANNER", os.path.join(ROOT, "build-core", "notation-scan"))
SCORE_EXT = (".mscx", ".mscz", ".mxl", ".musicxml", ".xml", ".krn", ".abc")


def gather(paths, exts, limit):
    """Score files under the paths; where one piece is there in several
    formats (song.mscx, song.mscz, song.mxl), the first format listed wins."""
    out = []
    for p in paths:
        if os.path.isdir(p):
            for root, _, files in os.walk(p):
                for f in sorted(files):
                    if f.lower().endswith(exts):
                        out.append(os.path.join(root, f))
        else:
            out.append(p)
    best = {}
    for f in out:
        stem, ext = os.path.splitext(f)
        rank = exts.index(ext.lower()) if ext.lower() in exts else len(exts)
        if stem not in best or rank < best[stem][0]:
            best[stem] = (rank, f)
    out = sorted(f for _, f in best.values())
    return out[:limit] if limit else out


def name_of(path, base):
    rel = os.path.relpath(path, base) if base else os.path.basename(path)
    return rel.replace("/", "__").rsplit(".", 1)[0].replace(" ", "_")


# ---------------------------------------------------------------- MuseScore

def openable_copy(f, workdir):
    """MuseScore 3.2 refuses files saved by 3.5 and later (format 3.02),
    though the format barely changed. A copy marked 3.01 opens; whatever it
    does not understand is skipped - in the PDF and the truth alike."""
    if f.endswith(".mscz"):
        import zipfile
        try:
            with zipfile.ZipFile(f) as z:
                inner = [n for n in z.namelist() if n.endswith(".mscx")]
                if not inner:
                    return f
                text = z.read(inner[0]).decode("utf-8", errors="replace")
        except Exception:
            return f
    elif f.endswith(".mscx"):
        with open(f, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    else:
        return f
    if '<museScore version="3.02">' not in text and f.endswith(".mscx"):
        return f
    dst = os.path.join(workdir, os.path.splitext(os.path.basename(f))[0] + ".mscx")
    with open(dst, "w", encoding="utf-8") as fh:
        fh.write(text.replace('<museScore version="3.02">', '<museScore version="3.01">', 1))
    return dst


def engrave_musescore(files, out, jobs):
    pdfdir = os.path.join(out, "pdf")
    os.makedirs(pdfdir, exist_ok=True)
    copies = os.path.join(out, "copies")
    os.makedirs(copies, exist_ok=True)
    todo = []
    for f, n in files:
        f = openable_copy(f, copies)
        pdf, truth = os.path.join(pdfdir, n + ".pdf"), os.path.join(pdfdir, n + ".truth.musicxml")
        if not (os.path.exists(pdf) and os.path.exists(truth)) and not os.path.exists(os.path.join(pdfdir, n + ".failed")):
            todo.append((f, n, pdf, truth))
    batches = [todo[i:i + 1] for i in range(0, len(todo), 1)]

    def run(batch):
        job = tempfile.NamedTemporaryFile("w", suffix=".json", delete=False)
        json.dump([{"in": f, "out": [pdf, truth]} for f, n, pdf, truth in batch], job)
        job.close()
        env = dict(os.environ, QT_QPA_PLATFORM="offscreen")
        try:
            subprocess.run(["mscore3", "-j", job.name], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=300)
        except subprocess.TimeoutExpired:
            pass
        os.unlink(job.name)
        for f, n, pdf, truth in batch:
            if not (os.path.exists(pdf) and os.path.exists(truth)):
                open(os.path.join(pdfdir, n + ".failed"), "w").close()

    with ThreadPoolExecutor(jobs) as ex:
        list(ex.map(run, batches))
    return [(n, os.path.join(pdfdir, n + ".pdf"), os.path.join(pdfdir, n + ".truth.musicxml")) for f, n in files
            if os.path.exists(os.path.join(pdfdir, n + ".truth.musicxml"))]


def check_musicxml(args):
    name, pdf, truth = args
    read, report = pdf[:-4] + ".read.musicxml", pdf[:-4] + ".report.json"
    try:
        r = subprocess.run([SCANNER, pdf, "-o", read, "--report", report, "-q"], capture_output=True, text=True, timeout=300)
    except subprocess.TimeoutExpired:
        return {"name": name, "status": "timeout"}
    if r.returncode < 0 or r.returncode > 2:
        return {"name": name, "status": "crash", "why": f"exit {r.returncode}: " + r.stderr.strip()[-200:]}
    try:
        rep = json.load(open(report))
    except Exception:
        rep = {}
    if r.returncode == 2 or not os.path.exists(read):
        return {"name": name, "status": "rejected", "why": rep.get("summary", r.stderr.strip()[:200])}
    c = subprocess.run([sys.executable, "-I", os.path.join(HERE, "compare.py"), truth, read, "--json", "--verbose"],
                       capture_output=True, text=True, timeout=600, cwd=HERE)
    try:
        res = json.loads(c.stdout)
    except Exception:
        return {"name": name, "status": "compare-failed", "why": c.stderr.strip()[-200:]}
    marked = {i["bar"] for i in rep.get("issues", []) if i["level"] != "info"}
    wrong = set()
    for p in res["problems"]:
        if p.startswith("part") and " bar " in p:
            wrong.add(int(p.split(" bar ")[1].split(":")[0]))
    unmarked = sorted(b for b in wrong if not ({b - 1, b, b + 1} & marked))
    return {"name": name, "status": "read", "verdict": rep.get("verdict"), "matched": res["matched"], "total": res["total"],
            "extra": res["extra"], "bars": res["bars"], "bars_same": res["bars_same"], "wrong_bars": len(wrong),
            "unmarked": unmarked, "marked_bars": len(marked), "problems": res["problems"][:6]}


# ---------------------------------------------------------------- LilyPond + MIDI

def read_midi(path):
    """Notes from a MIDI file as (onset, pitch, duration) in quarter notes."""
    data = open(path, "rb").read()
    if data[:4] != b"MThd":
        return None
    division = int.from_bytes(data[12:14], "big")
    pos = 8 + int.from_bytes(data[4:8], "big")
    notes = []
    while pos < len(data):
        kind, length = data[pos:pos + 4], int.from_bytes(data[pos + 4:pos + 8], "big")
        chunk = data[pos + 8:pos + 8 + length]
        pos += 8 + length
        if kind != b"MTrk":
            continue
        i, t, status, on = 0, 0, 0, {}
        while i < len(chunk):
            delta = 0
            while True:
                b = chunk[i]; i += 1
                delta = (delta << 7) | (b & 0x7F)
                if b < 0x80:
                    break
            t += delta
            b = chunk[i]
            if b >= 0x80:
                status = b; i += 1
            if status == 0xFF:
                i += 1
                ln = 0
                while True:
                    c = chunk[i]; i += 1
                    ln = (ln << 7) | (c & 0x7F)
                    if c < 0x80:
                        break
                i += ln
                continue
            if status in (0xF0, 0xF7):
                ln = 0
                while True:
                    c = chunk[i]; i += 1
                    ln = (ln << 7) | (c & 0x7F)
                    if c < 0x80:
                        break
                i += ln
                continue
            hi = status & 0xF0
            if hi in (0x80, 0x90):
                key, vel = chunk[i], chunk[i + 1]; i += 2
                ch = status & 0x0F
                if hi == 0x90 and vel > 0:
                    on.setdefault((ch, key), []).append(t)
                elif on.get((ch, key)):
                    start = on[(ch, key)].pop(0)
                    notes.append((start / division, key, (t - start) / division))
            elif hi in (0xA0, 0xB0, 0xE0):
                i += 2
            elif hi in (0xC0, 0xD0):
                i += 1
    return notes


def notes_of_musicxml(path):
    from music21 import converter, note, chord
    s = converter.parse(path)
    out = []
    for p in s.parts:
        flat = p.stripTies().flatten()
        for el in flat.notes:
            if el.duration.isGrace:
                continue
            off = float(el.getOffsetInHierarchy(p))
            for pt in (el.pitches if isinstance(el, chord.Chord) else [el.pitch]):
                out.append((off, pt.midi, float(el.duration.quarterLength)))
    return out


def compare_notes(truth, read):
    """Multiset match by (onset, pitch) and by (onset, pitch, duration)."""
    from collections import Counter
    r4 = lambda x: round(x * 48) / 48
    T = Counter((r4(o), p) for o, p, d in truth)
    R = Counter((r4(o), p) for o, p, d in read)
    TD = Counter((r4(o), p, r4(d)) for o, p, d in truth)
    RD = Counter((r4(o), p, r4(d)) for o, p, d in read)
    return sum((T & R).values()), sum((TD & RD).values()), len(truth), sum((R - T).values())


def engrave_lilypond(args):
    f, n, outdir = args
    pdf, midi = os.path.join(outdir, n + ".pdf"), os.path.join(outdir, n + ".midi")
    if os.path.exists(pdf) or os.path.exists(os.path.join(outdir, n + ".failed")):
        return
    work = tempfile.mkdtemp()
    try:
        src = os.path.join(work, "in.ly")
        shutil.copy(f, src)
        # bring old syntax up to date, then engrave with Scheme restricted
        subprocess.run(["convert-ly", "-e", src], cwd=work, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
        subprocess.run(["lilypond", "-dsafe", "-s", "-I", os.path.dirname(os.path.abspath(f)), "-o", os.path.join(work, "out"), src],
                       cwd=work, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=300)
        pdfs = sorted(glob.glob(os.path.join(work, "out*.pdf")))
        mids = sorted(glob.glob(os.path.join(work, "out*.mid*")))
        if len(pdfs) == 1 and len(mids) == 1:   # one score per file: the MIDI is its truth
            shutil.move(pdfs[0], pdf)
            shutil.move(mids[0], midi)
        else:
            open(os.path.join(outdir, n + ".failed"), "w").close()
    except Exception:
        open(os.path.join(outdir, n + ".failed"), "w").close()
    finally:
        shutil.rmtree(work, ignore_errors=True)


def check_midi(args):
    name, pdf, midi = args
    read, report = pdf[:-4] + ".read.musicxml", pdf[:-4] + ".report.json"
    try:
        r = subprocess.run([SCANNER, pdf, "-o", read, "--report", report, "-q"], capture_output=True, text=True, timeout=300)
    except subprocess.TimeoutExpired:
        return {"name": name, "status": "timeout"}
    if r.returncode < 0 or r.returncode > 2:
        return {"name": name, "status": "crash", "why": f"exit {r.returncode}"}
    try:
        rep = json.load(open(report))
    except Exception:
        rep = {}
    if r.returncode == 2 or not os.path.exists(read):
        return {"name": name, "status": "rejected", "why": rep.get("summary", "")[:200]}
    truth = read_midi(midi)
    try:
        got = notes_of_musicxml(read)
    except Exception as e:
        return {"name": name, "status": "compare-failed", "why": str(e)[:200]}
    onset_pitch, full, total, extra = compare_notes(truth, got)
    return {"name": name, "status": "read", "verdict": rep.get("verdict"), "matched_pitch_onset": onset_pitch,
            "matched": full, "total": total, "extra": extra, "marked_bars": rep.get("barsWithIssues", 0), "bars": rep.get("bars", 0)}


def check_midi_isolated(args):
    # music21 parsing in a subprocess keeps one bad file from stopping the run
    r = subprocess.run([sys.executable, "-I", os.path.abspath(__file__), "_one_midi", json.dumps(args)],
                       capture_output=True, text=True, timeout=900, cwd=HERE)
    try:
        return json.loads(r.stdout.strip().splitlines()[-1])
    except Exception:
        return {"name": args[0], "status": "compare-failed", "why": r.stderr.strip()[-200:]}


# ---------------------------------------------------------------- summary

def summarise(results, mode):
    read = [r for r in results if r["status"] == "read"]
    rejected = [r for r in results if r["status"] == "rejected"]
    failed = [r for r in results if r["status"] not in ("read", "rejected")]
    t = sum(r["total"] for r in read)
    m = sum(r["matched"] for r in read)
    clean = [r for r in read if r["verdict"] == "clean"]
    clean_exact = [r for r in clean if r["matched"] == r["total"] and r["extra"] == 0]
    print(f"pieces: {len(results)}  read: {len(read)}  refused: {len(rejected)}  failed to compare: {len(failed)}")
    print(f"notes matched exactly: {m}/{t} ({100.0 * m / max(1, t):.2f}%)")
    if mode == "lilypond":
        po = sum(r["matched_pitch_onset"] for r in read)
        print(f"notes with the right pitch at the right time: {po}/{t} ({100.0 * po / max(1, t):.2f}%)")
    else:
        b = sum(r["bars"] for r in read)
        bs = sum(r["bars_same"] for r in read)
        wb = sum(r["wrong_bars"] for r in read)
        um = sum(len(r["unmarked"]) for r in read)
        print(f"bars identical: {bs}/{b} ({100.0 * bs / max(1, b):.2f}%)   wrong bars: {wb}, of which NOT marked: {um}")
    print(f"pieces reported 'every bar checked out': {len(clean)}, of which exactly right: {len(clean_exact)}")
    return {"pieces": len(results), "read": len(read), "refused": len(rejected), "failed": len(failed), "notes": t, "matched": m}


def main():
    if len(sys.argv) > 2 and sys.argv[1] == "_one_midi":
        print(json.dumps(check_midi(json.loads(sys.argv[2]))))
        return
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=["musescore", "lilypond"])
    ap.add_argument("out")
    ap.add_argument("inputs", nargs="+")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--jobs", type=int, default=8)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    base = os.path.commonpath([os.path.abspath(p) for p in a.inputs]) if len(a.inputs) > 1 else os.path.abspath(a.inputs[0])
    if a.mode == "musescore":
        files = [(f, name_of(os.path.abspath(f), base)) for f in gather(a.inputs, SCORE_EXT, a.limit)]
        triples = engrave_musescore(files, a.out, max(1, a.jobs // 2))
        with ThreadPoolExecutor(a.jobs) as ex:
            results = list(ex.map(check_musicxml, triples))
    else:
        files = [(f, name_of(os.path.abspath(f), base)) for f in gather(a.inputs, (".ly",), a.limit)]
        d = os.path.join(a.out, "pdf")
        os.makedirs(d, exist_ok=True)
        with ThreadPoolExecutor(a.jobs) as ex:
            list(ex.map(engrave_lilypond, [(f, n, d) for f, n in files]))
        triples = [(n, os.path.join(d, n + ".pdf"), os.path.join(d, n + ".midi")) for f, n in files if os.path.exists(os.path.join(d, n + ".pdf"))]
        with ThreadPoolExecutor(a.jobs) as ex:
            results = list(ex.map(check_midi_isolated, triples))
    with open(os.path.join(a.out, "results.jsonl"), "w") as f:
        for r in results:
            f.write(json.dumps(r) + "\n")
    summary = summarise(results, a.mode)
    json.dump(summary, open(os.path.join(a.out, "summary.json"), "w"))


if __name__ == "__main__":
    main()
