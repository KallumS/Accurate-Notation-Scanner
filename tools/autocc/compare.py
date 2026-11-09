"""The app's AutoCC port against AutoCC.jsfx itself, run through ysfx.

Random note sequences - lines, chords, short and long gaps (either side of
AutoCC's 0.4 s), overlaps, every preset, several Dynamic settings - go to
both; every CC value and the sample it is sent at must be the same.

    tools/autocc/build.sh
    python3 tools/autocc/compare.py path/to/AutoCC.jsfx [seed] [count]
"""
import os, random, subprocess, sys
B = os.environ.get('AUTOCC_BUILD', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'build'))
JS = sys.argv[1]
def seq(rng):
    """notes as a player or a score gives them: lines, chords, gaps short and long, repeats"""
    t = 0.0; out = []
    for _ in range(rng.randint(1, 8)):
        n = rng.choice([1, 1, 1, 2, 3])
        pitches = rng.sample(range(48, 84), n)
        dur = rng.choice([0.15, 0.3, 0.5, 1.0, 2.0, 3.5])
        vel = rng.randint(1, 127)
        for p in pitches:
            out.append((t, 'on', p, vel)); out.append((t + dur, 'off', p, 0))
        t += dur + rng.choice([0.0, 0.0, 0.05, 0.2, 0.39, 0.41, 0.8, 1.5])
        if rng.random() < 0.2: t -= dur * 0.3   # overlapping, legato
    return out
rng = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 1)
cases = same = 0; first = None
for i in range(int(sys.argv[3]) if len(sys.argv) > 3 else 200):
    ev = seq(rng); preset = rng.randint(0, 3); dyn = rng.choice([0, 0, -24, 14, 28])
    text = ''.join(f'{t:.6f} {k} {p} {v}\n' for t, k, p, v in ev)
    a = subprocess.run([B + '/jsfxhost', JS, str(preset), str(dyn)], input=text, capture_output=True, text=True).stdout.split('\n')
    b = subprocess.run([B + '/porthost', str(preset), str(dyn)], input=text, capture_output=True, text=True).stdout.split('\n')
    cases += 1
    if a == b: same += 1
    elif first is None:
        first = (preset, dyn, text, [x for x in zip(a, b) if x[0] != x[1]][:6], len(a), len(b))
print(f'{same}/{cases} note sequences give exactly the same CC values at the same samples')
if first: print('first difference:', first[0], first[1], first[3], first[4], first[5]); print(first[2])
