import re, sys
from pathlib import Path
# putfn.py FILE FUNC < code : replace INCLUDE_ASM(..., FUNC) or the C definition of FUNC.
# Lines of code before the definition (prototypes) are added once, before it.
path, func = sys.argv[1], sys.argv[2]
code = sys.stdin.read().rstrip("\n") + "\n"
if not code.strip(): sys.exit("empty code")
p = Path(path); s = p.read_text()
m = re.search(r'^INCLUDE_ASM\("[^"]+", %s\);\n' % re.escape(func), s, re.M)
if m:
    a, b = m.start(), m.end()
else:
    m = re.search(r'^[A-Za-z][^\n;]*\b%s\([^;{]*\) \{\n' % re.escape(func), s, re.M)
    if not m: sys.exit("not found: " + func)
    a = m.start(); b = s.index("\n}\n", a) + 3
dm = re.search(r'^[A-Za-z][^\n;]*\b%s\([^;{]*\) \{\n' % re.escape(func), code, re.M)
k = dm.start() if dm else 0
have = set(s.splitlines())
def keep(l):
    # single-line declarations at file scope are added once; everything else is kept
    if not l.strip():
        return False
    code = re.sub(r'\s*/\*.*?\*/\s*$', '', l)
    if l[0] not in " \t}" and (code.rstrip().endswith(";") or l.startswith("#define")) and l in have:
        return False
    return True
pre = code[:k]
# multi-line typedefs/structs already in the file are not added again
for c in re.findall(r'^typedef struct \{\n.*?^\} \w+;\n', pre, re.S | re.M):
    if c in s:
        pre = pre.replace(c, "")
# comment blocks already in the file are not added again
for c in re.findall(r'^/\*.*?\*/\n', pre, re.S | re.M):
    if c in s:
        pre = pre.replace(c, "")
prefix = "".join(l + "\n" for l in pre.splitlines() if keep(l))
p.write_text(s[:a] + prefix + ("\n" if prefix else "") + code[k:] + s[b:])
