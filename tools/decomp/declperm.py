"""Tries every reordering of the local declarations of a function (moving one
line at a time, greedily) and keeps the order with the fewest differing lines.
In this compiler the order rarely matters, but it is cheap to try.

usage: python3 tools/decomp/declperm.py SRC_FILE C_DRAFT FUNC OUT
  SRC_FILE  the file the function goes in (e.g. src/camera.c)
  C_DRAFT   the function's C code; OUT gets the best version
"""
import subprocess
import sys
import os

S = os.path.dirname(os.path.abspath(__file__)) + '/'
target, draft, func, out = sys.argv[1:5]
lines = open(draft).read().split('\n')
start = [i for i, l in enumerate(lines) if func + '(' in l and l.endswith('{') and not l.startswith(' ')][0] + 1
end = start
while (lines[end].strip() and lines[end].startswith('    ') and lines[end].rstrip().endswith(';')
       and '=' not in lines[end] and '(' not in lines[end]):
    end += 1
decl = lines[start:end]


def score(d):
    open('/tmp/perm_try.c', 'w').write('\n'.join(lines[:start] + d + lines[end:]))
    r = subprocess.run('%stryn.sh %s %s < /tmp/perm_try.c' % (S, target, func), shell=True,
                       capture_output=True, text=True).stdout
    return int(r.split()[-1]) if 'BUILD' not in r else 9999


best = score(decl)
print(len(decl), best, flush=True)
improved = True
while improved and best > 0:
    improved = False
    for i in range(len(decl)):
        for j in range(len(decl)):
            if i == j:
                continue
            d = list(decl)
            x = d.pop(i)
            d.insert(j, x)
            s = score(d)
            if s < best:
                best, decl, improved = s, d, True
                print(best, x.strip(), '->', j, flush=True)
                open(out, 'w').write('\n'.join(lines[:start] + decl + lines[end:]))
print('final', best)
