# unpark.py FILE FUNC OUT : replace the NON_MATCHING block of FUNC with
# INCLUDE_ASM, writing the C function (with its comment) to OUT.
import sys, re
path, func, out = sys.argv[1:4]
s = open(path).read()
m = re.search(r'#ifdef NON_MATCHING\n((?:(?!#ifdef NON_MATCHING).)*?\b%s\([^;]*?\{.*?)#else\n(INCLUDE_ASM\("[^"]+", %s\);\n)#endif\n' % (re.escape(func), re.escape(func)), s, re.S)
if not m:
    sys.exit("not found")
open(out, 'w').write(m.group(1))
s = s[:m.start()] + m.group(2) + s[m.end():]
open(path, 'w').write(s)
