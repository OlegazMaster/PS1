#!/bin/bash
# inst.sh FILE FUNC CODE : install matched CODE (NM note removed) into FILE (unparks FUNC in FILE first), full build check
S="$(cd "$(dirname "$0")" && pwd)"
cd "$(dirname "$0")/../.."
python3 - "$3" <<'PY'
import sys,re
p=sys.argv[1]; t=open(p).read()
# drop " * Equivalent...*/" style note lines in the leading comment
t=re.sub(r'\n \* (Equivalent|NON_MATCHING)[^\n]*?\*/', ' */', t, count=1)
t=re.sub(r'\n \* (Equivalent|NON_MATCHING)[^\n]*', '', t, count=1)
open(p+'.inst','w').write(t)
PY
python3 $S/unpark.py $1 $2 /tmp/inst_old.c || { echo "unpark failed"; exit 1; }
python3 $S/putfn.py $1 $2 < $3.inst
python3 - $1 $2 <<'PY'
import sys,re
p,f=sys.argv[1:3]; s=open(p).read()
s=re.sub(r'\*/\n\n((?:[A-Za-z][^\n]*)\b%s\()'%f, r'*/\n\1', s)
open(p,'w').write(s)
PY
rm -rf build/us/src; make 2>&1 | grep -q "OK, совпадает" && echo "MATCH $2" || { echo "NO MATCH $2"; git checkout -q $1; }
