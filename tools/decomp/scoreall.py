"""Scores every NON_MATCHING function: builds it from C in place of its
INCLUDE_ASM and counts the differing instruction lines (raw, and with the
register names normalized). Prints "file func raw=N norm=N len=N".

usage: python3 tools/decomp/scoreall.py > scores.txt   (takes ~20 minutes)
The C drafts and the bases are left in build/us/nm_score/.
"""
import glob
import os
import re
import subprocess

S = os.path.dirname(os.path.abspath(__file__)) + '/'
os.chdir(S + '../..')
NM = 'build/us/nm_score/'
os.makedirs(NM, exist_ok=True)
N = r's/\b(s[0-8]|fp|t[0-9]|a[0-3]|v[01])\b/R/g'
for file in sorted(glob.glob('src/*.c')):
    s = open(file).read()
    for f in re.findall(r'#else\nINCLUDE_ASM\("[^"]+", (\w+)\);\n#endif', s):
        r = subprocess.run(['python3', S + 'unpark.py', file, f, NM + f + '.c'], capture_output=True, text=True)
        if r.returncode:
            print(file, f, 'UNPARK_FAIL', flush=True)
            subprocess.run(['git', 'checkout', '-q', file])
            continue
        subprocess.run(['cp', file, NM + 'base_' + f + '.c'])
        subprocess.run(['git', 'checkout', '-q', file])
        out = subprocess.run('BASE=%sbase_%s.c %stryn.sh %s %s < %s%s.c' % (NM, f, S, file, f, NM, f),
                             shell=True, capture_output=True, text=True).stdout
        if 'BUILD' in out:
            print(file, f, 'BUILD_FAIL', flush=True)
            subprocess.run(['git', 'checkout', '-q', file])
            continue
        raw = out.split()[-1]
        n = subprocess.run("sed -E '%s' /tmp/od_a.txt > /tmp/na.txt; sed -E '%s' /tmp/od_b.txt > /tmp/nb.txt; "
                           "diff /tmp/na.txt /tmp/nb.txt | grep -c '^[<>]'" % (N, N),
                           shell=True, capture_output=True, text=True).stdout.strip()
        ln = sum(1 for _ in open('/tmp/od_a.txt'))
        print(file, f, 'raw=%s norm=%s len=%d' % (raw, n, ln), flush=True)
        subprocess.run(['git', 'checkout', '-q', file])
print('DONE', flush=True)
